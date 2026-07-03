#pragma once

namespace buckswood_lens {

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
    int lensPreset;
    float effectStrength;
    float distortion;
    float chromaticAberration;
    float fringing;
    float coma;
    float bloom;
    float bloomThreshold;
    float vignette;
    float cornerColor;
    float swirl;
    float fStopSharpener;
    float overdrive;
    float outputMix;
};

class Sampler {
public:
    virtual ~Sampler() = default;
    virtual Pixel sample(float x, float y) const = 0;
};

class LensPhysicsCore {
public:
    static Pixel processPixel(
        const Sampler& sampler,
        int x,
        int y,
        const FrameInfo& frame,
        const Controls& controls);

private:
    struct LensModel {
        float distortion;
        float chromaticAberration;
        float fringing;
        float coma;
        float bloom;
        float vignette;
        float cornerColor;
        float swirl;
        float sharpener;
        float warmBias;
        float blueBias;
    };

    static LensModel modelForPreset(int preset);
    static float clamp01(float value);
    static float luma(Pixel pixel);
    static float smoothstep(float edge0, float edge1, float x);
    static Pixel mix(Pixel a, Pixel b, float t);
    static Pixel add(Pixel a, Pixel b);
    static Pixel mul(Pixel a, float scalar);
    static Pixel applyRadialLens(
        const Sampler& sampler,
        float srcX,
        float srcY,
        float dirX,
        float dirY,
        float tangentX,
        float tangentY,
        float radius,
        const LensModel& model,
        const Controls& controls);
};

} // namespace buckswood_lens

