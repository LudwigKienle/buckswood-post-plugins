#include "DebandCore.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using buckswood_deband::Controls;
using buckswood_deband::DebandCore;
using buckswood_deband::FrameInfo;
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
    int width_ = 1;
    int height_ = 1;
};

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

float luma(const Pixel& pixel)
{
    return 0.2627f * pixel.r +
        0.6780f * pixel.g +
        0.0593f * pixel.b;
}

float rmseAgainstGradient(
    const std::vector<Pixel>& pixels,
    int width,
    int row)
{
    double squaredError = 0.0;
    for (int x = 0; x < width; ++x) {
        const float ideal = 0.08f +
            0.72f * static_cast<float>(x) /
                static_cast<float>(width - 1);
        const float error = luma(
            pixels[static_cast<std::size_t>(row * width + x)]) - ideal;
        squaredError += static_cast<double>(error * error);
    }
    return static_cast<float>(
        std::sqrt(squaredError / static_cast<double>(width)));
}

std::vector<Pixel> process(
    const std::vector<Pixel>& input,
    int width,
    int height,
    const Controls& controls,
    int frameIndex = 0)
{
    const BufferSampler sampler(input, width, height);
    const auto state = DebandCore::prepare(
        FrameInfo{width, height, frameIndex},
        controls);
    std::vector<Pixel> output(input.size());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            output[static_cast<std::size_t>(y * width + x)] =
                DebandCore::processPixel(sampler, x, y, state);
        }
    }
    return output;
}

void testQuantizedGradientRepair()
{
    constexpr int width = 320;
    constexpr int height = 24;
    std::vector<Pixel> input(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float ideal = 0.08f +
                0.72f * static_cast<float>(x) /
                    static_cast<float>(width - 1);
            const float banded = std::floor(ideal * 63.0f + 0.5f) / 63.0f;
            input[static_cast<std::size_t>(y * width + x)] =
                Pixel{banded, banded * 0.98f, banded * 1.02f, 0.73f};
        }
    }

    Controls controls{};
    controls.preset = buckswood_deband::PresetManual;
    controls.workingSpace = buckswood_deband::WorkingLog;
    controls.sourcePrecision = buckswood_deband::Precision8Bit;
    controls.strength = 1.0f;
    controls.detection = 0.90f;
    controls.radius = 12.0f;
    controls.edgeProtection = 0.88f;
    controls.textureProtection = 0.85f;
    controls.chromaRepair = 0.35f;
    controls.dither = 0.0f;
    const std::vector<Pixel> output = process(
        input,
        width,
        height,
        controls);
    const float before = rmseAgainstGradient(input, width, height / 2);
    const float after = rmseAgainstGradient(output, width, height / 2);
    require(after < before * 0.96f, "quantized gradient RMSE is reduced");
    require(
        std::fabs(output[width / 2].a - 0.73f) < 0.000001f,
        "alpha is preserved during gradient repair");
}

void testIntentionalFlatAndHardEdgeProtection()
{
    constexpr int width = 128;
    constexpr int height = 32;
    std::vector<Pixel> input(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float value = x < width / 2 ? 0.12f : 0.92f;
            input[static_cast<std::size_t>(y * width + x)] =
                Pixel{value, value * 0.85f, value * 0.72f, 1.0f};
        }
    }
    Controls controls{};
    controls.preset = buckswood_deband::PresetManual;
    controls.sourcePrecision = buckswood_deband::Precision8Bit;
    controls.strength = 1.0f;
    controls.detection = 1.0f;
    controls.radius = 16.0f;
    controls.edgeProtection = 1.0f;
    controls.textureProtection = 1.0f;
    controls.chromaRepair = 1.0f;
    controls.dither = 0.0f;
    const std::vector<Pixel> output = process(
        input,
        width,
        height,
        controls);
    const int y = height / 2;
    for (int x : {8, width / 2 - 1, width / 2, width - 9}) {
        const std::size_t index = static_cast<std::size_t>(y * width + x);
        require(
            std::fabs(output[index].r - input[index].r) < 0.002f,
            "intentional flat fields and hard edges remain stable");
    }
}

void testTextureAndHdrPreservation()
{
    constexpr int width = 96;
    constexpr int height = 48;
    std::vector<Pixel> input(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float texture = ((x + y) & 1) ? 0.035f : -0.035f;
            const float base = 1.25f + texture;
            input[static_cast<std::size_t>(y * width + x)] =
                Pixel{base, 0.92f + texture, -0.03f + texture, 0.42f};
        }
    }
    Controls controls{};
    controls.preset = buckswood_deband::PresetManual;
    controls.workingSpace = buckswood_deband::WorkingSceneLinear;
    controls.sourcePrecision = buckswood_deband::Precision10Bit;
    controls.strength = 1.0f;
    controls.detection = 0.8f;
    controls.radius = 10.0f;
    controls.edgeProtection = 0.95f;
    controls.textureProtection = 1.0f;
    controls.chromaRepair = 0.5f;
    controls.dither = 0.0f;
    const std::vector<Pixel> output = process(
        input,
        width,
        height,
        controls);
    float maximumDifference = 0.0f;
    for (std::size_t index = 0; index < input.size(); ++index) {
        maximumDifference = std::max(
            maximumDifference,
            std::fabs(output[index].r - input[index].r));
        require(output[index].a == input[index].a, "HDR alpha remains exact");
        require(std::isfinite(output[index].r), "HDR output remains finite");
    }
    require(maximumDifference < 0.002f, "fine texture is protected");
    require(output[0].r > 1.0f, "float HDR values are not clipped");
    require(output[0].b < 0.0f, "negative float values are not clipped");
}

void testDeterministicDither()
{
    constexpr int width = 64;
    constexpr int height = 16;
    std::vector<Pixel> input(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float value = std::floor(
                (0.2f + 0.5f * static_cast<float>(x) / (width - 1)) *
                    255.0f +
                0.5f) / 255.0f;
            input[static_cast<std::size_t>(y * width + x)] =
                Pixel{value, value, value, 1.0f};
        }
    }
    Controls controls{};
    controls.preset = buckswood_deband::PresetManual;
    controls.sourcePrecision = buckswood_deband::Precision8Bit;
    controls.strength = 0.8f;
    controls.detection = 0.8f;
    controls.dither = 1.0f;
    controls.ditherMotion = buckswood_deband::DitherStatic;
    const auto first = process(input, width, height, controls, 10);
    const auto second = process(input, width, height, controls, 100);
    require(
        first[width / 2].r == second[width / 2].r,
        "static dither is independent of render order and frame time");

    controls.ditherMotion = buckswood_deband::DitherFrameIndexed;
    const auto animatedA = process(input, width, height, controls, 10);
    const auto animatedB = process(input, width, height, controls, 11);
    require(
        animatedA[width / 2].r != animatedB[width / 2].r,
        "frame-indexed dither changes deterministically with time");
}

} // namespace

int main()
{
    testQuantizedGradientRepair();
    testIntentionalFlatAndHardEdgeProtection();
    testTextureAndHdrPreservation();
    testDeterministicDither();
    std::cout << "Buckswood Deband quality smoke tests passed\n";
    return 0;
}
