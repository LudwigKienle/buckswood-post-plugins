#pragma once

#include <memory>
#include <string>
#include <vector>

#include "OpticsLabCore.h"

namespace buckswood_optics {

struct AssetTexture {
    int width = 0;
    int height = 0;
    std::vector<float> luminance;

    TextureView view() const
    {
        return TextureView{
            luminance.empty() ? nullptr : luminance.data(),
            width,
            height,
        };
    }
};

struct LoadedAssets {
    std::shared_ptr<const AssetTexture> aperture;
    std::shared_ptr<const AssetTexture> dirt;

    AssetViews views() const
    {
        return AssetViews{
            aperture ? aperture->view() : TextureView{},
            dirt ? dirt->view() : TextureView{},
        };
    }
};

class OpticsAssetLibrary {
public:
    static LoadedAssets load(
        const std::string& assetRoot,
        int apertureIndex,
        int dirtIndex);

    static std::string aperturePath(const std::string& assetRoot, int index);
    static std::string dirtPath(const std::string& assetRoot, int index);
};

} // namespace buckswood_optics
