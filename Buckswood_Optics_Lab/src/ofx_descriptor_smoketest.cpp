#include "ofxCore.h"
#include "ofxGPURender.h"
#include "ofxImageEffect.h"
#include "ofxMultiThread.h"
#include "ofxParam.h"
#include "ofxProperty.h"

#include <dlfcn.h>

#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using PluginCountFunction = int (*)();
using PluginGetFunction = OfxPlugin* (*)(int);

struct PropertyBag {
    std::unordered_map<std::string, std::vector<std::string>> strings;
    std::unordered_map<std::string, std::vector<int>> ints;
    std::unordered_map<std::string, std::vector<double>> doubles;
};

struct ParamRecord {
    std::string type;
    std::string name;
    PropertyBag properties;
};

PropertyBag gEffectProperties;
PropertyBag gParamSetProperties;
std::deque<PropertyBag> gClipProperties;
std::deque<ParamRecord> gParams;
OfxPropertySuiteV1 gPropertySuite{};
OfxParameterSuiteV1 gParameterSuite{};
OfxImageEffectSuiteV1 gEffectSuite{};
OfxMultiThreadSuiteV1 gThreadSuite{};

template <typename T>
void setIndexed(std::vector<T>& values, int index, T value)
{
    if (index >= static_cast<int>(values.size())) {
        values.resize(static_cast<std::size_t>(index + 1));
    }
    values[static_cast<std::size_t>(index)] = std::move(value);
}

OfxStatus setString(
    OfxPropertySetHandle handle,
    const char* property,
    int index,
    const char* value)
{
    auto* bag = reinterpret_cast<PropertyBag*>(handle);
    if (!bag || !property || index < 0) {
        return kOfxStatErrBadHandle;
    }
    setIndexed(
        bag->strings[property],
        index,
        std::string(value ? value : ""));
    return kOfxStatOK;
}

OfxStatus setInt(
    OfxPropertySetHandle handle,
    const char* property,
    int index,
    int value)
{
    auto* bag = reinterpret_cast<PropertyBag*>(handle);
    if (!bag || !property || index < 0) {
        return kOfxStatErrBadHandle;
    }
    setIndexed(bag->ints[property], index, value);
    return kOfxStatOK;
}

OfxStatus setDouble(
    OfxPropertySetHandle handle,
    const char* property,
    int index,
    double value)
{
    auto* bag = reinterpret_cast<PropertyBag*>(handle);
    if (!bag || !property || index < 0) {
        return kOfxStatErrBadHandle;
    }
    setIndexed(bag->doubles[property], index, value);
    return kOfxStatOK;
}

OfxStatus getEffectProperties(
    OfxImageEffectHandle,
    OfxPropertySetHandle* handle)
{
    *handle = reinterpret_cast<OfxPropertySetHandle>(
        &gEffectProperties);
    return kOfxStatOK;
}

OfxStatus getParamSet(
    OfxImageEffectHandle,
    OfxParamSetHandle* handle)
{
    *handle = reinterpret_cast<OfxParamSetHandle>(
        &gParamSetProperties);
    return kOfxStatOK;
}

OfxStatus defineClip(
    OfxImageEffectHandle,
    const char*,
    OfxPropertySetHandle* handle)
{
    gClipProperties.emplace_back();
    *handle = reinterpret_cast<OfxPropertySetHandle>(
        &gClipProperties.back());
    return kOfxStatOK;
}

OfxStatus defineParam(
    OfxParamSetHandle,
    const char* type,
    const char* name,
    OfxPropertySetHandle* handle)
{
    gParams.push_back(ParamRecord{
        type ? type : "",
        name ? name : "",
        PropertyBag{},
    });
    if (handle) {
        *handle = reinterpret_cast<OfxPropertySetHandle>(
            &gParams.back().properties);
    }
    return kOfxStatOK;
}

