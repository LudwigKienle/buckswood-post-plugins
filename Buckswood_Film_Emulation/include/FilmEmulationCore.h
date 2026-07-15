#pragma once

#include <algorithm>
#include <cmath>

namespace buckswood_film {

struct Pixel {
    float r;
    float g;
    float b;
    float a;
};

struct FrameInfo {
    int width;
    int height;
    int frameIndex;
};

struct Controls {
    int inputSpace;
    int stockPreset;
    int printPreset;
    int processMode;
    int grainProfile;
    int halationProfile;
    int bloomProfile;
    int grainMode;
    int viewMode;
    float exposure;
    float pushPull;
    float density;
    float contrast;
    float developerContrast;
    float developerGamma;
    float colorSeparation;
    float colorBoost;
    float saturation;
    float temperature;
    float tint;
    float subtractiveColor;
    float interlayer;
    float neutralizeCurves;
    float bleachBypass;
    float filmCompression;
    float compressionWhitePoint;
    float compressionRange;
    float compressionColorDensity;
    float expandBlackPoint;
    float expandWhitePoint;
    float printerCyan;
    float printerMagenta;
    float printerYellow;
    float highlightRolloff;
    float blackLift;
    float halation;
    float halationLocal;
    float halationGlobal;
    float halationRedShift;
    float bloom;
    float bloomThreshold;
    float bloomSpread;
    float grain;
    float grainSize;
    float grainShadows;
    float grainMidtones;
    float grainHighlights;
    float grainChroma;
    float filmResolution;
    float gateWeave;
    float dust;
    float scratches;
    float flicker;
    float filmBreath;
    float temporalReconstruction;
    float temporalStability;
    float temporalMotionGuard;
    float skinProtect;
    float outputMix;
};

class Sampler {
public:
    virtual ~Sampler() = default;
    virtual Pixel sample(float x, float y) const = 0;
};

struct TemporalContext {
    const Sampler* previous2;
    const Sampler* previous;
    const Sampler* next;
    const Sampler* next2;
    bool hasPrevious2;
    bool hasPrevious;
    bool hasNext;
    bool hasNext2;
};

class FilmEmulationCore {
public:
    template <typename SamplerT>
    static Pixel processPixel(
        const SamplerT& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls);

    template <typename SamplerT>
    static Pixel processPixel(
        const SamplerT& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls,
        const TemporalContext* temporal);

private:
    struct StockModel {
        float contrast;
        float saturation;
        float density;
        float toe;
        float shoulder;
        float warmth;
        float greenBias;
        float blueBias;
        float subtractive;
        float halation;
        float grain;
        float silver;
        float monochrome;
        float pastel;
    };

    struct PrintModel {
        float contrast;
        float saturation;
        float toe;
        float shoulder;
        float warmHighlights;
        float coolShadows;
        float blackDensity;
        float silver;
    };

    static StockModel stockForPreset(int preset);
    static PrintModel printForPreset(int preset);

    static float clamp01(float value)
    {
        return std::min(1.0f, std::max(0.0f, value));
    }

    static float clamp(float value, float low, float high)
    {
        return std::min(high, std::max(low, value));
    }

    static float safePow(float value, float exponent)
    {
        return std::pow(std::max(0.0f, value), exponent);
    }

    static float safeLog2(float value)
    {
        return std::log2(std::max(0.000001f, value));
    }

    static float luma(Pixel pixel)
    {
        return 0.2627f * pixel.r + 0.6780f * pixel.g + 0.0593f * pixel.b;
    }

    static Pixel clampPixel(Pixel pixel)
    {
        return Pixel{
            clamp(pixel.r, 0.0f, 64.0f),
            clamp(pixel.g, 0.0f, 64.0f),
            clamp(pixel.b, 0.0f, 64.0f),
            clamp01(pixel.a),
        };
    }

    static Pixel mix(Pixel a, Pixel b, float t)
    {
        const float u = 1.0f - t;
        return Pixel{
            a.r * u + b.r * t,
            a.g * u + b.g * t,
            a.b * u + b.b * t,
            a.a * u + b.a * t,
        };
    }

    static Pixel add(Pixel a, Pixel b)
    {
        return Pixel{a.r + b.r, a.g + b.g, a.b + b.b, a.a};
    }

    static Pixel mul(Pixel pixel, float scalar)
    {
        return Pixel{pixel.r * scalar, pixel.g * scalar, pixel.b * scalar, pixel.a};
    }

