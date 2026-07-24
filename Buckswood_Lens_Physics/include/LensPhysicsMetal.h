#pragma once

#include "GpuRenderTypes.h"
#include "LensPhysicsCore.h"

namespace buckswood_lens {

bool runLensPhysicsMetal(
    void* commandQueue,
    const buckswood::gpu::ImageBuffer& source,
    const buckswood::gpu::ImageBuffer& destination,
    const buckswood::gpu::RenderWindow& renderWindow,
    const LensPhysicsCore::PreparedState& state,
    bool adjustmentLayerGuard,
    bool waitForCompletion = false);

} // namespace buckswood_lens
