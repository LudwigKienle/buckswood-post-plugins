#include "../../src/BuckswoodPremiereAdapter.h"

#include "PrSDKEffect.h"
#include "PrSDKPixelFormat.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

using buckswood_adobe::ImageViewF;
using buckswood_adobe::PixelF;

struct PhotorealizerSpec {
    float plasticReduction;
    float skinRealism;
    float highlightRealism;
    float colorNaturalize;
    float textureAmount;
    float microContrast;
    float gamutGuard;
    float shadowDepth;
    float smoothnessBreakup;
    float outputMix;
    float seed;
};

struct LensSpec {
    float lensPreset;
    float effectStrength;
    float distortion;
    float chromaticAberration;
    float fringing;
    float coma;
    float bloom;
    float bloomThreshold;
    float vignette;
    float cornerColor;
    float swirl;
    float fStopSharpener;
    float overdrive;
    float outputMix;
};

static_assert(sizeof(PhotorealizerSpec) == 44, "Premiere PiPL spec layout must stay packed");
static_assert(sizeof(LensSpec) == 56, "Premiere PiPL spec layout must stay packed");

constexpr bool kBuildPhotorealizer =
#if defined(BUCKSWOOD_PREMIERE_PHOTOREALIZER)
    true;
#else
    false;
#endif

constexpr bool kBuildLens =
#if defined(BUCKSWOOD_PREMIERE_LENS)
    true;
#else
    false;
#endif

static_assert(kBuildPhotorealizer != kBuildLens, "Define exactly one Buckswood Premiere effect mode");

template <typename T>
T* lockHandle(VideoRecord* video, PrMemoryHandle handle)
{
    if (!video || !handle || !video->piSuites || !video->piSuites->memFuncs) {
        return nullptr;
    }
    video->piSuites->memFuncs->lockHandle(handle);
    return reinterpret_cast<T*>(*handle);
}

void unlockHandle(VideoRecord* video, PrMemoryHandle handle)
{
    if (video && handle && video->piSuites && video->piSuites->memFuncs) {
        video->piSuites->memFuncs->unlockHandle(handle);
    }
}

csSDK_int32 handleSize(VideoRecord* video, PrMemoryHandle handle)
{
    if (!video || !handle || !video->piSuites || !video->piSuites->memFuncs) {
        return 0;
    }
    return video->piSuites->memFuncs->getHandleSize(handle);
}

PrMemoryHandle allocateSpec(VideoRecord* video)
{
    if (!video || !video->piSuites || !video->piSuites->memFuncs) {
        return nullptr;
    }

    const csSDK_uint32 size = kBuildPhotorealizer ? sizeof(PhotorealizerSpec) : sizeof(LensSpec);
    PrMemoryHandle handle = video->piSuites->memFuncs->newHandleClear(size);
    if (!handle) {
        return nullptr;
    }

    if constexpr (kBuildPhotorealizer) {
        auto* spec = lockHandle<PhotorealizerSpec>(video, handle);
        if (spec) {
            const auto defaults = buckswood_adobe::photorealizerDefaults();
            spec->plasticReduction = defaults.plasticReduction;
            spec->skinRealism = defaults.skinRealism;
            spec->highlightRealism = defaults.highlightRealism;
            spec->colorNaturalize = defaults.colorNaturalize;
            spec->textureAmount = defaults.textureAmount;
            spec->microContrast = defaults.microContrast;
            spec->gamutGuard = defaults.gamutGuard;
            spec->shadowDepth = defaults.shadowDepth;
            spec->smoothnessBreakup = defaults.smoothnessBreakup;
            spec->outputMix = defaults.outputMix;
            spec->seed = defaults.seed;
        }
    } else {
        auto* spec = lockHandle<LensSpec>(video, handle);
        if (spec) {
            const auto defaults = buckswood_adobe::realisticLensDefaults();
            spec->lensPreset = static_cast<float>(defaults.lensPreset);
            spec->effectStrength = defaults.effectStrength;
            spec->distortion = defaults.distortion;
            spec->chromaticAberration = defaults.chromaticAberration;
            spec->fringing = defaults.fringing;
            spec->coma = defaults.coma;
            spec->bloom = defaults.bloom;
            spec->bloomThreshold = defaults.bloomThreshold;
            spec->vignette = defaults.vignette;
            spec->cornerColor = defaults.cornerColor;
            spec->swirl = defaults.swirl;
            spec->fStopSharpener = defaults.fStopSharpener;
            spec->overdrive = defaults.overdrive;
            spec->outputMix = defaults.outputMix;
        }
    }

    unlockHandle(video, handle);
    return handle;
}

