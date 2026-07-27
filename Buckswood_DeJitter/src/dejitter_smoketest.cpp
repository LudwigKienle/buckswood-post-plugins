#include "DeJitterCore.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using buckswood_dejitter::Bounds;
using buckswood_dejitter::Pixel;

constexpr int kWidth = 320;
constexpr int kHeight = 180;
constexpr float kFeatureX = 158.0f;
constexpr float kFeatureY = 91.0f;

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

class FeatureSampler final : public buckswood_dejitter::Sampler {
public:
    FeatureSampler(float shiftX, float shiftY, bool flat = false)
        : shiftX_(shiftX)
        , shiftY_(shiftY)
        , flat_(flat)
    {
    }

    Pixel sample(float x, float y) const override
    {
        if (flat_) {
            return Pixel{0.18f, 0.18f, 0.18f, 1.0f};
        }
        const float fx = x - (kFeatureX + shiftX_);
        const float fy = y - (kFeatureY + shiftY_);
        const float spot = std::exp(
            -(fx * fx + fy * fy) / 48.0f);
        const float ring = std::exp(
            -(
                (std::sqrt(fx * fx + fy * fy) - 13.0f) *
                (std::sqrt(fx * fx + fy * fy) - 13.0f)) /
            9.0f);
        const float texture =
            std::sin((x - shiftX_) * 0.19f) *
            std::cos((y - shiftY_) * 0.13f) *
            0.06f;
        return Pixel{
            0.12f + spot * 0.78f + ring * 0.18f + texture,
            0.10f + spot * 0.48f + ring * 0.08f + texture,
            0.09f + spot * 0.22f + texture,
            1.0f,
        };
    }

    Bounds bounds() const override
    {
        return Bounds{0, 0, kWidth, kHeight};
    }

private:
    float shiftX_;
    float shiftY_;
    bool flat_;
};

class CutSampler final : public buckswood_dejitter::Sampler {
public:
    Pixel sample(float x, float y) const override
    {
        const float noise =
            std::sin(
                x * 12.9898f +
                y * 78.233f +
                std::sin(x * 0.071f) * 19.19f);
        const float value = 0.42f + noise * 0.31f;
        return Pixel{
            value,
            0.24f + value * 0.37f,
            0.68f - value * 0.41f,
            1.0f,
        };
    }

    Bounds bounds() const override
    {
        return Bounds{0, 0, kWidth, kHeight};
    }
};

float luma(Pixel pixel)
{
    return pixel.r * 0.2627f +
        pixel.g * 0.6780f +
        pixel.b * 0.0593f;
}

} // namespace

