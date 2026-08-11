#include "OpticsLabCore.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

using buckswood_optics::Controls;
using buckswood_optics::FrameInfo;
using buckswood_optics::OpticsLabCore;
using buckswood_optics::Pixel;

constexpr int kWidth = 512;
constexpr int kHeight = 288;

class ImageSampler final : public buckswood_optics::Sampler {
public:
    ImageSampler()
        : pixels_(
              static_cast<std::size_t>(kWidth) *
              static_cast<std::size_t>(kHeight))
    {
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                const float nx =
                    static_cast<float>(x) /
                    static_cast<float>(kWidth - 1);
                const float ny =
                    static_cast<float>(y) /
                    static_cast<float>(kHeight - 1);
                const float highlight =
                    x > kWidth * 3 / 4 && y < kHeight / 3
                    ? 0.85f
                    : 0.0f;
                pixels_[
                    static_cast<std::size_t>(y) *
                        static_cast<std::size_t>(kWidth) +
                    static_cast<std::size_t>(x)] = Pixel{
                    0.04f + nx * 0.94f + highlight,
                    0.05f + ny * 0.66f + highlight * 0.82f,
                    0.08f + (1.0f - nx) * 0.42f + highlight * 0.56f,
                    0.15f + ny * 0.75f,
                };
            }
        }
    }

    Pixel sample(float x, float y) const override
    {
        const float safeX = std::min(
            static_cast<float>(kWidth - 1),
            std::max(0.0f, x));
        const float safeY = std::min(
            static_cast<float>(kHeight - 1),
            std::max(0.0f, y));
        const int x0 = static_cast<int>(std::floor(safeX));
        const int y0 = static_cast<int>(std::floor(safeY));
        const int x1 = std::min(kWidth - 1, x0 + 1);
        const int y1 = std::min(kHeight - 1, y0 + 1);
        const float tx = safeX - static_cast<float>(x0);
        const float ty = safeY - static_cast<float>(y0);
        const Pixel& p00 = at(x0, y0);
        const Pixel& p10 = at(x1, y0);
        const Pixel& p01 = at(x0, y1);
        const Pixel& p11 = at(x1, y1);
        const float u = 1.0f - tx;
        const float v = 1.0f - ty;
        return Pixel{
            (p00.r * u + p10.r * tx) * v +
                (p01.r * u + p11.r * tx) * ty,
            (p00.g * u + p10.g * tx) * v +
                (p01.g * u + p11.g * tx) * ty,
            (p00.b * u + p10.b * tx) * v +
                (p01.b * u + p11.b * tx) * ty,
            (p00.a * u + p10.a * tx) * v +
                (p01.a * u + p11.a * tx) * ty,
        };
    }

private:
    const Pixel& at(int x, int y) const
    {
        return pixels_[
            static_cast<std::size_t>(y) *
                static_cast<std::size_t>(kWidth) +
            static_cast<std::size_t>(x)];
    }

    std::vector<Pixel> pixels_;
};

Controls controlsForPreset(int preset)
{
    Controls controls{};
    controls.preset = preset;
    controls.effectStrength = 0.65f;
    controls.focalLength = 50.0f;
    controls.fStop = 2.8f;
    controls.focusDistance = 3.0f;
    controls.sensorWidth = 36.0f;
    controls.anamorphicSqueeze = 1.0f;
    controls.bloomThreshold = 0.82f;
    controls.depthFar = 1.0f;
    controls.depthGamma = 1.0f;
    controls.focusPlane = 0.5f;
    controls.grainSize = 1.0f;
    controls.grainSeed = 1.0f;
    controls.sensorISO = 400.0f;
    controls.apertureInfluence = 0.85f;
    controls.dirtScale = 1.0f;
    controls.edgeGuard = 0.80f;
    controls.outputMix = 0.65f;
    return controls;
}

double renderCase(
    const ImageSampler& sampler,
    const Controls& controls,
    std::vector<Pixel>& output)
{
    const FrameInfo frame{kWidth, kHeight, 42};
    const auto prepared = OpticsLabCore::prepare(frame, controls);
    const auto start = std::chrono::steady_clock::now();
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            output[
                static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(kWidth) +
                static_cast<std::size_t>(x)] =
                OpticsLabCore::processPixel(
                    sampler,
                    x,
                    y,
                    prepared,
                    nullptr);
        }
    }
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

int main(int argc, char** argv)
{
    if (argc > 2) {
        std::cerr << "usage: optics_lab_benchmark [output.bin]\n";
        return 2;
    }

    const ImageSampler sampler;
    std::vector<Pixel> output(
        static_cast<std::size_t>(kWidth) *
        static_cast<std::size_t>(kHeight));
    std::vector<Pixel> parityOutput;
    parityOutput.reserve(output.size() * 3u);

    // Warm the executable and caches before measuring.
    renderCase(sampler, controlsForPreset(6), output);
    const double neutralMs =
        renderCase(sampler, controlsForPreset(0), output);
    parityOutput.insert(
        parityOutput.end(),
        output.begin(),
        output.end());
    const double defaultMs =
        renderCase(sampler, controlsForPreset(6), output);
    parityOutput.insert(
        parityOutput.end(),
        output.begin(),
        output.end());
    Controls demanding = controlsForPreset(5);
    demanding.effectStrength = 0.85f;
    demanding.outputMix = 0.85f;
    demanding.focusOffset = 0.72f;
    demanding.defocus = 0.65f;
    demanding.bloom = 0.45f;
    demanding.diffusion = 0.35f;
    const double demandingMs = renderCase(sampler, demanding, output);
    parityOutput.insert(
        parityOutput.end(),
        output.begin(),
        output.end());

    std::vector<Pixel> performanceOutput(output.size());
    Controls preview = demanding;
    preview.quality = 0;
    const double previewMs =
        renderCase(sampler, preview, performanceOutput);
    Controls bypassed = demanding;
    bypassed.geometryEnabled = false;
    bypassed.aberrationsEnabled = false;
    bypassed.defocusEnabled = false;
    bypassed.lightEnabled = false;
    bypassed.vignetteEnabled = false;
    bypassed.surfaceEnabled = false;
    bypassed.sensorEnabled = false;
    const double bypassedMs =
        renderCase(sampler, bypassed, performanceOutput);

    if (argc == 2) {
        std::ofstream stream(argv[1], std::ios::binary);
        if (!stream) {
            std::cerr << "failed to open output file\n";
            return 1;
        }
        stream.write(
            reinterpret_cast<const char*>(parityOutput.data()),
            static_cast<std::streamsize>(
                parityOutput.size() * sizeof(Pixel)));
    }

    double checksum = 0.0;
    for (const Pixel& pixel : output) {
        checksum +=
            static_cast<double>(pixel.r) * 0.31 +
            static_cast<double>(pixel.g) * 0.47 +
            static_cast<double>(pixel.b) * 0.22;
    }
    std::cout << std::fixed << std::setprecision(3)
              << "neutral_ms=" << neutralMs << '\n'
              << "default_ms=" << defaultMs << '\n'
              << "demanding_ms=" << demandingMs << '\n'
              << "preview_ms=" << previewMs << '\n'
              << "all_stages_bypassed_ms=" << bypassedMs << '\n'
              << std::setprecision(9)
              << "checksum=" << checksum << '\n';
    return 0;
}
