#include "OpticsAssetLibrary.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

namespace buckswood_optics {
namespace {

std::mutex gCacheMutex;
std::unordered_map<std::string, std::shared_ptr<const AssetTexture>> gCache;

std::string joinPath(const std::string& root, const std::string& child)
{
    if (root.empty()) {
        return {};
    }
    std::string expandedRoot = root;
    if (expandedRoot.size() >= 2 && expandedRoot[0] == '~' &&
        (expandedRoot[1] == '/' || expandedRoot[1] == '\\')) {
        const char* home = std::getenv("HOME");
        if (home && *home) {
            expandedRoot = std::string(home) + expandedRoot.substr(1);
        }
    }
    const char last = expandedRoot.back();
    return expandedRoot + (last == '/' || last == '\\' ? "" : "/") + child;
}

std::shared_ptr<const AssetTexture> loadTexture(const std::string& path)
{
    if (path.empty()) {
        return {};
    }

    {
        std::lock_guard<std::mutex> lock(gCacheMutex);
        const auto it = gCache.find(path);
        if (it != gCache.end()) {
            return it->second;
        }
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 3);
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels) {
            stbi_image_free(pixels);
        }
        return {};
    }

    auto texture = std::make_shared<AssetTexture>();
    texture->width = width;
    texture->height = height;
    texture->luminance.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (std::size_t i = 0; i < texture->luminance.size(); ++i) {
        const float r = pixels[i * 3 + 0] / 255.0f;
        const float g = pixels[i * 3 + 1] / 255.0f;
        const float b = pixels[i * 3 + 2] / 255.0f;
        texture->luminance[i] = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }
    stbi_image_free(pixels);

    {
        std::lock_guard<std::mutex> lock(gCacheMutex);
        gCache[path] = texture;
    }
    return texture;
}

} // namespace

std::string OpticsAssetLibrary::aperturePath(const std::string& assetRoot, int index)
{
    if (index <= 0) {
        return {};
    }
    char filename[64] = {};
    std::snprintf(filename, sizeof(filename), "apertures/aperture%03d.jpg", std::min(index, 157));
    return joinPath(assetRoot, filename);
}

std::string OpticsAssetLibrary::dirtPath(const std::string& assetRoot, int index)
{
    static constexpr const char* kFiles[] = {
        "",
        "dirt_textures/acg_fingerprints_01.png",
        "dirt_textures/acg_fingerprints_02.png",
        "dirt_textures/acg_fingerprints_03.png",
        "dirt_textures/acg_imperfections_01.png",
        "dirt_textures/acg_imperfections_02.png",
        "dirt_textures/acg_imperfections_03.png",
        "dirt_textures/acg_imperfections_04.png",
        "dirt_textures/acg_imperfections_05.png",
    };
    const int safeIndex = std::min(8, std::max(0, index));
    return safeIndex == 0 ? std::string{} : joinPath(assetRoot, kFiles[safeIndex]);
}

LoadedAssets OpticsAssetLibrary::load(
    const std::string& assetRoot,
    int apertureIndex,
    int dirtIndex)
{
    return LoadedAssets{
        loadTexture(aperturePath(assetRoot, apertureIndex)),
        loadTexture(dirtPath(assetRoot, dirtIndex)),
    };
}

} // namespace buckswood_optics
