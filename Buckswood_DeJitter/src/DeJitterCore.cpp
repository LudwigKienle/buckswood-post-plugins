#include "DeJitterCore.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace buckswood_dejitter {
namespace {

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep(float edge0, float edge1, float value)
{
    if (edge0 == edge1) {
        return value < edge0 ? 0.0f : 1.0f;
    }
    const float amount = clamp01((value - edge0) / (edge1 - edge0));
    return amount * amount * (3.0f - 2.0f * amount);
}

float luma(Pixel pixel)
{
    return pixel.r * 0.2627f + pixel.g * 0.6780f + pixel.b * 0.0593f;
}

float length(Vec2 value)
{
    return std::sqrt(value.x * value.x + value.y * value.y);
}

Vec2 add(Vec2 left, Vec2 right)
{
    return Vec2{left.x + right.x, left.y + right.y};
}

Vec2 mul(Vec2 value, float amount)
{
    return Vec2{value.x * amount, value.y * amount};
}

Pixel mix(Pixel left, Pixel right, float amount)
{
    amount = clamp01(amount);
    return Pixel{
        left.r + (right.r - left.r) * amount,
        left.g + (right.g - left.g) * amount,
        left.b + (right.b - left.b) * amount,
        left.a + (right.a - left.a) * amount,
    };
}

struct PatchTemplate {
    std::vector<Vec2> offsets;
    std::vector<float> centeredLuma;
    float mean = 0.0f;
    float variance = 0.0f;
    float halfWidth = 0.0f;
    float halfHeight = 0.0f;
};

PatchTemplate buildTemplate(
    const Sampler& sampler,
    float centerX,
    float centerY,
    float halfWidth,
    float halfHeight,
    int gridSize)
{
    PatchTemplate patch;
    patch.halfWidth = halfWidth;
    patch.halfHeight = halfHeight;
    patch.offsets.reserve(static_cast<std::size_t>(gridSize * gridSize));
    patch.centeredLuma.reserve(
        static_cast<std::size_t>(gridSize * gridSize));

    float sum = 0.0f;
    for (int gy = 0; gy < gridSize; ++gy) {
        const float ny =
            gridSize > 1
            ? static_cast<float>(gy) /
                    static_cast<float>(gridSize - 1) *
                    2.0f -
                1.0f
            : 0.0f;
        for (int gx = 0; gx < gridSize; ++gx) {
            const float nx =
                gridSize > 1
                ? static_cast<float>(gx) /
                        static_cast<float>(gridSize - 1) *
                        2.0f -
                    1.0f
                : 0.0f;
            const Vec2 offset{
                nx * halfWidth,
                ny * halfHeight,
            };
            patch.offsets.push_back(offset);
            const float value = luma(
                sampler.sample(
                    centerX + offset.x,
                    centerY + offset.y));
            patch.centeredLuma.push_back(value);
            sum += value;
        }
    }

    const float count =
        static_cast<float>(patch.centeredLuma.size());
    patch.mean = count > 0.0f ? sum / count : 0.0f;
    for (float& value : patch.centeredLuma) {
        value -= patch.mean;
        patch.variance += value * value;
    }
    return patch;
}

float correlationAt(
    const PatchTemplate& patch,
    const Sampler& candidate,
    float centerX,
    float centerY,
    float dx,
    float dy)
{
    if (patch.offsets.empty() || patch.variance < 1.0e-8f) {
        return -1.0f;
    }

    float candidateMean = 0.0f;
    for (const Vec2 offset : patch.offsets) {
        candidateMean += luma(
            candidate.sample(
                centerX + dx + offset.x,
                centerY + dy + offset.y));
    }
    candidateMean /= static_cast<float>(patch.offsets.size());

    float covariance = 0.0f;
    float candidateVariance = 0.0f;
    for (std::size_t index = 0; index < patch.offsets.size(); ++index) {
        const Vec2 offset = patch.offsets[index];
        const float value =
            luma(
                candidate.sample(
                    centerX + dx + offset.x,
                    centerY + dy + offset.y)) -
            candidateMean;
        covariance += patch.centeredLuma[index] * value;
        candidateVariance += value * value;
    }
    const float denominator =
        std::sqrt(patch.variance * candidateVariance);
    return denominator > 1.0e-8f
        ? std::clamp(covariance / denominator, -1.0f, 1.0f)
        : -1.0f;
}

bool patchFits(
    Bounds bounds,
    float centerX,
    float centerY,
    const PatchTemplate& patch,
    float dx,
    float dy)
{
    const float left = centerX + dx - patch.halfWidth;
    const float right = centerX + dx + patch.halfWidth;
    const float bottom = centerY + dy - patch.halfHeight;
    const float top = centerY + dy + patch.halfHeight;
    return left >= static_cast<float>(bounds.x1) &&
        right <= static_cast<float>(bounds.x2 - 1) &&
        bottom >= static_cast<float>(bounds.y1) &&
        top <= static_cast<float>(bounds.y2 - 1);
}

MotionEstimate estimateMotion(
    const PatchTemplate& patch,
    const Sampler& candidate,
    float centerX,
    float centerY,
    int searchRadius,
    int coarseStep,
    float textureConfidence,
    float sceneCutProtection)
{
    MotionEstimate result;
    const Bounds candidateBounds = candidate.bounds();
    float bestScore = -2.0f;
    float secondScore = -2.0f;
    int bestX = 0;
    int bestY = 0;

    auto consider = [&](int dx, int dy, bool trackSecond) {
        if (!patchFits(
                candidateBounds,
                centerX,
                centerY,
                patch,
                static_cast<float>(dx),
                static_cast<float>(dy))) {
            return;
        }
        const float score = correlationAt(
            patch,
            candidate,
            centerX,
            centerY,
            static_cast<float>(dx),
            static_cast<float>(dy));
        if (score > bestScore) {
            if (trackSecond) {
                secondScore = bestScore;
            }
            bestScore = score;
            bestX = dx;
            bestY = dy;
        } else if (
            trackSecond &&
            score > secondScore &&
            std::abs(dx - bestX) + std::abs(dy - bestY) > 3) {
            secondScore = score;
        }
    };

    for (
        int dy = -searchRadius;
        dy <= searchRadius;
        dy += coarseStep) {
        for (
            int dx = -searchRadius;
            dx <= searchRadius;
            dx += coarseStep) {
            consider(dx, dy, true);
        }
    }
    consider(0, 0, true);

    const int coarseX = bestX;
    const int coarseY = bestY;
    bestScore = -2.0f;
    secondScore = -2.0f;
    for (
        int dy = coarseY - coarseStep;
        dy <= coarseY + coarseStep;
        ++dy) {
        if (dy < -searchRadius || dy > searchRadius) {
            continue;
        }
        for (
            int dx = coarseX - coarseStep;
            dx <= coarseX + coarseStep;
            ++dx) {
            if (dx < -searchRadius || dx > searchRadius) {
                continue;
            }
            consider(dx, dy, true);
        }
    }

    if (bestScore <= -1.5f) {
        return result;
    }

    auto subpixelOffset = [](float negative, float center, float positive) {
        const float denominator =
            negative - 2.0f * center + positive;
        if (std::fabs(denominator) < 1.0e-6f) {
            return 0.0f;
        }
        return std::clamp(
            0.5f * (negative - positive) / denominator,
            -0.5f,
            0.5f);
    };

    auto subpixelScore = [&](int dx, int dy) {
        return patchFits(
                   candidateBounds,
                   centerX,
                   centerY,
                   patch,
                   static_cast<float>(dx),
                   static_cast<float>(dy))
            ? correlationAt(
                  patch,
                  candidate,
                  centerX,
                  centerY,
                  static_cast<float>(dx),
                  static_cast<float>(dy))
            : bestScore;
    };
    const float scoreLeft = subpixelScore(bestX - 1, bestY);
    const float scoreRight = subpixelScore(bestX + 1, bestY);
    const float scoreDown = subpixelScore(bestX, bestY - 1);
    const float scoreUp = subpixelScore(bestX, bestY + 1);

    result.offset.x =
        static_cast<float>(bestX) +
        subpixelOffset(scoreLeft, bestScore, scoreRight);
    result.offset.y =
        static_cast<float>(bestY) +
        subpixelOffset(scoreDown, bestScore, scoreUp);
    result.correlation = bestScore;

    const float minimumCorrelation =
        0.08f + clamp01(sceneCutProtection) * 0.42f;
    const float peakSeparation =
        secondScore > -1.5f
        ? bestScore - secondScore
        : 0.10f;
    const float correlationConfidence = smoothstep(
        minimumCorrelation,
        0.94f,
        bestScore);
    const float peakConfidence =
        0.35f +
        0.65f * smoothstep(0.002f, 0.045f, peakSeparation);
    const float boundaryPenalty =
        std::abs(bestX) >= searchRadius - 1 ||
            std::abs(bestY) >= searchRadius - 1
        ? 0.45f
        : 1.0f;
    result.confidence =
        clamp01(
            correlationConfidence *
            peakConfidence *
            textureConfidence *
            boundaryPenalty);
    result.valid =
        bestScore >= minimumCorrelation &&
        result.confidence > 0.01f;
    return result;
}

float mirrorCoordinate(float value, float minimum, float maximum)
{
    const float size = maximum - minimum;
    if (size <= 1.0f) {
        return minimum;
    }
    const float period = size * 2.0f;
    float wrapped = std::fmod(value - minimum, period);
    if (wrapped < 0.0f) {
        wrapped += period;
    }
    if (wrapped > size) {
        wrapped = period - wrapped;
    }
    return minimum + wrapped;
}

float distanceToSegment(
    float px,
    float py,
    float ax,
    float ay,
    float bx,
    float by)
{
    const float vx = bx - ax;
    const float vy = by - ay;
    const float denominator = vx * vx + vy * vy;
    const float amount =
        denominator > 1.0e-8f
        ? std::clamp(
              ((px - ax) * vx + (py - ay) * vy) /
                  denominator,
              0.0f,
              1.0f)
        : 0.0f;
    const float dx = px - (ax + vx * amount);
    const float dy = py - (ay + vy * amount);
    return std::sqrt(dx * dx + dy * dy);
}

Pixel overlayColor(float confidence, float alpha)
{
    const float low = 1.0f - smoothstep(0.15f, 0.55f, confidence);
    const float high = smoothstep(0.45f, 0.82f, confidence);
    return Pixel{
        0.12f + low * 0.90f,
        0.18f + high * 0.82f,
        0.08f + (1.0f - std::max(low, high)) * 0.25f,
        alpha,
    };
}

} // namespace

