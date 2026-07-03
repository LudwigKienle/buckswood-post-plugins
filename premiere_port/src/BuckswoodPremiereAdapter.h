#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "../../Buckswood_AI_Photorealizer/include/PhotorealizerCore.h"
#include "../../Buckswood_Lens_Physics/include/LensPhysicsCore.h"

namespace buckswood_adobe {

struct PixelF {
    float r;
    float g;
    float b;
    float a;
};

struct ImageViewF {
    const PixelF* pixels = nullptr;
    int width = 0;
    int height = 0;
    int rowStridePixels = 0;
};

inline float clamp01(float value)
{
    return std::min(1.0f, std::max(0.0f, value));
}

inline std::uint8_t to8(float value)
{
    return static_cast<std::uint8_t>(clamp01(value) * 255.0f + 0.5f);
}

inline float from8(std::uint8_t value)
{
    return static_cast<float>(value) / 255.0f;
}

class LensSampler final : public buckswood_lens::Sampler {
public:
    explicit LensSampler(ImageViewF view)
        : view_(view)
    {
    }

    buckswood_lens::Pixel sample(float x, float y) const override
    {
        if (!view_.pixels || view_.width <= 0 || view_.height <= 0 || view_.rowStridePixels <= 0) {
            return {0.0f, 0.0f, 0.0f, 1.0f};
        }

        const int ix = std::clamp(static_cast<int>(x + 0.5f), 0, view_.width - 1);
        const int iy = std::clamp(static_cast<int>(y + 0.5f), 0, view_.height - 1);
        const PixelF& p = view_.pixels[iy * view_.rowStridePixels + ix];
        return {p.r, p.g, p.b, p.a};
    }

private:
    ImageViewF view_;
};

inline buckswood_lens::Controls realisticLensDefaults()
{
    return {
        19,     // Buckswood Realistic Match
        0.45f,  // effectStrength
        0.0f,   // distortion
        0.015f, // chromaticAberration
        0.015f, // fringing
        0.035f, // coma
        0.075f, // bloom
        0.82f,  // bloomThreshold
        0.045f, // vignette
        0.020f, // cornerColor
        0.0f,   // swirl
        0.060f, // fStopSharpener
        0.85f,  // overdrive
        0.55f,  // outputMix
        0.82f,  // edgeHaloGuard
    };
}

inline buckswood::Controls photorealizerDefaults()
{
    return {
        0.65f, // plasticReduction
        0.55f, // skinRealism
        0.70f, // highlightRealism
        0.55f, // colorNaturalize
        0.30f, // textureAmount
        0.35f, // microContrast
        0.75f, // gamutGuard
        0.35f, // shadowDepth
        0.45f, // smoothnessBreakup
        1.0f,  // outputMix
        1.0f,  // seed
    };
}

PixelF processPhotorealizerPixel(
    PixelF input,
    int x,
    int y,
    int width,
    int height,
    const buckswood::Controls& controls);

PixelF processLensPixel(
    ImageViewF input,
    int x,
    int y,
    const buckswood_lens::Controls& controls);

} // namespace buckswood_adobe
