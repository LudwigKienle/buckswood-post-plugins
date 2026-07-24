#import <Metal/Metal.h>

#include "PhotorealizerMetal.h"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace buckswood {
namespace {

const char* kPhotorealizerMetalSource = R"METAL(
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
    int frameIndex;
    int adjustmentLayerGuard;
    float plasticReduction;
    float skinRealism;
    float highlightRealism;
    float colorNaturalize;
    float textureAmount;
    float microContrast;
    float gamutGuard;
    float shadowDepth;
    float smoothnessBreakup;
    float outputMix;
    float seed;
};

inline float clamp01(float value)
{
    return min(1.0f, max(0.0f, value));
}

inline float luma(float4 pixel)
{
    return 0.2627f * pixel.r + 0.6780f * pixel.g + 0.0593f * pixel.b;
}

inline float hash01(int x, int y, int z)
{
    uint h = uint(x) * 73856093u ^ uint(y) * 19349663u ^ uint(z) * 83492791u;
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return float(h & 0x00ffffffu) / 16777216.0f;
}

inline float softClip(float value, float knee, float amount)
{
    if (value <= knee) {
        return value;
    }
    const float over = value - knee;
    return knee + over / (1.0f + over * amount * 8.0f);
}

inline float skinWeight(float4 pixel)
{
    const float r = pixel.r;
    const float g = pixel.g;
    const float b = pixel.b;
    if (!(r > g && g > b)) {
        return 0.0f;
    }

    const float rg = r - g;
    const float gb = g - b;
    const float warm = (rg * gb) / (r * r + 0.001f);
    const float lum = luma(pixel);
    const float exposure = clamp01(1.0f - abs(lum - 0.52f) / 0.42f);
    return clamp01(warm * 3.0f) * exposure;
}

inline float4 processPixel(float4 input, int x, int y, constant Params& p)
{
    float4 rgb = input;
    const float4 dry = input;

    const float lum0 = luma(rgb);
    const float skin = clamp01(max(0.0f, skinWeight(rgb)) * p.skinRealism);
    const float plastic = clamp01(p.plasticReduction);
    const float highlight = clamp01(max(1.0f, (lum0 - 0.62f) / 0.28f));

    const float chromaR = rgb.r - lum0;
    const float chromaG = rgb.g - lum0;
    const float chromaB = rgb.b - lum0;
    const float chroma = abs(chromaR) + abs(chromaG) + abs(chromaB);
    const float neon = clamp01((chroma - 0.22f) / 0.42f);
    const float gamutCompress = neon * p.gamutGuard;
    rgb.r = lum0 + chromaR * (1.0f - 0.38f * gamutCompress);
    rgb.g = lum0 + chromaG * (1.0f - 0.30f * gamutCompress);
    rgb.b = lum0 + chromaB * (1.0f - 0.34f * gamutCompress);

    rgb.r += 0.018f * skin;
    rgb.g += 0.007f * skin;
    rgb.b -= 0.020f * skin;

    const float lum1 = luma(rgb);
    const float pivot = 0.42f;
    const float contrast = 1.0f + p.microContrast * 0.18f;
    const float shapedLum = pivot + (lum1 - pivot) * contrast;
    rgb.r += shapedLum - lum1;
    rgb.g += shapedLum - lum1;
    rgb.b += shapedLum - lum1;

    const float roll = p.highlightRealism * (0.45f + 0.55f * highlight);
    rgb.r = softClip(rgb.r, 0.74f, roll);
    rgb.g = softClip(rgb.g, 0.74f, roll);
    rgb.b = softClip(rgb.b, 0.74f, roll);

    const float lum2 = luma(rgb);
    const float shadowMask = clamp01((0.42f - lum2) / 0.42f);
    rgb.r *= 1.0f - shadowMask * p.shadowDepth * 0.07f;
    rgb.g *= 1.0f - shadowMask * p.shadowDepth * 0.06f;
    rgb.b *= 1.0f - shadowMask * p.shadowDepth * 0.04f;

    const int textureSeed = int(p.seed * 997.0f) + p.frameIndex * 17;
    const float n1 = hash01(x, y, textureSeed) - 0.5f;
    const float n2 = hash01(x + 19, y - 7, textureSeed + 113) - 0.5f;
    const float n3 = hash01(x - 5, y + 23, textureSeed + 271) - 0.5f;
    const float textureMask = clamp01(
        0.20f + plastic * 0.65f + p.smoothnessBreakup * 0.35f);
    const float texture = p.textureAmount * textureMask * 0.018f;
    rgb.r += n1 * texture;
    rgb.g += n2 * texture;
    rgb.b += n3 * texture;

    const float lum3 = luma(rgb);
    const float naturalSat =
        1.0f - p.colorNaturalize * (0.08f + neon * 0.22f + highlight * 0.12f);
    rgb.r = lum3 + (rgb.r - lum3) * naturalSat;
    rgb.g = lum3 + (rgb.g - lum3) * naturalSat;
    rgb.b = lum3 + (rgb.b - lum3) * naturalSat;

    rgb.r = clamp01(rgb.r);
    rgb.g = clamp01(rgb.g);
    rgb.b = clamp01(rgb.b);
    rgb.a = input.a;

    const float mixAmount = clamp01(p.outputMix);
    return dry * (1.0f - mixAmount) + rgb * mixAmount;
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

kernel void photorealizerFloat(
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
    const int dstX = x - p.dstX1;
    const int dstY = y - p.dstY1;
    device float4* output = destination + dstY * p.dstRowPixels + dstX;

    if (blankAdjustment[0] != 0u ||
        x < p.srcX1 || x >= p.srcX2 || y < p.srcY1 || y >= p.srcY2) {
        *output = float4(0.0f);
        return;
    }

    const float4 input = source[
        (y - p.srcY1) * p.srcRowPixels + (x - p.srcX1)];
    *output = processPixel(input, x, y, p);
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
    int frameIndex;
    int adjustmentLayerGuard;
    float plasticReduction;
    float skinRealism;
    float highlightRealism;
    float colorNaturalize;
    float textureAmount;
    float microContrast;
    float gamutGuard;
    float shadowDepth;
    float smoothnessBreakup;
    float outputMix;
    float seed;
};

struct Pipelines {
    id<MTLComputePipelineState> adjustmentGuard = nil;
    id<MTLComputePipelineState> photorealizer = nil;
};

std::mutex gPipelineMutex;
std::unordered_map<void*, Pipelines> gPipelines;

bool pipelineForFunction(
    id<MTLDevice> device,
    id<MTLLibrary> library,
    NSString* functionName,
    id<MTLComputePipelineState>* output)
{
    id<MTLFunction> function = [library newFunctionWithName:functionName];
    if (!function) {
        return false;
    }
    NSError* error = nil;
    *output = [device newComputePipelineStateWithFunction:function error:&error];
    [function release];
    if (!*output) {
        fprintf(stderr, "Buckswood Photorealizer Metal pipeline failed: %s\n",
            error.localizedDescription.UTF8String);
        return false;
    }
    return true;
}

bool pipelinesForDevice(id<MTLDevice> device, Pipelines& output)
{
    std::lock_guard<std::mutex> lock(gPipelineMutex);
    const void* key = static_cast<const void*>(device);
    const auto found = gPipelines.find(const_cast<void*>(key));
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
        newLibraryWithSource:[NSString stringWithUTF8String:kPhotorealizerMetalSource]
        options:options
        error:&error];
    [options release];
    if (!library) {
        fprintf(stderr, "Buckswood Photorealizer Metal compile failed: %s\n",
            error.localizedDescription.UTF8String);
        return false;
    }

    Pipelines created;
    const bool success =
        pipelineForFunction(device, library, @"adjustmentGuardFloat", &created.adjustmentGuard) &&
        pipelineForFunction(device, library, @"photorealizerFloat", &created.photorealizer);
    [library release];
    if (!success) {
        [created.adjustmentGuard release];
        [created.photorealizer release];
        return false;
    }

    gPipelines[const_cast<void*>(key)] = created;
    output = created;
    return true;
}

} // namespace

