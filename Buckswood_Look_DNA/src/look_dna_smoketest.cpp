#include "LookDNACore.h"

#include <cmath>
#include <cstdio>
#include <iostream>

namespace {

class SyntheticSampler final : public buckswood_lookdna::Sampler {
public:
    enum Style { Source, WarmReference, CoolReference, CutFrame };

    explicit SyntheticSampler(Style style)
        : style_(style)
    {
    }

    buckswood_lookdna::Pixel sample(float x, float y) const override
    {
        const float nx = std::max(0.0f, std::min(1.0f, x / 639.0f));
        const float ny = std::max(0.0f, std::min(1.0f, y / 359.0f));
        const bool face = x > 245.0f && x < 395.0f && y > 82.0f && y < 292.0f;
        if (style_ == CutFrame) {
            return buckswood_lookdna::Pixel{
                0.72f - nx * 0.42f,
                0.12f + ny * 0.12f,
                0.68f - ny * 0.28f,
                1.0f,
            };
        }
        float r = 0.06f + nx * 0.58f + (face ? 0.18f : 0.0f);
        float g = 0.08f + ny * 0.38f + (face ? 0.11f : 0.0f);
        float b = 0.12f + (1.0f - nx) * 0.26f + (face ? 0.055f : 0.0f);
        if (style_ == WarmReference) {
            const auto curve = [](float value) {
                const float centered = (value - 0.42f) * 1.22f + 0.42f;
                return std::max(0.0f, centered);
            };
            r = curve(r) * 1.12f + 0.018f;
            g = curve(g) * 1.01f;
            b = curve(b) * 0.82f;
        } else if (style_ == CoolReference) {
            r = r * 0.82f;
            g = g * 1.04f + 0.01f;
            b = b * 1.18f + 0.02f;
        }
        return buckswood_lookdna::Pixel{r, g, b, 1.0f};
    }

    buckswood_lookdna::Bounds bounds() const override
    {
        return buckswood_lookdna::Bounds{0, 0, 640, 360};
    }

private:
    Style style_;
};

bool finitePixel(const buckswood_lookdna::Pixel& pixel)
{
    return std::isfinite(pixel.r) && std::isfinite(pixel.g) &&
        std::isfinite(pixel.b) && std::isfinite(pixel.a);
}

float pixelDistance(buckswood_lookdna::Pixel a, buckswood_lookdna::Pixel b)
{
    return std::fabs(a.r - b.r) + std::fabs(a.g - b.g) + std::fabs(a.b - b.b);
}

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    using namespace buckswood_lookdna;
    SyntheticSampler source(SyntheticSampler::Source);
    SyntheticSampler warm(SyntheticSampler::WarmReference);
    SyntheticSampler cool(SyntheticSampler::CoolReference);
    SyntheticSampler cut(SyntheticSampler::CutFrame);
    const LookProfile sourceProfile = LookDNACore::analyze(source, TimelineDisplay, 1);
    const LookProfile warmProfile = LookDNACore::analyze(warm, TimelineDisplay, 1);
    const LookProfile coolProfile = LookDNACore::analyze(cool, TimelineDisplay, 1);
    const LookProfile cutProfile = LookDNACore::analyze(cut, TimelineDisplay, 1);
    require(sourceProfile.valid, "source analysis");
    require(warmProfile.valid, "reference analysis");
    require(coolProfile.valid, "second reference analysis");
    require(cutProfile.valid, "cut frame analysis");

    Controls controls;
    controls.inputSpace = TimelineDisplay;
    controls.referenceSpace = TimelineDisplay;
    controls.matchStrength = 0.90f;
    controls.outputMix = 1.0f;
    const MatchContext warmMatch = LookDNACore::buildMatch(sourceProfile, warmProfile, controls);
    require(warmMatch.valid, "warm match context");
    require(warmMatch.confidence > 0.20f, "usable match confidence");

    const FrameInfo frame{640, 360, 42};
    const Pixel original = source.sample(520.0f, 140.0f);
    const Pixel matched = LookDNACore::processPixel(source, 520, 140, frame, controls, warmMatch);
    require(finitePixel(matched), "finite warm match");
    require(pixelDistance(original, matched) > 0.015f, "reference creates a visible match");
    require((matched.r - matched.b) > (original.r - original.b), "warm reference shifts red-blue balance");

    const MatchContext identityMatch = LookDNACore::buildMatch(sourceProfile, sourceProfile, controls);
    const Pixel identity = LookDNACore::processPixel(source, 520, 140, frame, controls, identityMatch);
    require(pixelDistance(original, identity) < 0.025f, "identity reference remains near identity");

