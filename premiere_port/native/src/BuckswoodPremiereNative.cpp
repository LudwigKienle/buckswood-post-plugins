#include "../../src/BuckswoodPremiereAdapter.h"

#include "CinematicToolsCore.h"
#include "FakeDiagnosticCore.h"
#include "FilmEmulationCore.h"
#include "LookDNACore.h"

#include "PrSDKEffect.h"
#include "PrSDKPixelFormat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

#if (defined(BUCKSWOOD_PREMIERE_PHOTOREALIZER) + defined(BUCKSWOOD_PREMIERE_LENS) + \
     defined(BUCKSWOOD_PREMIERE_FAKE_DIAGNOSTIC) + defined(BUCKSWOOD_PREMIERE_FILM_EMULATION) + \
     defined(BUCKSWOOD_PREMIERE_FRAME_DIRECTOR) + defined(BUCKSWOOD_PREMIERE_RADIANCE_RECOVER) + \
     defined(BUCKSWOOD_PREMIERE_TEMPORAL_INTEGRITY) + defined(BUCKSWOOD_PREMIERE_LOOK_DNA)) != 1
#error "Define exactly one Buckswood Premiere effect mode"
#endif

namespace {

using buckswood_adobe::ImageViewF;
using buckswood_adobe::PixelF;

#if defined(BUCKSWOOD_PREMIERE_PHOTOREALIZER)
constexpr std::size_t kParameterCount = 11;
constexpr std::array<float, kParameterCount> kDefaults{
    0.65f, 0.55f, 0.70f, 0.55f, 0.30f, 0.35f, 0.75f, 0.35f, 0.45f, 1.0f, 1.0f,
};
#elif defined(BUCKSWOOD_PREMIERE_LENS)
constexpr std::size_t kParameterCount = 14;
constexpr std::array<float, kParameterCount> kDefaults{
    19.0f, 0.45f, 0.0f, 0.015f, 0.015f, 0.035f, 0.075f,
    0.82f, 0.045f, 0.020f, 0.0f, 0.060f, 0.85f, 0.55f,
};
#elif defined(BUCKSWOOD_PREMIERE_FAKE_DIAGNOSTIC)
constexpr std::size_t kParameterCount = 15;
constexpr std::array<float, kParameterCount> kDefaults{
    0.0f, 1.0f, 0.65f, 0.35f, 1.0f, 1.0f, 0.85f, 0.90f,
    0.85f, 0.35f, 0.15f, 0.55f, 0.45f, 0.25f, 17.0f,
};
#elif defined(BUCKSWOOD_PREMIERE_FILM_EMULATION)
constexpr std::size_t kParameterCount = 27;
constexpr std::array<float, kParameterCount> kDefaults{
    1.0f, 8.0f, 6.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.18f, 1.0f,
    0.94f, 0.0f, 0.0f, 0.30f, 0.34f, 0.38f, 0.62f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.12f, 0.05f, 0.18f, 0.85f, 0.82f, 0.72f, 0.75f,
};
#elif defined(BUCKSWOOD_PREMIERE_FRAME_DIRECTOR)
constexpr std::size_t kParameterCount = 16;
constexpr std::array<float, kParameterCount> kDefaults{
    0.0f, 0.0f, 0.0f, 0.78f, 0.75f, 0.35f, 0.0f, 0.50f,
    0.42f, 0.0f, 0.0f, 0.38f, 0.78f, 1.0f, 0.006f, 1.0f,
};
#elif defined(BUCKSWOOD_PREMIERE_RADIANCE_RECOVER)
constexpr std::size_t kParameterCount = 11;
constexpr std::array<float, kParameterCount> kDefaults{
    0.0f, 0.62f, 0.35f, 0.72f, 0.28f, 2.0f, 0.28f, 0.35f, 0.22f, 0.80f, 1.0f,
};
#elif defined(BUCKSWOOD_PREMIERE_TEMPORAL_INTEGRITY)
constexpr std::size_t kParameterCount = 10;
constexpr std::array<float, kParameterCount> kDefaults{
    0.0f, 1.0f, 0.42f, 0.55f, 0.32f, 0.82f, 0.28f, 0.72f, 1.0f, 1.0f,
};
#elif defined(BUCKSWOOD_PREMIERE_LOOK_DNA)
constexpr std::size_t kParameterCount = 24;
constexpr std::array<float, kParameterCount> kDefaults{
    0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.35f, 0.20f,
    0.70f, 0.82f, 0.78f, 0.72f, 0.62f, 0.42f, 0.28f, 0.18f,
    0.45f, 0.35f, 0.72f, 0.74f, 0.70f, 0.86f, 0.50f, 1.0f,
};
#endif

struct ActiveSpec {
    float value[kParameterCount];
};

static_assert(sizeof(ActiveSpec) == sizeof(float) * kParameterCount, "Premiere PiPL spec must be packed floats");

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
    PrMemoryHandle handle = video->piSuites->memFuncs->newHandleClear(sizeof(ActiveSpec));
    if (!handle) {
        return nullptr;
    }
    auto* spec = lockHandle<ActiveSpec>(video, handle);
    if (spec) {
        std::copy(kDefaults.begin(), kDefaults.end(), spec->value);
    }
    unlockHandle(video, handle);
    return handle;
}

