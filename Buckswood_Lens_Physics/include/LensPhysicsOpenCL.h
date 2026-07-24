#pragma once

#include "GpuRenderTypes.h"
#include "LensPhysicsCore.h"

namespace buckswood_lens {

bool runLensPhysicsOpenCL(
    void* commandQueue,
    const buckswood::gpu::ImageBuffer& source,
    const buckswood::gpu::ImageBuffer& destination,
    const buckswood::gpu::RenderWindow& renderWindow,
    const LensPhysicsCore::PreparedState& state,
    bool adjustmentLayerGuard);

} // namespace buckswood_lens
