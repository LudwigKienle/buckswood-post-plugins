#include "ReferenceImageLoader.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void writeTestBmp(const char* path)
{
    const int width = 96;
    const int height = 54;
    const int rowBytes = width * 3;
    const int imageBytes = rowBytes * height;
    const int fileBytes = 54 + imageBytes;
    std::vector<unsigned char> bytes(static_cast<std::size_t>(fileBytes), 0u);
    bytes[0] = 'B';
    bytes[1] = 'M';
    bytes[2] = static_cast<unsigned char>(fileBytes);
    bytes[3] = static_cast<unsigned char>(fileBytes >> 8);
    bytes[10] = 54;
    bytes[14] = 40;
    bytes[18] = width;
    bytes[22] = height;
    bytes[26] = 1;
    bytes[28] = 24;
    bytes[34] = static_cast<unsigned char>(imageBytes);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = 54u + static_cast<std::size_t>(y * rowBytes + x * 3);
            bytes[index] = static_cast<unsigned char>(30 + x * 2);
            bytes[index + 1u] = static_cast<unsigned char>(55 + y * 3);
            bytes[index + 2u] = static_cast<unsigned char>(130 + x);
        }
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

} // namespace

int main()
{
    const char* path = "/tmp/BuckswoodLookDNA_loader_test.bmp";
    writeTestBmp(path);
    std::shared_ptr<buckswood_lookdna::ReferenceImage> image;
    std::string error;
    require(
        buckswood_lookdna::ReferenceImageLoader::loadImage(path, image, error),
        error.empty() ? "load BMP reference" : error.c_str());
    require(image && image->width() == 96 && image->height() == 54, "reference dimensions");
    const auto pixel = image->sample(2.0f, 1.0f);
    require(pixel.a > 0.99f && pixel.r > pixel.b, "reference pixel decode");
    const auto asset = buckswood_lookdna::ReferenceImageLoader::loadCached(
        path,
        buckswood_lookdna::SRGBDisplay,
        0);
    require(asset && asset->valid(), "cached reference analysis");
    require(asset->spatialProfiles.valid, "cached 3x3 reference analysis");
    std::remove(path);
    buckswood_lookdna::ReferenceImageLoader::clearCache();
    std::cout << "Buckswood Look DNA reference loader tests passed\n";
    return 0;
}