std::array<float, kParameterCount> parameterValues(VideoRecord* video)
{
    std::array<float, kParameterCount> values = kDefaults;
    if (!video || !video->specsHandle) {
        return values;
    }
    const auto size = std::max<csSDK_int32>(0, handleSize(video, video->specsHandle));
    auto* data = lockHandle<unsigned char>(video, video->specsHandle);
    if (data) {
        std::memcpy(values.data(), data, std::min<std::size_t>(static_cast<std::size_t>(size), sizeof(ActiveSpec)));
    }
    unlockHandle(video, video->specsHandle);
    return values;
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
    return PixelF{pixel[2], pixel[1], pixel[0], pixel[3]};
}

float finiteOrZero(float value)
{
    return std::isfinite(value) ? value : 0.0f;
}

void writeBGRA32f(float* pixel, PixelF value, bool preserveWideRange)
{
    if (preserveWideRange) {
        pixel[0] = std::clamp(finiteOrZero(value.b), -16.0f, 64.0f);
        pixel[1] = std::clamp(finiteOrZero(value.g), -16.0f, 64.0f);
        pixel[2] = std::clamp(finiteOrZero(value.r), -16.0f, 64.0f);
    } else {
        pixel[0] = buckswood_adobe::clamp01(finiteOrZero(value.b));
        pixel[1] = buckswood_adobe::clamp01(finiteOrZero(value.g));
        pixel[2] = buckswood_adobe::clamp01(finiteOrZero(value.r));
    }
    pixel[3] = buckswood_adobe::clamp01(finiteOrZero(value.a));
}

