#include "DeJitterCore.h"
#include "DeJitterOverlay.h"
#include "OfxRenderRuntime.h"

#include "ofxDrawSuite.h"
#include "ofxImageEffect.h"
#include "ofxInteract.h"
#include "ofxParam.h"
#include "ofxPixels.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>

#if defined __APPLE__ || defined __linux__ || defined __FreeBSD__
#  define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#  define EXPORT OfxExport
#else
#  error Unsupported platform
#endif

namespace {

using buckswood_dejitter::Bounds;
using buckswood_dejitter::Controls;
using buckswood_dejitter::DeJitterCore;
using buckswood_dejitter::FrameInfo;
using buckswood_dejitter::DeJitterOverlay;
using buckswood_dejitter::OverlayBounds;
using buckswood_dejitter::OverlayDragMode;
using buckswood_dejitter::OverlayParameters;
using buckswood_dejitter::OverlayPoint;
using buckswood_dejitter::OverlayRect;
using buckswood_dejitter::Pixel;
using buckswood_dejitter::Sampler;
using buckswood_dejitter::TemporalContext;
using buckswood_dejitter::TrackingResult;

OfxHost* gHost = nullptr;
OfxImageEffectSuiteV1* gEffectHost = nullptr;
OfxPropertySuiteV1* gPropHost = nullptr;
OfxParameterSuiteV1* gParamHost = nullptr;
OfxMultiThreadSuiteV1* gThreadHost = nullptr;
OfxInteractSuiteV1* gInteractHost = nullptr;
OfxDrawSuiteV1* gDrawHost = nullptr;

constexpr const char* kPluginIdentifier = "com.buckswood.dejitter";
constexpr const char* kSourceFrameRangeProp =
    "OfxImageClipPropFrameRange_Source";
constexpr int kPluginMajorVersion = 1;
constexpr int kPluginMinorVersion = 2;
constexpr float kPi = 3.14159265358979323846f;

struct ImageInfo {
    void* data = nullptr;
    int rowBytes = 0;
    OfxRectI bounds{0, 0, 0, 0};
    char* pixelDepth = nullptr;
    double pixelAspect = 1.0;
};

template <typename T>
T* pixelAddress(
    T* base,
    const OfxRectI& rect,
    int x,
    int y,
    int rowBytes)
{
    if (
        !base ||
        x < rect.x1 ||
        x >= rect.x2 ||
        y < rect.y1 ||
        y >= rect.y2) {
        return nullptr;
    }
    char* row =
        reinterpret_cast<char*>(base) +
        (y - rect.y1) * rowBytes;
    return reinterpret_cast<T*>(row) + (x - rect.x1);
}

bool readImageInfo(
    OfxPropertySetHandle image,
    ImageInfo& info)
{
    const bool valid =
        gPropHost->propGetPointer(
            image,
            kOfxImagePropData,
            0,
            &info.data) == kOfxStatOK &&
        gPropHost->propGetInt(
            image,
            kOfxImagePropRowBytes,
            0,
            &info.rowBytes) == kOfxStatOK &&
        gPropHost->propGetIntN(
            image,
            kOfxImagePropBounds,
            4,
            &info.bounds.x1) == kOfxStatOK &&
        gPropHost->propGetString(
            image,
            kOfxImageEffectPropPixelDepth,
            0,
            &info.pixelDepth) == kOfxStatOK;
    if (valid) {
        gPropHost->propGetDouble(
            image,
            kOfxImagePropPixelAspectRatio,
            0,
            &info.pixelAspect);
        if (info.pixelAspect <= 0.0) {
            info.pixelAspect = 1.0;
        }
    }
    return valid;
}

class ScopedImage {
public:
    ScopedImage() = default;
    ScopedImage(const ScopedImage&) = delete;
    ScopedImage& operator=(const ScopedImage&) = delete;

    ~ScopedImage()
    {
        reset();
    }

    bool fetch(OfxImageClipHandle clip, OfxTime time)
    {
        reset();
        return clip &&
            gEffectHost->clipGetImage(
                clip,
                time,
                nullptr,
                &handle_) == kOfxStatOK;
    }

    void reset()
    {
        if (handle_) {
            gEffectHost->clipReleaseImage(handle_);
            handle_ = nullptr;
        }
    }

    OfxPropertySetHandle get() const
    {
        return handle_;
    }

private:
    OfxPropertySetHandle handle_ = nullptr;
};

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

unsigned char toByte(float value)
{
    return static_cast<unsigned char>(
        clamp01(value) * 255.0f + 0.5f);
}

template <typename NativePixel>
Pixel readNative(const NativePixel& pixel)
{
    if constexpr (
        std::is_same<NativePixel, OfxRGBAColourF>::value) {
        return Pixel{
            pixel.r,
            pixel.g,
            pixel.b,
            pixel.a,
        };
    } else {
        return Pixel{
            pixel.r / 255.0f,
            pixel.g / 255.0f,
            pixel.b / 255.0f,
            pixel.a / 255.0f,
        };
    }
}

void writeNative(OfxRGBAColourF* destination, Pixel pixel)
{
    destination->r = pixel.r;
    destination->g = pixel.g;
    destination->b = pixel.b;
    destination->a = pixel.a;
}

void writeNative(OfxRGBAColourB* destination, Pixel pixel)
{
    destination->r = toByte(pixel.r);
    destination->g = toByte(pixel.g);
    destination->b = toByte(pixel.b);
    destination->a = toByte(pixel.a);
}

Pixel addWeighted(Pixel value, Pixel sample, float weight)
{
    value.r += sample.r * weight;
    value.g += sample.g * weight;
    value.b += sample.b * weight;
    value.a += sample.a * weight;
    return value;
}

float cubicWeight(float distance)
{
    const float x = std::fabs(distance);
    if (x < 1.0f) {
        return 1.5f * x * x * x -
            2.5f * x * x +
            1.0f;
    }
    if (x < 2.0f) {
        return -0.5f * x * x * x +
            2.5f * x * x -
            4.0f * x +
            2.0f;
    }
    return 0.0f;
}

float sinc(float value)
{
    if (std::fabs(value) < 1.0e-6f) {
        return 1.0f;
    }
    const float angle = kPi * value;
    return std::sin(angle) / angle;
}

float lanczosWeight(float distance)
{
    const float x = std::fabs(distance);
    return x < 3.0f ? sinc(x) * sinc(x / 3.0f) : 0.0f;
}

template <typename NativePixel>
class ImageSampler final : public Sampler {
public:
    explicit ImageSampler(const ImageInfo& info)
        : info_(info)
        , base_(reinterpret_cast<NativePixel*>(info.data))
    {
    }

    Pixel sample(float x, float y) const override
    {
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const float fx = x - static_cast<float>(x0);
        const float fy = y - static_cast<float>(y0);
        const Pixel p00 = raw(x0, y0);
        const Pixel p10 = raw(x0 + 1, y0);
        const Pixel p01 = raw(x0, y0 + 1);
        const Pixel p11 = raw(x0 + 1, y0 + 1);
        Pixel result{};
        result = addWeighted(
            result,
            p00,
            (1.0f - fx) * (1.0f - fy));
        result = addWeighted(
            result,
            p10,
            fx * (1.0f - fy));
        result = addWeighted(
            result,
            p01,
            (1.0f - fx) * fy);
        result = addWeighted(result, p11, fx * fy);
        return result;
    }

