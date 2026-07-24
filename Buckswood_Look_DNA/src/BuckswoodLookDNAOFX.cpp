#include <algorithm>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <string>
#include <type_traits>

#include "LookDNACore.h"
#include "OfxRenderRuntime.h"
#include "ReferenceFileDialog.h"
#include "ReferenceImageLoader.h"

#include "ofxImageEffect.h"
#include "ofxParam.h"
#include "ofxPixels.h"

#if defined __APPLE__ || defined __linux__ || defined __FreeBSD__
#define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#define EXPORT OfxExport
#else
#error Unsupported platform
#endif

namespace {

OfxHost* gHost = nullptr;
OfxImageEffectSuiteV1* gEffectHost = nullptr;
OfxPropertySuiteV1* gPropHost = nullptr;
OfxParameterSuiteV1* gParamHost = nullptr;
OfxMultiThreadSuiteV1* gThreadHost = nullptr;

constexpr const char* kPluginIdentifier = "com.buckswood.look.dna";
constexpr const char* kSourceFrameRangeProp = "OfxImageClipPropFrameRange_Source";
constexpr int kPluginMajorVersion = 2;
constexpr int kPluginMinorVersion = 1;

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
void writePixel(DstPixel* destination, buckswood_lookdna::Pixel value)
{
    if constexpr (std::is_same<DstPixel, OfxRGBAColourF>::value) {
        destination->r = value.r;
        destination->g = value.g;
        destination->b = value.b;
        destination->a = value.a;
    } else {
        destination->r = toByte(value.r);
        destination->g = toByte(value.g);
        destination->b = toByte(value.b);
        destination->a = toByte(value.a);
    }
}

template <typename DstPixel>
OfxStatus fillTransparent(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& destination)
{
    auto* base = reinterpret_cast<DstPixel*>(destination.data);
    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        if (gEffectHost->abort(instance)) {
            break;
        }
        for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
            auto* pixel = pixelAddress(base, destination.bounds, x, y, destination.rowBytes);
            if (pixel) {
                writePixel(pixel, buckswood_lookdna::Pixel{0.0f, 0.0f, 0.0f, 0.0f});
            }
        }
    }
    return kOfxStatOK;
}

OfxStatus fillTransparentByDepth(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& destination)
{
    if (std::strcmp(destination.pixelDepth, kOfxBitDepthFloat) == 0) {
        return fillTransparent<OfxRGBAColourF>(instance, renderWindow, destination);
    }
    if (std::strcmp(destination.pixelDepth, kOfxBitDepthByte) == 0) {
        return fillTransparent<OfxRGBAColourB>(instance, renderWindow, destination);
    }
    return kOfxStatErrUnsupported;
}

template <typename SrcPixel>
bool sourceLooksLikeBlankAdjustment(const ImageInfo& source)
{
    const int width = source.bounds.x2 - source.bounds.x1;
    const int height = source.bounds.y2 - source.bounds.y1;
    if (width <= 0 || height <= 0) {
        return false;
    }
    auto* base = reinterpret_cast<SrcPixel*>(source.data);
    float maxRgb = 0.0f;
    float alphaSum = 0.0f;
    int samples = 0;
    constexpr int grid = 7;
    for (int gy = 0; gy < grid; ++gy) {
        const int y = source.bounds.y1 + (height - 1) * gy / (grid - 1);
        for (int gx = 0; gx < grid; ++gx) {
            const int x = source.bounds.x1 + (width - 1) * gx / (grid - 1);
            const auto* pixel = pixelAddress(base, source.bounds, x, y, source.rowBytes);
            if (!pixel) {
                continue;
            }
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            float a = 0.0f;
            if constexpr (std::is_same<SrcPixel, OfxRGBAColourF>::value) {
                r = pixel->r;
                g = pixel->g;
                b = pixel->b;
                a = pixel->a;
            } else {
                r = pixel->r / 255.0f;
                g = pixel->g / 255.0f;
                b = pixel->b / 255.0f;
                a = pixel->a / 255.0f;
            }
            maxRgb = std::max(maxRgb, std::max(r, std::max(g, b)));
            alphaSum += a;
            ++samples;
            if (maxRgb > 0.015f) {
                return false;
            }
        }
    }
    return samples > 0 && alphaSum / samples > 0.75f && maxRgb < 0.015f;
}

bool sourceLooksLikeBlankAdjustmentByDepth(const ImageInfo& source)
{
    if (std::strcmp(source.pixelDepth, kOfxBitDepthFloat) == 0) {
        return sourceLooksLikeBlankAdjustment<OfxRGBAColourF>(source);
    }
    if (std::strcmp(source.pixelDepth, kOfxBitDepthByte) == 0) {
        return sourceLooksLikeBlankAdjustment<OfxRGBAColourB>(source);
    }
    return false;
}

double doubleParamAtTime(
    OfxImageEffectHandle instance,
    const char* name,
    OfxTime time,
    double fallback)
{
    OfxParamSetHandle paramSet = nullptr;
    OfxParamHandle param = nullptr;
    if (gEffectHost->getParamSet(instance, &paramSet) != kOfxStatOK ||
        gParamHost->paramGetHandle(paramSet, name, &param, nullptr) != kOfxStatOK) {
        return fallback;
    }
    double value = fallback;
    return gParamHost->paramGetValueAtTime(param, time, &value) == kOfxStatOK ? value : fallback;
}

