#include "DebandCore.h"
#include "OfxRenderRuntime.h"

#include "ofxImageEffect.h"
#include "ofxMultiThread.h"
#include "ofxParam.h"
#include "ofxPixels.h"

#include <algorithm>
#include <cstring>
#include <exception>
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

using buckswood_deband::Controls;
using buckswood_deband::DebandCore;
using buckswood_deband::FrameInfo;
using buckswood_deband::Pixel;

OfxHost* gHost = nullptr;
OfxImageEffectSuiteV1* gEffectHost = nullptr;
OfxPropertySuiteV1* gPropHost = nullptr;
OfxParameterSuiteV1* gParamHost = nullptr;
OfxMultiThreadSuiteV1* gThreadHost = nullptr;

constexpr const char* kPluginIdentifier = "com.buckswood.deband";
constexpr int kPluginMajorVersion = 1;
constexpr int kPluginMinorVersion = 0;

struct ImageInfo {
    void* data = nullptr;
    int rowBytes = 0;
    OfxRectI bounds{0, 0, 0, 0};
    char* pixelDepth = nullptr;
};

template <typename T>
T* pixelAddress(
    T* base,
    const OfxRectI& bounds,
    int x,
    int y,
    int rowBytes)
{
    if (!base ||
        x < bounds.x1 || x >= bounds.x2 ||
        y < bounds.y1 || y >= bounds.y2) {
        return nullptr;
    }
    char* row = reinterpret_cast<char*>(base) +
        (y - bounds.y1) * rowBytes;
    return reinterpret_cast<T*>(row) + (x - bounds.x1);
}