    Pixel sampleHighQuality(
        float x,
        float y,
        int interpolation) const override
    {
        if (
            interpolation ==
            buckswood_dejitter::InterpolationBilinear) {
            return sample(x, y);
        }

        const int centerX = static_cast<int>(std::floor(x));
        const int centerY = static_cast<int>(std::floor(y));
        const int radius =
            interpolation ==
                buckswood_dejitter::InterpolationLanczos3
            ? 3
            : 2;
        Pixel result{};
        float weightSum = 0.0f;
        for (
            int sampleY = centerY - radius + 1;
            sampleY <= centerY + radius;
            ++sampleY) {
            const float weightY =
                interpolation ==
                    buckswood_dejitter::InterpolationLanczos3
                ? lanczosWeight(
                      y - static_cast<float>(sampleY))
                : cubicWeight(
                      y - static_cast<float>(sampleY));
            for (
                int sampleX = centerX - radius + 1;
                sampleX <= centerX + radius;
                ++sampleX) {
                const float weightX =
                    interpolation ==
                        buckswood_dejitter::InterpolationLanczos3
                    ? lanczosWeight(
                          x - static_cast<float>(sampleX))
                    : cubicWeight(
                          x - static_cast<float>(sampleX));
                const float weight = weightX * weightY;
                result = addWeighted(
                    result,
                    raw(sampleX, sampleY),
                    weight);
                weightSum += weight;
            }
        }
        if (std::fabs(weightSum) > 1.0e-6f) {
            result.r /= weightSum;
            result.g /= weightSum;
            result.b /= weightSum;
            result.a /= weightSum;
        }
        return result;
    }

    Bounds bounds() const override
    {
        return Bounds{
            info_.bounds.x1,
            info_.bounds.y1,
            info_.bounds.x2,
            info_.bounds.y2,
        };
    }

private:
    Pixel raw(int x, int y) const
    {
        const int sampleX = std::clamp(
            x,
            info_.bounds.x1,
            std::max(info_.bounds.x1, info_.bounds.x2 - 1));
        const int sampleY = std::clamp(
            y,
            info_.bounds.y1,
            std::max(info_.bounds.y1, info_.bounds.y2 - 1));
        const NativePixel* pixel = pixelAddress(
            base_,
            info_.bounds,
            sampleX,
            sampleY,
            info_.rowBytes);
        return pixel
            ? readNative(*pixel)
            : Pixel{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const ImageInfo& info_;
    NativePixel* base_;
};

OfxParamHandle paramHandle(
    OfxImageEffectHandle instance,
    const char* name)
{
    OfxParamSetHandle paramSet = nullptr;
    OfxParamHandle param = nullptr;
    if (
        gEffectHost->getParamSet(instance, &paramSet) !=
            kOfxStatOK ||
        gParamHost->paramGetHandle(
            paramSet,
            name,
            &param,
            nullptr) != kOfxStatOK) {
        return nullptr;
    }
    return param;
}

double doubleParamAtTime(
    OfxImageEffectHandle instance,
    const char* name,
    OfxTime time,
    double fallback)
{
    OfxParamHandle param = paramHandle(instance, name);
    double value = fallback;
    return param &&
        gParamHost->paramGetValueAtTime(
            param,
            time,
            &value) == kOfxStatOK
        ? value
        : fallback;
}

int intParamAtTime(
    OfxImageEffectHandle instance,
    const char* name,
    OfxTime time,
    int fallback)
{
    OfxParamHandle param = paramHandle(instance, name);
    int value = fallback;
    return param &&
        gParamHost->paramGetValueAtTime(
            param,
            time,
            &value) == kOfxStatOK
        ? value
        : fallback;
}

void pointParamAtTime(
    OfxImageEffectHandle instance,
    const char* name,
    OfxTime time,
    double fallbackX,
    double fallbackY,
    double& x,
    double& y)
{
    OfxParamHandle param = paramHandle(instance, name);
    x = fallbackX;
    y = fallbackY;
    if (param) {
        gParamHost->paramGetValueAtTime(param, time, &x, &y);
    }
}

Controls controlsAtTime(
    OfxImageEffectHandle instance,
    OfxTime time,
    const ImageInfo& source,
    const double renderScale[2])
{
    Controls controls = DeJitterCore::defaultControls();
    controls.enabled =
        intParamAtTime(instance, "enabled", time, 1) != 0;

    double pointX = 0.0;
    double pointY = 0.0;
    const double fallbackX =
        0.5 * static_cast<double>(
            source.bounds.x1 + source.bounds.x2 - 1) *
        source.pixelAspect /
        std::max(0.0001, renderScale[0]);
    const double fallbackY =
        0.5 * static_cast<double>(
            source.bounds.y1 + source.bounds.y2 - 1) /
        std::max(0.0001, renderScale[1]);
    pointParamAtTime(
        instance,
        "trackPoint",
        time,
        fallbackX,
        fallbackY,
        pointX,
        pointY);
    controls.trackX = static_cast<float>(
        pointX * renderScale[0] / source.pixelAspect);
    controls.trackY = static_cast<float>(
        pointY * renderScale[1]);
    controls.trackX += static_cast<float>(
        doubleParamAtTime(
            instance,
            "trackNudgeX",
            time,
            0.0) *
        renderScale[0]);
    controls.trackY += static_cast<float>(
        doubleParamAtTime(
            instance,
            "trackNudgeY",
            time,
            0.0) *
        renderScale[1]);
    controls.textureSnap =
        intParamAtTime(instance, "textureSnap", time, 0) != 0;
    controls.textureSnapRadius = std::max(
        0,
        static_cast<int>(
            std::lround(
                intParamAtTime(
                    instance,
                    "textureSnapRadius",
                    time,
                    controls.textureSnapRadius) *
                std::max(renderScale[0], renderScale[1]))));
    controls.regionWidth = static_cast<float>(
        doubleParamAtTime(
            instance,
            "regionWidth",
            time,
            controls.regionWidth));
    controls.regionHeight = static_cast<float>(
        doubleParamAtTime(
            instance,
            "regionHeight",
            time,
            controls.regionHeight));
    controls.searchRadius = intParamAtTime(
        instance,
        "searchRadius",
        time,
        controls.searchRadius);
    controls.temporalRadius =
        intParamAtTime(
            instance,
            "temporalRadius",
            time,
            controls.temporalRadius - 1) +
        1;
    controls.analysisQuality = intParamAtTime(
        instance,
        "analysisQuality",
        time,
        controls.analysisQuality);
    controls.stabilizationStrength = static_cast<float>(
        doubleParamAtTime(
            instance,
            "stabilizationStrength",
            time,
            controls.stabilizationStrength));
    controls.outerFrameWeight = static_cast<float>(
        doubleParamAtTime(
            instance,
            "outerFrameWeight",
            time,
            controls.outerFrameWeight));
    controls.maxCorrection = static_cast<float>(
        doubleParamAtTime(
            instance,
            "maxCorrection",
            time,
            controls.maxCorrection));
    controls.confidenceThreshold = static_cast<float>(
        doubleParamAtTime(
            instance,
            "confidenceThreshold",
            time,
            controls.confidenceThreshold));
    controls.sceneCutProtection = static_cast<float>(
        doubleParamAtTime(
            instance,
            "sceneCutProtection",
            time,
            controls.sceneCutProtection));
    controls.interpolation = intParamAtTime(
        instance,
        "interpolation",
        time,
        controls.interpolation);
    controls.edgeMode = intParamAtTime(
        instance,
        "edgeMode",
        time,
        controls.edgeMode);
    controls.autoZoomStrength = static_cast<float>(
        doubleParamAtTime(
            instance,
            "autoZoomStrength",
            time,
            controls.autoZoomStrength));
    controls.viewMode = intParamAtTime(
        instance,
        "viewMode",
        time,
        controls.viewMode);
    controls.overlayOpacity = static_cast<float>(
        doubleParamAtTime(
            instance,
            "overlayOpacity",
            time,
            controls.overlayOpacity));
    controls.outputMix = static_cast<float>(
        doubleParamAtTime(
            instance,
            "outputMix",
            time,
            controls.outputMix));
    return controls;
}

template <typename NativePixel>
bool sourceLooksLikeBlankAdjustment(
    const ImageInfo& source)
{
    const int width = source.bounds.x2 - source.bounds.x1;
    const int height = source.bounds.y2 - source.bounds.y1;
    if (width <= 0 || height <= 0) {
        return false;
    }
    auto* base = reinterpret_cast<NativePixel*>(source.data);
    float maxRgb = 0.0f;
    float alphaSum = 0.0f;
    int sampleCount = 0;
    for (int gy = 0; gy < 7; ++gy) {
        const int y =
            source.bounds.y1 + (height - 1) * gy / 6;
        for (int gx = 0; gx < 7; ++gx) {
            const int x =
                source.bounds.x1 + (width - 1) * gx / 6;
            const NativePixel* pixel = pixelAddress(
                base,
                source.bounds,
                x,
                y,
                source.rowBytes);
            if (!pixel) {
                continue;
            }
            const Pixel value = readNative(*pixel);
            maxRgb = std::max(
                maxRgb,
                std::max(
                    value.r,
                    std::max(value.g, value.b)));
            alphaSum += value.a;
            ++sampleCount;
        }
    }
    return sampleCount > 0 &&
        maxRgb < 0.015f &&
        alphaSum / static_cast<float>(sampleCount) > 0.75f;
}

bool sourceLooksLikeBlankAdjustmentByDepth(
    const ImageInfo& source)
{
    if (
        std::strcmp(
            source.pixelDepth,
            kOfxBitDepthFloat) == 0) {
        return sourceLooksLikeBlankAdjustment<
            OfxRGBAColourF>(source);
    }
    if (
        std::strcmp(
            source.pixelDepth,
            kOfxBitDepthByte) == 0) {
        return sourceLooksLikeBlankAdjustment<
            OfxRGBAColourB>(source);
    }
    return false;
}

void hashBytes(
    std::uint64_t& hash,
    const void* data,
    std::size_t size)
{
    const auto* bytes =
        static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= static_cast<std::uint64_t>(bytes[index]);
        hash *= 1099511628211ull;
    }
}

template <typename T>
void hashValue(std::uint64_t& hash, const T& value)
{
    hashBytes(hash, &value, sizeof(value));
}

std::uint64_t analysisSignature(
    OfxImageEffectHandle instance,
    OfxTime time,
    const ImageInfo& source,
    const std::array<const ImageInfo*, 4>& temporal,
    const Controls& controls)
{
    std::uint64_t hash = 1469598103934665603ull;
    hashValue(hash, instance);
    hashValue(hash, time);
    hashValue(hash, source.data);
    hashValue(hash, source.rowBytes);
    hashBytes(hash, &source.bounds, sizeof(source.bounds));
    for (const ImageInfo* image : temporal) {
        const void* data = image ? image->data : nullptr;
        hashValue(hash, data);
        if (image) {
            hashValue(hash, image->rowBytes);
            hashBytes(hash, &image->bounds, sizeof(image->bounds));
        }
    }
    hashValue(hash, controls.enabled);
    hashValue(hash, controls.trackX);
    hashValue(hash, controls.trackY);
    hashValue(hash, controls.textureSnap);
    hashValue(hash, controls.textureSnapRadius);
    hashValue(hash, controls.regionWidth);
    hashValue(hash, controls.regionHeight);
    hashValue(hash, controls.searchRadius);
    hashValue(hash, controls.temporalRadius);
    hashValue(hash, controls.analysisQuality);
    hashValue(hash, controls.stabilizationStrength);
    hashValue(hash, controls.outerFrameWeight);
    hashValue(hash, controls.maxCorrection);
    hashValue(hash, controls.confidenceThreshold);
    hashValue(hash, controls.sceneCutProtection);
    return hash;
}

class AnalysisStore {
public:
    template <typename Builder>
    std::shared_ptr<const TrackingResult> getOrCreate(
        OfxImageEffectHandle instance,
        std::uint64_t signature,
        Builder&& builder)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto found = std::find_if(
                entries_.begin(),
                entries_.end(),
                [&](const Entry& entry) {
                    return entry.instance == instance &&
                        entry.signature == signature;
                });
            if (found != entries_.end()) {
                auto result = found->result;
                entries_.splice(
                    entries_.begin(),
                    entries_,
                    found);
                return result;
            }
        }