int intParamAtTime(
    OfxImageEffectHandle instance,
    const char* name,
    OfxTime time,
    int fallback)
{
    OfxParamSetHandle paramSet = nullptr;
    OfxParamHandle param = nullptr;
    if (gEffectHost->getParamSet(instance, &paramSet) != kOfxStatOK ||
        gParamHost->paramGetHandle(paramSet, name, &param, nullptr) != kOfxStatOK) {
        return fallback;
    }
    int value = fallback;
    return gParamHost->paramGetValueAtTime(param, time, &value) == kOfxStatOK ? value : fallback;
}

std::string stringParamAtTime(
    OfxImageEffectHandle instance,
    const char* name,
    OfxTime time,
    const char* fallback)
{
    OfxParamSetHandle paramSet = nullptr;
    OfxParamHandle param = nullptr;
    if (gEffectHost->getParamSet(instance, &paramSet) != kOfxStatOK ||
        gParamHost->paramGetHandle(paramSet, name, &param, nullptr) != kOfxStatOK) {
        return fallback;
    }
    char* value = nullptr;
    if (gParamHost->paramGetValueAtTime(param, time, &value) != kOfxStatOK || !value) {
        return fallback;
    }
    return value;
}

bool setStringParam(
    OfxImageEffectHandle instance,
    const char* name,
    const std::string& value)
{
    OfxParamSetHandle paramSet = nullptr;
    OfxParamHandle param = nullptr;
    return gEffectHost->getParamSet(instance, &paramSet) == kOfxStatOK &&
        gParamHost->paramGetHandle(paramSet, name, &param, nullptr) == kOfxStatOK &&
        gParamHost->paramSetValue(param, value.c_str()) == kOfxStatOK;
}

buckswood_lookdna::Controls controlsAtTime(OfxImageEffectHandle instance, OfxTime time)
{
    buckswood_lookdna::Controls controls;
    controls.enabled = intParamAtTime(instance, "enabled", time, 1);
    controls.inputSpace = intParamAtTime(instance, "inputSpace", time, buckswood_lookdna::TimelineDisplay);
    controls.referenceSpace = intParamAtTime(instance, "referenceSpace", time, buckswood_lookdna::SRGBDisplay);
    controls.matchMode = intParamAtTime(instance, "matchMode", time, buckswood_lookdna::FullLook);
    controls.analysisQuality = intParamAtTime(instance, "analysisQuality", time, 1);
    controls.viewMode = intParamAtTime(instance, "viewMode", time, buckswood_lookdna::ResultView);
    controls.temporalRadius = intParamAtTime(instance, "temporalRadius", time, 2);
    controls.referenceBMix = static_cast<float>(doubleParamAtTime(instance, "referenceBMix", time, 0.35));
    controls.referenceCMix = static_cast<float>(doubleParamAtTime(instance, "referenceCMix", time, 0.20));
    controls.referenceAdaptivity = static_cast<float>(doubleParamAtTime(instance, "referenceAdaptivity", time, 0.70));
    controls.matchStrength = static_cast<float>(doubleParamAtTime(instance, "matchStrength", time, 0.82));
    controls.toneMatch = static_cast<float>(doubleParamAtTime(instance, "toneMatch", time, 0.78));
    controls.paletteMatch = static_cast<float>(doubleParamAtTime(instance, "paletteMatch", time, 0.72));
    controls.semanticMatch = static_cast<float>(doubleParamAtTime(instance, "semanticMatch", time, 0.62));
    controls.localContrastMatch = static_cast<float>(doubleParamAtTime(instance, "localContrastMatch", time, 0.42));
    controls.textureMatch = static_cast<float>(doubleParamAtTime(instance, "textureMatch", time, 0.28));
    controls.grainMatch = static_cast<float>(doubleParamAtTime(instance, "grainMatch", time, 0.18));
    controls.densityMatch = static_cast<float>(doubleParamAtTime(instance, "densityMatch", time, 0.45));
    controls.exposureLock = static_cast<float>(doubleParamAtTime(instance, "exposureLock", time, 0.35));
    controls.skinProtect = static_cast<float>(doubleParamAtTime(instance, "skinProtect", time, 0.72));
    controls.highlightProtect = static_cast<float>(doubleParamAtTime(instance, "highlightProtect", time, 0.74));
    controls.sceneIdentityGuard = static_cast<float>(doubleParamAtTime(instance, "sceneIdentityGuard", time, 0.70));
    controls.temporalStability = static_cast<float>(doubleParamAtTime(instance, "temporalStability", time, 0.65));
    controls.spatialMatch = static_cast<float>(doubleParamAtTime(instance, "spatialMatch", time, 0.35));
    controls.shadowMatch = static_cast<float>(doubleParamAtTime(instance, "shadowMatch", time, 0.82));
    controls.midtoneMatch = static_cast<float>(doubleParamAtTime(instance, "midtoneMatch", time, 1.0));
    controls.highlightMatch = static_cast<float>(doubleParamAtTime(instance, "highlightMatch", time, 0.72));
    controls.gamutGuard = static_cast<float>(doubleParamAtTime(instance, "gamutGuard", time, 0.86));
    controls.splitPosition = static_cast<float>(doubleParamAtTime(instance, "splitPosition", time, 0.50));
    controls.outputMix = static_cast<float>(doubleParamAtTime(instance, "outputMix", time, 1.0));
    return controls;
}

