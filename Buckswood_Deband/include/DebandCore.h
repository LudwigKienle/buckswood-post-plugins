#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace buckswood_deband {

enum Preset {
    PresetBalanced = 0,
    PresetSubtle10Bit = 1,
    PresetSky8Bit = 2,
    PresetAIFootage = 3,
    PresetHeavyCompression = 4,
    PresetManual = 5,
};

enum WorkingSpace {
    WorkingAuto = 0,
    WorkingDisplayReferred = 1,
    WorkingLog = 2,
    WorkingSceneLinear = 3,
};

enum SourcePrecision {
    PrecisionAuto = 0,
    Precision8Bit = 1,
    Precision10Bit = 2,
    Precision12Bit = 3,
};

enum DitherMotion {
    DitherStatic = 0,
    DitherFrameIndexed = 1,
};

enum ViewMode {
    ViewResult = 0,
    ViewBandingMap = 1,
    ViewProtectedDetail = 2,
    ViewDifference = 3,
};

struct Pixel {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
};

struct FrameInfo {
    int width = 1;
    int height = 1;
    int frameIndex = 0;
};

struct Controls {
    int preset = PresetBalanced;
    int workingSpace = WorkingAuto;
    int sourcePrecision = PrecisionAuto;
    float strength = 0.70f;
    float detection = 0.55f;
    float radius = 8.0f;
    float edgeProtection = 0.85f;
    float textureProtection = 0.80f;
    float chromaRepair = 0.35f;
    float dither = 0.35f;
    int ditherMotion = DitherStatic;
    float seed = 1.0f;
    int viewMode = ViewResult;
    float outputMix = 1.0f;
};

struct PixelAnalysis {
    Pixel repaired{};
    float bandingMask = 0.0f;
    float protectedDetail = 0.0f;
    float difference = 0.0f;
};

class DebandCore {
public:
    struct PreparedState {
        Controls controls{};
        int frameIndex = 0;
        int radiusSmall = 2;
        int radiusMedium = 4;
        int radiusLarge = 8;
        float threshold = 0.014f;
        float noiseFloor = 0.0007f;
        float codeStep = 1.0f / 1023.0f;
    };

    static PreparedState prepare(
        const FrameInfo& frame,
        const Controls& controls);

