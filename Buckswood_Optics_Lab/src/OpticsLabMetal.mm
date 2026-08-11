#import <Metal/Metal.h>

#include "OpticsLabMetal.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace buckswood_optics {
namespace {

const char* kOpticsLabMetalSource = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct Params {
    int srcRowPixels;
    int dstRowPixels;
    int srcX1;
    int srcY1;
    int srcX2;
    int srcY2;
    int dstX1;
    int dstY1;
    int renderX1;
    int renderY1;
    int renderWidth;
    int renderHeight;
    int frameIndex;
    int depthSource;
    int depthInvert;
    int needsEdgeGuard;
    int identityMapping;
    int identityOutput;
    int apertureWidth;
    int apertureHeight;
    int dirtWidth;
    int dirtHeight;
    int smudgeWidth;
    int smudgeHeight;
    int defocusSamples;
    int glowSamples;
    int comaSamples;
    int irisBlades;
    int maxStarSpokes;

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

    float bloomThreshold;
    float depthNear;
    float depthFar;
    float depthGamma;
    float focusPlane;
    float focusOffset;
    float anamorphicSqueeze;
    float apertureInfluence;
    float dirtAmount;
    float dirtScale;
    float smudgeAmount;
    float smudgeScale;
    float grainSize;
    float grainSeed;
    float edgeGuard;
    float outputMix;

    float width;
    float height;
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
    float fStop;
    float irisRoundness;
    float irisRotation;
    float starUnevenness;
    float starGate;
};

inline float clamp01(float value)
{
    return min(1.0f, max(0.0f, value));
}

inline float opticsSmoothstep(float edge0, float edge1, float value)
{
    const float t =
        clamp01((value - edge0) / max(0.00001f, edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

inline float opticsLuma(float4 pixel)
{
    return 0.2627f * pixel.r + 0.6780f * pixel.g + 0.0593f * pixel.b;
}

inline float4 mixPixel(float4 a, float4 b, float amount)
{
    const float inverse = 1.0f - amount;
    return float4(
        a.r * inverse + b.r * amount,
        a.g * inverse + b.g * amount,
        a.b * inverse + b.b * amount,
        a.a * inverse + b.a * amount);
}

inline float4 sampleSource(
    device const float4* source,
    float x,
    float y,
    constant Params& p)
{
    const float safeX = clamp(
        x,
        float(p.srcX1),
        float(p.srcX2 - 1));
    const float safeY = clamp(
        y,
        float(p.srcY1),
        float(p.srcY2 - 1));
    const int x0 = int(floor(safeX));
    const int y0 = int(floor(safeY));
    const int x1 = min(p.srcX2 - 1, x0 + 1);
    const int y1 = min(p.srcY2 - 1, y0 + 1);
    const float tx = safeX - float(x0);
    const float ty = safeY - float(y0);
    const float4 p00 =
        source[(y0 - p.srcY1) * p.srcRowPixels + (x0 - p.srcX1)];
    const float4 p10 =
        source[(y0 - p.srcY1) * p.srcRowPixels + (x1 - p.srcX1)];
    const float4 p01 =
        source[(y1 - p.srcY1) * p.srcRowPixels + (x0 - p.srcX1)];
    const float4 p11 =
        source[(y1 - p.srcY1) * p.srcRowPixels + (x1 - p.srcX1)];
    const float u = 1.0f - tx;
    const float v = 1.0f - ty;
    return float4(
        (p00.r * u + p10.r * tx) * v +
            (p01.r * u + p11.r * tx) * ty,
        (p00.g * u + p10.g * tx) * v +
            (p01.g * u + p11.g * tx) * ty,
        (p00.b * u + p10.b * tx) * v +
            (p01.b * u + p11.b * tx) * ty,
        (p00.a * u + p10.a * tx) * v +
            (p01.a * u + p11.a * tx) * ty);
}

inline float sampleTexture(
    device const float* texture,
    int width,
    int height,
    float u,
    float v)
{
    if (width <= 0 || height <= 0) {
        return 1.0f;
    }
    const float cu = clamp01(u);
    const float cv = clamp01(v);
    const int x = min(
        width - 1,
        int(cu * float(width - 1) + 0.5f));
    const int y = min(
        height - 1,
        int(cv * float(height - 1) + 0.5f));
    return texture[y * width + x];
}

inline float hashNoise(int x, int y, int frame, float seed)
{
    uint h = uint(x) * 0x8da6b343u;
    h ^= uint(y) * 0xd8163841u;
    h ^= uint(frame) * 0xcb1ab31fu;
    h ^= uint(seed * 4096.0f + 0.5f) * 0x165667b1u;
    h ^= h >> 13;
    h *= 0x85ebca6bu;
    h ^= h >> 16;
    return float(h & 0x00ffffffu) / 8388607.5f - 1.0f;
}

inline float proceduralIris(
    float x,
    float y,
    int blades,
    float roundness,
    float rotation)
{
    const float radius = sqrt(x * x + y * y);
    if (blades < 3) {
        return clamp01(1.0f - opticsSmoothstep(0.88f, 1.02f, radius));
    }
    const float pi = 3.14159265358979323846f;
    const float sector = 2.0f * pi / float(blades);
    float theta = atan2(y, x) - rotation;
    theta -= sector * floor(theta / sector + 0.5f);
    const float polygonRadius =
        cos(pi / float(blades)) / max(0.05f, cos(theta));
    const float boundary =
        polygonRadius * (1.0f - roundness) + roundness;
    return clamp01(
        1.0f - opticsSmoothstep(boundary * 0.82f, boundary, radius));
}

inline float4 sanitizePixel(float4 pixel, float4 fallback)
{
    return float4(
        isfinite(pixel.r) ? pixel.r : fallback.r,
        isfinite(pixel.g) ? pixel.g : fallback.g,
        isfinite(pixel.b) ? pixel.b : fallback.b,
        fallback.a);
}

inline float4 processOpticsPixel(
    device const float4* source,
    device const float* aperture,
    device const float* dirtTexture,
    device const float* smudgeTexture,
    int x,
    int y,
    constant Params& p)
{
    const float4 dry = sampleSource(source, float(x), float(y), p);
    if (p.amount <= 0.000001f ||
        p.outputMix <= 0.000001f ||
        p.identityOutput != 0) {
        return dry;
    }

    const float cx = p.centerX;
    const float cy = p.centerY;
    const float nx = (float(x) - cx) / max(1.0f, cx);
    const float ny = (float(y) - cy) / max(1.0f, cy);
    const float ax = nx * p.aspect;
    const float radius = sqrt(ax * ax + ny * ny);
    const float edge = opticsSmoothstep(0.12f, 1.10f, radius);
    const float edge2 = edge * edge;

    float dirX = nx;
    float dirY = ny;
    const float dirLength = sqrt(dirX * dirX + dirY * dirY);
    if (dirLength > 0.0001f) {
        dirX /= dirLength;
        dirY /= dirLength;
    }
    const float tangentX = -dirY;
    const float tangentY = dirX;

    const float dryY = opticsLuma(dry);
    float guard = 0.0f;
    if (p.needsEdgeGuard != 0) {
        const float4 dryLeft = sampleSource(source, float(x - 1), float(y), p);
        const float4 dryRight = sampleSource(source, float(x + 1), float(y), p);
        const float4 dryUp = sampleSource(source, float(x), float(y - 1), p);
        const float4 dryDown = sampleSource(source, float(x), float(y + 1), p);
        const float localGradient =
            abs(opticsLuma(dryRight) - opticsLuma(dryLeft)) +
            abs(opticsLuma(dryDown) - opticsLuma(dryUp));
        const float contourRisk =
            opticsSmoothstep(0.045f, 0.42f, localGradient);
        guard = p.edgeGuard * contourRisk;
    }

    const float swirlAngle = p.swirl * p.amount * edge2 * 0.24f;
    float sx = nx;
    float sy = ny;
    if (abs(swirlAngle) > 0.000001f) {
        const float cosAngle = cos(swirlAngle);
        const float sinAngle = sin(swirlAngle);
        sx = nx * cosAngle - ny * sinAngle;
        sy = nx * sinAngle + ny * cosAngle;
    }

    const float r2 = radius * radius;
    const float radial =
        1.0f +
        p.distortion * p.amount * r2 +
        p.distortion * p.amount * 0.28f * r2 * r2;
    const float mappingScale = radial * p.breathingScale;
    const float fullSrcX = cx + sx * mappingScale * cx;
    const float fullSrcY = cy + sy * mappingScale * cy;
    const float srcX =
        float(x) + (fullSrcX - float(x)) * p.outputMix;
    const float srcY =
        float(y) + (fullSrcY - float(y)) * p.outputMix;
    const float4 center =
        p.identityMapping != 0
        ? dry
        : sampleSource(source, srcX, srcY, p);

    const float caPixels =
        p.lateralCA * p.amount * edge2 *
        (1.2f + 2.6f * p.focalScale) *
        (1.0f - guard * 0.82f);
    float4 result = center;
    result.a = dry.a;
    if (caPixels > 0.0001f) {
        const float4 redSample = sampleSource(
            source,
            srcX + dirX * caPixels,
            srcY + dirY * caPixels,
            p);
        const float4 blueSample = sampleSource(
            source,
            srcX - dirX * caPixels,
            srcY - dirY * caPixels,
            p);
        result.r = redSample.r;
        result.b = blueSample.b;
    }

    const float curvatureBlur =
        p.fieldCurvature * p.amount * edge2 *
        (1.0f - guard * 0.78f) * 4.0f;
    const float astigBlur =
        p.astigmatism * p.amount * edge2 *
        (1.0f - guard * 0.78f) * 3.2f;
    const float sphericalBlur =
        p.spherical * p.amount * (0.30f + edge * 0.70f) *
        (1.0f - guard * 0.72f) * 2.8f;
    if (curvatureBlur + astigBlur + sphericalBlur > 0.001f) {
        const float radialRadius = curvatureBlur + sphericalBlur;
        const float tangentRadius = astigBlur + sphericalBlur * 0.72f;
        const float4 radialA = sampleSource(
            source,
            srcX + dirX * radialRadius,
            srcY + dirY * radialRadius,
            p);
        const float4 radialB = sampleSource(
            source,
            srcX - dirX * radialRadius,
            srcY - dirY * radialRadius,
            p);
        const float4 tangentA = sampleSource(
            source,
            srcX + tangentX * tangentRadius,
            srcY + tangentY * tangentRadius,
            p);
        const float4 tangentB = sampleSource(
            source,
            srcX - tangentX * tangentRadius,
            srcY - tangentY * tangentRadius,
            p);
        const float4 opticalBlur = float4(
            (center.r * 2.0f + radialA.r + radialB.r + tangentA.r + tangentB.r) / 6.0f,
            (center.g * 2.0f + radialA.g + radialB.g + tangentA.g + tangentB.g) / 6.0f,
            (center.b * 2.0f + radialA.b + radialB.b + tangentA.b + tangentB.b) / 6.0f,
            dry.a);
        const float blurMix = clamp01(
            (curvatureBlur + astigBlur + sphericalBlur) * 0.22f);
        result = mixPixel(result, opticalBlur, blurMix);
    }

    const float axialStrength =
        p.axialCA * p.amount * p.apertureScale *
        (1.0f - guard * 0.72f);
    if (axialStrength > 0.0001f) {
        const float axialRadius = 1.0f + axialStrength * 5.0f;
        const float4 a = sampleSource(source, srcX - axialRadius, srcY, p);
        const float4 b = sampleSource(source, srcX + axialRadius, srcY, p);
        const float4 q = sampleSource(source, srcX, srcY - axialRadius, p);
        const float4 d = sampleSource(source, srcX, srcY + axialRadius, p);
        const float4 blur = (a + b + q + d) * 0.25f;
        result.r =
            result.r * (1.0f - axialStrength * 0.22f) +
            blur.r * axialStrength * 0.22f;
        result.b =
            result.b * (1.0f - axialStrength * 0.34f) +
            blur.b * axialStrength * 0.34f;
    }

    const float comaStrength =
        p.coma * p.amount * edge2 * p.apertureScale *
        (1.0f - guard * 0.88f);
    if (comaStrength > 0.0001f) {
        float3 comet = float3(0.0f);
        float weightSum = 0.0f;
        for (int i = 1; i <= p.comaSamples; ++i) {
            const float fi = float(i);
            const float spread = fi * (1.0f + comaStrength * 4.5f);
            const float4 sample = sampleSource(
                source,
                srcX + tangentX * spread + dirX * spread * 0.48f,
                srcY + tangentY * spread + dirY * spread * 0.48f,
                p);
            const float hot = opticsSmoothstep(
                p.bloomThreshold * 0.72f,
                p.bloomThreshold + 0.75f,
                opticsLuma(sample));
            const float weight = hot / fi;
            comet += sample.rgb * weight;
            weightSum += weight;
        }
        if (weightSum > 0.0001f) {
            const float amount = comaStrength * 0.34f / weightSum;
            result.r += comet.r * amount;
            result.g += comet.g * amount * 0.86f;
            result.b += comet.b * amount * 0.72f;
        }
    }

    float signedDepthError = p.focusOffset;
    if (p.depthSource == 1) {
        float depth = clamp01(
            (clamp01(dry.a) - p.depthNear) /
            (p.depthFar - p.depthNear));
        if (p.depthInvert != 0) {
            depth = 1.0f - depth;
        }
        depth = pow(max(0.000001f, depth), p.depthGamma);
        signedDepthError = depth - p.focusPlane;
    }
    const float depthError = abs(signedDepthError);
    const float foregroundMirror = signedDepthError < 0.0f ? -1.0f : 1.0f;
    const float defocusStrength =
        p.defocus * p.amount * p.apertureScale *
        clamp01(depthError) * (1.0f - guard * 0.55f);
    if (defocusStrength > 0.0005f) {
        const float radiusPx =
            min(18.0f, 0.75f + defocusStrength * 16.0f);
        const float catEye = p.catEye * edge2;
        const float horizontal =
            radiusPx * p.anamorphicSqueeze *
            (1.0f - catEye * 0.18f);
        const float vertical =
            radiusPx / p.anamorphicSqueeze *
            (1.0f - catEye * 0.52f);
        const float2 offsets[12] = {
            float2(1.000f, 0.000f), float2(-1.000f, 0.000f),
            float2(0.000f, 1.000f), float2(0.000f, -1.000f),
            float2(0.707f, 0.707f), float2(-0.707f, 0.707f),
            float2(0.707f, -0.707f), float2(-0.707f, -0.707f),
            float2(0.383f, 0.924f), float2(-0.383f, 0.924f),
            float2(0.383f, -0.924f), float2(-0.383f, -0.924f)
        };
        float3 blur = center.rgb * 2.0f;
        float weightSum = 2.0f;
        for (int i = 0; i < p.defocusSamples; ++i) {
            const float ellipseX = offsets[i].x * horizontal;
            const float ellipseY = offsets[i].y * vertical;
            const float ox =
                ellipseX * p.anamorphicCos -
                ellipseY * p.anamorphicSin -
                dirX * catEye * radiusPx * 0.45f * foregroundMirror;
            const float oy =
                ellipseX * p.anamorphicSin +
                ellipseY * p.anamorphicCos -
                dirY * catEye * radiusPx * 0.45f * foregroundMirror;
            const float4 sample = sampleSource(
                source,
                srcX + ox,
                srcY + oy,
                p);
            float weight = 1.0f;
            if (p.apertureWidth > 0) {
                const float apertureWeight = sampleTexture(
                    aperture,
                    p.apertureWidth,
                    p.apertureHeight,
                    offsets[i].x * 0.5f + 0.5f,
                    offsets[i].y * 0.5f + 0.5f);
                weight =
                    1.0f +
                    (0.15f + apertureWeight * 1.70f - 1.0f) *
                    p.apertureInfluence;
            } else if (p.irisBlades >= 3) {
                const float apertureWeight = proceduralIris(
                    offsets[i].x,
                    offsets[i].y,
                    p.irisBlades,
                    p.irisRoundness,
                    p.irisRotation +
                        (foregroundMirror < 0.0f ? 3.14159265358979323846f : 0.0f));
                weight =
                    1.0f +
                    (0.15f + apertureWeight * 1.70f - 1.0f) *
                    p.apertureInfluence;
            }
            blur += sample.rgb * weight;
            weightSum += weight;
        }
        blur *= 1.0f / weightSum;
        result = mixPixel(
            result,
            float4(blur, dry.a),
            clamp01(defocusStrength * 1.24f));
    }

    const float bloomStrength = p.bloom * p.amount;
    const float diffusionStrength = p.diffusion * p.amount;
    const float halationStrength = p.halation * p.amount;
    if (bloomStrength + diffusionStrength + halationStrength > 0.0001f) {
        const float radiusPx =
            2.0f + diffusionStrength * 10.0f + bloomStrength * 6.0f;
        const float2 offsets[8] = {
            float2(1.0f, 0.0f), float2(-1.0f, 0.0f),
            float2(0.0f, 1.0f), float2(0.0f, -1.0f),
            float2(0.707f, 0.707f), float2(-0.707f, 0.707f),
            float2(0.707f, -0.707f), float2(-0.707f, -0.707f)
        };
        float3 glow = float3(0.0f);
        float hotWeight = 0.0f;
        for (int i = 0; i < p.glowSamples; ++i) {
            const float4 sample = sampleSource(
                source,
                srcX + offsets[i].x * radiusPx,
                srcY + offsets[i].y * radiusPx,
                p);
            const float hot = opticsSmoothstep(
                p.bloomThreshold,
                p.bloomThreshold + 0.85f,
                opticsLuma(sample));
            glow += sample.rgb * hot;
            hotWeight += hot;
        }
        if (hotWeight > 0.0001f) {
            glow *= 1.0f / hotWeight;
            const float bloomAmount = bloomStrength * 0.34f;
            result.rgb += glow * bloomAmount;
            result = mixPixel(
                result,
                float4(glow, dry.a),
                clamp01(diffusionStrength * 0.42f));
            const float halo = halationStrength * 0.23f;
            result.r += glow.r * halo;
            result.g += glow.g * halo * 0.34f;
            result.b += glow.b * halo * 0.10f;
        }
    }

    const float ghostStrength = p.flareGhosts * p.amount;
    if (ghostStrength > 0.0001f) {
        const float ghostScale = 0.42f + edge * 0.28f;
        const float ghostX = cx - (srcX - cx) * ghostScale;
        const float ghostY = cy - (srcY - cy) * ghostScale;
        const float4 ghost = sampleSource(source, ghostX, ghostY, p);
        const float hot = opticsSmoothstep(
            p.bloomThreshold,
            p.bloomThreshold + 1.0f,
            opticsLuma(ghost));
        const float amount = hot * ghostStrength * 0.22f;
        result.r += ghost.r * amount * (1.05f + p.warmth);
        result.g += ghost.g * amount * 0.74f;
        result.b += ghost.b * amount * (0.90f - p.warmth * 0.5f);
    }

    const float streakStrength = p.flareStreak * p.amount;
    if (streakStrength > 0.0001f) {
        const float streakRadius = 5.0f + streakStrength * 28.0f;
        const float streakX = p.anamorphicCos * streakRadius;
        const float streakY = p.anamorphicSin * streakRadius;
        const float4 left = sampleSource(
            source,
            srcX - streakX,
            srcY - streakY,
            p);
        const float4 right = sampleSource(
            source,
            srcX + streakX,
            srcY + streakY,
            p);
        const float hotLeft = opticsSmoothstep(
            p.bloomThreshold,
            p.bloomThreshold + 0.9f,
            opticsLuma(left));
        const float hotRight = opticsSmoothstep(
            p.bloomThreshold,
            p.bloomThreshold + 0.9f,
            opticsLuma(right));
        const float amount = streakStrength * 0.15f;
        result.r +=
            (left.r * hotLeft + right.r * hotRight) *
            amount * 0.30f;
        result.g +=
            (left.g * hotLeft + right.g * hotRight) *
            amount * 0.55f;
        result.b +=
            (left.b * hotLeft + right.b * hotRight) * amount;
    }

    const float physicalStarGate = opticsSmoothstep(8.0f, 22.0f, p.fStop);
    const float starResponse =
        (1.0f - p.starGate) + p.starGate * physicalStarGate;
    const float starStrength = p.starburst * p.amount * starResponse;
    if (starStrength > 0.0001f) {
        const float starRadius = 4.0f + starStrength * 18.0f;
        int spokeCount = 4;
        if (p.irisBlades >= 3) {
            spokeCount = p.irisBlades % 2 == 0
                ? p.irisBlades
                : p.irisBlades * 2;
            spokeCount = min(spokeCount, p.maxStarSpokes);
        }
        float3 star = float3(0.0f);
        for (int spoke = 0; spoke < spokeCount; ++spoke) {
            const float normalized = float(spoke) / float(max(1, spokeCount));
            const float angle =
                p.irisRotation + normalized * 6.28318530717958647692f;
            const float uneven = 1.0f + p.starUnevenness *
                sin(float(spoke * 17 + 3)) * 0.28f;
            star += sampleSource(
                source,
                srcX + cos(angle) * starRadius * uneven,
                srcY + sin(angle) * starRadius * uneven,
                p).rgb;
        }
        const float hot = opticsSmoothstep(
            p.bloomThreshold * 4.0f,
            p.bloomThreshold * 4.0f + 2.0f,
            dot(star, float3(0.2627f, 0.6780f, 0.0593f)));
        const float amount =
            starStrength * hot * 0.22f / float(max(1, spokeCount));
        result.rgb += star * amount;
    }

    const float sensorStrength = p.debayer * p.amount;
    const float chromaStrength = p.chromaSmear * p.amount;
    if (sensorStrength + chromaStrength > 0.0001f) {
        const float sensorRadius = 1.0f + 1.8f * sensorStrength;
        const float4 a =
            sampleSource(source, srcX - sensorRadius, srcY, p);
        const float4 b =
            sampleSource(source, srcX + sensorRadius, srcY, p);
        const float4 chromaBlur = float4(
            (a.r + b.r) * 0.5f,
            center.g,
            (a.b + b.b) * 0.5f,
            dry.a);
        result.r =
            result.r * (1.0f - sensorStrength * 0.30f) +
            chromaBlur.r * sensorStrength * 0.30f;
        result.b =
            result.b * (1.0f - sensorStrength * 0.36f) +
            chromaBlur.b * sensorStrength * 0.36f;
        const float yValue = opticsLuma(result);
        const float smearMix = clamp01(chromaStrength * 0.32f);
        const float chromaY = opticsLuma(chromaBlur);
        result.r =
            result.r * (1.0f - smearMix) +
            (yValue + (chromaBlur.r - chromaY) * 0.72f) * smearMix;
        result.b =
            result.b * (1.0f - smearMix) +
            (yValue + (chromaBlur.b - chromaY) * 0.72f) * smearMix;
    }

    const float vignette = p.vignette * p.amount * edge2;
    const float vignetteGain = max(0.0f, 1.0f - vignette * 0.58f);
    result.rgb *= vignetteGain;

    if (p.dirtWidth > 0 && p.dirtAmount > 0.0001f) {
        const float u = fmod((float(x) + 0.5f) / p.width * p.dirtScale, 1.0f);
        const float v = fmod((float(y) + 0.5f) / p.height * p.dirtScale, 1.0f);
        const float dirt = clamp01(sampleTexture(
            dirtTexture,
            p.dirtWidth,
            p.dirtHeight,
            u,
            v));
        const float hot = opticsSmoothstep(
            p.bloomThreshold * 0.72f,
            p.bloomThreshold + 0.90f,
            dryY);
        const float dirtMask = dirt * p.dirtAmount * p.amount;
        const float transmission =
            1.0f - dirtMask * (0.10f + hot * 0.24f);
        result.rgb *= transmission;
        result.r +=
            dirtMask * hot * 0.030f * (1.0f + p.warmth);
        result.g += dirtMask * hot * 0.018f;
        result.b += dirtMask * hot * 0.010f;
    }

    if (p.smudgeWidth > 0 && p.smudgeAmount > 0.0001f) {
        const float u = fmod((float(x) + 0.5f) / p.width * p.smudgeScale, 1.0f);
        const float v = fmod((float(y) + 0.5f) / p.height * p.smudgeScale, 1.0f);
        const float smudge = clamp01(sampleTexture(
            smudgeTexture,
            p.smudgeWidth,
            p.smudgeHeight,
            u,
            v));
        const float hot = opticsSmoothstep(
            p.bloomThreshold * 0.68f,
            p.bloomThreshold + 0.82f,
            dryY);
        const float smudgeMask =
            smudge * p.smudgeAmount * p.amount;
        const float transmission =
            1.0f - smudgeMask * (0.07f + hot * 0.18f);
        result.rgb *= transmission;
        result.r +=
            smudgeMask * hot * 0.024f *
            (1.0f + p.warmth * 0.5f);
        result.g += smudgeMask * hot * 0.019f;
        result.b += smudgeMask * hot * 0.016f;
    }

    const float grainStrength = p.grain * p.amount * p.isoGrainScale;
    if (grainStrength > 0.0001f) {
        const int grainX = int(float(x) / p.grainSize);
        const int grainY = int(float(y) / p.grainSize);
        const float mono =
            hashNoise(grainX, grainY, p.frameIndex, p.grainSeed);
        const float animated = hashNoise(
            grainX + 13,
            grainY - 7,
            p.frameIndex,
            p.grainSeed + 17.0f);
        const float noise = mono * 0.72f + animated * 0.28f;
        const float grainMask =
            0.38f +
            0.62f *
                (1.0f - opticsSmoothstep(
                    0.18f,
                    1.20f,
                    max(0.0f, dryY)));
        const float amount = grainStrength * grainMask * 0.055f;
        result.r += noise * amount * 1.03f;
        result.g += noise * amount;
        result.b += noise * amount * 1.08f;
    }

    result = mixPixel(center, result, p.outputMix);
    result.a = dry.a;
    return sanitizePixel(result, dry);
}

kernel void opticsLabFloat(
    device const float4* source [[buffer(0)]],
    device float4* destination [[buffer(1)]],
    device const float* aperture [[buffer(2)]],
    device const float* dirtTexture [[buffer(3)]],
    device const float* smudgeTexture [[buffer(4)]],
    constant Params& p [[buffer(5)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= uint(p.renderWidth) ||
        gid.y >= uint(p.renderHeight)) {
        return;
    }
    const int x = p.renderX1 + int(gid.x);
    const int y = p.renderY1 + int(gid.y);
    device float4* output =
        destination +
        (y - p.dstY1) * p.dstRowPixels +
        (x - p.dstX1);
    *output = processOpticsPixel(
        source,
        aperture,
        dirtTexture,
        smudgeTexture,
        x,
        y,
        p);
}
)METAL";

struct MetalParams {
    int srcRowPixels;
    int dstRowPixels;
    int srcX1;
    int srcY1;
    int srcX2;
    int srcY2;
    int dstX1;
    int dstY1;
    int renderX1;
    int renderY1;
    int renderWidth;
    int renderHeight;
    int frameIndex;
    int depthSource;
    int depthInvert;
    int needsEdgeGuard;
    int identityMapping;
    int identityOutput;
    int apertureWidth;
    int apertureHeight;
    int dirtWidth;
    int dirtHeight;
    int smudgeWidth;
    int smudgeHeight;
    int defocusSamples;
    int glowSamples;
    int comaSamples;
    int irisBlades;
    int maxStarSpokes;

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

    float bloomThreshold;
    float depthNear;
    float depthFar;
    float depthGamma;
    float focusPlane;
    float focusOffset;
    float anamorphicSqueeze;
    float apertureInfluence;
    float dirtAmount;
    float dirtScale;
    float smudgeAmount;
    float smudgeScale;
    float grainSize;
    float grainSeed;
    float edgeGuard;
    float outputMix;

    float width;
    float height;
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
    float fStop;
    float irisRoundness;
    float irisRotation;
    float starUnevenness;
    float starGate;
};

struct Pipelines {
    id<MTLComputePipelineState> opticsLab = nil;
};

struct AssetBufferKey {
    void* device;
    const float* pixels;

    bool operator==(const AssetBufferKey& other) const
    {
        return device == other.device && pixels == other.pixels;
    }
};

struct AssetBufferKeyHash {
    std::size_t operator()(const AssetBufferKey& key) const
    {
        const auto a = reinterpret_cast<std::uintptr_t>(key.device);
        const auto b = reinterpret_cast<std::uintptr_t>(key.pixels);
        return static_cast<std::size_t>(
            (a >> 4) ^ (b + 0x9e3779b97f4a7c15ull + (a << 6)));
    }
};

std::mutex gMetalMutex;
std::unordered_map<void*, Pipelines> gPipelines;
std::unordered_map<
    AssetBufferKey,
    id<MTLBuffer>,
    AssetBufferKeyHash> gAssetBuffers;

bool pipelinesForDevice(id<MTLDevice> device, Pipelines& output)
{
    std::lock_guard<std::mutex> lock(gMetalMutex);
    void* key = static_cast<void*>(device);
    const auto found = gPipelines.find(key);
    if (found != gPipelines.end()) {
        output = found->second;
        return true;
    }

    MTLCompileOptions* options = [MTLCompileOptions new];
    if (@available(macOS 15.0, *)) {
        options.mathMode = MTLMathModeSafe;
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        options.fastMathEnabled = NO;
#pragma clang diagnostic pop
    }
    NSError* error = nil;
    id<MTLLibrary> library = [device
        newLibraryWithSource:
            [NSString stringWithUTF8String:kOpticsLabMetalSource]
        options:options
        error:&error];
    [options release];
    if (!library) {
        std::fprintf(
            stderr,
            "Buckswood Optics Lab Metal compile failed: %s\n",
            error.localizedDescription.UTF8String);
        return false;
    }

    id<MTLFunction> function =
        [library newFunctionWithName:@"opticsLabFloat"];
    if (!function) {
        [library release];
        return false;
    }
    id<MTLComputePipelineState> pipeline =
        [device newComputePipelineStateWithFunction:function error:&error];
    [function release];
    [library release];
    if (!pipeline) {
        std::fprintf(
            stderr,
            "Buckswood Optics Lab Metal pipeline failed: %s\n",
            error.localizedDescription.UTF8String);
        return false;
    }

    Pipelines created;
    created.opticsLab = pipeline;
    gPipelines.emplace(key, created);
    output = created;
    return true;
}

id<MTLBuffer> bufferForTexture(
    id<MTLDevice> device,
    const TextureView& texture)
{
    static const float kFallback = 1.0f;
    const float* pixels = texture.valid()
        ? texture.luminance
        : &kFallback;
    const std::size_t count = texture.valid()
        ? static_cast<std::size_t>(texture.width) *
            static_cast<std::size_t>(texture.height)
        : 1u;
    const AssetBufferKey key{
        static_cast<void*>(device),
        pixels,
    };
    std::lock_guard<std::mutex> lock(gMetalMutex);
    const auto found = gAssetBuffers.find(key);
    if (found != gAssetBuffers.end()) {
        return found->second;
    }
    id<MTLBuffer> buffer = [device
        newBufferWithBytes:pixels
        length:count * sizeof(float)
        options:MTLResourceStorageModeShared];
    if (buffer) {
        gAssetBuffers.emplace(key, buffer);
    }
    return buffer;
}

} // namespace

bool runOpticsLabMetal(
    void* commandQueue,
    const buckswood::gpu::ImageBuffer& source,
    const buckswood::gpu::ImageBuffer& destination,
    const buckswood::gpu::RenderWindow& renderWindow,
    const OpticsLabCore::PreparedState& state,
    const AssetViews& assets,
    bool waitForCompletion)
{
    if (!commandQueue ||
        source.format != buckswood::gpu::PixelFormat::Float32 ||
        destination.format != buckswood::gpu::PixelFormat::Float32) {
        return false;
    }

    id<MTLCommandQueue> queue =
        static_cast<id<MTLCommandQueue>>(commandQueue);
    id<MTLDevice> device = queue.device;
    id<MTLBuffer> sourceBuffer =
        reinterpret_cast<id<MTLBuffer>>(source.handle);
    id<MTLBuffer> destinationBuffer =
        reinterpret_cast<id<MTLBuffer>>(destination.handle);
    if (!device || !sourceBuffer || !destinationBuffer) {
        return false;
    }

    Pipelines pipelines;
    if (!pipelinesForDevice(device, pipelines)) {
        return false;
    }
    id<MTLBuffer> apertureBuffer =
        bufferForTexture(device, assets.aperture);
    id<MTLBuffer> dirtBuffer =
        bufferForTexture(device, assets.dirt);
    id<MTLBuffer> smudgeBuffer =
        bufferForTexture(device, assets.smudge);
    if (!apertureBuffer || !dirtBuffer || !smudgeBuffer) {
        return false;
    }

    const OpticsLabCore::Model& model = state.model;
    const Controls& controls = state.controls;
    MetalParams params{};
    params.srcRowPixels =
        source.rowBytes / static_cast<int>(sizeof(float) * 4);
    params.dstRowPixels =
        destination.rowBytes / static_cast<int>(sizeof(float) * 4);
    params.srcX1 = source.x1;
    params.srcY1 = source.y1;
    params.srcX2 = source.x2;
    params.srcY2 = source.y2;
    params.dstX1 = destination.x1;
    params.dstY1 = destination.y1;
    params.renderX1 = renderWindow.x1;
    params.renderY1 = renderWindow.y1;
    params.renderWidth = renderWindow.x2 - renderWindow.x1;
    params.renderHeight = renderWindow.y2 - renderWindow.y1;
    params.frameIndex = state.frameIndex;
    params.depthSource = controls.depthSource;
    params.depthInvert = controls.depthInvert ? 1 : 0;
    params.needsEdgeGuard = state.needsEdgeGuard ? 1 : 0;
    params.identityMapping = state.identityMapping ? 1 : 0;
    params.identityOutput = state.identityOutput ? 1 : 0;
    params.apertureWidth = assets.aperture.width;
    params.apertureHeight = assets.aperture.height;
    params.dirtWidth = assets.dirt.width;
    params.dirtHeight = assets.dirt.height;
    params.smudgeWidth = assets.smudge.width;
    params.smudgeHeight = assets.smudge.height;
    params.defocusSamples = state.defocusSamples;
    params.glowSamples = state.glowSamples;
    params.comaSamples = state.comaSamples;
    params.irisBlades = state.irisBlades;
    params.maxStarSpokes = state.maxStarSpokes;

    params.distortion = model.distortion;
    params.breathing = model.breathing;
    params.lateralCA = model.lateralCA;
    params.axialCA = model.axialCA;
    params.coma = model.coma;
    params.astigmatism = model.astigmatism;
    params.fieldCurvature = model.fieldCurvature;
    params.spherical = model.spherical;
    params.swirl = model.swirl;
    params.defocus = model.defocus;
    params.catEye = model.catEye;
    params.bloom = model.bloom;
    params.diffusion = model.diffusion;
    params.halation = model.halation;
    params.flareGhosts = model.flareGhosts;
    params.flareStreak = model.flareStreak;
    params.starburst = model.starburst;
    params.vignette = model.vignette;
    params.debayer = model.debayer;
    params.chromaSmear = model.chromaSmear;
    params.grain = model.grain;
    params.warmth = model.warmth;

    params.bloomThreshold = controls.bloomThreshold;
    params.depthNear = controls.depthNear;
    params.depthFar = controls.depthFar;
    params.depthGamma = controls.depthGamma;
    params.focusPlane = controls.focusPlane;
    params.focusOffset = controls.focusOffset;
    params.anamorphicSqueeze = controls.anamorphicSqueeze;
    params.apertureInfluence = controls.apertureInfluence;
    params.dirtAmount = controls.dirtAmount;
    params.dirtScale = controls.dirtScale;
    params.smudgeAmount = controls.smudgeAmount;
    params.smudgeScale = controls.smudgeScale;
    params.grainSize = controls.grainSize;
    params.grainSeed = controls.grainSeed;
    params.edgeGuard = controls.edgeGuard;
    params.outputMix = controls.outputMix;

    params.width = state.width;
    params.height = state.height;
    params.centerX = state.centerX;
    params.centerY = state.centerY;
    params.aspect = state.aspect;
    params.amount = state.amount;
    params.apertureScale = state.apertureScale;
    params.focalScale = state.focalScale;
    params.breathingScale = state.breathingScale;
    params.anamorphicCos = state.anamorphicCos;
    params.anamorphicSin = state.anamorphicSin;
    params.isoGrainScale = state.isoGrainScale;
    params.fStop = controls.fStop;
    params.irisRoundness = state.irisRoundness;
    params.irisRotation = state.irisRotation;
    params.starUnevenness = state.starUnevenness;
    params.starGate = state.starGate;

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    commandBuffer.label = @"Buckswood Optics Lab";
    id<MTLComputeCommandEncoder> encoder =
        [commandBuffer computeCommandEncoder];
    [encoder setComputePipelineState:pipelines.opticsLab];
    [encoder setBuffer:sourceBuffer offset:0 atIndex:0];
    [encoder setBuffer:destinationBuffer offset:0 atIndex:1];
    [encoder setBuffer:apertureBuffer offset:0 atIndex:2];
    [encoder setBuffer:dirtBuffer offset:0 atIndex:3];
    [encoder setBuffer:smudgeBuffer offset:0 atIndex:4];
    [encoder setBytes:&params length:sizeof(params) atIndex:5];
    const NSUInteger executionWidth =
        pipelines.opticsLab.threadExecutionWidth;
    const NSUInteger maxThreads =
        pipelines.opticsLab.maxTotalThreadsPerThreadgroup;
    const NSUInteger groupHeight = std::max<NSUInteger>(
        1,
        std::min<NSUInteger>(8, maxThreads / executionWidth));
    [encoder
        dispatchThreads:
            MTLSizeMake(
                params.renderWidth,
                params.renderHeight,
                1)
        threadsPerThreadgroup:
            MTLSizeMake(executionWidth, groupHeight, 1)];
    [encoder endEncoding];
    [commandBuffer commit];
    if (waitForCompletion) {
        [commandBuffer waitUntilCompleted];
    }
    return commandBuffer.status != MTLCommandBufferStatusError;
}

} // namespace buckswood_optics