        auto created = std::make_shared<TrackingResult>(
            builder());
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_front(
            Entry{instance, signature, created});
        while (entries_.size() > 64u) {
            entries_.pop_back();
        }
        return created;
    }

    void erase(OfxImageEffectHandle instance)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.remove_if(
            [&](const Entry& entry) {
                return entry.instance == instance;
            });
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

private:
    struct Entry {
        OfxImageEffectHandle instance;
        std::uint64_t signature;
        std::shared_ptr<const TrackingResult> result;
    };

    std::mutex mutex_;
    std::list<Entry> entries_;
};

AnalysisStore& analysisStore()
{
    static AnalysisStore store;
    return store;
}

template <typename NativePixel>
OfxStatus fillTransparent(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& destination)
{
    auto* base =
        reinterpret_cast<NativePixel*>(destination.data);
    auto renderRows = [&](int firstY, int lastY) {
        for (int y = firstY; y < lastY; ++y) {
            if (gEffectHost->abort(instance)) {
                break;
            }
            for (
                int x = renderWindow.x1;
                x < renderWindow.x2;
                ++x) {
                NativePixel* pixel = pixelAddress(
                    base,
                    destination.bounds,
                    x,
                    y,
                    destination.rowBytes);
                if (pixel) {
                    writeNative(
                        pixel,
                        Pixel{0.0f, 0.0f, 0.0f, 0.0f});
                }
            }
        }
    };
    return buckswood::ofx_runtime::parallelRows(
        gThreadHost,
        renderWindow.y1,
        renderWindow.y2,
        renderRows);
}

OfxStatus fillTransparentByDepth(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& destination)
{
    if (
        std::strcmp(
            destination.pixelDepth,
            kOfxBitDepthFloat) == 0) {
        return fillTransparent<OfxRGBAColourF>(
            instance,
            renderWindow,
            destination);
    }
    if (
        std::strcmp(
            destination.pixelDepth,
            kOfxBitDepthByte) == 0) {
        return fillTransparent<OfxRGBAColourB>(
            instance,
            renderWindow,
            destination);
    }
    return kOfxStatErrUnsupported;
}

