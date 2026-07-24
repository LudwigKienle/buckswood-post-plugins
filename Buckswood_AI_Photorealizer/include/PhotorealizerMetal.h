#pragma once

#include "GpuRenderTypes.h"
#include "PhotorealizerCore.h"

namespace buckswood {

bool runPhotorealizerMetal(
    void* commandQueue,
    const gpu::ImageBuffer& source,
    const gpu::ImageBuffer& destination,
    const gpu::RenderWindow& renderWindow,
    const FrameInfo& frame,
    const Controls& controls,
    bool adjustmentLayerGuard,
    bool waitForCompletion = false);

} // namespace buckswood
