#include <algorithm>
#include <cstring>
#include <exception>
#include <new>
#include <thread>
#include <type_traits>
#include <vector>

#include "FilmEmulationCore.h"

#include "ofxImageEffect.h"
#include "ofxMemory.h"
#include "ofxMultiThread.h"
#include "ofxParam.h"
#include "ofxPixels.h"

#if defined __APPLE__ || defined __linux__ || defined __FreeBSD__
#  define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#  define EXPORT OfxExport
#else
#  error Unsupported platform
#endif

namespace {

OfxHost* gHost = nullptr;
OfxImageEffectSuiteV1* gEffectHost = nullptr;
OfxPropertySuiteV1* gPropHost = nullptr;
OfxParameterSuiteV1* gParamHost = nullptr;

constexpr const char* kPluginIdentifier = "com.buckswood.film.emulation";
constexpr const char* kSourceFrameRangeProp = "OfxImageClipPropFrameRange_Source";
constexpr int kPluginMajorVersion = 2;
constexpr int kPluginMinorVersion = 0;

struct ImageInfo {
    void* data = nullptr;
    int rowBytes = 0;
    OfxRectI bounds{0, 0, 0, 0};
    char* pixelDepth = nullptr;
};

template <typename T>
T* pixelAddress(T* base, const OfxRectI& rect, int x, int y, int rowBytes)
{
    if (!base || x < rect.x1 || x >= rect.x2 || y < rect.y1 || y >= rect.y2) {
        return nullptr;
    }
    char* row = reinterpret_cast<char*>(base) + (y - rect.y1) * rowBytes;
    return reinterpret_cast<T*>(row) + (x - rect.x1);
}

bool readImageInfo(OfxPropertySetHandle image, ImageInfo& info)
{
    return gPropHost->propGetPointer(image, kOfxImagePropData, 0, &info.data) == kOfxStatOK &&
           gPropHost->propGetInt(image, kOfxImagePropRowBytes, 0, &info.rowBytes) == kOfxStatOK &&
           gPropHost->propGetIntN(image, kOfxImagePropBounds, 4, &info.bounds.x1) == kOfxStatOK &&
           gPropHost->propGetString(image, kOfxImageEffectPropPixelDepth, 0, &info.pixelDepth) == kOfxStatOK;
}

float clamp01(float value)
{
    return std::min(1.0f, std::max(0.0f, value));
}

unsigned char toByte(float value)
{
    return static_cast<unsigned char>(clamp01(value) * 255.0f + 0.5f);
}

template <typename DstPixel>
void writeTransparent(DstPixel* pixel)
{
    if constexpr (std::is_same<DstPixel, OfxRGBAColourF>::value) {
        *pixel = OfxRGBAColourF{0.0f, 0.0f, 0.0f, 0.0f};
    } else {
        *pixel = OfxRGBAColourB{0, 0, 0, 0};
    }
}

template <typename DstPixel>
OfxStatus fillTransparent(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& dstInfo)
{
    auto* dst = reinterpret_cast<DstPixel*>(dstInfo.data);
    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        if (gEffectHost->abort(instance)) {
            break;
        }
        auto* dstPix = pixelAddress(dst, dstInfo.bounds, renderWindow.x1, y, dstInfo.rowBytes);
        for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
            (void)x;
            if (dstPix) {
                writeTransparent(dstPix);
                ++dstPix;
            }
        }
    }
    return kOfxStatOK;
}

OfxStatus fillTransparentByDepth(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& dstInfo)
{
    if (std::strcmp(dstInfo.pixelDepth, kOfxBitDepthFloat) == 0) {
        return fillTransparent<OfxRGBAColourF>(instance, renderWindow, dstInfo);
    }
    if (std::strcmp(dstInfo.pixelDepth, kOfxBitDepthByte) == 0) {
        return fillTransparent<OfxRGBAColourB>(instance, renderWindow, dstInfo);
    }
    return kOfxStatErrUnsupported;
}

template <typename SrcPixel>
bool sourceLooksLikeBlankAdjustment(const ImageInfo& srcInfo)
{
    const int width = srcInfo.bounds.x2 - srcInfo.bounds.x1;
    const int height = srcInfo.bounds.y2 - srcInfo.bounds.y1;
    if (width <= 0 || height <= 0) {
        return false;
    }

    auto* src = reinterpret_cast<SrcPixel*>(srcInfo.data);
    float maxRgb = 0.0f;
    float alphaSum = 0.0f;
    int samples = 0;
    const int grid = 7;
    for (int gy = 0; gy < grid; ++gy) {
        const int y = srcInfo.bounds.y1 + (height - 1) * gy / (grid - 1);
        for (int gx = 0; gx < grid; ++gx) {
            const int x = srcInfo.bounds.x1 + (width - 1) * gx / (grid - 1);
            auto* p = pixelAddress(src, srcInfo.bounds, x, y, srcInfo.rowBytes);
            if (!p) {
                continue;
            }
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            float a = 0.0f;
            if constexpr (std::is_same<SrcPixel, OfxRGBAColourF>::value) {
                r = p->r;
                g = p->g;
                b = p->b;
                a = p->a;
            } else {
                r = p->r / 255.0f;
                g = p->g / 255.0f;
                b = p->b / 255.0f;
                a = p->a / 255.0f;
            }
            maxRgb = std::max(maxRgb, std::max(r, std::max(g, b)));
            alphaSum += a;
            ++samples;
            if (maxRgb > 0.015f) {
                return false;
            }
        }
    }

    return samples > 0 && (alphaSum / samples) > 0.75f && maxRgb < 0.015f;
}