class FloatSampler final : public buckswood_lookdna::Sampler {
public:
    explicit FloatSampler(const ImageInfo& info)
        : info_(info)
        , base_(reinterpret_cast<OfxRGBAColourF*>(info.data))
    {
    }

    buckswood_lookdna::Pixel sample(float x, float y) const override
    {
        x = std::max(static_cast<float>(info_.bounds.x1), std::min(static_cast<float>(info_.bounds.x2 - 1), x));
        y = std::max(static_cast<float>(info_.bounds.y1), std::min(static_cast<float>(info_.bounds.y2 - 1), y));
        const int x0 = static_cast<int>(x);
        const int y0 = static_cast<int>(y);
        const int x1 = std::min(info_.bounds.x2 - 1, x0 + 1);
        const int y1 = std::min(info_.bounds.y2 - 1, y0 + 1);
        const float tx = x - static_cast<float>(x0);
        const float ty = y - static_cast<float>(y0);
        const auto p00 = read(x0, y0);
        const auto p10 = read(x1, y0);
        const auto p01 = read(x0, y1);
        const auto p11 = read(x1, y1);
        const float ux = 1.0f - tx;
        const float uy = 1.0f - ty;
        return buckswood_lookdna::Pixel{
            (p00.r * ux + p10.r * tx) * uy + (p01.r * ux + p11.r * tx) * ty,
            (p00.g * ux + p10.g * tx) * uy + (p01.g * ux + p11.g * tx) * ty,
            (p00.b * ux + p10.b * tx) * uy + (p01.b * ux + p11.b * tx) * ty,
            (p00.a * ux + p10.a * tx) * uy + (p01.a * ux + p11.a * tx) * ty,
        };
    }

    buckswood_lookdna::Bounds bounds() const override
    {
        return buckswood_lookdna::Bounds{info_.bounds.x1, info_.bounds.y1, info_.bounds.x2, info_.bounds.y2};
    }

private:
    buckswood_lookdna::Pixel read(int x, int y) const
    {
        const auto* pixel = pixelAddress(base_, info_.bounds, x, y, info_.rowBytes);
        return pixel
            ? buckswood_lookdna::Pixel{pixel->r, pixel->g, pixel->b, pixel->a}
            : buckswood_lookdna::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const ImageInfo& info_;
    OfxRGBAColourF* base_;
};

class ByteSampler final : public buckswood_lookdna::Sampler {
public:
    explicit ByteSampler(const ImageInfo& info)
        : info_(info)
        , base_(reinterpret_cast<OfxRGBAColourB*>(info.data))
    {
    }

    buckswood_lookdna::Pixel sample(float x, float y) const override
    {
        x = std::max(static_cast<float>(info_.bounds.x1), std::min(static_cast<float>(info_.bounds.x2 - 1), x));
        y = std::max(static_cast<float>(info_.bounds.y1), std::min(static_cast<float>(info_.bounds.y2 - 1), y));
        const int x0 = static_cast<int>(x);
        const int y0 = static_cast<int>(y);
        const int x1 = std::min(info_.bounds.x2 - 1, x0 + 1);
        const int y1 = std::min(info_.bounds.y2 - 1, y0 + 1);
        const float tx = x - static_cast<float>(x0);
        const float ty = y - static_cast<float>(y0);
        const auto p00 = read(x0, y0);
        const auto p10 = read(x1, y0);
        const auto p01 = read(x0, y1);
        const auto p11 = read(x1, y1);
        const float ux = 1.0f - tx;
        const float uy = 1.0f - ty;
        return buckswood_lookdna::Pixel{
            (p00.r * ux + p10.r * tx) * uy + (p01.r * ux + p11.r * tx) * ty,
            (p00.g * ux + p10.g * tx) * uy + (p01.g * ux + p11.g * tx) * ty,
            (p00.b * ux + p10.b * tx) * uy + (p01.b * ux + p11.b * tx) * ty,
            (p00.a * ux + p10.a * tx) * uy + (p01.a * ux + p11.a * tx) * ty,
        };
    }