    static float smoothstep(float edge0, float edge1, float x)
    {
        const float t = clamp01((x - edge0) / std::max(0.0001f, edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }

    static float hash(float x, float y, float seed);
    static Pixel decodeInput(Pixel pixel, int inputSpace);
    static Pixel encodeOutput(Pixel pixel, int inputSpace);
    static float decodeTransfer(float value, int inputSpace);
    static float encodeTransfer(float value, int inputSpace);
    static Pixel applyFilmCurve(Pixel pixel, float contrast, float toe, float shoulder, float pivot);
    static Pixel applySaturation(Pixel pixel, float amount);
    static Pixel applySubtractiveColor(Pixel pixel, float amount, float skinMask);
    static Pixel applyTemperatureTint(Pixel pixel, float temperature, float tint, float skinMask);
    static Pixel applyDeveloper(Pixel pixel, const Controls& controls, float skinMask);
    static Pixel applyFilmCompression(Pixel pixel, const Controls& controls);
    static Pixel applyExpand(Pixel pixel, const Controls& controls);
    static Pixel applyPrinterLights(Pixel pixel, const Controls& controls);
    static Pixel applyStock(Pixel pixel, const StockModel& stock, const Controls& controls, float skinMask);
    static Pixel applyPrint(Pixel pixel, const PrintModel& print, const Controls& controls);
    static float skinMask(Pixel pixel);
    static float grainValue(int x, int y, int frameIndex, float size, int channel, int mode);
    static float toneGrainMask(float value, const Controls& controls);

    template <typename SamplerT>
    static Pixel sampleDecoded(
        const SamplerT& sampler,
        float x,
        float y,
        int inputSpace);

    template <typename SamplerT>
    static Pixel opticalPass(
        const SamplerT& sampler,
        float x,
        float y,
        Pixel center,
        const Controls& controls,
        const StockModel& stock);

    static Pixel temporalReconstruct(
        Pixel scene,
        int x,
        int y,
        const Controls& controls,
        const TemporalContext* temporal);
};

template <typename SamplerT>
Pixel FilmEmulationCore::sampleDecoded(
    const SamplerT& sampler,
    float x,
    float y,
    int inputSpace)
{
    return decodeInput(sampler.sample(x, y), inputSpace);
}

template <typename SamplerT>
Pixel FilmEmulationCore::opticalPass(
    const SamplerT& sampler,
    float x,
    float y,
    Pixel center,
    const Controls& controls,
    const StockModel& stock)
{
    float haloProfileScale = 1.0f;
    float bloomProfileScale = 1.0f;
    float haloSpread = 1.0f;
    float bloomSpread = 1.0f + clamp01(controls.bloomSpread) * 1.2f;
    if (controls.halationProfile == 1) {
        haloProfileScale = 1.28f;
        haloSpread = 0.86f;
    } else if (controls.halationProfile == 2) {
        haloProfileScale = 1.12f;
        haloSpread = 0.98f;
    } else if (controls.halationProfile == 3) {
        haloProfileScale = 0.92f;
        haloSpread = 1.08f;
    } else if (controls.halationProfile == 4) {
        haloProfileScale = 0.58f;
        haloSpread = 1.28f;
    } else if (controls.halationProfile == 5) {
        haloProfileScale = 1.86f;
        haloSpread = 1.18f;
    }
    if (controls.bloomProfile == 1) {
        bloomProfileScale = 1.30f;
        bloomSpread *= 0.90f;
    } else if (controls.bloomProfile == 2) {
        bloomProfileScale = 1.10f;
    } else if (controls.bloomProfile == 3) {
        bloomProfileScale = 0.92f;
        bloomSpread *= 1.16f;
    } else if (controls.bloomProfile == 4) {
        bloomProfileScale = 0.60f;
        bloomSpread *= 1.36f;
    }

    const float haloAmount = clamp01(controls.halation + stock.halation * clamp01(controls.density)) * 0.42f * haloProfileScale;
    const float bloomAmount = clamp01(controls.bloom) * 0.34f * bloomProfileScale;
    if (haloAmount <= 0.0001f && bloomAmount <= 0.0001f) {
        return Pixel{0.0f, 0.0f, 0.0f, center.a};
    }

    const float centerY = luma(center);
    const float radius = (1.6f + 5.2f * clamp01(controls.halation + controls.bloom)) * std::max(0.25f, haloSpread * bloomSpread);
    const float offsets[8][2] = {
        {-1.0f, 0.0f},
        {1.0f, 0.0f},
        {0.0f, -1.0f},
        {0.0f, 1.0f},
        {-0.707f, -0.707f},
        {0.707f, -0.707f},
        {-0.707f, 0.707f},
        {0.707f, 0.707f},
    };

    Pixel glow{0.0f, 0.0f, 0.0f, center.a};
    float haloSum = 0.0f;
    float bloomSum = 0.0f;
    for (const auto& offset : offsets) {
        const Pixel sample = sampleDecoded(sampler, x + offset[0] * radius, y + offset[1] * radius, controls.inputSpace);
        const float sy = luma(sample);
        const float highlight = smoothstep(0.54f, 2.4f, sy);
        const float edgeEnergy =
            smoothstep(0.04f, 0.48f, sy - centerY) * clamp01(controls.halationLocal) +
            smoothstep(0.82f, 3.2f, sy) * clamp01(controls.halationGlobal);
        const float halo = highlight * edgeEnergy;
        const float bloom = smoothstep(std::max(0.05f, controls.bloomThreshold), 3.0f, sy);
        const float red = 0.72f + controls.halationRedShift * 0.78f;
        glow.r += sample.r * (halo * red + bloom * 0.64f);
        glow.g += sample.g * (halo * 0.42f + bloom * 0.58f);
        glow.b += sample.b * (halo * (0.30f - controls.halationRedShift * 0.18f) + bloom * 0.52f);
        haloSum += halo;
        bloomSum += bloom;
    }

    const float inv = 1.0f / 8.0f;
    glow.r *= inv;
    glow.g *= inv;
    glow.b *= inv;
    const float haloScale = haloAmount * clamp01(haloSum * inv * 1.6f);
    const float bloomScale = bloomAmount * clamp01(bloomSum * inv * 1.3f);
    return Pixel{
        glow.r * (haloScale + bloomScale),
        glow.g * (haloScale * 0.70f + bloomScale),
        glow.b * (haloScale * 0.40f + bloomScale),
        center.a,
    };
}

template <typename SamplerT>
Pixel FilmEmulationCore::processPixel(
    const SamplerT& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const Controls& controls)
{
    return processPixel(sampler, x, y, frame, controls, nullptr);
}

template <typename SamplerT>
Pixel FilmEmulationCore::processPixel(
    const SamplerT& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const Controls& controls,
    const TemporalContext* temporal)
{
    const float frameSeed = static_cast<float>(frame.frameIndex);
    const float weave = clamp01(controls.gateWeave);
    const float weaveX =
        std::sin(frameSeed * 0.417f + 1.7f) * 0.42f * weave +
        std::sin(frameSeed * 0.113f + 5.3f) * 0.28f * weave;
    const float weaveY =
        std::sin(frameSeed * 0.311f + 0.2f) * 0.28f * weave +
        std::sin(frameSeed * 0.071f + 4.1f) * 0.18f * weave;

    const float sx = static_cast<float>(x) + weaveX;
    const float sy = static_cast<float>(y) + weaveY;
    const Pixel dry = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    Pixel scene = sampleDecoded(sampler, sx, sy, controls.inputSpace);
    scene = clampPixel(scene);
    const Pixel preTemporal = scene;
    scene = temporalReconstruct(scene, x, y, controls, temporal);

    const StockModel stock = stockForPreset(controls.stockPreset);
    const PrintModel print = printForPreset(controls.printPreset);
    const float skin = skinMask(scene) * clamp01(controls.skinProtect);
    const bool colorStage = controls.processMode != 2;
    const bool textureStage = controls.processMode != 1;

    const float flickerNoise = hash(17.0f, frameSeed, 9.13f) - 0.5f;
    const float breathSlow = std::sin(frameSeed * 0.077f + 2.4f);
    const float breathFast = std::sin(frameSeed * 0.193f + 4.0f);
    const float breath = (breathSlow * 0.65f + breathFast * 0.35f) * clamp01(controls.filmBreath);
    const float flickerGain =
        1.0f +
        flickerNoise * 0.055f * clamp01(controls.flicker) +
        breath * 0.045f;
    const float exposureGain = std::pow(2.0f, clamp(controls.exposure + controls.pushPull * 0.72f, -4.0f, 4.0f)) * flickerGain;
    scene.r *= exposureGain;
    scene.g *= exposureGain;
    scene.b *= exposureGain;

    Pixel worked = scene;
    if (colorStage) {
        worked = applyDeveloper(worked, controls, skin);
        worked = applyStock(worked, stock, controls, skin);
        worked = applyFilmCompression(worked, controls);
    }
    const Pixel optics = textureStage
        ? opticalPass(sampler, sx, sy, scene, controls, stock)
        : Pixel{0.0f, 0.0f, 0.0f, scene.a};
    if (textureStage) {
        worked = add(worked, optics);
    }
    if (colorStage) {
        worked = applyPrinterLights(worked, controls);
        worked = applyPrint(worked, print, controls);
        worked = applyExpand(worked, controls);

        const float roll = clamp01(controls.highlightRolloff) * 1.35f;
        if (roll > 0.0001f) {
            worked.r = worked.r / (1.0f + worked.r * roll * 0.22f);
            worked.g = worked.g / (1.0f + worked.g * roll * 0.22f);
            worked.b = worked.b / (1.0f + worked.b * roll * 0.22f);
        }

        const float lift = controls.blackLift * 0.035f;
        if (std::fabs(lift) > 0.0001f) {
            const float shadowMask = 1.0f - smoothstep(0.03f, 0.24f, luma(worked));
            worked.r += lift * shadowMask;
            worked.g += lift * shadowMask;
            worked.b += lift * shadowMask;
        }
    }

    const float grainAmount = clamp01(controls.grain + stock.grain * 0.55f);
    const float yScene = luma(worked);
    const float grainMask = toneGrainMask(yScene, controls) *
        (0.55f + 0.45f * smoothstep(0.01f, 0.12f, yScene));
    if (textureStage && grainAmount > 0.0001f) {
        const float resolutionBlur = 1.0f + (1.0f - clamp01(controls.filmResolution)) * 0.32f;
        const float base = grainAmount * grainMask * 0.042f * resolutionBlur;
        const float mono = grainValue(x, y, frame.frameIndex, controls.grainSize, 0, controls.grainMode);
        const float r = grainValue(x, y, frame.frameIndex, controls.grainSize, 1, controls.grainMode);
        const float g = grainValue(x, y, frame.frameIndex, controls.grainSize, 2, controls.grainMode);
        const float b = grainValue(x, y, frame.frameIndex, controls.grainSize, 3, controls.grainMode);
        const float chroma = clamp01(controls.grainChroma);
        worked.r += base * (mono * (1.0f - chroma * 0.52f) + r * chroma * 0.52f);
        worked.g += base * (mono * (1.0f - chroma * 0.38f) + g * chroma * 0.38f);
        worked.b += base * (mono * (1.0f - chroma * 0.58f) + b * chroma * 0.58f);
    }

    const float dustAmount = clamp01(controls.dust);
    if (textureStage && dustAmount > 0.0001f) {
        const float staticDust = hash(std::floor(static_cast<float>(x) / 3.0f), std::floor(static_cast<float>(y) / 3.0f), 37.0f);
        const float movingDust = hash(std::floor(static_cast<float>(x) / 6.0f), frameSeed + std::floor(static_cast<float>(y) / 19.0f), 81.0f);
        const float whiteSpeck = smoothstep(0.9975f - dustAmount * 0.006f, 1.0f, staticDust) * 0.16f * dustAmount;
        const float blackSpeck = smoothstep(0.9970f - dustAmount * 0.006f, 1.0f, movingDust) * 0.11f * dustAmount;
        worked.r = worked.r * (1.0f - blackSpeck) + whiteSpeck;
        worked.g = worked.g * (1.0f - blackSpeck) + whiteSpeck;
        worked.b = worked.b * (1.0f - blackSpeck) + whiteSpeck;
    }

    const float scratches = clamp01(controls.scratches);
    if (textureStage && scratches > 0.0001f) {
        const float column = hash(std::floor(static_cast<float>(x) / 2.0f), std::floor(frameSeed / 3.0f), 301.0f);
        const float streak = smoothstep(0.9980f - scratches * 0.006f, 1.0f, column);
        const float broken = hash(std::floor(static_cast<float>(y) / 11.0f), frameSeed, 307.0f);
        const float line = streak * smoothstep(0.18f, 0.92f, broken) * scratches * 0.18f;
        worked.r += line;
        worked.g += line * 0.92f;
        worked.b += line * 0.76f;
    }

    if (controls.viewMode == 1) {
        const float gv = 0.5f + grainValue(x, y, frame.frameIndex, controls.grainSize, 0, controls.grainMode) * 0.5f;
        return Pixel{gv, gv, gv, dry.a};
    }
    if (controls.viewMode == 2) {
        const float oy = clamp01(luma(optics) * 5.0f);
        return Pixel{oy, oy * 0.35f, oy * 0.12f, dry.a};
    }
    if (controls.viewMode == 3) {
        const float dy = clamp01(luma(worked) / std::max(0.0001f, luma(scene) + 0.02f));
        return Pixel{dy, dy, dy, dry.a};
    }
    if (controls.viewMode == 4) {
        const float ty = clamp01(
            (std::fabs(scene.r - preTemporal.r) +
             std::fabs(scene.g - preTemporal.g) +
             std::fabs(scene.b - preTemporal.b)) * 2.4f);
        return Pixel{ty, ty * 0.72f, 1.0f - ty, dry.a};
    }

    Pixel encoded = encodeOutput(clampPixel(worked), controls.inputSpace);
    encoded.a = dry.a;
    return mix(dry, encoded, clamp01(controls.outputMix));
}

} // namespace buckswood_film