template <typename NativePixel>
OfxStatus renderTyped(
    OfxImageEffectHandle instance,
    OfxTime time,
    const OfxRectI& renderWindow,
    const ImageInfo& source,
    const ImageInfo* previous2,
    const ImageInfo* previous,
    const ImageInfo* next,
    const ImageInfo* next2,
    const ImageInfo& destination,
    const double renderScale[2])
{
    const ImageSampler<NativePixel> currentSampler(source);
    const ImageSampler<NativePixel> previous2Sampler(
        previous2 ? *previous2 : source);
    const ImageSampler<NativePixel> previousSampler(
        previous ? *previous : source);
    const ImageSampler<NativePixel> nextSampler(
        next ? *next : source);
    const ImageSampler<NativePixel> next2Sampler(
        next2 ? *next2 : source);
    const TemporalContext temporalContext{
        previous2
            ? static_cast<const Sampler*>(&previous2Sampler)
            : nullptr,
        previous
            ? static_cast<const Sampler*>(&previousSampler)
            : nullptr,
        next
            ? static_cast<const Sampler*>(&nextSampler)
            : nullptr,
        next2
            ? static_cast<const Sampler*>(&next2Sampler)
            : nullptr,
    };
    const FrameInfo frame{
        source.bounds.x2 - source.bounds.x1,
        source.bounds.y2 - source.bounds.y1,
        static_cast<int>(std::llround(time)),
    };
    const Controls controls = controlsAtTime(
        instance,
        time,
        source,
        renderScale);
    const std::array<const ImageInfo*, 4> temporalImages{
        previous2,
        previous,
        next,
        next2,
    };
    const std::uint64_t signature = analysisSignature(
        instance,
        time,
        source,
        temporalImages,
        controls);
    const auto tracking = analysisStore().getOrCreate(
        instance,
        signature,
        [&]() {
            return DeJitterCore::analyze(
                currentSampler,
                temporalContext,
                frame,
                controls);
        });

    auto* destinationBase =
        reinterpret_cast<NativePixel*>(destination.data);
    auto renderRows = [&](int firstY, int lastY) {
        for (int y = firstY; y < lastY; ++y) {
            if (gEffectHost->abort(instance)) {
                break;
            }
            for (
                int x = renderWindow.x1;
                x < renderWindow.x2;
                ++x) {
                NativePixel* destinationPixel = pixelAddress(
                    destinationBase,
                    destination.bounds,
                    x,
                    y,
                    destination.rowBytes);
                if (!destinationPixel) {
                    continue;
                }
                const Pixel output = DeJitterCore::processPixel(
                    currentSampler,
                    x,
                    y,
                    frame,
                    controls,
                    *tracking);
                writeNative(destinationPixel, output);
            }
        }
    };
    return buckswood::ofx_runtime::parallelRows(
        gThreadHost,
        renderWindow.y1,
        renderWindow.y2,
        renderRows);
}

OfxStatus render(
    OfxImageEffectHandle instance,
    OfxPropertySetHandle inArgs)
{
    OfxTime time = 0.0;
    OfxRectI renderWindow{0, 0, 0, 0};
    double renderScale[2] = {1.0, 1.0};
    gPropHost->propGetDouble(
        inArgs,
        kOfxPropTime,
        0,
        &time);
    gPropHost->propGetIntN(
        inArgs,
        kOfxImageEffectPropRenderWindow,
        4,
        &renderWindow.x1);
    gPropHost->propGetDoubleN(
        inArgs,
        kOfxImageEffectPropRenderScale,
        2,
        renderScale);

    OfxImageClipHandle outputClip = nullptr;
    OfxImageClipHandle sourceClip = nullptr;
    gEffectHost->clipGetHandle(
        instance,
        kOfxImageEffectOutputClipName,
        &outputClip,
        nullptr);
    gEffectHost->clipGetHandle(
        instance,
        kOfxImageEffectSimpleSourceClipName,
        &sourceClip,
        nullptr);

    ScopedImage outputImage;
    if (!outputImage.fetch(outputClip, time)) {
        return gEffectHost->abort(instance)
            ? kOfxStatOK
            : kOfxStatFailed;
    }
    ImageInfo destination;
    if (!readImageInfo(outputImage.get(), destination)) {
        return kOfxStatFailed;
    }

    ScopedImage sourceImage;
    if (!sourceImage.fetch(sourceClip, time)) {
        return fillTransparentByDepth(
            instance,
            renderWindow,
            destination);
    }
    ImageInfo source;
    if (!readImageInfo(sourceImage.get(), source)) {
        return kOfxStatFailed;
    }

    if (
        intParamAtTime(
            instance,
            "adjustmentLayerGuard",
            time,
            1) != 0 &&
        sourceLooksLikeBlankAdjustmentByDepth(source)) {
        return fillTransparentByDepth(
            instance,
            renderWindow,
            destination);
    }

    const int temporalRadius =
        intParamAtTime(
            instance,
            "temporalRadius",
            time,
            1) +
        1;
    ScopedImage previous2Image;
    ScopedImage previousImage;
    ScopedImage nextImage;
    ScopedImage next2Image;
    ImageInfo previous2;
    ImageInfo previous;
    ImageInfo next;
    ImageInfo next2;
    ImageInfo* previous2Ptr = nullptr;
    ImageInfo* previousPtr = nullptr;
    ImageInfo* nextPtr = nullptr;
    ImageInfo* next2Ptr = nullptr;

    auto fetchTemporal = [&](ScopedImage& image,
                             ImageInfo& info,
                             OfxTime frameTime,
                             ImageInfo*& pointer) {
        if (
            image.fetch(sourceClip, frameTime) &&
            readImageInfo(image.get(), info) &&
            info.pixelDepth &&
            source.pixelDepth &&
            std::strcmp(
                info.pixelDepth,
                source.pixelDepth) == 0) {
            pointer = &info;
        }
    };
    fetchTemporal(
        previousImage,
        previous,
        time - 1.0,
        previousPtr);
    fetchTemporal(
        nextImage,
        next,
        time + 1.0,
        nextPtr);
    if (temporalRadius >= 2) {
        fetchTemporal(
            previous2Image,
            previous2,
            time - 2.0,
            previous2Ptr);
        fetchTemporal(
            next2Image,
            next2,
            time + 2.0,
            next2Ptr);
    }

    if (
        std::strcmp(
            destination.pixelDepth,
            kOfxBitDepthFloat) == 0 &&
        std::strcmp(
            source.pixelDepth,
            kOfxBitDepthFloat) == 0) {
        return renderTyped<OfxRGBAColourF>(
            instance,
            time,
            renderWindow,
            source,
            previous2Ptr,
            previousPtr,
            nextPtr,
            next2Ptr,
            destination,
            renderScale);
    }
    if (
        std::strcmp(
            destination.pixelDepth,
            kOfxBitDepthByte) == 0 &&
        std::strcmp(
            source.pixelDepth,
            kOfxBitDepthByte) == 0) {
        return renderTyped<OfxRGBAColourB>(
            instance,
            time,
            renderWindow,
            source,
            previous2Ptr,
            previousPtr,
            nextPtr,
            next2Ptr,
            destination,
            renderScale);
    }
    return kOfxStatErrUnsupported;
}

struct OverlayInteractData {
    OfxParamSetHandle paramSet = nullptr;
    OfxImageClipHandle sourceClip = nullptr;
    OfxParamHandle trackPoint = nullptr;
    OfxParamHandle trackNudgeX = nullptr;
    OfxParamHandle trackNudgeY = nullptr;
    OfxParamHandle regionWidth = nullptr;
    OfxParamHandle regionHeight = nullptr;
    OfxParamHandle showViewerControls = nullptr;
    OverlayDragMode dragMode =
        buckswood_dejitter::OverlayDragNone;
    OverlayParameters dragStart;
    OverlayBounds dragBounds;
    OverlayPoint grabOffset;
    bool editOpen = false;
};

OverlayInteractData* overlayData(
    OfxInteractHandle interact)
{
    if (!gInteractHost || !gPropHost) {
        return nullptr;
    }
    OfxPropertySetHandle properties = nullptr;
    void* data = nullptr;
    if (
        gInteractHost->interactGetPropertySet(
            interact,
            &properties) != kOfxStatOK ||
        gPropHost->propGetPointer(
            properties,
            kOfxPropInstanceData,
            0,
            &data) != kOfxStatOK) {
        return nullptr;
    }
    return static_cast<OverlayInteractData*>(data);
}

OfxTime overlayTime(OfxPropertySetHandle inArgs)
{
    double time = 0.0;
    if (inArgs) {
        gPropHost->propGetDouble(
            inArgs,
            kOfxPropTime,
            0,
            &time);
    }
    return time;
}

