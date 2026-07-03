#include <algorithm>
#include <cstring>
#include <exception>
#include <new>
#include <type_traits>

#include "PhotorealizerCore.h"

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

constexpr const char* kPluginIdentifier = "com.buckswood.ai.photorealizer";
constexpr int kPluginMajorVersion = 0;
constexpr int kPluginMinorVersion = 2;

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

double paramAtTime(OfxImageEffectHandle instance, const char* name, OfxTime time, double fallback)
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

buckswood::Controls controlsAtTime(OfxImageEffectHandle instance, OfxTime time)
{
    buckswood::Controls c{};
    c.plasticReduction = static_cast<float>(paramAtTime(instance, "plasticReduction", time, 0.65));
    c.skinRealism = static_cast<float>(paramAtTime(instance, "skinRealism", time, 0.55));
    c.highlightRealism = static_cast<float>(paramAtTime(instance, "highlightRealism", time, 0.70));
    c.colorNaturalize = static_cast<float>(paramAtTime(instance, "colorNaturalize", time, 0.55));
    c.textureAmount = static_cast<float>(paramAtTime(instance, "textureAmount", time, 0.30));
    c.microContrast = static_cast<float>(paramAtTime(instance, "microContrast", time, 0.35));
    c.gamutGuard = static_cast<float>(paramAtTime(instance, "gamutGuard", time, 0.75));
    c.shadowDepth = static_cast<float>(paramAtTime(instance, "shadowDepth", time, 0.35));
    c.smoothnessBreakup = static_cast<float>(paramAtTime(instance, "smoothnessBreakup", time, 0.45));
    c.outputMix = static_cast<float>(paramAtTime(instance, "outputMix", time, 1.0));
    c.seed = static_cast<float>(paramAtTime(instance, "seed", time, 1.0));
    return c;
}

unsigned char toByte(float value)
{
    const float clamped = std::min(1.0f, std::max(0.0f, value));
    return static_cast<unsigned char>(clamped * 255.0f + 0.5f);
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
    const int grid = 9;
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

OfxStatus renderFloat(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& srcInfo,
    const ImageInfo& dstInfo,
    const buckswood::FrameInfo& frame,
    const buckswood::Controls& controls)
{
    auto* src = reinterpret_cast<OfxRGBAColourF*>(srcInfo.data);
    auto* dst = reinterpret_cast<OfxRGBAColourF*>(dstInfo.data);

    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        if (gEffectHost->abort(instance)) {
            break;
        }

        auto* dstPix = pixelAddress(dst, dstInfo.bounds, renderWindow.x1, y, dstInfo.rowBytes);
        for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
            auto* srcPix = pixelAddress(src, srcInfo.bounds, x, y, srcInfo.rowBytes);
            if (srcPix && dstPix) {
                buckswood::Pixel in{srcPix->r, srcPix->g, srcPix->b, srcPix->a};
                buckswood::Pixel out = buckswood::PhotorealizerCore::processPixel(in, x, y, frame, controls);
                dstPix->r = out.r;
                dstPix->g = out.g;
                dstPix->b = out.b;
                dstPix->a = out.a;
            } else if (dstPix) {
                *dstPix = OfxRGBAColourF{0.0f, 0.0f, 0.0f, 0.0f};
            }
            if (dstPix) {
                ++dstPix;
            }
        }
    }
    return kOfxStatOK;
}

