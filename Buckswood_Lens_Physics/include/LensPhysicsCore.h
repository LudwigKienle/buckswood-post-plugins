#pragma once

#include <algorithm>
#include <cmath>

namespace buckswood_lens {

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
    int lensPreset;
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
    float edgeHaloGuard;
};

// Kept for compatibility with existing samplers; the render kernel is a
// template over the concrete sampler type so calls devirtualize and inline.
class Sampler {
public:
    virtual ~Sampler() = default;
    virtual Pixel sample(float x, float y) const = 0;
};

class LensPhysicsCore {
public:
    template <typename SamplerT>
    static Pixel processPixel(
        const SamplerT& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls);

private:
    struct LensModel {
        float distortion;
        float chromaticAberration;
        float fringing;
        float coma;
        float bloom;
        float vignette;
        float cornerColor;
        float swirl;
        float sharpener;
        float warmBias;
        float blueBias;
    };

    static LensModel modelForPreset(int preset);

    static float clamp01(float value)
    {
        return std::min(1.0f, std::max(0.0f, value));
    }

    static float luma(Pixel pixel)
    {
        return 0.2627f * pixel.r + 0.6780f * pixel.g + 0.0593f * pixel.b;
    }

    static float smoothstep(float edge0, float edge1, float x)
    {
        const float t = clamp01((x - edge0) / std::max(0.0001f, edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
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

    static Pixel mul(Pixel a, float scalar)
    {
        return Pixel{a.r * scalar, a.g * scalar, a.b * scalar, a.a};
    }

    static float safeDiv(float a, float b)
    {
        return b == 0.0f ? 0.0f : a / b;
    }

    static float clamp(float value, float low, float high)
    {
        return std::min(high, std::max(low, value));
    }

    template <typename SamplerT>
    static Pixel applyRadialLens(
        const SamplerT& sampler,
        float srcX,
        float srcY,
        float dirX,
        float dirY,
        float tangentX,
        float tangentY,
        float radius,
        const LensModel& model,
        const Controls& controls);
};

template <typename SamplerT>
Pixel LensPhysicsCore::processPixel(
    const SamplerT& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const Controls& controls)
{
    const Pixel dry = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    LensModel model = modelForPreset(controls.lensPreset);
    const float lensAmount = clamp01(controls.effectStrength) * clamp01(controls.outputMix);
    const float rawOverdrive = std::max(0.0f, controls.overdrive);
    const float baseOd = rawOverdrive * lensAmount;

    model.distortion += controls.distortion * 0.18f;
    model.chromaticAberration += controls.chromaticAberration;
    model.fringing += controls.fringing;
    model.coma += controls.coma;
    model.bloom += controls.bloom;
    model.vignette += controls.vignette;
    model.cornerColor += controls.cornerColor;
    model.swirl += controls.swirl;
    model.sharpener += controls.fStopSharpener;

    const float width = static_cast<float>(std::max(1, frame.width));
    const float height = static_cast<float>(std::max(1, frame.height));
    const float cx = (width - 1.0f) * 0.5f;
    const float cy = (height - 1.0f) * 0.5f;
    const float aspect = safeDiv(width, height);
    const float nx = safeDiv(static_cast<float>(x) - cx, cx);
    const float ny = safeDiv(static_cast<float>(y) - cy, cy);
    const float ax = nx * aspect;
    const float radius = std::sqrt(ax * ax + ny * ny);
    const float edge = smoothstep(0.18f, 1.18f, radius);

    float dirX = nx;
    float dirY = ny;
    const float dirLen = std::sqrt(dirX * dirX + dirY * dirY);
    if (dirLen > 0.0001f) {
        dirX /= dirLen;
        dirY /= dirLen;
    }
    const float tangentX = -dirY;
    const float tangentY = dirX;

    const Pixel dryLeft = sampler.sample(static_cast<float>(x - 1), static_cast<float>(y));
    const Pixel dryRight = sampler.sample(static_cast<float>(x + 1), static_cast<float>(y));
    const Pixel dryUp = sampler.sample(static_cast<float>(x), static_cast<float>(y - 1));
    const Pixel dryDown = sampler.sample(static_cast<float>(x), static_cast<float>(y + 1));
    const float dryY = luma(dry);
    const float dryLeftY = luma(dryLeft);
    const float dryRightY = luma(dryRight);
    const float dryUpY = luma(dryUp);
    const float dryDownY = luma(dryDown);
    const float neighborMinY = std::min(std::min(dryLeftY, dryRightY), std::min(dryUpY, dryDownY));
    const float neighborMaxY = std::max(std::max(dryLeftY, dryRightY), std::max(dryUpY, dryDownY));
    const float localSpan = std::max(neighborMaxY, dryY) - std::min(neighborMinY, dryY);
    const float crossGradient =
        std::fabs(dryRightY - dryLeftY) +
        std::fabs(dryDownY - dryUpY) +
        std::fabs(dryY - dryLeftY) * 0.5f +
        std::fabs(dryY - dryRightY) * 0.5f +
        std::fabs(dryY - dryUpY) * 0.5f +
        std::fabs(dryY - dryDownY) * 0.5f;
    const float edgeRisk = smoothstep(0.035f, 0.26f, localSpan + crossGradient * 0.26f);
    const float darkSilhouette =
        (1.0f - smoothstep(0.10f, 0.44f, dryY)) *
        smoothstep(0.10f, 0.42f, neighborMaxY - dryY);
    const float brightSilhouette =
        smoothstep(0.54f, 0.90f, dryY) *
        smoothstep(0.10f, 0.42f, dryY - neighborMinY);
    const float contourRisk = smoothstep(0.065f, 0.34f, neighborMaxY - neighborMinY);
    const float silhouetteRisk = clamp01(edgeRisk * (
        0.34f +
        0.46f * std::max(darkSilhouette, brightSilhouette) +
        0.20f * contourRisk));
    const float overdrivePressure = smoothstep(0.18f, 0.85f, clamp01(rawOverdrive));
    const float edgeGuard = clamp01(controls.edgeHaloGuard) * silhouetteRisk * (0.42f + 0.58f * overdrivePressure);
    const float opticalOd = baseOd * (1.0f - edgeGuard * (0.62f + 0.24f * overdrivePressure));

    Controls activeControls = controls;
    activeControls.overdrive = opticalOd;
    const float od = baseOd;

    const float swirlAngle = model.swirl * od * edge * edge * 0.22f;
    const float cosA = std::cos(swirlAngle);
    const float sinA = std::sin(swirlAngle);
    const float sx = nx * cosA - ny * sinA;
    const float sy = nx * sinA + ny * cosA;

    const float r2 = radius * radius;
    const float radialScale = 1.0f + model.distortion * od * r2 + model.distortion * od * 0.35f * r2 * r2;
    const float srcX = cx + sx * radialScale * cx;
    const float srcY = cy + sy * radialScale * cy;

    Pixel rgb = applyRadialLens(sampler, srcX, srcY, dirX, dirY, tangentX, tangentY, radius, model, activeControls);

    const float vignette = clamp01(model.vignette * od) * edge * edge;
    const float vignetteGain = 1.0f - vignette * 0.46f;
    rgb.r *= vignetteGain;
    rgb.g *= vignetteGain;
    rgb.b *= vignetteGain;

    const float corner = clamp01(model.cornerColor * od) * edge * edge;
    rgb.r += corner * (model.warmBias + 0.025f);
    rgb.g += corner * 0.006f;
    rgb.b += corner * (model.blueBias - 0.018f);

    if (model.sharpener != 0.0f) {
        const Pixel c = sampler.sample(srcX, srcY);
        const Pixel b1 = sampler.sample(srcX - 1.4f, srcY);
        const Pixel b2 = sampler.sample(srcX + 1.4f, srcY);
        const Pixel b3 = sampler.sample(srcX, srcY - 1.4f);
        const Pixel b4 = sampler.sample(srcX, srcY + 1.4f);
        Pixel blur{
            (b1.r + b2.r + b3.r + b4.r) * 0.25f,
            (b1.g + b2.g + b3.g + b4.g) * 0.25f,
            (b1.b + b2.b + b3.b + b4.b) * 0.25f,
            c.a,
        };
        const float sharp = model.sharpener * od * 0.32f * (1.0f - edgeGuard * 0.86f);
        rgb.r += (c.r - blur.r) * sharp;
        rgb.g += (c.g - blur.g) * sharp;
        rgb.b += (c.b - blur.b) * sharp;
    }

    if (edgeGuard > 0.0001f) {
        const float maxDelta = 0.055f + (1.0f - edgeGuard) * 0.24f + localSpan * 0.18f;
        Pixel deltaClamped{
            dry.r + clamp(rgb.r - dry.r, -maxDelta, maxDelta),
            dry.g + clamp(rgb.g - dry.g, -maxDelta, maxDelta),
            dry.b + clamp(rgb.b - dry.b, -maxDelta, maxDelta),
            dry.a,
        };

        const float minAllowedY = clamp01(std::min(neighborMinY, dryY) - (0.025f + localSpan * 0.10f));
        const float maxAllowedY = clamp01(std::max(neighborMaxY, dryY) + (0.025f + localSpan * 0.10f));
        const float clampedY = luma(deltaClamped);
        if (clampedY < minAllowedY || clampedY > maxAllowedY) {
            const float targetY = clamp(clampedY, minAllowedY, maxAllowedY);
            const float correction = targetY - clampedY;
            deltaClamped.r += correction;
            deltaClamped.g += correction;
            deltaClamped.b += correction;
        }

        rgb = mix(rgb, deltaClamped, edgeGuard);
        rgb = mix(rgb, dry, edgeGuard * 0.22f);
    }

    rgb.r = clamp01(rgb.r);
    rgb.g = clamp01(rgb.g);
    rgb.b = clamp01(rgb.b);
    rgb.a = dry.a;

    return rgb;
}

template <typename SamplerT>
Pixel LensPhysicsCore::applyRadialLens(
    const SamplerT& sampler,
    float srcX,
    float srcY,
    float dirX,
    float dirY,
    float tangentX,
    float tangentY,
    float radius,
    const LensModel& model,
    const Controls& controls)
{
    const float edge = smoothstep(0.16f, 1.10f, radius);

    const Pixel center = sampler.sample(srcX, srcY);
    const Pixel l = sampler.sample(srcX - 1.0f, srcY);
    const Pixel r = sampler.sample(srcX + 1.0f, srcY);
    const Pixel u = sampler.sample(srcX, srcY - 1.0f);
    const Pixel d = sampler.sample(srcX, srcY + 1.0f);
    const float gradient = std::fabs(luma(r) - luma(l)) + std::fabs(luma(d) - luma(u));
    const float centerY = luma(center);
    const float silhouetteRisk =
        smoothstep(0.20f, 0.72f, gradient) *
        (1.0f - smoothstep(0.12f, 0.46f, centerY));
    const float silhouetteGuard = 1.0f - silhouetteRisk * (0.72f + clamp01(controls.edgeHaloGuard) * 0.22f);
    const float od = std::max(0.0f, controls.overdrive);
    const float caPx = model.chromaticAberration * od * edge * edge * 4.2f * silhouetteGuard;
    const Pixel red = sampler.sample(srcX + dirX * caPx, srcY + dirY * caPx);
    const Pixel blue = sampler.sample(srcX - dirX * caPx, srcY - dirY * caPx);
    Pixel rgb{red.r, center.g, blue.b, center.a};

    const float fringe = clamp01(model.fringing * od * edge * gradient * 1.32f * silhouetteGuard);
    rgb.r += fringe * (0.08f + 0.05f * edge);
    rgb.g -= fringe * 0.035f;
    rgb.b += fringe * (0.04f + 0.04f * edge);

    const float threshold = std::max(0.35f, controls.bloomThreshold);
    const float bloomStrength = clamp01(model.bloom * od);
    if (bloomStrength > 0.0001f) {
        Pixel bloom{0.0f, 0.0f, 0.0f, center.a};
        float weightSum = 0.0f;
        const float radiusPx = 2.0f + bloomStrength * 8.0f;
        const float offsets[8][2] = {
            {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
            {0.707f, 0.707f}, {-0.707f, 0.707f}, {0.707f, -0.707f}, {-0.707f, -0.707f},
        };
        for (const auto& offset : offsets) {
            const Pixel p = sampler.sample(srcX + offset[0] * radiusPx, srcY + offset[1] * radiusPx);
            const float hot = clamp01((luma(p) - threshold) / std::max(0.001f, 1.0f - threshold));
            const float w = hot * hot;
            bloom = add(bloom, mul(p, w));
            weightSum += w;
        }
        if (weightSum > 0.0001f) {
            bloom = mul(bloom, 1.0f / weightSum);
            const float hotCenter = clamp01((luma(center) - threshold) / std::max(0.001f, 1.0f - threshold));
            const float amount = bloomStrength * (0.18f + 0.42f * hotCenter) * silhouetteGuard;
            rgb.r += bloom.r * amount;
            rgb.g += bloom.g * amount;
            rgb.b += bloom.b * amount;
        }
    }

    const float comaStrength = clamp01(model.coma * od) * edge * edge * silhouetteGuard;
    if (comaStrength > 0.0001f) {
        Pixel comet{0.0f, 0.0f, 0.0f, center.a};
        float weightSum = 0.0f;
        for (int i = 1; i <= 5; ++i) {
            const float fi = static_cast<float>(i);
            const float spread = fi * (1.2f + comaStrength * 4.0f);
            const float tx = tangentX * spread + dirX * spread * 0.55f;
            const float ty = tangentY * spread + dirY * spread * 0.55f;
            const Pixel p = sampler.sample(srcX + tx, srcY + ty);
            const float hot = clamp01((luma(p) - 0.58f) / 0.42f);
            const float w = hot * (1.0f / fi);
            comet = add(comet, mul(p, w));
            weightSum += w;
        }
        if (weightSum > 0.0001f) {
            comet = mul(comet, 1.0f / weightSum);
            rgb.r += comet.r * comaStrength * 0.34f;
            rgb.g += comet.g * comaStrength * 0.24f;
            rgb.b += comet.b * comaStrength * 0.18f;
        }
    }

    return rgb;
}

} // namespace buckswood_lens