OverlayPoint overlayPen(OfxPropertySetHandle inArgs)
{
    OverlayPoint point;
    if (inArgs) {
        gPropHost->propGetDoubleN(
            inArgs,
            kOfxInteractPropPenPosition,
            2,
            &point.x);
    }
    return point;
}

OverlayPoint overlayPixelScale(OfxPropertySetHandle inArgs)
{
    OverlayPoint scale{1.0, 1.0};
    if (inArgs) {
        gPropHost->propGetDoubleN(
            inArgs,
            kOfxInteractPropPixelScale,
            2,
            &scale.x);
    }
    scale.x = std::max(1.0e-6, std::fabs(scale.x));
    scale.y = std::max(1.0e-6, std::fabs(scale.y));
    return scale;
}

bool viewerControlsVisible(
    const OverlayInteractData& data,
    OfxTime time)
{
    if (!data.showViewerControls) {
        return true;
    }
    int visible = 1;
    if (
        gParamHost->paramGetValueAtTime(
            data.showViewerControls,
            time,
            &visible) != kOfxStatOK) {
        return true;
    }
    return visible != 0;
}

OverlayParameters overlayParameters(
    const OverlayInteractData& data,
    OfxTime time)
{
    OverlayParameters parameters;
    if (data.trackPoint) {
        gParamHost->paramGetValueAtTime(
            data.trackPoint,
            time,
            &parameters.center.x,
            &parameters.center.y);
    }
    if (data.regionWidth) {
        gParamHost->paramGetValueAtTime(
            data.regionWidth,
            time,
            &parameters.regionWidth);
    }
    if (data.regionHeight) {
        gParamHost->paramGetValueAtTime(
            data.regionHeight,
            time,
            &parameters.regionHeight);
    }
    return parameters;
}

OverlayBounds overlayBounds(
    const OverlayInteractData& data,
    OfxTime time,
    OverlayPoint fallbackCenter)
{
    OfxRectD rod{
        fallbackCenter.x - 960.0,
        fallbackCenter.y - 540.0,
        fallbackCenter.x + 960.0,
        fallbackCenter.y + 540.0,
    };
    if (data.sourceClip) {
        gEffectHost->clipGetRegionOfDefinition(
            data.sourceClip,
            time,
            &rod);
    }
    if (
        rod.x2 <= rod.x1 + 1.0e-6 ||
        rod.y2 <= rod.y1 + 1.0e-6) {
        rod = OfxRectD{0.0, 0.0, 1920.0, 1080.0};
    }
    return OverlayBounds{rod.x1, rod.y1, rod.x2, rod.y2};
}

void drawOverlayHandle(
    OfxDrawContextHandle context,
    OverlayPoint position,
    OverlayPoint pixelScale)
{
    const double halfWidth = pixelScale.x * 4.5;
    const double halfHeight = pixelScale.y * 4.5;
    const OfxPointD points[2] = {
        {position.x - halfWidth, position.y - halfHeight},
        {position.x + halfWidth, position.y + halfHeight},
    };
    gDrawHost->draw(
        context,
        kOfxDrawPrimitiveRectangle,
        points,
        2);
}

OfxStatus drawOverlay(
    OfxInteractHandle interact,
    OfxPropertySetHandle inArgs)
{
    OverlayInteractData* data = overlayData(interact);
    if (
        !data ||
        !gDrawHost ||
        !viewerControlsVisible(*data, overlayTime(inArgs))) {
        return kOfxStatReplyDefault;
    }

    void* contextPointer = nullptr;
    if (
        gPropHost->propGetPointer(
            inArgs,
            kOfxInteractPropDrawContext,
            0,
            &contextPointer) != kOfxStatOK ||
        !contextPointer) {
        return kOfxStatReplyDefault;
    }
    auto context =
        static_cast<OfxDrawContextHandle>(contextPointer);
    const OfxTime time = overlayTime(inArgs);
    const OverlayParameters parameters =
        overlayParameters(*data, time);
    const OverlayBounds bounds =
        overlayBounds(*data, time, parameters.center);
    const OverlayRect area =
        DeJitterOverlay::rect(bounds, parameters);
    const OverlayPoint pixelScale =
        overlayPixelScale(inArgs);

    const OfxPointD outline[4] = {
        {area.left, area.bottom},
        {area.right, area.bottom},
        {area.right, area.top},
        {area.left, area.top},
    };
    OfxRGBAColourF shadow{0.02f, 0.02f, 0.02f, 0.82f};
    gDrawHost->setColour(context, &shadow);
    gDrawHost->setLineWidth(context, 4.0f);
    gDrawHost->draw(
        context,
        kOfxDrawPrimitiveLineLoop,
        outline,
        4);

    OfxRGBAColourF color{0.18f, 0.92f, 0.34f, 1.0f};
    const OfxStandardColour standardColor =
        data->dragMode == buckswood_dejitter::OverlayDragNone
        ? kOfxStandardColourOverlaySelected
        : kOfxStandardColourOverlayActive;
    gDrawHost->getColour(
        context,
        standardColor,
        &color);
    gDrawHost->setColour(context, &color);
    gDrawHost->setLineWidth(context, 1.7f);
    gDrawHost->draw(
        context,
        kOfxDrawPrimitiveLineLoop,
        outline,
        4);

    const double crossX = pixelScale.x * 12.0;
    const double crossY = pixelScale.y * 12.0;
    const OfxPointD cross[4] = {
        {parameters.center.x - crossX, parameters.center.y},
        {parameters.center.x + crossX, parameters.center.y},
        {parameters.center.x, parameters.center.y - crossY},
        {parameters.center.x, parameters.center.y + crossY},
    };
    gDrawHost->draw(
        context,
        kOfxDrawPrimitiveLines,
        cross,
        4);

    const double middleX = (area.left + area.right) * 0.5;
    const double middleY = (area.bottom + area.top) * 0.5;
    const OverlayPoint handles[8] = {
        {area.left, area.bottom},
        {area.right, area.bottom},
        {area.left, area.top},
        {area.right, area.top},
        {area.left, middleY},
        {area.right, middleY},
        {middleX, area.bottom},
        {middleX, area.top},
    };
    for (const OverlayPoint handle : handles) {
        drawOverlayHandle(context, handle, pixelScale);
    }

    const OfxPointD labelPosition{
        area.left + pixelScale.x * 7.0,
        area.top + pixelScale.y * 8.0,
    };
    gDrawHost->drawText(
        context,
        "TRACKING AREA (ANALYSIS)",
        &labelPosition,
        kOfxDrawTextAlignmentLeft |
            kOfxDrawTextAlignmentBaseline);
    return kOfxStatOK;
}

OfxStatus createOverlayInstance(
    OfxImageEffectHandle effect,
    OfxInteractHandle interact)
{
    auto data = std::make_unique<OverlayInteractData>();
    if (
        !effect ||
        gEffectHost->getParamSet(
            effect,
            &data->paramSet) != kOfxStatOK) {
        return kOfxStatFailed;
    }

    auto getParam = [&](const char* name, OfxParamHandle& handle) {
        gParamHost->paramGetHandle(
            data->paramSet,
            name,
            &handle,
            nullptr);
    };
    getParam("trackPoint", data->trackPoint);
    getParam("trackNudgeX", data->trackNudgeX);
    getParam("trackNudgeY", data->trackNudgeY);
    getParam("regionWidth", data->regionWidth);
    getParam("regionHeight", data->regionHeight);
    getParam("showViewerControls", data->showViewerControls);
    if (
        !data->trackPoint ||
        !data->regionWidth ||
        !data->regionHeight) {
        return kOfxStatFailed;
    }
    gEffectHost->clipGetHandle(
        effect,
        kOfxImageEffectSimpleSourceClipName,
        &data->sourceClip,
        nullptr);

    OfxPropertySetHandle properties = nullptr;
    if (
        gInteractHost->interactGetPropertySet(
            interact,
            &properties) != kOfxStatOK) {
        return kOfxStatFailed;
    }
    gPropHost->propSetPointer(
        properties,
        kOfxPropInstanceData,
        0,
        data.get());
    const char* slaves[] = {
        "trackPoint",
        "regionWidth",
        "regionHeight",
        "showViewerControls",
    };
    for (int index = 0; index < 4; ++index) {
        gPropHost->propSetString(
            properties,
            kOfxInteractPropSlaveToParam,
            index,
            slaves[index]);
    }
    data.release();
    return kOfxStatOK;
}

