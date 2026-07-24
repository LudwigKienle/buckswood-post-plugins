#include "CinematicToolsCore.h"
#include "FakeDiagnosticCore.h"
#include "FilmEmulationCore.h"
#include "LookDNACore.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 180;

template <typename Pixel>
void append(std::vector<float>& output, const Pixel& pixel)
{
    output.push_back(pixel.r);
    output.push_back(pixel.g);
    output.push_back(pixel.b);
    output.push_back(pixel.a);
}

class FakeSampler final : public buckswood_fake::Sampler {
public:
    buckswood_fake::Pixel sample(float x, float y) const override
    {
        const float nx = x / static_cast<float>(kWidth - 1);
        const float ny = y / static_cast<float>(kHeight - 1);
        return {
            0.08f + nx * 0.72f + std::sin(y * 0.11f) * 0.015f,
            0.07f + ny * 0.55f + std::sin(x * 0.07f) * 0.012f,
            0.10f + (1.0f - nx) * 0.38f,
            1.0f,
        };
    }
};

class FilmSampler final : public buckswood_film::Sampler {
public:
    buckswood_film::Pixel sample(float x, float y) const override
    {
        const float nx = x / static_cast<float>(kWidth - 1);
        const float ny = y / static_cast<float>(kHeight - 1);
        const float hot = x > kWidth * 0.78f && y < kHeight * 0.24f
            ? 0.72f
            : 0.0f;
        return {
            0.08f + nx * 0.58f + hot,
            0.09f + ny * 0.36f + hot * 0.82f,
            0.10f + (1.0f - nx) * 0.22f + hot * 0.42f,
            1.0f,
        };
    }
};

buckswood_film::Controls filmControls()
{
    buckswood_film::Controls c{};
    c.inputSpace = 1;
    c.stockPreset = 8;
    c.printPreset = 6;
    c.processMode = 0;
    c.grainProfile = 3;
    c.halationProfile = 2;
    c.bloomProfile = 2;
    c.grainMode = 1;
    c.viewMode = 0;
    c.density = 0.18f;
    c.contrast = 1.0f;
    c.developerContrast = 0.10f;
    c.colorSeparation = 0.30f;
    c.colorBoost = 0.04f;
    c.saturation = 0.94f;
    c.subtractiveColor = 0.34f;
    c.interlayer = 0.34f;
    c.filmCompression = 0.38f;
    c.compressionWhitePoint = 1.08f;
    c.compressionRange = 0.55f;
    c.compressionColorDensity = 0.48f;
    c.highlightRolloff = 0.62f;
    c.halation = 0.12f;
    c.halationLocal = 0.70f;
    c.halationGlobal = 0.18f;
    c.halationRedShift = 0.72f;
    c.bloom = 0.05f;
    c.bloomThreshold = 0.78f;
    c.bloomSpread = 0.42f;
    c.grain = 0.18f;
    c.grainSize = 0.85f;
    c.grainShadows = 0.70f;
    c.grainMidtones = 0.55f;
    c.grainHighlights = 0.42f;
    c.grainChroma = 0.26f;
    c.filmResolution = 0.82f;
    c.gateWeave = 0.04f;
    c.filmBreath = 0.08f;
    c.temporalStability = 0.62f;
    c.temporalMotionGuard = 0.82f;
    c.skinProtect = 0.72f;
    c.outputMix = 0.75f;
    return c;
}

class CinematicSampler final : public buckswood_cinematic::Sampler {
public:
    buckswood_cinematic::Pixel sample(float x, float y) const override
    {
        const float nx = x / static_cast<float>(kWidth - 1);
        const float ny = y / static_cast<float>(kHeight - 1);
        const float highlight = nx > 0.72f && ny < 0.35f ? 0.58f : 0.0f;
        return {
            0.04f + nx * 0.86f + highlight,
            0.05f + ny * 0.62f + highlight * 0.82f,
            0.08f + (1.0f - nx) * 0.32f + highlight * 0.61f,
            1.0f,
        };
    }
};

class LookSampler final : public buckswood_lookdna::Sampler {
public:
    explicit LookSampler(float warmth)
        : warmth_(warmth)
    {
    }

    buckswood_lookdna::Pixel sample(float x, float y) const override
    {
        const float nx = x / static_cast<float>(kWidth - 1);
        const float ny = y / static_cast<float>(kHeight - 1);
        return {
            0.07f + nx * 0.68f + warmth_ * 0.08f,
            0.08f + ny * 0.50f + warmth_ * 0.01f,
            0.10f + (1.0f - nx) * 0.31f - warmth_ * 0.05f,
            1.0f,
        };
    }

    buckswood_lookdna::Bounds bounds() const override
    {
        return {0, 0, kWidth, kHeight};
    }

private:
    float warmth_;
};

template <typename Function>
double measure(Function&& function)
{
    const auto begin = std::chrono::steady_clock::now();
    function();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: all_plugin_cpu_probe output.bin\n";
        return 2;
    }

    std::vector<float> output;
    output.reserve(
        static_cast<std::size_t>(kWidth) *
        static_cast<std::size_t>(kHeight) *
        4u *
        5u);
    volatile float sink = 0.0f;

    const FakeSampler fakeSampler;
    const auto fakeControls =
        buckswood_fake::FakeDiagnosticCore::defaultControls();
    const buckswood_fake::FrameInfo fakeFrame{kWidth, kHeight, 42};
#if defined(BW_OPTIMIZED)
    const auto fakePrepared =
        buckswood_fake::FakeDiagnosticCore::prepare(
            fakeFrame,
            fakeControls);
