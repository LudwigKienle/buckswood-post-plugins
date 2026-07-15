#include "FilmEmulationCore.h"

#include <cmath>
#include <iomanip>
#include <iostream>

namespace {

class TestSampler final : public buckswood_film::Sampler {
public:
    buckswood_film::Pixel sample(float x, float y) const override
    {
        const float nx = x / 1919.0f;
        const float ny = y / 1079.0f;
        const float face =
            (x > 760.0f && x < 1160.0f && y > 260.0f && y < 790.0f) ? 1.0f : 0.0f;
        const float hot = (x > 1500.0f && y < 260.0f) ? 1.6f : 0.0f;
        return buckswood_film::Pixel{
            0.08f + nx * 0.58f + face * 0.20f + hot,
            0.09f + ny * 0.36f + face * 0.13f + hot * 0.82f,
            0.10f + (1.0f - nx) * 0.22f + face * 0.08f + hot * 0.42f,
            1.0f,
        };
    }
};

float luma(const buckswood_film::Pixel& pixel)
{
    return 0.2627f * pixel.r + 0.6780f * pixel.g + 0.0593f * pixel.b;
}

bool finitePixel(const buckswood_film::Pixel& pixel)
{
    return std::isfinite(pixel.r) &&
        std::isfinite(pixel.g) &&
        std::isfinite(pixel.b) &&
        std::isfinite(pixel.a);
}

buckswood_film::Controls defaultControls()
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
    c.exposure = 0.0f;
    c.pushPull = 0.0f;
    c.density = 0.18f;
    c.contrast = 1.0f;
    c.developerContrast = 0.10f;
    c.developerGamma = 0.0f;
    c.colorSeparation = 0.30f;
    c.colorBoost = 0.04f;
    c.saturation = 0.94f;
    c.temperature = 0.0f;
    c.tint = 0.0f;
    c.subtractiveColor = 0.34f;
    c.interlayer = 0.34f;
    c.neutralizeCurves = 0.0f;
    c.bleachBypass = 0.0f;
    c.filmCompression = 0.38f;
    c.compressionWhitePoint = 1.08f;
    c.compressionRange = 0.55f;
    c.compressionColorDensity = 0.48f;
    c.expandBlackPoint = 0.0f;
    c.expandWhitePoint = 0.0f;
    c.printerCyan = 0.0f;
    c.printerMagenta = 0.0f;
    c.printerYellow = 0.0f;
    c.highlightRolloff = 0.62f;
    c.blackLift = 0.0f;
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
    c.dust = 0.0f;
    c.scratches = 0.0f;
    c.flicker = 0.0f;
    c.filmBreath = 0.08f;
    c.temporalReconstruction = 0.0f;
    c.temporalStability = 0.62f;
    c.temporalMotionGuard = 0.82f;
    c.skinProtect = 0.72f;
    c.outputMix = 0.75f;
    return c;
}

} // namespace

int main()
{
    TestSampler sampler;
    buckswood_film::FrameInfo frame{1920, 1080, 42};
    buckswood_film::Controls controls = defaultControls();

    const int points[][2] = {
        {120, 120},
        {960, 540},
        {1700, 120},
        {1800, 960},
    };

    std::cout << std::fixed << std::setprecision(4);
    for (const auto& point : points) {
        const buckswood_film::Pixel out = buckswood_film::FilmEmulationCore::processPixel(
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
        if (!finitePixel(out) || out.a < 0.99f || out.a > 1.01f) {
            std::cerr << "Invalid output pixel\n";
            return 1;
        }
    }

    buckswood_film::Controls neutral = controls;
    neutral.stockPreset = 0;
    neutral.printPreset = 0;
    neutral.subtractiveColor = 0.0f;
    neutral.grain = 0.0f;
    neutral.halation = 0.0f;
    neutral.bloom = 0.0f;
    neutral.outputMix = 1.0f;

    const buckswood_film::Pixel ai = buckswood_film::FilmEmulationCore::processPixel(
        sampler,
        960,
        540,
        frame,
        controls);
    const buckswood_film::Pixel base = buckswood_film::FilmEmulationCore::processPixel(
        sampler,
        960,
        540,
        frame,
        neutral);
    const float aiDelta = std::fabs(ai.r - base.r) + std::fabs(ai.g - base.g) + std::fabs(ai.b - base.b);
    std::cout << "ai_delta " << aiDelta << "\n";
    if (aiDelta < 0.015f) {
        std::cerr << "AI film preset is too close to neutral\n";
        return 1;
    }

    buckswood_film::Controls noRoll = controls;
    noRoll.highlightRolloff = 0.0f;
    noRoll.bloom = 0.0f;
    noRoll.halation = 0.0f;
    noRoll.grain = 0.0f;
    noRoll.outputMix = 1.0f;
    buckswood_film::Controls roll = noRoll;
    roll.highlightRolloff = 1.0f;

    const float hotNoRoll = luma(buckswood_film::FilmEmulationCore::processPixel(sampler, 1700, 120, frame, noRoll));
    const float hotRoll = luma(buckswood_film::FilmEmulationCore::processPixel(sampler, 1700, 120, frame, roll));
    std::cout << "highlight_rolloff " << hotNoRoll << " -> " << hotRoll << "\n";
    if (!(hotRoll < hotNoRoll) || !std::isfinite(hotRoll)) {
        std::cerr << "Highlight rolloff regression\n";
        return 1;
    }

    buckswood_film::Controls grainView = controls;
    grainView.viewMode = 1;
    const buckswood_film::Pixel grain = buckswood_film::FilmEmulationCore::processPixel(
        sampler,
        480,
        270,
        frame,
        grainView);
    if (!finitePixel(grain) || grain.r < 0.0f || grain.r > 1.0f) {
        std::cerr << "Grain matte view regression\n";
        return 1;
    }

    buckswood_film::Controls textureOnly = controls;
    textureOnly.processMode = 2;
    textureOnly.outputMix = 1.0f;
    const buckswood_film::Pixel texture = buckswood_film::FilmEmulationCore::processPixel(
        sampler,
        960,
        540,
        frame,
        textureOnly);
    if (!finitePixel(texture)) {
        std::cerr << "Texture-only mode regression\n";
        return 1;
    }

    std::cout << "Buckswood Film Emulation smoke tests passed\n";
    return 0;
}
