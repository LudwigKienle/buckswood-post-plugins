#include "ofxCore.h"

#include <dlfcn.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

using PluginCountFunction = int (*)();
using PluginGetFunction = OfxPlugin* (*)(int);

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
    require(argc == 2, "expected path to OFX bundle binary");
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        std::cerr << "FAILED: dlopen: " << dlerror() << '\n';
        return 1;
    }

    auto getCount = reinterpret_cast<PluginCountFunction>(dlsym(library, "OfxGetNumberOfPlugins"));
    auto getPlugin = reinterpret_cast<PluginGetFunction>(dlsym(library, "OfxGetPlugin"));
    require(getCount != nullptr, "OfxGetNumberOfPlugins export");
    require(getPlugin != nullptr, "OfxGetPlugin export");
    require(getCount() == 3, "bundle exposes exactly three effects");

    const char* expected[] = {
        "com.buckswood.frame.director",
        "com.buckswood.radiance.recover",
        "com.buckswood.temporal.integrity",
    };
    for (int index = 0; index < 3; ++index) {
        OfxPlugin* plugin = getPlugin(index);
        require(plugin != nullptr, "plugin descriptor exists");
        require(plugin->pluginIdentifier != nullptr, "plugin identifier exists");
        require(std::strcmp(plugin->pluginIdentifier, expected[index]) == 0, "plugin identifier matches");
        require(plugin->pluginVersionMajor == 2, "plugin exposes v2 major version");
        require(plugin->mainEntry != nullptr, "plugin main entry exists");
    }
    require(getPlugin(3) == nullptr, "out-of-range descriptor is null");

    dlclose(library);
    std::cout << "Buckswood Cinematic Tools descriptor tests passed\n";
    return 0;
}