#endif
    const double fakeMs = measure([&]() {
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
#if defined(BW_OPTIMIZED)
                const auto pixel =
                    buckswood_fake::FakeDiagnosticCore::processPixel(
                        fakeSampler,
                        x,
                        y,
                        fakeFrame,
                        fakeControls,
                        fakePrepared,
                        buckswood_fake::ViewAssistWithOverlay);
#else
                const auto pixel =
                    buckswood_fake::FakeDiagnosticCore::processPixel(
                        fakeSampler,
                        x,
                        y,
                        fakeFrame,
                        fakeControls,
                        buckswood_fake::ViewAssistWithOverlay);
#endif
                append(output, pixel);
                sink += pixel.r * 0.000001f;
            }
        }
    });

    const FilmSampler filmSampler;
    const auto filmControlValues = filmControls();
    const buckswood_film::FrameInfo filmFrame{kWidth, kHeight, 42};
#if defined(BW_OPTIMIZED)
    const auto filmPrepared =
        buckswood_film::FilmEmulationCore::prepare(
            filmFrame,
            filmControlValues);
#endif
    const double filmMs = measure([&]() {
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
#if defined(BW_OPTIMIZED)
                const auto pixel =
                    buckswood_film::FilmEmulationCore::processPixel(
                        filmSampler,
                        x,
                        y,
                        filmFrame,
                        filmControlValues,
                        filmPrepared,
                        nullptr);
#else
                const auto pixel =
                    buckswood_film::FilmEmulationCore::processPixel(
                        filmSampler,
                        x,
                        y,
                        filmFrame,
                        filmControlValues);
#endif
                append(output, pixel);
                sink += pixel.g * 0.000001f;
            }
        }
    });

    const CinematicSampler cinematicSampler;
    const buckswood_cinematic::FrameInfo cinematicFrame{
        kWidth,
        kHeight,
        42,
    };
    const auto radianceControls =
        buckswood_cinematic::CinematicToolsCore::
            defaultRadianceControls();
#if defined(BW_OPTIMIZED)
    const auto radiancePrepared =
        buckswood_cinematic::CinematicToolsCore::prepareRadiance(
            radianceControls);
#endif
    const double radianceMs = measure([&]() {
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
#if defined(BW_OPTIMIZED)
                const auto pixel =
                    buckswood_cinematic::CinematicToolsCore::
                        processRadiance(
                            cinematicSampler,
                            x,
                            y,
                            cinematicFrame,
                            radianceControls,
                            radiancePrepared,
                            nullptr);
#else
                const auto pixel =
                    buckswood_cinematic::CinematicToolsCore::
                        processRadiance(
                            cinematicSampler,
                            x,
                            y,
                            cinematicFrame,
                            radianceControls,
                            nullptr);
#endif
                append(output, pixel);
                sink += pixel.b * 0.000001f;
            }
        }
    });

    auto frameControls =
        buckswood_cinematic::CinematicToolsCore::
            defaultFrameDirectorControls();
    const buckswood_cinematic::FocusAnalysis focus{0.55f, 0.44f, 0.82f};
    const auto crop =
        buckswood_cinematic::CinematicToolsCore::cropWindow(
            cinematicFrame,
            frameControls,
            focus);
#if defined(BW_OPTIMIZED)
    const auto framePrepared =
        buckswood_cinematic::CinematicToolsCore::
            prepareFrameDirector(
                cinematicFrame,
                frameControls,
                crop);
#endif
    const double frameMs = measure([&]() {
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
#if defined(BW_OPTIMIZED)
                const auto pixel =
                    buckswood_cinematic::CinematicToolsCore::
                        processFrameDirector(
                            cinematicSampler,
                            x,
                            y,
                            cinematicFrame,
                            frameControls,
                            focus,
                            crop,
                            framePrepared);
#else
                const auto pixel =
                    buckswood_cinematic::CinematicToolsCore::
                        processFrameDirector(
                            cinematicSampler,
                            x,
                            y,
                            cinematicFrame,
                            frameControls,
                            focus,
                            crop);
#endif
                append(output, pixel);
                sink += pixel.r * 0.000001f;
            }
        }
    });

    const LookSampler lookSource(0.0f);
    const LookSampler lookReference(1.0f);
    buckswood_lookdna::Controls lookControls;
    lookControls.analysisQuality = 0;
    const auto sourceProfile =
        buckswood_lookdna::LookDNACore::analyze(
            lookSource,
            lookControls.inputSpace,
            lookControls.analysisQuality);
    const auto referenceProfile =
        buckswood_lookdna::LookDNACore::analyze(
            lookReference,
            lookControls.referenceSpace,
            lookControls.analysisQuality);
    const auto lookMatch =
        buckswood_lookdna::LookDNACore::buildMatch(
            sourceProfile,
            referenceProfile,
            lookControls);
    const buckswood_lookdna::FrameInfo lookFrame{kWidth, kHeight, 42};
    const double lookMs = measure([&]() {
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                const auto pixel =
                    buckswood_lookdna::LookDNACore::processPixel(
                        lookSource,
                        x,
                        y,
                        lookFrame,
                        lookControls,
                        lookMatch,
                        nullptr);
                append(output, pixel);
                sink += pixel.g * 0.000001f;
            }
        }
    });

    std::ofstream stream(argv[1], std::ios::binary);
    stream.write(
        reinterpret_cast<const char*>(output.data()),
        static_cast<std::streamsize>(
            output.size() * sizeof(float)));
    if (!stream) {
        std::cerr << "failed to write parity output\n";
        return 1;
    }

    std::cout
        << "fake_ms=" << fakeMs << '\n'
        << "film_ms=" << filmMs << '\n'
        << "radiance_ms=" << radianceMs << '\n'
        << "frame_director_ms=" << frameMs << '\n'
        << "look_dna_ms=" << lookMs << '\n'
        << "sink=" << sink << '\n';
    return 0;
}
