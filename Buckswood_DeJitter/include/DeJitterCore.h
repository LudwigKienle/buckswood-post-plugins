#pragma once

#include <array>

namespace buckswood_dejitter {

struct Pixel {
    float r;
    float g;
    float b;
    float a;
};

struct Bounds {
    int x1;
    int y1;
    int x2;
    int y2;
};

struct FrameInfo {
    int width;
    int height;
    int frameIndex;
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

enum ViewMode {
    ViewResult = 0,
    ViewTrackingRegion = 1,
    ViewMotionVector = 2,
    ViewConfidence = 3,
    ViewDifference = 4,
};

enum EdgeMode {
    EdgeAutoZoom = 0,
    EdgeReflect = 1,
    EdgeExtend = 2,
};

enum InterpolationMode {
    InterpolationBicubic = 0,
    InterpolationLanczos3 = 1,
    InterpolationBilinear = 2,
};

enum AnalysisQuality {
    QualityDraft = 0,
    QualityStandard = 1,
    QualityHigh = 2,
};

struct Controls {
    bool enabled = true;
    float trackX = 0.0f;
    float trackY = 0.0f;
    bool textureSnap = false;
    int textureSnapRadius = 24;
    float regionWidth = 0.12f;
    float regionHeight = 0.12f;
    int searchRadius = 32;
    int temporalRadius = 2;
    int analysisQuality = QualityStandard;
    float stabilizationStrength = 0.85f;
    float outerFrameWeight = 0.30f;
    float maxCorrection = 24.0f;
    float confidenceThreshold = 0.28f;
    float sceneCutProtection = 0.72f;
    int interpolation = InterpolationBicubic;
    int edgeMode = EdgeAutoZoom;
    float autoZoomStrength = 1.0f;
    int viewMode = ViewResult;
    float overlayOpacity = 0.88f;
    float outputMix = 1.0f;
};

class Sampler {
public:
    virtual ~Sampler() = default;
    virtual Pixel sample(float x, float y) const = 0;
    virtual Pixel sampleHighQuality(
        float x,
        float y,
        int interpolation) const
    {
        (void)interpolation;
        return sample(x, y);
    }
    virtual Bounds bounds() const = 0;
};

struct TemporalContext {
    const Sampler* previous2 = nullptr;
    const Sampler* previous = nullptr;
    const Sampler* next = nullptr;
    const Sampler* next2 = nullptr;
};

struct MotionEstimate {
    Vec2 offset;
    float correlation = -1.0f;
    float confidence = 0.0f;
    bool valid = false;
};

struct TrackingResult {
    MotionEstimate previous2;
    MotionEstimate previous;
    MotionEstimate next;
    MotionEstimate next2;
    Vec2 rawCorrection;
    Vec2 correction;
    float confidence = 0.0f;
    float textureConfidence = 0.0f;
    float autoZoom = 1.0f;
    Vec2 requestedTrackCenter;
    Vec2 trackCenter;
    float regionHalfWidth = 0.0f;
    float regionHalfHeight = 0.0f;
    bool pointSnapped = false;
    bool valid = false;
};

class DeJitterCore {
public:
    static Controls defaultControls();

    static TrackingResult analyze(
        const Sampler& current,
        const TemporalContext& temporal,
        const FrameInfo& frame,
        const Controls& controls);

    static Pixel processPixel(
        const Sampler& current,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls,
        const TrackingResult& tracking);
};

} // namespace buckswood_dejitter