std::vector<PixelF> copySource(char* srcBase, int width, int height, int rowBytes)
{
    std::vector<PixelF> source(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        const auto* row = reinterpret_cast<const float*>(srcBase + y * rowBytes);
        for (int x = 0; x < width; ++x) {
            source[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                readBGRA32f(row + x * 4);
        }
    }
    return source;
}

PixelF sampleImage(ImageViewF view, float x, float y)
{
    if (!view.pixels || view.width <= 0 || view.height <= 0 || view.rowStridePixels <= 0) {
        return PixelF{0.0f, 0.0f, 0.0f, 1.0f};
    }
    const int ix = std::clamp(static_cast<int>(x + 0.5f), 0, view.width - 1);
    const int iy = std::clamp(static_cast<int>(y + 0.5f), 0, view.height - 1);
    return view.pixels[iy * view.rowStridePixels + ix];
}

class FakeSampler final {
public:
    explicit FakeSampler(ImageViewF view) : view_(view) {}
    buckswood_fake::Pixel sample(float x, float y) const
    {
        const PixelF p = sampleImage(view_, x, y);
        return {p.r, p.g, p.b, p.a};
    }
private:
    ImageViewF view_;
};

class FilmSampler final : public buckswood_film::Sampler {
public:
    explicit FilmSampler(ImageViewF view) : view_(view) {}
    buckswood_film::Pixel sample(float x, float y) const override
    {
        const PixelF p = sampleImage(view_, x, y);
        return {p.r, p.g, p.b, p.a};
    }
private:
    ImageViewF view_;
};

class CinematicSampler final : public buckswood_cinematic::Sampler {
public:
    explicit CinematicSampler(ImageViewF view) : view_(view) {}
    buckswood_cinematic::Pixel sample(float x, float y) const override
    {
        const PixelF p = sampleImage(view_, x, y);
        return {p.r, p.g, p.b, p.a};
    }
private:
    ImageViewF view_;
};

class LookSampler final : public buckswood_lookdna::Sampler {
public:
    explicit LookSampler(ImageViewF view) : view_(view) {}
    buckswood_lookdna::Pixel sample(float x, float y) const override
    {
        const PixelF p = sampleImage(view_, x, y);
        return {p.r, p.g, p.b, p.a};
    }
    buckswood_lookdna::Bounds bounds() const override
    {
        return {0, 0, view_.width, view_.height};
    }
private:
    ImageViewF view_;
};

int roundedChoice(float value, int maximum)
{
    return std::clamp(static_cast<int>(std::lround(value)), 0, maximum);
}

int frameIndex(VideoRecord* video)
{
    return video ? static_cast<int>(video->part) : 0;
}

PRFILTERENTRY renderPhotorealizer(
    VideoRecord* video,
    char* srcBase,
    char* dstBase,
    int width,
    int height,
    int srcRowBytes,
    int dstRowBytes)
{
    const auto p = parameterValues(video);
    auto controls = buckswood_adobe::photorealizerDefaults();
    controls.plasticReduction = p[0];
    controls.skinRealism = p[1];
    controls.highlightRealism = p[2];
    controls.colorNaturalize = p[3];
    controls.textureAmount = p[4];
    controls.microContrast = p[5];
    controls.gamutGuard = p[6];
    controls.shadowDepth = p[7];
    controls.smoothnessBreakup = p[8];
    controls.outputMix = p[9];
    controls.seed = p[10];
    for (int y = 0; y < height; ++y) {
        const auto* src = reinterpret_cast<const float*>(srcBase + y * srcRowBytes);
        auto* dst = reinterpret_cast<float*>(dstBase + y * dstRowBytes);
        for (int x = 0; x < width; ++x) {
            const PixelF out = buckswood_adobe::processPhotorealizerPixel(readBGRA32f(src + x * 4), x, y, width, height, controls);
            writeBGRA32f(dst + x * 4, out, false);
        }
    }
    return fsNoErr;
}

PRFILTERENTRY renderLens(
    VideoRecord* video,
    char* srcBase,
    char* dstBase,
    int width,
    int height,
    int srcRowBytes,
    int dstRowBytes)
{
    const auto p = parameterValues(video);
    auto controls = buckswood_adobe::realisticLensDefaults();
    controls.lensPreset = roundedChoice(p[0], 19);
    controls.effectStrength = p[1];
    controls.distortion = p[2];
    controls.chromaticAberration = p[3];
    controls.fringing = p[4];
    controls.coma = p[5];
    controls.bloom = p[6];
    controls.bloomThreshold = p[7];
    controls.vignette = p[8];
    controls.cornerColor = p[9];
    controls.swirl = p[10];
    controls.fStopSharpener = p[11];
    controls.overdrive = p[12];
    controls.outputMix = p[13];
    std::vector<PixelF> source = copySource(srcBase, width, height, srcRowBytes);
    const ImageViewF view{source.data(), width, height, width};
    for (int y = 0; y < height; ++y) {
        auto* dst = reinterpret_cast<float*>(dstBase + y * dstRowBytes);
        for (int x = 0; x < width; ++x) {
            writeBGRA32f(dst + x * 4, buckswood_adobe::processLensPixel(view, x, y, controls), false);
        }
    }
    return fsNoErr;
}

PRFILTERENTRY renderFakeDiagnostic(
    VideoRecord* video,
    char* srcBase,
    char* dstBase,
    int width,
    int height,
    int srcRowBytes,
    int dstRowBytes)
{
    const auto p = parameterValues(video);
    auto controls = buckswood_fake::FakeDiagnosticCore::defaultControls();
    controls.sensitivity = p[1];
    controls.overlayStrength = p[2];
    controls.correctionStrength = p[3];
    controls.plasticWeight = p[4];
    controls.highlightWeight = p[5];
    controls.edgeWeight = p[6];
    controls.gradeWeight = p[7];
    controls.textureWeight = p[8];
    controls.temporalWeight = 0.0f;
    controls.microContrast = p[9];
    controls.edgeSoften = p[10];
    controls.highlightRolloff = p[11];
    controls.gamutGuard = p[12];
    controls.sensorTexture = p[13];
    controls.temporalAssist = 0.0f;
    controls.seed = p[14];
    std::vector<PixelF> source = copySource(srcBase, width, height, srcRowBytes);
    const ImageViewF view{source.data(), width, height, width};
    const FakeSampler sampler(view);
    const buckswood_fake::FrameInfo frame{width, height, frameIndex(video)};
    const int viewMode = roundedChoice(p[0], 4);
    const auto prepared =
        buckswood_fake::FakeDiagnosticCore::prepare(frame, controls);
    for (int y = 0; y < height; ++y) {
        auto* dst = reinterpret_cast<float*>(dstBase + y * dstRowBytes);
        for (int x = 0; x < width; ++x) {
            const auto out = buckswood_fake::FakeDiagnosticCore::processPixel(
                sampler,
                x,
                y,
                frame,
                controls,
                prepared,
                viewMode);
            writeBGRA32f(dst + x * 4, PixelF{out.r, out.g, out.b, out.a}, false);
        }
    }
    return fsNoErr;
}

buckswood_film::Controls filmControls(const std::array<float, kParameterCount>& p)
{
    buckswood_film::Controls c{};
    c.inputSpace = roundedChoice(p[0], 7);
    c.stockPreset = roundedChoice(p[1], 15);
    c.printPreset = roundedChoice(p[2], 8);
    c.processMode = roundedChoice(p[3], 2);
    c.grainProfile = 3;
    c.halationProfile = 2;
    c.bloomProfile = 2;
    c.grainMode = 1;
    c.viewMode = roundedChoice(p[4], 4);
    c.exposure = p[5];
    c.pushPull = p[6];
    c.density = p[7];
    c.contrast = p[8];
    c.developerContrast = 0.10f;
    c.developerGamma = 0.0f;
    c.colorSeparation = p[12];
    c.colorBoost = 0.04f;
    c.saturation = p[9];
    c.temperature = p[10];
    c.tint = p[11];
    c.subtractiveColor = p[13];
    c.interlayer = 0.34f;
    c.neutralizeCurves = 0.0f;
    c.bleachBypass = 0.0f;
    c.filmCompression = p[14];
    c.compressionWhitePoint = 1.08f;
    c.compressionRange = 0.55f;
    c.compressionColorDensity = 0.48f;
    c.expandBlackPoint = 0.0f;
    c.expandWhitePoint = 0.0f;
    c.printerCyan = p[17];
    c.printerMagenta = p[18];
    c.printerYellow = p[19];
    c.highlightRolloff = p[15];
    c.blackLift = p[16];
    c.halation = p[20];
    c.halationLocal = 0.70f;
    c.halationGlobal = 0.18f;
    c.halationRedShift = 0.72f;
    c.bloom = p[21];
    c.bloomThreshold = 0.78f;
    c.bloomSpread = 0.42f;
    c.grain = p[22];
    c.grainSize = p[23];
    c.grainShadows = 0.70f;
    c.grainMidtones = 0.55f;
    c.grainHighlights = 0.42f;
    c.grainChroma = 0.26f;
    c.filmResolution = p[24];
    c.gateWeave = 0.04f;
    c.dust = 0.0f;
    c.scratches = 0.0f;
    c.flicker = 0.0f;
    c.filmBreath = 0.08f;
    c.temporalReconstruction = 0.0f;
    c.temporalStability = 0.0f;
    c.temporalMotionGuard = 1.0f;
    c.skinProtect = p[25];
    c.outputMix = p[26];
    return c;
}

PRFILTERENTRY renderFilmEmulation(
    VideoRecord* video,
    char* srcBase,
    char* dstBase,
    int width,
    int height,
    int srcRowBytes,
    int dstRowBytes)
{
    const auto controls = filmControls(parameterValues(video));
    std::vector<PixelF> source = copySource(srcBase, width, height, srcRowBytes);
    const ImageViewF view{source.data(), width, height, width};
    const FilmSampler sampler(view);
    const buckswood_film::FrameInfo frame{width, height, frameIndex(video)};
    const auto prepared =
        buckswood_film::FilmEmulationCore::prepare(frame, controls);
    for (int y = 0; y < height; ++y) {
        auto* dst = reinterpret_cast<float*>(dstBase + y * dstRowBytes);
        for (int x = 0; x < width; ++x) {
            const auto out = buckswood_film::FilmEmulationCore::processPixel(
                sampler,
                x,
                y,
                frame,
                controls,
                prepared,
                nullptr);
            writeBGRA32f(dst + x * 4, PixelF{out.r, out.g, out.b, out.a}, true);
        }
    }
    return fsNoErr;
}

PRFILTERENTRY renderFrameDirector(
    VideoRecord* video,
    char* srcBase,
    char* dstBase,
    int width,
    int height,
    int srcRowBytes,
    int dstRowBytes)
{
    const auto p = parameterValues(video);
    auto c = buckswood_cinematic::CinematicToolsCore::defaultFrameDirectorControls();
    c.targetAspect = roundedChoice(p[0], 6);
    c.viewMode = roundedChoice(p[1], 4);
    c.framingMode = roundedChoice(p[2], 4);
    c.autoStrength = p[3];
    c.subjectWeight = p[4];
    c.skinWeight = p[5];
    c.subjectLock = p[6] >= 0.5f;
    c.lockX = p[7];
    c.lockY = p[8];
    c.temporalSmoothing = 0.0f;
    c.manualX = p[9];
    c.manualY = p[10];
    c.subjectVerticalPosition = p[11];
    c.guideOpacity = p[12];
    c.matteOpacity = p[13];
    c.cropFeather = p[14];
    c.outputMix = p[15];
    std::vector<PixelF> source = copySource(srcBase, width, height, srcRowBytes);
    const ImageViewF view{source.data(), width, height, width};
    const CinematicSampler sampler(view);
    const buckswood_cinematic::FrameInfo frame{width, height, frameIndex(video)};
    auto focus = buckswood_cinematic::CinematicToolsCore::analyzeFocus(sampler, frame, c);
    focus = buckswood_cinematic::CinematicToolsCore::smoothFocus(focus, nullptr, nullptr, nullptr, nullptr, c);
    const auto crop = buckswood_cinematic::CinematicToolsCore::cropWindow(frame, c, focus);
    const auto prepared =
        buckswood_cinematic::CinematicToolsCore::prepareFrameDirector(
            frame,
            c,
            crop);
    for (int y = 0; y < height; ++y) {
        auto* dst = reinterpret_cast<float*>(dstBase + y * dstRowBytes);
        for (int x = 0; x < width; ++x) {
            const auto out =
                buckswood_cinematic::CinematicToolsCore::
                    processFrameDirector(
                        sampler,
                        x,
                        y,
                        frame,
                        c,
                        focus,
                        crop,
                        prepared);
            writeBGRA32f(dst + x * 4, PixelF{out.r, out.g, out.b, out.a}, true);
        }
    }
    return fsNoErr;
}

PRFILTERENTRY renderRadianceRecover(
    VideoRecord* video,
    char* srcBase,
    char* dstBase,
    int width,
    int height,
    int srcRowBytes,
    int dstRowBytes)
{
    const auto p = parameterValues(video);
    auto c = buckswood_cinematic::CinematicToolsCore::defaultRadianceControls();
    c.viewMode = roundedChoice(p[0], 3);
    c.highlightRecovery = p[1];
    c.specularRecovery = p[2];
    c.highlightRolloff = p[3];
    c.shadowRecovery = p[4];
    c.recoveredHeadroomStops = p[5];
    c.localDetail = p[6];
    c.shadowDenoise = p[7];
    c.dequantization = p[8];
    c.temporalConsistency = 0.0f;
    c.longTermConsistency = 0.0f;
    c.colorGuard = p[9];
    c.outputMix = p[10];
    std::vector<PixelF> source = copySource(srcBase, width, height, srcRowBytes);
    const ImageViewF view{source.data(), width, height, width};
    const CinematicSampler sampler(view);
    const buckswood_cinematic::FrameInfo frame{width, height, frameIndex(video)};
    const auto prepared =
        buckswood_cinematic::CinematicToolsCore::prepareRadiance(c);
    for (int y = 0; y < height; ++y) {
        auto* dst = reinterpret_cast<float*>(dstBase + y * dstRowBytes);
        for (int x = 0; x < width; ++x) {
            const auto out =
                buckswood_cinematic::CinematicToolsCore::
                    processRadiance(
                        sampler,
                        x,
                        y,
                        frame,
                        c,
                        prepared,
                        nullptr);
            writeBGRA32f(dst + x * 4, PixelF{out.r, out.g, out.b, out.a}, true);
        }
    }
    return fsNoErr;
}

float luma(PixelF p)
{
    return p.r * 0.2126f + p.g * 0.7152f + p.b * 0.0722f;
}

float smoothstep(float edge0, float edge1, float value)
{
    const float t = std::clamp((value - edge0) / std::max(0.000001f, edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

PixelF mixPixel(PixelF a, PixelF b, float amount)
{
    const float t = std::clamp(amount, 0.0f, 1.0f);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a};
}

PixelF spatialTemporalFallback(ImageViewF view, int x, int y, const std::array<float, kParameterCount>& p)
{
    const PixelF input = sampleImage(view, static_cast<float>(x), static_cast<float>(y));
    const float radius = std::clamp(p[9], 0.5f, 3.0f);
    const PixelF left = sampleImage(view, x - radius, y);
    const PixelF right = sampleImage(view, x + radius, y);
    const PixelF up = sampleImage(view, x, y - radius);
    const PixelF down = sampleImage(view, x, y + radius);
    const PixelF average{
        (left.r + right.r + up.r + down.r) * 0.25f,
        (left.g + right.g + up.g + down.g) * 0.25f,
        (left.b + right.b + up.b + down.b) * 0.25f,
        input.a,
    };
    const float difference = std::fabs(input.r - average.r) + std::fabs(input.g - average.g) + std::fabs(input.b - average.b);
    const float edge = std::fabs(luma(right) - luma(left)) + std::fabs(luma(down) - luma(up));
    const float sensitivity = std::max(0.1f, p[1]);
    const float outlier = smoothstep(0.018f / sensitivity, 0.22f / sensitivity, difference);
    const float edgeMask = smoothstep(0.025f, 0.30f, edge);
    const float edgeProtection = 1.0f - edgeMask * std::clamp(p[5], 0.0f, 1.0f);
    const float lumaMismatch = smoothstep(0.008f, 0.12f, std::fabs(luma(input) - luma(average)));
    const float chromaMismatch = smoothstep(
        0.015f,
        0.20f,
        std::fabs((input.r - luma(input)) - (average.r - luma(average))) +
            std::fabs((input.b - luma(input)) - (average.b - luma(average))));
    const float repairMask = std::clamp(
        outlier * edgeProtection * p[2] * (0.55f + p[3] * lumaMismatch + p[4] * 0.45f),
        0.0f,
        1.0f);
    PixelF repaired = mixPixel(input, average, repairMask * (0.45f + p[4] * 0.40f));
    repaired = mixPixel(repaired, average, repairMask * chromaMismatch * p[6] * 0.35f);
    const int viewMode = roundedChoice(p[0], 5);
    if (viewMode == 1) {
        return {repairMask, repairMask, repairMask, input.a};
    }
    if (viewMode == 2) {
        return mixPixel(input, PixelF{1.0f, 0.10f, 0.02f, input.a}, repairMask * p[7]);
    }
    if (viewMode == 3) {
        return {edgeMask, edgeMask, edgeMask, input.a};
    }
    if (viewMode == 4) {
        const float confidence = std::clamp((1.0f - outlier) * 0.6f + edgeProtection * 0.4f, 0.0f, 1.0f);
        return {confidence, confidence, confidence, input.a};
    }
    if (viewMode == 5) {
        return {chromaMismatch, chromaMismatch * 0.20f, 0.0f, input.a};
    }
    return mixPixel(input, repaired, p[8]);
}

PRFILTERENTRY renderTemporalIntegrity(
    VideoRecord* video,
    char* srcBase,
    char* dstBase,
    int width,
    int height,
    int srcRowBytes,
    int dstRowBytes)
{
    const auto p = parameterValues(video);
    std::vector<PixelF> source = copySource(srcBase, width, height, srcRowBytes);
    const ImageViewF view{source.data(), width, height, width};
    for (int y = 0; y < height; ++y) {
        auto* dst = reinterpret_cast<float*>(dstBase + y * dstRowBytes);
        for (int x = 0; x < width; ++x) {
            writeBGRA32f(dst + x * 4, spatialTemporalFallback(view, x, y, p), true);
        }
    }
    return fsNoErr;
}

buckswood_lookdna::LookProfile builtInLookProfile(int mode)
{
    using buckswood_lookdna::LookProfile;
    LookProfile profile;
    profile.valid = true;
    profile.sampleCount = 4096;
    profile.lumaQuantiles = {0.008f, 0.025f, 0.12f, 0.34f, 0.62f, 0.88f, 0.98f};
    profile.lumaMean = 0.39f;
    profile.lumaStd = 0.25f;
    profile.saturationMean = 0.28f;
    profile.localContrast = 0.060f;
    profile.grainEstimate = 0.010f;
    profile.chromaCovUU = 0.012f;
    profile.chromaCovUV = -0.002f;
    profile.chromaCovVV = 0.010f;
    if (mode == 1) {
        profile.lumaQuantiles = {0.006f, 0.020f, 0.105f, 0.32f, 0.60f, 0.87f, 0.97f};
        profile.chromaMeanU = 0.016f;
        profile.chromaMeanV = -0.014f;
        profile.saturationMean = 0.31f;
        profile.localContrast = 0.072f;
        profile.grainEstimate = 0.015f;
    } else if (mode == 2) {
        profile.lumaQuantiles = {0.004f, 0.016f, 0.09f, 0.29f, 0.57f, 0.84f, 0.96f};
        profile.chromaMeanU = -0.010f;
        profile.chromaMeanV = 0.016f;
        profile.saturationMean = 0.25f;
        profile.localContrast = 0.082f;
        profile.grainEstimate = 0.012f;
    } else {
        profile.lumaQuantiles = {0.010f, 0.032f, 0.14f, 0.37f, 0.65f, 0.90f, 0.985f};
        profile.chromaMeanU = 0.004f;
        profile.chromaMeanV = -0.003f;
        profile.saturationMean = 0.27f;
        profile.localContrast = 0.055f;
        profile.grainEstimate = 0.008f;
    }
    for (std::size_t index = 0; index < profile.zones.size(); ++index) {
        profile.zones[index].meanU = profile.chromaMeanU * (0.75f + static_cast<float>(index) * 0.05f);
        profile.zones[index].meanV = profile.chromaMeanV * (0.75f + static_cast<float>(index) * 0.05f);
        profile.zones[index].weight = 0.12f + static_cast<float>(index) * 0.015f;
    }
    return profile;
}

std::string lookProfilePath(int slot)
{
    const char* overrideDirectory = std::getenv("BUCKSWOOD_LOOKDNA_DIR");
#if defined(_WIN32)
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    const std::string base = overrideDirectory && *overrideDirectory
        ? overrideDirectory
        : (home && *home ? std::string(home) + "/Documents/Buckswood/LookDNA" : ".");
    const char suffix = static_cast<char>('a' + std::clamp(slot, 0, 2));
    return base + "/reference_" + suffix + ".bwlook";
}

std::array<buckswood_lookdna::LookProfile, 3> externalLookProfiles(int reloadToken)
{
    static std::mutex mutex;
    static int cachedToken = std::numeric_limits<int>::min();
    static std::array<buckswood_lookdna::LookProfile, 3> cached{};
    std::lock_guard<std::mutex> lock(mutex);
    if (reloadToken != cachedToken) {
        cached = {};
        for (int slot = 0; slot < 3; ++slot) {
            buckswood_lookdna::LookDNACore::loadProfile(lookProfilePath(slot), cached[slot], nullptr);
        }
        cachedToken = reloadToken;
    }
    return cached;
}

PRFILTERENTRY renderLookDNA(
    VideoRecord* video,
    char* srcBase,
    char* dstBase,
    int width,
    int height,
    int srcRowBytes,
    int dstRowBytes)
{
    const auto p = parameterValues(video);
    std::vector<PixelF> sourcePixels = copySource(srcBase, width, height, srcRowBytes);
    const ImageViewF view{sourcePixels.data(), width, height, width};
    const LookSampler sampler(view);
    buckswood_lookdna::Controls c;
    c.inputSpace = roundedChoice(p[2], 8);
    c.referenceSpace = buckswood_lookdna::SRGBDisplay;
    c.matchMode = roundedChoice(p[3], 3);
    c.analysisQuality = roundedChoice(p[4], 2);
    constexpr std::array<int, 8> premiereViews{
        buckswood_lookdna::ResultView,
        buckswood_lookdna::SplitView,
        buckswood_lookdna::DifferenceView,
        buckswood_lookdna::ConfidenceView,
        buckswood_lookdna::SkinMaskView,
        buckswood_lookdna::SemanticZonesView,
        buckswood_lookdna::ToneMapView,
        buckswood_lookdna::GamutWarningView,
    };
    c.viewMode = premiereViews[static_cast<std::size_t>(roundedChoice(p[5], 7))];
    c.temporalRadius = 0;
    c.referenceBMix = p[6];
    c.referenceCMix = p[7];
    c.referenceAdaptivity = p[8];
    c.matchStrength = p[9];
    c.toneMatch = p[10];
    c.paletteMatch = p[11];
    c.semanticMatch = p[12];
    c.localContrastMatch = p[13];
    c.textureMatch = p[14];
    c.grainMatch = p[15];
    c.densityMatch = p[16];
    c.exposureLock = p[17];
    c.skinProtect = p[18];
    c.highlightProtect = p[19];
    c.sceneIdentityGuard = p[20];
    c.temporalStability = 0.0f;
    c.spatialMatch = 0.0f;
    c.gamutGuard = p[21];
    c.splitPosition = p[22];
    c.outputMix = p[23];

    const auto sourceProfile = buckswood_lookdna::LookDNACore::analyze(sampler, c.inputSpace, c.analysisQuality);
    std::array<buckswood_lookdna::LookProfile, 3> referenceStorage{};
    const int referenceMode = roundedChoice(p[0], 3);
    if (referenceMode == 0) {
        referenceStorage = externalLookProfiles(static_cast<int>(std::lround(p[1])));
    } else {
        referenceStorage[0] = builtInLookProfile(referenceMode);
        referenceStorage[1] = referenceStorage[0];
        referenceStorage[2] = referenceStorage[0];
    }
    if (!referenceStorage[0].valid) {
        referenceStorage[0] = sourceProfile;
    }
    const std::array<const buckswood_lookdna::LookProfile*, 3> references{
        &referenceStorage[0],
        referenceStorage[1].valid ? &referenceStorage[1] : nullptr,
        referenceStorage[2].valid ? &referenceStorage[2] : nullptr,
    };
    const auto reference = buckswood_lookdna::LookDNACore::blendReferenceProfiles(
        sourceProfile,
        references,
        {1.0f, c.referenceBMix, c.referenceCMix},
        c.referenceAdaptivity,
        nullptr);
    const auto match = buckswood_lookdna::LookDNACore::buildMatch(sourceProfile, reference, c);
    const buckswood_lookdna::FrameInfo frame{width, height, frameIndex(video)};
    for (int y = 0; y < height; ++y) {
        auto* dst = reinterpret_cast<float*>(dstBase + y * dstRowBytes);
        for (int x = 0; x < width; ++x) {
            const auto out = buckswood_lookdna::LookDNACore::processPixel(sampler, x, y, frame, c, match, nullptr);
            writeBGRA32f(dst + x * 4, PixelF{out.r, out.g, out.b, out.a}, true);
        }
    }
    return fsNoErr;
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
#if defined(BUCKSWOOD_PREMIERE_PHOTOREALIZER)
        result = renderPhotorealizer(video, srcBase, dstBase, width, height, srcRowBytes, dstRowBytes);
#elif defined(BUCKSWOOD_PREMIERE_LENS)
        result = renderLens(video, srcBase, dstBase, width, height, srcRowBytes, dstRowBytes);
#elif defined(BUCKSWOOD_PREMIERE_FAKE_DIAGNOSTIC)
        result = renderFakeDiagnostic(video, srcBase, dstBase, width, height, srcRowBytes, dstRowBytes);
#elif defined(BUCKSWOOD_PREMIERE_FILM_EMULATION)
        result = renderFilmEmulation(video, srcBase, dstBase, width, height, srcRowBytes, dstRowBytes);
#elif defined(BUCKSWOOD_PREMIERE_FRAME_DIRECTOR)
        result = renderFrameDirector(video, srcBase, dstBase, width, height, srcRowBytes, dstRowBytes);
#elif defined(BUCKSWOOD_PREMIERE_RADIANCE_RECOVER)
        result = renderRadianceRecover(video, srcBase, dstBase, width, height, srcRowBytes, dstRowBytes);
#elif defined(BUCKSWOOD_PREMIERE_TEMPORAL_INTEGRITY)
        result = renderTemporalIntegrity(video, srcBase, dstBase, width, height, srcRowBytes, dstRowBytes);
#elif defined(BUCKSWOOD_PREMIERE_LOOK_DNA)
        result = renderLookDNA(video, srcBase, dstBase, width, height, srcRowBytes, dstRowBytes);
#endif
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
    case fsDisposeData:
        return fsNoErr;
    default:
        return fsUnsupported;
    }
}
