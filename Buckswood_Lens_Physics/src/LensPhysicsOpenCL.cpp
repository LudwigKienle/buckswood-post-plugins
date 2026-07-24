#include "LensPhysicsOpenCL.h"

#include "OpenCLDynamic.h"

#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>

namespace buckswood_lens {
namespace {

const char* kLensPhysicsOpenCLSource = R"OPENCL(
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
} Params;

float clamp01(float value)
{
    return min(1.0f, max(0.0f, value));
}

float lensLuma(float4 pixel)
{
    return 0.2627f * pixel.x + 0.6780f * pixel.y + 0.0593f * pixel.z;
}

float lensSmoothstep(float edge0, float edge1, float value)
{
    const float t = clamp01((value - edge0) / max(0.0001f, edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

float4 mixPixel(float4 a, float4 b, float amount)
{
    const float inverse = 1.0f - amount;
    return (float4)(
        a.x * inverse + b.x * amount,
        a.y * inverse + b.y * amount,
        a.z * inverse + b.z * amount,
        a.w * inverse + b.w * amount);
}

float4 sampleSource(
    __global const float4* source,
    float x,
    float y,
    Params p)
{
    if (x < (float)p.srcX1 ||
        x > (float)(p.srcX2 - 1) ||
        y < (float)p.srcY1 ||
        y > (float)(p.srcY2 - 1)) {
        return (float4)(0.0f);
    }
    const int x0 = (int)x;
    const int y0 = (int)y;
    const int x1 = min(p.srcX2 - 1, x0 + 1);
    const int y1 = min(p.srcY2 - 1, y0 + 1);
    const float tx = x - (float)x0;
    const float ty = y - (float)y0;
    const float4 p00 = source[(y0 - p.srcY1) * p.srcRowPixels + (x0 - p.srcX1)];
    const float4 p10 = source[(y0 - p.srcY1) * p.srcRowPixels + (x1 - p.srcX1)];
    const float4 p01 = source[(y1 - p.srcY1) * p.srcRowPixels + (x0 - p.srcX1)];
    const float4 p11 = source[(y1 - p.srcY1) * p.srcRowPixels + (x1 - p.srcX1)];
    const float u = 1.0f - tx;
    const float v = 1.0f - ty;
    return (float4)(
        (p00.x * u + p10.x * tx) * v + (p01.x * u + p11.x * tx) * ty,
        (p00.y * u + p10.y * tx) * v + (p01.y * u + p11.y * tx) * ty,
        (p00.z * u + p10.z * tx) * v + (p01.z * u + p11.z * tx) * ty,
        (p00.w * u + p10.w * tx) * v + (p01.w * u + p11.w * tx) * ty);
}

float4 applyRadialLens(
    __global const float4* source,
    float srcX,
    float srcY,
    float dirX,
    float dirY,
    float tangentX,
    float tangentY,
    float radius,
    float opticalOverdrive,
    Params p)
{
    const float edge = lensSmoothstep(0.16f, 1.10f, radius);
    const float4 center = sampleSource(source, srcX, srcY, p);
    const float4 left = sampleSource(source, srcX - 1.0f, srcY, p);
    const float4 right = sampleSource(source, srcX + 1.0f, srcY, p);
    const float4 up = sampleSource(source, srcX, srcY - 1.0f, p);
    const float4 down = sampleSource(source, srcX, srcY + 1.0f, p);
    const float gradient =
        fabs(lensLuma(right) - lensLuma(left)) +
        fabs(lensLuma(down) - lensLuma(up));
    const float centerY = lensLuma(center);
    const float silhouetteRisk =
        lensSmoothstep(0.20f, 0.72f, gradient) *
        (1.0f - lensSmoothstep(0.12f, 0.46f, centerY));
    const float silhouetteGuard =
        1.0f - silhouetteRisk * (0.72f + clamp01(p.edgeHaloGuard) * 0.22f);
    const float od = max(0.0f, opticalOverdrive);
    const float caPx =
        p.chromaticAberration * od * edge * edge * 4.2f * silhouetteGuard;
    const float4 red = sampleSource(
        source,
        srcX + dirX * caPx,
        srcY + dirY * caPx,
        p);
    const float4 blue = sampleSource(
        source,
        srcX - dirX * caPx,
        srcY - dirY * caPx,
        p);
    float4 rgb = (float4)(red.x, center.y, blue.z, center.w);
    const float fringe =
        clamp01(p.fringing * od * edge * gradient * 1.32f * silhouetteGuard);
    rgb.x += fringe * (0.08f + 0.05f * edge);
    rgb.y -= fringe * 0.035f;
    rgb.z += fringe * (0.04f + 0.04f * edge);

    const float threshold = max(0.35f, p.bloomThreshold);
    const float bloomStrength = clamp01(p.bloom * od);
    if (bloomStrength > 0.0001f) {
        float3 bloom = (float3)(0.0f);
        float weightSum = 0.0f;
        const float radiusPx = 2.0f + bloomStrength * 8.0f;
        const float2 offsets[8] = {
            (float2)(1.0f, 0.0f),
            (float2)(-1.0f, 0.0f),
            (float2)(0.0f, 1.0f),
            (float2)(0.0f, -1.0f),
            (float2)(0.707f, 0.707f),
            (float2)(-0.707f, 0.707f),
            (float2)(0.707f, -0.707f),
            (float2)(-0.707f, -0.707f)
        };
        for (int i = 0; i < 8; ++i) {
            const float4 pixel = sampleSource(
                source,
                srcX + offsets[i].x * radiusPx,
                srcY + offsets[i].y * radiusPx,
                p);
            const float hot =
                clamp01((lensLuma(pixel) - threshold) / max(0.001f, 1.0f - threshold));
            const float weight = hot * hot;
            bloom.x += pixel.x * weight;
            bloom.y += pixel.y * weight;
            bloom.z += pixel.z * weight;
            weightSum += weight;
        }
        if (weightSum > 0.0001f) {
            bloom.x *= 1.0f / weightSum;
            bloom.y *= 1.0f / weightSum;
            bloom.z *= 1.0f / weightSum;
            const float hotCenter =
                clamp01((lensLuma(center) - threshold) / max(0.001f, 1.0f - threshold));
            const float amount =
                bloomStrength * (0.18f + 0.42f * hotCenter) * silhouetteGuard;
            rgb.x += bloom.x * amount;
            rgb.y += bloom.y * amount;
            rgb.z += bloom.z * amount;
        }
    }

    const float comaStrength =
        clamp01(p.coma * od) * edge * edge * silhouetteGuard;
    if (comaStrength > 0.0001f) {
        float3 comet = (float3)(0.0f);
        float weightSum = 0.0f;
        for (int i = 1; i <= 5; ++i) {
            const float fi = (float)i;
            const float spread = fi * (1.2f + comaStrength * 4.0f);
            const float tx = tangentX * spread + dirX * spread * 0.55f;
            const float ty = tangentY * spread + dirY * spread * 0.55f;
            const float4 pixel = sampleSource(source, srcX + tx, srcY + ty, p);
            const float hot = clamp01((lensLuma(pixel) - 0.58f) / 0.42f);
            const float weight = hot * (1.0f / fi);
            comet.x += pixel.x * weight;
            comet.y += pixel.y * weight;
            comet.z += pixel.z * weight;
            weightSum += weight;
        }
        if (weightSum > 0.0001f) {
            comet.x *= 1.0f / weightSum;
            comet.y *= 1.0f / weightSum;
            comet.z *= 1.0f / weightSum;
            rgb.x += comet.x * comaStrength * 0.34f;
            rgb.y += comet.y * comaStrength * 0.24f;
            rgb.z += comet.z * comaStrength * 0.18f;
        }
    }
    return rgb;
}

float4 processLensPixel(
    __global const float4* source,
    int x,
    int y,
    Params p)
{
    const float4 dry = sampleSource(source, (float)x, (float)y, p);
    const float cx = p.centerX;
    const float cy = p.centerY;
    const float nx = cx == 0.0f ? 0.0f : ((float)x - cx) / cx;
    const float ny = cy == 0.0f ? 0.0f : ((float)y - cy) / cy;
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

    const float4 dryLeft = sampleSource(source, (float)(x - 1), (float)y, p);
    const float4 dryRight = sampleSource(source, (float)(x + 1), (float)y, p);
    const float4 dryUp = sampleSource(source, (float)x, (float)(y - 1), p);
    const float4 dryDown = sampleSource(source, (float)x, (float)(y + 1), p);
    const float dryY = lensLuma(dry);
    const float dryLeftY = lensLuma(dryLeft);
    const float dryRightY = lensLuma(dryRight);
    const float dryUpY = lensLuma(dryUp);
    const float dryDownY = lensLuma(dryDown);
    const float neighborMinY = min(min(dryLeftY, dryRightY), min(dryUpY, dryDownY));
    const float neighborMaxY = max(max(dryLeftY, dryRightY), max(dryUpY, dryDownY));
    const float localSpan = max(neighborMaxY, dryY) - min(neighborMinY, dryY);
    const float crossGradient =
        fabs(dryRightY - dryLeftY) +
        fabs(dryDownY - dryUpY) +
        fabs(dryY - dryLeftY) * 0.5f +
        fabs(dryY - dryRightY) * 0.5f +
        fabs(dryY - dryUpY) * 0.5f +
        fabs(dryY - dryDownY) * 0.5f;
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
    rgb.x *= vignetteGain;
    rgb.y *= vignetteGain;
    rgb.z *= vignetteGain;
    const float corner = clamp01(p.cornerColor * od) * edge * edge;
    rgb.x += corner * (p.warmBias + 0.025f);
    rgb.y += corner * 0.006f;
    rgb.z += corner * (p.blueBias - 0.018f);

    if (p.sharpener != 0.0f) {
        const float4 center = sampleSource(source, srcX, srcY, p);
        const float4 b1 = sampleSource(source, srcX - 1.4f, srcY, p);
        const float4 b2 = sampleSource(source, srcX + 1.4f, srcY, p);
        const float4 b3 = sampleSource(source, srcX, srcY - 1.4f, p);
        const float4 b4 = sampleSource(source, srcX, srcY + 1.4f, p);
        const float3 blur = (float3)(
            (b1.x + b2.x + b3.x + b4.x) * 0.25f,
            (b1.y + b2.y + b3.y + b4.y) * 0.25f,
            (b1.z + b2.z + b3.z + b4.z) * 0.25f);
        const float sharp =
            p.sharpener * od * 0.32f * (1.0f - edgeGuard * 0.86f);
        rgb.x += (center.x - blur.x) * sharp;
        rgb.y += (center.y - blur.y) * sharp;
        rgb.z += (center.z - blur.z) * sharp;
    }

    if (edgeGuard > 0.0001f) {
        const float maxDelta =
            0.055f + (1.0f - edgeGuard) * 0.24f + localSpan * 0.18f;
        float4 deltaClamped = (float4)(
            dry.x + clamp(rgb.x - dry.x, -maxDelta, maxDelta),
            dry.y + clamp(rgb.y - dry.y, -maxDelta, maxDelta),
            dry.z + clamp(rgb.z - dry.z, -maxDelta, maxDelta),
            dry.w);
        const float minAllowedY =
            clamp01(min(neighborMinY, dryY) - (0.025f + localSpan * 0.10f));
        const float maxAllowedY =
            clamp01(max(neighborMaxY, dryY) + (0.025f + localSpan * 0.10f));
        const float clampedY = lensLuma(deltaClamped);
        if (clampedY < minAllowedY || clampedY > maxAllowedY) {
            const float targetY = clamp(clampedY, minAllowedY, maxAllowedY);
            const float correction = targetY - clampedY;
            deltaClamped.x += correction;
            deltaClamped.y += correction;
            deltaClamped.z += correction;
        }
        rgb = mixPixel(rgb, deltaClamped, edgeGuard);
        rgb = mixPixel(rgb, dry, edgeGuard * 0.22f);
    }
    rgb.x = clamp01(rgb.x);
    rgb.y = clamp01(rgb.y);
    rgb.z = clamp01(rgb.z);
    rgb.w = dry.w;
    return rgb;
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

__kernel void lensPhysicsFloat(
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
    if (blankAdjustment[0] != 0u) {
        *output = (float4)(0.0f);
        return;
    }
    *output = processLensPixel(source, x, y, p);
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
    buckswood::opencl::Program program = nullptr;
    buckswood::opencl::Kernel adjustmentGuard = nullptr;
    buckswood::opencl::Kernel lensPhysics = nullptr;
};

std::mutex gPipelineMutex;
std::mutex gDispatchMutex;
std::unordered_map<void*, Pipelines> gPipelines;

bool pipelinesForQueue(
    buckswood::opencl::CommandQueue queue,
    buckswood::opencl::Context& context,
    Pipelines& output)
{
    auto& runtime = buckswood::opencl::Runtime::instance();
    buckswood::opencl::Device device = nullptr;
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
        kLensPhysicsOpenCLSource,
        buildLog);
    if (!created.program) {
        std::fprintf(stderr, "Buckswood Lens Physics OpenCL compile failed: %s\n",
            buildLog.c_str());
        return false;
    }
    created.adjustmentGuard =
        runtime.createKernel(created.program, "adjustmentGuardFloat");
    created.lensPhysics =
        runtime.createKernel(created.program, "lensPhysicsFloat");
    if (!created.adjustmentGuard || !created.lensPhysics) {
        runtime.releaseKernel(created.adjustmentGuard);
        runtime.releaseKernel(created.lensPhysics);
        runtime.releaseProgram(created.program);
        return false;
    }
    gPipelines[context] = created;
    output = created;
    return true;
}

} // namespace

bool runLensPhysicsOpenCL(
    void* commandQueue,
    const buckswood::gpu::ImageBuffer& source,
    const buckswood::gpu::ImageBuffer& destination,
    const buckswood::gpu::RenderWindow& renderWindow,
    const LensPhysicsCore::PreparedState& state,
    bool adjustmentLayerGuard)
{
    if (!commandQueue ||
        source.format != buckswood::gpu::PixelFormat::Float32 ||
        destination.format != buckswood::gpu::PixelFormat::Float32) {
        return false;
    }

    auto& runtime = buckswood::opencl::Runtime::instance();
    buckswood::opencl::Context context = nullptr;
    Pipelines pipelines;
    if (!pipelinesForQueue(commandQueue, context, pipelines)) {
        return false;
    }

    const LensPhysicsCore::LensModel& model = state.model;
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

    buckswood::opencl::Memory sourceMemory = source.handle;
    buckswood::opencl::Memory destinationMemory = destination.handle;
    buckswood::opencl::Memory blankAdjustment =
        runtime.createBuffer(context, sizeof(buckswood::opencl::UInt));
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
    buckswood::opencl::Event guardEvent = nullptr;
    const bool guardQueued = guardArgs && runtime.enqueue1D(
        commandQueue,
        pipelines.adjustmentGuard,
        1,
        nullptr,
        &guardEvent);

    const bool renderArgs =
        runtime.setKernelArg(
            pipelines.lensPhysics,
            0,
            sizeof(sourceMemory),
            &sourceMemory) &&
        runtime.setKernelArg(
            pipelines.lensPhysics,
            1,
            sizeof(destinationMemory),
            &destinationMemory) &&
        runtime.setKernelArg(
            pipelines.lensPhysics,
            2,
            sizeof(blankAdjustment),
            &blankAdjustment) &&
        runtime.setKernelArg(
            pipelines.lensPhysics,
            3,
            sizeof(params),
            &params);
    const bool renderQueued = guardQueued && renderArgs && runtime.enqueue2D(
        commandQueue,
        pipelines.lensPhysics,
        static_cast<std::size_t>(params.renderWidth),
        static_cast<std::size_t>(params.renderHeight),
        guardEvent,
        nullptr);

    runtime.releaseEvent(guardEvent);
    runtime.releaseMemory(blankAdjustment);
    return renderQueued;
}

} // namespace buckswood_lens