bool sourceLooksLikeBlankAdjustmentByDepth(const ImageInfo& srcInfo)
{
    if (std::strcmp(srcInfo.pixelDepth, kOfxBitDepthFloat) == 0) {
        return sourceLooksLikeBlankAdjustment<OfxRGBAColourF>(srcInfo);
    }
    if (std::strcmp(srcInfo.pixelDepth, kOfxBitDepthByte) == 0) {
        return sourceLooksLikeBlankAdjustment<OfxRGBAColourB>(srcInfo);
    }
    return false;
}

double doubleParamAtTime(OfxImageEffectHandle instance, const char* name, OfxTime time, double fallback)
{
    OfxParamSetHandle paramSet = nullptr;
    OfxParamHandle param = nullptr;
    if (gEffectHost->getParamSet(instance, &paramSet) != kOfxStatOK ||
        gParamHost->paramGetHandle(paramSet, name, &param, nullptr) != kOfxStatOK) {
        return fallback;
    }

    double value = fallback;
    if (gParamHost->paramGetValueAtTime(param, time, &value) != kOfxStatOK) {
        return fallback;
    }
    return value;
}

int intParamAtTime(OfxImageEffectHandle instance, const char* name, OfxTime time, int fallback)
{
    OfxParamSetHandle paramSet = nullptr;
    OfxParamHandle param = nullptr;
    if (gEffectHost->getParamSet(instance, &paramSet) != kOfxStatOK ||
        gParamHost->paramGetHandle(paramSet, name, &param, nullptr) != kOfxStatOK) {
        return fallback;
    }

    int value = fallback;
    if (gParamHost->paramGetValueAtTime(param, time, &value) != kOfxStatOK) {
        return fallback;
    }
    return value;
}

buckswood_film::Controls controlsAtTime(OfxImageEffectHandle instance, OfxTime time)
{
    buckswood_film::Controls c{};
    c.inputSpace = intParamAtTime(instance, "inputSpace", time, 1);
    c.stockPreset = intParamAtTime(instance, "stockPreset", time, 8);
    c.printPreset = intParamAtTime(instance, "printPreset", time, 6);
    c.processMode = intParamAtTime(instance, "processMode", time, 0);
    c.grainProfile = intParamAtTime(instance, "grainProfile", time, 3);
    c.halationProfile = intParamAtTime(instance, "halationProfile", time, 2);
    c.bloomProfile = intParamAtTime(instance, "bloomProfile", time, 2);
    c.grainMode = intParamAtTime(instance, "grainMode", time, 1);
    c.viewMode = intParamAtTime(instance, "viewMode", time, 0);
    c.exposure = static_cast<float>(doubleParamAtTime(instance, "exposure", time, 0.0));
    c.pushPull = static_cast<float>(doubleParamAtTime(instance, "pushPull", time, 0.0));
    c.density = static_cast<float>(doubleParamAtTime(instance, "density", time, 0.18));
    c.contrast = static_cast<float>(doubleParamAtTime(instance, "contrast", time, 1.00));
    c.developerContrast = static_cast<float>(doubleParamAtTime(instance, "developerContrast", time, 0.10));
    c.developerGamma = static_cast<float>(doubleParamAtTime(instance, "developerGamma", time, 0.0));
    c.colorSeparation = static_cast<float>(doubleParamAtTime(instance, "colorSeparation", time, 0.30));
    c.colorBoost = static_cast<float>(doubleParamAtTime(instance, "colorBoost", time, 0.04));
    c.saturation = static_cast<float>(doubleParamAtTime(instance, "saturation", time, 0.94));
    c.temperature = static_cast<float>(doubleParamAtTime(instance, "temperature", time, 0.0));
    c.tint = static_cast<float>(doubleParamAtTime(instance, "tint", time, 0.0));
    c.subtractiveColor = static_cast<float>(doubleParamAtTime(instance, "subtractiveColor", time, 0.34));
    c.interlayer = static_cast<float>(doubleParamAtTime(instance, "interlayer", time, 0.34));
    c.neutralizeCurves = static_cast<float>(doubleParamAtTime(instance, "neutralizeCurves", time, 0.0));
    c.bleachBypass = static_cast<float>(doubleParamAtTime(instance, "bleachBypass", time, 0.0));
    c.filmCompression = static_cast<float>(doubleParamAtTime(instance, "filmCompression", time, 0.38));
    c.compressionWhitePoint = static_cast<float>(doubleParamAtTime(instance, "compressionWhitePoint", time, 1.08));
    c.compressionRange = static_cast<float>(doubleParamAtTime(instance, "compressionRange", time, 0.55));
    c.compressionColorDensity = static_cast<float>(doubleParamAtTime(instance, "compressionColorDensity", time, 0.48));
    c.expandBlackPoint = static_cast<float>(doubleParamAtTime(instance, "expandBlackPoint", time, 0.0));
    c.expandWhitePoint = static_cast<float>(doubleParamAtTime(instance, "expandWhitePoint", time, 0.0));
    c.printerCyan = static_cast<float>(doubleParamAtTime(instance, "printerCyan", time, 0.0));
    c.printerMagenta = static_cast<float>(doubleParamAtTime(instance, "printerMagenta", time, 0.0));
    c.printerYellow = static_cast<float>(doubleParamAtTime(instance, "printerYellow", time, 0.0));
    c.highlightRolloff = static_cast<float>(doubleParamAtTime(instance, "highlightRolloff", time, 0.62));
    c.blackLift = static_cast<float>(doubleParamAtTime(instance, "blackLift", time, 0.0));
    c.halation = static_cast<float>(doubleParamAtTime(instance, "halation", time, 0.12));
    c.halationLocal = static_cast<float>(doubleParamAtTime(instance, "halationLocal", time, 0.70));
    c.halationGlobal = static_cast<float>(doubleParamAtTime(instance, "halationGlobal", time, 0.18));
    c.halationRedShift = static_cast<float>(doubleParamAtTime(instance, "halationRedShift", time, 0.72));
    c.bloom = static_cast<float>(doubleParamAtTime(instance, "bloom", time, 0.05));
    c.bloomThreshold = static_cast<float>(doubleParamAtTime(instance, "bloomThreshold", time, 0.78));
    c.bloomSpread = static_cast<float>(doubleParamAtTime(instance, "bloomSpread", time, 0.42));
    c.grain = static_cast<float>(doubleParamAtTime(instance, "grain", time, 0.18));
    c.grainSize = static_cast<float>(doubleParamAtTime(instance, "grainSize", time, 0.85));
    c.grainShadows = static_cast<float>(doubleParamAtTime(instance, "grainShadows", time, 0.70));
    c.grainMidtones = static_cast<float>(doubleParamAtTime(instance, "grainMidtones", time, 0.55));
    c.grainHighlights = static_cast<float>(doubleParamAtTime(instance, "grainHighlights", time, 0.42));
    c.grainChroma = static_cast<float>(doubleParamAtTime(instance, "grainChroma", time, 0.26));
    c.filmResolution = static_cast<float>(doubleParamAtTime(instance, "filmResolution", time, 0.82));
    c.gateWeave = static_cast<float>(doubleParamAtTime(instance, "gateWeave", time, 0.04));
    c.dust = static_cast<float>(doubleParamAtTime(instance, "dust", time, 0.0));
    c.scratches = static_cast<float>(doubleParamAtTime(instance, "scratches", time, 0.0));
    c.flicker = static_cast<float>(doubleParamAtTime(instance, "flicker", time, 0.0));
    c.filmBreath = static_cast<float>(doubleParamAtTime(instance, "filmBreath", time, 0.08));
    c.temporalReconstruction = static_cast<float>(doubleParamAtTime(instance, "temporalReconstruction", time, 0.0));
    c.temporalStability = static_cast<float>(doubleParamAtTime(instance, "temporalStability", time, 0.62));
    c.temporalMotionGuard = static_cast<float>(doubleParamAtTime(instance, "temporalMotionGuard", time, 0.82));
    c.skinProtect = static_cast<float>(doubleParamAtTime(instance, "skinProtect", time, 0.72));
    c.outputMix = static_cast<float>(doubleParamAtTime(instance, "outputMix", time, 0.75));
    return c;
}

