#import <Metal/Metal.h>

#include "LensPhysicsCore.h"
#include "LensPhysicsMetal.h"
#include "PhotorealizerCore.h"
#include "PhotorealizerMetal.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

namespace {

struct Rgba {
    float r;
    float g;
    float b;
    float a;
};

class BufferSampler {
public:
    BufferSampler(const Rgba* pixels, int width, int height)
        : pixels_(pixels)
        , width_(width)
        , height_(height)
    {
    }

    buckswood_lens::Pixel sample(float x, float y) const
    {
        if (x < 0.0f ||
            x > static_cast<float>(width_ - 1) ||
            y < 0.0f ||
            y > static_cast<float>(height_ - 1)) {
            return buckswood_lens::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }
        const int x0 = static_cast<int>(x);
        const int y0 = static_cast<int>(y);
        const int x1 = std::min(width_ - 1, x0 + 1);
        const int y1 = std::min(height_ - 1, y0 + 1);
        const float tx = x - static_cast<float>(x0);
        const float ty = y - static_cast<float>(y0);
        const float u = 1.0f - tx;
        const float v = 1.0f - ty;
        const Rgba& p00 = pixels_[y0 * width_ + x0];
        const Rgba& p10 = pixels_[y0 * width_ + x1];
        const Rgba& p01 = pixels_[y1 * width_ + x0];
        const Rgba& p11 = pixels_[y1 * width_ + x1];
        return buckswood_lens::Pixel{
            (p00.r * u + p10.r * tx) * v + (p01.r * u + p11.r * tx) * ty,
            (p00.g * u + p10.g * tx) * v + (p01.g * u + p11.g * tx) * ty,
            (p00.b * u + p10.b * tx) * v + (p01.b * u + p11.b * tx) * ty,
            (p00.a * u + p10.a * tx) * v + (p01.a * u + p11.a * tx) * ty,
        };
    }

private:
    const Rgba* pixels_;
    int width_;
    int height_;
};

template <typename RowFn>
void parallelRows(int height, const RowFn& rowFn)
{
    const unsigned int threadCount = std::min<unsigned int>(
        std::max(1u, std::thread::hardware_concurrency()),
        static_cast<unsigned int>(height));
    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    for (unsigned int index = 0; index < threadCount; ++index) {
        const int first = height * static_cast<int>(index) /
            static_cast<int>(threadCount);
        const int last = height * static_cast<int>(index + 1) /
            static_cast<int>(threadCount);
        workers.emplace_back(rowFn, first, last);
    }
    for (auto& worker : workers) {
        worker.join();
    }
}

double milliseconds(const std::function<void()>& operation, int iterations)
{
    const auto start = std::chrono::steady_clock::now();
    for (int index = 0; index < iterations; ++index) {
        operation();
    }
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count() /
        static_cast<double>(iterations);
}

void printResult(const char* name, double cpuMs, double gpuMs)
{
    std::cout << name
              << ": CPU " << cpuMs << " ms, Metal " << gpuMs
              << " ms, speedup " << cpuMs / gpuMs << "x\n";
}

} // namespace

int main()
{
    @autoreleasepool {
        constexpr int width = 1920;
        constexpr int height = 1080;
        constexpr int pixelCount = width * height;
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (!device || !queue) {
            std::cerr << "Metal is unavailable\n";
            return 1;
        }

        id<MTLBuffer> sourceBuffer = [device
            newBufferWithLength:pixelCount * sizeof(Rgba)
            options:MTLResourceStorageModeShared];
        id<MTLBuffer> destinationBuffer = [device
            newBufferWithLength:pixelCount * sizeof(Rgba)
            options:MTLResourceStorageModeShared];
        auto* source = static_cast<Rgba*>(sourceBuffer.contents);
        auto* destination = static_cast<Rgba*>(destinationBuffer.contents);
        std::vector<Rgba> cpuOutput(pixelCount);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const bool bright = ((x / 160) + (y / 120)) % 5 == 0;
                source[y * width + x] = Rgba{
                    0.03f + 0.78f * static_cast<float>(x) / width +
                        (bright ? 0.12f : 0.0f),
                    0.04f + 0.72f * static_cast<float>(y) / height +
                        (bright ? 0.10f : 0.0f),
                    0.06f + 0.46f * static_cast<float>((x + y) % width) / width,
                    1.0f,
                };
            }
        }

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

        const buckswood::Controls photoControls{
            0.65f,
            0.55f,
            0.70f,
            0.55f,
            0.30f,
            0.35f,
            0.75f,
            0.35f,
            0.45f,
            1.0f,
            1.0f,
        };
        const buckswood::FrameInfo photoFrame{width, height, 17};
        buckswood::runPhotorealizerMetal(
            queue,
            sourceImage,
            destinationImage,
            window,
            photoFrame,
            photoControls,
            false,
            true);
        const double photoCpuMs = milliseconds([&] {
            parallelRows(height, [&](int first, int last) {
                for (int y = first; y < last; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const Rgba& input = source[y * width + x];
                        const buckswood::Pixel output =
                            buckswood::PhotorealizerCore::processPixel(
                                buckswood::Pixel{input.r, input.g, input.b, input.a},
                                x,
                                y,
                                photoFrame,
                                photoControls);
                        cpuOutput[y * width + x] =
                            Rgba{output.r, output.g, output.b, output.a};
                    }
                }
            });
        }, 3);
        const double photoGpuMs = milliseconds([&] {
            buckswood::runPhotorealizerMetal(
                queue,
                sourceImage,
                destinationImage,
                window,
                photoFrame,
                photoControls,
                false,
                true);
        }, 8);
        printResult("Photorealizer 1080p", photoCpuMs, photoGpuMs);

        const buckswood_lens::Controls lensControls{
            18,
            0.75f,
            0.08f,
            0.32f,
            0.24f,
            0.30f,
            0.36f,
            0.72f,
            0.18f,
            0.12f,
            0.06f,
            0.14f,
            1.20f,
            0.90f,
            0.82f,
        };
        const buckswood_lens::FrameInfo lensFrame{width, height, 17};
        const auto prepared =
            buckswood_lens::LensPhysicsCore::prepare(lensFrame, lensControls);
        const BufferSampler sampler(source, width, height);
        buckswood_lens::runLensPhysicsMetal(
            queue,
            sourceImage,
            destinationImage,
            window,
            prepared,
            false,
            true);
        const double lensCpuMs = milliseconds([&] {
            parallelRows(height, [&](int first, int last) {
                for (int y = first; y < last; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const buckswood_lens::Pixel output =
                            buckswood_lens::LensPhysicsCore::processPixel(
                                sampler,
                                x,
                                y,
                                prepared);
                        cpuOutput[y * width + x] =
                            Rgba{output.r, output.g, output.b, output.a};
                    }
                }
            });
        }, 1);
        const double lensGpuMs = milliseconds([&] {
            buckswood_lens::runLensPhysicsMetal(
                queue,
                sourceImage,
                destinationImage,
                window,
                prepared,
                false,
                true);
        }, 4);
        printResult("Lens Physics 1080p", lensCpuMs, lensGpuMs);

        const Rgba& sample = destination[pixelCount / 2];
        std::cout << "Output checksum sample: "
                  << sample.r + sample.g + sample.b + sample.a << "\n";

        [destinationBuffer release];
        [sourceBuffer release];
        [queue release];
        [device release];
    }
    return 0;
}
