#pragma once

#include <cstdint>

namespace buckswood {

struct Pixel {
    float r;
    float g;
    float b;
    float a;
};

struct Masks {
    float skin;
    float plastic;
    float highlight;
    float edge;
    float detail;
};

struct Controls {
    float plasticReduction;
    float skinRealism;
    float highlightRealism;
    float colorNaturalize;
    float textureAmount;
    float microContrast;
    float gamutGuard;
    float shadowDepth;
    float smoothnessBreakup;
    float outputMix;
    float seed;
};

struct FrameInfo {
    int width;
    int height;
    int frameIndex;
};

class PhotorealizerCore {
public:
    static Pixel processPixel(
        Pixel input,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls,
        const Masks& masks = defaultMasks());

    static Masks defaultMasks();

private:
    static float clamp01(float value);
    static float luma(Pixel pixel);
    static float hash01(int x, int y, int z);
    static float softClip(float value, float knee, float amount);
    static float skinWeight(Pixel pixel);
    static Pixel mix(Pixel a, Pixel b, float t);
};

} // namespace buckswood
