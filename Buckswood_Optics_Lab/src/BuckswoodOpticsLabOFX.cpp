#include <algorithm>
#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <type_traits>

#include "OpticsAssetLibrary.h"
#include "OpticsLabCore.h"
#include "OfxRenderRuntime.h"
#if defined(__APPLE__)
#include "OpticsLabMetal.h"
#endif

#include "ofxGPURender.h"
#include "ofxImageEffect.h"
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
OfxMultiThreadSuiteV1* gThreadHost = nullptr;

constexpr const char* kPluginIdentifier = "com.buckswood.optics.lab";
constexpr int kPluginMajorVersion = 1;
constexpr int kPluginMinorVersion = 3;

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

#if defined(__APPLE__)
buckswood::gpu::PixelFormat gpuPixelFormat(const char* pixelDepth)
{
    return std::strcmp(pixelDepth, kOfxBitDepthFloat) == 0
        ? buckswood::gpu::PixelFormat::Float32
        : buckswood::gpu::PixelFormat::Byte;
}

buckswood::gpu::ImageBuffer gpuImageBuffer(const ImageInfo& info)
{
    return buckswood::gpu::ImageBuffer{
        info.data,
        info.rowBytes,
        info.bounds.x1,
        info.bounds.y1,
        info.bounds.x2,
        info.bounds.y2,
        gpuPixelFormat(info.pixelDepth),
    };
}
#endif

float clamp01(float value)
{
    return std::min(1.0f, std::max(0.0f, value));
}

unsigned char toByte(float value)
{
    return static_cast<unsigned char>(clamp01(value) * 255.0f + 0.5f);
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
    return gParamHost->paramGetValueAtTime(param, time, &value) == kOfxStatOK
        ? value
        : fallback;
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
    return gParamHost->paramGetValueAtTime(param, time, &value) == kOfxStatOK
        ? value
        : fallback;
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
        return fallback ? fallback : "";
    }
    char* value = nullptr;
    if (gParamHost->paramGetValueAtTime(param, time, &value) != kOfxStatOK || !value) {
        return fallback ? fallback : "";
    }
    return value;
}