    Controls unprotected = controls;
    unprotected.skinProtect = 0.0f;
    Controls protectedControls = controls;
    protectedControls.skinProtect = 1.0f;
    const Pixel skinOriginal = source.sample(320.0f, 180.0f);
    const Pixel skinUnprotected = LookDNACore::processPixel(source, 320, 180, frame, unprotected, warmMatch);
    const Pixel skinProtected = LookDNACore::processPixel(source, 320, 180, frame, protectedControls, warmMatch);
    require(
        pixelDistance(skinOriginal, skinProtected) < pixelDistance(skinOriginal, skinUnprotected),
        "skin guard reduces chroma intervention");

    const LookProfile stabilized = LookDNACore::stabilizeProfile(
        sourceProfile,
        &cutProfile,
        &cutProfile,
        1.0f);
    require(
        LookDNACore::profileDistance(stabilized, sourceProfile) < 0.08f,
        "cut guard rejects unrelated temporal frames");

    float temporalConfidence = 0.0f;
    const LookProfile stabilized5 = LookDNACore::stabilizeProfile5(
        sourceProfile,
        &cutProfile,
        &sourceProfile,
        &sourceProfile,
        &cutProfile,
        1.0f,
        &temporalConfidence);
    require(stabilized5.valid, "five-frame stabilization");
    require(temporalConfidence > 0.35f && temporalConfidence < 0.90f, "five-frame cut confidence");
    require(
        LookDNACore::profileDistance(stabilized5, sourceProfile) < 0.05f,
        "five-frame stabilization rejects distant cuts");

    const std::array<const LookProfile*, 3> references{
        &warmProfile,
        &coolProfile,
        &cutProfile,
    };
    const std::array<float, 3> requestedWeights{1.0f, 0.70f, 0.70f};
    std::array<float, 3> normalizedWeights{};
    const LookProfile blendedReference = LookDNACore::blendReferenceProfiles(
        sourceProfile,
        references,
        requestedWeights,
        1.0f,
        &normalizedWeights);
    require(blendedReference.valid, "multi-reference blend");
    require(
        normalizedWeights[0] + normalizedWeights[1] > normalizedWeights[2],
        "auto reference balance suppresses incompatible still");
    require(
        std::fabs(normalizedWeights[0] + normalizedWeights[1] + normalizedWeights[2] - 1.0f) < 0.0001f,
        "reference weights normalize");

    const ProfileGrid sourceGrid = LookDNACore::analyzeGrid(source, TimelineDisplay, 1);
    const ProfileGrid warmGrid = LookDNACore::analyzeGrid(warm, TimelineDisplay, 1);
    require(sourceGrid.valid && warmGrid.valid, "spatial 3x3 analysis");
    std::array<MatchContext, SpatialProfileCount> spatialMatches{};
    for (int cell = 0; cell < SpatialProfileCount; ++cell) {
        spatialMatches[static_cast<std::size_t>(cell)] = LookDNACore::buildMatch(
            sourceGrid.cells[static_cast<std::size_t>(cell)],
            warmGrid.cells[static_cast<std::size_t>(cell)],
            controls);
    }
    const MatchContext spatialMatch = LookDNACore::sampleSpatialMatch(
        warmMatch,
        spatialMatches,
        0.72f,
        0.28f,
        0.65f);
    require(spatialMatch.valid && spatialMatch.spatialInfluence > 0.1f, "spatial match interpolation");
    require(
        finitePixel(LookDNACore::processPixel(source, 460, 100, frame, controls, spatialMatch)),
        "finite spatial result");

    const char* profilePath = "/tmp/BuckswoodLookDNA_smoketest.bwlook";
    std::string error;
    require(LookDNACore::saveProfile(profilePath, warmProfile, &error), "save BWLOOK profile");
    LookProfile loaded;
    require(LookDNACore::loadProfile(profilePath, loaded, &error), "load BWLOOK profile");
    require(LookDNACore::profileDistance(warmProfile, loaded) < 0.00001f, "BWLOOK roundtrip");
    std::remove(profilePath);

    for (int view = ResultView; view <= CutGuardView; ++view) {
        Controls viewControls = controls;
        viewControls.viewMode = view;
        const Pixel output = LookDNACore::processPixel(
            source,
            420,
            210,
            frame,
            viewControls,
            warmMatch,
            &warm);
        require(finitePixel(output), "finite diagnostic view");
    }

    std::cout << "Buckswood Look DNA smoke tests passed\n";
    return 0;
}
