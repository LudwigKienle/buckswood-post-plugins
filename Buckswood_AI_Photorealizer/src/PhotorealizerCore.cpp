#include "PhotorealizerCore.h"

#include <algorithm>
#include <cmath>

namespace buckswood {

Masks PhotorealizerCore::defaultMasks()
{
    return Masks{0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
}

Pixel PhotorealizerCore::processPixel(
    Pixel input,
    int x,
    int y,
    const FrameInfo& frame,
    const Controls& controls,
    const Masks& masks)
{
    Pixel rgb = input;
    const Pixel dry = input;

    const float lum0 = luma(rgb);
    const float skin = clamp01(std::max(masks.skin, skinWeight(rgb)) * controls.skinRealism);
    const float plastic = clamp01(masks.plastic * controls.plasticReduction);
    const float highlight = clamp01(std::max(masks.highlight, (lum0 - 0.62f) / 0.28f));

    const float chromaR = rgb.r - lum0;
    const float chromaG = rgb.g - lum0;
    const float chromaB = rgb.b - lum0;
    const float chroma = std::fabs(chromaR) + std::fabs(chromaG) + std::fabs(chromaB);

    // AI outputs often carry over-bright saturated colors. Compress only the
    // extreme chroma region, then let normal colors breathe.
    const float neon = clamp01((chroma - 0.22f) / 0.42f);
    const float gamutCompress = neon * controls.gamutGuard;
    rgb.r = lum0 + chromaR * (1.0f - 0.38f * gamutCompress);
    rgb.g = lum0 + chromaG * (1.0f - 0.30f * gamutCompress);
    rgb.b = lum0 + chromaB * (1.0f - 0.34f * gamutCompress);

    // Skin gets a tiny bias away from magenta/pink plastic and toward a more
    // camera-like peach/yellow response.
    rgb.r += 0.018f * skin;
    rgb.g += 0.007f * skin;
    rgb.b -= 0.020f * skin;

    const float lum1 = luma(rgb);
    const float pivot = 0.42f;
    const float contrast = 1.0f + controls.microContrast * 0.18f;
    const float shapedLum = pivot + (lum1 - pivot) * contrast;
    rgb.r += shapedLum - lum1;
    rgb.g += shapedLum - lum1;
    rgb.b += shapedLum - lum1;

    // Real cameras roll hot highlights; AI often clips into synthetic gloss.
    const float roll = controls.highlightRealism * (0.45f + 0.55f * highlight);
    rgb.r = softClip(rgb.r, 0.74f, roll);
    rgb.g = softClip(rgb.g, 0.74f, roll);
    rgb.b = softClip(rgb.b, 0.74f, roll);

    const float lum2 = luma(rgb);
    const float shadowMask = clamp01((0.42f - lum2) / 0.42f);
    rgb.r *= 1.0f - shadowMask * controls.shadowDepth * 0.07f;
    rgb.g *= 1.0f - shadowMask * controls.shadowDepth * 0.06f;
    rgb.b *= 1.0f - shadowMask * controls.shadowDepth * 0.04f;

    // Break up smooth AI surfaces with very fine deterministic sensor texture.
    // This is intentionally tiny; it should read as camera noise, not an effect.
    const int seed = static_cast<int>(controls.seed * 997.0f) + frame.frameIndex * 17;
    const float n1 = hash01(x, y, seed) - 0.5f;
    const float n2 = hash01(x + 19, y - 7, seed + 113) - 0.5f;
    const float n3 = hash01(x - 5, y + 23, seed + 271) - 0.5f;
    const float textureMask = clamp01((0.20f + plastic * 0.65f + controls.smoothnessBreakup * 0.35f) * masks.detail);
    const float texture = controls.textureAmount * textureMask * 0.018f;
    rgb.r += n1 * texture;
    rgb.g += n2 * texture;
    rgb.b += n3 * texture;

    const float lum3 = luma(rgb);
    const float naturalSat = 1.0f - controls.colorNaturalize * (0.08f + neon * 0.22f + highlight * 0.12f);
    rgb.r = lum3 + (rgb.r - lum3) * naturalSat;
    rgb.g = lum3 + (rgb.g - lum3) * naturalSat;
    rgb.b = lum3 + (rgb.b - lum3) * naturalSat;

    rgb.r = clamp01(rgb.r);
    rgb.g = clamp01(rgb.g);
    rgb.b = clamp01(rgb.b);
    rgb.a = input.a;

    return mix(dry, rgb, clamp01(controls.outputMix));
}

float PhotorealizerCore::clamp01(float value)
{
    return std::min(1.0f, std::max(0.0f, value));
}

float PhotorealizerCore::luma(Pixel pixel)
{
    return 0.2627f * pixel.r + 0.6780f * pixel.g + 0.0593f * pixel.b;
}

float PhotorealizerCore::hash01(int x, int y, int z)
{
    uint32_t h = static_cast<uint32_t>(x) * 73856093u ^
                 static_cast<uint32_t>(y) * 19349663u ^
                 static_cast<uint32_t>(z) * 83492791u;
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return static_cast<float>(h & 0x00ffffffu) / 16777216.0f;
}

float PhotorealizerCore::softClip(float value, float knee, float amount)
{
    if (value <= knee) {
        return value;
    }
    const float over = value - knee;
    return knee + over / (1.0f + over * amount * 8.0f);
}

float PhotorealizerCore::skinWeight(Pixel pixel)
{
    const float r = pixel.r;
    const float g = pixel.g;
    const float b = pixel.b;
    if (!(r > g && g > b)) {
        return 0.0f;
    }

    const float rg = r - g;
    const float gb = g - b;
    const float warm = (rg * gb) / (r * r + 0.001f);
    const float lum = luma(pixel);
    const float exposure = clamp01(1.0f - std::fabs(lum - 0.52f) / 0.42f);
    return clamp01(warm * 3.0f) * exposure;
}

Pixel PhotorealizerCore::mix(Pixel a, Pixel b, float t)
{
    const float u = 1.0f - t;
    return Pixel{
        a.r * u + b.r * t,
        a.g * u + b.g * t,
        a.b * u + b.b * t,
        a.a * u + b.a * t,
    };
}

} // namespace buckswood