OfxStatus renderByte(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& srcInfo,
    const ImageInfo& dstInfo,
    const buckswood::FrameInfo& frame,
    const buckswood::Controls& controls)
{
    auto* src = reinterpret_cast<OfxRGBAColourB*>(srcInfo.data);
    auto* dst = reinterpret_cast<OfxRGBAColourB*>(dstInfo.data);

    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        if (gEffectHost->abort(instance)) {
            break;
        }

        auto* dstPix = pixelAddress(dst, dstInfo.bounds, renderWindow.x1, y, dstInfo.rowBytes);
        for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
            auto* srcPix = pixelAddress(src, srcInfo.bounds, x, y, srcInfo.rowBytes);
            if (srcPix && dstPix) {
                buckswood::Pixel in{
                    srcPix->r / 255.0f,
                    srcPix->g / 255.0f,
                    srcPix->b / 255.0f,
                    srcPix->a / 255.0f,
                };
                buckswood::Pixel out = buckswood::PhotorealizerCore::processPixel(in, x, y, frame, controls);
                dstPix->r = toByte(out.r);
                dstPix->g = toByte(out.g);
                dstPix->b = toByte(out.b);
                dstPix->a = toByte(out.a);
            } else if (dstPix) {
                *dstPix = OfxRGBAColourB{0, 0, 0, 0};
            }
            if (dstPix) {
                ++dstPix;
            }
        }
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
                buckswood::FrameInfo frame{
                    dstInfo.bounds.x2 - dstInfo.bounds.x1,
                    dstInfo.bounds.y2 - dstInfo.bounds.y1,
                    static_cast<int>(time),
                };
                buckswood::Controls controls = controlsAtTime(instance, time);

                if (std::strcmp(dstInfo.pixelDepth, kOfxBitDepthFloat) == 0 &&
                    std::strcmp(srcInfo.pixelDepth, kOfxBitDepthFloat) == 0) {
                    status = renderFloat(instance, renderWindow, srcInfo, dstInfo, frame, controls);
                } else if (std::strcmp(dstInfo.pixelDepth, kOfxBitDepthByte) == 0 &&
                           std::strcmp(srcInfo.pixelDepth, kOfxBitDepthByte) == 0) {
                    status = renderByte(instance, renderWindow, srcInfo, dstInfo, frame, controls);
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
    gPropHost->propSetString(page, kOfxPropLabel, 0, "AI Photorealizer");

    defineDoubleParam(paramSet, "plasticReduction", "Plastic Reduction", 0.65, 0.0, 1.0, 0, page);
    defineDoubleParam(paramSet, "skinRealism", "Skin Realism", 0.55, 0.0, 1.0, 1, page);
    defineDoubleParam(paramSet, "highlightRealism", "Highlight Realism", 0.70, 0.0, 1.0, 2, page);
    defineDoubleParam(paramSet, "colorNaturalize", "Color Naturalize", 0.55, 0.0, 1.0, 3, page);
    defineDoubleParam(paramSet, "textureAmount", "Sensor Texture", 0.30, 0.0, 1.0, 4, page);
    defineDoubleParam(paramSet, "microContrast", "Micro Contrast", 0.35, 0.0, 1.0, 5, page);
    defineDoubleParam(paramSet, "gamutGuard", "Gamut Guard", 0.75, 0.0, 1.0, 6, page);
    defineDoubleParam(paramSet, "shadowDepth", "Shadow Depth", 0.35, 0.0, 1.0, 7, page);
    defineDoubleParam(paramSet, "smoothnessBreakup", "Smoothness Breakup", 0.45, 0.0, 1.0, 8, page);
    defineDoubleParam(paramSet, "outputMix", "Output Mix", 1.0, 0.0, 1.0, 9, page);
    defineDoubleParam(paramSet, "seed", "Texture Seed", 1.0, 1.0, 1000.0, 10, page);
    defineBooleanParam(paramSet, "adjustmentLayerGuard", "Adjustment Layer Guard", 1, 11, page);

    return kOfxStatOK;
}

OfxStatus describe(OfxImageEffectHandle effect)
{
    OfxPropertySetHandle effectProps = nullptr;
    gEffectHost->getPropertySet(effect, &effectProps);
    gPropHost->propSetInt(effectProps, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 0);
    gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthByte);
    gPropHost->propSetString(effectProps, kOfxPropLabel, 0, "Buckswood AI Photorealizer v0.2");
    gPropHost->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "Buckswood AI");
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

OfxStatus pluginMain(
    const char* action,
    const void* handle,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle outArgs)
{
    (void)outArgs;
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
