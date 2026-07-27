#include "OpticsAssetLibrary.h"

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main(int argc, char** argv)
{
    require(argc == 2, "expected path to a licensed Glass asset folder");
    const auto assets = buckswood_optics::OpticsAssetLibrary::load(argv[1], 1, 1);
    require(assets.aperture != nullptr, "aperture001.jpg loads");
    require(assets.dirt != nullptr, "first CC0 dirt PNG loads");
    require(assets.aperture->width > 0 && assets.aperture->height > 0, "aperture has dimensions");
    require(assets.dirt->width > 0 && assets.dirt->height > 0, "dirt has dimensions");
    require(assets.views().aperture.valid(), "aperture view is valid");
    require(assets.views().dirt.valid(), "dirt view is valid");
    std::cout << "Buckswood Optics Lab licensed asset tests passed\n";
    return 0;
}
