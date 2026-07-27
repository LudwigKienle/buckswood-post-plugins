#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace buckswood_optics {

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

struct TextureView {
    const float* luminance = nullptr;
    int width = 0;
    int height = 0;

    bool valid() const
    {
        return luminance && width > 0 && height > 0;
    }

    float sample(float u, float v) const
    {
        if (!valid()) {
            return 1.0f;
        }
        const float cu = std::min(1.0f, std::max(0.0f, u));
        const float cv = std::min(1.0f, std::max(0.0f, v));
        const int x = std::min(width - 1, static_cast<int>(cu * static_cast<float>(width - 1) + 0.5f));
        const int y = std::min(height - 1, static_cast<int>(cv * static_cast<float>(height - 1) + 0.5f));
        return luminance[y * width + x];
    }
};

struct AssetViews {
    TextureView aperture;
    TextureView dirt;
    TextureView smudge;
};

struct Controls {
    int preset;
    float effectStrength;

    float focalLength;
    float fStop;
    float focusDistance;
    float sensorWidth;
    float anamorphicSqueeze;
    float anamorphicAngle;

    float distortion;
    float breathing;
    float lateralCA;
    float axialCA;
    float coma;
    float astigmatism;
    float fieldCurvature;
    float spherical;
    float swirl;

    int depthSource;
    bool depthInvert;
    float depthNear;
    float depthFar;
    float depthGamma;
    float focusPlane;
    float focusOffset;
    float defocus;
    float catEye;

    float bloom;
    float bloomThreshold;
    float diffusion;
    float halation;
    float flareGhosts;
    float flareStreak;
    float starburst;

    float vignette;
    float debayer;
    float chromaSmear;
    float grain;
    float grainSize;
    float grainSeed;
    float sensorISO;
    float apertureInfluence;
    float dirtAmount;
    float dirtScale;
    float smudgeAmount;
    float smudgeScale;

    float edgeGuard;
    float outputMix;
};

class Sampler {
public:
    virtual ~Sampler() = default;
    virtual Pixel sample(float x, float y) const = 0;
};

class OpticsLabCore {
public:
    struct Model {
        float distortion;
        float breathing;
        float lateralCA;
        float axialCA;
        float coma;
        float astigmatism;
        float fieldCurvature;
        float spherical;
        float swirl;
        float defocus;
        float catEye;
        float bloom;
        float diffusion;
        float halation;
        float flareGhosts;
        float flareStreak;
        float starburst;
        float vignette;
        float debayer;
        float chromaSmear;
        float grain;
        float warmth;
    };

    struct PreparedState {
        Model model;
        Controls controls;
        float width;
        float height;
        int frameIndex;
        float centerX;
        float centerY;
        float aspect;
        float amount;
        float apertureScale;
        float focalScale;
        float breathingScale;
        float anamorphicCos;
        float anamorphicSin;
        float isoGrainScale;
        bool needsEdgeGuard;
        bool identityMapping;
        bool identityOutput;
    };

    static PreparedState prepare(const FrameInfo& frame, const Controls& controls);

    template <typename SamplerT>
    static Pixel processPixel(
        const SamplerT& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls)
    {
        return processPixel(sampler, x, y, prepare(frame, controls));
    }

    template <typename SamplerT>
    static Pixel processPixel(
        const SamplerT& sampler,
        int x,
        int y,
        const PreparedState& state,
        const AssetViews* assets = nullptr);

private:
    static Model modelForPreset(int preset);

    static float clamp(float value, float low, float high)
    {
        return std::min(high, std::max(low, value));
    }

    static float clamp01(float value)
    {
        return clamp(value, 0.0f, 1.0f);
    }

