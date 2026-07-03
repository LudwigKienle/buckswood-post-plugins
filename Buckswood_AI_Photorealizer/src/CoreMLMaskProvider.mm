#include "MLMaskProvider.h"

#if defined(__APPLE__) && defined(BUCKSWOOD_ENABLE_COREML)
#import <CoreML/CoreML.h>
#endif

namespace buckswood {

// This file is intentionally a scaffold. The production path should load a
// small segmentation/regression model that emits soft masks for skin, plastic
// smoothness, highlights, edges, and detail. The OFX plugin currently defaults
// to the deterministic no-ML processor so Resolve remains stable.

class CoreMLMaskProvider final : public MLMaskProvider {
public:
    explicit CoreMLMaskProvider(const char*) {}

    bool isAvailable() const override
    {
#if defined(__APPLE__) && defined(BUCKSWOOD_ENABLE_COREML)
        return false;
#else
        return false;
#endif
    }

    Masks masksForPixel(int, int, Pixel, const MaskRequest&) const override
    {
        return PhotorealizerCore::defaultMasks();
    }
};

} // namespace buckswood