int widthOf(const prRect& rect)
{
    return std::max(0, static_cast<int>(rect.right) - static_cast<int>(rect.left));
}

int heightOf(const prRect& rect)
{
    return std::max(0, static_cast<int>(rect.bottom) - static_cast<int>(rect.top));
}

PixelF readBGRA32f(const float* pixel)
{
    return PixelF{
        pixel[2],
        pixel[1],
        pixel[0],
        pixel[3],
    };
}

void writeBGRA32f(float* pixel, PixelF value)
{
    pixel[0] = buckswood_adobe::clamp01(value.b);
    pixel[1] = buckswood_adobe::clamp01(value.g);
    pixel[2] = buckswood_adobe::clamp01(value.r);
    pixel[3] = buckswood_adobe::clamp01(value.a);
}

PRFILTERENTRY initSpec(VideoHandle data)
{
    if (!data || !*data) {
        return fsUnsupported;
    }
    VideoRecord* video = *data;
    if (!video->specsHandle) {
        video->specsHandle = allocateSpec(video);
    }
    return video->specsHandle ? fsNoErr : fsUnsupported;
}

PRFILTERENTRY getPixelFormat(VideoHandle data)
{
    if (!data || !*data) {
        return fsUnsupported;
    }
    VideoRecord* video = *data;
    if (video->pixelFormatIndex == 0) {
        video->pixelFormatSupported = PrPixelFormat_BGRA_4444_32f;
        return fsNoErr;
    }
    return fsBadFormatIndex;
}

PRFILTERENTRY executePhotorealizer(VideoRecord* video, char* srcBase, char* dstBase, int width, int height, int srcRowBytes, int dstRowBytes)
{
    auto controls = buckswood_adobe::photorealizerDefaults();
    if (video->specsHandle) {
        auto* spec = lockHandle<PhotorealizerSpec>(video, video->specsHandle);
        if (spec) {
            const csSDK_int32 size = handleSize(video, video->specsHandle);
            if (size >= static_cast<csSDK_int32>(offsetof(PhotorealizerSpec, plasticReduction) + sizeof(spec->plasticReduction))) {
                controls.plasticReduction = spec->plasticReduction;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(PhotorealizerSpec, skinRealism) + sizeof(spec->skinRealism))) {
                controls.skinRealism = spec->skinRealism;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(PhotorealizerSpec, highlightRealism) + sizeof(spec->highlightRealism))) {
                controls.highlightRealism = spec->highlightRealism;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(PhotorealizerSpec, colorNaturalize) + sizeof(spec->colorNaturalize))) {
                controls.colorNaturalize = spec->colorNaturalize;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(PhotorealizerSpec, textureAmount) + sizeof(spec->textureAmount))) {
                controls.textureAmount = spec->textureAmount;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(PhotorealizerSpec, microContrast) + sizeof(spec->microContrast))) {
                controls.microContrast = spec->microContrast;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(PhotorealizerSpec, gamutGuard) + sizeof(spec->gamutGuard))) {
                controls.gamutGuard = spec->gamutGuard;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(PhotorealizerSpec, shadowDepth) + sizeof(spec->shadowDepth))) {
                controls.shadowDepth = spec->shadowDepth;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(PhotorealizerSpec, smoothnessBreakup) + sizeof(spec->smoothnessBreakup))) {
                controls.smoothnessBreakup = spec->smoothnessBreakup;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(PhotorealizerSpec, outputMix) + sizeof(spec->outputMix))) {
                controls.outputMix = spec->outputMix;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(PhotorealizerSpec, seed) + sizeof(spec->seed))) {
                controls.seed = spec->seed;
            }
        }
        unlockHandle(video, video->specsHandle);
    }

    for (int y = 0; y < height; ++y) {
        const auto* src = reinterpret_cast<const float*>(srcBase + y * srcRowBytes);
        auto* dst = reinterpret_cast<float*>(dstBase + y * dstRowBytes);
        for (int x = 0; x < width; ++x) {
            const PixelF input = readBGRA32f(src + x * 4);
            const PixelF output = buckswood_adobe::processPhotorealizerPixel(input, x, y, width, height, controls);
            writeBGRA32f(dst + x * 4, output);
        }
    }

    return fsNoErr;
}

