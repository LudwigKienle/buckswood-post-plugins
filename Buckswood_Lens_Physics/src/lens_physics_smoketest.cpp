#include "LensPhysicsCore.h"

#include <iomanip>
#include <iostream>

namespace {

class TestSampler final : public buckswood_lens::Sampler {
public:
    buckswood_lens::Pixel sample(float x, float y) const override
    {
        const float nx = x / 1919.0f;
        const float ny = y / 1079.0f;
        const float hot = (x > 1500.0f && y < 250.0f) ? 0.8f : 0.0f;
        return buckswood_lens::Pixel{
            0.08f + nx * 0.55f + hot,
            0.10f + ny * 0.42f + hot * 0.8f,
            0.12f + (1.0f - nx) * 0.28f + hot * 0.45f,
            1.0f,
        };
    }
};

} // namespace

int main()
{
    TestSampler sampler;
    buckswood_lens::FrameInfo frame{1920, 1080, 0};
    buckswood_lens::Controls controls{
        2,      // lensPreset
        0.75f,  // effectStrength
        0.0f,   // distortion
        0.25f,  // chromaticAberration
        0.20f,  // fringing
        0.35f,  // coma
        0.45f,  // bloom
        0.72f,  // bloomThreshold
        0.30f,  // vignette
        0.18f,  // cornerColor
        0.05f,  // swirl
        0.10f,  // fStopSharpener
        1.0f,   // overdrive
        1.0f,   // outputMix
    };

    const int points[][2] = {
        {100, 100},
        {960, 540},
        {1780, 130},
        {1800, 950},
    };

    std::cout << std::fixed << std::setprecision(4);
    for (const auto& point : points) {
        buckswood_lens::Pixel out = buckswood_lens::LensPhysicsCore::processPixel(
            sampler,
            point[0],
            point[1],
            frame,
            controls);
        std::cout << point[0] << "," << point[1] << " "
                  << out.r << " "
                  << out.g << " "
                  << out.b << " "
                  << out.a << "\n";
    }
    return 0;
}

