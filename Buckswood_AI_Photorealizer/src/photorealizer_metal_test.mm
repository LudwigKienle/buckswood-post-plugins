#import <Metal/Metal.h>

#include "PhotorealizerCore.h"
#include "PhotorealizerMetal.h"

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

float maxDifference(const Rgba& actual, const buckswood::Pixel& expected)
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

        constexpr int width = 96;
        constexpr int height = 64;
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
                source[y * width + x] = Rgba{
                    0.04f + 0.91f * static_cast<float>(x) / static_cast<float>(width - 1),
                    0.03f + 0.88f * static_cast<float>(y) / static_cast<float>(height - 1),
                    0.06f + 0.55f * static_cast<float>((x * 7 + y * 3) % width) /
                        static_cast<float>(width - 1),
                    1.0f,
                };
            }
        }

        const buckswood::Controls controls{
            0.65f,
            0.55f,
            0.70f,
            0.55f,
            0.30f,
            0.35f,
            0.75f,
            0.35f,
            0.45f,
            1.00f,
            7.00f,
        };
        const buckswood::FrameInfo frame{width, height, 11};
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

        if (!buckswood::runPhotorealizerMetal(
                queue,
                sourceImage,
                destinationImage,
                window,
                frame,
                controls,
                false,
                true)) {
            std::cerr << "Metal render failed\n";
            return 1;
        }

        float largestDifference = 0.0f;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const Rgba& input = source[y * width + x];
                const buckswood::Pixel expected = buckswood::PhotorealizerCore::processPixel(
                    buckswood::Pixel{input.r, input.g, input.b, input.a},
                    x,
                    y,
                    frame,
                    controls);
                largestDifference = std::max(
                    largestDifference,
                    maxDifference(destination[y * width + x], expected));
            }
        }

        std::cout << "Photorealizer Metal max difference: "
                  << largestDifference << "\n";
        if (!std::isfinite(largestDifference) || largestDifference > 0.000005f) {
            std::cerr << "Photorealizer Metal quality regression\n";
            return 1;
        }

        std::fill(source, source + pixelCount, Rgba{0.0f, 0.0f, 0.0f, 1.0f});
        std::fill(destination, destination + pixelCount, Rgba{1.0f, 1.0f, 1.0f, 1.0f});
        if (!buckswood::runPhotorealizerMetal(
                queue,
                sourceImage,
                destinationImage,
                window,
                frame,
                controls,
                true,
                true)) {
            std::cerr << "Metal adjustment guard render failed\n";
            return 1;
        }
        for (int i = 0; i < pixelCount; ++i) {
            if (destination[i].r != 0.0f ||
                destination[i].g != 0.0f ||
                destination[i].b != 0.0f ||
                destination[i].a != 0.0f) {
                std::cerr << "Metal adjustment guard regression\n";
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
