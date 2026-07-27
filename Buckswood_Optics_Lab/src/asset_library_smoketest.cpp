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
    const auto builtIns =
        buckswood_optics::OpticsAssetLibrary::load("", 0, 0, 2, 3, 1);
    require(builtIns.aperture != nullptr, "built-in glass loads");
    require(builtIns.dirt != nullptr, "built-in dirt loads");
    require(builtIns.smudge != nullptr, "built-in smudge loads");
    require(
        builtIns.aperture->width > 0 && builtIns.aperture->height > 0,
        "built-in glass has dimensions");
    require(
        builtIns.dirt->width > 0 && builtIns.dirt->height > 0,
        "built-in dirt has dimensions");
    require(
        builtIns.smudge->width > 0 && builtIns.smudge->height > 0,
        "built-in smudge has dimensions");
    require(builtIns.views().aperture.valid(), "built-in aperture view is valid");
    require(builtIns.views().dirt.valid(), "built-in dirt view is valid");
    require(builtIns.views().smudge.valid(), "built-in smudge view is valid");

    const auto cached =
        buckswood_optics::OpticsAssetLibrary::load("", 0, 0, 2, 3, 1);
    require(
        cached.aperture == builtIns.aperture &&
            cached.dirt == builtIns.dirt &&
            cached.smudge == builtIns.smudge,
        "built-in textures are cached");

    if (argc == 2) {
        const auto licensed =
            buckswood_optics::OpticsAssetLibrary::load(argv[1], 1, 1);
        require(licensed.aperture != nullptr, "aperture001.jpg loads");
        require(licensed.dirt != nullptr, "first CC0 dirt PNG loads");
        require(
            licensed.aperture->width > 0 && licensed.aperture->height > 0,
            "licensed aperture has dimensions");
        require(
            licensed.dirt->width > 0 && licensed.dirt->height > 0,
            "licensed dirt has dimensions");
    } else {
        require(argc == 1, "expected zero or one asset root argument");
    }

    std::cout << "Buckswood Optics Lab asset tests passed\n";
    return 0;
}