    static float smoothstep(float edge0, float edge1, float value)
    {
        const float t = clamp01((value - edge0) / std::max(0.00001f, edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }

    static float luma(const Pixel& p)
    {
        return 0.2627f * p.r + 0.6780f * p.g + 0.0593f * p.b;
    }

    static Pixel mix(const Pixel& a, const Pixel& b, float t)
    {
        const float u = 1.0f - t;
        return Pixel{
            a.r * u + b.r * t,
            a.g * u + b.g * t,
            a.b * u + b.b * t,
            a.a * u + b.a * t,
        };
    }

    static Pixel add(const Pixel& a, const Pixel& b)
    {
        return Pixel{a.r + b.r, a.g + b.g, a.b + b.b, a.a};
    }

    static Pixel mul(const Pixel& p, float value)
    {
        return Pixel{p.r * value, p.g * value, p.b * value, p.a};
    }

    static float hashNoise(int x, int y, int frame, float seed)
    {
        std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8da6b343u;
        h ^= static_cast<std::uint32_t>(y) * 0xd8163841u;
        h ^= static_cast<std::uint32_t>(frame) * 0xcb1ab31fu;
        h ^= static_cast<std::uint32_t>(seed * 4096.0f + 0.5f) * 0x165667b1u;
        h ^= h >> 13;
        h *= 0x85ebca6bu;
        h ^= h >> 16;
        return static_cast<float>(h & 0x00ffffffu) / 8388607.5f - 1.0f;
    }

    static Pixel sanitize(const Pixel& p, const Pixel& fallback)
    {
        return Pixel{
            std::isfinite(p.r) ? p.r : fallback.r,
            std::isfinite(p.g) ? p.g : fallback.g,
            std::isfinite(p.b) ? p.b : fallback.b,
            fallback.a,
        };
    }
};

template <typename SamplerT>
Pixel OpticsLabCore::processPixel(
    const SamplerT& sampler,
    int x,
    int y,
    const PreparedState& state,
    const AssetViews* assets)
{
    const Pixel dry = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    if (
        state.amount <= 0.000001f ||
        state.controls.outputMix <= 0.000001f ||
        state.identityOutput) {
        return dry;
    }

    const Model& model = state.model;
    const Controls& c = state.controls;
    const float cx = state.centerX;
    const float cy = state.centerY;
    const float nx = (static_cast<float>(x) - cx) / std::max(1.0f, cx);
    const float ny = (static_cast<float>(y) - cy) / std::max(1.0f, cy);
    const float ax = nx * state.aspect;
    const float radius = std::sqrt(ax * ax + ny * ny);
    const float edge = smoothstep(0.12f, 1.10f, radius);
    const float edge2 = edge * edge;

    float dirX = nx;
    float dirY = ny;
    const float dirLength = std::sqrt(dirX * dirX + dirY * dirY);
    if (dirLength > 0.0001f) {
        dirX /= dirLength;
        dirY /= dirLength;
    }
    const float tangentX = -dirY;
    const float tangentY = dirX;

    const float dryY = luma(dry);
    float guard = 0.0f;
    if (state.needsEdgeGuard) {
        const Pixel dryLeft = sampler.sample(
            static_cast<float>(x - 1),
            static_cast<float>(y));
        const Pixel dryRight = sampler.sample(
            static_cast<float>(x + 1),
            static_cast<float>(y));
        const Pixel dryUp = sampler.sample(
            static_cast<float>(x),
            static_cast<float>(y - 1));
        const Pixel dryDown = sampler.sample(
            static_cast<float>(x),
            static_cast<float>(y + 1));
        const float localGradient =
            std::fabs(luma(dryRight) - luma(dryLeft)) +
            std::fabs(luma(dryDown) - luma(dryUp));
        const float contourRisk =
            smoothstep(0.045f, 0.42f, localGradient);
        guard = c.edgeGuard * contourRisk;
    }

    const float swirlAngle = model.swirl * state.amount * edge2 * 0.24f;
    float sx = nx;
    float sy = ny;
    if (std::fabs(swirlAngle) > 0.000001f) {
        const float cosAngle = std::cos(swirlAngle);
        const float sinAngle = std::sin(swirlAngle);
        sx = nx * cosAngle - ny * sinAngle;
        sy = nx * sinAngle + ny * cosAngle;
    }

    const float r2 = radius * radius;
    const float radial =
        1.0f +
        model.distortion * state.amount * r2 +
        model.distortion * state.amount * 0.28f * r2 * r2;
    const float mappingScale = radial * state.breathingScale;
    const float srcX = cx + sx * mappingScale * cx;
    const float srcY = cy + sy * mappingScale * cy;
    const Pixel center =
        state.identityMapping
        ? dry
        : sampler.sample(srcX, srcY);

    const float caPixels =
        model.lateralCA * state.amount * edge2 * (1.2f + 2.6f * state.focalScale) *
        (1.0f - guard * 0.82f);
    Pixel result = center;
    result.a = dry.a;
    if (caPixels > 0.0001f) {
        const Pixel redSample = sampler.sample(
            srcX + dirX * caPixels,
            srcY + dirY * caPixels);
        const Pixel blueSample = sampler.sample(
            srcX - dirX * caPixels,
            srcY - dirY * caPixels);
        result.r = redSample.r;
        result.b = blueSample.b;
    }

    const float curvatureBlur =
        model.fieldCurvature * state.amount * edge2 * (1.0f - guard * 0.78f) * 4.0f;
    const float astigBlur =
        model.astigmatism * state.amount * edge2 * (1.0f - guard * 0.78f) * 3.2f;
    const float sphericalBlur =
        model.spherical * state.amount * (0.30f + edge * 0.70f) * (1.0f - guard * 0.72f) * 2.8f;
    if (curvatureBlur + astigBlur + sphericalBlur > 0.001f) {
        const float radialRadius = curvatureBlur + sphericalBlur;
        const float tangentRadius = astigBlur + sphericalBlur * 0.72f;
        const Pixel radialA = sampler.sample(srcX + dirX * radialRadius, srcY + dirY * radialRadius);
        const Pixel radialB = sampler.sample(srcX - dirX * radialRadius, srcY - dirY * radialRadius);
        const Pixel tangentA = sampler.sample(srcX + tangentX * tangentRadius, srcY + tangentY * tangentRadius);
        const Pixel tangentB = sampler.sample(srcX - tangentX * tangentRadius, srcY - tangentY * tangentRadius);
        const Pixel opticalBlur{
            (center.r * 2.0f + radialA.r + radialB.r + tangentA.r + tangentB.r) / 6.0f,
            (center.g * 2.0f + radialA.g + radialB.g + tangentA.g + tangentB.g) / 6.0f,
            (center.b * 2.0f + radialA.b + radialB.b + tangentA.b + tangentB.b) / 6.0f,
            dry.a,
        };
        const float blurMix = clamp01(
            (curvatureBlur + astigBlur + sphericalBlur) * 0.22f);
        result = mix(result, opticalBlur, blurMix);
    }

    const float axialStrength =
        model.axialCA * state.amount * state.apertureScale * (1.0f - guard * 0.72f);
    if (axialStrength > 0.0001f) {
        const float axialRadius = 1.0f + axialStrength * 5.0f;
        const Pixel a = sampler.sample(srcX - axialRadius, srcY);
        const Pixel b = sampler.sample(srcX + axialRadius, srcY);
        const Pixel q = sampler.sample(srcX, srcY - axialRadius);
        const Pixel d = sampler.sample(srcX, srcY + axialRadius);
        const Pixel blur{
            (a.r + b.r + q.r + d.r) * 0.25f,
            (a.g + b.g + q.g + d.g) * 0.25f,
            (a.b + b.b + q.b + d.b) * 0.25f,
            dry.a,
        };
        result.r = result.r * (1.0f - axialStrength * 0.22f) + blur.r * axialStrength * 0.22f;
        result.b = result.b * (1.0f - axialStrength * 0.34f) + blur.b * axialStrength * 0.34f;
    }

    const float comaStrength =
        model.coma * state.amount * edge2 * state.apertureScale * (1.0f - guard * 0.88f);
    if (comaStrength > 0.0001f) {
        Pixel comet{0.0f, 0.0f, 0.0f, dry.a};
        float weightSum = 0.0f;
        for (int i = 1; i <= 4; ++i) {
            const float fi = static_cast<float>(i);
            const float spread = fi * (1.0f + comaStrength * 4.5f);
            const Pixel p = sampler.sample(
                srcX + tangentX * spread + dirX * spread * 0.48f,
                srcY + tangentY * spread + dirY * spread * 0.48f);
            const float hot = smoothstep(c.bloomThreshold * 0.72f, c.bloomThreshold + 0.75f, luma(p));
            const float weight = hot / fi;
            comet.r += p.r * weight;
            comet.g += p.g * weight;
            comet.b += p.b * weight;
            weightSum += weight;
        }
        if (weightSum > 0.0001f) {
            const float amount = comaStrength * 0.34f / weightSum;
            result.r += comet.r * amount;
            result.g += comet.g * amount * 0.86f;
            result.b += comet.b * amount * 0.72f;
        }
    }

    float depthError = std::fabs(c.focusOffset);
    if (c.depthSource == 1) {
        float depth = clamp01(
            (clamp01(dry.a) - c.depthNear) /
            (c.depthFar - c.depthNear));
        if (c.depthInvert) {
            depth = 1.0f - depth;
        }
        depth = std::pow(
            std::max(0.000001f, depth),
            c.depthGamma);
        depthError =
            std::fabs(depth - c.focusPlane);
    }
    const float defocusStrength =
        model.defocus * state.amount * state.apertureScale *
        clamp01(depthError) * (1.0f - guard * 0.55f);
    if (defocusStrength > 0.0005f) {
        const float radiusPx = std::min(18.0f, 0.75f + defocusStrength * 16.0f);
        const float squeeze = c.anamorphicSqueeze;
        const float catEye = model.catEye * edge2;
        const float horizontal = radiusPx * squeeze * (1.0f - catEye * 0.18f);
        const float vertical = radiusPx / squeeze * (1.0f - catEye * 0.52f);
        static constexpr float kOffsets[12][2] = {
            {1.000f, 0.000f}, {-1.000f, 0.000f}, {0.000f, 1.000f}, {0.000f, -1.000f},
            {0.707f, 0.707f}, {-0.707f, 0.707f}, {0.707f, -0.707f}, {-0.707f, -0.707f},
            {0.383f, 0.924f}, {-0.383f, 0.924f}, {0.383f, -0.924f}, {-0.383f, -0.924f},
        };
        Pixel blur = mul(center, 2.0f);
        float weightSum = 2.0f;
        for (const auto& offset : kOffsets) {
            const float ellipseX = offset[0] * horizontal;
            const float ellipseY = offset[1] * vertical;
            const float ox =
                ellipseX * state.anamorphicCos -
                ellipseY * state.anamorphicSin -
                dirX * catEye * radiusPx * 0.45f;
            const float oy =
                ellipseX * state.anamorphicSin +
                ellipseY * state.anamorphicCos -
                dirY * catEye * radiusPx * 0.45f;
            const Pixel p = sampler.sample(srcX + ox, srcY + oy);
            float weight = 1.0f;
            if (assets && assets->aperture.valid()) {
                const float apertureWeight = assets->aperture.sample(
                    offset[0] * 0.5f + 0.5f,
                    offset[1] * 0.5f + 0.5f);
                weight =
                    1.0f +
                    (0.15f + apertureWeight * 1.70f - 1.0f) *
                    c.apertureInfluence;
            }
            blur.r += p.r * weight;
            blur.g += p.g * weight;
            blur.b += p.b * weight;
            weightSum += weight;
        }
        blur = mul(blur, 1.0f / weightSum);
        result = mix(result, blur, clamp01(defocusStrength * 1.24f));
    }

    const float bloomStrength = model.bloom * state.amount;
    const float diffusionStrength = model.diffusion * state.amount;
    const float halationStrength = model.halation * state.amount;
    if (bloomStrength + diffusionStrength + halationStrength > 0.0001f) {
        const float radiusPx = 2.0f + diffusionStrength * 10.0f + bloomStrength * 6.0f;
        static constexpr float kOffsets[8][2] = {
            {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
            {0.707f, 0.707f}, {-0.707f, 0.707f}, {0.707f, -0.707f}, {-0.707f, -0.707f},
        };
        Pixel glow{0.0f, 0.0f, 0.0f, dry.a};
        float hotWeight = 0.0f;
        for (const auto& offset : kOffsets) {
            const Pixel p = sampler.sample(
                srcX + offset[0] * radiusPx,
                srcY + offset[1] * radiusPx);
            const float hot = smoothstep(c.bloomThreshold, c.bloomThreshold + 0.85f, luma(p));
            glow.r += p.r * hot;
            glow.g += p.g * hot;
            glow.b += p.b * hot;
            hotWeight += hot;
        }
        if (hotWeight > 0.0001f) {
            glow = mul(glow, 1.0f / hotWeight);
            const float bloomAmount = bloomStrength * 0.34f;
            result.r += glow.r * bloomAmount;
            result.g += glow.g * bloomAmount;
            result.b += glow.b * bloomAmount;

            const float diffuseMix = clamp01(diffusionStrength * 0.42f);
            result = mix(result, glow, diffuseMix);

            const float halo = halationStrength * 0.23f;
            result.r += glow.r * halo;
            result.g += glow.g * halo * 0.34f;
            result.b += glow.b * halo * 0.10f;
        }
    }

    const float ghostStrength = model.flareGhosts * state.amount;
    if (ghostStrength > 0.0001f) {
        const float ghostX = cx - (srcX - cx) * (0.42f + edge * 0.28f);
        const float ghostY = cy - (srcY - cy) * (0.42f + edge * 0.28f);
        const Pixel ghost = sampler.sample(ghostX, ghostY);
        const float hot = smoothstep(c.bloomThreshold, c.bloomThreshold + 1.0f, luma(ghost));
        const float amount = hot * ghostStrength * 0.22f;
        result.r += ghost.r * amount * (1.05f + model.warmth);
        result.g += ghost.g * amount * 0.74f;
        result.b += ghost.b * amount * (0.90f - model.warmth * 0.5f);
    }

    const float streakStrength = model.flareStreak * state.amount;
    if (streakStrength > 0.0001f) {
        const float streakRadius = 5.0f + streakStrength * 28.0f;
        const float streakX =
            state.anamorphicCos * streakRadius;
        const float streakY =
            state.anamorphicSin * streakRadius;
        const Pixel left = sampler.sample(
            srcX - streakX,
            srcY - streakY);
        const Pixel right = sampler.sample(
            srcX + streakX,
            srcY + streakY);
        const float hotLeft = smoothstep(c.bloomThreshold, c.bloomThreshold + 0.9f, luma(left));
        const float hotRight = smoothstep(c.bloomThreshold, c.bloomThreshold + 0.9f, luma(right));
        const float amount = streakStrength * 0.15f;
        result.r += (left.r * hotLeft + right.r * hotRight) * amount * 0.30f;
        result.g += (left.g * hotLeft + right.g * hotRight) * amount * 0.55f;
        result.b += (left.b * hotLeft + right.b * hotRight) * amount;
    }

    const float starStrength = model.starburst * state.amount;
    if (starStrength > 0.0001f) {
        const float starRadius = 4.0f + starStrength * 18.0f;
        const Pixel horizontalA = sampler.sample(srcX - starRadius, srcY);
        const Pixel horizontalB = sampler.sample(srcX + starRadius, srcY);
        const Pixel verticalA = sampler.sample(srcX, srcY - starRadius);
        const Pixel verticalB = sampler.sample(srcX, srcY + starRadius);
        const Pixel star{
            horizontalA.r + horizontalB.r + verticalA.r + verticalB.r,
            horizontalA.g + horizontalB.g + verticalA.g + verticalB.g,
            horizontalA.b + horizontalB.b + verticalA.b + verticalB.b,
            dry.a,
        };
        const float hot = smoothstep(c.bloomThreshold * 4.0f, c.bloomThreshold * 4.0f + 2.0f, luma(star));
        const float amount = starStrength * hot * 0.055f;
        result.r += star.r * amount;
        result.g += star.g * amount;
        result.b += star.b * amount;
    }

    const float sensorStrength = model.debayer * state.amount;
    const float chromaStrength = model.chromaSmear * state.amount;
    if (sensorStrength + chromaStrength > 0.0001f) {
        const float sensorRadius = 1.0f + 1.8f * sensorStrength;
        const Pixel a = sampler.sample(srcX - sensorRadius, srcY);
        const Pixel b = sampler.sample(srcX + sensorRadius, srcY);
        const Pixel chromaBlur{
            (a.r + b.r) * 0.5f,
            center.g,
            (a.b + b.b) * 0.5f,
            dry.a,
        };
        result.r = result.r * (1.0f - sensorStrength * 0.30f) + chromaBlur.r * sensorStrength * 0.30f;
        result.b = result.b * (1.0f - sensorStrength * 0.36f) + chromaBlur.b * sensorStrength * 0.36f;

        const float yValue = luma(result);
        const float smearMix = clamp01(chromaStrength * 0.32f);
        result.r = result.r * (1.0f - smearMix) + (yValue + (chromaBlur.r - luma(chromaBlur)) * 0.72f) * smearMix;
        result.b = result.b * (1.0f - smearMix) + (yValue + (chromaBlur.b - luma(chromaBlur)) * 0.72f) * smearMix;
    }

    const float vignette = model.vignette * state.amount * edge2;
    const float vignetteGain = std::max(0.0f, 1.0f - vignette * 0.58f);
    result.r *= vignetteGain;
    result.g *= vignetteGain;
    result.b *= vignetteGain;

    if (assets && assets->dirt.valid() && c.dirtAmount > 0.0001f) {
        const float scale = c.dirtScale;
        const float u = std::fmod((static_cast<float>(x) + 0.5f) / state.width * scale, 1.0f);
        const float v = std::fmod((static_cast<float>(y) + 0.5f) / state.height * scale, 1.0f);
        const float dirt = clamp01(assets->dirt.sample(u, v));
        const float hot = smoothstep(c.bloomThreshold * 0.72f, c.bloomThreshold + 0.90f, dryY);
        const float dirtMask = dirt * c.dirtAmount * state.amount;
        const float transmission = 1.0f - dirtMask * (0.10f + hot * 0.24f);
        result.r *= transmission;
        result.g *= transmission;
        result.b *= transmission;
        result.r += dirtMask * hot * 0.030f * (1.0f + model.warmth);
        result.g += dirtMask * hot * 0.018f;
        result.b += dirtMask * hot * 0.010f;
    }

    if (assets && assets->smudge.valid() && c.smudgeAmount > 0.0001f) {
        const float scale = c.smudgeScale;
        const float u = std::fmod((static_cast<float>(x) + 0.5f) / state.width * scale, 1.0f);
        const float v = std::fmod((static_cast<float>(y) + 0.5f) / state.height * scale, 1.0f);
        const float smudge = clamp01(assets->smudge.sample(u, v));
        const float hot = smoothstep(c.bloomThreshold * 0.68f, c.bloomThreshold + 0.82f, dryY);
        const float smudgeMask =
            smudge * c.smudgeAmount * state.amount;
        const float transmission =
            1.0f - smudgeMask * (0.07f + hot * 0.18f);
        result.r *= transmission;
        result.g *= transmission;
        result.b *= transmission;
        result.r +=
            smudgeMask * hot * 0.024f * (1.0f + model.warmth * 0.5f);
        result.g += smudgeMask * hot * 0.019f;
        result.b += smudgeMask * hot * 0.016f;
    }

    const float grainStrength =
        model.grain * state.amount * state.isoGrainScale;
    if (grainStrength > 0.0001f) {
        const float size = c.grainSize;
        const int grainX = static_cast<int>(static_cast<float>(x) / size);
        const int grainY = static_cast<int>(static_cast<float>(y) / size);
        const float mono = hashNoise(grainX, grainY, state.frameIndex, c.grainSeed);
        const float animated = hashNoise(grainX + 13, grainY - 7, state.frameIndex, c.grainSeed + 17.0f);
        const float noise = (mono * 0.72f + animated * 0.28f);
        const float grainMask = 0.38f + 0.62f * (1.0f - smoothstep(0.18f, 1.20f, std::max(0.0f, dryY)));
        const float amount = grainStrength * grainMask * 0.055f;
        result.r += noise * amount * 1.03f;
        result.g += noise * amount;
        result.b += noise * amount * 1.08f;
    }

    const float finalMix = c.outputMix;
    result = mix(dry, result, finalMix);
    result.a = dry.a;
    return sanitize(result, dry);
}

} // namespace buckswood_optics