buckswood_optics::Controls controlsAtTime(OfxImageEffectHandle instance, OfxTime time)
{
    buckswood_optics::Controls c{};
    c.preset = intParamAtTime(instance, "preset", time, 6);
    c.effectStrength = static_cast<float>(doubleParamAtTime(instance, "effectStrength", time, 0.65));

    c.focalLength = static_cast<float>(doubleParamAtTime(instance, "focalLength", time, 50.0));
    c.fStop = static_cast<float>(doubleParamAtTime(instance, "fStop", time, 2.8));
    c.focusDistance = static_cast<float>(doubleParamAtTime(instance, "focusDistance", time, 3.0));
    c.sensorWidth = static_cast<float>(doubleParamAtTime(instance, "sensorWidth", time, 36.0));
    c.anamorphicSqueeze = static_cast<float>(doubleParamAtTime(instance, "anamorphicSqueeze", time, 1.0));
    c.anamorphicAngle = static_cast<float>(doubleParamAtTime(instance, "anamorphicAngle", time, 0.0));

    c.distortion = static_cast<float>(doubleParamAtTime(instance, "distortion", time, 0.0));
    c.breathing = static_cast<float>(doubleParamAtTime(instance, "breathing", time, 0.0));
    c.lateralCA = static_cast<float>(doubleParamAtTime(instance, "lateralCA", time, 0.0));
    c.axialCA = static_cast<float>(doubleParamAtTime(instance, "axialCA", time, 0.0));
    c.coma = static_cast<float>(doubleParamAtTime(instance, "coma", time, 0.0));
    c.astigmatism = static_cast<float>(doubleParamAtTime(instance, "astigmatism", time, 0.0));
    c.fieldCurvature = static_cast<float>(doubleParamAtTime(instance, "fieldCurvature", time, 0.0));
    c.spherical = static_cast<float>(doubleParamAtTime(instance, "spherical", time, 0.0));
    c.swirl = static_cast<float>(doubleParamAtTime(instance, "swirl", time, 0.0));

    c.depthSource = intParamAtTime(instance, "depthSource", time, 0);
    c.depthInvert = intParamAtTime(instance, "depthInvert", time, 0) != 0;
    c.depthNear = static_cast<float>(doubleParamAtTime(instance, "depthNear", time, 0.0));
    c.depthFar = static_cast<float>(doubleParamAtTime(instance, "depthFar", time, 1.0));
    c.depthGamma = static_cast<float>(doubleParamAtTime(instance, "depthGamma", time, 1.0));
    c.focusPlane = static_cast<float>(doubleParamAtTime(instance, "focusPlane", time, 0.5));
    c.focusOffset = static_cast<float>(doubleParamAtTime(instance, "focusOffset", time, 0.0));
    c.defocus = static_cast<float>(doubleParamAtTime(instance, "defocus", time, 0.0));
    c.catEye = static_cast<float>(doubleParamAtTime(instance, "catEye", time, 0.0));

    c.bloom = static_cast<float>(doubleParamAtTime(instance, "bloom", time, 0.0));
    c.bloomThreshold = static_cast<float>(doubleParamAtTime(instance, "bloomThreshold", time, 0.82));
    c.diffusion = static_cast<float>(doubleParamAtTime(instance, "diffusion", time, 0.0));
    c.halation = static_cast<float>(doubleParamAtTime(instance, "halation", time, 0.0));
    c.flareGhosts = static_cast<float>(doubleParamAtTime(instance, "flareGhosts", time, 0.0));
    c.flareStreak = static_cast<float>(doubleParamAtTime(instance, "flareStreak", time, 0.0));
    c.starburst = static_cast<float>(doubleParamAtTime(instance, "starburst", time, 0.0));

    c.vignette = static_cast<float>(doubleParamAtTime(instance, "vignette", time, 0.0));
    c.debayer = static_cast<float>(doubleParamAtTime(instance, "debayer", time, 0.0));
    c.chromaSmear = static_cast<float>(doubleParamAtTime(instance, "chromaSmear", time, 0.0));
    c.grain = static_cast<float>(doubleParamAtTime(instance, "grain", time, 0.0));
    c.grainSize = static_cast<float>(doubleParamAtTime(instance, "grainSize", time, 1.0));
    c.grainSeed = static_cast<float>(doubleParamAtTime(instance, "grainSeed", time, 1.0));
    c.sensorISO = static_cast<float>(doubleParamAtTime(instance, "sensorISO", time, 400.0));
    c.apertureInfluence = static_cast<float>(doubleParamAtTime(instance, "apertureInfluence", time, 0.85));
    c.dirtAmount = static_cast<float>(doubleParamAtTime(instance, "dirtAmount", time, 0.0));
    c.dirtScale = static_cast<float>(doubleParamAtTime(instance, "dirtScale", time, 1.0));
    c.smudgeAmount = static_cast<float>(doubleParamAtTime(instance, "smudgeAmount", time, 0.0));
    c.smudgeScale = static_cast<float>(doubleParamAtTime(instance, "smudgeScale", time, 1.0));

    c.edgeGuard = static_cast<float>(doubleParamAtTime(instance, "edgeGuard", time, 0.80));
    c.outputMix = static_cast<float>(doubleParamAtTime(instance, "outputMix", time, 0.65));

    c.geometryEnabled = intParamAtTime(instance, "geometryEnabled", time, 1) != 0;
    c.aberrationsEnabled = intParamAtTime(instance, "aberrationsEnabled", time, 1) != 0;
    c.defocusEnabled = intParamAtTime(instance, "defocusEnabled", time, 1) != 0;
    c.lightEnabled = intParamAtTime(instance, "lightEnabled", time, 1) != 0;
    c.vignetteEnabled = intParamAtTime(instance, "vignetteEnabled", time, 1) != 0;
    c.surfaceEnabled = intParamAtTime(instance, "surfaceEnabled", time, 1) != 0;
    c.sensorEnabled = intParamAtTime(instance, "sensorEnabled", time, 1) != 0;
    c.quality = intParamAtTime(instance, "quality", time, 1);
    c.sceneUnits = intParamAtTime(instance, "sceneUnits", time, 0);
    c.sceneScale = static_cast<float>(doubleParamAtTime(instance, "sceneScale", time, 1.0));
    c.irisBlades = intParamAtTime(instance, "irisBlades", time, 0);
    c.irisRoundnessTrim = static_cast<float>(doubleParamAtTime(instance, "irisRoundnessTrim", time, 0.0));
    c.irisRotation = static_cast<float>(doubleParamAtTime(instance, "irisRotation", time, 0.0));
    c.starUnevennessTrim = static_cast<float>(doubleParamAtTime(instance, "starUnevennessTrim", time, 0.0));
    c.starFStopResponse = static_cast<float>(doubleParamAtTime(instance, "starFStopResponse", time, 1.0));
    return c;
}

class FloatSampler final : public buckswood_optics::Sampler {
public:
    explicit FloatSampler(const ImageInfo& info)
        : info_(info)
        , base_(reinterpret_cast<OfxRGBAColourF*>(info.data))
    {
    }

