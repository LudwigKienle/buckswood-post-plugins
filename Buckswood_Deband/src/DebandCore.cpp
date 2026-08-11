#include "DebandCore.h"

#include <algorithm>
#include <cmath>

namespace buckswood_deband {

namespace {

void applyPreset(Controls& controls)
{
    switch (controls.preset) {
    case PresetSubtle10Bit:
        controls.strength *= 0.68f;
        controls.detection *= 0.78f;
        controls.radius *= 0.68f;
        controls.edgeProtection = std::max(controls.edgeProtection, 0.92f);
        controls.textureProtection = std::max(controls.textureProtection, 0.92f);
        controls.chromaRepair *= 0.58f;
        controls.dither *= 0.55f;
        if (controls.sourcePrecision == PrecisionAuto) {
            controls.sourcePrecision = Precision10Bit;
        }
        break;
    case PresetSky8Bit:
        controls.strength *= 1.18f;
        controls.detection *= 1.28f;
        controls.radius *= 1.35f;
        controls.edgeProtection = std::max(controls.edgeProtection, 0.86f);
        controls.textureProtection = std::max(controls.textureProtection, 0.84f);
        controls.chromaRepair *= 1.28f;
        controls.dither *= 1.55f;
        if (controls.sourcePrecision == PrecisionAuto) {
            controls.sourcePrecision = Precision8Bit;
        }
        break;
    case PresetAIFootage:
        controls.strength *= 1.04f;
        controls.detection *= 1.16f;
        controls.radius *= 1.15f;
        controls.edgeProtection = std::max(controls.edgeProtection, 0.90f);
        controls.textureProtection = std::max(controls.textureProtection, 0.88f);
        controls.chromaRepair *= 0.88f;
        controls.dither *= 0.98f;
        break;
    case PresetHeavyCompression:
        controls.strength *= 1.32f;
        controls.detection *= 1.48f;
        controls.radius *= 1.75f;
        controls.edgeProtection *= 0.90f;
        controls.textureProtection *= 0.90f;
        controls.chromaRepair *= 1.70f;
        controls.dither *= 1.82f;
        if (controls.sourcePrecision == PrecisionAuto) {
            controls.sourcePrecision = Precision8Bit;
        }
        break;
    case PresetManual:
        break;
    case PresetBalanced:
    default:
        break;
    }
}

} // namespace

DebandCore::PreparedState DebandCore::prepare(
    const FrameInfo& frame,
    const Controls& inputControls)
{
    Controls controls = inputControls;
    if (controls.preset != PresetManual) {
        applyPreset(controls);
    }

    controls.strength = std::clamp(controls.strength, 0.0f, 1.0f);
    controls.detection = std::clamp(controls.detection, 0.0f, 1.0f);
    controls.radius = std::clamp(controls.radius, 2.0f, 24.0f);
    controls.edgeProtection = std::clamp(controls.edgeProtection, 0.0f, 1.0f);
    controls.textureProtection = std::clamp(controls.textureProtection, 0.0f, 1.0f);
    controls.chromaRepair = std::clamp(controls.chromaRepair, 0.0f, 1.0f);
    controls.dither = std::clamp(controls.dither, 0.0f, 1.0f);
    controls.outputMix = std::clamp(controls.outputMix, 0.0f, 1.0f);
    controls.seed = std::clamp(controls.seed, 1.0f, 1000.0f);

    const int large = std::max(2, static_cast<int>(std::lround(controls.radius)));
    const int medium = std::max(2, static_cast<int>(std::lround(controls.radius * 0.55f)));
    const int small = std::max(1, static_cast<int>(std::lround(controls.radius * 0.25f)));

    int effectivePrecision = controls.sourcePrecision;
    if (effectivePrecision == PrecisionAuto) {
        effectivePrecision = Precision10Bit;
    }
    int levels = 1023;
    if (effectivePrecision == Precision8Bit) {
        levels = 255;
    } else if (effectivePrecision == Precision12Bit) {
        levels = 4095;
    }

    const float codeStep = 1.0f / static_cast<float>(levels);
    const float detectionScale = 0.65f + controls.detection * 1.50f;
    const float threshold = std::clamp(
        codeStep * 5.5f * detectionScale,
        0.0030f,
        0.055f);
    const float noiseFloor = std::max(
        0.00008f,
        codeStep * (0.22f + 0.25f * controls.detection));

    return PreparedState{
        controls,
        frame.frameIndex,
        small,
        medium,
        large,
        threshold,
        noiseFloor,
        codeStep,
    };
}

} // namespace buckswood_deband
