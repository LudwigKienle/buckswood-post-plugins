#include "LensPhysicsCore.h"

#include <algorithm>
#include <cmath>

namespace buckswood_lens {

namespace {

float safeDiv(float a, float b)
{
    return b == 0.0f ? 0.0f : a / b;
}

} // namespace

LensPhysicsCore::LensModel LensPhysicsCore::modelForPreset(int preset)
{
    switch (preset) {
    case 1: // Clean Cinema Prime
        return LensModel{0.015f, 0.16f, 0.08f, 0.04f, 0.18f, 0.18f, 0.06f, 0.00f, 0.16f, 0.01f, 0.00f};
    case 2: // Vintage Spherical
        return LensModel{-0.075f, 0.36f, 0.25f, 0.28f, 0.42f, 0.35f, 0.22f, 0.04f, 0.04f, 0.03f, -0.01f};
    case 3: // Petzval Swirl
        return LensModel{-0.120f, 0.30f, 0.18f, 0.34f, 0.32f, 0.42f, 0.20f, 0.42f, -0.02f, 0.02f, 0.02f};
    case 4: // Anamorphic Soft
        return LensModel{0.030f, 0.46f, 0.35f, 0.18f, 0.52f, 0.26f, 0.14f, 0.02f, 0.02f, 0.01f, 0.04f};
    case 5: // Soviet Glow
        return LensModel{-0.105f, 0.42f, 0.30f, 0.38f, 0.60f, 0.48f, 0.30f, 0.16f, -0.04f, 0.05f, -0.02f};
    case 6: // AI Deplastic Glass
        return LensModel{-0.035f, 0.22f, 0.14f, 0.12f, 0.28f, 0.24f, 0.10f, 0.03f, 0.08f, 0.02f, 0.01f};
    case 7: // Lensfun: KMZ Helios-44 58mm f/2, M42, full frame
        return LensModel{0.035f, 0.10f, 0.18f, 0.30f, 0.42f, 0.46f, 0.28f, 0.24f, -0.03f, 0.035f, -0.01f};
    case 8: // Lensfun: KMZ MC Helios-44M-4 58mm f/2, M42, full frame
        return LensModel{0.026f, 0.07f, 0.14f, 0.26f, 0.36f, 0.48f, 0.30f, 0.18f, -0.02f, 0.030f, -0.01f};
    case 9: // Lensfun: Zeiss Batis 25mm f/2, Sony E
        return LensModel{-0.055f, 0.04f, 0.06f, 0.06f, 0.20f, 0.39f, 0.12f, 0.00f, 0.12f, 0.005f, 0.01f};
    case 10: // Lensfun: Voigtlander Nokton 58mm f/1.4 SLII
        return LensModel{-0.030f, 0.06f, 0.12f, 0.18f, 0.30f, 0.34f, 0.18f, 0.08f, 0.02f, 0.020f, 0.00f};
    case 11: // Lensfun: Voigtlander Nokton 28mm f/1.5 Aspherical
        return LensModel{0.006f, 0.22f, 0.20f, 0.14f, 0.34f, 0.58f, 0.24f, 0.04f, 0.04f, 0.010f, 0.01f};
    case 12: // Dune Part One: Panavision H Series, spherical IMAX 1.43 inspiration
        return LensModel{-0.012f, 0.10f, 0.09f, 0.08f, 0.24f, 0.20f, 0.07f, 0.00f, 0.02f, 0.018f, 0.004f};
    case 13: // Dune Part One: Panavision Ultra Vista 1.6x anamorphic inspiration
        return LensModel{0.020f, 0.34f, 0.30f, 0.10f, 0.45f, 0.24f, 0.11f, 0.02f, -0.02f, 0.012f, 0.020f};
    case 14: // Dune Part Two: ARRI Rental Moviecam spherical inspiration
        return LensModel{-0.018f, 0.12f, 0.10f, 0.16f, 0.28f, 0.22f, 0.10f, 0.03f, 0.00f, 0.020f, 0.005f};
    case 15: // Dune Part Two: IronGlass Soviet-era spherical inspiration
        return LensModel{-0.070f, 0.28f, 0.26f, 0.34f, 0.48f, 0.42f, 0.24f, 0.18f, -0.06f, 0.035f, -0.010f};
    case 16: // IMAX 15/70: 50mm clean wide face sweet spot inspiration
        return LensModel{-0.006f, 0.08f, 0.06f, 0.04f, 0.18f, 0.08f, 0.03f, 0.00f, 0.08f, 0.006f, 0.004f};
    case 17: // IMAX 15/70: 80mm portrait sweet spot inspiration
        return LensModel{-0.004f, 0.06f, 0.05f, 0.03f, 0.16f, 0.06f, 0.025f, 0.00f, 0.10f, 0.004f, 0.003f};
    case 18: // Dune Part Three: Atlas golden single-coating IMAX inspiration
        return LensModel{-0.004f, 0.12f, 0.18f, 0.10f, 0.30f, 0.02f, 0.14f, 0.00f, 0.06f, 0.045f, 0.000f};
    case 19: // Buckswood Realistic Match: subtle AI-footage lens correction default
        return LensModel{-0.006f, 0.030f, 0.020f, 0.020f, 0.055f, 0.040f, 0.014f, 0.000f, 0.055f, 0.003f, 0.001f};
    case 0:
    default:
        return LensModel{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    }
}

Pixel LensPhysicsCore::processPixel(
    const Sampler& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const Controls& controls)
{
    const Pixel dry = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    LensModel model = modelForPreset(controls.lensPreset);
    Controls activeControls = controls;
    const float lensAmount = clamp01(controls.effectStrength) * clamp01(controls.outputMix);
    activeControls.overdrive = std::max(0.0f, controls.overdrive) * lensAmount;
    const float od = activeControls.overdrive;

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
        const float sharp = model.sharpener * od * 0.32f;
        rgb.r += (c.r - blur.r) * sharp;
        rgb.g += (c.g - blur.g) * sharp;
        rgb.b += (c.b - blur.b) * sharp;
    }

    rgb.r = clamp01(rgb.r);
    rgb.g = clamp01(rgb.g);
    rgb.b = clamp01(rgb.b);
    rgb.a = dry.a;

    return rgb;
}

Pixel LensPhysicsCore::applyRadialLens(
    const Sampler& sampler,
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
    const float silhouetteGuard = 1.0f - silhouetteRisk * 0.72f;
    const float od = std::max(0.0f, controls.overdrive);
    const float caPx = model.chromaticAberration * od * edge * edge * 5.0f * silhouetteGuard;
    const Pixel red = sampler.sample(srcX + dirX * caPx, srcY + dirY * caPx);
    const Pixel blue = sampler.sample(srcX - dirX * caPx, srcY - dirY * caPx);
    Pixel rgb{red.r, center.g, blue.b, center.a};

    const float fringe = clamp01(model.fringing * od * edge * gradient * 1.8f * silhouetteGuard);
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

float LensPhysicsCore::clamp01(float value)
{
    return std::min(1.0f, std::max(0.0f, value));
}

float LensPhysicsCore::luma(Pixel pixel)
{
    return 0.2627f * pixel.r + 0.6780f * pixel.g + 0.0593f * pixel.b;
}

float LensPhysicsCore::smoothstep(float edge0, float edge1, float x)
{
    const float t = clamp01((x - edge0) / std::max(0.0001f, edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

Pixel LensPhysicsCore::mix(Pixel a, Pixel b, float t)
{
    const float u = 1.0f - t;
    return Pixel{
        a.r * u + b.r * t,
        a.g * u + b.g * t,
        a.b * u + b.b * t,
        a.a * u + b.a * t,
    };
}

Pixel LensPhysicsCore::add(Pixel a, Pixel b)
{
    return Pixel{a.r + b.r, a.g + b.g, a.b + b.b, a.a};
}

Pixel LensPhysicsCore::mul(Pixel a, float scalar)
{
    return Pixel{a.r * scalar, a.g * scalar, a.b * scalar, a.a};
}

} // namespace buckswood_lens
