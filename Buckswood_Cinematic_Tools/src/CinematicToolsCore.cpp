#include "CinematicToolsCore.h"

#include <array>

namespace buckswood_cinematic {

namespace {

constexpr float kEpsilon = 1.0e-6f;

float distanceToLine(float value, float line)
{
    return std::fabs(value - line);
}

float localSmoothstep(float edge0, float edge1, float value)
{
    if (std::fabs(edge1 - edge0) < kEpsilon) {
        return value < edge0 ? 0.0f : 1.0f;
    }
    const float t = std::min(1.0f, std::max(0.0f, (value - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

float guideLine(float value, float line, float thickness)
{
    const float distance = distanceToLine(value, line);
    return 1.0f - localSmoothstep(thickness, thickness * 2.0f, distance);
}

} // namespace

FrameDirectorControls CinematicToolsCore::defaultFrameDirectorControls()
{
    return FrameDirectorControls{
        0,     // targetAspect: 2.39:1
        0,     // viewMode
        0,     // framingMode: auto
        0.78f, // autoStrength
        0.75f, // subjectWeight
        0.35f, // skinWeight
        false, // subjectLock
        0.5f,  // lockX
        0.42f, // lockY
        0.65f, // cutSensitivity
        0.70f, // temporalSmoothing
        0.18f, // motionLimit
        0.0f,  // manualX
        0.0f,  // manualY
        0.38f, // subjectVerticalPosition
        0.78f, // guideOpacity
        1.0f,  // matteOpacity
        0.006f,// cropFeather
        1.0f,  // outputMix
    };
}

RadianceControls CinematicToolsCore::defaultRadianceControls()
{
    return RadianceControls{
        0,     // viewMode
        0.62f, // highlightRecovery
        0.35f, // specularRecovery
        0.72f, // highlightRolloff
        0.28f, // shadowRecovery
        2.0f,  // recoveredHeadroomStops
        0.28f, // localDetail
        0.35f, // shadowDenoise
        0.22f, // dequantization
        0.55f, // temporalConsistency
        0.30f, // longTermConsistency
        0.80f, // colorGuard
        1.0f,  // outputMix
    };
}

TemporalIntegrityControls CinematicToolsCore::defaultTemporalIntegrityControls()
{
    return TemporalIntegrityControls{
        0,     // viewMode
        1.0f,  // sensitivity
        0.42f, // repairStrength
        0.55f, // flickerRepair
        0.32f, // textureStability
        0.30f, // longTermStability
        0.88f, // motionGuard
        0.90f, // ghostGuard
        0.82f, // edgeProtection
        0.28f, // chromaStability
        0.72f, // overlayStrength
        1.0f,  // outputMix
    };
}

float CinematicToolsCore::aspectRatioForChoice(int choice)
{
    constexpr std::array<float, 7> ratios = {
        2.39f,
        2.00f,
        1.85f,
        16.0f / 9.0f,
        4.0f / 3.0f,
        1.0f,
        9.0f / 16.0f,
    };
    return ratios[static_cast<std::size_t>(std::clamp(choice, 0, 6))];
}

float CinematicToolsCore::clamp01(float value)
{
    return std::min(1.0f, std::max(0.0f, value));
}

float CinematicToolsCore::smoothstep(float edge0, float edge1, float value)
{
    if (std::fabs(edge1 - edge0) < kEpsilon) {
        return value < edge0 ? 0.0f : 1.0f;
    }
    const float t = clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

float CinematicToolsCore::luma(Pixel pixel)
{
    return pixel.r * 0.2126f + pixel.g * 0.7152f + pixel.b * 0.0722f;
}

float CinematicToolsCore::maxChannel(Pixel pixel)
{
    return std::max(pixel.r, std::max(pixel.g, pixel.b));
}

float CinematicToolsCore::minChannel(Pixel pixel)
{
    return std::min(pixel.r, std::min(pixel.g, pixel.b));
}

float CinematicToolsCore::hash01(int x, int y, int z)
{
    std::uint32_t n = static_cast<std::uint32_t>(x) * 1973u;
    n ^= static_cast<std::uint32_t>(y) * 9277u;
    n ^= static_cast<std::uint32_t>(z) * 26699u;
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return static_cast<float>(n & 0x00ffffffu) / 16777215.0f;
}

Pixel CinematicToolsCore::mix(Pixel a, Pixel b, float amount)
{
    amount = clamp01(amount);
    return Pixel{
        a.r + (b.r - a.r) * amount,
        a.g + (b.g - a.g) * amount,
        a.b + (b.b - a.b) * amount,
        a.a + (b.a - a.a) * amount,
    };
}

Pixel CinematicToolsCore::add(Pixel a, Pixel b)
{
    return Pixel{a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a};
}

Pixel CinematicToolsCore::mul(Pixel pixel, float amount)
{
    return Pixel{pixel.r * amount, pixel.g * amount, pixel.b * amount, pixel.a * amount};
}

Pixel CinematicToolsCore::clampDisplay(Pixel pixel)
{
    return Pixel{clamp01(pixel.r), clamp01(pixel.g), clamp01(pixel.b), clamp01(pixel.a)};
}

float CinematicToolsCore::channelMedian(float a, float b, float c)
{
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

Pixel CinematicToolsCore::median(Pixel a, Pixel b, Pixel c)
{
    return Pixel{
        channelMedian(a.r, b.r, c.r),
        channelMedian(a.g, b.g, c.g),
        channelMedian(a.b, b.b, c.b),
        channelMedian(a.a, b.a, c.a),
    };
}

float CinematicToolsCore::channelMedian5(float a, float b, float c, float d, float e)
{
    std::array<float, 5> values = {a, b, c, d, e};
    std::sort(values.begin(), values.end());
    return values[2];
}

Pixel CinematicToolsCore::median5(Pixel a, Pixel b, Pixel c, Pixel d, Pixel e)
{
    return Pixel{
        channelMedian5(a.r, b.r, c.r, d.r, e.r),
        channelMedian5(a.g, b.g, c.g, d.g, e.g),
        channelMedian5(a.b, b.b, c.b, d.b, e.b),
        channelMedian5(a.a, b.a, c.a, d.a, e.a),
    };
}

float CinematicToolsCore::pixelDistance(Pixel a, Pixel b)
{
    const float dr = a.r - b.r;
    const float dg = a.g - b.g;
    const float db = a.b - b.b;
    return std::sqrt(dr * dr * 0.2126f + dg * dg * 0.7152f + db * db * 0.0722f);
}

float CinematicToolsCore::localEdge(const Sampler& sampler, int x, int y)
{
    const float gx = luma(sampler.sample(static_cast<float>(x + 1), static_cast<float>(y))) -
                     luma(sampler.sample(static_cast<float>(x - 1), static_cast<float>(y)));
    const float gy = luma(sampler.sample(static_cast<float>(x), static_cast<float>(y + 1))) -
                     luma(sampler.sample(static_cast<float>(x), static_cast<float>(y - 1)));
    return std::sqrt(gx * gx + gy * gy);
}

Pixel CinematicToolsCore::crossBlur(const Sampler& sampler, int x, int y, float radius)
{
    const Pixel c = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    const Pixel l = sampler.sample(static_cast<float>(x) - radius, static_cast<float>(y));
    const Pixel r = sampler.sample(static_cast<float>(x) + radius, static_cast<float>(y));
    const Pixel u = sampler.sample(static_cast<float>(x), static_cast<float>(y) - radius);
    const Pixel d = sampler.sample(static_cast<float>(x), static_cast<float>(y) + radius);
    return mul(add(add(add(add(c, l), r), u), d), 0.2f);
}

float CinematicToolsCore::saliencyAt(
    const Sampler& sampler,
    float x,
    float y,
    const FrameInfo& frame,
    const FrameDirectorControls& controls)
{
    const float stepX = std::max(2.0f, static_cast<float>(frame.width) / 160.0f);
    const float stepY = std::max(2.0f, static_cast<float>(frame.height) / 90.0f);
    const Pixel c = sampler.sample(x, y);
    const float centerLuma = luma(c);
    const float gx = luma(sampler.sample(x + stepX, y)) - luma(sampler.sample(x - stepX, y));
    const float gy = luma(sampler.sample(x, y + stepY)) - luma(sampler.sample(x, y - stepY));
    const float gradient = std::sqrt(gx * gx + gy * gy);
    const float chroma = maxChannel(c) - minChannel(c);

    const float warmSkin =
        smoothstep(0.08f, 0.45f, c.r - c.b) *
        smoothstep(-0.05f, 0.22f, c.r - c.g) *
        smoothstep(0.08f, 0.85f, centerLuma) *
        (1.0f - smoothstep(0.72f, 1.05f, chroma));

    const float nx = x / std::max(1.0f, static_cast<float>(frame.width - 1));
    const float ny = y / std::max(1.0f, static_cast<float>(frame.height - 1));
    const float dx = nx - 0.5f;
    const float dy = ny - 0.46f;
    const float centerPrior = std::exp(-(dx * dx * 2.2f + dy * dy * 1.6f));
    const float exposureWeight =
        smoothstep(0.015f, 0.10f, centerLuma) *
        (1.0f - smoothstep(0.94f, 1.08f, centerLuma));

    return std::max(
        0.0001f,
        exposureWeight *
            (0.06f + centerPrior * 0.16f +
             gradient * (1.2f + controls.subjectWeight * 2.2f) +
             chroma * 0.18f +
             warmSkin * controls.skinWeight * 1.5f));
}

FocusAnalysis CinematicToolsCore::analyzeFocus(
    const Sampler& sampler,
    const FrameInfo& frame,
    const FrameDirectorControls& controls)
{
    constexpr int columns = 40;
    constexpr int rows = 23;
    float sumWeight = 0.0f;
    float sumX = 0.0f;
    float sumY = 0.0f;
    float peak = 0.0f;

    for (int gy = 0; gy < rows; ++gy) {
        const float y = (static_cast<float>(gy) + 0.5f) * frame.height / rows;
        for (int gx = 0; gx < columns; ++gx) {
            const float x = (static_cast<float>(gx) + 0.5f) * frame.width / columns;
            const float weight = saliencyAt(sampler, x, y, frame, controls);
            sumWeight += weight;
            sumX += x * weight;
            sumY += y * weight;
            peak = std::max(peak, weight);
        }
    }

    if (sumWeight < kEpsilon) {
        return FocusAnalysis{0.5f, 0.46f, 0.0f};
    }

    const float analyzedX = clamp01((sumX / sumWeight) / std::max(1.0f, static_cast<float>(frame.width)));
    const float analyzedY = clamp01((sumY / sumWeight) / std::max(1.0f, static_cast<float>(frame.height)));
    const float confidence = clamp01(peak / std::max(0.05f, sumWeight / (columns * rows)) - 1.0f);
    return FocusAnalysis{analyzedX, analyzedY, confidence};
}

FocusAnalysis CinematicToolsCore::smoothFocus(
    const FocusAnalysis& current,
    const FocusAnalysis* previous2,
    const FocusAnalysis* previous,
    const FocusAnalysis* next,
    const FocusAnalysis* next2,
    const FrameDirectorControls& controls)
{
    if (controls.subjectLock) {
        const float lockStrength = clamp01(0.80f + current.confidence * 0.15f);
        return FocusAnalysis{
            current.x + (clamp01(controls.lockX) - current.x) * lockStrength,
            current.y + (clamp01(controls.lockY) - current.y) * lockStrength,
            std::max(current.confidence, 0.9f),
        };
    }

    float neighborX = current.x * 1.5f;
    float neighborY = current.y * 1.5f;
    float neighborWeight = 1.5f;
    const float cutThreshold = 0.10f + (1.0f - clamp01(controls.cutSensitivity)) * 0.32f;
    auto includeNeighbor = [&](const FocusAnalysis* candidate, float weight) {
        if (!candidate) {
            return;
        }
        const float dx = candidate->x - current.x;
        const float dy = candidate->y - current.y;
        if (std::sqrt(dx * dx + dy * dy) > cutThreshold) {
            return;
        }
        neighborX += candidate->x * weight;
        neighborY += candidate->y * weight;
        neighborWeight += weight;
    };
    includeNeighbor(previous2, 0.45f);
    includeNeighbor(previous, 1.0f);
    includeNeighbor(next, 1.0f);
    includeNeighbor(next2, 0.45f);
    neighborX /= neighborWeight;
    neighborY /= neighborWeight;

    const float dx = neighborX - current.x;
    const float dy = neighborY - current.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const float limitedDistance = std::max(0.001f, controls.motionLimit);
    const float limiter = distance > limitedDistance ? limitedDistance / distance : 1.0f;
    const float smoothing = clamp01(controls.temporalSmoothing);

    return FocusAnalysis{
        clamp01(current.x + dx * limiter * smoothing),
        clamp01(current.y + dy * limiter * smoothing),
        current.confidence,
    };
}

CropWindow CinematicToolsCore::cropWindow(
    const FrameInfo& frame,
    const FrameDirectorControls& controls,
    const FocusAnalysis& focus)
{
    const float width = static_cast<float>(std::max(1, frame.width));
    const float height = static_cast<float>(std::max(1, frame.height));
    const float frameAspect = width / height;
    const float targetAspect = aspectRatioForChoice(controls.targetAspect);

    float cropWidth = width;
    float cropHeight = height;
    if (targetAspect > frameAspect) {
        cropHeight = width / targetAspect;
    } else if (targetAspect < frameAspect) {
        cropWidth = height * targetAspect;
    }

    float targetX = focus.x;
    float targetY = focus.y;
    switch (controls.framingMode) {
    case FramingCenter:
        targetX = 0.5f;
        targetY = 0.5f;
        break;
    case FramingLeftThird:
        targetX = 1.0f / 3.0f;
        break;
    case FramingRightThird:
        targetX = 2.0f / 3.0f;
        break;
    case FramingManual:
        targetX = clamp01(0.5f + controls.manualX * 0.5f);
        targetY = clamp01(0.5f + controls.manualY * 0.5f);
        break;
    default:
        targetX = 0.5f + (targetX - 0.5f) * clamp01(controls.autoStrength);
        targetY = 0.5f + (targetY - 0.5f) * clamp01(controls.autoStrength);
        break;
    }

    targetX = clamp01(targetX + controls.manualX * 0.20f);
    targetY = clamp01(targetY + controls.manualY * 0.20f);
    const float subjectPositionY = controls.framingMode == FramingCenter
        ? 0.5f
        : clamp01(controls.subjectVerticalPosition);

    float x1 = targetX * width - cropWidth * 0.5f;
    float y1 = targetY * height - cropHeight * subjectPositionY;
    x1 = std::clamp(x1, 0.0f, width - cropWidth);
    y1 = std::clamp(y1, 0.0f, height - cropHeight);
    return CropWindow{x1, y1, x1 + cropWidth, y1 + cropHeight};
}

Pixel CinematicToolsCore::processFrameDirector(
    const Sampler& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const FrameDirectorControls& controls,
    const FocusAnalysis& focus,
    const CropWindow& crop)
{
    return processFrameDirector(
        sampler,
        x,
        y,
        frame,
        controls,
        focus,
        crop,
        prepareFrameDirector(frame, controls, crop));
}

CinematicToolsCore::FrameDirectorState
CinematicToolsCore::prepareFrameDirector(
    const FrameInfo& frame,
    const FrameDirectorControls& controls,
    const CropWindow& crop)
{
    const float cropWidth = std::max(1.0f, crop.x2 - crop.x1);
    const float cropHeight = std::max(1.0f, crop.y2 - crop.y1);
    return FrameDirectorState{
        1.0f /
            std::max(
                1.0f,
                static_cast<float>(frame.width - 1)),
        1.0f /
            std::max(
                1.0f,
                static_cast<float>(frame.height - 1)),
        std::max(
            1.0f,
            controls.cropFeather *
                std::min(frame.width, frame.height)),
        cropWidth,
        cropHeight,
        1.0f / cropWidth,
        1.0f / cropHeight,
        1.5f /
            std::max(
                120.0f,
                std::min(cropWidth, cropHeight)),
    };
}

Pixel CinematicToolsCore::processFrameDirector(
    const Sampler& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const FrameDirectorControls& controls,
    const FocusAnalysis& focus,
    const CropWindow& crop,
    const FrameDirectorState& prepared)
{
    const Pixel input = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    const float nx = static_cast<float>(x) * prepared.inverseWidth;
    const float ny = static_cast<float>(y) * prepared.inverseHeight;

    if (controls.viewMode == FrameDirectorSaliency) {
        const float saliency = clamp01(saliencyAt(sampler, static_cast<float>(x), static_cast<float>(y), frame, controls) * 2.2f);
        const Pixel heat{
            smoothstep(0.25f, 0.85f, saliency),
            smoothstep(0.05f, 0.55f, saliency) * (1.0f - smoothstep(0.70f, 1.0f, saliency)),
            1.0f - smoothstep(0.05f, 0.55f, saliency),
            input.a,
        };
        return mix(input, heat, controls.outputMix);
    }
    if (controls.viewMode == FrameDirectorConfidence) {
        const float confidence = clamp01(focus.confidence);
        return Pixel{
            1.0f - confidence,
            confidence,
            0.08f,
            input.a,
        };
    }

    const float insideX =
        smoothstep(crop.x1 - prepared.featherPixels, crop.x1 + prepared.featherPixels, static_cast<float>(x)) *
        (1.0f - smoothstep(crop.x2 - prepared.featherPixels, crop.x2 + prepared.featherPixels, static_cast<float>(x)));
    const float insideY =
        smoothstep(crop.y1 - prepared.featherPixels, crop.y1 + prepared.featherPixels, static_cast<float>(y)) *
        (1.0f - smoothstep(crop.y2 - prepared.featherPixels, crop.y2 + prepared.featherPixels, static_cast<float>(y)));
    const float inside = clamp01(insideX * insideY);

    if (controls.viewMode == FrameDirectorCropMask) {
        return Pixel{inside, inside, inside, input.a};
    }

    Pixel result = mix(Pixel{0.0f, 0.0f, 0.0f, input.a}, input, 1.0f - controls.matteOpacity * (1.0f - inside));
    if (controls.viewMode == FrameDirectorResult) {
        return mix(input, result, controls.outputMix);
    }
    result = input;

    const float localX =
        (static_cast<float>(x) - crop.x1) *
        prepared.inverseCropWidth;
    const float localY =
        (static_cast<float>(y) - crop.y1) *
        prepared.inverseCropHeight;
    float guide = 0.0f;
    guide = std::max(guide, guideLine(localX, 1.0f / 3.0f, prepared.guideThickness));
    guide = std::max(guide, guideLine(localX, 2.0f / 3.0f, prepared.guideThickness));
    guide = std::max(guide, guideLine(localY, 1.0f / 3.0f, prepared.guideThickness));
    guide = std::max(guide, guideLine(localY, 2.0f / 3.0f, prepared.guideThickness));
    const float cropBorder =
        std::max(
            std::max(guideLine(static_cast<float>(x), crop.x1, 1.0f), guideLine(static_cast<float>(x), crop.x2, 1.0f)),
            std::max(guideLine(static_cast<float>(y), crop.y1, 1.0f), guideLine(static_cast<float>(y), crop.y2, 1.0f)));
    const float focusMarker =
        1.0f - smoothstep(0.006f, 0.013f, std::sqrt((nx - focus.x) * (nx - focus.x) + (ny - focus.y) * (ny - focus.y)));
    const float overlay = clamp01(std::max(std::max(guide * inside, cropBorder), focusMarker) * controls.guideOpacity);
    const Pixel guideColor = focusMarker > guide ? Pixel{1.0f, 0.69f, 0.0f, input.a} : Pixel{0.10f, 0.85f, 1.0f, input.a};
    result = mix(result, guideColor, overlay);
    return mix(input, result, controls.outputMix);
}

Pixel CinematicToolsCore::processRadiance(
    const Sampler& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const RadianceControls& controls,
    const TemporalContext* temporal)
{
    return processRadiance(
        sampler,
        x,
        y,
        frame,
        controls,
        prepareRadiance(controls),
        temporal);
}

CinematicToolsCore::RadianceState
CinematicToolsCore::prepareRadiance(
    const RadianceControls& controls)
{
    const float headroom =
        std::max(
            0.0f,
            std::pow(
                2.0f,
                controls.recoveredHeadroomStops) -
                1.0f);
    return RadianceState{
        headroom,
        1.0f + headroom * 0.35f,
    };
}

Pixel CinematicToolsCore::processRadiance(
    const Sampler& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const RadianceControls& controls,
    const RadianceState& prepared,
    const TemporalContext* temporal)
{
    const Pixel input = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    const Pixel blur = crossBlur(sampler, x, y, 2.0f);
    const float y0 = std::max(0.0f, luma(input));
    const float blurY = std::max(0.0f, luma(blur));
    const float highlightMask = smoothstep(0.68f, 0.99f, y0);
    const float clipMask = smoothstep(0.94f, 1.005f, maxChannel(input));
    const float specularMask =
        clipMask *
        smoothstep(0.02f, 0.24f, maxChannel(input) - minChannel(input) + std::fabs(y0 - blurY));
    const float shadowMask = 1.0f - smoothstep(0.025f, 0.24f, y0);
    const float normalizedHighlight = clamp01((y0 - 0.68f) / 0.32f);

    float recoveredY =
        y0 +
        controls.highlightRecovery * highlightMask * normalizedHighlight * normalizedHighlight * prepared.headroom * 0.24f;
    recoveredY += (y0 - blurY) * controls.localDetail * highlightMask * 1.6f;
    recoveredY +=
        specularMask *
        controls.specularRecovery *
        prepared.headroom *
        0.10f;
    if (recoveredY > prepared.softLimit) {
        const float excess = recoveredY - prepared.softLimit;
        recoveredY = prepared.softLimit + excess / (1.0f + excess * (1.0f + controls.highlightRolloff * 3.0f));
    }

    const float denoisedY = y0 + (blurY - y0) * clamp01(controls.shadowDenoise * shadowMask);
    const float shadowLift = controls.shadowRecovery * shadowMask * (0.018f + (0.18f - y0) * 0.16f);
    recoveredY = recoveredY * (1.0f - shadowMask) + (denoisedY + shadowLift) * shadowMask;

    if (temporal && temporal->hasPrevious && temporal->hasNext && controls.temporalConsistency > 0.0f) {
        const Pixel previous = temporal->previous->sample(static_cast<float>(x), static_cast<float>(y));
        const Pixel next = temporal->next->sample(static_cast<float>(x), static_cast<float>(y));
        const float neighborAgreement = 1.0f - smoothstep(0.015f, 0.16f, pixelDistance(previous, next));
        const float neighborY = 0.5f * (luma(previous) + luma(next));
        const float temporalTarget = recoveredY + (neighborY - y0);
        recoveredY +=
            (temporalTarget - recoveredY) *
            controls.temporalConsistency *
            neighborAgreement *
            std::max(highlightMask, shadowMask) *
            0.35f;
    }
    if (temporal && temporal->hasPrevious2 && temporal->hasNext2 && controls.longTermConsistency > 0.0f) {
        const Pixel previous2 = temporal->previous2->sample(static_cast<float>(x), static_cast<float>(y));
        const Pixel next2 = temporal->next2->sample(static_cast<float>(x), static_cast<float>(y));
        const float farAgreement = 1.0f - smoothstep(0.02f, 0.18f, pixelDistance(previous2, next2));
        const float farY = 0.5f * (luma(previous2) + luma(next2));
        recoveredY +=
            (farY - y0) *
            controls.longTermConsistency *
            farAgreement *
            std::max(highlightMask, shadowMask) *
            0.16f;
    }

    const float noise =
        (hash01(x, y, frame.frameIndex) - 0.5f) *
        controls.dequantization *
        (1.0f / 1023.0f) *
        (0.25f + 0.75f * (1.0f - clipMask));
    recoveredY += noise;

    const float safeY = std::max(0.001f, y0);
    Pixel recovered{
        input.r * recoveredY / safeY,
        input.g * recoveredY / safeY,
        input.b * recoveredY / safeY,
        input.a,
    };
    if (y0 < 0.001f) {
        recovered.r = recovered.g = recovered.b = recoveredY;
    }

    const float peak = std::max(0.001f, maxChannel(recovered));
    const float minimum = minChannel(recovered);
    const float gamutRisk = smoothstep(0.78f, 1.55f, peak - minimum);
    const Pixel neutral{recoveredY, recoveredY, recoveredY, input.a};
    recovered = mix(recovered, neutral, gamutRisk * clamp01(controls.colorGuard) * 0.45f);

    const float recoveryMask = clamp01(std::max(
        highlightMask * controls.highlightRecovery,
        shadowMask * std::max(controls.shadowRecovery, controls.shadowDenoise)));
    if (controls.viewMode == RadianceRecoveryMatte) {
        return Pixel{recoveryMask, recoveryMask, recoveryMask, input.a};
    }
    if (controls.viewMode == RadianceClippingMap) {
        const Pixel map{
            clamp01(clipMask + shadowMask * 0.15f),
            clamp01(highlightMask * (1.0f - clipMask)),
            clamp01(shadowMask),
            input.a,
        };
        return map;
    }
    if (controls.viewMode == RadianceDifference) {
        const float difference = clamp01(std::fabs(recoveredY - y0) * 2.0f);
        return Pixel{difference, difference * 0.58f, 0.05f, input.a};
    }
    return mix(input, recovered, controls.outputMix);
}

Pixel CinematicToolsCore::processTemporalIntegrity(
    const Sampler& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const TemporalIntegrityControls& controls,
    const TemporalContext* temporal)
{
    return processTemporalIntegrity(
        sampler,
        x,
        y,
        frame,
        controls,
        prepareTemporalIntegrity(controls),
        temporal);
}

CinematicToolsCore::TemporalIntegrityState
CinematicToolsCore::prepareTemporalIntegrity(
    const TemporalIntegrityControls& controls)
{
    const float sensitivity =
        std::max(0.1f, controls.sensitivity);
    return TemporalIntegrityState{
        0.018f / sensitivity,
        0.16f / sensitivity,
        0.012f / sensitivity,
        0.13f / sensitivity,
        clamp01(controls.motionGuard),
        clamp01(controls.edgeProtection),
    };
}

Pixel CinematicToolsCore::processTemporalIntegrity(
    const Sampler& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const TemporalIntegrityControls& controls,
    const TemporalIntegrityState& prepared,
    const TemporalContext* temporal)
{
    (void)frame;
    const Pixel input = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    if (!temporal || (!temporal->hasPrevious && !temporal->hasNext)) {
        if (controls.viewMode == TemporalErrorMatte ||
            controls.viewMode == TemporalMotionMatte ||
            controls.viewMode == TemporalConfidenceMatte ||
            controls.viewMode == TemporalGhostRiskMatte) {
            return Pixel{0.0f, 0.0f, 0.0f, input.a};
        }
        return input;
    }

    const Pixel previous = temporal->hasPrevious
        ? temporal->previous->sample(static_cast<float>(x), static_cast<float>(y))
        : input;
    const Pixel next = temporal->hasNext
        ? temporal->next->sample(static_cast<float>(x), static_cast<float>(y))
        : input;
    const Pixel previous2 = temporal->hasPrevious2
        ? temporal->previous2->sample(static_cast<float>(x), static_cast<float>(y))
        : previous;
    const Pixel next2 = temporal->hasNext2
        ? temporal->next2->sample(static_cast<float>(x), static_cast<float>(y))
        : next;
    const Pixel temporalMedian = median5(previous2, previous, input, next, next2);
    const Pixel neighborAverage = mul(add(previous, next), 0.5f);
    const float neighborDifference = pixelDistance(previous, next);
    const float currentOutlier = pixelDistance(input, neighborAverage);
    const float agreement = 1.0f - smoothstep(prepared.agreementLow, prepared.agreementHigh, neighborDifference);
    const float outlier = smoothstep(prepared.outlierLow, prepared.outlierHigh, currentOutlier);
    const float motion = smoothstep(0.025f, 0.24f, neighborDifference);
    const float farDifference = pixelDistance(previous2, next2);
    const float longTermAgreement = 1.0f - smoothstep(0.025f, 0.22f, farDifference);
    const float ghostRisk =
        smoothstep(0.04f, 0.28f, std::max(neighborDifference, farDifference)) *
        smoothstep(0.015f, 0.16f, pixelDistance(temporalMedian, input));
    const float edge = smoothstep(0.025f, 0.24f, localEdge(sampler, x, y));
    const float motionProtection = 1.0f - motion * prepared.motionGuard;
    const float edgeProtection = 1.0f - edge * prepared.edgeProtection;

    const float previousY = luma(previous);
    const float currentY = luma(input);
    const float nextY = luma(next);
    const float signedA = currentY - previousY;
    const float signedB = currentY - nextY;
    const float sameDirection = signedA * signedB > 0.0f ? 1.0f : 0.0f;
    const float flicker = outlier * agreement * sameDirection;
    const float textureRisk =
        outlier *
        agreement *
        (1.0f - smoothstep(0.08f, 0.30f, std::fabs(currentY - luma(crossBlur(sampler, x, y, 1.0f)))));
    const float repairMask = clamp01(
        (flicker * controls.flickerRepair +
         textureRisk * controls.textureStability +
         outlier * longTermAgreement * controls.longTermStability * 0.55f) *
        controls.repairStrength *
        motionProtection *
        edgeProtection *
        (1.0f - ghostRisk * controls.ghostGuard));

    Pixel repaired = mix(input, temporalMedian, repairMask);
    const float inputChromaR = input.r - currentY;
    const float inputChromaB = input.b - currentY;
    const float medianY = luma(temporalMedian);
    const float medianChromaR = temporalMedian.r - medianY;
    const float medianChromaB = temporalMedian.b - medianY;
    const float chromaMismatch =
        smoothstep(0.015f, 0.18f, std::fabs(inputChromaR - medianChromaR) + std::fabs(inputChromaB - medianChromaB));
    repaired = mix(repaired, temporalMedian, repairMask * chromaMismatch * controls.chromaStability);
    repaired.a = input.a;

    if (controls.viewMode == TemporalErrorMatte) {
        return Pixel{repairMask, repairMask, repairMask, input.a};
    }
    if (controls.viewMode == TemporalMotionMatte) {
        return Pixel{motion, motion, motion, input.a};
    }
    if (controls.viewMode == TemporalConfidenceMatte) {
        const float confidence = clamp01(agreement * (0.55f + longTermAgreement * 0.45f) * (1.0f - ghostRisk));
        return Pixel{confidence, confidence, confidence, input.a};
    }
    if (controls.viewMode == TemporalGhostRiskMatte) {
        return Pixel{ghostRisk, ghostRisk * 0.22f, 0.0f, input.a};
    }
    if (controls.viewMode == TemporalDifferenceOverlay) {
        const Pixel overlay{1.0f, 0.10f, 0.02f, input.a};
        return mix(input, overlay, clamp01(repairMask * controls.overlayStrength * 1.8f));
    }
    return mix(input, repaired, controls.outputMix);
}

} // namespace buckswood_cinematic
