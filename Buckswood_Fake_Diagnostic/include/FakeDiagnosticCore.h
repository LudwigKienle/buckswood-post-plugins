#pragma once

#include <algorithm>
#include <cmath>

namespace buckswood_fake {

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
    float sensitivity;
    float overlayStrength;
    float correctionStrength;
    float plasticWeight;
    float highlightWeight;
    float edgeWeight;
    float gradeWeight;
    float textureWeight;
    float temporalWeight;
    float microContrast;
    float edgeSoften;
    float highlightRolloff;
    float gamutGuard;
    float sensorTexture;
    float temporalAssist;
    float seed;
};

struct Scores {
    float plastic;
    float highlight;
    float edge;
    float grade;
    float texture;
    float temporal;
    float overall;
};

enum ViewMode {
    ViewDiagnosticOverlay = 0,
    ViewProblemMatte = 1,
    ViewRealityMatchAssist = 2,
    ViewAssistWithOverlay = 3,
    ViewCategoryColors = 4,
    ViewTemporalStabilityMatte = 5,
    ViewTemporalDifferenceOverlay = 6,
};

// Kept for compatibility with existing samplers; the render kernel is a
// template over the concrete sampler type so calls devirtualize and inline.
class Sampler {
public:
    virtual ~Sampler() = default;
    virtual Pixel sample(float x, float y) const = 0;
};

template <typename SamplerT>
struct TemporalContextT {
    const SamplerT* previous = nullptr;
    const SamplerT* next = nullptr;
    bool hasPrevious = false;
    bool hasNext = false;
};

using TemporalContext = TemporalContextT<Sampler>;

class FakeDiagnosticCore {
public:
    static Controls defaultControls();

    template <typename SamplerT>
    static Scores analyzePixel(
        const SamplerT& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls,
        const TemporalContextT<SamplerT>* temporal = nullptr);

    template <typename SamplerT>
    static Pixel processPixel(
        const SamplerT& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls,
        int viewMode,
        const TemporalContextT<SamplerT>* temporal = nullptr);

private:
    static float clamp01(float value)
    {
        return std::min(1.0f, std::max(0.0f, value));
    }

    static float luma(Pixel pixel)
    {
        return pixel.r * 0.2126f + pixel.g * 0.7152f + pixel.b * 0.0722f;
    }

    static float max3(float a, float b, float c)
    {
        return std::max(a, std::max(b, c));
    }

    static float min3(float a, float b, float c)
    {
        return std::min(a, std::min(b, c));
    }

