#import <Metal/Metal.h>

#include "LensPhysicsMetal.h"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace buckswood_lens {
namespace {

const char* kLensPhysicsMetalSource = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct Params {
    int srcRowPixels;
    int dstRowPixels;
    int srcX1;
    int srcY1;
    int srcX2;
    int srcY2;
    int dstX1;
    int dstY1;
    int renderX1;
    int renderY1;
    int renderWidth;
    int renderHeight;
    int adjustmentLayerGuard;
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
    float rawOverdrive;
    float baseOverdrive;
    float centerX;
    float centerY;
    float aspect;
    float bloomThreshold;
    float edgeHaloGuard;
};

inline float clamp01(float value)
{
    return min(1.0f, max(0.0f, value));
}

inline float lensLuma(float4 pixel)
{
    return 0.2627f * pixel.r + 0.6780f * pixel.g + 0.0593f * pixel.b;
}

inline float lensSmoothstep(float edge0, float edge1, float value)
{
    const float t = clamp01((value - edge0) / max(0.0001f, edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

inline float4 mixPixel(float4 a, float4 b, float amount)
{
    const float inverse = 1.0f - amount;
    return float4(
        a.r * inverse + b.r * amount,
        a.g * inverse + b.g * amount,
        a.b * inverse + b.b * amount,
        a.a * inverse + b.a * amount);
}

inline float4 sampleSource(device const float4* source, float x, float y, constant Params& p)
{
    const float minX = float(p.srcX1);
    const float minY = float(p.srcY1);
    const float maxX = float(p.srcX2 - 1);
    const float maxY = float(p.srcY2 - 1);
    if (x < minX || x > maxX || y < minY || y > maxY) {
        return float4(0.0f);
    }

    const int x0 = int(x);
    const int y0 = int(y);
    const int x1 = min(p.srcX2 - 1, x0 + 1);
    const int y1 = min(p.srcY2 - 1, y0 + 1);
    const float tx = x - float(x0);
    const float ty = y - float(y0);
    const float4 p00 = source[(y0 - p.srcY1) * p.srcRowPixels + (x0 - p.srcX1)];
    const float4 p10 = source[(y0 - p.srcY1) * p.srcRowPixels + (x1 - p.srcX1)];
    const float4 p01 = source[(y1 - p.srcY1) * p.srcRowPixels + (x0 - p.srcX1)];
    const float4 p11 = source[(y1 - p.srcY1) * p.srcRowPixels + (x1 - p.srcX1)];
    const float u = 1.0f - tx;
    const float v = 1.0f - ty;
    return float4(
        (p00.r * u + p10.r * tx) * v + (p01.r * u + p11.r * tx) * ty,
        (p00.g * u + p10.g * tx) * v + (p01.g * u + p11.g * tx) * ty,
        (p00.b * u + p10.b * tx) * v + (p01.b * u + p11.b * tx) * ty,
        (p00.a * u + p10.a * tx) * v + (p01.a * u + p11.a * tx) * ty);
}

inline float4 applyRadialLens(
    device const float4* source,
    float srcX,
    float srcY,
    float dirX,
    float dirY,
    float tangentX,
    float tangentY,
    float radius,
    float opticalOverdrive,
    constant Params& p)
{
    const float edge = lensSmoothstep(0.16f, 1.10f, radius);
    const float4 center = sampleSource(source, srcX, srcY, p);
    const float4 left = sampleSource(source, srcX - 1.0f, srcY, p);
    const float4 right = sampleSource(source, srcX + 1.0f, srcY, p);
    const float4 up = sampleSource(source, srcX, srcY - 1.0f, p);
    const float4 down = sampleSource(source, srcX, srcY + 1.0f, p);
    const float gradient =
        abs(lensLuma(right) - lensLuma(left)) +
        abs(lensLuma(down) - lensLuma(up));
    const float centerY = lensLuma(center);
    const float silhouetteRisk =
        lensSmoothstep(0.20f, 0.72f, gradient) *
        (1.0f - lensSmoothstep(0.12f, 0.46f, centerY));
    const float silhouetteGuard =
        1.0f - silhouetteRisk * (0.72f + clamp01(p.edgeHaloGuard) * 0.22f);
    const float od = max(0.0f, opticalOverdrive);
    const float caPx =
        p.chromaticAberration * od * edge * edge * 4.2f * silhouetteGuard;
    const float4 red = sampleSource(source, srcX + dirX * caPx, srcY + dirY * caPx, p);
    const float4 blue = sampleSource(source, srcX - dirX * caPx, srcY - dirY * caPx, p);
    float4 rgb = float4(red.r, center.g, blue.b, center.a);

    const float fringe =
        clamp01(p.fringing * od * edge * gradient * 1.32f * silhouetteGuard);
    rgb.r += fringe * (0.08f + 0.05f * edge);
    rgb.g -= fringe * 0.035f;
    rgb.b += fringe * (0.04f + 0.04f * edge);

    const float threshold = max(0.35f, p.bloomThreshold);
    const float bloomStrength = clamp01(p.bloom * od);
    if (bloomStrength > 0.0001f) {
        float3 bloom = float3(0.0f);
        float weightSum = 0.0f;
        const float radiusPx = 2.0f + bloomStrength * 8.0f;
        const float2 offsets[8] = {
            float2(1.0f, 0.0f),
            float2(-1.0f, 0.0f),
            float2(0.0f, 1.0f),
            float2(0.0f, -1.0f),
            float2(0.707f, 0.707f),
            float2(-0.707f, 0.707f),
            float2(0.707f, -0.707f),
            float2(-0.707f, -0.707f),
        };
        for (int i = 0; i < 8; ++i) {
            const float4 sample = sampleSource(
                source,
                srcX + offsets[i].x * radiusPx,
                srcY + offsets[i].y * radiusPx,
                p);
            const float hot =
                clamp01((lensLuma(sample) - threshold) / max(0.001f, 1.0f - threshold));
            const float weight = hot * hot;
            bloom.r += sample.r * weight;
            bloom.g += sample.g * weight;
            bloom.b += sample.b * weight;
            weightSum += weight;
        }
        if (weightSum > 0.0001f) {
            bloom.r *= 1.0f / weightSum;
            bloom.g *= 1.0f / weightSum;
            bloom.b *= 1.0f / weightSum;
            const float hotCenter =
                clamp01((lensLuma(center) - threshold) / max(0.001f, 1.0f - threshold));
            const float amount =
                bloomStrength * (0.18f + 0.42f * hotCenter) * silhouetteGuard;
            rgb.r += bloom.r * amount;
            rgb.g += bloom.g * amount;
            rgb.b += bloom.b * amount;
        }
    }

    const float comaStrength =
        clamp01(p.coma * od) * edge * edge * silhouetteGuard;
    if (comaStrength > 0.0001f) {
        float3 comet = float3(0.0f);
        float weightSum = 0.0f;
        for (int i = 1; i <= 5; ++i) {
            const float fi = float(i);
            const float spread = fi * (1.2f + comaStrength * 4.0f);
            const float tx = tangentX * spread + dirX * spread * 0.55f;
            const float ty = tangentY * spread + dirY * spread * 0.55f;
            const float4 sample = sampleSource(source, srcX + tx, srcY + ty, p);
            const float hot = clamp01((lensLuma(sample) - 0.58f) / 0.42f);
            const float weight = hot * (1.0f / fi);
            comet.r += sample.r * weight;
            comet.g += sample.g * weight;
            comet.b += sample.b * weight;
            weightSum += weight;
        }
        if (weightSum > 0.0001f) {
            comet.r *= 1.0f / weightSum;
            comet.g *= 1.0f / weightSum;
            comet.b *= 1.0f / weightSum;
            rgb.r += comet.r * comaStrength * 0.34f;
            rgb.g += comet.g * comaStrength * 0.24f;
            rgb.b += comet.b * comaStrength * 0.18f;
        }
    }
    return rgb;
}

inline float4 processLensPixel(
    device const float4* source,
    int x,
    int y,
    constant Params& p)
{
    const float4 dry = sampleSource(source, float(x), float(y), p);
    const float cx = p.centerX;
    const float cy = p.centerY;
    const float nx = cx == 0.0f ? 0.0f : (float(x) - cx) / cx;
    const float ny = cy == 0.0f ? 0.0f : (float(y) - cy) / cy;
    const float ax = nx * p.aspect;
    const float radius = sqrt(ax * ax + ny * ny);
    const float edge = lensSmoothstep(0.18f, 1.18f, radius);

    float dirX = nx;
    float dirY = ny;
    const float dirLen = sqrt(dirX * dirX + dirY * dirY);
    if (dirLen > 0.0001f) {
        dirX /= dirLen;
        dirY /= dirLen;
    }
    const float tangentX = -dirY;
    const float tangentY = dirX;

    const float4 dryLeft = sampleSource(source, float(x - 1), float(y), p);
    const float4 dryRight = sampleSource(source, float(x + 1), float(y), p);
    const float4 dryUp = sampleSource(source, float(x), float(y - 1), p);
    const float4 dryDown = sampleSource(source, float(x), float(y + 1), p);
    const float dryY = lensLuma(dry);
    const float dryLeftY = lensLuma(dryLeft);
    const float dryRightY = lensLuma(dryRight);
    const float dryUpY = lensLuma(dryUp);
    const float dryDownY = lensLuma(dryDown);
    const float neighborMinY = min(min(dryLeftY, dryRightY), min(dryUpY, dryDownY));
    const float neighborMaxY = max(max(dryLeftY, dryRightY), max(dryUpY, dryDownY));
    const float localSpan = max(neighborMaxY, dryY) - min(neighborMinY, dryY);
    const float crossGradient =
        abs(dryRightY - dryLeftY) +
        abs(dryDownY - dryUpY) +
        abs(dryY - dryLeftY) * 0.5f +
        abs(dryY - dryRightY) * 0.5f +
        abs(dryY - dryUpY) * 0.5f +
        abs(dryY - dryDownY) * 0.5f;
    const float edgeRisk =
        lensSmoothstep(0.035f, 0.26f, localSpan + crossGradient * 0.26f);
    const float darkSilhouette =
        (1.0f - lensSmoothstep(0.10f, 0.44f, dryY)) *
        lensSmoothstep(0.10f, 0.42f, neighborMaxY - dryY);
    const float brightSilhouette =
        lensSmoothstep(0.54f, 0.90f, dryY) *
        lensSmoothstep(0.10f, 0.42f, dryY - neighborMinY);
    const float contourRisk =
        lensSmoothstep(0.065f, 0.34f, neighborMaxY - neighborMinY);
    const float silhouetteRisk = clamp01(edgeRisk * (
        0.34f +
        0.46f * max(darkSilhouette, brightSilhouette) +
        0.20f * contourRisk));
    const float overdrivePressure =
        lensSmoothstep(0.18f, 0.85f, clamp01(p.rawOverdrive));
    const float edgeGuard =
        clamp01(p.edgeHaloGuard) * silhouetteRisk *
        (0.42f + 0.58f * overdrivePressure);
    const float opticalOverdrive =
        p.baseOverdrive *
        (1.0f - edgeGuard * (0.62f + 0.24f * overdrivePressure));
    const float od = p.baseOverdrive;

    const float swirlAngle = p.swirl * od * edge * edge * 0.22f;
    const float cosAngle = cos(swirlAngle);
    const float sinAngle = sin(swirlAngle);
    const float sx = nx * cosAngle - ny * sinAngle;
    const float sy = nx * sinAngle + ny * cosAngle;
    const float radiusSquared = radius * radius;
    const float radialScale =
        1.0f +
        p.distortion * od * radiusSquared +
        p.distortion * od * 0.35f * radiusSquared * radiusSquared;
    const float srcX = cx + sx * radialScale * cx;
    const float srcY = cy + sy * radialScale * cy;

    float4 rgb = applyRadialLens(
        source,
        srcX,
        srcY,
        dirX,
        dirY,
        tangentX,
        tangentY,
        radius,
        opticalOverdrive,
        p);

    const float vignette = clamp01(p.vignette * od) * edge * edge;
    const float vignetteGain = 1.0f - vignette * 0.46f;
    rgb.r *= vignetteGain;
    rgb.g *= vignetteGain;
    rgb.b *= vignetteGain;

    const float corner = clamp01(p.cornerColor * od) * edge * edge;
    rgb.r += corner * (p.warmBias + 0.025f);
    rgb.g += corner * 0.006f;
    rgb.b += corner * (p.blueBias - 0.018f);

    if (p.sharpener != 0.0f) {
        const float4 center = sampleSource(source, srcX, srcY, p);
        const float4 b1 = sampleSource(source, srcX - 1.4f, srcY, p);
        const float4 b2 = sampleSource(source, srcX + 1.4f, srcY, p);
        const float4 b3 = sampleSource(source, srcX, srcY - 1.4f, p);
        const float4 b4 = sampleSource(source, srcX, srcY + 1.4f, p);
        const float3 blur = float3(
            (b1.r + b2.r + b3.r + b4.r) * 0.25f,
            (b1.g + b2.g + b3.g + b4.g) * 0.25f,
            (b1.b + b2.b + b3.b + b4.b) * 0.25f);
        const float sharp =
            p.sharpener * od * 0.32f * (1.0f - edgeGuard * 0.86f);
        rgb.r += (center.r - blur.r) * sharp;
        rgb.g += (center.g - blur.g) * sharp;
        rgb.b += (center.b - blur.b) * sharp;
    }

    if (edgeGuard > 0.0001f) {
        const float maxDelta =
            0.055f + (1.0f - edgeGuard) * 0.24f + localSpan * 0.18f;
        float4 deltaClamped = float4(
            dry.r + clamp(rgb.r - dry.r, -maxDelta, maxDelta),
            dry.g + clamp(rgb.g - dry.g, -maxDelta, maxDelta),
            dry.b + clamp(rgb.b - dry.b, -maxDelta, maxDelta),
            dry.a);
        const float minAllowedY =
            clamp01(min(neighborMinY, dryY) - (0.025f + localSpan * 0.10f));
        const float maxAllowedY =
            clamp01(max(neighborMaxY, dryY) + (0.025f + localSpan * 0.10f));
        const float clampedY = lensLuma(deltaClamped);
        if (clampedY < minAllowedY || clampedY > maxAllowedY) {
            const float targetY = clamp(clampedY, minAllowedY, maxAllowedY);
            const float correction = targetY - clampedY;
            deltaClamped.r += correction;
            deltaClamped.g += correction;
            deltaClamped.b += correction;
        }
        rgb = mixPixel(rgb, deltaClamped, edgeGuard);
        rgb = mixPixel(rgb, dry, edgeGuard * 0.22f);
    }

    rgb.r = clamp01(rgb.r);
    rgb.g = clamp01(rgb.g);
    rgb.b = clamp01(rgb.b);
    rgb.a = dry.a;
    return rgb;
}

kernel void adjustmentGuardFloat(
    device const float4* source [[buffer(0)]],
    device uint* blankAdjustment [[buffer(1)]],
    constant Params& p [[buffer(2)]],
    uint index [[thread_position_in_grid]])
{
    if (index != 0) {
        return;
    }
    if (p.adjustmentLayerGuard == 0) {
        blankAdjustment[0] = 0;
        return;
    }

    const int width = p.srcX2 - p.srcX1;
    const int height = p.srcY2 - p.srcY1;
    if (width <= 0 || height <= 0) {
        blankAdjustment[0] = 0;
        return;
    }

    float maxRgb = 0.0f;
    float alphaSum = 0.0f;
    const int grid = 9;
    for (int gy = 0; gy < grid; ++gy) {
        const int y = (height - 1) * gy / (grid - 1);
        for (int gx = 0; gx < grid; ++gx) {
            const int x = (width - 1) * gx / (grid - 1);
            const float4 sample = source[y * p.srcRowPixels + x];
            maxRgb = max(maxRgb, max(sample.r, max(sample.g, sample.b)));
            alphaSum += sample.a;
        }
    }
    blankAdjustment[0] =
        (alphaSum / float(grid * grid)) > 0.75f && maxRgb < 0.015f ? 1u : 0u;
}

kernel void lensPhysicsFloat(
    device const float4* source [[buffer(0)]],
    device float4* destination [[buffer(1)]],
    device const uint* blankAdjustment [[buffer(2)]],
    constant Params& p [[buffer(3)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= uint(p.renderWidth) || gid.y >= uint(p.renderHeight)) {
        return;
    }

    const int x = p.renderX1 + int(gid.x);
    const int y = p.renderY1 + int(gid.y);
    device float4* output = destination +
        (y - p.dstY1) * p.dstRowPixels + (x - p.dstX1);
    if (blankAdjustment[0] != 0u) {
        *output = float4(0.0f);
        return;
    }
    *output = processLensPixel(source, x, y, p);
}
)METAL";

struct MetalParams {
    int srcRowPixels;
    int dstRowPixels;
    int srcX1;
    int srcY1;
    int srcX2;
    int srcY2;
    int dstX1;
    int dstY1;
    int renderX1;
    int renderY1;
    int renderWidth;
    int renderHeight;
    int adjustmentLayerGuard;
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
    float rawOverdrive;
    float baseOverdrive;
    float centerX;
    float centerY;
    float aspect;
    float bloomThreshold;
    float edgeHaloGuard;
};

struct Pipelines {
    id<MTLComputePipelineState> adjustmentGuard = nil;
    id<MTLComputePipelineState> lensPhysics = nil;
};

std::mutex gPipelineMutex;
std::unordered_map<void*, Pipelines> gPipelines;

bool createPipeline(
    id<MTLDevice> device,
    id<MTLLibrary> library,
    NSString* name,
    id<MTLComputePipelineState>* output)
{
    id<MTLFunction> function = [library newFunctionWithName:name];
    if (!function) {
        return false;
    }
    NSError* error = nil;
    *output = [device newComputePipelineStateWithFunction:function error:&error];
    [function release];
    if (!*output) {
        fprintf(stderr, "Buckswood Lens Physics Metal pipeline failed: %s\n",
            error.localizedDescription.UTF8String);
        return false;
    }
    return true;
}

bool pipelinesForDevice(id<MTLDevice> device, Pipelines& output)
{
    std::lock_guard<std::mutex> lock(gPipelineMutex);
    void* key = static_cast<void*>(device);
    const auto found = gPipelines.find(key);
    if (found != gPipelines.end()) {
        output = found->second;
        return true;
    }

    MTLCompileOptions* options = [MTLCompileOptions new];
    if (@available(macOS 15.0, *)) {
        options.mathMode = MTLMathModeSafe;
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        options.fastMathEnabled = NO;
#pragma clang diagnostic pop
    }
    NSError* error = nil;
    id<MTLLibrary> library = [device
        newLibraryWithSource:[NSString stringWithUTF8String:kLensPhysicsMetalSource]
        options:options
        error:&error];
    [options release];
    if (!library) {
        fprintf(stderr, "Buckswood Lens Physics Metal compile failed: %s\n",
            error.localizedDescription.UTF8String);
        return false;
    }

    Pipelines created;
    const bool success =
        createPipeline(device, library, @"adjustmentGuardFloat", &created.adjustmentGuard) &&
        createPipeline(device, library, @"lensPhysicsFloat", &created.lensPhysics);
    [library release];
    if (!success) {
        [created.adjustmentGuard release];
        [created.lensPhysics release];
        return false;
    }

    gPipelines[key] = created;
    output = created;
    return true;
}

} // namespace

bool runLensPhysicsMetal(
    void* commandQueue,
    const buckswood::gpu::ImageBuffer& source,
    const buckswood::gpu::ImageBuffer& destination,
    const buckswood::gpu::RenderWindow& renderWindow,
    const LensPhysicsCore::PreparedState& state,
    bool adjustmentLayerGuard,
    bool waitForCompletion)
{
    if (!commandQueue ||
        source.format != buckswood::gpu::PixelFormat::Float32 ||
        destination.format != buckswood::gpu::PixelFormat::Float32) {
        return false;
    }

    id<MTLCommandQueue> queue = static_cast<id<MTLCommandQueue>>(commandQueue);
    id<MTLDevice> device = queue.device;
    Pipelines pipelines;
    if (!pipelinesForDevice(device, pipelines)) {
        return false;
    }

    id<MTLBuffer> sourceBuffer = reinterpret_cast<id<MTLBuffer>>(source.handle);
    id<MTLBuffer> destinationBuffer = reinterpret_cast<id<MTLBuffer>>(destination.handle);
    if (!sourceBuffer || !destinationBuffer) {
        return false;
    }

    const LensPhysicsCore::LensModel& model = state.model;
    const MetalParams params{
        source.rowBytes / static_cast<int>(sizeof(float) * 4),
        destination.rowBytes / static_cast<int>(sizeof(float) * 4),
        source.x1,
        source.y1,
        source.x2,
        source.y2,
        destination.x1,
        destination.y1,
        renderWindow.x1,
        renderWindow.y1,
        renderWindow.x2 - renderWindow.x1,
        renderWindow.y2 - renderWindow.y1,
        adjustmentLayerGuard ? 1 : 0,
        model.distortion,
        model.chromaticAberration,
        model.fringing,
        model.coma,
        model.bloom,
        model.vignette,
        model.cornerColor,
        model.swirl,
        model.sharpener,
        model.warmBias,
        model.blueBias,
        state.rawOverdrive,
        state.baseOverdrive,
        state.centerX,
        state.centerY,
        state.aspect,
        state.controls.bloomThreshold,
        state.controls.edgeHaloGuard,
    };

    id<MTLBuffer> blankAdjustment = [device
        newBufferWithLength:sizeof(uint32_t)
        options:MTLResourceStorageModePrivate];
    if (!blankAdjustment) {
        return false;
    }

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    commandBuffer.label = @"Buckswood Lens Physics";

    id<MTLComputeCommandEncoder> guardEncoder = [commandBuffer computeCommandEncoder];
    [guardEncoder setComputePipelineState:pipelines.adjustmentGuard];
    [guardEncoder setBuffer:sourceBuffer offset:0 atIndex:0];
    [guardEncoder setBuffer:blankAdjustment offset:0 atIndex:1];
    [guardEncoder setBytes:&params length:sizeof(params) atIndex:2];
    [guardEncoder dispatchThreads:MTLSizeMake(1, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
    [guardEncoder endEncoding];

    id<MTLComputeCommandEncoder> renderEncoder = [commandBuffer computeCommandEncoder];
    [renderEncoder setComputePipelineState:pipelines.lensPhysics];
    [renderEncoder setBuffer:sourceBuffer offset:0 atIndex:0];
    [renderEncoder setBuffer:destinationBuffer offset:0 atIndex:1];
    [renderEncoder setBuffer:blankAdjustment offset:0 atIndex:2];
    [renderEncoder setBytes:&params length:sizeof(params) atIndex:3];
    const NSUInteger executionWidth = pipelines.lensPhysics.threadExecutionWidth;
    const NSUInteger maxThreads = pipelines.lensPhysics.maxTotalThreadsPerThreadgroup;
    const NSUInteger groupHeight = std::max<NSUInteger>(
        1,
        std::min<NSUInteger>(8, maxThreads / executionWidth));
    [renderEncoder
        dispatchThreads:MTLSizeMake(params.renderWidth, params.renderHeight, 1)
        threadsPerThreadgroup:MTLSizeMake(executionWidth, groupHeight, 1)];
    [renderEncoder endEncoding];

    [commandBuffer commit];
    if (waitForCompletion) {
        [commandBuffer waitUntilCompleted];
    }
    [blankAdjustment release];
    return commandBuffer.status != MTLCommandBufferStatusError;
}

} // namespace buckswood_lens