PRFILTERENTRY executeLens(VideoRecord* video, char* srcBase, char* dstBase, int width, int height, int srcRowBytes, int dstRowBytes)
{
    auto controls = buckswood_adobe::realisticLensDefaults();
    if (video->specsHandle) {
        auto* spec = lockHandle<LensSpec>(video, video->specsHandle);
        if (spec) {
            const csSDK_int32 size = handleSize(video, video->specsHandle);
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, lensPreset) + sizeof(spec->lensPreset))) {
                controls.lensPreset = std::clamp(static_cast<int>(std::lround(spec->lensPreset)), 0, 19);
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, effectStrength) + sizeof(spec->effectStrength))) {
                controls.effectStrength = spec->effectStrength;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, distortion) + sizeof(spec->distortion))) {
                controls.distortion = spec->distortion;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, chromaticAberration) + sizeof(spec->chromaticAberration))) {
                controls.chromaticAberration = spec->chromaticAberration;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, fringing) + sizeof(spec->fringing))) {
                controls.fringing = spec->fringing;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, coma) + sizeof(spec->coma))) {
                controls.coma = spec->coma;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, bloom) + sizeof(spec->bloom))) {
                controls.bloom = spec->bloom;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, bloomThreshold) + sizeof(spec->bloomThreshold))) {
                controls.bloomThreshold = spec->bloomThreshold;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, vignette) + sizeof(spec->vignette))) {
                controls.vignette = spec->vignette;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, cornerColor) + sizeof(spec->cornerColor))) {
                controls.cornerColor = spec->cornerColor;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, swirl) + sizeof(spec->swirl))) {
                controls.swirl = spec->swirl;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, fStopSharpener) + sizeof(spec->fStopSharpener))) {
                controls.fStopSharpener = spec->fStopSharpener;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, overdrive) + sizeof(spec->overdrive))) {
                controls.overdrive = spec->overdrive;
            }
            if (size >= static_cast<csSDK_int32>(offsetof(LensSpec, outputMix) + sizeof(spec->outputMix))) {
                controls.outputMix = spec->outputMix;
            }
        }
        unlockHandle(video, video->specsHandle);
    }

    std::vector<PixelF> source(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        const auto* src = reinterpret_cast<const float*>(srcBase + y * srcRowBytes);
        for (int x = 0; x < width; ++x) {
            source[static_cast<std::size_t>(y) * width + x] = readBGRA32f(src + x * 4);
        }
    }

    ImageViewF view{source.data(), width, height, width};
    for (int y = 0; y < height; ++y) {
        auto* dst = reinterpret_cast<float*>(dstBase + y * dstRowBytes);
        for (int x = 0; x < width; ++x) {
            const PixelF output = buckswood_adobe::processLensPixel(view, x, y, controls);
            writeBGRA32f(dst + x * 4, output);
        }
    }

    return fsNoErr;
}

PRFILTERENTRY execute(VideoHandle data)
{
    if (!data || !*data) {
        return fsUnsupported;
    }
    VideoRecord* video = *data;
    if (!video->source || !video->destination || !video->piSuites || !video->piSuites->ppixFuncs) {
        return fsUnsupported;
    }

    auto* ppix = video->piSuites->ppixFuncs;
    ppix->ppixLockPixels(video->source);
    ppix->ppixLockPixels(video->destination);

    prRect bounds{};
    ppix->ppixGetBounds(video->source, &bounds);
    const int width = widthOf(bounds);
    const int height = heightOf(bounds);
    char* srcBase = ppix->ppixGetPixels(video->source);
    char* dstBase = ppix->ppixGetPixels(video->destination);
    const int srcRowBytes = ppix->ppixGetRowbytes(video->source);
    const int dstRowBytes = ppix->ppixGetRowbytes(video->destination);

    PRFILTERENTRY result = fsUnsupported;
    if (width > 0 && height > 0 && srcBase && dstBase) {
        if constexpr (kBuildPhotorealizer) {
            result = executePhotorealizer(video, srcBase, dstBase, width, height, srcRowBytes, dstRowBytes);
        } else {
            result = executeLens(video, srcBase, dstBase, width, height, srcRowBytes, dstRowBytes);
        }
    }

    ppix->ppixUnlockPixels(video->destination);
    ppix->ppixUnlockPixels(video->source);
    return result;
}

} // namespace

extern "C" DllExport PRFILTERENTRY xFilter(short selector, VideoHandle theData)
{
    switch (selector) {
    case fsInitSpec:
        return initSpec(theData);
    case fsSetup:
        return initSpec(theData);
    case fsHasSetupDialog:
        return fsHasNoSetupDialog;
    case fsExecute:
        return execute(theData);
    case fsCanHandlePAR:
        return prEffectCanHandlePAR;
    case fsGetPixelFormatsSupported:
        return getPixelFormat(theData);
    case fsCacheOnLoad:
        return fsNoErr;
    case fsDisposeData:
        return fsNoErr;
    default:
        return fsUnsupported;
    }
}