class FloatSampler final : public buckswood_film::Sampler {
public:
    explicit FloatSampler(const ImageInfo& info)
        : info_(info)
        , base_(reinterpret_cast<OfxRGBAColourF*>(info.data))
    {
    }

    buckswood_film::Pixel sample(float x, float y) const override
    {
        const float minX = static_cast<float>(info_.bounds.x1);
        const float minY = static_cast<float>(info_.bounds.y1);
        const float maxX = static_cast<float>(info_.bounds.x2 - 1);
        const float maxY = static_cast<float>(info_.bounds.y2 - 1);
        if (x < minX || x > maxX || y < minY || y > maxY) {
            return buckswood_film::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }

        const int x0 = static_cast<int>(x);
        const int y0 = static_cast<int>(y);
        const int x1 = std::min(info_.bounds.x2 - 1, x0 + 1);
        const int y1 = std::min(info_.bounds.y2 - 1, y0 + 1);
        const float tx = x - static_cast<float>(x0);
        const float ty = y - static_cast<float>(y0);
        const buckswood_film::Pixel p00 = read(x0, y0);
        const buckswood_film::Pixel p10 = read(x1, y0);
        const buckswood_film::Pixel p01 = read(x0, y1);
        const buckswood_film::Pixel p11 = read(x1, y1);
        const float u = 1.0f - tx;
        const float v = 1.0f - ty;
        return buckswood_film::Pixel{
            (p00.r * u + p10.r * tx) * v + (p01.r * u + p11.r * tx) * ty,
            (p00.g * u + p10.g * tx) * v + (p01.g * u + p11.g * tx) * ty,
            (p00.b * u + p10.b * tx) * v + (p01.b * u + p11.b * tx) * ty,
            (p00.a * u + p10.a * tx) * v + (p01.a * u + p11.a * tx) * ty,
        };
    }

private:
    buckswood_film::Pixel read(int x, int y) const
    {
        auto* p = pixelAddress(base_, info_.bounds, x, y, info_.rowBytes);
        if (!p) {
            return buckswood_film::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }
        return buckswood_film::Pixel{p->r, p->g, p->b, p->a};
    }

    const ImageInfo& info_;
    OfxRGBAColourF* base_;
};

class ByteSampler final : public buckswood_film::Sampler {
public:
    explicit ByteSampler(const ImageInfo& info)
        : info_(info)
        , base_(reinterpret_cast<OfxRGBAColourB*>(info.data))
    {
    }