    buckswood_lookdna::Bounds bounds() const override
    {
        return buckswood_lookdna::Bounds{info_.bounds.x1, info_.bounds.y1, info_.bounds.x2, info_.bounds.y2};
    }

private:
    buckswood_lookdna::Pixel read(int x, int y) const
    {
        const auto* pixel = pixelAddress(base_, info_.bounds, x, y, info_.rowBytes);
        return pixel
            ? buckswood_lookdna::Pixel{
                  pixel->r / 255.0f,
                  pixel->g / 255.0f,
                  pixel->b / 255.0f,
                  pixel->a / 255.0f}
            : buckswood_lookdna::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const ImageInfo& info_;
    OfxRGBAColourB* base_;
};

template <typename DstPixel, typename SamplerType>
OfxStatus renderTyped(
    OfxImageEffectHandle instance,
    OfxTime time,
    const OfxRectI& renderWindow,
    const ImageInfo& source,
    const ImageInfo* previous2,
    const ImageInfo* previous1,
    const ImageInfo* next1,
    const ImageInfo* next2,
    const ImageInfo& destination,
    const buckswood_lookdna::Controls& controls,
    const std::array<std::shared_ptr<const buckswood_lookdna::ReferenceAsset>, 3>& references)
{
    const SamplerType sampler(source);
    const auto currentProfile = buckswood_lookdna::LookDNACore::analyze(
        sampler,
        controls.inputSpace,
        controls.analysisQuality);

    auto analyzeNeighbor = [&](const ImageInfo* info, buckswood_lookdna::LookProfile& profile)
        -> const buckswood_lookdna::LookProfile* {
        if (!info || controls.temporalStability <= 0.0001f) {
            return static_cast<const buckswood_lookdna::LookProfile*>(nullptr);
        }
        const SamplerType neighborSampler(*info);
        profile = buckswood_lookdna::LookDNACore::analyze(
            neighborSampler,
            controls.inputSpace,
            controls.analysisQuality);
        return profile.valid ? &profile : nullptr;
    };
    buckswood_lookdna::LookProfile previous2Profile;
    buckswood_lookdna::LookProfile previous1Profile;
    buckswood_lookdna::LookProfile next1Profile;
    buckswood_lookdna::LookProfile next2Profile;
    const auto* previous2ProfilePtr = analyzeNeighbor(previous2, previous2Profile);
    const auto* previous1ProfilePtr = analyzeNeighbor(previous1, previous1Profile);
    const auto* next1ProfilePtr = analyzeNeighbor(next1, next1Profile);
    const auto* next2ProfilePtr = analyzeNeighbor(next2, next2Profile);
    float temporalConfidence = 1.0f;
    const auto stableProfile = buckswood_lookdna::LookDNACore::stabilizeProfile5(
        currentProfile,
        previous2ProfilePtr,
        previous1ProfilePtr,
        next1ProfilePtr,
        next2ProfilePtr,
        controls.temporalStability,
        &temporalConfidence);

    const std::array<const buckswood_lookdna::LookProfile*, 3> referenceProfiles{
        references[0] && references[0]->valid() ? &references[0]->profile : nullptr,
        references[1] && references[1]->valid() ? &references[1]->profile : nullptr,
        references[2] && references[2]->valid() ? &references[2]->profile : nullptr,
    };
    const std::array<float, 3> requestedReferenceWeights{
        1.0f,
        controls.referenceBMix,
        controls.referenceCMix,
    };
    std::array<float, 3> normalizedReferenceWeights{};
    const auto blendedReference = buckswood_lookdna::LookDNACore::blendReferenceProfiles(
        stableProfile,
        referenceProfiles,
        requestedReferenceWeights,
        controls.referenceAdaptivity,
        &normalizedReferenceWeights);
    auto globalMatch = buckswood_lookdna::LookDNACore::buildMatch(
        stableProfile,
        blendedReference,
        controls);
    globalMatch.referenceWeights = normalizedReferenceWeights;
    globalMatch.temporalConfidence = temporalConfidence;

    std::array<buckswood_lookdna::MatchContext, buckswood_lookdna::SpatialProfileCount> spatialMatches{};
    bool hasSpatialReference = false;
    for (const auto& reference : references) {
        hasSpatialReference = hasSpatialReference ||
            (reference && reference->spatialProfiles.valid);
    }
    const auto sourceGrid = controls.spatialMatch > 0.0001f && hasSpatialReference
        ? buckswood_lookdna::LookDNACore::analyzeGrid(
              sampler,
              controls.inputSpace,
              controls.analysisQuality)
        : buckswood_lookdna::ProfileGrid{};
    if (sourceGrid.valid) {
        for (int cell = 0; cell < buckswood_lookdna::SpatialProfileCount; ++cell) {
            const std::array<const buckswood_lookdna::LookProfile*, 3> regionalReferences{
                references[0] && references[0]->spatialProfiles.valid
                    ? &references[0]->spatialProfiles.cells[static_cast<std::size_t>(cell)]
                    : referenceProfiles[0],
                references[1] && references[1]->spatialProfiles.valid
                    ? &references[1]->spatialProfiles.cells[static_cast<std::size_t>(cell)]
                    : referenceProfiles[1],
                references[2] && references[2]->spatialProfiles.valid
                    ? &references[2]->spatialProfiles.cells[static_cast<std::size_t>(cell)]
                    : referenceProfiles[2],
            };
            std::array<float, 3> regionalWeights{};
            const auto regionalReference = buckswood_lookdna::LookDNACore::blendReferenceProfiles(
                sourceGrid.cells[static_cast<std::size_t>(cell)],
                regionalReferences,
                requestedReferenceWeights,
                controls.referenceAdaptivity,
                &regionalWeights);
            auto regionalMatch = buckswood_lookdna::LookDNACore::buildMatch(
                sourceGrid.cells[static_cast<std::size_t>(cell)],
                regionalReference,
                controls);
            regionalMatch.referenceWeights = regionalWeights;
            regionalMatch.temporalConfidence = temporalConfidence;
            spatialMatches[static_cast<std::size_t>(cell)] = regionalMatch;
        }
    } else {
        spatialMatches.fill(globalMatch);
    }

    const buckswood_lookdna::Sampler* referencePreview =
        references[0] && references[0]->image
        ? static_cast<const buckswood_lookdna::Sampler*>(references[0]->image.get())
        : references[1] && references[1]->image
            ? static_cast<const buckswood_lookdna::Sampler*>(references[1]->image.get())
            : references[2] && references[2]->image
                ? static_cast<const buckswood_lookdna::Sampler*>(references[2]->image.get())
                : nullptr;
    const buckswood_lookdna::FrameInfo frame{
        source.bounds.x2 - source.bounds.x1,
        source.bounds.y2 - source.bounds.y1,
        static_cast<int>(time),
    };
    int spatialLookupWidth = 0;
    int spatialLookupHeight = 0;
    std::vector<buckswood_lookdna::MatchContext> spatialLookup;
    if (sourceGrid.valid) {
        spatialLookupWidth = std::min(160, std::max(24, frame.width / 16));
        spatialLookupHeight = std::min(90, std::max(14, frame.height / 16));
        spatialLookup.resize(static_cast<std::size_t>(spatialLookupWidth * spatialLookupHeight));
        for (int lookupY = 0; lookupY < spatialLookupHeight; ++lookupY) {
            const float normalizedY =
                (static_cast<float>(lookupY) + 0.5f) / static_cast<float>(spatialLookupHeight);
            for (int lookupX = 0; lookupX < spatialLookupWidth; ++lookupX) {
                const float normalizedX =
                    (static_cast<float>(lookupX) + 0.5f) / static_cast<float>(spatialLookupWidth);
                spatialLookup[static_cast<std::size_t>(
                    lookupY * spatialLookupWidth + lookupX)] =
                    buckswood_lookdna::LookDNACore::sampleSpatialMatch(
                        globalMatch,
                        spatialMatches,
                        normalizedX,
                        normalizedY,
                        controls.spatialMatch);
            }
        }
    }
    auto* destinationBase = reinterpret_cast<DstPixel*>(destination.data);

    auto renderRows = [&](int firstY, int lastY) {
        for (int y = firstY; y < lastY; ++y) {
            if (gEffectHost->abort(instance)) {
                break;
            }
            for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
                auto* destinationPixel = pixelAddress(
                    destinationBase,
                    destination.bounds,
                    x,
                    y,
                    destination.rowBytes);
                if (!destinationPixel) {
                    continue;
                }
                const buckswood_lookdna::MatchContext* pixelMatch = &globalMatch;
                if (!spatialLookup.empty()) {
                    const int lookupX = std::min(
                        spatialLookupWidth - 1,
                        std::max(0, (x - source.bounds.x1) * spatialLookupWidth / std::max(1, frame.width)));
                    const int lookupY = std::min(
                        spatialLookupHeight - 1,
                        std::max(0, (y - source.bounds.y1) * spatialLookupHeight / std::max(1, frame.height)));
                    pixelMatch = &spatialLookup[static_cast<std::size_t>(
                        lookupY * spatialLookupWidth + lookupX)];
                }
                const buckswood_lookdna::Pixel output = controls.enabled
                    ? buckswood_lookdna::LookDNACore::processPixel(
                          sampler,
                          x,
                          y,
                          frame,
                          controls,
                          *pixelMatch,
                          referencePreview)
                    : sampler.sample(static_cast<float>(x), static_cast<float>(y));
                writePixel(destinationPixel, output);
            }
        }
    };

