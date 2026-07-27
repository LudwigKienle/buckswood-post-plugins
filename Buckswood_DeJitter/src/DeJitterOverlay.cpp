#include "DeJitterOverlay.h"

#include <algorithm>
#include <cmath>

namespace buckswood_dejitter {
namespace {

constexpr double kMinimumRegion = 0.02;
constexpr double kMaximumRegion = 0.50;

double width(const OverlayBounds& bounds)
{
    return std::max(1.0e-6, bounds.x2 - bounds.x1);
}

double height(const OverlayBounds& bounds)
{
    return std::max(1.0e-6, bounds.y2 - bounds.y1);
}

bool closeTo(
    OverlayPoint pen,
    OverlayPoint handle,
    OverlayPoint tolerance)
{
    return
        std::fabs(pen.x - handle.x) <= tolerance.x &&
        std::fabs(pen.y - handle.y) <= tolerance.y;
}

double clampRegion(double value)
{
    return std::clamp(
        value,
        kMinimumRegion,
        kMaximumRegion);
}

} // namespace

OverlayRect DeJitterOverlay::rect(
    const OverlayBounds& bounds,
    const OverlayParameters& parameters)
{
    const double halfWidth =
        width(bounds) * clampRegion(parameters.regionWidth) * 0.5;
    const double halfHeight =
        height(bounds) * clampRegion(parameters.regionHeight) * 0.5;
    return OverlayRect{
        parameters.center.x - halfWidth,
        parameters.center.y - halfHeight,
        parameters.center.x + halfWidth,
        parameters.center.y + halfHeight,
    };
}

OverlayDragMode DeJitterOverlay::hitTest(
    const OverlayRect& area,
    OverlayPoint center,
    OverlayPoint pen,
    OverlayPoint pixelScale)
{
    const OverlayPoint cornerTolerance{
        std::max(1.0e-6, pixelScale.x) * 10.0,
        std::max(1.0e-6, pixelScale.y) * 10.0,
    };
    const OverlayPoint edgeTolerance{
        std::max(1.0e-6, pixelScale.x) * 8.0,
        std::max(1.0e-6, pixelScale.y) * 8.0,
    };

    if (closeTo(
            pen,
            OverlayPoint{area.left, area.bottom},
            cornerTolerance)) {
        return OverlayDragBottomLeft;
    }
    if (closeTo(
            pen,
            OverlayPoint{area.right, area.bottom},
            cornerTolerance)) {
        return OverlayDragBottomRight;
    }
    if (closeTo(
            pen,
            OverlayPoint{area.left, area.top},
            cornerTolerance)) {
        return OverlayDragTopLeft;
    }
    if (closeTo(
            pen,
            OverlayPoint{area.right, area.top},
            cornerTolerance)) {
        return OverlayDragTopRight;
    }

    const double middleX = (area.left + area.right) * 0.5;
    const double middleY = (area.bottom + area.top) * 0.5;
    if (closeTo(
            pen,
            OverlayPoint{area.left, middleY},
            edgeTolerance)) {
        return OverlayDragLeft;
    }
    if (closeTo(
            pen,
            OverlayPoint{area.right, middleY},
            edgeTolerance)) {
        return OverlayDragRight;
    }
    if (closeTo(
            pen,
            OverlayPoint{middleX, area.bottom},
            edgeTolerance)) {
        return OverlayDragBottom;
    }
    if (closeTo(
            pen,
            OverlayPoint{middleX, area.top},
            edgeTolerance)) {
        return OverlayDragTop;
    }

    const OverlayPoint centerTolerance{
        std::max(1.0e-6, pixelScale.x) * 12.0,
        std::max(1.0e-6, pixelScale.y) * 12.0,
    };
    if (
        closeTo(pen, center, centerTolerance) ||
        (
            pen.x >= area.left &&
            pen.x <= area.right &&
            pen.y >= area.bottom &&
            pen.y <= area.top)) {
        return OverlayDragMove;
    }
    return OverlayDragNone;
}

OverlayParameters DeJitterOverlay::drag(
    OverlayDragMode mode,
    const OverlayBounds& bounds,
    const OverlayParameters& start,
    OverlayPoint pen,
    OverlayPoint grabOffset)
{
    OverlayParameters result = start;
    const double boundsWidth = width(bounds);
    const double boundsHeight = height(bounds);

    if (mode == OverlayDragMove) {
        const double halfWidth =
            boundsWidth * clampRegion(start.regionWidth) * 0.5;
        const double halfHeight =
            boundsHeight * clampRegion(start.regionHeight) * 0.5;
        result.center.x = std::clamp(
            pen.x + grabOffset.x,
            bounds.x1 + halfWidth,
            bounds.x2 - halfWidth);
        result.center.y = std::clamp(
            pen.y + grabOffset.y,
            bounds.y1 + halfHeight,
            bounds.y2 - halfHeight);
        return result;
    }

    const bool resizeWidth =
        mode == OverlayDragLeft ||
        mode == OverlayDragRight ||
        mode == OverlayDragBottomLeft ||
        mode == OverlayDragBottomRight ||
        mode == OverlayDragTopLeft ||
        mode == OverlayDragTopRight;
    const bool resizeHeight =
        mode == OverlayDragBottom ||
        mode == OverlayDragTop ||
        mode == OverlayDragBottomLeft ||
        mode == OverlayDragBottomRight ||
        mode == OverlayDragTopLeft ||
        mode == OverlayDragTopRight;

    if (resizeWidth) {
        result.regionWidth = clampRegion(
            2.0 * std::fabs(pen.x - start.center.x) /
            boundsWidth);
    }
    if (resizeHeight) {
        result.regionHeight = clampRegion(
            2.0 * std::fabs(pen.y - start.center.y) /
            boundsHeight);
    }
    return result;
}

} // namespace buckswood_dejitter
