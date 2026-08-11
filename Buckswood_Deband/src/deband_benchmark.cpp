#include "DebandCore.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

namespace {

using buckswood_deband::Pixel;

class BufferSampler {
public:
    BufferSampler(const std::vector<Pixel>& pixels, int width, int height)
        : pixels_(pixels)
        , width_(width)
        , height_(height)
    {
    }

    Pixel sample(int x, int y) const
    {
        x = std::clamp(x, 0, width_ - 1);
        y = std::clamp(y, 0, height_ - 1);
        return pixels_[static_cast<std::size_t>(y * width_ + x)];
    }

private:
    const std::vector<Pixel>& pixels_;
    int width_;
    int height_;
};

} // namespace

int main()
{
    constexpr int width = 640;
    constexpr int height = 360;
    std::vector<Pixel> input(width * height);
    std::vector<Pixel> output(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float value = static_cast<float>((x + y) & 255) / 255.0f;
            input[static_cast<std::size_t>(y * width + x)] =
                Pixel{value, value * 0.9f, value * 0.8f, 1.0f};
        }
    }
    BufferSampler sampler(input, width, height);
    buckswood_deband::Controls controls{};
    const auto state = buckswood_deband::DebandCore::prepare(
        buckswood_deband::FrameInfo{width, height, 0},
        controls);
    const auto started = std::chrono::steady_clock::now();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            output[static_cast<std::size_t>(y * width + x)] =
                buckswood_deband::DebandCore::processPixel(
                    sampler,
                    x,
                    y,
                    state);
        }
    }
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    const double megapixels = static_cast<double>(width * height) / 1000000.0;
    std::cout << "Buckswood Deband CPU reference: "
              << megapixels / seconds << " MP/s (single-thread core)\n";
    return output[0].a > 0.0f ? 0 : 1;
}