Controls DeJitterCore::defaultControls()
{
    return Controls{};
}

TrackingResult DeJitterCore::analyze(
    const Sampler& current,
    const TemporalContext& temporal,
    const FrameInfo& frame,
    const Controls& controls)
{
    TrackingResult result;
    if (!controls.enabled || frame.width <= 1 || frame.height <= 1) {
        return result;
    }

    const Bounds bounds = current.bounds();
    const float maximumHalfWidth =
        std::max(
            0.5f,
            0.5f * static_cast<float>(bounds.x2 - bounds.x1 - 1));
    const float maximumHalfHeight =
        std::max(
            0.5f,
            0.5f * static_cast<float>(bounds.y2 - bounds.y1 - 1));
    const float requestedHalfWidth =
        std::min(
            maximumHalfWidth,
            std::max(
            4.0f,
            static_cast<float>(frame.width) *
                std::clamp(controls.regionWidth, 0.01f, 0.50f) *
                0.5f));
    const float requestedHalfHeight =
        std::min(
            maximumHalfHeight,
            std::max(
            4.0f,
            static_cast<float>(frame.height) *
                std::clamp(controls.regionHeight, 0.01f, 0.50f) *
                0.5f));
    const float centerX = std::clamp(
        controls.trackX,
        static_cast<float>(bounds.x1) + requestedHalfWidth,
        static_cast<float>(bounds.x2 - 1) - requestedHalfWidth);
    const float centerY = std::clamp(
        controls.trackY,
        static_cast<float>(bounds.y1) + requestedHalfHeight,
        static_cast<float>(bounds.y2 - 1) - requestedHalfHeight);
    result.regionHalfWidth = requestedHalfWidth;
    result.regionHalfHeight = requestedHalfHeight;
    result.trackCenter = Vec2{centerX, centerY};

    int gridSize = 9;
    int coarseStep = 4;
    if (controls.analysisQuality == QualityDraft) {
        gridSize = 7;
        coarseStep = 6;
    } else if (controls.analysisQuality == QualityHigh) {
        gridSize = 11;
        coarseStep = 3;
    }

    const PatchTemplate patch = buildTemplate(
        current,
        centerX,
        centerY,
        requestedHalfWidth,
        requestedHalfHeight,
        gridSize);
    const float perSampleVariance =
        patch.offsets.empty()
        ? 0.0f
        : patch.variance /
            static_cast<float>(patch.offsets.size());
    result.textureConfidence = smoothstep(
        0.00002f,
        0.0045f,
        perSampleVariance);
    if (result.textureConfidence <= 0.001f) {
        return result;
    }

    const int searchRadius =
        std::clamp(controls.searchRadius, 2, 192);
    auto estimate = [&](const Sampler* candidate) {
        return candidate
            ? estimateMotion(
                  patch,
                  *candidate,
                  centerX,
                  centerY,
                  searchRadius,
                  coarseStep,
                  result.textureConfidence,
                  controls.sceneCutProtection)
            : MotionEstimate{};
    };

    result.previous = estimate(temporal.previous);
    result.next = estimate(temporal.next);
    if (controls.temporalRadius >= 2) {
        result.previous2 = estimate(temporal.previous2);
        result.next2 = estimate(temporal.next2);
    }

    Vec2 combined{};
    float totalWeight = 0.0f;
    float weightedConfidence = 0.0f;
    if (result.previous.valid && result.next.valid) {
        const float confidence = std::sqrt(
            result.previous.confidence *
            result.next.confidence);
        combined = add(
            combined,
            mul(
                add(
                    result.previous.offset,
                    result.next.offset),
                0.5f * confidence));
        totalWeight += confidence;
        weightedConfidence += confidence * confidence;
    }

    if (
        controls.temporalRadius >= 2 &&
        result.previous2.valid &&
        result.next2.valid) {
        const float confidence =
            std::sqrt(
                result.previous2.confidence *
                result.next2.confidence) *
            clamp01(controls.outerFrameWeight);
        combined = add(
            combined,
            mul(
                add(
                    result.previous2.offset,
                    result.next2.offset),
                0.5f * confidence));
        totalWeight += confidence;
        weightedConfidence += confidence * confidence;
    }

    if (totalWeight <= 1.0e-5f) {
        return result;
    }

    result.rawCorrection = mul(combined, 1.0f / totalWeight);
    result.confidence = clamp01(
        weightedConfidence / totalWeight);
    const float gate = smoothstep(
        clamp01(controls.confidenceThreshold),
        std::min(
            1.0f,
            clamp01(controls.confidenceThreshold) + 0.12f),
        result.confidence);
    result.correction = mul(
        result.rawCorrection,
        clamp01(controls.stabilizationStrength) * gate);

    const float correctionLength = length(result.correction);
    const float maxCorrection =
        std::max(0.0f, controls.maxCorrection);
    if (
        correctionLength > maxCorrection &&
        correctionLength > 1.0e-5f) {
        result.correction = mul(
            result.correction,
            maxCorrection / correctionLength);
    }

    const float zoomNeeded =
        1.0f +
        2.0f *
            std::max(
                std::fabs(result.correction.x) /
                    std::max(1.0f, static_cast<float>(frame.width)),
                std::fabs(result.correction.y) /
                    std::max(1.0f, static_cast<float>(frame.height)));
    result.autoZoom =
        1.0f +
        (zoomNeeded - 1.0f) *
            clamp01(controls.autoZoomStrength);
    result.valid = gate > 0.0f;
    return result;
}