    return buckswood::ofx_runtime::parallelRows(
        gThreadHost,
        renderWindow.y1,
        renderWindow.y2,
        renderRows);
}

class NoImageException {};

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
    std::array<OfxPropertySetHandle, 4> temporalImages{};
    OfxStatus status = kOfxStatOK;
    try {
        if (gEffectHost->clipGetImage(outputClip, time, nullptr, &outputImage) != kOfxStatOK) {
            throw NoImageException();
        }
        ImageInfo destination;
        if (!readImageInfo(outputImage, destination)) {
            throw NoImageException();
        }
        if (gEffectHost->clipGetImage(sourceClip, time, nullptr, &sourceImage) != kOfxStatOK) {
            status = fillTransparentByDepth(instance, renderWindow, destination);
        } else {
            ImageInfo source;
            if (!readImageInfo(sourceImage, source)) {
                throw NoImageException();
            }
            const int adjustmentLayerGuard = intParamAtTime(instance, "adjustmentLayerGuard", time, 1);
            if (adjustmentLayerGuard && sourceLooksLikeBlankAdjustmentByDepth(source)) {
                status = fillTransparentByDepth(instance, renderWindow, destination);
            } else {
                const auto controls = controlsAtTime(instance, time);
                const std::array<std::string, 3> referencePaths{
                    stringParamAtTime(instance, "referencePath", time, ""),
                    stringParamAtTime(instance, "referencePathB", time, ""),
                    stringParamAtTime(instance, "referencePathC", time, ""),
                };
                std::array<std::shared_ptr<const buckswood_lookdna::ReferenceAsset>, 3> references{};
                for (std::size_t index = 0; index < references.size(); ++index) {
                    references[index] = buckswood_lookdna::ReferenceImageLoader::loadCached(
                        referencePaths[index],
                        controls.referenceSpace,
                        controls.analysisQuality);
                }

                std::array<ImageInfo, 4> temporalInfo{};
                std::array<ImageInfo*, 4> temporal{};
                auto fetchTemporal = [&](std::size_t index, double offset) {
                    if (gEffectHost->clipGetImage(
                            sourceClip,
                            time + offset,
                            nullptr,
                            &temporalImages[index]) == kOfxStatOK &&
                        readImageInfo(temporalImages[index], temporalInfo[index]) &&
                        std::strcmp(temporalInfo[index].pixelDepth, source.pixelDepth) == 0) {
                        temporal[index] = &temporalInfo[index];
                    }
                };
                if (controls.temporalStability > 0.0001f && controls.temporalRadius > 0) {
                    fetchTemporal(1, -1.0);
                    fetchTemporal(2, 1.0);
                    if (controls.temporalRadius > 1) {
                        fetchTemporal(0, -2.0);
                        fetchTemporal(3, 2.0);
                    }
                }

                if (std::strcmp(destination.pixelDepth, kOfxBitDepthFloat) == 0 &&
                    std::strcmp(source.pixelDepth, kOfxBitDepthFloat) == 0) {
                    status = renderTyped<OfxRGBAColourF, FloatSampler>(
                        instance,
                        time,
                        renderWindow,
                        source,
                        temporal[0],
                        temporal[1],
                        temporal[2],
                        temporal[3],
                        destination,
                        controls,
                        references);
                } else if (std::strcmp(destination.pixelDepth, kOfxBitDepthByte) == 0 &&
                    std::strcmp(source.pixelDepth, kOfxBitDepthByte) == 0) {
                    status = renderTyped<OfxRGBAColourB, ByteSampler>(
                        instance,
                        time,
                        renderWindow,
                        source,
                        temporal[0],
                        temporal[1],
                        temporal[2],
                        temporal[3],
                        destination,
                        controls,
                        references);
                } else {
                    status = kOfxStatErrUnsupported;
                }
            }
        }
    } catch (NoImageException&) {
        status = gEffectHost->abort(instance) ? kOfxStatOK : kOfxStatFailed;
    }