const void* fetchSuite(
    OfxPropertySetHandle,
    const char* name,
    int version)
{
    if (version != 1 || !name) {
        return nullptr;
    }
    if (std::strcmp(name, kOfxImageEffectSuite) == 0) {
        return &gEffectSuite;
    }
    if (std::strcmp(name, kOfxPropertySuite) == 0) {
        return &gPropertySuite;
    }
    if (std::strcmp(name, kOfxParameterSuite) == 0) {
        return &gParameterSuite;
    }
    if (std::strcmp(name, kOfxMultiThreadSuite) == 0) {
        return &gThreadSuite;
    }
    return nullptr;
}

void configureSuites()
{
    gPropertySuite.propSetString = setString;
    gPropertySuite.propSetInt = setInt;
    gPropertySuite.propSetDouble = setDouble;
    gParameterSuite.paramDefine = defineParam;
    gEffectSuite.getPropertySet = getEffectProperties;
    gEffectSuite.getParamSet = getParamSet;
    gEffectSuite.clipDefine = defineClip;
}

const ParamRecord* findParam(const char* name)
{
    for (const ParamRecord& param : gParams) {
        if (param.name == name) {
            return &param;
        }
    }
    return nullptr;
}

std::string stringProperty(
    const PropertyBag& bag,
    const char* property,
    int index = 0)
{
    const auto found = bag.strings.find(property);
    return found != bag.strings.end() &&
            index >= 0 &&
            index < static_cast<int>(found->second.size())
        ? found->second[static_cast<std::size_t>(index)]
        : std::string{};
}

