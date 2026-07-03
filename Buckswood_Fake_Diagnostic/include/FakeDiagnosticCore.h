#pragma once

namespace buckswood_fake {

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

struct Controls {
    float sensitivity;
    float overlayStrength;
    float correctionStrength;
    float plasticWeight;
    float highlightWeight;
    float edgeWeight;
    float gradeWeight;
    float textureWeight;
    float temporalWeight;
    float microContrast;
    float edgeSoften;
    float highlightRolloff;
    float gamutGuard;
    float sensorTexture;
    float temporalAssist;
    float seed;
};

struct Scores {
    float plastic;
    float highlight;
    float edge;
    float grade;
    float texture;
    float temporal;
    float overall;
};

enum ViewMode {
    ViewDiagnosticOverlay = 0,
    ViewProblemMatte = 1,
    ViewRealityMatchAssist = 2,
    ViewAssistWithOverlay = 3,
    ViewCategoryColors = 4,
    ViewTemporalStabilityMatte = 5,
    ViewTemporalDifferenceOverlay = 6,
};

class Sampler {
public:
    virtual ~Sampler() = default;
    virtual Pixel sample(float x, float y) const = 0;
};

struct TemporalContext {
    const Sampler* previous = nullptr;
    const Sampler* next = nullptr;
    bool hasPrevious = false;
    bool hasNext = false;
};

class FakeDiagnosticCore {
public:
    static Controls defaultControls();

    static Scores analyzePixel(
        const Sampler& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls,
        const TemporalContext* temporal = nullptr);

    static Pixel processPixel(
        const Sampler& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls,
        int viewMode,
        const TemporalContext* temporal = nullptr);

private:
    static float clamp01(float value);
    static float luma(Pixel pixel);
    static float max3(float a, float b, float c);
    static float min3(float a, float b, float c);
    static float smoothstep(float edge0, float edge1, float x);
    static float hash01(int x, int y, int z);
    static Pixel add(Pixel a, Pixel b);
    static Pixel sub(Pixel a, Pixel b);
    static Pixel mul(Pixel a, float scalar);
    static Pixel mix(Pixel a, Pixel b, float t);
    static Pixel clampPixel(Pixel pixel);
    static Pixel blur5(const Sampler& sampler, int x, int y);
    static Pixel categoryColor(const Scores& scores);
    static float temporalDifference(
        const Sampler& sampler,
        int x,
        int y,
        const TemporalContext* temporal);
    static float temporalRisk(
        const Sampler& sampler,
        int x,
        int y,
        const Scores& spatialScores,
        const TemporalContext* temporal);
    static Pixel applyAssist(
        const Sampler& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        Pixel input,
        const Scores& scores,
        const Controls& controls,
        const TemporalContext* temporal);
};

} // namespace buckswood_fake
