#import <Metal/Metal.h>

#include "OpticsAssetLibrary.h"
#include "OpticsLabCore.h"
#include "OpticsLabMetal.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

namespace {

using buckswood_optics::AssetViews;
using buckswood_optics::Controls;
using buckswood_optics::FrameInfo;
using buckswood_optics::OpticsLabCore;
using buckswood_optics::Pixel;

struct Rgba {
    float r;
    float g;
    float b;
    float a;
};

class BufferSampler final : public buckswood_optics::Sampler {
public:
    BufferSampler(const Rgba* pixels, int width, int height)
        : pixels_(pixels)
        , width_(width)
        , height_(height)
    {
    }

    Pixel sample(float x, float y) const override
    {
        const float safeX = std::min(
            static_cast<float>(width_ - 1),
            std::max(0.0f, x));
        const float safeY = std::min(
            static_cast<float>(height_ - 1),
            std::max(0.0f, y));
        const int x0 = static_cast<int>(std::floor(safeX));
        const int y0 = static_cast<int>(std::floor(safeY));
        const int x1 = std::min(width_ - 1, x0 + 1);
        const int y1 = std::min(height_ - 1, y0 + 1);
        const float tx = safeX - static_cast<float>(x0);
        const float ty = safeY - static_cast<float>(y0);
        const float u = 1.0f - tx;
        const float v = 1.0f - ty;
        const Rgba& p00 = pixels_[y0 * width_ + x0];
        const Rgba& p10 = pixels_[y0 * width_ + x1];
        const Rgba& p01 = pixels_[y1 * width_ + x0];
        const Rgba& p11 = pixels_[y1 * width_ + x1];
        return Pixel{
            (p00.r * u + p10.r * tx) * v +
                (p01.r * u + p11.r * tx) * ty,
            (p00.g * u + p10.g * tx) * v +
                (p01.g * u + p11.g * tx) * ty,
            (p00.b * u + p10.b * tx) * v +
                (p01.b * u + p11.b * tx) * ty,
            (p00.a * u + p10.a * tx) * v +
                (p01.a * u + p11.a * tx) * ty,
        };
    }

private:
    const Rgba* pixels_;
    int width_;
    int height_;
};

Controls defaultControls()
{
    Controls controls{};
    controls.preset = 6;
    controls.effectStrength = 0.65f;
    controls.focalLength = 50.0f;
    controls.fStop = 2.8f;
    controls.focusDistance = 3.0f;
    controls.sensorWidth = 36.0f;
    controls.anamorphicSqueeze = 1.0f;
    controls.bloomThreshold = 0.82f;
    controls.depthFar = 1.0f;
    controls.depthGamma = 1.0f;
    controls.focusPlane = 0.5f;
    controls.grainSize = 1.0f;
    controls.grainSeed = 1.0f;
    controls.sensorISO = 400.0f;
    controls.apertureInfluence = 0.85f;
    controls.dirtScale = 1.0f;
    controls.smudgeScale = 1.0f;
    controls.edgeGuard = 0.80f;
    controls.outputMix = 0.65f;
    return controls;
}

void fillSource(Rgba* source, int width, int height)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float nx =
                static_cast<float>(x) /
                static_cast<float>(width - 1);
            const float ny =
                static_cast<float>(y) /
                static_cast<float>(height - 1);
            const bool highlight =
                x > width * 3 / 4 && y < height / 3;
            source[y * width + x] = Rgba{
                0.04f + nx * 0.94f + (highlight ? 0.85f : 0.0f),
                0.05f + ny * 0.66f + (highlight ? 0.70f : 0.0f),
                0.08f + (1.0f - nx) * 0.42f +
                    (highlight ? 0.48f : 0.0f),
                0.15f + ny * 0.75f,
            };
        }
    }
}

float difference(const Rgba& actual, const Pixel& expected)
{
    return std::max({
        std::fabs(actual.r - expected.r),
        std::fabs(actual.g - expected.g),
        std::fabs(actual.b - expected.b),
        std::fabs(actual.a - expected.a),
    });
}