int main()
{
    auto controls =
        buckswood_dejitter::DeJitterCore::defaultControls();
    controls.trackX = kFeatureX;
    controls.trackY = kFeatureY;
    controls.regionWidth = 0.18f;
    controls.regionHeight = 0.24f;
    controls.searchRadius = 24;
    controls.temporalRadius = 2;
    controls.analysisQuality =
        buckswood_dejitter::QualityHigh;
    controls.stabilizationStrength = 1.0f;
    controls.confidenceThreshold = 0.05f;
    controls.maxCorrection = 20.0f;

    const buckswood_dejitter::FrameInfo frame{
        kWidth,
        kHeight,
        10,
    };

    const FeatureSampler current(3.0f, -2.0f);
    const FeatureSampler previous(0.0f, 0.0f);
    const FeatureSampler next(0.0f, 0.0f);
    const FeatureSampler previous2(0.0f, 0.0f);
    const FeatureSampler next2(0.0f, 0.0f);
    const buckswood_dejitter::TemporalContext temporal{
        &previous2,
        &previous,
        &next,
        &next2,
    };
    const auto tracked =
        buckswood_dejitter::DeJitterCore::analyze(
            current,
            temporal,
            frame,
            controls);
    require(tracked.valid, "jittered feature tracks");
    require(
        std::fabs(tracked.rawCorrection.x + 3.0f) < 0.45f,
        "horizontal jitter estimate");
    require(
        std::fabs(tracked.rawCorrection.y - 2.0f) < 0.45f,
        "vertical jitter estimate");
    require(
        tracked.confidence > 0.35f,
        "tracking confidence");

    auto snappedControls = controls;
    snappedControls.trackX = kFeatureX - 20.0f;
    snappedControls.textureSnap = true;
    snappedControls.textureSnapRadius = 28;
    const auto snapped =
        buckswood_dejitter::DeJitterCore::analyze(
            current,
            temporal,
            frame,
            snappedControls);
    require(snapped.pointSnapped, "texture snap moves the analysis center");
    require(
        std::fabs(snapped.trackCenter.x - (kFeatureX + 3.0f)) <
            std::fabs(
                snapped.requestedTrackCenter.x -
                (kFeatureX + 3.0f)),
        "texture snap moves toward the tracked feature");

    const Pixel before = current.sample(kFeatureX, kFeatureY);
    const Pixel after =
        buckswood_dejitter::DeJitterCore::processPixel(
            current,
            static_cast<int>(kFeatureX),
            static_cast<int>(kFeatureY),
            frame,
            controls,
            tracked);
    require(
        luma(after) > luma(before) + 0.08f,
        "stabilized sampling moves feature to target");

    auto defaultStrengthControls =
        buckswood_dejitter::DeJitterCore::defaultControls();
    defaultStrengthControls.trackX = kFeatureX;
    defaultStrengthControls.trackY = kFeatureY;
    defaultStrengthControls.regionWidth = 0.18f;
    defaultStrengthControls.regionHeight = 0.24f;
    defaultStrengthControls.searchRadius = 24;
    const auto defaultStrength =
        buckswood_dejitter::DeJitterCore::analyze(
            current,
            temporal,
            frame,
            defaultStrengthControls);
    if (
        !defaultStrength.valid ||
        std::fabs(defaultStrength.correction.x) <= 1.0f) {
        std::cerr
            << "Default correction=("
            << defaultStrength.correction.x
            << ", "
            << defaultStrength.correction.y
            << "), confidence="
            << defaultStrength.confidence
            << '\n';
    }
    require(
        defaultStrength.valid &&
            std::fabs(defaultStrength.correction.x) > 1.0f,
        "default controls visibly correct real jitter");

    const FeatureSampler subpixelCurrent(2.35f, -1.65f);
    const auto subpixel =
        buckswood_dejitter::DeJitterCore::analyze(
            subpixelCurrent,
            temporal,
            frame,
            controls);
    require(subpixel.valid, "subpixel jitter tracks");
    require(
        std::fabs(subpixel.rawCorrection.x + 2.35f) < 0.35f &&
            std::fabs(subpixel.rawCorrection.y - 1.65f) < 0.35f,
        "subpixel jitter estimate");

    const FeatureSampler linearCurrent(0.0f, 0.0f);
    const FeatureSampler linearPrevious(-2.0f, 1.0f);
    const FeatureSampler linearNext(2.0f, -1.0f);
    const FeatureSampler linearPrevious2(-4.0f, 2.0f);
    const FeatureSampler linearNext2(4.0f, -2.0f);
    const buckswood_dejitter::TemporalContext linearTemporal{
        &linearPrevious2,
        &linearPrevious,
        &linearNext,
        &linearNext2,
    };
    const auto linear =
        buckswood_dejitter::DeJitterCore::analyze(
            linearCurrent,
            linearTemporal,
            frame,
            controls);
    require(linear.valid, "linear move tracks");
    require(
        std::fabs(linear.correction.x) < 0.30f &&
            std::fabs(linear.correction.y) < 0.30f,
        "linear camera move is preserved");

    const FeatureSampler flat(0.0f, 0.0f, true);
    const buckswood_dejitter::TemporalContext flatTemporal{
        &flat,
        &flat,
        &flat,
        &flat,
    };
    const auto rejected =
        buckswood_dejitter::DeJitterCore::analyze(
            flat,
            flatTemporal,
            frame,
            controls);
    require(!rejected.valid, "flat tracking region is rejected");
    require(
        std::fabs(rejected.correction.x) < 0.0001f &&
            std::fabs(rejected.correction.y) < 0.0001f,
        "rejected track leaves image unchanged");

    auto cutControls = controls;
    cutControls.sceneCutProtection = 1.0f;
    const CutSampler cut;
    const buckswood_dejitter::TemporalContext cutTemporal{
        &cut,
        &cut,
        &cut,
        &cut,
    };
    const auto cutRejected =
        buckswood_dejitter::DeJitterCore::analyze(
            current,
            cutTemporal,
            frame,
            cutControls);
    require(!cutRejected.valid, "scene cut is rejected");
    require(
        std::fabs(cutRejected.correction.x) < 0.0001f &&
            std::fabs(cutRejected.correction.y) < 0.0001f,
        "scene cut leaves image unchanged");

    std::cout
        << "Buckswood DeJitter smoke tests passed. correction=("
        << tracked.correction.x
        << ", "
        << tracked.correction.y
        << "), confidence="
        << tracked.confidence
        << '\n';
    return 0;
}
