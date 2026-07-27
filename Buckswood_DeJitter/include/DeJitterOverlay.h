#pragma once

namespace buckswood_dejitter {

struct OverlayPoint {
    double x = 0.0;
    double y = 0.0;
};

struct OverlayBounds {
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 1.0;
    double y2 = 1.0;
};

struct OverlayRect {
    double left = 0.0;
    double bottom = 0.0;
    double right = 0.0;
    double top = 0.0;
};

enum OverlayDragMode {
    OverlayDragNone = 0,
    OverlayDragMove,
    OverlayDragLeft,
    OverlayDragRight,
    OverlayDragBottom,
    OverlayDragTop,
    OverlayDragBottomLeft,
    OverlayDragBottomRight,
    OverlayDragTopLeft,
    OverlayDragTopRight,
};

struct OverlayParameters {
    OverlayPoint center;
    double regionWidth = 0.12;
    double regionHeight = 0.12;
};

class DeJitterOverlay {
public:
    static OverlayRect rect(
        const OverlayBounds& bounds,
        const OverlayParameters& parameters);

    static OverlayDragMode hitTest(
        const OverlayRect& rect,
        OverlayPoint center,
        OverlayPoint pen,
        OverlayPoint pixelScale);

    static OverlayParameters drag(
        OverlayDragMode mode,
        const OverlayBounds& bounds,
        const OverlayParameters& start,
        OverlayPoint pen,
        OverlayPoint grabOffset);
};

} // namespace buckswood_dejitter