    if (sourceImage) {
        gEffectHost->clipReleaseImage(sourceImage);
    }
    for (OfxPropertySetHandle image : temporalImages) {
        if (image) {
            gEffectHost->clipReleaseImage(image);
        }
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
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeDouble, name, &properties);
    gPropHost->propSetString(properties, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, label);
    gPropHost->propSetString(properties, kOfxParamPropDoubleType, 0, kOfxParamDoubleTypePlain);
    gPropHost->propSetDouble(properties, kOfxParamPropDefault, 0, defaultValue);
    gPropHost->propSetDouble(properties, kOfxParamPropMin, 0, minValue);
    gPropHost->propSetDouble(properties, kOfxParamPropMax, 0, maxValue);
    gPropHost->propSetDouble(properties, kOfxParamPropDisplayMin, 0, minValue);
    gPropHost->propSetDouble(properties, kOfxParamPropDisplayMax, 0, maxValue);
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
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeChoice, name, &properties);
    gPropHost->propSetString(properties, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, label);
    gPropHost->propSetInt(properties, kOfxParamPropDefault, 0, defaultValue);
    for (int index = 0; index < optionCount; ++index) {
        gPropHost->propSetString(properties, kOfxParamPropChoiceOption, index, options[index]);
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
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeBoolean, name, &properties);
    gPropHost->propSetString(properties, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, label);
    gPropHost->propSetInt(properties, kOfxParamPropDefault, 0, defaultValue);
    gPropHost->propSetString(page, kOfxParamPropPageChild, pageIndex, name);
}

void defineFileParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeString, name, &properties);
    gPropHost->propSetString(properties, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, label);
    gPropHost->propSetString(properties, kOfxParamPropStringMode, 0, kOfxParamStringIsFilePath);
    gPropHost->propSetInt(properties, kOfxParamPropStringFilePathExists, 0, 1);
    gPropHost->propSetString(properties, kOfxParamPropDefault, 0, "");
    gPropHost->propSetString(page, kOfxParamPropPageChild, pageIndex, name);
}

void defineButtonParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypePushButton, name, &properties);
    gPropHost->propSetString(properties, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, label);
    gPropHost->propSetString(page, kOfxParamPropPageChild, pageIndex, name);
}