    static float smoothstep(float edge0, float edge1, float x)
    {
        if (edge0 == edge1) {
            return x < edge0 ? 0.0f : 1.0f;
        }
        const float t = clamp01((x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }

    static float hash01(int x, int y, int z)
    {
        int n = x * 15731 + y * 789221 + z * 1376312589;
        n = (n << 13) ^ n;
        const int hashed = (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff;
        return static_cast<float>(hashed) / 2147483647.0f;
    }

    static Pixel add(Pixel a, Pixel b)
    {
        return Pixel{a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a};
    }

    static Pixel sub(Pixel a, Pixel b)
    {
        return Pixel{a.r - b.r, a.g - b.g, a.b - b.b, a.a - b.a};
    }

    static Pixel mul(Pixel a, float scalar)
    {
        return Pixel{a.r * scalar, a.g * scalar, a.b * scalar, a.a * scalar};
    }

    static Pixel mix(Pixel a, Pixel b, float t)
    {
        t = clamp01(t);
        return Pixel{
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t,
        };
    }

    static Pixel clampPixel(Pixel pixel)
    {
        return Pixel{
            clamp01(pixel.r),
            clamp01(pixel.g),
            clamp01(pixel.b),
            clamp01(pixel.a),
        };
    }

    static Pixel categoryColor(const Scores& scores);

    template <typename SamplerT>
    static Pixel blur5(const SamplerT& sampler, int x, int y)
    {
        Pixel c = sampler.sample(static_cast<float>(x), static_cast<float>(y));
        Pixel l = sampler.sample(static_cast<float>(x - 1), static_cast<float>(y));
        Pixel r = sampler.sample(static_cast<float>(x + 1), static_cast<float>(y));
        Pixel u = sampler.sample(static_cast<float>(x), static_cast<float>(y - 1));
        Pixel d = sampler.sample(static_cast<float>(x), static_cast<float>(y + 1));
        return mul(add(add(add(add(c, l), r), u), d), 0.2f);
    }

    template <typename SamplerT>
    static float temporalDifference(
        const SamplerT& sampler,
        int x,
        int y,
        const TemporalContextT<SamplerT>* temporal);

    template <typename SamplerT>
    static float temporalRisk(
        const SamplerT& sampler,
        int x,
        int y,
        const Scores& spatialScores,
        const TemporalContextT<SamplerT>* temporal);

    template <typename SamplerT>
    static Pixel applyAssist(
        const SamplerT& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        Pixel input,
        const Scores& scores,
        const Controls& controls,
        const TemporalContextT<SamplerT>* temporal);
};

template <typename SamplerT>
Scores FakeDiagnosticCore::analyzePixel(
    const SamplerT& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const Controls& controls,
    const TemporalContextT<SamplerT>* temporal)
{
    (void)frame;

    const Pixel c = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    const Pixel l = sampler.sample(static_cast<float>(x - 1), static_cast<float>(y));
    const Pixel r = sampler.sample(static_cast<float>(x + 1), static_cast<float>(y));
    const Pixel u = sampler.sample(static_cast<float>(x), static_cast<float>(y - 1));
    const Pixel d = sampler.sample(static_cast<float>(x), static_cast<float>(y + 1));
    const Pixel ul = sampler.sample(static_cast<float>(x - 1), static_cast<float>(y - 1));
    const Pixel ur = sampler.sample(static_cast<float>(x + 1), static_cast<float>(y - 1));
    const Pixel dl = sampler.sample(static_cast<float>(x - 1), static_cast<float>(y + 1));
    const Pixel dr = sampler.sample(static_cast<float>(x + 1), static_cast<float>(y + 1));

    const float cy = luma(c);
    const float ly = luma(l);
    const float ry = luma(r);
    const float uy = luma(u);
    const float dy = luma(d);
    const float d1y = luma(ul);
    const float d2y = luma(ur);
    const float d3y = luma(dl);
    const float d4y = luma(dr);

    const float detail =
        (std::fabs(cy - ly) + std::fabs(cy - ry) + std::fabs(cy - uy) + std::fabs(cy - dy) +
         std::fabs(cy - d1y) + std::fabs(cy - d2y) + std::fabs(cy - d3y) + std::fabs(cy - d4y)) *
        0.125f;
    const float gx = ry - ly;
    const float gy = dy - uy;
    const float gradient = std::sqrt(gx * gx + gy * gy);
    const float maxChannel = max3(c.r, c.g, c.b);
    const float minChannel = min3(c.r, c.g, c.b);
    const float chroma = maxChannel - minChannel;
    const float saturation = chroma / std::max(0.001f, maxChannel);
    const float midtoneMask = smoothstep(0.06f, 0.28f, cy) * (1.0f - smoothstep(0.78f, 0.98f, cy));
    const float brightMask = smoothstep(0.74f, 1.0f, cy);
    const float clipped = std::max(
        smoothstep(0.965f, 1.02f, c.r),
        std::max(smoothstep(0.965f, 1.02f, c.g), smoothstep(0.965f, 1.02f, c.b)));
    const float localClean = 1.0f - smoothstep(0.012f, 0.085f, detail);

    Scores scores{};
    scores.plastic = clamp01(localClean * midtoneMask * (0.45f + 0.55f * smoothstep(0.06f, 0.55f, saturation)));
    scores.highlight = clamp01(brightMask * (0.35f + 0.65f * clipped));
    scores.edge = clamp01(smoothstep(0.10f, 0.42f, gradient) * (0.35f + 0.65f * localClean));
    scores.grade = clamp01(std::max(smoothstep(0.68f, 1.0f, saturation), smoothstep(0.92f, 1.06f, maxChannel)));
    scores.texture = clamp01(localClean * (1.0f - smoothstep(0.12f, 0.38f, gradient)) * midtoneMask);
    scores.temporal = temporalRisk(sampler, x, y, scores, temporal);

    const float weightSum = std::max(
        0.001f,
        controls.plasticWeight + controls.highlightWeight + controls.edgeWeight + controls.gradeWeight + controls.textureWeight +
            controls.temporalWeight);
    scores.overall = clamp01(
        controls.sensitivity *
        (scores.plastic * controls.plasticWeight +
         scores.highlight * controls.highlightWeight +
         scores.edge * controls.edgeWeight +
         scores.grade * controls.gradeWeight +
         scores.texture * controls.textureWeight +
         scores.temporal * controls.temporalWeight) /
            weightSum);
    return scores;
}

template <typename SamplerT>
float FakeDiagnosticCore::temporalDifference(
    const SamplerT& sampler,
    int x,
    int y,
    const TemporalContextT<SamplerT>* temporal)
{
    if (!temporal || (!temporal->hasPrevious && !temporal->hasNext)) {
        return 0.0f;
    }

    const Pixel c = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    float total = 0.0f;
    float count = 0.0f;

    auto accumulate = [&](const SamplerT* other) {
        if (!other) {
            return;
        }
        const Pixel p = other->sample(static_cast<float>(x), static_cast<float>(y));
        const float lumaDelta = std::fabs(luma(c) - luma(p));
        const float chromaDelta =
            (std::fabs(c.r - p.r) + std::fabs(c.g - p.g) + std::fabs(c.b - p.b)) * 0.333f;
        total += lumaDelta * 0.65f + chromaDelta * 0.35f;
        count += 1.0f;
    };

    if (temporal->hasPrevious) {
        accumulate(temporal->previous);
    }
    if (temporal->hasNext) {
        accumulate(temporal->next);
    }

    return count > 0.0f ? clamp01(total / count) : 0.0f;
}

template <typename SamplerT>
float FakeDiagnosticCore::temporalRisk(
    const SamplerT& sampler,
    int x,
    int y,
    const Scores& spatialScores,
    const TemporalContextT<SamplerT>* temporal)
{
    if (!temporal || (!temporal->hasPrevious && !temporal->hasNext)) {
        return 0.0f;
    }

    const float diff = temporalDifference(sampler, x, y, temporal);
    const float fakeSpatial =
        spatialScores.plastic * 0.35f +
        spatialScores.edge * 0.25f +
        spatialScores.texture * 0.25f +
        spatialScores.highlight * 0.10f +
        spatialScores.grade * 0.05f;

    const float tooStatic = (1.0f - smoothstep(0.006f, 0.055f, diff)) * fakeSpatial;
    const float temporalFlicker = smoothstep(0.10f, 0.32f, diff) * (spatialScores.highlight * 0.45f + spatialScores.grade * 0.35f + 0.20f);
    return clamp01(std::max(tooStatic, temporalFlicker * 0.65f));
}

template <typename SamplerT>
Pixel FakeDiagnosticCore::applyAssist(
    const SamplerT& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    Pixel input,
    const Scores& scores,
    const Controls& controls,
    const TemporalContextT<SamplerT>* temporal)
{
    Pixel out = input;
    const Pixel blur = blur5(sampler, x, y);
    const Pixel detail = sub(input, blur);
    const float correction = clamp01(controls.correctionStrength) * scores.overall;

    out = add(out, mul(detail, controls.microContrast * correction));
    out = mix(out, blur, controls.edgeSoften * correction * scores.edge);

    const float yv = luma(out);
    const Pixel gray{yv, yv, yv, out.a};
    out = mix(out, gray, controls.gamutGuard * correction * scores.grade * 0.45f);

    const float roll = controls.highlightRolloff * correction * scores.highlight;
    if (roll > 0.0f) {
        auto rollChannel = [roll](float v) {
            const float knee = 0.72f;
            if (v <= knee) {
                return v;
            }
            const float compressed = knee + (1.0f - std::exp(-(v - knee) * 3.2f)) * 0.28f;
            return v + (compressed - v) * roll;
        };
        out.r = rollChannel(out.r);
        out.g = rollChannel(out.g);
        out.b = rollChannel(out.b);
    }

    const float noiseBase = (hash01(x, y, frame.frameIndex + static_cast<int>(controls.seed * 13.0f)) - 0.5f);
    const float noiseAlt = (hash01(x + 17, y - 11, frame.frameIndex + static_cast<int>(controls.seed * 29.0f)) - 0.5f);
    const float textureAmount = controls.sensorTexture * correction * (0.35f + 0.65f * scores.texture) * 0.045f;
    out.r += noiseBase * textureAmount;
    out.g += (noiseBase * 0.55f + noiseAlt * 0.45f) * textureAmount;
    out.b += noiseAlt * textureAmount;

    if (temporal && (temporal->hasPrevious || temporal->hasNext)) {
        const float temporalMix = clamp01(controls.temporalAssist * correction * scores.temporal);
        if (temporalMix > 0.0f) {
            Pixel temporalAverage{0.0f, 0.0f, 0.0f, input.a};
            float count = 0.0f;
            if (temporal->hasPrevious && temporal->previous) {
                temporalAverage = add(temporalAverage, temporal->previous->sample(static_cast<float>(x), static_cast<float>(y)));
                count += 1.0f;
            }
            if (temporal->hasNext && temporal->next) {
                temporalAverage = add(temporalAverage, temporal->next->sample(static_cast<float>(x), static_cast<float>(y)));
                count += 1.0f;
            }
            if (count > 0.0f) {
                temporalAverage = mul(temporalAverage, 1.0f / count);
                temporalAverage.a = input.a;
                out = mix(out, temporalAverage, temporalMix * 0.35f);
            }
        }
    }
    out.a = input.a;

    return clampPixel(out);
}

template <typename SamplerT>
Pixel FakeDiagnosticCore::processPixel(
    const SamplerT& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const Controls& controls,
    int viewMode,
    const TemporalContextT<SamplerT>* temporal)
{
    const Pixel input = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    const Scores scores = analyzePixel(sampler, x, y, frame, controls, temporal);
    const Pixel category = categoryColor(scores);

    if (viewMode == ViewProblemMatte) {
        return Pixel{scores.overall, scores.overall, scores.overall, input.a};
    }

    if (viewMode == ViewCategoryColors) {
        const float level = std::max(0.18f, scores.overall);
        return Pixel{category.r * level, category.g * level, category.b * level, input.a};
    }

    if (viewMode == ViewTemporalStabilityMatte) {
        return Pixel{scores.temporal, scores.temporal, scores.temporal, input.a};
    }

    if (viewMode == ViewTemporalDifferenceOverlay) {
        const float diff = temporalDifference(sampler, x, y, temporal);
        const Pixel diffColor{0.0f, clamp01(diff * 4.0f), clamp01(1.0f - diff * 5.0f), input.a};
        const Pixel riskColor{0.0f, 1.0f, 0.22f, input.a};
        const Pixel color = mix(diffColor, riskColor, scores.temporal);
        return clampPixel(mix(input, color, controls.overlayStrength * std::max(diff, scores.temporal)));
    }

    if (viewMode == ViewRealityMatchAssist) {
        return applyAssist(sampler, x, y, frame, input, scores, controls, temporal);
    }

    Pixel base = input;
    if (viewMode == ViewAssistWithOverlay) {
        base = applyAssist(sampler, x, y, frame, input, scores, controls, temporal);
    }

    const Pixel overlayBase = mix(base, category, controls.overlayStrength * scores.overall);
    return clampPixel(Pixel{overlayBase.r, overlayBase.g, overlayBase.b, input.a});
}

} // namespace buckswood_fake