int intProperty(
    const PropertyBag& bag,
    const char* property,
    int index = 0)
{
    const auto found = bag.ints.find(property);
    return found != bag.ints.end() &&
            index >= 0 &&
            index < static_cast<int>(found->second.size())
        ? found->second[static_cast<std::size_t>(index)]
        : 0;
}

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

    auto getCount = reinterpret_cast<PluginCountFunction>(
        dlsym(library, "OfxGetNumberOfPlugins"));
    auto getPlugin = reinterpret_cast<PluginGetFunction>(
        dlsym(library, "OfxGetPlugin"));
    require(getCount != nullptr, "OfxGetNumberOfPlugins export");
    require(getPlugin != nullptr, "OfxGetPlugin export");
    require(getCount() == 1, "bundle exposes exactly one effect");

    OfxPlugin* plugin = getPlugin(0);
    require(plugin != nullptr, "plugin descriptor exists");
    require(plugin->pluginIdentifier != nullptr, "plugin identifier exists");
    require(
        std::strcmp(plugin->pluginIdentifier, "com.buckswood.optics.lab") == 0,
        "plugin identifier matches");
    require(plugin->pluginVersionMajor == 1, "plugin exposes v1 major version");
    require(plugin->pluginVersionMinor == 3, "plugin exposes v1.3 minor version");
    require(plugin->mainEntry != nullptr, "plugin main entry exists");
    require(getPlugin(1) == nullptr, "out-of-range descriptor is null");

    configureSuites();
    OfxHost host{
        nullptr,
        fetchSuite,
    };
    plugin->setHost(&host);
    require(
        plugin->mainEntry(
            kOfxActionLoad,
            nullptr,
            nullptr,
            nullptr) == kOfxStatOK,
        "plugin loads against required host suites");
    auto effect = reinterpret_cast<OfxImageEffectHandle>(
        &gEffectProperties);
    require(
        plugin->mainEntry(
            kOfxActionDescribe,
            effect,
            nullptr,
            nullptr) == kOfxStatOK,
        "plugin describe action succeeds");
    require(
        plugin->mainEntry(
            kOfxImageEffectActionDescribeInContext,
            effect,
            nullptr,
            nullptr) == kOfxStatOK,
        "plugin context describe action succeeds");

    const ParamRecord* lens = findParam("preset");
    const ParamRecord* glass = findParam("glassAsset");
    const ParamRecord* dirt = findParam("dirtAsset");
    const ParamRecord* smudge = findParam("smudgeAsset");
    require(lens && lens->type == kOfxParamTypeChoice, "Lens is a choice selector");
    require(glass && glass->type == kOfxParamTypeChoice, "Glass is a choice selector");
    require(dirt && dirt->type == kOfxParamTypeChoice, "Dirt is a choice selector");
    require(smudge && smudge->type == kOfxParamTypeChoice, "Smudge is a choice selector");
    require(
        lens->properties.strings.at(kOfxParamPropChoiceOption).size() == 21,
        "Lens exposes the compatible v1.2 set plus twelve v1.3 recipes");
    require(
        glass->properties.strings.at(kOfxParamPropChoiceOption).size() == 8,
        "Glass exposes seven built-ins plus compatibility Off");
    require(
        dirt->properties.strings.at(kOfxParamPropChoiceOption).size() == 6,
        "Dirt exposes five built-ins plus compatibility Off");
    require(
        smudge->properties.strings.at(kOfxParamPropChoiceOption).size() == 5,
        "Smudge exposes four built-ins plus Off");
    require(
        stringProperty(
            glass->properties,
            kOfxParamPropParent) == "glassGroup",
        "Glass is grouped under Defocus and Bokeh");
    require(
        stringProperty(
            dirt->properties,
            kOfxParamPropParent) == "surfaceGroup" &&
        stringProperty(
            smudge->properties,
            kOfxParamPropParent) == "surfaceGroup",
        "Dirt and Smudge share their finishing group");

    const char* groups[] = {
        "lensGroup",
        "distortionGroup",
        "chromaticGroup",
        "glassGroup",
        "lightGroup",
        "vignetteGroup",
        "surfaceGroup",
        "sensorGroup",
        "workflowGroup",
    };
    for (const char* groupName : groups) {
        const ParamRecord* group = findParam(groupName);
        require(
            group && group->type == kOfxParamTypeGroup,
            "lens-anatomy group exists");
    }

    const ParamRecord* quality = findParam("quality");
    const ParamRecord* geometryEnabled = findParam("geometryEnabled");
    const ParamRecord* sensorEnabled = findParam("sensorEnabled");
    const ParamRecord* sceneUnits = findParam("sceneUnits");
    const ParamRecord* irisBlades = findParam("irisBlades");
    const ParamRecord* starResponse = findParam("starFStopResponse");
    require(
        quality && quality->type == kOfxParamTypeChoice &&
        intProperty(quality->properties, kOfxParamPropDefault) == 1,
        "Full quality remains the compatibility default");
    require(
        geometryEnabled && sensorEnabled &&
        intProperty(geometryEnabled->properties, kOfxParamPropDefault) == 1 &&
        intProperty(sensorEnabled->properties, kOfxParamPropDefault) == 1,
        "v1.3 render stages default to enabled");
    require(
        sceneUnits && sceneUnits->type == kOfxParamTypeChoice,
        "physical scene units are exposed");
    require(
        irisBlades && irisBlades->type == kOfxParamTypeInteger,
        "shared procedural iris is exposed");
    require(
        starResponse && starResponse->type == kOfxParamTypeDouble,
        "physical starburst response is exposed");

    const ParamRecord* legacyRoot = findParam("glassAssetRoot");
    const ParamRecord* legacyAperture = findParam("apertureIndex");
    const ParamRecord* legacyDirt = findParam("dirtIndex");
    require(
        legacyRoot && legacyAperture && legacyDirt,
        "legacy asset parameters remain defined");
    require(
        intProperty(
            legacyRoot->properties,
            kOfxParamPropSecret) == 1 &&
        intProperty(
            legacyAperture->properties,
            kOfxParamPropSecret) == 1 &&
        intProperty(
            legacyDirt->properties,
            kOfxParamPropSecret) == 1,
        "legacy filesystem controls remain hidden from the v1.3 UI");
#if defined(__APPLE__)
    require(
        stringProperty(
            gEffectProperties,
            kOfxImageEffectPropMetalRenderSupported) == "true",
        "descriptor advertises Metal rendering");
#endif

    dlclose(library);
    std::cout << "Buckswood Optics Lab descriptor and UI tests passed\n";
    return 0;
}
