#include "ReferenceImageLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <unordered_map>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#elif defined(_WIN32)
#include <windows.h>
#include <wincodec.h>
#endif

namespace buckswood_lookdna {
namespace {

std::mutex gCacheMutex;
std::unordered_map<std::string, std::weak_ptr<const ReferenceAsset>> gCache;

float clamp(float value, float low, float high)
{
    return std::min(high, std::max(low, value));
}

std::string lowercaseExtension(const std::string& path)
{
    std::string extension = std::filesystem::path(path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}

std::string cacheKey(const std::string& path, int colorSpace, int quality)
{
    std::int64_t modified = 0;
    try {
        modified = static_cast<std::int64_t>(
            std::filesystem::last_write_time(path).time_since_epoch().count());
    } catch (const std::exception&) {
        modified = 0;
    }
    std::ostringstream stream;
    stream << path << '|' << colorSpace << '|' << quality << '|' << modified;
    return stream.str();
}

#if defined(_WIN32)
std::wstring utf8ToWide(const std::string& value)
{
    if (value.empty()) {
        return std::wstring();
    }
    const int length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (length <= 0) {
        return std::wstring();
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.c_str(),
        static_cast<int>(value.size()),
        wide.data(),
        length);
    return wide;
}

template <typename T>
void releaseCom(T*& pointer)
{
    if (pointer) {
        pointer->Release();
        pointer = nullptr;
    }
}
#endif

} // namespace

ReferenceImage::ReferenceImage(int width, int height, std::vector<Pixel> pixels)
    : width_(width)
    , height_(height)
    , pixels_(std::move(pixels))
{
}

Pixel ReferenceImage::read(int x, int y) const
{
    if (width_ <= 0 || height_ <= 0 || pixels_.empty()) {
        return Pixel{0.0f, 0.0f, 0.0f, 0.0f};
    }
    x = std::max(0, std::min(width_ - 1, x));
    y = std::max(0, std::min(height_ - 1, y));
    return pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
}

Pixel ReferenceImage::sample(float x, float y) const
{
    if (width_ <= 0 || height_ <= 0) {
        return Pixel{0.0f, 0.0f, 0.0f, 0.0f};
    }
    x = clamp(x, 0.0f, static_cast<float>(width_ - 1));
    y = clamp(y, 0.0f, static_cast<float>(height_ - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(width_ - 1, x0 + 1);
    const int y1 = std::min(height_ - 1, y0 + 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const Pixel p00 = read(x0, y0);
    const Pixel p10 = read(x1, y0);
    const Pixel p01 = read(x0, y1);
    const Pixel p11 = read(x1, y1);
    const float ux = 1.0f - tx;
    const float uy = 1.0f - ty;
    return Pixel{
        (p00.r * ux + p10.r * tx) * uy + (p01.r * ux + p11.r * tx) * ty,
        (p00.g * ux + p10.g * tx) * uy + (p01.g * ux + p11.g * tx) * ty,
        (p00.b * ux + p10.b * tx) * uy + (p01.b * ux + p11.b * tx) * ty,
        (p00.a * ux + p10.a * tx) * uy + (p01.a * ux + p11.a * tx) * ty,
    };
}

Bounds ReferenceImage::bounds() const
{
    return Bounds{0, 0, width_, height_};
}

int ReferenceImage::width() const
{
    return width_;
}

int ReferenceImage::height() const
{
    return height_;
}

std::shared_ptr<const ReferenceAsset> ReferenceImageLoader::loadCached(
    const std::string& path,
    int referenceColorSpace,
    int analysisQuality)
{
    if (path.empty()) {
        auto asset = std::make_shared<ReferenceAsset>();
        asset->error = "No reference still selected";
        return asset;
    }
    const std::string key = cacheKey(path, referenceColorSpace, analysisQuality);
    {
        std::lock_guard<std::mutex> lock(gCacheMutex);
        auto found = gCache.find(key);
        if (found != gCache.end()) {
            if (auto cached = found->second.lock()) {
                return cached;
            }
        }
    }

    auto asset = std::make_shared<ReferenceAsset>();
    if (lowercaseExtension(path) == ".bwlook") {
        LookDNACore::loadProfile(path, asset->profile, &asset->error);
    } else if (loadImage(path, asset->image, asset->error)) {
        asset->profile = LookDNACore::analyze(
            *asset->image,
            referenceColorSpace,
            analysisQuality);
        asset->spatialProfiles = LookDNACore::analyzeGrid(
            *asset->image,
            referenceColorSpace,
            analysisQuality);
        if (!asset->profile.valid) {
            asset->error = "Reference image did not contain enough valid pixels";
        }
    }

    {
        std::lock_guard<std::mutex> lock(gCacheMutex);
        gCache[key] = asset;
    }
    return asset;
}

void ReferenceImageLoader::clearCache()
{
    std::lock_guard<std::mutex> lock(gCacheMutex);
    gCache.clear();
}

bool ReferenceImageLoader::loadImage(
    const std::string& path,
    std::shared_ptr<ReferenceImage>& image,
    std::string& error)
{
#if defined(__APPLE__)
    CFStringRef pathString = CFStringCreateWithCString(
        kCFAllocatorDefault,
        path.c_str(),
        kCFStringEncodingUTF8);
    if (!pathString) {
        error = "Reference path is not valid UTF-8";
        return false;
    }
    CFURLRef url = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,
        pathString,
        kCFURLPOSIXPathStyle,
        false);
    CFRelease(pathString);
    if (!url) {
        error = "Could not create a file URL for the reference";
        return false;
    }
    CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!source) {
        error = "ImageIO could not open the reference still";
        return false;
    }
    CGImageRef cgImage = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);
    if (!cgImage) {
        error = "ImageIO could not decode the reference still";
        return false;
    }
    const std::size_t width = CGImageGetWidth(cgImage);
    const std::size_t height = CGImageGetHeight(cgImage);
    if (width == 0 || height == 0 || width > 32768 || height > 32768) {
        CGImageRelease(cgImage);
        error = "Reference still dimensions are unsupported";
        return false;
    }

    std::vector<unsigned char> bytes(width * height * 4u, 0u);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef context = CGBitmapContextCreate(
        bytes.data(),
        width,
        height,
        8,
        width * 4u,
        colorSpace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(colorSpace);
    if (!context) {
        CGImageRelease(cgImage);
        error = "Could not allocate the reference image buffer";
        return false;
    }
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextTranslateCTM(context, 0.0, static_cast<CGFloat>(height));
    CGContextScaleCTM(context, 1.0, -1.0);
    CGContextDrawImage(
        context,
        CGRectMake(0.0, 0.0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)),
        cgImage);
    CGContextRelease(context);
    CGImageRelease(cgImage);

    std::vector<Pixel> pixels(width * height);
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const float alpha = bytes[index * 4u + 3u] / 255.0f;
        const float inverseAlpha = alpha > 0.0001f ? 1.0f / alpha : 0.0f;
        pixels[index] = Pixel{
            clamp(bytes[index * 4u] / 255.0f * inverseAlpha, 0.0f, 1.0f),
            clamp(bytes[index * 4u + 1u] / 255.0f * inverseAlpha, 0.0f, 1.0f),
            clamp(bytes[index * 4u + 2u] / 255.0f * inverseAlpha, 0.0f, 1.0f),
            alpha,
        };
    }
    image = std::make_shared<ReferenceImage>(
        static_cast<int>(width),
        static_cast<int>(height),
        std::move(pixels));
    return true;
#elif defined(_WIN32)
    const std::wstring widePath = utf8ToWide(path);
    if (widePath.empty()) {
        error = "Reference path is not valid UTF-8";
        return false;
    }
    const HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(initializeResult);
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result)) {
        result = factory->CreateDecoderFromFilename(
            widePath.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &decoder);
    }
    if (SUCCEEDED(result)) {
        result = decoder->GetFrame(0, &frame);
    }
    UINT width = 0;
    UINT height = 0;
    if (SUCCEEDED(result)) {
        result = frame->GetSize(&width, &height);
    }
    if (SUCCEEDED(result) && (width == 0 || height == 0 || width > 32768 || height > 32768)) {
        result = E_INVALIDARG;
    }
    if (SUCCEEDED(result)) {
        result = factory->CreateFormatConverter(&converter);
    }
    if (SUCCEEDED(result)) {
        result = converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
    }
    std::vector<unsigned char> bytes;
    if (SUCCEEDED(result)) {
        bytes.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
        result = converter->CopyPixels(
            nullptr,
            width * 4u,
            static_cast<UINT>(bytes.size()),
            bytes.data());
    }
    if (SUCCEEDED(result)) {
        std::vector<Pixel> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
        for (std::size_t index = 0; index < pixels.size(); ++index) {
            pixels[index] = Pixel{
                bytes[index * 4u] / 255.0f,
                bytes[index * 4u + 1u] / 255.0f,
                bytes[index * 4u + 2u] / 255.0f,
                bytes[index * 4u + 3u] / 255.0f,
            };
        }
        image = std::make_shared<ReferenceImage>(
            static_cast<int>(width),
            static_cast<int>(height),
            std::move(pixels));
    } else {
        error = "Windows Imaging Component could not decode the reference still";
    }
    releaseCom(converter);
    releaseCom(frame);
    releaseCom(decoder);
    releaseCom(factory);
    if (uninitialize) {
        CoUninitialize();
    }
    return SUCCEEDED(result);
#else
    (void)path;
    (void)image;
    error = "Direct reference image loading is not available on this platform; use a .bwlook profile";
    return false;
#endif
}

} // namespace buckswood_lookdna