bool runPhotorealizerMetal(
    void* commandQueue,
    const gpu::ImageBuffer& source,
    const gpu::ImageBuffer& destination,
    const gpu::RenderWindow& renderWindow,
    const FrameInfo& frame,
    const Controls& controls,
    bool adjustmentLayerGuard,
    bool waitForCompletion)
{
    if (!commandQueue ||
        source.format != gpu::PixelFormat::Float32 ||
        destination.format != gpu::PixelFormat::Float32) {
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
        frame.frameIndex,
        adjustmentLayerGuard ? 1 : 0,
        controls.plasticReduction,
        controls.skinRealism,
        controls.highlightRealism,
        controls.colorNaturalize,
        controls.textureAmount,
        controls.microContrast,
        controls.gamutGuard,
        controls.shadowDepth,
        controls.smoothnessBreakup,
        controls.outputMix,
        controls.seed,
    };

    id<MTLBuffer> blankAdjustment = [device
        newBufferWithLength:sizeof(uint32_t)
        options:MTLResourceStorageModePrivate];
    if (!blankAdjustment) {
        return false;
    }

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    commandBuffer.label = @"Buckswood AI Photorealizer";

    id<MTLComputeCommandEncoder> guardEncoder = [commandBuffer computeCommandEncoder];
    [guardEncoder setComputePipelineState:pipelines.adjustmentGuard];
    [guardEncoder setBuffer:sourceBuffer offset:0 atIndex:0];
    [guardEncoder setBuffer:blankAdjustment offset:0 atIndex:1];
    [guardEncoder setBytes:&params length:sizeof(params) atIndex:2];
    [guardEncoder dispatchThreads:MTLSizeMake(1, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
    [guardEncoder endEncoding];

    id<MTLComputeCommandEncoder> renderEncoder = [commandBuffer computeCommandEncoder];
    [renderEncoder setComputePipelineState:pipelines.photorealizer];
    [renderEncoder setBuffer:sourceBuffer offset:0 atIndex:0];
    [renderEncoder setBuffer:destinationBuffer offset:0 atIndex:1];
    [renderEncoder setBuffer:blankAdjustment offset:0 atIndex:2];
    [renderEncoder setBytes:&params length:sizeof(params) atIndex:3];

    const NSUInteger executionWidth = pipelines.photorealizer.threadExecutionWidth;
    const NSUInteger maxThreads = pipelines.photorealizer.maxTotalThreadsPerThreadgroup;
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

} // namespace buckswood
