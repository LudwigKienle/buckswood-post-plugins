#import <Metal/Metal.h>

#include "LensPhysicsCore.h"
#include "LensPhysicsMetal.h"

#include <algorithm>
#include <cmath>
#include <iostream>

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

float maxDifference(const Rgba& actual, const buckswood_lens::Pixel& expected)
{
    return std::max({
        std::fabs(actual.r - expected.r),
        std::fabs(actual.g - expected.g),
        std::fabs(actual.b - expected.b),
        std::fabs(actual.a - expected.a),
    });
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
        auto* destination = static_cast<Rgba*>(destinationBuffer.contents);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const bool hardEdge = x > width * 2 / 3;
                source[y * width + x] = Rgba{
                    hardEdge ? 0.88f : 0.05f + 0.45f * static_cast<float>(x) / width,
                    hardEdge ? 0.76f : 0.06f + 0.38f * static_cast<float>(y) / height,
                    hardEdge ? 0.62f : 0.08f + 0.22f * static_cast<float>(x + y) /
                        static_cast<float>(width + height),
                    1.0f,
                };
            }
        }

        const buckswood_lens::Controls controls{
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
        const buckswood_lens::FrameInfo frame{width, height, 4};
        const auto prepared =
            buckswood_lens::LensPhysicsCore::prepare(frame, controls);
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

        if (!buckswood_lens::runLensPhysicsMetal(
                queue,
                sourceImage,
                destinationImage,
                window,
                prepared,
                false,
                true)) {
            std::cerr << "Lens Physics Metal render failed\n";
            return 1;
        }

        const BufferSampler sampler(source, width, height);
        float largestDifference = 0.0f;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const buckswood_lens::Pixel expected =
                    buckswood_lens::LensPhysicsCore::processPixel(
                        sampler,
                        x,
                        y,
                        prepared);
                largestDifference = std::max(
                    largestDifference,
                    maxDifference(destination[y * width + x], expected));
            }
        }

        std::cout << "Lens Physics Metal max difference: "
                  << largestDifference << "\n";
        if (!std::isfinite(largestDifference) || largestDifference > 0.00002f) {
            std::cerr << "Lens Physics Metal quality regression\n";
            return 1;
        }

        std::fill(source, source + pixelCount, Rgba{0.0f, 0.0f, 0.0f, 1.0f});
        std::fill(destination, destination + pixelCount, Rgba{1.0f, 1.0f, 1.0f, 1.0f});
        if (!buckswood_lens::runLensPhysicsMetal(
                queue,
                sourceImage,
                destinationImage,
                window,
                prepared,
                true,
                true)) {
            std::cerr << "Lens Physics adjustment guard render failed\n";
            return 1;
        }
        for (int i = 0; i < pixelCount; ++i) {
            if (destination[i].r != 0.0f ||
                destination[i].g != 0.0f ||
                destination[i].b != 0.0f ||
                destination[i].a != 0.0f) {
                std::cerr << "Lens Physics adjustment guard regression\n";
                return 1;
            }
        }

        [destinationBuffer release];
        [sourceBuffer release];
        [queue release];
        [device release];
    }
    return 0;
}