OfxStatus describeInContext(OfxImageEffectHandle effect)
{
    OfxPropertySetHandle properties = nullptr;
    gEffectHost->clipDefine(effect, kOfxImageEffectOutputClipName, &properties);
    gPropHost->propSetString(properties, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gEffectHost->clipDefine(effect, kOfxImageEffectSimpleSourceClipName, &properties);
    gPropHost->propSetString(properties, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropHost->propSetInt(properties, kOfxImageEffectPropTemporalClipAccess, 0, 1);

    OfxParamSetHandle paramSet = nullptr;
    gEffectHost->getParamSet(effect, &paramSet);
    OfxPropertySetHandle page = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypePage, "Main", &page);
    gPropHost->propSetString(page, kOfxPropLabel, 0, "Look DNA");

    const char* colorSpaces[] = {
        "Timeline / Display Referred",
        "Rec.709 Gamma 2.4",
        "sRGB",
        "Scene Linear",
        "ARRI LogC3",
        "ARRI LogC4",
        "Sony S-Log3",
        "Apple Log",
        "BMD Film Gen 5 / DWG",
    };
    const char* qualities[] = {"Draft", "High", "Ultra"};
    const char* modes[] = {"Full Look", "Color Only", "Tone Only", "Texture Only"};
    const char* temporalRadii[] = {"Off", "3 Frames", "5 Frames"};
    const char* views[] = {
        "Matched Result",
        "Original / Match Split",
        "Match Difference",
        "Match Confidence",
        "Skin Protection Matte",
        "Semantic Zones",
        "Reference Preview",
        "Tone Mapping View",
        "Gamut Warning",
        "Spatial Look Map",
        "Reference Balance",
        "Temporal Cut Guard",
    };

    defineBooleanParam(paramSet, "enabled", "Enable Look Match", 1, 0, page);
    defineFileParam(paramSet, "referencePath", "Reference A / BWLOOK", 1, page);
    defineButtonParam(paramSet, "browseReferenceA", "Browse / Load Reference A...", 2, page);
    defineFileParam(paramSet, "referencePathB", "Reference B (Optional)", 3, page);
    defineButtonParam(paramSet, "browseReferenceB", "Browse / Load Reference B...", 4, page);
    defineFileParam(paramSet, "referencePathC", "Reference C (Optional)", 5, page);
    defineButtonParam(paramSet, "browseReferenceC", "Browse / Load Reference C...", 6, page);
    defineButtonParam(paramSet, "refreshReference", "Refresh Reference Analysis", 7, page);
    defineDoubleParam(paramSet, "referenceBMix", "Reference B Mix", 0.35, 0.0, 1.0, 8, page);
    defineDoubleParam(paramSet, "referenceCMix", "Reference C Mix", 0.20, 0.0, 1.0, 9, page);
    defineDoubleParam(paramSet, "referenceAdaptivity", "Auto Reference Balance", 0.70, 0.0, 1.0, 10, page);
    defineChoiceParam(paramSet, "referenceSpace", "Reference Color Space", 2, colorSpaces, 9, 11, page);
    defineChoiceParam(paramSet, "inputSpace", "Footage Color Space", 0, colorSpaces, 9, 12, page);
    defineChoiceParam(paramSet, "analysisQuality", "Analysis Quality", 1, qualities, 3, 13, page);
    defineChoiceParam(paramSet, "matchMode", "Match Mode", 0, modes, 4, 14, page);
    defineDoubleParam(paramSet, "matchStrength", "Overall Match", 0.82, 0.0, 1.0, 15, page);
    defineDoubleParam(paramSet, "toneMatch", "Tone & Contrast", 0.78, 0.0, 1.0, 16, page);
    defineDoubleParam(paramSet, "paletteMatch", "Palette & Color Separation", 0.72, 0.0, 1.0, 17, page);
    defineDoubleParam(paramSet, "densityMatch", "Color Density", 0.45, 0.0, 1.0, 18, page);
    defineDoubleParam(paramSet, "semanticMatch", "Semantic Region Match", 0.62, 0.0, 1.0, 19, page);
    defineDoubleParam(paramSet, "localContrastMatch", "Local Contrast Match", 0.42, 0.0, 1.0, 20, page);
    defineDoubleParam(paramSet, "textureMatch", "Texture Match", 0.28, 0.0, 1.0, 21, page);
    defineDoubleParam(paramSet, "grainMatch", "Reference Grain", 0.18, 0.0, 1.0, 22, page);
    defineDoubleParam(paramSet, "shadowMatch", "Shadow Transfer", 0.82, 0.0, 1.0, 23, page);
    defineDoubleParam(paramSet, "midtoneMatch", "Midtone Transfer", 1.0, 0.0, 1.0, 24, page);
    defineDoubleParam(paramSet, "highlightMatch", "Highlight Transfer", 0.72, 0.0, 1.0, 25, page);
    defineDoubleParam(paramSet, "exposureLock", "Preserve Footage Exposure", 0.35, 0.0, 1.0, 26, page);
    defineDoubleParam(paramSet, "skinProtect", "Skin Identity Guard", 0.72, 0.0, 1.0, 27, page);
    defineDoubleParam(paramSet, "highlightProtect", "Highlight Identity Guard", 0.74, 0.0, 1.0, 28, page);
    defineDoubleParam(paramSet, "sceneIdentityGuard", "Scene Identity Guard", 0.70, 0.0, 1.0, 29, page);
    defineDoubleParam(paramSet, "temporalStability", "Temporal Look Stability", 0.65, 0.0, 1.0, 30, page);
    defineChoiceParam(paramSet, "temporalRadius", "Temporal Analysis", 2, temporalRadii, 3, 31, page);
    defineDoubleParam(paramSet, "spatialMatch", "Spatial Look Map", 0.35, 0.0, 1.0, 32, page);
    defineDoubleParam(paramSet, "gamutGuard", "Gamut Guard", 0.86, 0.0, 1.0, 33, page);
    defineDoubleParam(paramSet, "splitPosition", "Split Position", 0.50, 0.0, 1.0, 34, page);
    defineDoubleParam(paramSet, "outputMix", "Output Mix", 1.0, 0.0, 1.0, 35, page);
    defineChoiceParam(paramSet, "viewMode", "View", 0, views, 12, 36, page);
    defineBooleanParam(paramSet, "adjustmentLayerGuard", "Adjustment Layer Guard", 1, 37, page);
    return kOfxStatOK;
}

OfxStatus describe(OfxImageEffectHandle effect)
{
    OfxPropertySetHandle properties = nullptr;
    gEffectHost->getPropertySet(effect, &properties);
    gPropHost->propSetInt(properties, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 0);
    gPropHost->propSetInt(properties, kOfxImageEffectPropSupportsTiles, 0, 0);
    gPropHost->propSetInt(properties, kOfxImageEffectPropTemporalClipAccess, 0, 1);
    gPropHost->propSetString(properties, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gPropHost->propSetString(properties, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthByte);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, "Buckswood Look DNA v2.1");
    gPropHost->propSetString(properties, kOfxImageEffectPluginPropGrouping, 0, "Buckswood");
    gPropHost->propSetString(properties, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    return kOfxStatOK;
}

OfxStatus onLoad()
{
    if (!gHost) {
        return kOfxStatErrMissingHostFeature;
    }
    gEffectHost = const_cast<OfxImageEffectSuiteV1*>(
        reinterpret_cast<const OfxImageEffectSuiteV1*>(
            gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1)));
    gPropHost = const_cast<OfxPropertySuiteV1*>(
        reinterpret_cast<const OfxPropertySuiteV1*>(
            gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1)));
    gParamHost = const_cast<OfxParameterSuiteV1*>(
        reinterpret_cast<const OfxParameterSuiteV1*>(
            gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1)));
    gThreadHost = const_cast<OfxMultiThreadSuiteV1*>(
        reinterpret_cast<const OfxMultiThreadSuiteV1*>(
            gHost->fetchSuite(gHost->host, kOfxMultiThreadSuite, 1)));
    return gEffectHost && gPropHost && gParamHost ? kOfxStatOK : kOfxStatErrMissingHostFeature;
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
    const int radius = intParamAtTime(instance, "temporalRadius", time, 2);
    if (doubleParamAtTime(instance, "temporalStability", time, 0.65) <= 0.0001 || radius <= 0) {
        return kOfxStatReplyDefault;
    }
    const double frameRadius = radius > 1 ? 2.0 : 1.0;
    const double ranges[4] = {
        time - frameRadius,
        time - frameRadius,
        time + frameRadius,
        time + frameRadius,
    };
    gPropHost->propSetDoubleN(outArgs, kSourceFrameRangeProp, 4, ranges);
    return kOfxStatOK;
}

