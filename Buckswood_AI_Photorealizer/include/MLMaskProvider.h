#pragma once

#include "PhotorealizerCore.h"

namespace buckswood {

struct MaskRequest {
    int width;
    int height;
    int frameIndex;
};

class MLMaskProvider {
public:
    virtual ~MLMaskProvider() = default;
    virtual bool isAvailable() const = 0;
    virtual Masks masksForPixel(int x, int y, Pixel input, const MaskRequest& request) const = 0;
};

class NoopMaskProvider final : public MLMaskProvider {
public:
    bool isAvailable() const override { return false; }
    Masks masksForPixel(int, int, Pixel, const MaskRequest&) const override
    {
        return PhotorealizerCore::defaultMasks();
    }
};

} // namespace buckswood