OfxStatus destroyOverlayInstance(OfxInteractHandle interact)
{
    OverlayInteractData* data = overlayData(interact);
    if (data && data->editOpen && data->paramSet) {
        gParamHost->paramEditEnd(data->paramSet);
    }
    delete data;
    OfxPropertySetHandle properties = nullptr;
    if (
        gInteractHost &&
        gInteractHost->interactGetPropertySet(
            interact,
            &properties) == kOfxStatOK) {
        gPropHost->propSetPointer(
            properties,
            kOfxPropInstanceData,
            0,
            nullptr);
    }
    return kOfxStatOK;
}

OfxStatus overlayPenDown(
    OfxInteractHandle interact,
    OfxPropertySetHandle inArgs)
{
    OverlayInteractData* data = overlayData(interact);
    const OfxTime time = overlayTime(inArgs);
    if (
        !data ||
        !viewerControlsVisible(*data, time)) {
        return kOfxStatReplyDefault;
    }
    const OverlayPoint pen = overlayPen(inArgs);
    data->dragStart = overlayParameters(*data, time);
    data->dragBounds =
        overlayBounds(*data, time, data->dragStart.center);
    const OverlayRect area =
        DeJitterOverlay::rect(
            data->dragBounds,
            data->dragStart);
    data->dragMode = DeJitterOverlay::hitTest(
        area,
        data->dragStart.center,
        pen,
        overlayPixelScale(inArgs));
    if (
        data->dragMode ==
        buckswood_dejitter::OverlayDragNone) {
        return kOfxStatReplyDefault;
    }

    data->grabOffset = OverlayPoint{
        data->dragStart.center.x - pen.x,
        data->dragStart.center.y - pen.y,
    };
    if (data->paramSet) {
        data->editOpen =
            gParamHost->paramEditBegin(
                data->paramSet,
                "Edit DeJitter Tracking Area") == kOfxStatOK;
    }
    if (
        data->dragMode ==
        buckswood_dejitter::OverlayDragMove) {
        if (data->trackNudgeX) {
            gParamHost->paramSetValue(
                data->trackNudgeX,
                0.0);
        }
        if (data->trackNudgeY) {
            gParamHost->paramSetValue(
                data->trackNudgeY,
                0.0);
        }
    }
    return kOfxStatOK;
}

OfxStatus overlayPenMotion(
    OfxInteractHandle interact,
    OfxPropertySetHandle inArgs)
{
    OverlayInteractData* data = overlayData(interact);
    if (
        !data ||
        data->dragMode ==
            buckswood_dejitter::OverlayDragNone) {
        return kOfxStatReplyDefault;
    }
    const OverlayParameters parameters =
        DeJitterOverlay::drag(
            data->dragMode,
            data->dragBounds,
            data->dragStart,
            overlayPen(inArgs),
            data->grabOffset);
    if (
        data->dragMode ==
        buckswood_dejitter::OverlayDragMove) {
        gParamHost->paramSetValue(
            data->trackPoint,
            parameters.center.x,
            parameters.center.y);
    } else {
        if (
            parameters.regionWidth !=
            data->dragStart.regionWidth) {
            gParamHost->paramSetValue(
                data->regionWidth,
                parameters.regionWidth);
        }
        if (
            parameters.regionHeight !=
            data->dragStart.regionHeight) {
            gParamHost->paramSetValue(
                data->regionHeight,
                parameters.regionHeight);
        }
    }
    if (gInteractHost) {
        gInteractHost->interactRedraw(interact);
    }
    return kOfxStatOK;
}

OfxStatus overlayPenUp(OfxInteractHandle interact)
{
    OverlayInteractData* data = overlayData(interact);
    if (
        !data ||
        data->dragMode ==
            buckswood_dejitter::OverlayDragNone) {
        return kOfxStatReplyDefault;
    }
    data->dragMode =
        buckswood_dejitter::OverlayDragNone;
    if (data->editOpen && data->paramSet) {
        gParamHost->paramEditEnd(data->paramSet);
        data->editOpen = false;
    }
    if (gInteractHost) {
        gInteractHost->interactRedraw(interact);
    }
    return kOfxStatOK;
}

OfxStatus overlayMain(
    const char* action,
    const void* handle,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle)
{
    auto interact = reinterpret_cast<OfxInteractHandle>(
        const_cast<void*>(handle));
    if (std::strcmp(action, kOfxActionDescribe) == 0) {
        return kOfxStatOK;
    }
    if (!gInteractHost || !gPropHost) {
        return kOfxStatReplyDefault;
    }

    OfxPropertySetHandle properties = nullptr;
    OfxImageEffectHandle effect = nullptr;
    if (
        gInteractHost->interactGetPropertySet(
            interact,
            &properties) == kOfxStatOK) {
        gPropHost->propGetPointer(
            properties,
            kOfxPropEffectInstance,
            0,
            reinterpret_cast<void**>(&effect));
    }
    if (
        std::strcmp(
            action,
            kOfxActionCreateInstance) == 0) {
        return createOverlayInstance(effect, interact);
    }
    if (
        std::strcmp(
            action,
            kOfxActionDestroyInstance) == 0) {
        return destroyOverlayInstance(interact);
    }
    if (
        std::strcmp(
            action,
            kOfxInteractActionDraw) == 0) {
        return drawOverlay(interact, inArgs);
    }
    if (
        std::strcmp(
            action,
            kOfxInteractActionPenDown) == 0) {
        return overlayPenDown(interact, inArgs);
    }
    if (
        std::strcmp(
            action,
            kOfxInteractActionPenMotion) == 0) {
        return overlayPenMotion(interact, inArgs);
    }
    if (
        std::strcmp(
            action,
            kOfxInteractActionPenUp) == 0) {
        return overlayPenUp(interact);
    }
    return kOfxStatReplyDefault;
}

void addParamToPage(
    OfxPropertySetHandle page,
    int index,
    const char* name)
{
    gPropHost->propSetString(
        page,
        kOfxParamPropPageChild,
        index,
        name);
}

void defineDoubleParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    double defaultValue,
    double minimum,
    double maximum,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(
        paramSet,
        kOfxParamTypeDouble,
        name,
        &properties);
    gPropHost->propSetString(
        properties,
        kOfxParamPropScriptName,
        0,
        name);
    gPropHost->propSetString(
        properties,
        kOfxPropLabel,
        0,
        label);
    if (std::strcmp(name, "regionWidth") == 0) {
        gPropHost->propSetString(
            properties,
            kOfxParamPropHint,
            0,
            "Drag the left or right viewer handle to resize the tracking area.");
    } else if (std::strcmp(name, "regionHeight") == 0) {
        gPropHost->propSetString(
            properties,
            kOfxParamPropHint,
            0,
            "Drag the top or bottom viewer handle to resize the tracking area.");
    }
    gPropHost->propSetString(
        properties,
        kOfxParamPropDoubleType,
        0,
        kOfxParamDoubleTypePlain);
    gPropHost->propSetDouble(
        properties,
        kOfxParamPropDefault,
        0,
        defaultValue);
    gPropHost->propSetDouble(
        properties,
        kOfxParamPropMin,
        0,
        minimum);
    gPropHost->propSetDouble(
        properties,
        kOfxParamPropMax,
        0,
        maximum);
    gPropHost->propSetDouble(
        properties,
        kOfxParamPropDisplayMin,
        0,
        minimum);
    gPropHost->propSetDouble(
        properties,
        kOfxParamPropDisplayMax,
        0,
        maximum);
    addParamToPage(page, pageIndex, name);
}

void defineIntegerParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int defaultValue,
    int minimum,
    int maximum,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(
        paramSet,
        kOfxParamTypeInteger,
        name,
        &properties);
    gPropHost->propSetString(
        properties,
        kOfxParamPropScriptName,
        0,
        name);
    gPropHost->propSetString(
        properties,
        kOfxPropLabel,
        0,
        label);
    gPropHost->propSetInt(
        properties,
        kOfxParamPropDefault,
        0,
        defaultValue);
    gPropHost->propSetInt(
        properties,
        kOfxParamPropMin,
        0,
        minimum);
    gPropHost->propSetInt(
        properties,
        kOfxParamPropMax,
        0,
        maximum);
    gPropHost->propSetInt(
        properties,
        kOfxParamPropDisplayMin,
        0,
        minimum);
    gPropHost->propSetInt(
        properties,
        kOfxParamPropDisplayMax,
        0,
        maximum);
    addParamToPage(page, pageIndex, name);
}

void defineBooleanParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int defaultValue,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(
        paramSet,
        kOfxParamTypeBoolean,
        name,
        &properties);
    gPropHost->propSetString(
        properties,
        kOfxParamPropScriptName,
        0,
        name);
    gPropHost->propSetString(
        properties,
        kOfxPropLabel,
        0,
        label);
    gPropHost->propSetInt(
        properties,
        kOfxParamPropDefault,
        0,
        defaultValue);
    addParamToPage(page, pageIndex, name);
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
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(
        paramSet,
        kOfxParamTypeChoice,
        name,
        &properties);
    gPropHost->propSetString(
        properties,
        kOfxParamPropScriptName,
        0,
        name);
    gPropHost->propSetString(
        properties,
        kOfxPropLabel,
        0,
        label);
    gPropHost->propSetInt(
        properties,
        kOfxParamPropDefault,
        0,
        defaultValue);
    for (int index = 0; index < optionCount; ++index) {
        gPropHost->propSetString(
            properties,
            kOfxParamPropChoiceOption,
            index,
            options[index]);
    }
    addParamToPage(page, pageIndex, name);
}

void definePointParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(
        paramSet,
        kOfxParamTypeDouble2D,
        name,
        &properties);
    gPropHost->propSetString(
        properties,
        kOfxParamPropScriptName,
        0,
        name);
    gPropHost->propSetString(
        properties,
        kOfxPropLabel,
        0,
        label);
    gPropHost->propSetString(
        properties,
        kOfxParamPropHint,
        0,
        "Drag the center or interior of the tracking box directly in the viewer.");
    gPropHost->propSetString(
        properties,
        kOfxParamPropDoubleType,
        0,
        kOfxParamDoubleTypeXYAbsolute);
    gPropHost->propSetString(
        properties,
        kOfxParamPropDefaultCoordinateSystem,
        0,
        kOfxParamCoordinatesNormalised);
    const double defaults[2] = {0.5, 0.5};
    gPropHost->propSetDoubleN(
        properties,
        kOfxParamPropDefault,
        2,
        defaults);
    addParamToPage(page, pageIndex, name);
}

OfxStatus describeInContext(OfxImageEffectHandle effect)
{
    OfxPropertySetHandle properties = nullptr;
    gEffectHost->clipDefine(
        effect,
        kOfxImageEffectOutputClipName,
        &properties);
    gPropHost->propSetString(
        properties,
        kOfxImageEffectPropSupportedComponents,
        0,
        kOfxImageComponentRGBA);
    gEffectHost->clipDefine(
        effect,
        kOfxImageEffectSimpleSourceClipName,
        &properties);
    gPropHost->propSetString(
        properties,
        kOfxImageEffectPropSupportedComponents,
        0,
        kOfxImageComponentRGBA);
    gPropHost->propSetInt(
        properties,
        kOfxImageEffectPropTemporalClipAccess,
        0,
        1);

    OfxParamSetHandle paramSet = nullptr;
    gEffectHost->getParamSet(effect, &paramSet);
    OfxPropertySetHandle page = nullptr;
    gParamHost->paramDefine(
        paramSet,
        kOfxParamTypePage,
        "Main",
        &page);
    gPropHost->propSetString(
        page,
        kOfxPropLabel,
        0,
        "DeJitter");

    const char* radiusOptions[] = {
        "1 Frame Pair (Fast)",
        "2 Frame Pairs (Stable)",
    };
    const char* qualityOptions[] = {
        "Draft",
        "Standard",
        "High",
    };
    const char* interpolationOptions[] = {
        "Bicubic",
        "Lanczos 3",
        "Bilinear (Fast)",
    };
    const char* edgeOptions[] = {
        "Auto Zoom",
        "Reflect",
        "Edge Extend",
    };
    const char* viewOptions[] = {
        "Stabilized Result",
        "Tracking Region",
        "Motion Vector",
        "Confidence",
        "Difference",
    };

    defineBooleanParam(
        paramSet,
        "enabled",
        "Enable DeJitter",
        1,
        0,
        page);
    definePointParam(
        paramSet,
        "trackPoint",
        "Track Point",
        1,
        page);
    defineDoubleParam(
        paramSet,
        "trackNudgeX",
        "Track Point Fine X (Pixels)",
        0.0,
        -200.0,
        200.0,
        2,
        page);
    defineDoubleParam(
        paramSet,
        "trackNudgeY",
        "Track Point Fine Y (Pixels)",
        0.0,
        -200.0,
        200.0,
        3,
        page);
    defineBooleanParam(
        paramSet,
        "textureSnap",
        "Snap Point to Texture",
        0,
        4,
        page);
    defineIntegerParam(
        paramSet,
        "textureSnapRadius",
        "Texture Snap Radius (Pixels)",
        24,
        0,
        96,
        5,
        page);
    defineDoubleParam(
        paramSet,
        "regionWidth",
        "Tracking Region Width",
        0.12,
        0.02,
        0.50,
        6,
        page);
    defineDoubleParam(
        paramSet,
        "regionHeight",
        "Tracking Region Height",
        0.12,
        0.02,
        0.50,
        7,
        page);
    defineIntegerParam(
        paramSet,
        "searchRadius",
        "Maximum Jitter (Pixels)",
        32,
        8,
        192,
        4,
        page);
    defineChoiceParam(
        paramSet,
        "temporalRadius",
        "Temporal Analysis",
        1,
        radiusOptions,
        2,
        9,
        page);
    defineChoiceParam(
        paramSet,
        "analysisQuality",
        "Tracking Quality",
        1,
        qualityOptions,
        3,
        10,
        page);
    defineDoubleParam(
        paramSet,
        "stabilizationStrength",
        "DeJitter Strength",
        0.85,
        0.0,
        1.0,
        11,
        page);
    defineDoubleParam(
        paramSet,
        "outerFrameWeight",
        "Long-Term Stability",
        0.30,
        0.0,
        1.0,
        12,
        page);
    defineDoubleParam(
        paramSet,
        "maxCorrection",
        "Maximum Correction (Pixels)",
        24.0,
        0.0,
        128.0,
        13,
        page);
    defineDoubleParam(
        paramSet,
        "confidenceThreshold",
        "Tracking Confidence Guard",
        0.28,
        0.0,
        1.0,
        14,
        page);
    defineDoubleParam(
        paramSet,
        "sceneCutProtection",
        "Scene Cut Protection",
        0.72,
        0.0,
        1.0,
        15,
        page);
    defineChoiceParam(
        paramSet,
        "interpolation",
        "Resampling",
        0,
        interpolationOptions,
        3,
        16,
        page);
    defineChoiceParam(
        paramSet,
        "edgeMode",
        "Frame Edge Handling",
        0,
        edgeOptions,
        3,
        17,
        page);
    defineDoubleParam(
        paramSet,
        "autoZoomStrength",
        "Auto Zoom Strength",
        1.0,
        0.0,
        1.0,
        18,
        page);
    defineChoiceParam(
        paramSet,
        "viewMode",
        "View",
        0,
        viewOptions,
        5,
        19,
        page);
    defineDoubleParam(
        paramSet,
        "overlayOpacity",
        "Diagnostic Overlay Opacity",
        0.88,
        0.0,
        1.0,
        20,
        page);
    defineDoubleParam(
        paramSet,
        "outputMix",
        "Output Mix",
        1.0,
        0.0,
        1.0,
        21,
        page);
    defineBooleanParam(
        paramSet,
        "adjustmentLayerGuard",
        "Adjustment Layer Guard",
        1,
        22,
        page);
    defineBooleanParam(
        paramSet,
        "showViewerControls",
        "Show Viewer Tracking Area",
        1,
        23,
        page);
    return kOfxStatOK;
}

