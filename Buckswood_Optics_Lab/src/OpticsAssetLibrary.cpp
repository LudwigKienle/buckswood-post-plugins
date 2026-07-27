#include "OpticsAssetLibrary.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

namespace buckswood_optics {
namespace {

std::mutex gCacheMutex;
std::unordered_map<std::string, std::shared_ptr<const AssetTexture>> gCache;

float clamp01(float value)
{
    return std::min(1.0f, std::max(0.0f, value));
}

float smoothMask(float edge0, float edge1, float value)
{
    const float t = clamp01((value - edge0) / std::max(0.00001f, edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

std::uint32_t hash2d(int x, int y, int seed)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8da6b343u;
    h ^= static_cast<std::uint32_t>(y) * 0xd8163841u;
    h ^= static_cast<std::uint32_t>(seed) * 0xcb1ab31fu;
    h ^= h >> 13;
    h *= 0x85ebca6bu;
    h ^= h >> 16;
    return h;
}

float noise01(int x, int y, int seed)
{
    return static_cast<float>(hash2d(x, y, seed) & 0x00ffffffu) /
        16777215.0f;
}

std::shared_ptr<const AssetTexture> generateTexture(
    int width,
    int height,
    const std::function<float(float, float, int, int)>& generator)
{
    auto texture = std::make_shared<AssetTexture>();
    texture->width = width;
    texture->height = height;
    texture->luminance.resize(
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float u =
                (static_cast<float>(x) + 0.5f) /
                static_cast<float>(width);
            const float v =
                (static_cast<float>(y) + 0.5f) /
                static_cast<float>(height);
            texture->luminance[
                static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(width) +
                static_cast<std::size_t>(x)] =
                clamp01(generator(u, v, x, y));
        }
    }
    return texture;
}

float polygonAperture(float x, float y, int blades, float rotation)
{
    constexpr float kPi = 3.14159265358979323846f;
    const float angle = std::atan2(y, x) + rotation;
    const float radius = std::sqrt(x * x + y * y);
    const float sector = 2.0f * kPi / static_cast<float>(blades);
    const float local =
        std::fmod(angle + kPi + sector * 0.5f, sector) -
        sector * 0.5f;
    const float boundary =
        0.88f * std::cos(kPi / static_cast<float>(blades)) /
        std::max(0.0001f, std::cos(local));
    return 1.0f - smoothMask(boundary - 0.035f, boundary + 0.035f, radius);
}

std::shared_ptr<const AssetTexture> makeBuiltInGlass(int index)
{
    return generateTexture(
        64,
        64,
        [index](float u, float v, int, int) {
            constexpr float kPi = 3.14159265358979323846f;
            float x = (u - 0.5f) * 2.0f;
            float y = (v - 0.5f) * 2.0f;
            if (index == 4) {
                x *= 1.45f;
            } else if (index == 5) {
                x *= 1.85f;
            }
            if (index == 6) {
                x += 0.30f * (0.15f + std::fabs(y));
            }

            const float radius = std::sqrt(x * x + y * y);
            switch (index) {
            case 2:
                return polygonAperture(x, y, 6, kPi / 6.0f);
            case 3:
                return polygonAperture(x, y, 8, kPi / 8.0f);
            case 4:
            case 5:
            case 6:
                return 1.0f - smoothMask(0.82f, 0.92f, radius);
            case 7: {
                const float angle = std::atan2(y, x);
                const float scallop =
                    0.78f +
                    0.08f * std::sin(angle * 9.0f + radius * 5.0f);
                return 1.0f - smoothMask(scallop, scallop + 0.06f, radius);
            }
            case 1:
            default:
                return 1.0f - smoothMask(0.84f, 0.92f, radius);
            }
        });
}

std::shared_ptr<const AssetTexture> makeBuiltInDirt(int index)
{
    return generateTexture(
        96,
        96,
        [index](float u, float v, int x, int y) {
            const float fine = noise01(x, y, 100 + index * 17);
            const float coarse =
                noise01(x / 4, y / 4, 240 + index * 29);
            const float veryCoarse =
                noise01(x / 11, y / 11, 400 + index * 41);
            float mask = 0.0f;
            switch (index) {
            case 1:
                mask =
                    smoothMask(0.93f, 0.995f, fine) * 0.72f +
                    smoothMask(0.86f, 0.98f, coarse) * 0.22f;
                break;
            case 2:
                mask =
                    smoothMask(0.76f, 0.94f, coarse) * 0.76f +
                    smoothMask(0.97f, 1.0f, fine) * 0.30f;
                break;
            case 3:
                mask =
                    smoothMask(0.985f, 1.0f, fine) +
                    smoothMask(0.94f, 0.99f, veryCoarse) * 0.25f;
                break;
            case 4: {
                const float edge =
                    smoothMask(0.58f, 0.96f, std::fabs(u - 0.5f) * 2.0f) +
                    smoothMask(0.58f, 0.96f, std::fabs(v - 0.5f) * 2.0f);
                mask = edge * smoothMask(0.66f, 0.94f, coarse) * 0.72f;
                break;
            }
            case 5:
            default:
                mask =
                    smoothMask(0.58f, 0.90f, veryCoarse) *
                    (0.32f + 0.68f * coarse) * 0.78f;
                break;
            }
            return clamp01(mask);
        });
}

std::shared_ptr<const AssetTexture> makeBuiltInSmudge(int index)
{
    return generateTexture(
        96,
        96,
        [index](float u, float v, int x, int y) {
            constexpr float kPi = 3.14159265358979323846f;
            const float nx = u - 0.5f;
            const float ny = v - 0.5f;
            const float radius = std::sqrt(nx * nx + ny * ny);
            const float angle = std::atan2(ny, nx);
            const float breakup =
                0.72f + noise01(x / 3, y / 3, 700 + index * 31) * 0.28f;
            switch (index) {
            case 1: {
                const float ridge =
                    0.5f + 0.5f * std::sin(radius * 132.0f + angle * 2.2f);
                return smoothMask(0.64f, 0.94f, ridge) *
                    (1.0f - smoothMask(0.25f, 0.48f, radius)) *
                    breakup;
            }
            case 2: {
                const float ridge =
                    0.5f + 0.5f * std::sin(radius * 174.0f + angle * 3.6f);
                return smoothMask(0.48f, 0.88f, ridge) *
                    (1.0f - smoothMask(0.30f, 0.52f, radius)) *
                    breakup;
            }
            case 3: {
                const float arcRadius =
                    std::sqrt(
                        (nx + 0.22f) * (nx + 0.22f) +
                        (ny - 0.12f) * (ny - 0.12f));
                const float arc =
                    0.5f + 0.5f * std::sin(arcRadius * 56.0f);
                return smoothMask(0.70f, 0.98f, arc) *
                    (1.0f - smoothMask(0.34f, 0.68f, arcRadius)) *
                    breakup;
            }
            case 4:
            default: {
                const float diagonal = nx * 0.38f + ny;
                const float streak =
                    0.5f + 0.5f * std::sin(diagonal * kPi * 24.0f);
                const float envelope =
                    1.0f - smoothMask(0.22f, 0.70f, std::fabs(nx - ny * 0.18f));
                return smoothMask(0.68f, 0.96f, streak) *
                    envelope * breakup * 0.82f;
            }
            }
        });
}

std::shared_ptr<const AssetTexture> loadBuiltIn(
    const char* kind,
    int index)
{
    if (index <= 0) {
        return {};
    }
    const std::string key =
        std::string("builtin:") + kind + ":" + std::to_string(index);
    {
        std::lock_guard<std::mutex> lock(gCacheMutex);
        const auto it = gCache.find(key);
        if (it != gCache.end()) {
            return it->second;
        }
    }

    std::shared_ptr<const AssetTexture> texture;
    if (std::string(kind) == "glass") {
        texture = makeBuiltInGlass(std::min(index, 7));
    } else if (std::string(kind) == "dirt") {
        texture = makeBuiltInDirt(std::min(index, 5));
    } else {
        texture = makeBuiltInSmudge(std::min(index, 4));
    }

    std::lock_guard<std::mutex> lock(gCacheMutex);
    const auto inserted = gCache.emplace(key, texture);
    return inserted.first->second;
}

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
    int dirtIndex,
    int builtInGlass,
    int builtInDirt,
    int builtInSmudge)
{
    return LoadedAssets{
        builtInGlass > 0
            ? loadBuiltIn("glass", builtInGlass)
            : loadTexture(aperturePath(assetRoot, apertureIndex)),
        builtInDirt > 0
            ? loadBuiltIn("dirt", builtInDirt)
            : loadTexture(dirtPath(assetRoot, dirtIndex)),
        loadBuiltIn("smudge", builtInSmudge),
    };
}

std::string OpticsAssetLibrary::defaultAssetRoot()
{
#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    return appData && *appData
        ? joinPath(appData, "Buckswood/OpticsLab/GlassAssets")
        : std::string{};
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return home && *home
        ? joinPath(home, "Library/Application Support/Buckswood/OpticsLab/GlassAssets")
        : std::string{};
#else
    const char* home = std::getenv("HOME");
    return home && *home
        ? joinPath(home, ".local/share/Buckswood/OpticsLab/GlassAssets")
        : std::string{};
#endif
}

} // namespace buckswood_optics