    buckswood_optics::Pixel sample(float x, float y) const override
    {
        return bilinear(x, y);
    }

private:
    buckswood_optics::Pixel bilinear(float x, float y) const
    {
        if (!base_) {
            return buckswood_optics::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }
        const float safeX = std::min(
            static_cast<float>(info_.bounds.x2 - 1),
            std::max(static_cast<float>(info_.bounds.x1), x));
        const float safeY = std::min(
            static_cast<float>(info_.bounds.y2 - 1),
            std::max(static_cast<float>(info_.bounds.y1), y));
        const int x0 = static_cast<int>(std::floor(safeX));
        const int y0 = static_cast<int>(std::floor(safeY));
        const int x1 = std::min(info_.bounds.x2 - 1, x0 + 1);
        const int y1 = std::min(info_.bounds.y2 - 1, y0 + 1);
        const float tx = safeX - static_cast<float>(x0);
        const float ty = safeY - static_cast<float>(y0);
        const auto* row0 = reinterpret_cast<const OfxRGBAColourF*>(
            reinterpret_cast<const char*>(base_) +
            (y0 - info_.bounds.y1) * info_.rowBytes);
        const auto* row1 = reinterpret_cast<const OfxRGBAColourF*>(
            reinterpret_cast<const char*>(base_) +
            (y1 - info_.bounds.y1) * info_.rowBytes);
        const auto& p00 = row0[x0 - info_.bounds.x1];
        const auto& p10 = row0[x1 - info_.bounds.x1];
        const auto& p01 = row1[x0 - info_.bounds.x1];
        const auto& p11 = row1[x1 - info_.bounds.x1];
        const float u = 1.0f - tx;
        const float v = 1.0f - ty;
        return buckswood_optics::Pixel{
            (p00.r * u + p10.r * tx) * v + (p01.r * u + p11.r * tx) * ty,
            (p00.g * u + p10.g * tx) * v + (p01.g * u + p11.g * tx) * ty,
            (p00.b * u + p10.b * tx) * v + (p01.b * u + p11.b * tx) * ty,
            (p00.a * u + p10.a * tx) * v + (p01.a * u + p11.a * tx) * ty,
        };
    }

    const ImageInfo& info_;
    OfxRGBAColourF* base_;
};

class ByteSampler final : public buckswood_optics::Sampler {
public:
    explicit ByteSampler(const ImageInfo& info)
        : info_(info)
        , base_(reinterpret_cast<OfxRGBAColourB*>(info.data))
    {
    }

    buckswood_optics::Pixel sample(float x, float y) const override
    {
        const int ix = std::min(
            info_.bounds.x2 - 1,
            std::max(info_.bounds.x1, static_cast<int>(std::floor(x + 0.5f))));
        const int iy = std::min(
            info_.bounds.y2 - 1,
            std::max(info_.bounds.y1, static_cast<int>(std::floor(y + 0.5f))));
        if (!base_) {
            return buckswood_optics::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }
        const auto* row = reinterpret_cast<const OfxRGBAColourB*>(
            reinterpret_cast<const char*>(base_) +
            (iy - info_.bounds.y1) * info_.rowBytes);
        const auto* p = row + (ix - info_.bounds.x1);
        return p
            ? buckswood_optics::Pixel{
                p->r / 255.0f,
                p->g / 255.0f,
                p->b / 255.0f,
                p->a / 255.0f,
            }
            : buckswood_optics::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
    }

private:
    const ImageInfo& info_;
    OfxRGBAColourB* base_;
};