    buckswood_film::Pixel sample(float x, float y) const override
    {
        const float minX = static_cast<float>(info_.bounds.x1);
        const float minY = static_cast<float>(info_.bounds.y1);
        const float maxX = static_cast<float>(info_.bounds.x2 - 1);
        const float maxY = static_cast<float>(info_.bounds.y2 - 1);
        if (x < minX || x > maxX || y < minY || y > maxY) {
            return buckswood_film::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }

        const int x0 = static_cast<int>(x);
        const int y0 = static_cast<int>(y);
        const int x1 = std::min(info_.bounds.x2 - 1, x0 + 1);
        const int y1 = std::min(info_.bounds.y2 - 1, y0 + 1);
        const float tx = x - static_cast<float>(x0);
        const float ty = y - static_cast<float>(y0);
        const buckswood_film::Pixel p00 = read(x0, y0);
        const buckswood_film::Pixel p10 = read(x1, y0);
        const buckswood_film::Pixel p01 = read(x0, y1);
        const buckswood_film::Pixel p11 = read(x1, y1);
        const float u = 1.0f - tx;
        const float v = 1.0f - ty;
        return buckswood_film::Pixel{
            (p00.r * u + p10.r * tx) * v + (p01.r * u + p11.r * tx) * ty,
            (p00.g * u + p10.g * tx) * v + (p01.g * u + p11.g * tx) * ty,
            (p00.b * u + p10.b * tx) * v + (p01.b * u + p11.b * tx) * ty,
            (p00.a * u + p10.a * tx) * v + (p01.a * u + p11.a * tx) * ty,
        };
    }

private:
    buckswood_film::Pixel read(int x, int y) const
    {
        auto* p = pixelAddress(base_, info_.bounds, x, y, info_.rowBytes);
        if (!p) {
            return buckswood_film::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }
        return buckswood_film::Pixel{p->r / 255.0f, p->g / 255.0f, p->b / 255.0f, p->a / 255.0f};
    }

    const ImageInfo& info_;
    OfxRGBAColourB* base_;
};

unsigned renderThreadCount(int rowCount)
{
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    return std::min<unsigned>(hw, static_cast<unsigned>(std::max(1, rowCount)));
}

template <typename DstPixel, typename SamplerT>
OfxStatus renderTyped(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& srcInfo,
    const ImageInfo* previous2Info,
    const ImageInfo* previousInfo,
    const ImageInfo* nextInfo,
    const ImageInfo* next2Info,
    const ImageInfo& dstInfo,
    const buckswood_film::FrameInfo& frame,
    const buckswood_film::Controls& controls)
{
    const SamplerT sampler(srcInfo);
    const SamplerT previous2Sampler(previous2Info ? *previous2Info : srcInfo);
    const SamplerT previousSampler(previousInfo ? *previousInfo : srcInfo);
    const SamplerT nextSampler(nextInfo ? *nextInfo : srcInfo);
    const SamplerT next2Sampler(next2Info ? *next2Info : srcInfo);
    const buckswood_film::TemporalContext temporal{
        previous2Info ? static_cast<const buckswood_film::Sampler*>(&previous2Sampler) : nullptr,
        previousInfo ? static_cast<const buckswood_film::Sampler*>(&previousSampler) : nullptr,
        nextInfo ? static_cast<const buckswood_film::Sampler*>(&nextSampler) : nullptr,
        next2Info ? static_cast<const buckswood_film::Sampler*>(&next2Sampler) : nullptr,
        previous2Info != nullptr,
        previousInfo != nullptr,
        nextInfo != nullptr,
        next2Info != nullptr,
    };
    auto* dst = reinterpret_cast<DstPixel*>(dstInfo.data);

    auto renderRows = [&](int yBegin, int yEnd) {
        for (int y = yBegin; y < yEnd; ++y) {
            if (gEffectHost->abort(instance)) {
                break;
            }

            auto* dstPix = pixelAddress(dst, dstInfo.bounds, renderWindow.x1, y, dstInfo.rowBytes);
            for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
                if (dstPix) {
                    buckswood_film::Pixel out = buckswood_film::FilmEmulationCore::processPixel(
                        sampler,
                        x,
                        y,
                        frame,
                        controls,
                        &temporal);
                    if constexpr (std::is_same<DstPixel, OfxRGBAColourF>::value) {
                        dstPix->r = out.r;
                        dstPix->g = out.g;
                        dstPix->b = out.b;
                        dstPix->a = out.a;
                    } else {
                        dstPix->r = toByte(out.r);
                        dstPix->g = toByte(out.g);
                        dstPix->b = toByte(out.b);
                        dstPix->a = toByte(out.a);
                    }
                    ++dstPix;
                }
            }
        }
    };

    const int rowCount = renderWindow.y2 - renderWindow.y1;
    const unsigned threadCount = renderThreadCount(rowCount);
    if (threadCount <= 1) {
        renderRows(renderWindow.y1, renderWindow.y2);
        return kOfxStatOK;
    }

    const int rowsPerBand = (rowCount + static_cast<int>(threadCount) - 1) / static_cast<int>(threadCount);
    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    for (unsigned t = 0; t < threadCount; ++t) {
        const int yBegin = renderWindow.y1 + static_cast<int>(t) * rowsPerBand;
        const int yEnd = std::min(renderWindow.y2, yBegin + rowsPerBand);
        if (yBegin >= yEnd) {
            break;
        }
        workers.emplace_back(renderRows, yBegin, yEnd);
    }
    for (auto& worker : workers) {
        worker.join();
    }
    return kOfxStatOK;
}

class NoImageEx {};

