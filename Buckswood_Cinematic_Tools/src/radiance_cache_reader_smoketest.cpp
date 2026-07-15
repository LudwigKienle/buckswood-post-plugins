#include "BuckswoodCinematicToolsOFX.cpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

std::uint16_t floatToHalf(float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const std::uint32_t sign = (bits >> 16u) & 0x8000u;
    const int exponent = static_cast<int>((bits >> 23u) & 0xffu) - 127 + 15;
    const std::uint32_t mantissa = bits & 0x007fffffu;
    if (exponent <= 0) {
        return static_cast<std::uint16_t>(sign);
    }
    if (exponent >= 31) {
        return static_cast<std::uint16_t>(sign | 0x7c00u);
    }
    return static_cast<std::uint16_t>(
        sign |
        (static_cast<std::uint32_t>(exponent) << 10u) |
        (mantissa >> 13u));
}

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    const auto directory = std::filesystem::temp_directory_path() / "buckswood-radiance-cache-reader-test";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "frame_00000007.bwrcache";

    RadianceCacheHeader header{};
    const char magic[8] = {'B', 'W', 'R', 'C', 'V', '2', '\0', '\0'};
    std::memcpy(header.magic, magic, sizeof(magic));
    header.version = 2;
    header.width = 4;
    header.height = 2;
    header.frameIndex = 7;
    header.flags = 0;

    std::vector<std::uint16_t> payload(4u * 2u * 4u);
    for (std::size_t pixel = 0; pixel < 8; ++pixel) {
        payload[pixel * 4u] = floatToHalf(0.25f + static_cast<float>(pixel) * 0.1f);
        payload[pixel * 4u + 1] = floatToHalf(0.5f);
        payload[pixel * 4u + 2] = floatToHalf(2.0f);
        payload[pixel * 4u + 3] = floatToHalf(0.75f);
    }

    {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(&header), sizeof(header));
        output.write(
            reinterpret_cast<const char*>(payload.data()),
            static_cast<std::streamsize>(payload.size() * sizeof(std::uint16_t)));
    }

    RadianceCacheFrame cache;
    const OfxRectI bounds{0, 0, 4, 2};
    const OfxRectI renderWindow{1, 0, 4, 2};
    require(cache.load(directory.string(), 7, bounds, renderWindow), "cache frame loads");
    require(cache.valid(), "cache reports valid");

    buckswood_cinematic::Pixel pixel{};
    float confidence = 0.0f;
    require(cache.sample(2, 1, pixel, confidence), "cached pixel is available");
    require(std::fabs(pixel.r - 0.85f) < 0.01f, "half-float red value decodes");
    require(std::fabs(pixel.g - 0.5f) < 0.01f, "half-float green value decodes");
    require(std::fabs(pixel.b - 2.0f) < 0.01f, "HDR value above one survives");
    require(std::fabs(confidence - 0.75f) < 0.01f, "confidence decodes");
    require(!cache.sample(0, 0, pixel, confidence), "pixels outside render window are unavailable");

    std::filesystem::remove_all(directory);
    std::cout << "Buckswood Radiance Cache reader tests passed\n";
    return 0;
}