template <typename DstPixel, typename SamplerT>
OfxStatus renderTyped(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& srcInfo,
    const ImageInfo& dstInfo,
    const buckswood_optics::FrameInfo& frame,
    const buckswood_optics::Controls& controls,
    const buckswood_optics::AssetViews& assets)
{
    const SamplerT sampler(srcInfo);
    auto* dst = reinterpret_cast<DstPixel*>(dstInfo.data);
    const auto prepared = buckswood_optics::OpticsLabCore::prepare(frame, controls);

    auto renderRows = [&](int yBegin, int yEnd) {
        for (int y = yBegin; y < yEnd; ++y) {
            if (gEffectHost->abort(instance)) {
                break;
            }
            auto* dstPix = pixelAddress(dst, dstInfo.bounds, renderWindow.x1, y, dstInfo.rowBytes);
            if (!dstPix) {
                continue;
            }
            for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
                const auto out = buckswood_optics::OpticsLabCore::processPixel(
                    sampler,
                    x,
                    y,
                    prepared,
                    &assets);
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
    };

    return buckswood::ofx_runtime::parallelRows(
        gThreadHost,
        renderWindow.y1,
        renderWindow.y2,
        renderRows);
}

OfxStatus render(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs)
{
    OfxTime time = 0.0;
    OfxRectI renderWindow{0, 0, 0, 0};
    gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    gPropHost->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);
#if defined(__APPLE__)
    int metalEnabled = 0;
    void* metalCommandQueue = nullptr;
    gPropHost->propGetInt(
        inArgs,
        kOfxImageEffectPropMetalEnabled,
        0,
        &metalEnabled);
    if (metalEnabled) {
        gPropHost->propGetPointer(
            inArgs,
            kOfxImageEffectPropMetalCommandQueue,
            0,
            &metalCommandQueue);
    }
#endif

    OfxImageClipHandle outputClip = nullptr;
    OfxImageClipHandle sourceClip = nullptr;
    gEffectHost->clipGetHandle(instance, kOfxImageEffectOutputClipName, &outputClip, nullptr);
    gEffectHost->clipGetHandle(instance, kOfxImageEffectSimpleSourceClipName, &sourceClip, nullptr);

    OfxPropertySetHandle outputImage = nullptr;
    OfxPropertySetHandle sourceImage = nullptr;
    OfxStatus status = kOfxStatFailed;

    if (gEffectHost->clipGetImage(outputClip, time, nullptr, &outputImage) != kOfxStatOK) {
        return gEffectHost->abort(instance) ? kOfxStatOK : kOfxStatFailed;
    }
    if (gEffectHost->clipGetImage(sourceClip, time, nullptr, &sourceImage) != kOfxStatOK) {
        gEffectHost->clipReleaseImage(outputImage);
        return kOfxStatFailed;
    }

    ImageInfo dstInfo;
    ImageInfo srcInfo;
    if (!readImageInfo(outputImage, dstInfo) || !readImageInfo(sourceImage, srcInfo)) {
        gEffectHost->clipReleaseImage(sourceImage);
        gEffectHost->clipReleaseImage(outputImage);
        return kOfxStatFailed;
    }

    const auto controls = controlsAtTime(instance, time);
    std::string assetRoot = stringParamAtTime(instance, "glassAssetRoot", time, "");
    if (assetRoot.empty()) {
        assetRoot = buckswood_optics::OpticsAssetLibrary::defaultAssetRoot();
    }
    const int apertureIndex = intParamAtTime(instance, "apertureIndex", time, 0);
    const int dirtIndex = intParamAtTime(instance, "dirtIndex", time, 0);
    const int glassAsset = intParamAtTime(instance, "glassAsset", time, 0);
    const int dirtAsset = intParamAtTime(instance, "dirtAsset", time, 0);
    const int smudgeAsset = intParamAtTime(instance, "smudgeAsset", time, 0);
    const auto loadedAssets = buckswood_optics::OpticsAssetLibrary::load(
        assetRoot,
        apertureIndex,
        dirtIndex,
        glassAsset,
        dirtAsset,
        smudgeAsset);
    const auto assetViews = loadedAssets.views();
    const buckswood_optics::FrameInfo frame{
        dstInfo.bounds.x2 - dstInfo.bounds.x1,
        dstInfo.bounds.y2 - dstInfo.bounds.y1,
        static_cast<int>(std::floor(time + 0.5)),
    };

    if (
#if defined(__APPLE__)
        metalEnabled &&
#endif
        std::strcmp(dstInfo.pixelDepth, kOfxBitDepthFloat) == 0 &&
        std::strcmp(srcInfo.pixelDepth, kOfxBitDepthFloat) == 0) {
#if defined(__APPLE__)
        const auto prepared =
            buckswood_optics::OpticsLabCore::prepare(frame, controls);
        const bool queued = buckswood_optics::runOpticsLabMetal(
            metalCommandQueue,
            gpuImageBuffer(srcInfo),
            gpuImageBuffer(dstInfo),
            buckswood::gpu::RenderWindow{
                renderWindow.x1,
                renderWindow.y1,
                renderWindow.x2,
                renderWindow.y2,
            },
            prepared,
            assetViews);
        status = queued ? kOfxStatOK : kOfxStatFailed;
#endif
    } else if (std::strcmp(dstInfo.pixelDepth, kOfxBitDepthFloat) == 0 &&
        std::strcmp(srcInfo.pixelDepth, kOfxBitDepthFloat) == 0) {
        status = renderTyped<OfxRGBAColourF, FloatSampler>(
            instance,
            renderWindow,
            srcInfo,
            dstInfo,
            frame,
            controls,
            assetViews);
    } else if (std::strcmp(dstInfo.pixelDepth, kOfxBitDepthByte) == 0 &&
               std::strcmp(srcInfo.pixelDepth, kOfxBitDepthByte) == 0) {
        status = renderTyped<OfxRGBAColourB, ByteSampler>(
            instance,
            renderWindow,
            srcInfo,
            dstInfo,
            frame,
            controls,
            assetViews);
    } else {
        status = kOfxStatErrUnsupported;
    }

    gEffectHost->clipReleaseImage(sourceImage);
    gEffectHost->clipReleaseImage(outputImage);
    return status;
}

void placeParam(
    OfxPropertySetHandle props,
    const char* name,
    int pageIndex,
    OfxPropertySetHandle page,
    const char* parent,
    const char* hint)
{
    if (parent && *parent) {
        gPropHost->propSetString(props, kOfxParamPropParent, 0, parent);
    }
    if (hint && *hint) {
        gPropHost->propSetString(props, kOfxParamPropHint, 0, hint);
    }
    gPropHost->propSetString(page, kOfxParamPropPageChild, pageIndex, name);
}

void defineGroupParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    bool open)
{
    OfxPropertySetHandle props = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeGroup, name, &props);
    gPropHost->propSetString(props, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(props, kOfxPropLabel, 0, label);
    gPropHost->propSetInt(props, kOfxParamPropGroupOpen, 0, open ? 1 : 0);
}

void defineDoubleParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    double defaultValue,
    double minValue,
    double maxValue,
    int pageIndex,
    OfxPropertySetHandle page,
    const char* parent = nullptr,
    const char* hint = nullptr)
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
    placeParam(props, name, pageIndex, page, parent, hint);
}

void defineIntegerParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int defaultValue,
    int minValue,
    int maxValue,
    int pageIndex,
    OfxPropertySetHandle page,
    const char* parent = nullptr,
    const char* hint = nullptr,
    bool secret = false)
{
    OfxPropertySetHandle props = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeInteger, name, &props);
    gPropHost->propSetString(props, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(props, kOfxPropLabel, 0, label);
    gPropHost->propSetInt(props, kOfxParamPropDefault, 0, defaultValue);
    gPropHost->propSetInt(props, kOfxParamPropMin, 0, minValue);
    gPropHost->propSetInt(props, kOfxParamPropMax, 0, maxValue);
    gPropHost->propSetInt(props, kOfxParamPropDisplayMin, 0, minValue);
    gPropHost->propSetInt(props, kOfxParamPropDisplayMax, 0, maxValue);
    if (secret) {
        gPropHost->propSetInt(props, kOfxParamPropSecret, 0, 1);
    }
    placeParam(props, name, pageIndex, page, parent, hint);
}

void defineChoiceParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int defaultValue,
    const char* const* options,
    int optionCount,
    int pageIndex,
    OfxPropertySetHandle page,
    const char* parent = nullptr,
    const char* hint = nullptr)
{
    OfxPropertySetHandle props = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeChoice, name, &props);
    gPropHost->propSetString(props, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(props, kOfxPropLabel, 0, label);
    gPropHost->propSetInt(props, kOfxParamPropDefault, 0, defaultValue);
    for (int i = 0; i < optionCount; ++i) {
        gPropHost->propSetString(props, kOfxParamPropChoiceOption, i, options[i]);
    }
    placeParam(props, name, pageIndex, page, parent, hint);
}

void defineBooleanParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int defaultValue,
    int pageIndex,
    OfxPropertySetHandle page,
    const char* parent = nullptr,
    const char* hint = nullptr)
{
    OfxPropertySetHandle props = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeBoolean, name, &props);
    gPropHost->propSetString(props, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(props, kOfxPropLabel, 0, label);
    gPropHost->propSetInt(props, kOfxParamPropDefault, 0, defaultValue);
    placeParam(props, name, pageIndex, page, parent, hint);
}

void defineDirectoryParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    const char* defaultValue,
    int pageIndex,
    OfxPropertySetHandle page,
    bool secret = false)
{
    OfxPropertySetHandle props = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeString, name, &props);
    gPropHost->propSetString(props, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(props, kOfxPropLabel, 0, label);
    gPropHost->propSetString(props, kOfxParamPropStringMode, 0, kOfxParamStringIsDirectoryPath);
    gPropHost->propSetString(props, kOfxParamPropDefault, 0, defaultValue);
    if (secret) {
        gPropHost->propSetInt(props, kOfxParamPropSecret, 0, 1);
    }
    placeParam(props, name, pageIndex, page, nullptr, nullptr);
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
    gPropHost->propSetString(page, kOfxPropLabel, 0, "Optics Lab v1.3");

    int p = 0;
    constexpr const char* kLensGroup = "lensGroup";
    constexpr const char* kDistortionGroup = "distortionGroup";
    constexpr const char* kChromaticGroup = "chromaticGroup";
    constexpr const char* kGlassGroup = "glassGroup";
    constexpr const char* kLightGroup = "lightGroup";
    constexpr const char* kVignetteGroup = "vignetteGroup";
    constexpr const char* kSurfaceGroup = "surfaceGroup";
    constexpr const char* kSensorGroup = "sensorGroup";
    constexpr const char* kWorkflowGroup = "workflowGroup";
    defineGroupParam(paramSet, kLensGroup, "01  Lens State & Focus", true);
    defineGroupParam(paramSet, kDistortionGroup, "02  Distortion & Field", true);
    defineGroupParam(paramSet, kChromaticGroup, "03  Chromatic Aberration", true);
    defineGroupParam(paramSet, kGlassGroup, "04  Defocus & Bokeh / Glass", true);
    defineGroupParam(paramSet, kLightGroup, "05  Flaring & Bloom", true);
    defineGroupParam(paramSet, kVignetteGroup, "06  Vignetting", true);
    defineGroupParam(paramSet, kSurfaceGroup, "07  Dirt & Smudge", true);
    defineGroupParam(paramSet, kSensorGroup, "08  Sensor & Output", false);
    defineGroupParam(paramSet, kWorkflowGroup, "09  Workflow & Performance", false);

    const char* presets[] = {
        "Neutral / Manual",
        "Modern Cinema",
        "Warm Classic",
        "Fast Vintage",
        "Anamorphic Character",
        "Petzval Portrait",
        "AI Deplastic",
        "Large Format Clean",
        "Dream Diffusion",
        "Clean Modern",
        "Classic Spherical",
        "Vintage Swirl",
        "Soft Focus Portrait",
        "Anamorphic Classic 2x",
        "Anamorphic Blue 1.8x",
        "Vintage Flare",
        "Clinical APO",
        "Rangefinder Tele",
        "Retrofocus Wide",
        "Modern Zoom",
        "AI Natural Lens",
    };
    defineChoiceParam(
        paramSet, "preset", "Lens", 6, presets, 21, p++, page, kLensGroup,
        "Selects a coherent optical recipe. The controls below remain available as trims.");
    defineDoubleParam(
        paramSet, "effectStrength", "Lens Strength", 0.65, 0.0, 1.0,
        p++, page, kLensGroup);
    defineDoubleParam(
        paramSet, "focalLength", "Focal Length (mm)", 50.0, 8.0, 300.0,
        p++, page, kLensGroup);
    defineDoubleParam(
        paramSet, "fStop", "F-Stop", 2.8, 0.7, 32.0,
        p++, page, kLensGroup);
    defineDoubleParam(
        paramSet, "focusDistance", "Focus Distance", 3.0, 0.0002, 1000.0,
        p++, page, kLensGroup);
    defineDoubleParam(
        paramSet, "breathing", "Focus Breathing", 0.0, -1.0, 1.0,
        p++, page, kLensGroup);
    defineDoubleParam(
        paramSet, "sensorWidth", "Sensor Width (mm)", 36.0, 8.0, 70.0,
        p++, page, kLensGroup);
    defineDoubleParam(
        paramSet, "anamorphicSqueeze", "Anamorphic Squeeze", 1.0, 1.0, 2.0,
        p++, page, kLensGroup);
    defineDoubleParam(
        paramSet, "anamorphicAngle", "Anamorphic Axis (Degrees)", 0.0, -180.0, 180.0,
        p++, page, kLensGroup);

    const char* glassAssets[] = {
        "Off / Legacy Selection",
        "Clean Circular",
        "Six-Blade Hexagon",
        "Eight-Blade Octagon",
        "Anamorphic Oval 1.5x",
        "Anamorphic Oval 2.0x",
        "Cat-Eye Oval",
        "Vintage Scalloped",
    };
    defineChoiceParam(
        paramSet, "glassAsset", "Glass", 0, glassAssets, 8,
        p++, page, kGlassGroup,
        "Built-in aperture character. No external asset folder is required.");
    defineDoubleParam(
        paramSet, "apertureInfluence", "Glass Influence", 0.85, 0.0, 1.0,
        p++, page, kGlassGroup);

    const char* depthSources[] = {"Uniform Focus Offset", "Source Alpha as Depth"};
    defineChoiceParam(
        paramSet, "depthSource", "Depth Source", 0, depthSources, 2,
        p++, page, kGlassGroup);
    defineBooleanParam(
        paramSet, "depthInvert", "Invert Alpha Depth", 0,
        p++, page, kGlassGroup);
    defineDoubleParam(
        paramSet, "depthNear", "Alpha Depth Near", 0.0, 0.0, 1.0,
        p++, page, kGlassGroup);
    defineDoubleParam(
        paramSet, "depthFar", "Alpha Depth Far", 1.0, 0.0, 1.0,
        p++, page, kGlassGroup);
    defineDoubleParam(
        paramSet, "depthGamma", "Alpha Depth Gamma", 1.0, 0.10, 4.0,
        p++, page, kGlassGroup);
    defineDoubleParam(
        paramSet, "focusPlane", "Alpha Focus Plane", 0.5, 0.0, 1.0,
        p++, page, kGlassGroup);
    defineDoubleParam(
        paramSet, "focusOffset", "Uniform Focus Offset", 0.0, -1.0, 1.0,
        p++, page, kGlassGroup);
    defineDoubleParam(
        paramSet, "defocus", "Defocus", 0.0, 0.0, 1.0,
        p++, page, kGlassGroup);
    defineDoubleParam(
        paramSet, "catEye", "Cat-Eye Bokeh", 0.0, 0.0, 1.0,
        p++, page, kGlassGroup);

    defineDoubleParam(
        paramSet, "distortion", "Distortion Trim", 0.0, -1.0, 1.0,
        p++, page, kDistortionGroup);
    defineDoubleParam(
        paramSet, "fieldCurvature", "Field Curvature", 0.0, 0.0, 1.0,
        p++, page, kDistortionGroup);
    defineDoubleParam(
        paramSet, "swirl", "Swirl", 0.0, 0.0, 1.0,
        p++, page, kDistortionGroup);
    defineDoubleParam(
        paramSet, "lateralCA", "Lateral CA", 0.0, 0.0, 1.0,
        p++, page, kChromaticGroup);
    defineDoubleParam(
        paramSet, "axialCA", "Axial CA", 0.0, 0.0, 1.0,
        p++, page, kChromaticGroup);
    defineDoubleParam(
        paramSet, "coma", "Coma", 0.0, 0.0, 1.0,
        p++, page, kGlassGroup);
    defineDoubleParam(
        paramSet, "astigmatism", "Astigmatism", 0.0, 0.0, 1.0,
        p++, page, kGlassGroup);
    defineDoubleParam(
        paramSet, "spherical", "Spherical Aberration", 0.0, 0.0, 1.0,
        p++, page, kGlassGroup);

    defineDoubleParam(
        paramSet, "bloom", "Bloom", 0.0, 0.0, 1.0,
        p++, page, kLightGroup);
    defineDoubleParam(
        paramSet, "bloomThreshold", "Highlight Threshold", 0.82, 0.0, 4.0,
        p++, page, kLightGroup);
    defineDoubleParam(
        paramSet, "diffusion", "Diffusion", 0.0, 0.0, 1.0,
        p++, page, kLightGroup);
    defineDoubleParam(
        paramSet, "halation", "Halation", 0.0, 0.0, 1.0,
        p++, page, kLightGroup);
    defineDoubleParam(
        paramSet, "flareGhosts", "Flare Ghosts", 0.0, 0.0, 1.0,
        p++, page, kLightGroup);
    defineDoubleParam(
        paramSet, "flareStreak", "Anamorphic Streak", 0.0, 0.0, 1.0,
        p++, page, kLightGroup);
    defineDoubleParam(
        paramSet, "starburst", "Starburst", 0.0, 0.0, 1.0,
        p++, page, kLightGroup);

    const char* dirtAssets[] = {
        "Off / Legacy Selection",
        "Fine Dust",
        "Coarse Dust",
        "Clean-Room Flecks",
        "Edge Dust",
        "Organic Specks",
    };
    const char* smudgeAssets[] = {
        "Off",
        "Soft Fingerprint",
        "Dense Fingerprint",
        "Wipe Arc",
        "Streaked Glass",
    };
    defineChoiceParam(
        paramSet, "dirtAsset", "Dirt", 0, dirtAssets, 6,
        p++, page, kSurfaceGroup,
        "Built-in deterministic dirt texture; selecting Off preserves legacy projects.");
    defineDoubleParam(
        paramSet, "dirtAmount", "Dirt Amount", 0.0, 0.0, 1.0,
        p++, page, kSurfaceGroup);
    defineDoubleParam(
        paramSet, "dirtScale", "Dirt Scale", 1.0, 0.25, 8.0,
        p++, page, kSurfaceGroup);
    defineChoiceParam(
        paramSet, "smudgeAsset", "Smudge", 0, smudgeAssets, 5,
        p++, page, kSurfaceGroup,
        "Independent built-in fingerprint or wipe texture.");
    defineDoubleParam(
        paramSet, "smudgeAmount", "Smudge Amount", 0.0, 0.0, 1.0,
        p++, page, kSurfaceGroup);
    defineDoubleParam(
        paramSet, "smudgeScale", "Smudge Scale", 1.0, 0.25, 8.0,
        p++, page, kSurfaceGroup);

    defineDoubleParam(
        paramSet, "vignette", "Vignette", 0.0, 0.0, 1.0,
        p++, page, kVignetteGroup);
    defineDoubleParam(
        paramSet, "debayer", "Sensor Debayer Character", 0.0, 0.0, 1.0,
        p++, page, kSensorGroup);
    defineDoubleParam(
        paramSet, "chromaSmear", "Chroma Detail Smear", 0.0, 0.0, 1.0,
        p++, page, kSensorGroup);
    defineDoubleParam(
        paramSet, "grain", "Sensor Grain", 0.0, 0.0, 1.0,
        p++, page, kSensorGroup);
    defineDoubleParam(
        paramSet, "grainSize", "Grain Size", 1.0, 0.5, 4.0,
        p++, page, kSensorGroup);
    defineDoubleParam(
        paramSet, "grainSeed", "Grain Seed", 1.0, 0.0, 1000.0,
        p++, page, kSensorGroup);
    defineDoubleParam(
        paramSet, "sensorISO", "Sensor ISO", 400.0, 50.0, 12800.0,
        p++, page, kSensorGroup);

    defineDoubleParam(
        paramSet, "edgeGuard", "Edge Halo Guard", 0.80, 0.0, 1.0,
        p++, page, kSensorGroup);
    defineDoubleParam(
        paramSet, "outputMix", "Output Mix", 0.65, 0.0, 1.0,
        p++, page, kSensorGroup);

    const char* qualityOptions[] = {"Preview", "Full"};
    defineChoiceParam(
        paramSet, "quality", "Render Quality", 1, qualityOptions, 2,
        p++, page, kWorkflowGroup,
        "Preview reduces optical samples while preserving the same stage model. Full is the v1.2-compatible render path.");
    defineBooleanParam(
        paramSet, "geometryEnabled", "Enable Geometry", 1,
        p++, page, kWorkflowGroup);
    defineBooleanParam(
        paramSet, "aberrationsEnabled", "Enable Aberrations", 1,
        p++, page, kWorkflowGroup);
    defineBooleanParam(
        paramSet, "defocusEnabled", "Enable Defocus / Iris", 1,
        p++, page, kWorkflowGroup);
    defineBooleanParam(
        paramSet, "lightEnabled", "Enable Light Effects", 1,
        p++, page, kWorkflowGroup);
    defineBooleanParam(
        paramSet, "vignetteEnabled", "Enable Vignette", 1,
        p++, page, kWorkflowGroup);
    defineBooleanParam(
        paramSet, "surfaceEnabled", "Enable Dirt / Smudge", 1,
        p++, page, kWorkflowGroup);
    defineBooleanParam(
        paramSet, "sensorEnabled", "Enable Sensor", 1,
        p++, page, kWorkflowGroup,
        "Disabled stages are removed from the render path and cost no image-processing work.");

    const char* sceneUnits[] = {"Meters", "Centimeters", "Millimeters", "Feet", "Inches"};
    defineChoiceParam(
        paramSet, "sceneUnits", "Scene Units", 0, sceneUnits, 5,
        p++, page, kLensGroup);
    defineDoubleParam(
        paramSet, "sceneScale", "Scene Scale", 1.0, 0.001, 1000.0,
        p++, page, kLensGroup,
        "Converts focus distance to a physical meter scale without changing the shot layout.");
    defineIntegerParam(
        paramSet, "irisBlades", "Iris Blades (0 = Lens)", 0, 0, 16,
        p++, page, kGlassGroup,
        "Zero uses the selected lens recipe. Values from 3 to 16 override its shared procedural iris.");
    defineDoubleParam(
        paramSet, "irisRoundnessTrim", "Iris Roundness Trim", 0.0, -1.0, 1.0,
        p++, page, kGlassGroup);
    defineDoubleParam(
        paramSet, "irisRotation", "Iris Rotation", 0.0, -180.0, 180.0,
        p++, page, kGlassGroup);
    defineDoubleParam(
        paramSet, "starUnevennessTrim", "Star Spoke Unevenness", 0.0, -1.0, 1.0,
        p++, page, kLightGroup);
    defineDoubleParam(
        paramSet, "starFStopResponse", "Physical F-Stop Response", 1.0, 0.0, 1.0,
        p++, page, kLightGroup,
        "For v1.3 lenses, starbursts emerge near f/8 and reach full response near f/22.");

    // Kept serialized and readable for existing projects, but intentionally hidden
    // from the current UI. Choice value 0 above falls back to these parameters.
    defineDirectoryParam(
        paramSet,
        "glassAssetRoot",
        "Legacy Glass Asset Folder",
#if defined(_WIN32)
        "",
#else
        "~/Library/Application Support/Buckswood/OpticsLab/GlassAssets",
#endif
        p++,
        page,
        true);
    defineIntegerParam(
        paramSet, "apertureIndex", "Legacy Aperture Index", 0, 0, 157,
        p++, page, nullptr, nullptr, true);
    defineIntegerParam(
        paramSet, "dirtIndex", "Legacy Dirt Index", 0, 0, 8,
        p++, page, nullptr, nullptr, true);
    return kOfxStatOK;
}

