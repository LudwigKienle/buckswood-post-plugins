#include "BuckswoodPremiereAdapter.h"

#include <array>
#include <cstdio>

int main()
{
    constexpr int width = 4;
    constexpr int height = 4;
    std::array<buckswood_adobe::PixelF, width * height> pixels{};

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            pixels[y * width + x] = {
                static_cast<float>(x) / static_cast<float>(width - 1),
                static_cast<float>(y) / static_cast<float>(height - 1),
                0.35f,
                1.0f,
            };
        }
    }

    buckswood_adobe::ImageViewF view{pixels.data(), width, height, width};
    const auto photo = buckswood_adobe::processPhotorealizerPixel(
        pixels[5], 1, 1, width, height, buckswood_adobe::photorealizerDefaults());
    const auto lens = buckswood_adobe::processLensPixel(
        view, 2, 2, buckswood_adobe::realisticLensDefaults());

    std::printf("photorealizer %.4f %.4f %.4f %.4f\n", photo.r, photo.g, photo.b, photo.a);
    std::printf("lens %.4f %.4f %.4f %.4f\n", lens.r, lens.g, lens.b, lens.a);
    return 0;
}
