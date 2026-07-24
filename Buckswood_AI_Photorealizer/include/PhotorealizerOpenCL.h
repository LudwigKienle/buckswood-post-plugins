#pragma once

#include "GpuRenderTypes.h"
#include "PhotorealizerCore.h"

namespace buckswood {

bool runPhotorealizerOpenCL(
    void* commandQueue,
    const gpu::ImageBuffer& source,
    const gpu::ImageBuffer& destination,
    const gpu::RenderWindow& renderWindow,
    const FrameInfo& frame,
    const Controls& controls,
    bool adjustmentLayerGuard);

} // namespace buckswood
