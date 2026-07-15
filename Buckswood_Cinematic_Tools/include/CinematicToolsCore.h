#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace buckswood_cinematic {

struct Pixel {
    float r;
    float g;
    float b;
    float a;
};

struct FrameInfo {
    int width;
    int height;
    int frameIndex;
};

class Sampler {
public:
    virtual ~Sampler() = default;
    virtual Pixel sample(float x, float y) const = 0;
};

struct TemporalContext {
    const Sampler* previous2 = nullptr;
    const Sampler* previous = nullptr;
    const Sampler* next = nullptr;
    const Sampler* next2 = nullptr;
    bool hasPrevious2 = false;
    bool hasPrevious = false;
    bool hasNext = false;
    bool hasNext2 = false;
};

enum FrameDirectorView {
    FrameDirectorResult = 0,
    FrameDirectorGuides = 1,
    FrameDirectorSaliency = 2,
    FrameDirectorCropMask = 3,
    FrameDirectorConfidence = 4,
};

enum FramingMode {
    FramingAuto = 0,
    FramingCenter = 1,
    FramingLeftThird = 2,
    FramingRightThird = 3,
    FramingManual = 4,
};

struct FrameDirectorControls {
    int targetAspect;
    int viewMode;
    int framingMode;
    float autoStrength;
    float subjectWeight;
    float skinWeight;
    bool subjectLock;
    float lockX;
    float lockY;
    float cutSensitivity;
    float temporalSmoothing;
    float motionLimit;
    float manualX;
    float manualY;
    float subjectVerticalPosition;
    float guideOpacity;
    float matteOpacity;
    float cropFeather;
    float outputMix;
};

struct FocusAnalysis {
    float x;
    float y;
    float confidence;
};

struct CropWindow {
    float x1;
    float y1;
    float x2;
    float y2;
};

enum RadianceView {
    RadianceResult = 0,
    RadianceRecoveryMatte = 1,
    RadianceClippingMap = 2,
    RadianceDifference = 3,
    RadianceMLConfidence = 4,
    RadianceMLResult = 5,
};

struct RadianceControls {
    int viewMode;
    float highlightRecovery;
    float specularRecovery;
    float highlightRolloff;
    float shadowRecovery;
    float recoveredHeadroomStops;
    float localDetail;
    float shadowDenoise;
    float dequantization;
    float temporalConsistency;
    float longTermConsistency;
    float colorGuard;
    float outputMix;
};

enum TemporalView {
    TemporalRepair = 0,
    TemporalErrorMatte = 1,
    TemporalDifferenceOverlay = 2,
    TemporalMotionMatte = 3,
    TemporalConfidenceMatte = 4,
    TemporalGhostRiskMatte = 5,
};

struct TemporalIntegrityControls {
    int viewMode;
    float sensitivity;
    float repairStrength;
    float flickerRepair;
    float textureStability;
    float longTermStability;
    float motionGuard;
    float ghostGuard;
    float edgeProtection;
    float chromaStability;
    float overlayStrength;
    float outputMix;
};

class CinematicToolsCore {
public:
    static FrameDirectorControls defaultFrameDirectorControls();
    static RadianceControls defaultRadianceControls();
    static TemporalIntegrityControls defaultTemporalIntegrityControls();

    static float aspectRatioForChoice(int choice);

    static FocusAnalysis analyzeFocus(
        const Sampler& sampler,
        const FrameInfo& frame,
        const FrameDirectorControls& controls);

    static FocusAnalysis smoothFocus(
        const FocusAnalysis& current,
        const FocusAnalysis* previous2,
        const FocusAnalysis* previous,
        const FocusAnalysis* next,
        const FocusAnalysis* next2,
        const FrameDirectorControls& controls);

    static CropWindow cropWindow(
        const FrameInfo& frame,
        const FrameDirectorControls& controls,
        const FocusAnalysis& focus);

    static Pixel processFrameDirector(
        const Sampler& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const FrameDirectorControls& controls,
        const FocusAnalysis& focus,
        const CropWindow& crop);

    static Pixel processRadiance(
        const Sampler& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const RadianceControls& controls,
        const TemporalContext* temporal);

    static Pixel processTemporalIntegrity(
        const Sampler& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const TemporalIntegrityControls& controls,
        const TemporalContext* temporal);

private:
    static float clamp01(float value);
    static float smoothstep(float edge0, float edge1, float value);
    static float luma(Pixel pixel);
    static float maxChannel(Pixel pixel);
    static float minChannel(Pixel pixel);
    static float hash01(int x, int y, int z);
    static Pixel mix(Pixel a, Pixel b, float amount);
    static Pixel add(Pixel a, Pixel b);
    static Pixel mul(Pixel pixel, float amount);
    static Pixel clampDisplay(Pixel pixel);
    static Pixel median(Pixel a, Pixel b, Pixel c);
    static Pixel median5(Pixel a, Pixel b, Pixel c, Pixel d, Pixel e);
    static float channelMedian(float a, float b, float c);
    static float channelMedian5(float a, float b, float c, float d, float e);
    static float pixelDistance(Pixel a, Pixel b);
    static float localEdge(const Sampler& sampler, int x, int y);
    static Pixel crossBlur(const Sampler& sampler, int x, int y, float radius);
    static float saliencyAt(
        const Sampler& sampler,
        float x,
        float y,
        const FrameInfo& frame,
        const FrameDirectorControls& controls);
};

} // namespace buckswood_cinematic
