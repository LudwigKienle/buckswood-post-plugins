#include <algorithm>
#include <cstring>
#include <exception>
#include <new>
#include <type_traits>

#include "FakeDiagnosticCore.h"
#include "OfxRenderRuntime.h"

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
OfxMultiThreadSuiteV1* gThreadHost = nullptr;

constexpr const char* kPluginIdentifier = "com.buckswood.fake.diagnostic";
constexpr int kPluginMajorVersion = 2;
constexpr int kPluginMinorVersion = 1;
constexpr const char* kSourceFrameRangeProp = "OfxImageClipPropFrameRange_Source";

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

buckswood_fake::Controls controlsAtTime(OfxImageEffectHandle instance, OfxTime time)
{
    auto c = buckswood_fake::FakeDiagnosticCore::defaultControls();
    c.sensitivity = static_cast<float>(doubleParamAtTime(instance, "sensitivity", time, c.sensitivity));
    c.overlayStrength = static_cast<float>(doubleParamAtTime(instance, "overlayStrength", time, c.overlayStrength));
    c.correctionStrength = static_cast<float>(doubleParamAtTime(instance, "correctionStrength", time, c.correctionStrength));
    c.plasticWeight = static_cast<float>(doubleParamAtTime(instance, "plasticWeight", time, c.plasticWeight));
    c.highlightWeight = static_cast<float>(doubleParamAtTime(instance, "highlightWeight", time, c.highlightWeight));
    c.edgeWeight = static_cast<float>(doubleParamAtTime(instance, "edgeWeight", time, c.edgeWeight));
    c.gradeWeight = static_cast<float>(doubleParamAtTime(instance, "gradeWeight", time, c.gradeWeight));
    c.textureWeight = static_cast<float>(doubleParamAtTime(instance, "textureWeight", time, c.textureWeight));
    c.temporalWeight = static_cast<float>(doubleParamAtTime(instance, "temporalWeight", time, c.temporalWeight));
    c.microContrast = static_cast<float>(doubleParamAtTime(instance, "microContrast", time, c.microContrast));
    c.edgeSoften = static_cast<float>(doubleParamAtTime(instance, "edgeSoften", time, c.edgeSoften));
    c.highlightRolloff = static_cast<float>(doubleParamAtTime(instance, "highlightRolloff", time, c.highlightRolloff));
    c.gamutGuard = static_cast<float>(doubleParamAtTime(instance, "gamutGuard", time, c.gamutGuard));
    c.sensorTexture = static_cast<float>(doubleParamAtTime(instance, "sensorTexture", time, c.sensorTexture));
    c.temporalAssist = static_cast<float>(doubleParamAtTime(instance, "temporalAssist", time, c.temporalAssist));
    c.seed = static_cast<float>(doubleParamAtTime(instance, "seed", time, c.seed));
    return c;
}

class FloatSampler final : public buckswood_fake::Sampler {
public:
    explicit FloatSampler(const ImageInfo& info)
        : info_(info)
        , base_(reinterpret_cast<OfxRGBAColourF*>(info.data))
    {
    }

    buckswood_fake::Pixel sample(float x, float y) const override
    {
        const float maxX = static_cast<float>(info_.bounds.x2 - 1);
        const float maxY = static_cast<float>(info_.bounds.y2 - 1);
        if (x < static_cast<float>(info_.bounds.x1) || x > maxX ||
            y < static_cast<float>(info_.bounds.y1) || y > maxY) {
            return buckswood_fake::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }
        const int ix = static_cast<int>(x + 0.5f);
        const int iy = static_cast<int>(y + 0.5f);
        auto* p = pixelAddress(base_, info_.bounds, ix, iy, info_.rowBytes);
        if (!p) {
            return buckswood_fake::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }
        return buckswood_fake::Pixel{p->r, p->g, p->b, p->a};
    }

private:
    const ImageInfo& info_;
    OfxRGBAColourF* base_;
};

class ByteSampler final : public buckswood_fake::Sampler {
public:
    explicit ByteSampler(const ImageInfo& info)
        : info_(info)
        , base_(reinterpret_cast<OfxRGBAColourB*>(info.data))
    {
    }