Pixel DeJitterCore::processPixel(
    const Sampler& current,
    int x,
    int y,
    const FrameInfo& frame,
    const Controls& controls,
    const TrackingResult& tracking)
{
    const Pixel original = current.sample(
        static_cast<float>(x),
        static_cast<float>(y));
    if (!controls.enabled) {
        return original;
    }

    const Bounds bounds = current.bounds();
    const float centerX =
        0.5f *
        static_cast<float>(bounds.x1 + bounds.x2 - 1);
    const float centerY =
        0.5f *
        static_cast<float>(bounds.y1 + bounds.y2 - 1);
    const float zoom =
        controls.edgeMode == EdgeAutoZoom
        ? std::max(1.0f, tracking.autoZoom)
        : 1.0f;
    float sourceX =
        centerX +
        (static_cast<float>(x) - centerX) / zoom -
        tracking.correction.x;
    float sourceY =
        centerY +
        (static_cast<float>(y) - centerY) / zoom -
        tracking.correction.y;

    if (controls.edgeMode == EdgeReflect) {
        sourceX = mirrorCoordinate(
            sourceX,
            static_cast<float>(bounds.x1),
            static_cast<float>(bounds.x2 - 1));
        sourceY = mirrorCoordinate(
            sourceY,
            static_cast<float>(bounds.y1),
            static_cast<float>(bounds.y2 - 1));
    } else {
        sourceX = std::clamp(
            sourceX,
            static_cast<float>(bounds.x1),
            static_cast<float>(bounds.x2 - 1));
        sourceY = std::clamp(
            sourceY,
            static_cast<float>(bounds.y1),
            static_cast<float>(bounds.y2 - 1));
    }

    Pixel stabilized = current.sampleHighQuality(
        sourceX,
        sourceY,
        controls.interpolation);
    stabilized.a = original.a +
        (stabilized.a - original.a) *
            clamp01(controls.outputMix);
    Pixel result = mix(
        original,
        stabilized,
        clamp01(controls.outputMix));
    result.a = stabilized.a;

    if (controls.viewMode == ViewConfidence) {
        const float confidence = tracking.confidence;
        return Pixel{
            confidence,
            confidence,
            confidence,
            original.a,
        };
    }
    if (controls.viewMode == ViewDifference) {
        return Pixel{
            clamp01(std::fabs(result.r - original.r) * 5.0f),
            clamp01(std::fabs(result.g - original.g) * 5.0f),
            clamp01(std::fabs(result.b - original.b) * 5.0f),
            original.a,
        };
    }

    if (
        controls.viewMode == ViewTrackingRegion ||
        controls.viewMode == ViewMotionVector) {
        const float trackX = tracking.trackCenter.x;
        const float trackY = tracking.trackCenter.y;
        const float halfWidth = tracking.regionHalfWidth;
        const float halfHeight = tracking.regionHalfHeight;
        const float px = static_cast<float>(x) + 0.5f;
        const float py = static_cast<float>(y) + 0.5f;
        const float lineThickness =
            std::max(
                1.25f,
                static_cast<float>(
                    std::min(frame.width, frame.height)) /
                    900.0f);
        const float left = trackX - halfWidth;
        const float right = trackX + halfWidth;
        const float bottom = trackY - halfHeight;
        const float top = trackY + halfHeight;
        const bool onRegion =
            ((std::fabs(px - left) <= lineThickness ||
              std::fabs(px - right) <= lineThickness) &&
             py >= bottom && py <= top) ||
            ((std::fabs(py - bottom) <= lineThickness ||
              std::fabs(py - top) <= lineThickness) &&
             px >= left && px <= right);
        const bool onCross =
            (std::fabs(px - trackX) <= lineThickness &&
             std::fabs(py - trackY) <= 10.0f) ||
            (std::fabs(py - trackY) <= lineThickness &&
             std::fabs(px - trackX) <= 10.0f);
        const float vectorEndX =
            trackX + tracking.correction.x * 8.0f;
        const float vectorEndY =
            trackY + tracking.correction.y * 8.0f;
        const bool onVector =
            distanceToSegment(
                px,
                py,
                trackX,
                trackY,
                vectorEndX,
                vectorEndY) <= lineThickness * 1.4f;
        const Pixel color = overlayColor(
            tracking.confidence,
            original.a);

        Pixel base =
            controls.viewMode == ViewMotionVector
            ? mix(
                  original,
                  Pixel{
                      luma(original) * 0.22f,
                      luma(original) * 0.22f,
                      luma(original) * 0.22f,
                      original.a,
                  },
                  0.72f)
            : result;
        if (
            onCross ||
            onVector ||
            (controls.viewMode == ViewTrackingRegion && onRegion)) {
            base = mix(
                base,
                color,
                clamp01(controls.overlayOpacity));
        }
        base.a = original.a;
        return base;
    }

    return result;
}

} // namespace buckswood_dejitter
