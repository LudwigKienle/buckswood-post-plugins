#include "FakeDiagnosticCore.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {

class TestSampler final : public buckswood_fake::Sampler {
public:
    TestSampler(std::vector<buckswood_fake::Pixel> pixels, int width, int height)
        : pixels_(std::move(pixels))
        , width_(width)
        , height_(height)
    {
    }

    buckswood_fake::Pixel sample(float x, float y) const override
    {
        int ix = static_cast<int>(std::round(x));
        int iy = static_cast<int>(std::round(y));
        ix = std::max(0, std::min(width_ - 1, ix));
        iy = std::max(0, std::min(height_ - 1, iy));
        return pixels_[static_cast<std::size_t>(iy) * width_ + ix];
    }

private:
    std::vector<buckswood_fake::Pixel> pixels_;
    int width_;
    int height_;
};

bool finitePixel(buckswood_fake::Pixel p)
{
    return std::isfinite(p.r) && std::isfinite(p.g) && std::isfinite(p.b) && std::isfinite(p.a);
}

} // namespace

int main()
{
    constexpr int width = 24;
    constexpr int height = 16;
    std::vector<buckswood_fake::Pixel> pixels(static_cast<std::size_t>(width * height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float edge = x > width / 2 ? 0.82f : 0.18f;
            const float pattern = ((x + y) % 3 == 0) ? 0.015f : -0.015f;
            pixels[static_cast<std::size_t>(y) * width + x] = buckswood_fake::Pixel{
                edge + pattern,
                edge * 0.92f + pattern,
                edge * 0.78f + pattern,
                1.0f,
            };
        }
    }

    TestSampler sampler(std::move(pixels), width, height);
    const auto controls = buckswood_fake::FakeDiagnosticCore::defaultControls();
    const buckswood_fake::FrameInfo frame{width, height, 42};

    const auto scores = buckswood_fake::FakeDiagnosticCore::analyzePixel(sampler, width / 2, height / 2, frame, controls);
    const auto overlay = buckswood_fake::FakeDiagnosticCore::processPixel(
        sampler,
        width / 2,
        height / 2,
        frame,
        controls,
        buckswood_fake::ViewDiagnosticOverlay);
    const auto assist = buckswood_fake::FakeDiagnosticCore::processPixel(
        sampler,
        width / 2,
        height / 2,
        frame,
        controls,
        buckswood_fake::ViewRealityMatchAssist);

    if (!std::isfinite(scores.overall) || !finitePixel(overlay) || !finitePixel(assist)) {
        std::cerr << "Fake diagnostic smoke test failed: non-finite result\n";
        return 1;
    }

    if (scores.overall < 0.0f || scores.overall > 1.0f) {
        std::cerr << "Fake diagnostic smoke test failed: score out of range\n";
        return 1;
    }

    std::cout << "Fake diagnostic smoke test passed. overall=" << scores.overall << "\n";
    return 0;
}