    buckswood_fake::Pixel sample(float x, float y) const override
    {
        const float maxX = static_cast<float>(info_.bounds.x2 - 1);
        const float maxY = static_cast<float>(info_.bounds.y2 - 1);
        if (x < static_cast<float>(info_.bounds.x1) || x > maxX ||
            y < static_cast<float>(info_.bounds.y1) || y > maxY) {
            return buckswood_fake::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }
        const int ix = static_cast<int>(x + 0.5f);
        const int iy = static_cast<int>(y + 0.5f);
        auto* p = pixelAddress(base_, info_.bounds, ix, iy, info_.rowBytes);
        if (!p) {
            return buckswood_fake::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }
        return buckswood_fake::Pixel{
            p->r / 255.0f,
            p->g / 255.0f,
            p->b / 255.0f,
            p->a / 255.0f,
        };
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
    const ImageInfo* previousInfo,
    const ImageInfo* nextInfo,
    const ImageInfo& dstInfo,
    const buckswood_fake::FrameInfo& frame,
    const buckswood_fake::Controls& controls,
    int viewMode)
{
    const SamplerT sampler(srcInfo);
    const SamplerT previousSampler(previousInfo ? *previousInfo : srcInfo);
    const SamplerT nextSampler(nextInfo ? *nextInfo : srcInfo);
    const buckswood_fake::TemporalContextT<SamplerT> temporal{
        previousInfo ? &previousSampler : nullptr,
        nextInfo ? &nextSampler : nullptr,
        previousInfo != nullptr,
        nextInfo != nullptr,
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
                    const buckswood_fake::Pixel out = buckswood_fake::FakeDiagnosticCore::processPixel(
                        sampler,
                        x,
                        y,
                        frame,
                        controls,
                        viewMode,
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

    return buckswood::ofx_runtime::parallelRows(
        gThreadHost,
        renderWindow.y1,
        renderWindow.y2,
        renderRows);
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
    OfxPropertySetHandle previousImage = nullptr;
    OfxPropertySetHandle nextImage = nullptr;
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
        const auto controls = controlsAtTime(instance, time);
        const int viewMode = intParamAtTime(instance, "viewMode", time, buckswood_fake::ViewDiagnosticOverlay);
        const int temporalAnalysis = intParamAtTime(instance, "temporalAnalysis", time, 0);
        const int temporalOffset = std::max(1, intParamAtTime(instance, "temporalOffset", time, 0) + 1);
        const int adjustmentLayerGuard = intParamAtTime(instance, "adjustmentLayerGuard", time, 1);

        ImageInfo srcInfo;
        if (!readImageInfo(sourceImage, srcInfo)) {
            return kOfxStatFailed;
        }

        if (adjustmentLayerGuard && sourceLooksLikeBlankAdjustmentByDepth(srcInfo)) {
            status = fillTransparentByDepth(instance, renderWindow, dstInfo);
        } else {
        ImageInfo previousInfo;
        ImageInfo nextInfo;
        ImageInfo* previousInfoPtr = nullptr;
        ImageInfo* nextInfoPtr = nullptr;
        if (temporalAnalysis) {
            if (gEffectHost->clipGetImage(sourceClip, time - temporalOffset, nullptr, &previousImage) == kOfxStatOK &&
                readImageInfo(previousImage, previousInfo) &&
                std::strcmp(previousInfo.pixelDepth, srcInfo.pixelDepth) == 0) {
                previousInfoPtr = &previousInfo;
            }
            if (gEffectHost->clipGetImage(sourceClip, time + temporalOffset, nullptr, &nextImage) == kOfxStatOK &&
                readImageInfo(nextImage, nextInfo) &&
                std::strcmp(nextInfo.pixelDepth, srcInfo.pixelDepth) == 0) {
                nextInfoPtr = &nextInfo;
            }
        }

        buckswood_fake::FrameInfo frame{
            dstInfo.bounds.x2 - dstInfo.bounds.x1,
            dstInfo.bounds.y2 - dstInfo.bounds.y1,
            static_cast<int>(time),
        };

        if (std::strcmp(dstInfo.pixelDepth, kOfxBitDepthFloat) == 0 &&
            std::strcmp(srcInfo.pixelDepth, kOfxBitDepthFloat) == 0) {
            status = renderTyped<OfxRGBAColourF, FloatSampler>(
                instance,
                renderWindow,
                srcInfo,
                previousInfoPtr,
                nextInfoPtr,
                dstInfo,
                frame,
                controls,
                viewMode);
        } else if (std::strcmp(dstInfo.pixelDepth, kOfxBitDepthByte) == 0 &&
                   std::strcmp(srcInfo.pixelDepth, kOfxBitDepthByte) == 0) {
            status = renderTyped<OfxRGBAColourB, ByteSampler>(
                instance,
                renderWindow,
                srcInfo,
                previousInfoPtr,
                nextInfoPtr,
                dstInfo,
                frame,
                controls,
                viewMode);
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
    if (previousImage) {
        gEffectHost->clipReleaseImage(previousImage);
    }
    if (nextImage) {
        gEffectHost->clipReleaseImage(nextImage);
    }
    if (outputImage) {
        gEffectHost->clipReleaseImage(outputImage);
    }
    return status;
}

OfxStatus getFramesNeeded(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs)
{
    OfxTime time = 0.0;
    gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    const int temporalAnalysis = intParamAtTime(instance, "temporalAnalysis", time, 0);
    if (!temporalAnalysis) {
        return kOfxStatReplyDefault;
    }

    const int temporalOffset = std::max(1, intParamAtTime(instance, "temporalOffset", time, 0) + 1);
    const double ranges[4] = {
        time - temporalOffset,
        time - temporalOffset,
        time + temporalOffset,
        time + temporalOffset,
    };
    gPropHost->propSetDoubleN(outArgs, kSourceFrameRangeProp, 4, ranges);
    return kOfxStatOK;
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
    gPropHost->propSetInt(props, kOfxImageEffectPropTemporalClipAccess, 0, 1);

    OfxParamSetHandle paramSet = nullptr;
    gEffectHost->getParamSet(effect, &paramSet);

    OfxPropertySetHandle page = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypePage, "Main", &page);
    gPropHost->propSetString(page, kOfxPropLabel, 0, "Fake Diagnostic");

    const char* viewModes[] = {
        "Diagnostic Overlay",
        "Problem Matte",
        "Reality Match Assist",
        "Assist + Overlay",
        "Category Colors",
        "Temporal Stability Matte",
        "Temporal Difference Overlay",
    };
    const char* temporalOffsets[] = {
        "1 Frame",
        "2 Frames",
        "3 Frames",
    };

    defineChoiceParam(paramSet, "viewMode", "View Mode", 0, viewModes, 7, 0, page);
    defineDoubleParam(paramSet, "sensitivity", "Sensitivity", 1.0, 0.10, 3.0, 1, page);
    defineDoubleParam(paramSet, "overlayStrength", "Overlay Strength", 0.65, 0.0, 1.0, 2, page);
    defineDoubleParam(paramSet, "correctionStrength", "Correction Strength", 0.35, 0.0, 1.0, 3, page);
    defineBooleanParam(paramSet, "temporalAnalysis", "Temporal Analysis", 0, 4, page);
    defineChoiceParam(paramSet, "temporalOffset", "Temporal Frame Offset", 0, temporalOffsets, 3, 5, page);
    defineDoubleParam(paramSet, "plasticWeight", "Plastic/Smooth Weight", 1.0, 0.0, 2.0, 6, page);
    defineDoubleParam(paramSet, "highlightWeight", "Highlight/Clip Weight", 1.0, 0.0, 2.0, 7, page);
    defineDoubleParam(paramSet, "edgeWeight", "Edge/Matte Weight", 0.85, 0.0, 2.0, 8, page);
    defineDoubleParam(paramSet, "gradeWeight", "Grade/Gamut Weight", 0.90, 0.0, 2.0, 9, page);
    defineDoubleParam(paramSet, "textureWeight", "Texture Weight", 0.85, 0.0, 2.0, 10, page);
    defineDoubleParam(paramSet, "temporalWeight", "Temporal Weight", 0.80, 0.0, 2.0, 11, page);
    defineDoubleParam(paramSet, "microContrast", "Micro Contrast Assist", 0.35, -1.0, 1.0, 12, page);
    defineDoubleParam(paramSet, "edgeSoften", "Edge Soften Assist", 0.15, 0.0, 1.0, 13, page);
    defineDoubleParam(paramSet, "highlightRolloff", "Highlight Rolloff Assist", 0.55, 0.0, 1.0, 14, page);
    defineDoubleParam(paramSet, "gamutGuard", "Gamut Guard Assist", 0.45, 0.0, 1.0, 15, page);
    defineDoubleParam(paramSet, "sensorTexture", "Sensor Texture Assist", 0.25, 0.0, 1.0, 16, page);
    defineDoubleParam(paramSet, "temporalAssist", "Temporal Smooth Assist", 0.25, 0.0, 1.0, 17, page);
    defineDoubleParam(paramSet, "seed", "Texture Seed", 17.0, 1.0, 1000.0, 18, page);
    defineBooleanParam(paramSet, "adjustmentLayerGuard", "Adjustment Layer Guard", 1, 19, page);

    return kOfxStatOK;
}

OfxStatus describe(OfxImageEffectHandle effect)
{
    OfxPropertySetHandle effectProps = nullptr;
    gEffectHost->getPropertySet(effect, &effectProps);
    gPropHost->propSetInt(effectProps, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 0);
    gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthByte);
    gPropHost->propSetInt(effectProps, kOfxImageEffectPropTemporalClipAccess, 0, 1);
    gPropHost->propSetString(effectProps, kOfxPropLabel, 0, "Buckswood Fake Diagnostic v2.1");
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
    gThreadHost = const_cast<OfxMultiThreadSuiteV1*>(
        reinterpret_cast<const OfxMultiThreadSuiteV1*>(gHost->fetchSuite(gHost->host, kOfxMultiThreadSuite, 1)));
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
