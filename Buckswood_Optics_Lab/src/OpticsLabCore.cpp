#include "OpticsLabCore.h"

namespace buckswood_optics {

OpticsLabCore::PreparedState OpticsLabCore::prepare(
    const FrameInfo& frame,
    const Controls& controls)
{
    Controls preparedControls = controls;
    preparedControls.edgeGuard = clamp01(controls.edgeGuard);
    preparedControls.outputMix = clamp01(controls.outputMix);
    preparedControls.depthNear = clamp01(controls.depthNear);
    preparedControls.depthFar = std::max(
        preparedControls.depthNear + 0.0001f,
        clamp01(controls.depthFar));
    preparedControls.depthGamma =
        clamp(controls.depthGamma, 0.10f, 4.0f);
    preparedControls.focusPlane = clamp01(controls.focusPlane);
    preparedControls.anamorphicSqueeze =
        clamp(controls.anamorphicSqueeze, 1.0f, 2.0f);
    preparedControls.apertureInfluence =
        clamp01(controls.apertureInfluence);
    preparedControls.dirtAmount = clamp01(controls.dirtAmount);
    preparedControls.dirtScale =
        clamp(controls.dirtScale, 0.25f, 8.0f);
    preparedControls.smudgeAmount = clamp01(controls.smudgeAmount);
    preparedControls.smudgeScale =
        clamp(controls.smudgeScale, 0.25f, 8.0f);
    preparedControls.grainSize =
        clamp(controls.grainSize, 0.5f, 4.0f);

    Model model = modelForPreset(controls.preset);
    model.distortion += controls.distortion * 0.16f;
    model.breathing += controls.breathing;
    model.lateralCA += controls.lateralCA;
    model.axialCA += controls.axialCA;
    model.coma += controls.coma;
    model.astigmatism += controls.astigmatism;
    model.fieldCurvature += controls.fieldCurvature;
    model.spherical += controls.spherical;
    model.swirl += controls.swirl;
    model.defocus += controls.defocus;
    model.catEye += controls.catEye;
    model.bloom += controls.bloom;
    model.diffusion += controls.diffusion;
    model.halation += controls.halation;
    model.flareGhosts += controls.flareGhosts;
    model.flareStreak += controls.flareStreak;
    model.starburst += controls.starburst;
    model.vignette += controls.vignette;
    model.debayer += controls.debayer;
    model.chromaSmear += controls.chromaSmear;
    model.grain += controls.grain;

    const float width = static_cast<float>(std::max(1, frame.width));
    const float height = static_cast<float>(std::max(1, frame.height));
    const float focalLength = clamp(controls.focalLength, 8.0f, 300.0f);
    const float sensorWidth = clamp(controls.sensorWidth, 8.0f, 70.0f);
    const float fStop = clamp(controls.fStop, 0.7f, 32.0f);
    const float focusDistance = clamp(controls.focusDistance, 0.2f, 1000.0f);
    const float focalScale = clamp((50.0f / focalLength) * (sensorWidth / 36.0f), 0.35f, 3.5f);
    const float apertureScale = clamp(2.8f / fStop, 0.12f, 3.5f);
    const float breathingScale =
        1.0f + model.breathing * clamp(1.0f / focusDistance, 0.0f, 2.0f) * 0.045f;
    const float anamorphicRadians =
        controls.anamorphicAngle *
        0.01745329251994329577f;
    const float amount = clamp01(controls.effectStrength);
    const bool needsEdgeGuard =
        controls.edgeGuard > 0.0001f &&
        amount > 0.0001f &&
        (
            model.lateralCA > 0.0001f ||
            model.axialCA > 0.0001f ||
            model.coma > 0.0001f ||
            model.astigmatism > 0.0001f ||
            model.fieldCurvature > 0.0001f ||
            model.spherical > 0.0001f ||
            model.defocus > 0.0001f);
    const bool identityMapping =
        std::fabs(model.distortion * amount) <= 0.000001f &&
        std::fabs(model.swirl * amount) <= 0.000001f &&
        std::fabs(breathingScale - 1.0f) <= 0.000001f;
    const bool identityOutput =
        identityMapping &&
        model.lateralCA == 0.0f &&
        model.axialCA == 0.0f &&
        model.coma == 0.0f &&
        model.astigmatism == 0.0f &&
        model.fieldCurvature == 0.0f &&
        model.spherical == 0.0f &&
        model.defocus == 0.0f &&
        model.bloom == 0.0f &&
        model.diffusion == 0.0f &&
        model.halation == 0.0f &&
        model.flareGhosts == 0.0f &&
        model.flareStreak == 0.0f &&
        model.starburst == 0.0f &&
        model.vignette == 0.0f &&
        model.debayer == 0.0f &&
        model.chromaSmear == 0.0f &&
        model.grain == 0.0f &&
        preparedControls.dirtAmount <= 0.0001f &&
        preparedControls.smudgeAmount <= 0.0001f;

    return PreparedState{
        model,
        preparedControls,
        width,
        height,
        frame.frameIndex,
        (width - 1.0f) * 0.5f,
        (height - 1.0f) * 0.5f,
        width / height,
        amount,
        apertureScale,
        focalScale,
        breathingScale,
        std::cos(anamorphicRadians),
        std::sin(anamorphicRadians),
        std::sqrt(
            clamp(controls.sensorISO, 50.0f, 12800.0f) /
            400.0f),
        needsEdgeGuard,
        identityMapping,
        identityOutput,
    };
}

OpticsLabCore::Model OpticsLabCore::modelForPreset(int preset)
{
    switch (preset) {
    case 1: // Modern Cinema
        return Model{0.010f, 0.04f, 0.035f, 0.020f, 0.015f, 0.010f, 0.015f, 0.010f, 0.000f, 0.00f, 0.02f, 0.045f, 0.020f, 0.010f, 0.005f, 0.000f, 0.005f, 0.035f, 0.050f, 0.035f, 0.025f, 0.004f};
    case 2: // Warm Classic
        return Model{-0.030f, 0.10f, 0.080f, 0.070f, 0.070f, 0.055f, 0.075f, 0.050f, 0.025f, 0.02f, 0.10f, 0.120f, 0.090f, 0.100f, 0.040f, 0.010f, 0.015f, 0.100f, 0.080f, 0.080f, 0.060f, 0.035f};
    case 3: // Fast Vintage
        return Model{-0.055f, 0.18f, 0.140f, 0.160f, 0.180f, 0.120f, 0.150f, 0.120f, 0.060f, 0.08f, 0.28f, 0.180f, 0.140f, 0.160f, 0.080f, 0.015f, 0.030f, 0.160f, 0.120f, 0.110f, 0.085f, 0.045f};
    case 4: // Anamorphic Character
        return Model{0.025f, 0.16f, 0.160f, 0.120f, 0.100f, 0.110f, 0.085f, 0.070f, 0.025f, 0.05f, 0.22f, 0.155f, 0.100f, 0.125f, 0.060f, 0.180f, 0.025f, 0.090f, 0.090f, 0.090f, 0.055f, 0.012f};
    case 5: // Petzval Portrait
        return Model{-0.070f, 0.20f, 0.100f, 0.145f, 0.200f, 0.170f, 0.230f, 0.110f, 0.250f, 0.10f, 0.45f, 0.140f, 0.115f, 0.120f, 0.045f, 0.000f, 0.015f, 0.220f, 0.070f, 0.080f, 0.060f, 0.025f};
    case 6: // AI Deplastic
        return Model{-0.012f, 0.05f, 0.040f, 0.025f, 0.030f, 0.025f, 0.025f, 0.025f, 0.004f, 0.00f, 0.03f, 0.080f, 0.075f, 0.055f, 0.015f, 0.000f, 0.006f, 0.050f, 0.160f, 0.120f, 0.075f, 0.006f};
    case 7: // Large Format Clean
        return Model{-0.004f, 0.025f, 0.018f, 0.012f, 0.008f, 0.006f, 0.010f, 0.008f, 0.000f, 0.00f, 0.01f, 0.030f, 0.018f, 0.006f, 0.003f, 0.000f, 0.003f, 0.018f, 0.035f, 0.020f, 0.018f, 0.002f};
    case 8: // Dream Diffusion
        return Model{-0.018f, 0.08f, 0.055f, 0.080f, 0.040f, 0.045f, 0.050f, 0.080f, 0.020f, 0.03f, 0.12f, 0.220f, 0.240f, 0.210f, 0.030f, 0.020f, 0.020f, 0.110f, 0.060f, 0.070f, 0.055f, 0.018f};
    case 0:
    default:
        return Model{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    }
}

} // namespace buckswood_optics