template <typename Function>
double milliseconds(Function&& function, int runs)
{
    std::vector<double> samples;
    for (int run = 0; run < runs; ++run) {
        const auto begin = std::chrono::steady_clock::now();
        function();
        const auto end = std::chrono::steady_clock::now();
        samples.push_back(
            std::chrono::duration<double, std::milli>(
                end - begin).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

void renderCpuParallel(
    const BufferSampler& sampler,
    const OpticsLabCore::PreparedState& prepared,
    const AssetViews& assets,
    std::vector<Rgba>& destination,
    int width,
    int height)
{
    std::atomic<int> nextRow{0};
    const unsigned workerCount = std::max(
        1u,
        std::min(
            std::thread::hardware_concurrency(),
            static_cast<unsigned>(height)));
    std::vector<std::thread> workers;
    workers.reserve(workerCount);
    for (unsigned worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back([&] {
            for (;;) {
                const int y = nextRow.fetch_add(1);
                if (y >= height) {
                    break;
                }
                for (int x = 0; x < width; ++x) {
                    const Pixel pixel = OpticsLabCore::processPixel(
                        sampler,
                        x,
                        y,
                        prepared,
                        &assets);
                    destination[y * width + x] =
                        Rgba{pixel.r, pixel.g, pixel.b, pixel.a};
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
}

bool validateCase(
    id<MTLDevice> device,
    id<MTLCommandQueue> queue,
    const char* label,
    Controls controls,
    const AssetViews& assets)
{
    constexpr int width = 128;
    constexpr int height = 72;
    constexpr int pixelCount = width * height;
    id<MTLBuffer> sourceBuffer = [device
        newBufferWithLength:pixelCount * sizeof(Rgba)
        options:MTLResourceStorageModeShared];
    id<MTLBuffer> destinationBuffer = [device
        newBufferWithLength:pixelCount * sizeof(Rgba)
        options:MTLResourceStorageModeShared];
    auto* source = static_cast<Rgba*>(sourceBuffer.contents);
    auto* destination =
        static_cast<Rgba*>(destinationBuffer.contents);
    fillSource(source, width, height);
    std::fill(
        destination,
        destination + pixelCount,
        Rgba{0.0f, 0.0f, 0.0f, 0.0f});

    const FrameInfo frame{width, height, 42};
    const auto prepared = OpticsLabCore::prepare(frame, controls);
    const buckswood::gpu::ImageBuffer sourceImage{
        sourceBuffer,
        width * static_cast<int>(sizeof(Rgba)),
        0,
        0,
        width,
        height,
        buckswood::gpu::PixelFormat::Float32,
    };
    const buckswood::gpu::ImageBuffer destinationImage{
        destinationBuffer,
        width * static_cast<int>(sizeof(Rgba)),
        0,
        0,
        width,
        height,
        buckswood::gpu::PixelFormat::Float32,
    };
    const buckswood::gpu::RenderWindow window{
        0,
        0,
        width,
        height,
    };
    const bool rendered = buckswood_optics::runOpticsLabMetal(
        queue,
        sourceImage,
        destinationImage,
        window,
        prepared,
        assets,
        true);
    if (!rendered) {
        std::cerr << label << " Metal render failed\n";
        [destinationBuffer release];
        [sourceBuffer release];
        return false;
    }

    const BufferSampler sampler(source, width, height);
    float largestDifference = 0.0f;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Pixel expected = OpticsLabCore::processPixel(
                sampler,
                x,
                y,
                prepared,
                &assets);
            largestDifference = std::max(
                largestDifference,
                difference(destination[y * width + x], expected));
        }
    }
    std::cout << label << " Metal max difference: "
              << largestDifference << '\n';
    [destinationBuffer release];
    [sourceBuffer release];
    return std::isfinite(largestDifference) &&
        largestDifference <= 0.00005f;
}

bool runBenchmark(id<MTLDevice> device, id<MTLCommandQueue> queue)
{
    constexpr int width = 960;
    constexpr int height = 540;
    constexpr int pixelCount = width * height;
    id<MTLBuffer> sourceBuffer = [device
        newBufferWithLength:pixelCount * sizeof(Rgba)
        options:MTLResourceStorageModeShared];
    id<MTLBuffer> destinationBuffer = [device
        newBufferWithLength:pixelCount * sizeof(Rgba)
        options:MTLResourceStorageModeShared];
    auto* source = static_cast<Rgba*>(sourceBuffer.contents);
    fillSource(source, width, height);

    const Controls controls = defaultControls();
    const FrameInfo frame{width, height, 42};
    const auto prepared = OpticsLabCore::prepare(frame, controls);
    const AssetViews assets{};
    const BufferSampler sampler(source, width, height);
    std::vector<Rgba> cpuOutput(pixelCount);
    const buckswood::gpu::ImageBuffer sourceImage{
        sourceBuffer,
        width * static_cast<int>(sizeof(Rgba)),
        0,
        0,
        width,
        height,
        buckswood::gpu::PixelFormat::Float32,
    };
    const buckswood::gpu::ImageBuffer destinationImage{
        destinationBuffer,
        width * static_cast<int>(sizeof(Rgba)),
        0,
        0,
        width,
        height,
        buckswood::gpu::PixelFormat::Float32,
    };
    const buckswood::gpu::RenderWindow window{0, 0, width, height};

    buckswood_optics::runOpticsLabMetal(
        queue,
        sourceImage,
        destinationImage,
        window,
        prepared,
        assets,
        true);
    renderCpuParallel(
        sampler,
        prepared,
        assets,
        cpuOutput,
        width,
        height);
    const double cpuMs = milliseconds([&] {
        renderCpuParallel(
            sampler,
            prepared,
            assets,
            cpuOutput,
            width,
            height);
    }, 3);
    const double metalMs = milliseconds([&] {
        buckswood_optics::runOpticsLabMetal(
            queue,
            sourceImage,
            destinationImage,
            window,
            prepared,
            assets,
            true);
    }, 5);
    std::cout << "Optics Lab 960x540: CPU "
              << cpuMs << " ms, Metal "
              << metalMs << " ms, speedup "
              << cpuMs / metalMs << "x\n";
    [destinationBuffer release];
    [sourceBuffer release];
    return cpuMs > 0.0 && metalMs > 0.0;
}

} // namespace

int main()
{
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (!device || !queue) {
            std::cerr << "Metal is unavailable\n";
            return 1;
        }

        const AssetViews noAssets{};
        if (!validateCase(
                device,
                queue,
                "Default recipe",
                defaultControls(),
                noAssets)) {
            std::cerr << "Optics Lab default Metal quality regression\n";
            return 1;
        }

        Controls assetControls = defaultControls();
        assetControls.preset = 5;
        assetControls.effectStrength = 0.85f;
        assetControls.outputMix = 0.85f;
        assetControls.focusOffset = 0.72f;
        assetControls.defocus = 0.65f;
        assetControls.dirtAmount = 0.48f;
        assetControls.smudgeAmount = 0.38f;
        const auto loaded =
            buckswood_optics::OpticsAssetLibrary::load(
                "",
                0,
                0,
                2,
                3,
                1);
        const AssetViews assetViews = loaded.views();
        if (!validateCase(
                device,
                queue,
                "Built-in assets",
                assetControls,
                assetViews)) {
            std::cerr << "Optics Lab asset Metal quality regression\n";
            return 1;
        }

        Controls physicalControls = defaultControls();
        physicalControls.preset = 15;
        physicalControls.quality = 0;
        physicalControls.effectStrength = 0.90f;
        physicalControls.outputMix = 0.85f;
        physicalControls.focusOffset = -0.72f;
        physicalControls.defocus = 0.60f;
        physicalControls.catEye = 0.35f;
        physicalControls.fStop = 16.0f;
        physicalControls.starburst = 0.45f;
        physicalControls.irisBlades = 5;
        physicalControls.starUnevennessTrim = 0.25f;
        if (!validateCase(
                device,
                queue,
                "v1.3 physical iris preview",
                physicalControls,
                noAssets)) {
            std::cerr << "Optics Lab v1.3 Metal quality regression\n";
            return 1;
        }

        if (!runBenchmark(device, queue)) {
            std::cerr << "Optics Lab Metal benchmark failed\n";
            return 1;
        }

        [queue release];
        [device release];
    }
    return 0;
}