OfxStatus describe(OfxImageEffectHandle effect)
{
    OfxPropertySetHandle properties = nullptr;
    gEffectHost->getPropertySet(effect, &properties);
    gPropHost->propSetInt(
        properties,
        kOfxImageEffectPropSupportsMultipleClipDepths,
        0,
        0);
    gPropHost->propSetString(
        properties,
        kOfxImageEffectPropSupportedPixelDepths,
        0,
        kOfxBitDepthFloat);
    gPropHost->propSetString(
        properties,
        kOfxImageEffectPropSupportedPixelDepths,
        1,
        kOfxBitDepthByte);
    gPropHost->propSetInt(
        properties,
        kOfxImageEffectPropTemporalClipAccess,
        0,
        1);
    gPropHost->propSetInt(
        properties,
        kOfxImageEffectPropSupportsTiles,
        0,
        0);
    gPropHost->propSetString(
        properties,
        kOfxPropLabel,
        0,
        "Buckswood DeJitter v1.2");
    gPropHost->propSetString(
        properties,
        kOfxImageEffectPluginPropGrouping,
        0,
        "Buckswood");
    gPropHost->propSetString(
        properties,
        kOfxImageEffectPropSupportedContexts,
        0,
        kOfxImageEffectContextFilter);
    int supportsOverlays = 0;
    if (
        gInteractHost &&
        gDrawHost &&
        gPropHost->propGetInt(
            gHost->host,
            kOfxImageEffectPropSupportsOverlays,
            0,
            &supportsOverlays) == kOfxStatOK &&
        supportsOverlays != 0) {
        gPropHost->propSetPointer(
            properties,
            kOfxImageEffectPluginPropOverlayInteractV2,
            0,
            reinterpret_cast<void*>(overlayMain));
    }
    return kOfxStatOK;
}

OfxStatus onLoad()
{
    if (!gHost) {
        return kOfxStatErrMissingHostFeature;
    }
    gEffectHost =
        const_cast<OfxImageEffectSuiteV1*>(
            reinterpret_cast<
                const OfxImageEffectSuiteV1*>(
                gHost->fetchSuite(
                    gHost->host,
                    kOfxImageEffectSuite,
                    1)));
    gPropHost =
        const_cast<OfxPropertySuiteV1*>(
            reinterpret_cast<
                const OfxPropertySuiteV1*>(
                gHost->fetchSuite(
                    gHost->host,
                    kOfxPropertySuite,
                    1)));
    gParamHost =
        const_cast<OfxParameterSuiteV1*>(
            reinterpret_cast<
                const OfxParameterSuiteV1*>(
                gHost->fetchSuite(
                    gHost->host,
                    kOfxParameterSuite,
                    1)));
    gThreadHost =
        const_cast<OfxMultiThreadSuiteV1*>(
            reinterpret_cast<
                const OfxMultiThreadSuiteV1*>(
                gHost->fetchSuite(
                    gHost->host,
                    kOfxMultiThreadSuite,
                    1)));
    gInteractHost =
        const_cast<OfxInteractSuiteV1*>(
            reinterpret_cast<
                const OfxInteractSuiteV1*>(
                gHost->fetchSuite(
                    gHost->host,
                    kOfxInteractSuite,
                    1)));
    gDrawHost =
        const_cast<OfxDrawSuiteV1*>(
            reinterpret_cast<
                const OfxDrawSuiteV1*>(
                gHost->fetchSuite(
                    gHost->host,
                    kOfxDrawSuite,
                    1)));
    return gEffectHost && gPropHost && gParamHost
        ? kOfxStatOK
        : kOfxStatErrMissingHostFeature;
}

OfxStatus getFramesNeeded(
    OfxImageEffectHandle instance,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle outArgs)
{
    OfxTime time = 0.0;
    gPropHost->propGetDouble(
        inArgs,
        kOfxPropTime,
        0,
        &time);
    const int radius =
        intParamAtTime(
            instance,
            "temporalRadius",
            time,
            1) +
        1;
    const double range[2] = {
        time - static_cast<double>(radius),
        time + static_cast<double>(radius),
    };
    gPropHost->propSetDoubleN(
        outArgs,
        kSourceFrameRangeProp,
        2,
        range);
    return kOfxStatOK;
}

OfxStatus pluginMain(
    const char* action,
    const void* handle,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle outArgs)
{
    try {
        auto effect = reinterpret_cast<
            OfxImageEffectHandle>(const_cast<void*>(handle));
        if (std::strcmp(action, kOfxActionLoad) == 0) {
            return onLoad();
        }
        if (std::strcmp(action, kOfxActionDescribe) == 0) {
            return describe(effect);
        }
        if (
            std::strcmp(
                action,
                kOfxImageEffectActionDescribeInContext) == 0) {
            return describeInContext(effect);
        }
        if (
            std::strcmp(
                action,
                kOfxImageEffectActionRender) == 0) {
            return render(effect, inArgs);
        }
        if (
            std::strcmp(
                action,
                kOfxImageEffectActionGetFramesNeeded) == 0) {
            return getFramesNeeded(
                effect,
                inArgs,
                outArgs);
        }
        if (
            std::strcmp(
                action,
                kOfxActionDestroyInstance) == 0) {
            analysisStore().erase(effect);
            return kOfxStatOK;
        }
        if (
            std::strcmp(
                action,
                kOfxActionPurgeCaches) == 0) {
            analysisStore().clear();
            return kOfxStatOK;
        }
        if (
            std::strcmp(
                action,
                kOfxActionCreateInstance) == 0) {
            return kOfxStatOK;
        }
    } catch (const std::bad_alloc&) {
        return kOfxStatErrMemory;
    } catch (const std::exception&) {
        return kOfxStatErrUnknown;
    }
    return kOfxStatReplyDefault;
}

void setHost(OfxHost* host)
{
    gHost = host;
}

OfxPlugin plugin{
    kOfxImageEffectPluginApi,
    1,
    kPluginIdentifier,
    kPluginMajorVersion,
    kPluginMinorVersion,
    setHost,
    pluginMain,
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