    template <typename SamplerT>
    static PixelAnalysis analyzePixel(
        const SamplerT& sampler,
        int x,
        int y,
        const PreparedState& state)
    {
        const Pixel dry = sampler.sample(x, y);
        const Controls& controls = state.controls;

        if (controls.strength <= 0.000001f) {
            return PixelAnalysis{dry, 0.0f, 0.0f, 0.0f};
        }

        const float centerY = perceptualLuma(
            dry,
            controls.workingSpace);
        const Pixel left = sampler.sample(x - 1, y);
        const Pixel right = sampler.sample(x + 1, y);
        const Pixel up = sampler.sample(x, y - 1);
        const Pixel down = sampler.sample(x, y + 1);
        const float leftY = perceptualLuma(left, controls.workingSpace);
        const float rightY = perceptualLuma(right, controls.workingSpace);
        const float upY = perceptualLuma(up, controls.workingSpace);
        const float downY = perceptualLuma(down, controls.workingSpace);

        const float gradientX = std::fabs(rightY - leftY) * 0.5f;
        const float gradientY = std::fabs(downY - upY) * 0.5f;
        const float edgeMagnitude = std::sqrt(
            gradientX * gradientX + gradientY * gradientY);
        const float localRoughness = 0.25f * (
            std::fabs(leftY - centerY) +
            std::fabs(rightY - centerY) +
            std::fabs(upY - centerY) +
            std::fabs(downY - centerY));

        const std::array<int, 3> radii{
            state.radiusSmall,
            state.radiusMedium,
            state.radiusLarge,
        };
        const std::array<std::array<int, 2>, 4> directions{{
            {{1, 0}},
            {{0, 1}},
            {{1, 1}},
            {{1, -1}},
        }};

        Pixel candidate{};
        float candidateWeight = 0.0f;
        float contourEvidence = 0.0f;
        float farSpan = 0.0f;
        for (std::size_t scale = 0; scale < radii.size(); ++scale) {
            const int radius = radii[scale];
            const float scaleWeight = scale == 0 ? 0.75f : (scale == 1 ? 1.0f : 0.85f);
            for (const auto& direction : directions) {
                const Pixel negative = sampler.sample(
                    x - direction[0] * radius,
                    y - direction[1] * radius);
                const Pixel positive = sampler.sample(
                    x + direction[0] * radius,
                    y + direction[1] * radius);
                const float negativeY = perceptualLuma(
                    negative,
                    controls.workingSpace);
                const float positiveY = perceptualLuma(
                    positive,
                    controls.workingSpace);
                const float negativeDistance = std::fabs(negativeY - centerY);
                const float positiveDistance = std::fabs(positiveY - centerY);
                const float pairRange = std::max(
                    negativeDistance,
                    positiveDistance);
                const float pairSpan = std::fabs(positiveY - negativeY);
                const float midpointY = 0.5f * (negativeY + positiveY);
                const float residual = std::fabs(midpointY - centerY);
                const float rangeWeight = 1.0f - smoothstep(
                    state.threshold * 1.4f,
                    state.threshold * 4.5f,
                    pairRange);
                const float residualWeight = 0.20f + 0.80f * smoothstep(
                    state.noiseFloor * 0.20f,
                    state.threshold * 0.85f,
                    residual);
                const float weight = scaleWeight * rangeWeight * residualWeight;
                if (weight > 0.000001f) {
                    candidate = addWeighted(
                        candidate,
                        midpoint(negative, positive),
                        weight);
                    candidateWeight += weight;
                }
                const float transition = smoothstep(
                    state.noiseFloor * 0.35f,
                    state.threshold * 1.8f,
                    pairSpan);
                const float terrace = smoothstep(
                    state.noiseFloor * 0.20f,
                    state.threshold * 0.80f,
                    residual);
                contourEvidence = std::max(
                    contourEvidence,
                    transition * (0.30f + 0.70f * terrace) * rangeWeight);
                farSpan = std::max(farSpan, pairSpan * rangeWeight);
            }
        }

        if (candidateWeight <= 0.000001f) {
            return PixelAnalysis{dry, 0.0f, 1.0f, 0.0f};
        }
        candidate = multiply(candidate, 1.0f / candidateWeight);
        candidate.a = dry.a;

        const float candidateY = perceptualLuma(
            candidate,
            controls.workingSpace);
        const float reconstructionResidual = std::fabs(candidateY - centerY);
        const float residualScore = smoothstep(
            state.noiseFloor * 0.15f,
            state.threshold * 0.70f,
            reconstructionResidual);
        const float strongTransitionGuard = 1.0f - smoothstep(
            state.threshold * 1.4f,
            state.threshold * 4.0f,
            reconstructionResidual);

        const float edgeStart = state.threshold * (
            2.50f - 1.70f * controls.edgeProtection);
        const float edgeEnd = edgeStart * 3.5f;
        const float edgeGuard = 1.0f - smoothstep(
            edgeStart,
            edgeEnd,
            edgeMagnitude);
        const float textureStart = state.threshold * (
            1.55f - 1.20f * controls.textureProtection);
        const float textureEnd = textureStart * 3.25f;
        const float textureGuard = 1.0f - smoothstep(
            textureStart,
            textureEnd,
            localRoughness);
        const float intentionalFlatGuard = smoothstep(
            state.noiseFloor * 0.30f,
            state.threshold * 0.90f,
            farSpan);

        const float detectorGain = 0.45f + 1.55f * controls.detection;
        const float bandingMask = clamp01(
            detectorGain *
            (0.55f * residualScore + 0.45f * contourEvidence) *
            strongTransitionGuard *
            edgeGuard *
            textureGuard *
            intentionalFlatGuard);
        const float protectedDetail = clamp01(
            1.0f - edgeGuard * textureGuard);

        const float dryLinearY = linearLuma(dry);
        const float candidateLinearY = linearLuma(candidate);
        const float lumaDelta = candidateLinearY - dryLinearY;
        Pixel lumaOnly{
            dry.r + lumaDelta,
            dry.g + lumaDelta,
            dry.b + lumaDelta,
            dry.a,
        };
        Pixel repaired = mix(
            lumaOnly,
            candidate,
            controls.chromaRepair);
        repaired = mix(
            dry,
            repaired,
            controls.strength * bandingMask);

        const float gradientDitherMask = clamp01(
            intentionalFlatGuard * edgeGuard * textureGuard *
            (0.20f + 0.80f * std::max(bandingMask, contourEvidence)));
        if (controls.dither > 0.000001f && gradientDitherMask > 0.000001f) {
            const int noiseFrame = controls.ditherMotion == DitherFrameIndexed
                ? state.frameIndex
                : 0;
            const float noise = blueNoise(
                x,
                y,
                noiseFrame,
                controls.seed);
            const float ditherDelta = noise * state.codeStep *
                controls.dither * gradientDitherMask;
            repaired.r += ditherDelta;
            repaired.g += ditherDelta;
            repaired.b += ditherDelta;
        }
        repaired.a = dry.a;
        repaired = sanitize(repaired, dry);

        const float difference = std::max(
            std::fabs(repaired.r - dry.r),
            std::max(
                std::fabs(repaired.g - dry.g),
                std::fabs(repaired.b - dry.b)));
        return PixelAnalysis{
            repaired,
            bandingMask,
            protectedDetail,
            difference,
        };
    }

