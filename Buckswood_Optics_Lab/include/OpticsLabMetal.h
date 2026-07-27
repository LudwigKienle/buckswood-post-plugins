#pragma once

#include "GpuRenderTypes.h"
#include "OpticsLabCore.h"

namespace buckswood_optics {

bool runOpticsLabMetal(
    void* commandQueue,
    const buckswood::gpu::ImageBuffer& source,
    const buckswood::gpu::ImageBuffer& destination,
    const buckswood::gpu::RenderWindow& renderWindow,
    const OpticsLabCore::PreparedState& state,
    const AssetViews& assets,
    bool waitForCompletion = false);

} // namespace buckswood_optics
