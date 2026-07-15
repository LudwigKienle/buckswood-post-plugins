#include "CinematicToolsCore.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using buckswood_cinematic::CinematicToolsCore;
using buckswood_cinematic::FrameInfo;
using buckswood_cinematic::Pixel;

class ArraySampler final : public buckswood_cinematic::Sampler {
public:
    ArraySampler(int width, int height, Pixel fill)
        : width_(width)
        , height_(height)
        , pixels_(static_cast<std::size_t>(width * height), fill)
    {
    }

    void set(int x, int y, Pixel pixel)
    {
        pixels_[static_cast<std::size_t>(y * width_ + x)] = pixel;
    }

    Pixel sample(float x, float y) const override
    {
        const int ix = std::max(0, std::min(width_ - 1, static_cast<int>(x + 0.5f)));
        const int iy = std::max(0, std::min(height_ - 1, static_cast<int>(y + 0.5f)));
        return pixels_[static_cast<std::size_t>(iy * width_ + ix)];
    }

private:
    int width_;
    int height_;
    std::vector<Pixel> pixels_;
};

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

float luma(Pixel pixel)
{
    return pixel.r * 0.2126f + pixel.g * 0.7152f + pixel.b * 0.0722f;
}

} // namespace

int main()
{
    const FrameInfo frame{192, 108, 42};
    auto frameControls = CinematicToolsCore::defaultFrameDirectorControls();
    require(std::fabs(CinematicToolsCore::aspectRatioForChoice(0) - 2.39f) < 0.001f, "2.39 aspect mapping");

    ArraySampler framingSampler(frame.width, frame.height, Pixel{0.12f, 0.12f, 0.12f, 1.0f});
    for (int y = 24; y < 78; ++y) {
        for (int x = 122; x < 166; ++x) {
            const float stripe = (x + y) % 4 == 0 ? 0.16f : 0.0f;
            framingSampler.set(x, y, Pixel{0.68f + stripe, 0.42f + stripe, 0.30f, 1.0f});
        }
    }
    const auto focus = CinematicToolsCore::analyzeFocus(framingSampler, frame, frameControls);
    require(focus.x > 0.53f, "focus analysis follows detailed warm subject");
    frameControls.subjectLock = true;
    frameControls.lockX = 0.22f;
    frameControls.lockY = 0.31f;
    const auto lockedFocus = CinematicToolsCore::smoothFocus(
        focus,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        frameControls);
    require(std::fabs(lockedFocus.x - frameControls.lockX) < 0.15f, "subject lock holds horizontal anchor");
    require(std::fabs(lockedFocus.y - frameControls.lockY) < 0.15f, "subject lock holds vertical anchor");
    frameControls.subjectLock = false;
    const auto crop = CinematicToolsCore::cropWindow(frame, frameControls, focus);
    require(crop.y2 - crop.y1 < static_cast<float>(frame.height), "2.39 crop removes vertical area");
    require(crop.x1 == 0.0f && crop.x2 == static_cast<float>(frame.width), "2.39 crop preserves width");

    ArraySampler neutralFrame(frame.width, frame.height, Pixel{0.25f, 0.25f, 0.25f, 1.0f});
    frameControls.framingMode = buckswood_cinematic::FramingCenter;
    const buckswood_cinematic::FocusAnalysis centerFocus{0.5f, 0.5f, 1.0f};
    const auto centerCrop = CinematicToolsCore::cropWindow(frame, frameControls, centerFocus);
    const Pixel matteOutside = CinematicToolsCore::processFrameDirector(
        neutralFrame,
        96,
        0,
        frame,
        frameControls,
        centerFocus,
        centerCrop);
    require(luma(matteOutside) < 0.01f, "cinematic crop mattes pixels outside target aspect");
    const Pixel cleanResult = CinematicToolsCore::processFrameDirector(
        neutralFrame,
        64,
        54,
        frame,
        frameControls,
        centerFocus,
        centerCrop);
    require(std::fabs(cleanResult.r - 0.25f) < 0.001f, "final crop does not burn in guide lines");
    frameControls.viewMode = buckswood_cinematic::FrameDirectorGuides;
    const Pixel guideResult = CinematicToolsCore::processFrameDirector(
        neutralFrame,
        64,
        54,
        frame,
        frameControls,
        centerFocus,
        centerCrop);
    require(std::fabs(guideResult.r - 0.25f) > 0.02f, "guide view visibly overlays composition lines");

    ArraySampler brightSampler(16, 16, Pixel{1.0f, 0.98f, 0.95f, 1.0f});
    auto radianceControls = CinematicToolsCore::defaultRadianceControls();
    radianceControls.recoveredHeadroomStops = 2.0f;
    const Pixel radiance = CinematicToolsCore::processRadiance(
        brightSampler,
        8,
        8,
        FrameInfo{16, 16, 4},
        radianceControls,
        nullptr);
    require(luma(radiance) > 1.0f, "radiance recovery creates float headroom above one");

    ArraySampler current(8, 8, Pixel{0.75f, 0.75f, 0.75f, 1.0f});
    ArraySampler previous(8, 8, Pixel{0.50f, 0.50f, 0.50f, 1.0f});
    ArraySampler next(8, 8, Pixel{0.51f, 0.51f, 0.51f, 1.0f});
    ArraySampler previous2(8, 8, Pixel{0.49f, 0.49f, 0.49f, 1.0f});
    ArraySampler next2(8, 8, Pixel{0.50f, 0.50f, 0.50f, 1.0f});
    buckswood_cinematic::TemporalContext temporal{
        &previous2,
        &previous,
        &next,
        &next2,
        true,
        true,
        true,
        true,
    };
    auto temporalControls = CinematicToolsCore::defaultTemporalIntegrityControls();
    temporalControls.repairStrength = 1.0f;
    temporalControls.flickerRepair = 1.0f;
    const Pixel repaired = CinematicToolsCore::processTemporalIntegrity(
        current,
        4,
        4,
        FrameInfo{8, 8, 5},
        temporalControls,
        &temporal);
    require(luma(repaired) < 0.74f, "temporal outlier is pulled toward agreeing neighbors");

    temporalControls.viewMode = buckswood_cinematic::TemporalMotionMatte;
    ArraySampler movingPrevious(8, 8, Pixel{0.05f, 0.05f, 0.05f, 1.0f});
    ArraySampler movingNext(8, 8, Pixel{0.95f, 0.95f, 0.95f, 1.0f});
    ArraySampler movingPrevious2(8, 8, Pixel{0.90f, 0.90f, 0.90f, 1.0f});
    ArraySampler movingNext2(8, 8, Pixel{0.10f, 0.10f, 0.10f, 1.0f});
    buckswood_cinematic::TemporalContext movingTemporal{
        &movingPrevious2,
        &movingPrevious,
        &movingNext,
        &movingNext2,
        true,
        true,
        true,
        true,
    };
    const Pixel motionMatte = CinematicToolsCore::processTemporalIntegrity(
        current,
        4,
        4,
        FrameInfo{8, 8, 5},
        temporalControls,
        &movingTemporal);
    require(luma(motionMatte) > 0.9f, "motion guard detects large inter-frame movement");

    std::cout << "Buckswood Cinematic Tools smoke tests passed\n";
    return 0;
}