    template <typename SamplerT>
    static Pixel processPixel(
        const SamplerT& sampler,
        int x,
        int y,
        const PreparedState& state)
    {
        const Pixel dry = sampler.sample(x, y);
        const PixelAnalysis analysis = analyzePixel(
            sampler,
            x,
            y,
            state);
        Pixel output = analysis.repaired;
        switch (state.controls.viewMode) {
        case ViewBandingMap:
            output = heatMap(analysis.bandingMask, dry.a);
            break;
        case ViewProtectedDetail:
            output = Pixel{
                0.05f + 0.10f * analysis.protectedDetail,
                0.15f + 0.70f * analysis.protectedDetail,
                0.20f + 0.80f * analysis.protectedDetail,
                dry.a,
            };
            break;
        case ViewDifference: {
            const float gain = 12.0f;
            output = Pixel{
                0.5f + (analysis.repaired.r - dry.r) * gain,
                0.5f + (analysis.repaired.g - dry.g) * gain,
                0.5f + (analysis.repaired.b - dry.b) * gain,
                dry.a,
            };
            break;
        }
        case ViewResult:
        default:
            output = mix(
                dry,
                analysis.repaired,
                state.controls.outputMix);
            break;
        }
        output.a = dry.a;
        return sanitize(output, dry);
    }

private:
    static float clamp01(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    static float smoothstep(float edge0, float edge1, float value)
    {
        const float denominator = std::max(0.0000001f, edge1 - edge0);
        const float t = clamp01((value - edge0) / denominator);
        return t * t * (3.0f - 2.0f * t);
    }

    static float linearLuma(const Pixel& pixel)
    {
        return 0.2627f * pixel.r +
            0.6780f * pixel.g +
            0.0593f * pixel.b;
    }

    static float encodePerceptual(float value, int workingSpace)
    {
        // Display, log, and common grading working spaces are already
        // perceptually distributed. Only scene-linear input needs compression.
        if (workingSpace != WorkingSceneLinear) {
            return value;
        }
        const float magnitude = std::fabs(value);
        const float sign = value < 0.0f ? -1.0f : 1.0f;
        const float scale = 16.0f;
        return sign * std::log1p(magnitude * scale) / std::log1p(scale);
    }

    static float perceptualLuma(const Pixel& pixel, int workingSpace)
    {
        return encodePerceptual(linearLuma(pixel), workingSpace);
    }

    static Pixel midpoint(const Pixel& a, const Pixel& b)
    {
        return Pixel{
            0.5f * (a.r + b.r),
            0.5f * (a.g + b.g),
            0.5f * (a.b + b.b),
            0.5f * (a.a + b.a),
        };
    }

    static Pixel addWeighted(Pixel sum, const Pixel& value, float weight)
    {
        sum.r += value.r * weight;
        sum.g += value.g * weight;
        sum.b += value.b * weight;
        sum.a += value.a * weight;
        return sum;
    }

    static Pixel multiply(const Pixel& pixel, float value)
    {
        return Pixel{
            pixel.r * value,
            pixel.g * value,
            pixel.b * value,
            pixel.a * value,
        };
    }

    static Pixel mix(const Pixel& a, const Pixel& b, float amount)
    {
        const float t = clamp01(amount);
        const float u = 1.0f - t;
        return Pixel{
            a.r * u + b.r * t,
            a.g * u + b.g * t,
            a.b * u + b.b * t,
            a.a * u + b.a * t,
        };
    }

    static float hashNoise(int x, int y, int frame, float seed)
    {
        std::uint32_t hash = static_cast<std::uint32_t>(x) * 0x8da6b343u;
        hash ^= static_cast<std::uint32_t>(y) * 0xd8163841u;
        hash ^= static_cast<std::uint32_t>(frame) * 0xcb1ab31fu;
        hash ^= static_cast<std::uint32_t>(seed * 4096.0f + 0.5f) * 0x165667b1u;
        hash ^= hash >> 13;
        hash *= 0x85ebca6bu;
        hash ^= hash >> 16;
        return static_cast<float>(hash & 0x00ffffffu) /
            8388607.5f - 1.0f;
    }

    static float blueNoise(int x, int y, int frame, float seed)
    {
        const float center = hashNoise(x, y, frame, seed);
        const float neighborhood = 0.25f * (
            hashNoise(x - 1, y, frame, seed) +
            hashNoise(x + 1, y, frame, seed) +
            hashNoise(x, y - 1, frame, seed) +
            hashNoise(x, y + 1, frame, seed));
        return std::clamp((center - neighborhood) * 0.70f, -1.0f, 1.0f);
    }

    static Pixel heatMap(float value, float alpha)
    {
        const float v = clamp01(value);
        return Pixel{
            smoothstep(0.20f, 0.75f, v),
            smoothstep(0.02f, 0.45f, v) * (1.0f - smoothstep(0.75f, 1.0f, v)),
            0.08f * (1.0f - v),
            alpha,
        };
    }

    static Pixel sanitize(const Pixel& pixel, const Pixel& fallback)
    {
        return Pixel{
            std::isfinite(pixel.r) ? pixel.r : fallback.r,
            std::isfinite(pixel.g) ? pixel.g : fallback.g,
            std::isfinite(pixel.b) ? pixel.b : fallback.b,
            fallback.a,
        };
    }
};

} // namespace buckswood_deband