OfxStatus render(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs)
{
    OfxTime time = 0.0;
    OfxRectI renderWindow{0, 0, 0, 0};
    gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    gPropHost->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

    OfxImageClipHandle outputClip = nullptr;
    OfxImageClipHandle sourceClip = nullptr;
    gEffectHost->clipGetHandle(instance, kOfxImageEffectOutputClipName, &outputClip, nullptr);
    gEffectHost->clipGetHandle(instance, kOfxImageEffectSimpleSourceClipName, &sourceClip, nullptr);

    OfxPropertySetHandle outputImage = nullptr;
    OfxPropertySetHandle sourceImage = nullptr;
    OfxPropertySetHandle previous2Image = nullptr;
    OfxPropertySetHandle previousImage = nullptr;
    OfxPropertySetHandle nextImage = nullptr;
    OfxPropertySetHandle next2Image = nullptr;
    OfxStatus status = kOfxStatOK;

    try {
        if (gEffectHost->clipGetImage(outputClip, time, nullptr, &outputImage) != kOfxStatOK) {
            throw NoImageEx();
        }

        ImageInfo dstInfo;
        if (!readImageInfo(outputImage, dstInfo)) {
            return kOfxStatFailed;
        }

        if (gEffectHost->clipGetImage(sourceClip, time, nullptr, &sourceImage) != kOfxStatOK) {
            status = fillTransparentByDepth(instance, renderWindow, dstInfo);
        } else {
            ImageInfo srcInfo;
            if (!readImageInfo(sourceImage, srcInfo)) {
                return kOfxStatFailed;
            }

            const int adjustmentLayerGuard = intParamAtTime(instance, "adjustmentLayerGuard", time, 1);
            if (adjustmentLayerGuard && sourceLooksLikeBlankAdjustmentByDepth(srcInfo)) {
                status = fillTransparentByDepth(instance, renderWindow, dstInfo);
            } else {
                buckswood_film::FrameInfo frame{
                    dstInfo.bounds.x2 - dstInfo.bounds.x1,
                    dstInfo.bounds.y2 - dstInfo.bounds.y1,
                    static_cast<int>(time),
                };
                const buckswood_film::Controls controls = controlsAtTime(instance, time);
                const int temporalOffset = std::max(1, intParamAtTime(instance, "temporalOffset", time, 0) + 1);
                ImageInfo previous2Info;
                ImageInfo previousInfo;
                ImageInfo nextInfo;
                ImageInfo next2Info;
                ImageInfo* previous2Ptr = nullptr;
                ImageInfo* previousPtr = nullptr;
                ImageInfo* nextPtr = nullptr;
                ImageInfo* next2Ptr = nullptr;
                if (controls.temporalReconstruction > 0.0001f) {
                    if (gEffectHost->clipGetImage(sourceClip, time - temporalOffset * 2, nullptr, &previous2Image) == kOfxStatOK &&
                        readImageInfo(previous2Image, previous2Info) &&
                        std::strcmp(previous2Info.pixelDepth, srcInfo.pixelDepth) == 0) {
                        previous2Ptr = &previous2Info;
                    }
                    if (gEffectHost->clipGetImage(sourceClip, time - temporalOffset, nullptr, &previousImage) == kOfxStatOK &&
                        readImageInfo(previousImage, previousInfo) &&
                        std::strcmp(previousInfo.pixelDepth, srcInfo.pixelDepth) == 0) {
                        previousPtr = &previousInfo;
                    }
                    if (gEffectHost->clipGetImage(sourceClip, time + temporalOffset, nullptr, &nextImage) == kOfxStatOK &&
                        readImageInfo(nextImage, nextInfo) &&
                        std::strcmp(nextInfo.pixelDepth, srcInfo.pixelDepth) == 0) {
                        nextPtr = &nextInfo;
                    }
                    if (gEffectHost->clipGetImage(sourceClip, time + temporalOffset * 2, nullptr, &next2Image) == kOfxStatOK &&
                        readImageInfo(next2Image, next2Info) &&
                        std::strcmp(next2Info.pixelDepth, srcInfo.pixelDepth) == 0) {
                        next2Ptr = &next2Info;
                    }
                }

                if (std::strcmp(dstInfo.pixelDepth, kOfxBitDepthFloat) == 0 &&
                    std::strcmp(srcInfo.pixelDepth, kOfxBitDepthFloat) == 0) {
                    status = renderTyped<OfxRGBAColourF, FloatSampler>(
                        instance,
                        renderWindow,
                        srcInfo,
                        previous2Ptr,
                        previousPtr,
                        nextPtr,
                        next2Ptr,
                        dstInfo,
                        frame,
                        controls);
                } else if (std::strcmp(dstInfo.pixelDepth, kOfxBitDepthByte) == 0 &&
                           std::strcmp(srcInfo.pixelDepth, kOfxBitDepthByte) == 0) {
                    status = renderTyped<OfxRGBAColourB, ByteSampler>(
                        instance,
                        renderWindow,
                        srcInfo,
                        previous2Ptr,
                        previousPtr,
                        nextPtr,
                        next2Ptr,
                        dstInfo,
                        frame,
                        controls);
                } else {
                    status = kOfxStatErrUnsupported;
                }
            }
        }
    } catch (NoImageEx&) {
        status = gEffectHost->abort(instance) ? kOfxStatOK : kOfxStatFailed;
    }

    if (sourceImage) {
        gEffectHost->clipReleaseImage(sourceImage);
    }
    if (previous2Image) {
        gEffectHost->clipReleaseImage(previous2Image);
    }
    if (previousImage) {
        gEffectHost->clipReleaseImage(previousImage);
    }
    if (nextImage) {
        gEffectHost->clipReleaseImage(nextImage);
    }
    if (next2Image) {
        gEffectHost->clipReleaseImage(next2Image);
    }
    if (outputImage) {
        gEffectHost->clipReleaseImage(outputImage);
    }
    return status;
}

void defineDoubleParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    double defaultValue,
    double minValue,
    double maxValue,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle props = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeDouble, name, &props);
    gPropHost->propSetString(props, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(props, kOfxPropLabel, 0, label);
    gPropHost->propSetString(props, kOfxParamPropDoubleType, 0, kOfxParamDoubleTypePlain);
    gPropHost->propSetDouble(props, kOfxParamPropDefault, 0, defaultValue);
    gPropHost->propSetDouble(props, kOfxParamPropMin, 0, minValue);
    gPropHost->propSetDouble(props, kOfxParamPropMax, 0, maxValue);
    gPropHost->propSetDouble(props, kOfxParamPropDisplayMin, 0, minValue);
    gPropHost->propSetDouble(props, kOfxParamPropDisplayMax, 0, maxValue);
    gPropHost->propSetString(page, kOfxParamPropPageChild, pageIndex, name);
}

void defineChoiceParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int defaultValue,
    const char* const* options,
    int optionCount,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle props = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeChoice, name, &props);
    gPropHost->propSetString(props, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(props, kOfxPropLabel, 0, label);
    gPropHost->propSetInt(props, kOfxParamPropDefault, 0, defaultValue);
    for (int i = 0; i < optionCount; ++i) {
        gPropHost->propSetString(props, kOfxParamPropChoiceOption, i, options[i]);
    }
    gPropHost->propSetString(page, kOfxParamPropPageChild, pageIndex, name);
}

void defineBooleanParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int defaultValue,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle props = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeBoolean, name, &props);
    gPropHost->propSetString(props, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(props, kOfxPropLabel, 0, label);
    gPropHost->propSetInt(props, kOfxParamPropDefault, 0, defaultValue);
    gPropHost->propSetString(page, kOfxParamPropPageChild, pageIndex, name);
}

