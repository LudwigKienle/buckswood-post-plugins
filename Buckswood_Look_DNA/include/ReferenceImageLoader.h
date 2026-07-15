#pragma once

#include "LookDNACore.h"

#include <memory>
#include <string>
#include <vector>

namespace buckswood_lookdna {

class ReferenceImage final : public Sampler {
public:
    ReferenceImage(int width, int height, std::vector<Pixel> pixels);

    Pixel sample(float x, float y) const override;
    Bounds bounds() const override;
    int width() const;
    int height() const;

private:
    Pixel read(int x, int y) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<Pixel> pixels_;
};

struct ReferenceAsset {
    std::shared_ptr<ReferenceImage> image;
    LookProfile profile;
    ProfileGrid spatialProfiles;
    std::string error;

    bool valid() const { return profile.valid; }
};

class ReferenceImageLoader {
public:
    static std::shared_ptr<const ReferenceAsset> loadCached(
        const std::string& path,
        int referenceColorSpace,
        int analysisQuality);

    static bool loadImage(
        const std::string& path,
        std::shared_ptr<ReferenceImage>& image,
        std::string& error);

    static void clearCache();
};

} // namespace buckswood_lookdna
