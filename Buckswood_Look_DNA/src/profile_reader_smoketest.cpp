#include <iostream>
#include <string>

#include "LookDNACore.h"

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: profile_reader_smoketest profile.bwlook\n";
        return 2;
    }

    buckswood_lookdna::LookProfile profile;
    std::string error;
    if (!buckswood_lookdna::LookDNACore::loadProfile(argv[1], profile, &error)) {
        std::cerr << "Could not load companion profile: " << error << '\n';
        return 1;
    }
    if (!profile.valid || profile.sampleCount < 16 ||
        profile.lumaQuantiles.front() > profile.lumaQuantiles.back()) {
        std::cerr << "Companion profile has invalid statistics\n";
        return 1;
    }

    std::cout << "C++ loaded Python BWLOOK profile\n";
    return 0;
}