OfxStatus describe(OfxImageEffectHandle effect)
{
    OfxPropertySetHandle props = nullptr;
    gEffectHost->getPropertySet(effect, &props);
    gPropHost->propSetInt(props, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 0);
    gPropHost->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gPropHost->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthByte);
    gPropHost->propSetString(props, kOfxPropLabel, 0, "Buckswood Optics Lab v1.3");
    gPropHost->propSetString(props, kOfxImageEffectPluginPropGrouping, 0, "Buckswood");
    gPropHost->propSetString(props, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
#if defined(__APPLE__)
    gPropHost->propSetString(
        props,
        kOfxImageEffectPropMetalRenderSupported,
        0,
        "true");
#endif
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
    return gEffectHost && gPropHost && gParamHost
        ? kOfxStatOK
        : kOfxStatErrMissingHostFeature;
}

OfxStatus pluginMain(
    const char* action,
    const void* handle,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle)
{
    try {
        const auto effect = reinterpret_cast<OfxImageEffectHandle>(const_cast<void*>(handle));
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
        if (std::strcmp(action, kOfxActionCreateInstance) == 0 ||
            std::strcmp(action, kOfxActionDestroyInstance) == 0) {
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

OfxPlugin gPlugin = {
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

EXPORT OfxPlugin* OfxGetPlugin(int index)
{
    return index == 0 ? &gPlugin : nullptr;
}

EXPORT int OfxGetNumberOfPlugins()
{
    return 1;
}

}
