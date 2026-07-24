#include "PhotorealizerOpenCL.h"

#include "OpenCLDynamic.h"

#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>

namespace buckswood {
namespace {

const char* kPhotorealizerOpenCLSource = R"OPENCL(
typedef struct Params {
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
} Params;

float clamp01(float value)
{
    return min(1.0f, max(0.0f, value));
}

float photoLuma(float4 pixel)
{
    return 0.2627f * pixel.x + 0.6780f * pixel.y + 0.0593f * pixel.z;
}

float hash01(int x, int y, int z)
{
    uint h = (uint)x * 73856093u ^ (uint)y * 19349663u ^ (uint)z * 83492791u;
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return (float)(h & 0x00ffffffu) / 16777216.0f;
}

float softClipValue(float value, float knee, float amount)
{
    if (value <= knee) {
        return value;
    }
    const float over = value - knee;
    return knee + over / (1.0f + over * amount * 8.0f);
}

float skinWeight(float4 pixel)
{
    const float r = pixel.x;
    const float g = pixel.y;
    const float b = pixel.z;
    if (!(r > g && g > b)) {
        return 0.0f;
    }
    const float rg = r - g;
    const float gb = g - b;
    const float warm = (rg * gb) / (r * r + 0.001f);
    const float lum = photoLuma(pixel);
    const float exposure = clamp01(1.0f - fabs(lum - 0.52f) / 0.42f);
    return clamp01(warm * 3.0f) * exposure;
}

float4 processPixel(float4 input, int x, int y, Params p)
{
    float4 rgb = input;
    const float4 dry = input;
    const float lum0 = photoLuma(rgb);
    const float skin = clamp01(max(0.0f, skinWeight(rgb)) * p.skinRealism);
    const float plastic = clamp01(p.plasticReduction);
    const float highlight = clamp01(max(1.0f, (lum0 - 0.62f) / 0.28f));
    const float chromaR = rgb.x - lum0;
    const float chromaG = rgb.y - lum0;
    const float chromaB = rgb.z - lum0;
    const float chroma = fabs(chromaR) + fabs(chromaG) + fabs(chromaB);
    const float neon = clamp01((chroma - 0.22f) / 0.42f);
    const float gamutCompress = neon * p.gamutGuard;
    rgb.x = lum0 + chromaR * (1.0f - 0.38f * gamutCompress);
    rgb.y = lum0 + chromaG * (1.0f - 0.30f * gamutCompress);
    rgb.z = lum0 + chromaB * (1.0f - 0.34f * gamutCompress);
    rgb.x += 0.018f * skin;
    rgb.y += 0.007f * skin;
    rgb.z -= 0.020f * skin;

    const float lum1 = photoLuma(rgb);
    const float shapedLum = 0.42f + (lum1 - 0.42f) * (1.0f + p.microContrast * 0.18f);
    rgb.x += shapedLum - lum1;
    rgb.y += shapedLum - lum1;
    rgb.z += shapedLum - lum1;

    const float roll = p.highlightRealism * (0.45f + 0.55f * highlight);
    rgb.x = softClipValue(rgb.x, 0.74f, roll);
    rgb.y = softClipValue(rgb.y, 0.74f, roll);
    rgb.z = softClipValue(rgb.z, 0.74f, roll);

    const float lum2 = photoLuma(rgb);
    const float shadowMask = clamp01((0.42f - lum2) / 0.42f);
    rgb.x *= 1.0f - shadowMask * p.shadowDepth * 0.07f;
    rgb.y *= 1.0f - shadowMask * p.shadowDepth * 0.06f;
    rgb.z *= 1.0f - shadowMask * p.shadowDepth * 0.04f;

    const int textureSeed = (int)(p.seed * 997.0f) + p.frameIndex * 17;
    const float n1 = hash01(x, y, textureSeed) - 0.5f;
    const float n2 = hash01(x + 19, y - 7, textureSeed + 113) - 0.5f;
    const float n3 = hash01(x - 5, y + 23, textureSeed + 271) - 0.5f;
    const float textureMask =
        clamp01(0.20f + plastic * 0.65f + p.smoothnessBreakup * 0.35f);
    const float texture = p.textureAmount * textureMask * 0.018f;
    rgb.x += n1 * texture;
    rgb.y += n2 * texture;
    rgb.z += n3 * texture;

    const float lum3 = photoLuma(rgb);
    const float naturalSat =
        1.0f - p.colorNaturalize * (0.08f + neon * 0.22f + highlight * 0.12f);
    rgb.x = lum3 + (rgb.x - lum3) * naturalSat;
    rgb.y = lum3 + (rgb.y - lum3) * naturalSat;
    rgb.z = lum3 + (rgb.z - lum3) * naturalSat;
    rgb.x = clamp01(rgb.x);
    rgb.y = clamp01(rgb.y);
    rgb.z = clamp01(rgb.z);
    rgb.w = input.w;
    const float amount = clamp01(p.outputMix);
    return dry * (1.0f - amount) + rgb * amount;
}

__kernel void adjustmentGuardFloat(
    __global const float4* source,
    __global uint* blankAdjustment,
    Params p)
{
    if (get_global_id(0) != 0) {
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
            const float4 pixel = source[y * p.srcRowPixels + x];
            maxRgb = max(maxRgb, max(pixel.x, max(pixel.y, pixel.z)));
            alphaSum += pixel.w;
        }
    }
    blankAdjustment[0] =
        (alphaSum / (float)(grid * grid)) > 0.75f && maxRgb < 0.015f ? 1u : 0u;
}

__kernel void photorealizerFloat(
    __global const float4* source,
    __global float4* destination,
    __global const uint* blankAdjustment,
    Params p)
{
    const int gx = (int)get_global_id(0);
    const int gy = (int)get_global_id(1);
    if (gx >= p.renderWidth || gy >= p.renderHeight) {
        return;
    }
    const int x = p.renderX1 + gx;
    const int y = p.renderY1 + gy;
    __global float4* output = destination +
        (y - p.dstY1) * p.dstRowPixels + (x - p.dstX1);
    if (blankAdjustment[0] != 0u ||
        x < p.srcX1 || x >= p.srcX2 || y < p.srcY1 || y >= p.srcY2) {
        *output = (float4)(0.0f);
        return;
    }
    const float4 input = source[
        (y - p.srcY1) * p.srcRowPixels + (x - p.srcX1)];
    *output = processPixel(input, x, y, p);
}
)OPENCL";