OfxStatus instanceChanged(
    OfxImageEffectHandle instance,
    OfxPropertySetHandle inArgs)
{
    if (!inArgs) {
        return kOfxStatReplyDefault;
    }
    char* name = nullptr;
    if (gPropHost->propGetString(inArgs, kOfxPropName, 0, &name) != kOfxStatOK || !name) {
        return kOfxStatReplyDefault;
    }
    const char* targetReference = nullptr;
    if (std::strcmp(name, "browseReferenceA") == 0) {
        targetReference = "referencePath";
    } else if (std::strcmp(name, "browseReferenceB") == 0) {
        targetReference = "referencePathB";
    } else if (std::strcmp(name, "browseReferenceC") == 0) {
        targetReference = "referencePathC";
    }
    if (targetReference) {
        const std::string initialPath =
            stringParamAtTime(instance, targetReference, 0.0, "");
        std::string selectedPath;
        if (buckswood_lookdna::openReferenceFileDialog(initialPath, selectedPath) &&
            setStringParam(instance, targetReference, selectedPath)) {
            buckswood_lookdna::ReferenceImageLoader::clearCache();
        }
        return kOfxStatOK;
    }
    if (std::strcmp(name, "refreshReference") == 0 ||
        std::strcmp(name, "referencePath") == 0 ||
        std::strcmp(name, "referencePathB") == 0 ||
        std::strcmp(name, "referencePathC") == 0 ||
        std::strcmp(name, "referenceSpace") == 0 ||
        std::strcmp(name, "analysisQuality") == 0) {
        buckswood_lookdna::ReferenceImageLoader::clearCache();
    }
    return kOfxStatOK;
}

OfxStatus pluginMain(
    const char* action,
    const void* handle,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle outArgs)
{
    try {
        auto effect = reinterpret_cast<OfxImageEffectHandle>(const_cast<void*>(handle));
        if (std::strcmp(action, kOfxActionLoad) == 0) {
            return onLoad();
        }
        if (std::strcmp(action, kOfxActionDescribe) == 0) {
            return describe(effect);
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
        if (std::strcmp(action, kOfxActionInstanceChanged) == 0) {
            return instanceChanged(effect, inArgs);
        }
        if (std::strcmp(action, kOfxActionCreateInstance) == 0 ||
            std::strcmp(action, kOfxActionDestroyInstance) == 0) {
            return kOfxStatOK;
        }
    } catch (std::bad_alloc&) {
        return kOfxStatErrMemory;
    } catch (std::exception&) {
        return kOfxStatErrUnknown;
    }
    return kOfxStatReplyDefault;
}

void setHost(OfxHost* host)
{
    gHost = host;
}

OfxPlugin plugin = {
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

}
