#include "OpticsLabCore.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using buckswood_optics::Controls;
using buckswood_optics::FrameInfo;
using buckswood_optics::OpticsLabCore;
using buckswood_optics::Pixel;

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool near(float a, float b, float epsilon = 0.0001f)
{
    return std::fabs(a - b) <= epsilon;
}

struct TestSampler {
    mutable int sampleCount = 0;

    Pixel sample(float x, float y) const
    {
        ++sampleCount;
        const float checker = (static_cast<int>(std::floor(x / 4.0f)) +
                               static_cast<int>(std::floor(y / 4.0f))) % 2
            ? 0.15f
            : 1.75f;
        return Pixel{
            checker + x * 0.001f,
            checker * 0.9f + y * 0.001f,
            checker * 0.8f,
            std::min(1.0f, std::max(0.0f, x / 63.0f)),
        };
    }
};

Controls defaults()
{
    Controls c{};
    c.preset = 0;
    c.effectStrength = 1.0f;
    c.focalLength = 50.0f;
    c.fStop = 2.8f;
    c.focusDistance = 3.0f;
    c.sensorWidth = 36.0f;
    c.anamorphicSqueeze = 1.0f;
    c.anamorphicAngle = 0.0f;
    c.bloomThreshold = 0.82f;
    c.depthNear = 0.0f;
    c.depthFar = 1.0f;
    c.depthGamma = 1.0f;
    c.focusPlane = 0.5f;
    c.grainSize = 1.0f;
    c.grainSeed = 1.0f;
    c.sensorISO = 400.0f;
    c.apertureInfluence = 1.0f;
    c.dirtScale = 1.0f;
    c.smudgeScale = 1.0f;
    c.edgeGuard = 0.8f;
    c.outputMix = 1.0f;
    return c;
}

} // namespace

int main()
{
    const FrameInfo frame{64, 48, 12};
    const TestSampler sampler;

    Controls neutral = defaults();
    const Pixel dry = sampler.sample(20.0f, 17.0f);
    const Pixel identity = OpticsLabCore::processPixel(sampler, 20, 17, frame, neutral);
    require(near(identity.r, dry.r), "manual neutral preserves red");
    require(near(identity.g, dry.g), "manual neutral preserves green");
    require(near(identity.b, dry.b), "manual neutral preserves blue");
    require(near(identity.a, dry.a), "manual neutral preserves alpha");

    TestSampler identitySampler;
    OpticsLabCore::processPixel(
        identitySampler,
        20,
        17,
        frame,
        neutral);
    require(
        identitySampler.sampleCount == 1,
        "neutral render uses the single-sample fast path");

    Controls hdr = defaults();
    hdr.preset = 1;
    const Pixel hdrResult = OpticsLabCore::processPixel(sampler, 20, 17, frame, hdr);
    require(
        hdrResult.r > 1.0f || hdrResult.g > 1.0f || hdrResult.b > 1.0f,
        "float render does not clamp HDR highlights");
    require(near(hdrResult.a, dry.a), "effect preserves source alpha");
    require(
        std::isfinite(hdrResult.r) && std::isfinite(hdrResult.g) && std::isfinite(hdrResult.b),
        "effect output remains finite");

    Controls depth = defaults();
    depth.preset = 0;
    depth.depthSource = 1;
    depth.focusPlane = 0.5f;
    depth.defocus = 1.0f;
    depth.fStop = 1.0f;
    const Pixel depthResult = OpticsLabCore::processPixel(sampler, 8, 17, frame, depth);
    require(
        std::fabs(depthResult.r - sampler.sample(8.0f, 17.0f).r) > 0.001f,
        "source alpha drives depth-aware defocus");

    Controls invertedDepth = depth;
    invertedDepth.focusPlane = 0.2f;
    invertedDepth.depthInvert = true;
    const Pixel invertedDepthResult =
        OpticsLabCore::processPixel(
            sampler,
            8,
            17,
            frame,
            invertedDepth);
    require(
        std::fabs(invertedDepthResult.r - depthResult.r) > 0.001f,
        "alpha depth inversion changes calibrated defocus");

    Controls lowIso = defaults();
    lowIso.grain = 0.35f;
    lowIso.sensorISO = 100.0f;
    const Pixel lowIsoResult =
        OpticsLabCore::processPixel(
            sampler,
            20,
            17,
            frame,
            lowIso);
    Controls highIso = lowIso;
    highIso.sensorISO = 1600.0f;
    const Pixel highIsoResult =
        OpticsLabCore::processPixel(
            sampler,
            20,
            17,
            frame,
            highIso);
    require(
        std::fabs(highIsoResult.g - dry.g) >
            std::fabs(lowIsoResult.g - dry.g) * 2.5f,
        "sensor ISO scales grain energy");

    std::vector<float> aperturePixels = {
        0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f,
    };
    const buckswood_optics::AssetViews assets{
        buckswood_optics::TextureView{aperturePixels.data(), 3, 3},
        buckswood_optics::TextureView{},
        buckswood_optics::TextureView{},
    };
    const auto prepared = OpticsLabCore::prepare(frame, depth);
    const Pixel apertureResult = OpticsLabCore::processPixel(
        sampler,
        8,
        17,
        prepared,
        &assets);
    require(
        std::isfinite(apertureResult.r),
        "custom aperture texture produces a valid render");

    Controls preview = defaults();
    preview.preset = 20;
    preview.quality = 0;
    const auto previewState = OpticsLabCore::prepare(frame, preview);
    require(
        previewState.defocusSamples == 8 &&
        previewState.glowSamples == 4 &&
        previewState.comaSamples == 2,
        "Preview quality lowers only the expensive optical sample counts");

    Controls bypassed = preview;
    bypassed.geometryEnabled = false;
    bypassed.aberrationsEnabled = false;
    bypassed.defocusEnabled = false;
    bypassed.lightEnabled = false;
    bypassed.vignetteEnabled = false;
    bypassed.surfaceEnabled = false;
    bypassed.sensorEnabled = false;
    TestSampler bypassSampler;
    const Pixel bypassResult = OpticsLabCore::processPixel(
        bypassSampler,
        20,
        17,
        frame,
        bypassed);
    require(
        bypassSampler.sampleCount == 1 && near(bypassResult.r, dry.r),
        "disabled stages collapse to the zero-cost identity path");

    Controls metric = defaults();
    metric.focusDistance = 300.0f;
    metric.sceneUnits = 1;
    const auto metricState = OpticsLabCore::prepare(frame, metric);
    require(
        near(metricState.focusDistanceMeters, 3.0f),
        "scene units convert focus distance to meters");

    Controls foreground = defaults();
    foreground.defocus = 1.0f;
    foreground.catEye = 1.0f;
    foreground.fStop = 1.0f;
    foreground.irisBlades = 5;
    foreground.focusOffset = -0.8f;
    const Pixel foregroundResult = OpticsLabCore::processPixel(
        sampler, 56, 18, frame, foreground);
    foreground.focusOffset = 0.8f;
    const Pixel backgroundResult = OpticsLabCore::processPixel(
        sampler, 56, 18, frame, foreground);
    require(
        std::fabs(foregroundResult.r - backgroundResult.r) > 0.0001f,
        "foreground bokeh mirrors cat-eye and odd iris orientation");

    std::cout << "Buckswood Optics Lab core tests passed\n";
    return 0;
}
