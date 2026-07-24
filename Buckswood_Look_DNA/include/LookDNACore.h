#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace buckswood_lookdna {

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

enum ColorSpace {
    TimelineDisplay = 0,
    Rec709Gamma24 = 1,
    SRGBDisplay = 2,
    SceneLinear = 3,
    ArriLogC3 = 4,
    ArriLogC4 = 5,
    SonySLog3 = 6,
    AppleLog = 7,
    BmdFilmGen5 = 8,
};

enum MatchMode {
    FullLook = 0,
    ColorOnly = 1,
    ToneOnly = 2,
    TextureOnly = 3,
};

enum ViewMode {
    ResultView = 0,
    SplitView = 1,
    DifferenceView = 2,
    ConfidenceView = 3,
    SkinMaskView = 4,
    SemanticZonesView = 5,
    ReferencePreviewView = 6,
    ToneMapView = 7,
    GamutWarningView = 8,
    SpatialMapView = 9,
    ReferenceBalanceView = 10,
    CutGuardView = 11,
};

enum SemanticZoneIndex {
    SkinZone = 0,
    SkyZone = 1,
    FoliageZone = 2,
    WarmLightZone = 3,
    ShadowZone = 4,
    HighlightZone = 5,
    SemanticZoneCount = 6,
};

struct SemanticZone {
    float meanU = 0.0f;
    float meanV = 0.0f;
    float weight = 0.0f;
};

struct LookProfile {
    bool valid = false;
    int sampleCount = 0;
    std::array<float, 7> lumaQuantiles{};
    float lumaMean = 0.0f;
    float lumaStd = 0.0f;
    float saturationMean = 0.0f;
    float localContrast = 0.0f;
    float grainEstimate = 0.0f;
    float chromaMeanU = 0.0f;
    float chromaMeanV = 0.0f;
    float chromaCovUU = 0.0f;
    float chromaCovUV = 0.0f;
    float chromaCovVV = 0.0f;
    std::array<SemanticZone, SemanticZoneCount> zones{};
};

constexpr int SpatialGridSize = 3;
constexpr int SpatialProfileCount = SpatialGridSize * SpatialGridSize;

struct ProfileGrid {
    std::array<LookProfile, SpatialProfileCount> cells{};
    bool valid = false;
};

struct Controls {
    int enabled = 1;
    int inputSpace = TimelineDisplay;
    int referenceSpace = SRGBDisplay;
    int matchMode = FullLook;
    int analysisQuality = 1;
    int viewMode = ResultView;
    int temporalRadius = 2;
    float referenceBMix = 0.35f;
    float referenceCMix = 0.20f;
    float referenceAdaptivity = 0.70f;
    float matchStrength = 0.82f;
    float toneMatch = 0.78f;
    float paletteMatch = 0.72f;
    float semanticMatch = 0.62f;
    float localContrastMatch = 0.42f;
    float textureMatch = 0.28f;
    float grainMatch = 0.18f;
    float densityMatch = 0.45f;
    float exposureLock = 0.35f;
    float skinProtect = 0.72f;
    float highlightProtect = 0.74f;
    float sceneIdentityGuard = 0.70f;
    float temporalStability = 0.65f;
    float spatialMatch = 0.35f;
    float shadowMatch = 0.82f;
    float midtoneMatch = 1.0f;
    float highlightMatch = 0.72f;
    float gamutGuard = 0.86f;
    float splitPosition = 0.50f;
    float outputMix = 1.0f;
};

class Sampler {
public:
    virtual ~Sampler() = default;
    virtual Pixel sample(float x, float y) const = 0;
    virtual Bounds bounds() const = 0;
};

struct MatchContext {
    LookProfile source;
    LookProfile reference;
    std::array<float, 7> toneTarget{};
    float chromaM00 = 1.0f;
    float chromaM01 = 0.0f;
    float chromaM10 = 0.0f;
    float chromaM11 = 1.0f;
    float densityRatio = 1.0f;
    float detailRatio = 1.0f;
    float extraGrain = 0.0f;
    std::array<float, SemanticZoneCount> semanticOverlap{};
    std::array<float, SemanticZoneCount> semanticZoneGuard{};
    float confidence = 0.0f;
    float guardedStrength = 0.0f;
    std::array<float, 3> referenceWeights{1.0f, 0.0f, 0.0f};
    float temporalConfidence = 1.0f;
    float spatialInfluence = 0.0f;
    bool valid = false;
};

class LookDNACore {
public:
    static LookProfile analyze(
        const Sampler& sampler,
        int colorSpace,
        int quality);

    static ProfileGrid analyzeGrid(
        const Sampler& sampler,
        int colorSpace,
        int quality);

    static LookProfile stabilizeProfile(
        const LookProfile& current,
        const LookProfile* previous,
        const LookProfile* next,
        float temporalStability);

    static LookProfile stabilizeProfile5(
        const LookProfile& current,
        const LookProfile* previous2,
        const LookProfile* previous1,
        const LookProfile* next1,
        const LookProfile* next2,
        float temporalStability,
        float* temporalConfidence = nullptr);

    static LookProfile blendReferenceProfiles(
        const LookProfile& source,
        const std::array<const LookProfile*, 3>& references,
        const std::array<float, 3>& weights,
        float adaptivity,
        std::array<float, 3>* normalizedWeights = nullptr);

    static MatchContext buildMatch(
        const LookProfile& source,
        const LookProfile& reference,
        const Controls& controls);

    static MatchContext sampleSpatialMatch(
        const MatchContext& globalMatch,
        const std::array<MatchContext, SpatialProfileCount>& spatialMatches,
        float normalizedX,
        float normalizedY,
        float spatialStrength,
        float exposureLock = 0.35f,
        float skinProtect = 0.72f,
        float highlightProtect = 0.74f);

    static Pixel processPixel(
        const Sampler& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls,
        const MatchContext& match,
        const Sampler* referencePreview = nullptr);

    static bool saveProfile(
        const std::string& path,
        const LookProfile& profile,
        std::string* error = nullptr);

    static bool loadProfile(
        const std::string& path,
        LookProfile& profile,
        std::string* error = nullptr);

    static float profileDistance(
        const LookProfile& a,
        const LookProfile& b);

private:
    static float clamp(float value, float low, float high);
    static float clamp01(float value);
    static float smoothstep(float edge0, float edge1, float value);
    static float luma(Pixel pixel);
    static Pixel mix(Pixel a, Pixel b, float amount);
    static Pixel encodedToWorking(Pixel pixel, int colorSpace);
    static Pixel workingToEncoded(Pixel pixel, int colorSpace);
    static float decodeTransfer(float value, int colorSpace);
    static float encodeTransfer(float value, int colorSpace);
    static float linearToWorking(float value);
    static float workingToLinear(float value);
    static float mapTone(float value, const MatchContext& match, float exposureLock);
    static void prepareMatch(
        MatchContext& match,
        float exposureLock,
        float skinProtect,
        float highlightProtect);
    static float hueDegrees(Pixel pixel);
    static float saturation(Pixel pixel);
    static std::array<float, SemanticZoneCount> semanticMasks(
        Pixel pixel,
        const LookProfile& source);
    static float skinMask(Pixel pixel);
    static float hash(int x, int y, int frameIndex);
};

} // namespace buckswood_lookdna