struct OpenCLParams {
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
    opencl::Program program = nullptr;
    opencl::Kernel adjustmentGuard = nullptr;
    opencl::Kernel photorealizer = nullptr;
};

std::mutex gPipelineMutex;
std::mutex gDispatchMutex;
std::unordered_map<void*, Pipelines> gPipelines;

bool pipelinesForQueue(
    opencl::CommandQueue queue,
    opencl::Context& context,
    Pipelines& output)
{
    opencl::Runtime& runtime = opencl::Runtime::instance();
    opencl::Device device = nullptr;
    if (!runtime.queueContextAndDevice(queue, context, device)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(gPipelineMutex);
    const auto found = gPipelines.find(context);
    if (found != gPipelines.end()) {
        output = found->second;
        return true;
    }

    std::string buildLog;
    Pipelines created;
    created.program = runtime.buildProgram(
        context,
        device,
        kPhotorealizerOpenCLSource,
        buildLog);
    if (!created.program) {
        std::fprintf(stderr, "Buckswood Photorealizer OpenCL compile failed: %s\n",
            buildLog.c_str());
        return false;
    }
    created.adjustmentGuard =
        runtime.createKernel(created.program, "adjustmentGuardFloat");
    created.photorealizer =
        runtime.createKernel(created.program, "photorealizerFloat");
    if (!created.adjustmentGuard || !created.photorealizer) {
        runtime.releaseKernel(created.adjustmentGuard);
        runtime.releaseKernel(created.photorealizer);
        runtime.releaseProgram(created.program);
        return false;
    }
    gPipelines[context] = created;
    output = created;
    return true;
}

} // namespace

bool runPhotorealizerOpenCL(
    void* commandQueue,
    const gpu::ImageBuffer& source,
    const gpu::ImageBuffer& destination,
    const gpu::RenderWindow& renderWindow,
    const FrameInfo& frame,
    const Controls& controls,
    bool adjustmentLayerGuard)
{
    if (!commandQueue ||
        source.format != gpu::PixelFormat::Float32 ||
        destination.format != gpu::PixelFormat::Float32) {
        return false;
    }

    opencl::Runtime& runtime = opencl::Runtime::instance();
    opencl::Context context = nullptr;
    Pipelines pipelines;
    if (!pipelinesForQueue(commandQueue, context, pipelines)) {
        return false;
    }

    const OpenCLParams params{
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

    opencl::Memory sourceMemory = source.handle;
    opencl::Memory destinationMemory = destination.handle;
    opencl::Memory blankAdjustment =
        runtime.createBuffer(context, sizeof(opencl::UInt));
    if (!blankAdjustment) {
        return false;
    }

    // cl_kernel arguments are mutable state. Keep argument assignment and
    // enqueue atomic across concurrent Resolve renders that share the cache.
    std::lock_guard<std::mutex> dispatchLock(gDispatchMutex);
    const bool guardArgs =
        runtime.setKernelArg(
            pipelines.adjustmentGuard,
            0,
            sizeof(sourceMemory),
            &sourceMemory) &&
        runtime.setKernelArg(
            pipelines.adjustmentGuard,
            1,
            sizeof(blankAdjustment),
            &blankAdjustment) &&
        runtime.setKernelArg(
            pipelines.adjustmentGuard,
            2,
            sizeof(params),
            &params);
    opencl::Event guardEvent = nullptr;
    const bool guardQueued = guardArgs && runtime.enqueue1D(
        commandQueue,
        pipelines.adjustmentGuard,
        1,
        nullptr,
        &guardEvent);

    const bool renderArgs =
        runtime.setKernelArg(
            pipelines.photorealizer,
            0,
            sizeof(sourceMemory),
            &sourceMemory) &&
        runtime.setKernelArg(
            pipelines.photorealizer,
            1,
            sizeof(destinationMemory),
            &destinationMemory) &&
        runtime.setKernelArg(
            pipelines.photorealizer,
            2,
            sizeof(blankAdjustment),
            &blankAdjustment) &&
        runtime.setKernelArg(
            pipelines.photorealizer,
            3,
            sizeof(params),
            &params);
    const bool renderQueued = guardQueued && renderArgs && runtime.enqueue2D(
        commandQueue,
        pipelines.photorealizer,
        static_cast<std::size_t>(params.renderWidth),
        static_cast<std::size_t>(params.renderHeight),
        guardEvent,
        nullptr);

    runtime.releaseEvent(guardEvent);
    runtime.releaseMemory(blankAdjustment);
    return renderQueued;
}

} // namespace buckswood
