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
    require(argc == 2, "expected path to OFX binary");
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        std::cerr << "FAILED: dlopen: " << dlerror() << '\n';
        return 1;
    }
    auto getCount = reinterpret_cast<PluginCountFunction>(
        dlsym(library, "OfxGetNumberOfPlugins"));
    auto getPlugin = reinterpret_cast<PluginGetFunction>(
        dlsym(library, "OfxGetPlugin"));
    require(getCount != nullptr, "OfxGetNumberOfPlugins export");
    require(getPlugin != nullptr, "OfxGetPlugin export");
    require(getCount() == 1, "bundle exposes one effect");
    OfxPlugin* plugin = getPlugin(0);
    require(plugin != nullptr, "plugin descriptor exists");
    require(
        std::strcmp(plugin->pluginIdentifier, "com.buckswood.deband") == 0,
        "plugin identifier matches");
    require(plugin->pluginVersionMajor == 1, "plugin major version matches");
    require(plugin->pluginVersionMinor == 0, "plugin minor version matches");
    require(plugin->mainEntry != nullptr, "plugin entry point exists");
    require(getPlugin(1) == nullptr, "out-of-range descriptor is null");
    dlclose(library);
    std::cout << "Buckswood Deband descriptor tests passed\n";
    return 0;
}
