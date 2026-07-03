#include "BuckswoodPremiereAdapter.h"

namespace buckswood_adobe {

PixelF processPhotorealizerPixel(
    PixelF input,
    int x,
    int y,
    int width,
    int height,
    const buckswood::Controls& controls)
{
    buckswood::FrameInfo frame{width, height, 0};
    buckswood::Masks masks = buckswood::PhotorealizerCore::defaultMasks();
    buckswood::Pixel in{input.r, input.g, input.b, input.a};
    buckswood::Pixel out = buckswood::PhotorealizerCore::processPixel(in, x, y, frame, controls, masks);
    return {out.r, out.g, out.b, out.a};
}

PixelF processLensPixel(
    ImageViewF input,
    int x,
    int y,
    const buckswood_lens::Controls& controls)
{
    LensSampler sampler(input);
    buckswood_lens::FrameInfo frame{input.width, input.height, 0};
    buckswood_lens::Pixel out = buckswood_lens::LensPhysicsCore::processPixel(sampler, x, y, frame, controls);
    return {out.r, out.g, out.b, out.a};
}

} // namespace buckswood_adobe