bool readImageInfo(OfxPropertySetHandle image, ImageInfo& info)
{
    return gPropHost->propGetPointer(
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
    return static_cast<unsigned char>(clamp01(value) * 255.0f + 0.5f);
}

template <typename NativePixel>
Pixel readNative(const NativePixel& pixel)
{
    if constexpr (std::is_same<NativePixel, OfxRGBAColourF>::value) {
        return Pixel{pixel.r, pixel.g, pixel.b, pixel.a};
    }
    return Pixel{
        pixel.r / 255.0f,
        pixel.g / 255.0f,
        pixel.b / 255.0f,
        pixel.a / 255.0f,
    };
}

void writeNative(OfxRGBAColourF* destination, const Pixel& pixel)
{
    destination->r = pixel.r;
    destination->g = pixel.g;
    destination->b = pixel.b;
    destination->a = pixel.a;
}

void writeNative(OfxRGBAColourB* destination, const Pixel& pixel)
{
    destination->r = toByte(pixel.r);
    destination->g = toByte(pixel.g);
    destination->b = toByte(pixel.b);
    destination->a = toByte(pixel.a);
}

template <typename NativePixel>
class ImageSampler {
public:
    explicit ImageSampler(const ImageInfo& image)
        : image_(image)
        , base_(reinterpret_cast<NativePixel*>(image.data))
    {
    }

    Pixel sample(int x, int y) const
    {
        const int clampedX = std::clamp(
            x,
            image_.bounds.x1,
            image_.bounds.x2 - 1);
        const int clampedY = std::clamp(
            y,
            image_.bounds.y1,
            image_.bounds.y2 - 1);
        const NativePixel* pixel = pixelAddress(
            base_,
            image_.bounds,
            clampedX,
            clampedY,
            image_.rowBytes);
        return pixel ? readNative(*pixel) : Pixel{};
    }

private:
    const ImageInfo& image_;
    NativePixel* base_ = nullptr;
};

double doubleParamAtTime(
    OfxImageEffectHandle instance,
    const char* name,
    OfxTime time,
    double fallback)
{
    OfxParamSetHandle parameterSet = nullptr;
    OfxParamHandle parameter = nullptr;
    if (gEffectHost->getParamSet(instance, &parameterSet) != kOfxStatOK ||
        gParamHost->paramGetHandle(
            parameterSet,
            name,
            &parameter,
            nullptr) != kOfxStatOK) {
        return fallback;
    }
    double value = fallback;
    return gParamHost->paramGetValueAtTime(
               parameter,
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
    OfxParamSetHandle parameterSet = nullptr;
    OfxParamHandle parameter = nullptr;
    if (gEffectHost->getParamSet(instance, &parameterSet) != kOfxStatOK ||
        gParamHost->paramGetHandle(
            parameterSet,
            name,
            &parameter,
            nullptr) != kOfxStatOK) {
        return fallback;
    }
    int value = fallback;
    return gParamHost->paramGetValueAtTime(
               parameter,
               time,
               &value) == kOfxStatOK
        ? value
        : fallback;
}

Controls controlsAtTime(
    OfxImageEffectHandle instance,
    OfxTime time)
{
    Controls controls{};
    controls.preset = intParamAtTime(
        instance,
        "preset",
        time,
        buckswood_deband::PresetBalanced);
    controls.workingSpace = intParamAtTime(
        instance,
        "workingSpace",
        time,
        buckswood_deband::WorkingAuto);
    controls.sourcePrecision = intParamAtTime(
        instance,
        "sourcePrecision",
        time,
        buckswood_deband::PrecisionAuto);
    controls.strength = static_cast<float>(doubleParamAtTime(
        instance,
        "strength",
        time,
        0.70));
    controls.detection = static_cast<float>(doubleParamAtTime(
        instance,
        "detection",
        time,
        0.55));
    controls.radius = static_cast<float>(doubleParamAtTime(
        instance,
        "radius",
        time,
        8.0));
    controls.edgeProtection = static_cast<float>(doubleParamAtTime(
        instance,
        "edgeProtection",
        time,
        0.85));
    controls.textureProtection = static_cast<float>(doubleParamAtTime(
        instance,
        "textureProtection",
        time,
        0.80));
    controls.chromaRepair = static_cast<float>(doubleParamAtTime(
        instance,
        "chromaRepair",
        time,
        0.35));
    controls.dither = static_cast<float>(doubleParamAtTime(
        instance,
        "dither",
        time,
        0.35));
    controls.ditherMotion = intParamAtTime(
        instance,
        "ditherMotion",
        time,
        buckswood_deband::DitherStatic);
    controls.seed = static_cast<float>(doubleParamAtTime(
        instance,
        "seed",
        time,
        1.0));
    controls.viewMode = intParamAtTime(
        instance,
        "viewMode",
        time,
        buckswood_deband::ViewResult);
    controls.outputMix = static_cast<float>(doubleParamAtTime(
        instance,
        "outputMix",
        time,
        1.0));
    return controls;
}

template <typename NativePixel>
OfxStatus renderTyped(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& source,
    const ImageInfo& destination,
    const DebandCore::PreparedState& state)
{
    ImageSampler<NativePixel> sampler(source);
    NativePixel* output = reinterpret_cast<NativePixel*>(destination.data);
    const auto renderRows = [&](int firstRow, int lastRow) {
        for (int y = firstRow; y < lastRow; ++y) {
            if (gEffectHost->abort(instance)) {
                break;
            }
            NativePixel* destinationPixel = pixelAddress(
                output,
                destination.bounds,
                renderWindow.x1,
                y,
                destination.rowBytes);
            for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
                if (destinationPixel) {
                    writeNative(
                        destinationPixel,
                        DebandCore::processPixel(
                            sampler,
                            x,
                            y,
                            state));
                    ++destinationPixel;
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

OfxStatus render(
    OfxImageEffectHandle instance,
    OfxPropertySetHandle inputArguments)
{
    OfxTime time = 0.0;
    OfxRectI renderWindow{0, 0, 0, 0};
    gPropHost->propGetDouble(
        inputArguments,
        kOfxPropTime,
        0,
        &time);
    gPropHost->propGetIntN(
        inputArguments,
        kOfxImageEffectPropRenderWindow,
        4,
        &renderWindow.x1);

    OfxImageClipHandle sourceClip = nullptr;
    OfxImageClipHandle outputClip = nullptr;
    gEffectHost->clipGetHandle(
        instance,
        kOfxImageEffectSimpleSourceClipName,
        &sourceClip,
        nullptr);
    gEffectHost->clipGetHandle(
        instance,
        kOfxImageEffectOutputClipName,
        &outputClip,
        nullptr);

    ScopedImage sourceImage;
    ScopedImage outputImage;
    if (!sourceImage.fetch(sourceClip, time) ||
        !outputImage.fetch(outputClip, time)) {
        return gEffectHost->abort(instance)
            ? kOfxStatOK
            : kOfxStatFailed;
    }

    ImageInfo sourceInfo;
    ImageInfo outputInfo;
    if (!readImageInfo(sourceImage.get(), sourceInfo) ||
        !readImageInfo(outputImage.get(), outputInfo)) {
        return kOfxStatFailed;
    }
    if (std::strcmp(sourceInfo.pixelDepth, outputInfo.pixelDepth) != 0) {
        return kOfxStatErrUnsupported;
    }

    const FrameInfo frame{
        sourceInfo.bounds.x2 - sourceInfo.bounds.x1,
        sourceInfo.bounds.y2 - sourceInfo.bounds.y1,
        static_cast<int>(std::floor(time + 0.5)),
    };
    const DebandCore::PreparedState state = DebandCore::prepare(
        frame,
        controlsAtTime(instance, time));

    if (std::strcmp(sourceInfo.pixelDepth, kOfxBitDepthFloat) == 0) {
        return renderTyped<OfxRGBAColourF>(
            instance,
            renderWindow,
            sourceInfo,
            outputInfo,
            state);
    }
    if (std::strcmp(sourceInfo.pixelDepth, kOfxBitDepthByte) == 0) {
        return renderTyped<OfxRGBAColourB>(
            instance,
            renderWindow,
            sourceInfo,
            outputInfo,
            state);
    }
    return kOfxStatErrUnsupported;
}

void defineDoubleParam(
    OfxParamSetHandle parameterSet,
    const char* name,
    const char* label,
    const char* hint,
    double defaultValue,
    double minimum,
    double maximum,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(
        parameterSet,
        kOfxParamTypeDouble,
        name,
        &properties);
    gPropHost->propSetString(
        properties,
        kOfxParamPropScriptName,
        0,
        name);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, label);
    gPropHost->propSetString(properties, kOfxParamPropHint, 0, hint);
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
    gPropHost->propSetDouble(properties, kOfxParamPropMin, 0, minimum);
    gPropHost->propSetDouble(properties, kOfxParamPropMax, 0, maximum);
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
    gPropHost->propSetString(
        page,
        kOfxParamPropPageChild,
        pageIndex,
        name);
}

void defineChoiceParam(
    OfxParamSetHandle parameterSet,
    const char* name,
    const char* label,
    const char* hint,
    int defaultValue,
    const char* const* options,
    int optionCount,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(
        parameterSet,
        kOfxParamTypeChoice,
        name,
        &properties);
    gPropHost->propSetString(
        properties,
        kOfxParamPropScriptName,
        0,
        name);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, label);
    gPropHost->propSetString(properties, kOfxParamPropHint, 0, hint);
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
    gPropHost->propSetString(
        page,
        kOfxParamPropPageChild,
        pageIndex,
        name);
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

    OfxParamSetHandle parameterSet = nullptr;
    gEffectHost->getParamSet(effect, &parameterSet);
    OfxPropertySetHandle page = nullptr;
    gParamHost->paramDefine(
        parameterSet,
        kOfxParamTypePage,
        "Main",
        &page);
    gPropHost->propSetString(
        page,
        kOfxPropLabel,
        0,
        "Buckswood Deband");

    const char* presets[] = {
        "Balanced",
        "Subtle 10-bit",
        "8-bit Sky Rescue",
        "AI Footage Gradient Repair",
        "Heavy Compression Rescue",
        "Manual",
    };
    const char* workingSpaces[] = {
        "Auto / Wide Gamut",
        "Display-Referred",
        "Log",
        "Scene-Linear HDR",
    };
    const char* precisions[] = {
        "Auto",
        "8-bit",
        "10-bit",
        "12-bit",
    };
    const char* ditherModes[] = {
        "Static / Baselight Safe",
        "Frame-Indexed",
    };
    const char* views[] = {
        "Result",
        "Banding Map",
        "Protected Detail",
        "Difference x12",
    };

    defineChoiceParam(
        parameterSet,
        "preset",
        "Preset",
        "Selects a tuned starting profile. All repair controls remain active as trims.",
        0,
        presets,
        6,
        0,
        page);
    defineChoiceParam(
        parameterSet,
        "workingSpace",
        "Working Space",
        "Choose the transfer behavior of the incoming pixels.",
        0,
        workingSpaces,
        4,
        1,
        page);
    defineChoiceParam(
        parameterSet,
        "sourcePrecision",
        "Source Precision",
        "Sets the expected quantization step used for detection and dither.",
        0,
        precisions,
        4,
        2,
        page);
    defineDoubleParam(
        parameterSet,
        "strength",
        "Repair Strength",
        "Amount of reconstructed gradient mixed into detected bands.",
        0.70,
        0.0,
        1.0,
        3,
        page);
    defineDoubleParam(
        parameterSet,
        "detection",
        "Band Detection",
        "Raises sensitivity to weak false contours.",
        0.55,
        0.0,
        1.0,
        4,
        page);
    defineDoubleParam(
        parameterSet,
        "radius",
        "Repair Radius",
        "Largest spatial scale sampled by the multi-scale reconstruction.",
        8.0,
        2.0,
        24.0,
        5,
        page);
    defineDoubleParam(
        parameterSet,
        "edgeProtection",
        "Edge Protection",
        "Protects silhouettes, text, and intentional graphic boundaries.",
        0.85,
        0.0,
        1.0,
        6,
        page);
    defineDoubleParam(
        parameterSet,
        "textureProtection",
        "Texture Protection",
        "Protects grain, pores, fabric, and fine detail.",
        0.80,
        0.0,
        1.0,
        7,
        page);
    defineDoubleParam(
        parameterSet,
        "chromaRepair",
        "Chroma Repair",
        "Extends repair from luminance into low-frequency color gradients.",
        0.35,
        0.0,
        1.0,
        8,
        page);
    defineDoubleParam(
        parameterSet,
        "dither",
        "Gradient Dither",
        "Adds deterministic high-frequency dither only inside detected gradients.",
        0.35,
        0.0,
        1.0,
        9,
        page);
    defineChoiceParam(
        parameterSet,
        "ditherMotion",
        "Dither Motion",
        "Static is identical in every frame. Frame-Indexed changes deterministically with time.",
        0,
        ditherModes,
        2,
        10,
        page);
    defineDoubleParam(
        parameterSet,
        "seed",
        "Dither Seed",
        "Changes the deterministic dither pattern.",
        1.0,
        1.0,
        1000.0,
        11,
        page);
    defineChoiceParam(
        parameterSet,
        "viewMode",
        "View",
        "Inspect the detector and protected image structure before rendering the result.",
        0,
        views,
        4,
        12,
        page);
    defineDoubleParam(
        parameterSet,
        "outputMix",
        "Output Mix",
        "Final dry/wet mix for the Result view.",
        1.0,
        0.0,
        1.0,
        13,
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
    gPropHost->propSetInt(
        properties,
        kOfxImageEffectPropSupportsTiles,
        0,
        0);
    gPropHost->propSetInt(
        properties,
        kOfxImageEffectPropTemporalClipAccess,
        0,
        0);
    gPropHost->propSetInt(
        properties,
        kOfxImageEffectInstancePropSequentialRender,
        0,
        0);
    gPropHost->propSetString(
        properties,
        kOfxImageEffectPluginRenderThreadSafety,
        0,
        kOfxImageEffectRenderFullySafe);
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
    gPropHost->propSetString(
        properties,
        kOfxPropLabel,
        0,
        "Buckswood Deband v1.0");
    gPropHost->propSetString(
        properties,
        kOfxImageEffectPluginPropGrouping,
        0,
        "Buckswood Restoration");
    gPropHost->propSetString(
        properties,
        kOfxImageEffectPropSupportedContexts,
        0,
        kOfxImageEffectContextFilter);
    return kOfxStatOK;
}

OfxStatus onLoad()
{
    if (!gHost) {
        return kOfxStatErrMissingHostFeature;
    }
    gEffectHost = const_cast<OfxImageEffectSuiteV1*>(
        reinterpret_cast<const OfxImageEffectSuiteV1*>(
            gHost->fetchSuite(
                gHost->host,
                kOfxImageEffectSuite,
                1)));
    gPropHost = const_cast<OfxPropertySuiteV1*>(
        reinterpret_cast<const OfxPropertySuiteV1*>(
            gHost->fetchSuite(
                gHost->host,
                kOfxPropertySuite,
                1)));
    gParamHost = const_cast<OfxParameterSuiteV1*>(
        reinterpret_cast<const OfxParameterSuiteV1*>(
            gHost->fetchSuite(
                gHost->host,
                kOfxParameterSuite,
                1)));
    gThreadHost = const_cast<OfxMultiThreadSuiteV1*>(
        reinterpret_cast<const OfxMultiThreadSuiteV1*>(
            gHost->fetchSuite(
                gHost->host,
                kOfxMultiThreadSuite,
                1)));
    return gEffectHost && gPropHost && gParamHost
        ? kOfxStatOK
        : kOfxStatErrMissingHostFeature;
}

OfxStatus pluginMain(
    const char* action,
    const void* handle,
    OfxPropertySetHandle inputArguments,
    OfxPropertySetHandle outputArguments)
{
    (void)outputArguments;
    try {
        OfxImageEffectHandle effect =
            reinterpret_cast<OfxImageEffectHandle>(
                const_cast<void*>(handle));
        if (std::strcmp(action, kOfxActionLoad) == 0) {
            return onLoad();
        }
        if (std::strcmp(action, kOfxActionDescribe) == 0) {
            return describe(effect);
        }
        if (std::strcmp(
                action,
                kOfxImageEffectActionDescribeInContext) == 0) {
            return describeInContext(effect);
        }
        if (std::strcmp(action, kOfxImageEffectActionRender) == 0) {
            return render(effect, inputArguments);
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

EXPORT OfxPlugin* OfxGetPlugin(int index)
{
    return index == 0 ? &plugin : nullptr;
}

EXPORT int OfxGetNumberOfPlugins()
{
    return 1;
}

} // extern "C"
