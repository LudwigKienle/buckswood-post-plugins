#include "FakeDiagnosticCore.h"

#include <algorithm>

namespace buckswood_fake {

FakeDiagnosticCore::PreparedState FakeDiagnosticCore::prepare(
    const FrameInfo& frame,
    const Controls& controls)
{
    const float weightSum = std::max(
        0.001f,
        controls.plasticWeight +
            controls.highlightWeight +
            controls.edgeWeight +
            controls.gradeWeight +
            controls.textureWeight +
            controls.temporalWeight);
    return PreparedState{
        weightSum,
        frame.frameIndex + static_cast<int>(controls.seed * 13.0f),
        frame.frameIndex + static_cast<int>(controls.seed * 29.0f),
    };
}

Controls FakeDiagnosticCore::defaultControls()
{
    return Controls{
        1.0f,  // sensitivity
        0.65f, // overlayStrength
        0.35f, // correctionStrength
        1.0f,  // plasticWeight
        1.0f,  // highlightWeight
        0.85f, // edgeWeight
        0.90f, // gradeWeight
        0.85f, // textureWeight
        0.80f, // temporalWeight
        0.35f, // microContrast
        0.15f, // edgeSoften
        0.55f, // highlightRolloff
        0.45f, // gamutGuard
        0.25f, // sensorTexture
        0.25f, // temporalAssist
        17.0f, // seed
    };
}

Pixel FakeDiagnosticCore::categoryColor(const Scores& scores)
{
    const Pixel plastic{1.0f, 0.0f, 0.85f, 1.0f};
    const Pixel highlight{1.0f, 0.82f, 0.05f, 1.0f};
    const Pixel edge{0.0f, 0.85f, 1.0f, 1.0f};
    const Pixel grade{1.0f, 0.08f, 0.02f, 1.0f};
    const Pixel texture{0.12f, 0.32f, 1.0f, 1.0f};
    const Pixel temporal{0.0f, 1.0f, 0.22f, 1.0f};

    const float sum = std::max(
        0.001f,
        scores.plastic + scores.highlight + scores.edge + scores.grade + scores.texture + scores.temporal);
    Pixel color{0.0f, 0.0f, 0.0f, 1.0f};
    color = add(color, mul(plastic, scores.plastic / sum));
    color = add(color, mul(highlight, scores.highlight / sum));
    color = add(color, mul(edge, scores.edge / sum));
    color = add(color, mul(grade, scores.grade / sum));
    color = add(color, mul(texture, scores.texture / sum));
    color = add(color, mul(temporal, scores.temporal / sum));
    color.a = 1.0f;
    return color;
}

} // namespace buckswood_fake