OfxStatus describeInContext(OfxImageEffectHandle effect)
{
    OfxPropertySetHandle props = nullptr;
    gEffectHost->clipDefine(effect, kOfxImageEffectOutputClipName, &props);
    gPropHost->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);

    gEffectHost->clipDefine(effect, kOfxImageEffectSimpleSourceClipName, &props);
    gPropHost->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);

    OfxParamSetHandle paramSet = nullptr;
    gEffectHost->getParamSet(effect, &paramSet);

    OfxPropertySetHandle page = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypePage, "Main", &page);
    gPropHost->propSetString(page, kOfxPropLabel, 0, "Film Emulation");

    const char* inputSpaces[] = {
        "Rec.709 / Gamma 2.4",
        "AI Footage Rec.709",
        "Timeline / Scene Linear",
        "Sony S-Log3 / S-Gamut3.Cine",
        "ARRI LogC3",
        "ARRI LogC4",
        "Apple Log",
        "BMD Film Gen 5 / DWG",
    };
    const char* stocks[] = {
        "Neutral / Manual",
        "35mm Clean Negative",
        "500T Tungsten Inspired",
        "250D Daylight Inspired",
        "Soft Eterna Inspired",
        "16mm Documentary",
        "Bleach Bypass",
        "B/W Panchromatic",
        "AI Footage Deplastic Film",
        "AI Skin Recovery Negative",
        "AI Dream Grain",
        "AI Clean Commercial Film",
        "Large Format Clean",
        "Desert Epic Negative",
        "Green Cyber Print Negative",
        "Gritty Night 500T",
    };
    const char* prints[] = {
        "No Print / Manual",
        "2383 Theater Print Inspired",
        "3513 Softer Print Inspired",
        "Clean Scan Print",
        "Warm Show Print",
        "Cool Silver Retention",
        "AI Soft Print",
        "Dense Theatrical",
        "B/W Silver Print",
    };
    const char* processModes[] = {
        "Full Process",
        "Color Only",
        "Texture Only",
    };
    const char* grainProfiles[] = {
        "Manual",
        "8mm 500",
        "16mm 500",
        "35mm 500",
        "35mm 250",
        "65mm 50",
    };
    const char* halationProfiles[] = {
        "Manual",
        "8mm Emulsion",
        "16mm Emulsion",
        "35mm Emulsion",
        "65mm Subtle",
        "No Remjet",
    };
    const char* bloomProfiles[] = {
        "Manual",
        "8mm Diffusion",
        "16mm Diffusion",
        "35mm Diffusion",
        "65mm Clean",
    };
    const char* grainModes[] = {
        "Classic Negative Grain",
        "Fine Scan Grain",
        "Chunky 16mm Grain",
        "Monochrome Tight Grain",
    };
    const char* temporalOffsets[] = {
        "1 Frame",
        "2 Frames",
        "3 Frames",
    };
    const char* views[] = {
        "Final Image",
        "Grain Matte",
        "Halation / Bloom Matte",
        "Density Map",
        "Temporal Reconstruction Map",
    };

    defineChoiceParam(paramSet, "inputSpace", "Input Space", 1, inputSpaces, 8, 0, page);
    defineChoiceParam(paramSet, "stockPreset", "Film / AI Stock", 8, stocks, 16, 1, page);
    defineChoiceParam(paramSet, "printPreset", "Print Stock", 6, prints, 9, 2, page);
    defineChoiceParam(paramSet, "processMode", "Process Mode", 0, processModes, 3, 3, page);
    defineDoubleParam(paramSet, "exposure", "Input Exposure", 0.0, -2.0, 2.0, 4, page);
    defineDoubleParam(paramSet, "pushPull", "Push / Pull", 0.0, -2.0, 2.0, 5, page);
    defineDoubleParam(paramSet, "density", "Film Density", 0.18, -0.5, 1.5, 6, page);
    defineDoubleParam(paramSet, "contrast", "Base Contrast", 1.00, 0.4, 1.8, 7, page);
    defineDoubleParam(paramSet, "developerContrast", "Developer Contrast", 0.10, -1.0, 1.0, 8, page);
    defineDoubleParam(paramSet, "developerGamma", "Developer Gamma", 0.0, -1.0, 1.0, 9, page);
    defineDoubleParam(paramSet, "colorSeparation", "Color Separation", 0.30, 0.0, 1.0, 10, page);
    defineDoubleParam(paramSet, "colorBoost", "Color Boost", 0.04, -1.0, 1.0, 11, page);
    defineDoubleParam(paramSet, "saturation", "Saturation", 0.94, 0.0, 1.6, 12, page);
    defineDoubleParam(paramSet, "subtractiveColor", "Subtractive Color", 0.34, 0.0, 1.0, 13, page);
    defineDoubleParam(paramSet, "interlayer", "Interlayer", 0.34, 0.0, 1.0, 14, page);
    defineDoubleParam(paramSet, "neutralizeCurves", "Neutralize Curves", 0.0, 0.0, 1.0, 15, page);
    defineDoubleParam(paramSet, "bleachBypass", "Bleach Bypass", 0.0, 0.0, 1.0, 16, page);
    defineDoubleParam(paramSet, "filmCompression", "Film Compression", 0.38, 0.0, 1.0, 17, page);
    defineDoubleParam(paramSet, "compressionWhitePoint", "Compression White Point", 1.08, 0.35, 4.0, 18, page);
    defineDoubleParam(paramSet, "compressionRange", "Compression Range", 0.55, 0.05, 1.0, 19, page);
    defineDoubleParam(paramSet, "compressionColorDensity", "Compression Color Density", 0.48, 0.0, 1.0, 20, page);
    defineDoubleParam(paramSet, "expandBlackPoint", "Expand Black Point", 0.0, -1.0, 1.0, 21, page);
    defineDoubleParam(paramSet, "expandWhitePoint", "Expand White Point", 0.0, -1.0, 1.0, 22, page);
    defineDoubleParam(paramSet, "printerCyan", "Printer Cyan / Red", 0.0, -1.0, 1.0, 23, page);
    defineDoubleParam(paramSet, "printerMagenta", "Printer Magenta / Green", 0.0, -1.0, 1.0, 24, page);
    defineDoubleParam(paramSet, "printerYellow", "Printer Yellow / Blue", 0.0, -1.0, 1.0, 25, page);
    defineDoubleParam(paramSet, "temperature", "Temperature", 0.0, -1.0, 1.0, 26, page);
    defineDoubleParam(paramSet, "tint", "Tint", 0.0, -1.0, 1.0, 27, page);
    defineDoubleParam(paramSet, "highlightRolloff", "Highlight Rolloff", 0.62, 0.0, 1.0, 28, page);
    defineDoubleParam(paramSet, "blackLift", "Black Lift", 0.0, -1.0, 1.0, 29, page);
    defineChoiceParam(paramSet, "halationProfile", "Halation Profile", 2, halationProfiles, 6, 30, page);
    defineDoubleParam(paramSet, "halation", "Halation", 0.12, 0.0, 1.0, 31, page);
    defineDoubleParam(paramSet, "halationLocal", "Halation Locality", 0.70, 0.0, 1.0, 32, page);
    defineDoubleParam(paramSet, "halationGlobal", "Halation Global", 0.18, 0.0, 1.0, 33, page);
    defineDoubleParam(paramSet, "halationRedShift", "Halation Red Shift", 0.72, 0.0, 1.0, 34, page);
    defineChoiceParam(paramSet, "bloomProfile", "Bloom Profile", 2, bloomProfiles, 5, 35, page);
    defineDoubleParam(paramSet, "bloom", "Bloom", 0.05, 0.0, 1.0, 36, page);
    defineDoubleParam(paramSet, "bloomThreshold", "Bloom Threshold", 0.78, 0.2, 2.0, 37, page);
    defineDoubleParam(paramSet, "bloomSpread", "Bloom Spread", 0.42, 0.0, 1.0, 38, page);
    defineChoiceParam(paramSet, "grainProfile", "Grain Profile", 3, grainProfiles, 6, 39, page);
    defineDoubleParam(paramSet, "grain", "Grain Amount", 0.18, 0.0, 1.0, 40, page);
    defineDoubleParam(paramSet, "grainSize", "Grain Size", 0.85, 0.0, 3.0, 41, page);
    defineChoiceParam(paramSet, "grainMode", "Grain Mode", 1, grainModes, 4, 42, page);
    defineDoubleParam(paramSet, "grainShadows", "Grain Shadows", 0.70, 0.0, 1.0, 43, page);
    defineDoubleParam(paramSet, "grainMidtones", "Grain Midtones", 0.55, 0.0, 1.0, 44, page);
    defineDoubleParam(paramSet, "grainHighlights", "Grain Highlights", 0.42, 0.0, 1.0, 45, page);
    defineDoubleParam(paramSet, "grainChroma", "Grain Chroma", 0.26, 0.0, 1.0, 46, page);
    defineDoubleParam(paramSet, "filmResolution", "Film Resolution", 0.82, 0.0, 1.0, 47, page);
    defineDoubleParam(paramSet, "gateWeave", "Gate Weave", 0.04, 0.0, 1.0, 48, page);
    defineDoubleParam(paramSet, "dust", "Dust / Specks", 0.0, 0.0, 1.0, 49, page);
    defineDoubleParam(paramSet, "scratches", "Scratches", 0.0, 0.0, 1.0, 50, page);
    defineDoubleParam(paramSet, "flicker", "Exposure Flicker", 0.0, 0.0, 1.0, 51, page);
    defineDoubleParam(paramSet, "filmBreath", "Film Breath", 0.08, 0.0, 1.0, 52, page);
    defineDoubleParam(paramSet, "temporalReconstruction", "Temporal AI Reconstruction", 0.0, 0.0, 1.0, 53, page);
    defineDoubleParam(paramSet, "temporalStability", "Temporal Stability", 0.62, 0.0, 1.0, 54, page);
    defineDoubleParam(paramSet, "temporalMotionGuard", "Temporal Motion Guard", 0.82, 0.0, 1.0, 55, page);
    defineChoiceParam(paramSet, "temporalOffset", "Temporal Radius", 0, temporalOffsets, 3, 56, page);
    defineDoubleParam(paramSet, "skinProtect", "Skin Protect", 0.72, 0.0, 1.0, 57, page);
    defineDoubleParam(paramSet, "outputMix", "Output Mix", 0.75, 0.0, 1.0, 58, page);
    defineChoiceParam(paramSet, "viewMode", "View", 0, views, 5, 59, page);
    defineBooleanParam(paramSet, "adjustmentLayerGuard", "Adjustment Layer Guard", 1, 60, page);

    return kOfxStatOK;
}

