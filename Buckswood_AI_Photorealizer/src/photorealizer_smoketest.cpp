#include "PhotorealizerCore.h"

#include <iomanip>
#include <iostream>

int main()
{
    buckswood::Controls controls{
        0.65f, // plasticReduction
        0.55f, // skinRealism
        0.70f, // highlightRealism
        0.55f, // colorNaturalize
        0.30f, // textureAmount
        0.35f, // microContrast
        0.75f, // gamutGuard
        0.35f, // shadowDepth
        0.45f, // smoothnessBreakup
        1.00f, // outputMix
        1.00f, // seed
    };
    buckswood::FrameInfo frame{1920, 1080, 0};

    const buckswood::Pixel samples[] = {
        {0.74f, 0.55f, 0.48f, 1.0f},
        {0.10f, 0.85f, 1.00f, 1.0f},
        {0.92f, 0.91f, 0.88f, 1.0f},
        {0.08f, 0.09f, 0.12f, 1.0f},
    };

    std::cout << std::fixed << std::setprecision(4);
    for (int i = 0; i < 4; ++i) {
        buckswood::Pixel out = buckswood::PhotorealizerCore::processPixel(samples[i], i * 37, i * 19, frame, controls);
        std::cout << i << " "
                  << out.r << " "
                  << out.g << " "
                  << out.b << " "
                  << out.a << "\n";
    }
    return 0;
}
