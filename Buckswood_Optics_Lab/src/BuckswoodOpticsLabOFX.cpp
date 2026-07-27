#include <algorithm>
#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <type_traits>

#include "OpticsAssetLibrary.h"
#include "OpticsLabCore.h"
#include "OfxRenderRuntime.h"

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
    c.apertureInfluence = static_cast<float>(doubleParamAtTime(instance, "apertureInfluence", time, 0.85));
    c.dirtAmount = static_cast<float>(doubleParamAtTime(instance, "dirtAmount", time, 0.0));
    c.dirtScale = static_cast<float>(doubleParamAtTime(instance, "dirtScale", time, 1.0));

    c.edgeGuard = static_cast<float>(doubleParamAtTime(instance, "edgeGuard", time, 0.80));
    c.outputMix = static_cast<float>(doubleParamAtTime(instance, "outputMix", time, 0.65));
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
    buckswood_optics::Pixel read(int x, int y) const
    {
        x = std::min(info_.bounds.x2 - 1, std::max(info_.bounds.x1, x));
        y = std::min(info_.bounds.y2 - 1, std::max(info_.bounds.y1, y));
        auto* p = pixelAddress(base_, info_.bounds, x, y, info_.rowBytes);
        return p
            ? buckswood_optics::Pixel{p->r, p->g, p->b, p->a}
            : buckswood_optics::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
    }

    buckswood_optics::Pixel bilinear(float x, float y) const
    {
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
        const auto p00 = read(x0, y0);
        const auto p10 = read(x1, y0);
        const auto p01 = read(x0, y1);
        const auto p11 = read(x1, y1);
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
        auto* p = pixelAddress(base_, info_.bounds, ix, iy, info_.rowBytes);
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
            for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
                if (!dstPix) {
                    continue;
                }
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
    const std::string assetRoot = stringParamAtTime(instance, "glassAssetRoot", time, "");
    const int apertureIndex = intParamAtTime(instance, "apertureIndex", time, 0);
    const int dirtIndex = intParamAtTime(instance, "dirtIndex", time, 0);
    const auto loadedAssets = buckswood_optics::OpticsAssetLibrary::load(
        assetRoot,
        apertureIndex,
        dirtIndex);
    const auto assetViews = loadedAssets.views();
    const buckswood_optics::FrameInfo frame{
        dstInfo.bounds.x2 - dstInfo.bounds.x1,
        dstInfo.bounds.y2 - dstInfo.bounds.y1,
        static_cast<int>(std::floor(time + 0.5)),
    };

    if (std::strcmp(dstInfo.pixelDepth, kOfxBitDepthFloat) == 0 &&
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

void defineIntegerParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int defaultValue,
    int minValue,
    int maxValue,
    int pageIndex,
    OfxPropertySetHandle page)
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

void defineDirectoryParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    const char* defaultValue,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle props = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeString, name, &props);
    gPropHost->propSetString(props, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(props, kOfxPropLabel, 0, label);
    gPropHost->propSetString(props, kOfxParamPropStringMode, 0, kOfxParamStringIsDirectoryPath);
    gPropHost->propSetString(props, kOfxParamPropDefault, 0, defaultValue);
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
    gPropHost->propSetString(page, kOfxPropLabel, 0, "Optics Lab");

    int p = 0;
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
    };
    defineChoiceParam(paramSet, "preset", "Optical Preset", 6, presets, 9, p++, page);
    defineDoubleParam(paramSet, "effectStrength", "Effect Strength", 0.65, 0.0, 1.0, p++, page);

    defineDoubleParam(paramSet, "focalLength", "Focal Length (mm)", 50.0, 8.0, 300.0, p++, page);
    defineDoubleParam(paramSet, "fStop", "F-Stop", 2.8, 0.7, 32.0, p++, page);
    defineDoubleParam(paramSet, "focusDistance", "Focus Distance (m)", 3.0, 0.2, 1000.0, p++, page);
    defineDoubleParam(paramSet, "sensorWidth", "Sensor Width (mm)", 36.0, 8.0, 70.0, p++, page);
    defineDoubleParam(paramSet, "anamorphicSqueeze", "Anamorphic Squeeze", 1.0, 1.0, 2.0, p++, page);

    defineDoubleParam(paramSet, "distortion", "Distortion Trim", 0.0, -1.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "breathing", "Focus Breathing", 0.0, -1.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "lateralCA", "Lateral CA", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "axialCA", "Axial CA", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "coma", "Coma", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "astigmatism", "Astigmatism", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "fieldCurvature", "Field Curvature", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "spherical", "Spherical Aberration", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "swirl", "Swirl", 0.0, 0.0, 1.0, p++, page);

    const char* depthSources[] = {"Uniform Focus Offset", "Source Alpha as Depth"};
    defineChoiceParam(paramSet, "depthSource", "Depth Source", 0, depthSources, 2, p++, page);
    defineDoubleParam(paramSet, "focusPlane", "Alpha Focus Plane", 0.5, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "focusOffset", "Uniform Focus Offset", 0.0, -1.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "defocus", "Defocus", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "catEye", "Cat-Eye Bokeh", 0.0, 0.0, 1.0, p++, page);

    defineDoubleParam(paramSet, "bloom", "Bloom", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "bloomThreshold", "Highlight Threshold", 0.82, 0.0, 4.0, p++, page);
    defineDoubleParam(paramSet, "diffusion", "Diffusion", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "halation", "Halation", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "flareGhosts", "Flare Ghosts", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "flareStreak", "Anamorphic Streak", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "starburst", "Starburst", 0.0, 0.0, 1.0, p++, page);

    defineDoubleParam(paramSet, "vignette", "Vignette", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "debayer", "Sensor Debayer Character", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "chromaSmear", "Chroma Detail Smear", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "grain", "Sensor Grain", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "grainSize", "Grain Size", 1.0, 0.5, 4.0, p++, page);
    defineDoubleParam(paramSet, "grainSeed", "Grain Seed", 1.0, 0.0, 1000.0, p++, page);

    defineDirectoryParam(
        paramSet,
        "glassAssetRoot",
        "Licensed Glass Asset Folder",
#if defined(_WIN32)
        "",
#else
        "~/Library/Application Support/Buckswood/OpticsLab/GlassAssets",
#endif
        p++,
        page);
    defineIntegerParam(paramSet, "apertureIndex", "Glass Aperture Index", 0, 0, 157, p++, page);
    defineDoubleParam(paramSet, "apertureInfluence", "Aperture Influence", 0.85, 0.0, 1.0, p++, page);
    defineIntegerParam(paramSet, "dirtIndex", "Glass Dirt Index", 0, 0, 8, p++, page);
    defineDoubleParam(paramSet, "dirtAmount", "Dirt Amount", 0.0, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "dirtScale", "Dirt Scale", 1.0, 0.25, 8.0, p++, page);

    defineDoubleParam(paramSet, "edgeGuard", "Edge Halo Guard", 0.80, 0.0, 1.0, p++, page);
    defineDoubleParam(paramSet, "outputMix", "Output Mix", 0.65, 0.0, 1.0, p++, page);
    return kOfxStatOK;
}

OfxStatus describe(OfxImageEffectHandle effect)
{
    OfxPropertySetHandle props = nullptr;
    gEffectHost->getPropertySet(effect, &props);
    gPropHost->propSetInt(props, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 0);
    gPropHost->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gPropHost->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthByte);
    gPropHost->propSetString(props, kOfxPropLabel, 0, "Buckswood Optics Lab v1.0");
    gPropHost->propSetString(props, kOfxImageEffectPluginPropGrouping, 0, "Buckswood");
    gPropHost->propSetString(props, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
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