OfxStatus describe(OfxImageEffectHandle effect)
{
    OfxPropertySetHandle effectProps = nullptr;
    gEffectHost->getPropertySet(effect, &effectProps);
    gPropHost->propSetInt(effectProps, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 0);
    gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthByte);
    gPropHost->propSetString(effectProps, kOfxPropLabel, 0, "Buckswood Film Emulation v2.0");
    gPropHost->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "Buckswood");
    gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    return kOfxStatOK;
}

OfxStatus onLoad()
{
    if (!gHost) {
        return kOfxStatErrMissingHostFeature;
    }
    gEffectHost = const_cast<OfxImageEffectSuiteV1*>(
        reinterpret_cast<const OfxImageEffectSuiteV1*>(gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1)));
    gPropHost = const_cast<OfxPropertySuiteV1*>(
        reinterpret_cast<const OfxPropertySuiteV1*>(gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1)));
    gParamHost = const_cast<OfxParameterSuiteV1*>(
        reinterpret_cast<const OfxParameterSuiteV1*>(gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1)));
    if (!gEffectHost || !gPropHost || !gParamHost) {
        return kOfxStatErrMissingHostFeature;
    }
    return kOfxStatOK;
}

OfxStatus getFramesNeeded(
    OfxImageEffectHandle instance,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle outArgs)
{
    if (!outArgs) {
        return kOfxStatFailed;
    }
    OfxTime time = 0.0;
    gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    const int temporalOffset = std::max(1, intParamAtTime(instance, "temporalOffset", time, 0) + 1);
    const double ranges[4] = {
        time - temporalOffset * 2,
        time - temporalOffset,
        time + temporalOffset,
        time + temporalOffset * 2,
    };
    gPropHost->propSetDoubleN(outArgs, kSourceFrameRangeProp, 4, ranges);
    return kOfxStatOK;
}

OfxStatus pluginMain(
    const char* action,
    const void* handle,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle outArgs)
{
    try {
        OfxImageEffectHandle effect = (OfxImageEffectHandle)handle;
        if (std::strcmp(action, kOfxActionLoad) == 0) {
            return onLoad();
        }
        if (std::strcmp(action, kOfxActionDescribe) == 0) {
            return describe(effect);
        }
        if (std::strcmp(action, kOfxActionCreateInstance) == 0 ||
            std::strcmp(action, kOfxActionDestroyInstance) == 0) {
            return kOfxStatOK;
        }
        if (std::strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
            return describeInContext(effect);
        }
        if (std::strcmp(action, kOfxImageEffectActionRender) == 0) {
            return render(effect, inArgs);
        }
        if (std::strcmp(action, kOfxImageEffectActionGetFramesNeeded) == 0) {
            return getFramesNeeded(effect, inArgs, outArgs);
        }
    } catch (std::bad_alloc&) {
        return kOfxStatErrMemory;
    } catch (std::exception&) {
        return kOfxStatErrUnknown;
    }
    return kOfxStatReplyDefault;
}

void setHostFunc(OfxHost* hostStruct)
{
    gHost = hostStruct;
}

OfxPlugin plugin = {
    kOfxImageEffectPluginApi,
    1,
    kPluginIdentifier,
    kPluginMajorVersion,
    kPluginMinorVersion,
    setHostFunc,
    pluginMain
};

} // namespace

extern "C" {

EXPORT OfxPlugin* OfxGetPlugin(int nth)
{
    return nth == 0 ? &plugin : nullptr;
}

EXPORT int OfxGetNumberOfPlugins()
{
    return 1;
}

} // extern "C"
