#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "CinematicToolsCore.h"
#include "OfxRenderRuntime.h"

#include "ofxImageEffect.h"
#include "ofxParam.h"
#include "ofxPixels.h"

#if defined __APPLE__ || defined __linux__ || defined __FreeBSD__
#  define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#  define EXPORT OfxExport
#else
#  error Unsupported platform
#endif

namespace {

enum PluginKind {
    PluginFrameDirector = 0,
    PluginRadianceRecover = 1,
    PluginTemporalIntegrity = 2,
};

OfxHost* gHost = nullptr;
OfxImageEffectSuiteV1* gEffectHost = nullptr;
OfxPropertySuiteV1* gPropHost = nullptr;
OfxParameterSuiteV1* gParamHost = nullptr;
OfxMultiThreadSuiteV1* gThreadHost = nullptr;

constexpr const char* kSourceFrameRangeProp = "OfxImageClipPropFrameRange_Source";
constexpr int kPluginMajorVersion = 2;
constexpr int kPluginMinorVersion = 1;

struct ImageInfo {
    void* data = nullptr;
    int rowBytes = 0;
    OfxRectI bounds{0, 0, 0, 0};
    char* pixelDepth = nullptr;
};

template <typename T>
T* pixelAddress(T* base, const OfxRectI& rect, int x, int y, int rowBytes)
{
    if (!base || x < rect.x1 || x >= rect.x2 || y < rect.y1 || y >= rect.y2) {
        return nullptr;
    }
    char* row = reinterpret_cast<char*>(base) + (y - rect.y1) * rowBytes;
    return reinterpret_cast<T*>(row) + (x - rect.x1);
}

bool readImageInfo(OfxPropertySetHandle image, ImageInfo& info)
{
    return gPropHost->propGetPointer(image, kOfxImagePropData, 0, &info.data) == kOfxStatOK &&
           gPropHost->propGetInt(image, kOfxImagePropRowBytes, 0, &info.rowBytes) == kOfxStatOK &&
           gPropHost->propGetIntN(image, kOfxImagePropBounds, 4, &info.bounds.x1) == kOfxStatOK &&
           gPropHost->propGetString(image, kOfxImageEffectPropPixelDepth, 0, &info.pixelDepth) == kOfxStatOK;
}

unsigned char toByte(float value)
{
    const float clamped = std::min(1.0f, std::max(0.0f, value));
    return static_cast<unsigned char>(clamped * 255.0f + 0.5f);
}

class FloatSampler final : public buckswood_cinematic::Sampler {
public:
    explicit FloatSampler(const ImageInfo& info)
        : info_(info)
        , base_(reinterpret_cast<OfxRGBAColourF*>(info.data))
    {
    }

    buckswood_cinematic::Pixel sample(float x, float y) const override
    {
        const int ix = std::clamp(
            static_cast<int>(x + 0.5f),
            info_.bounds.x1,
            std::max(info_.bounds.x1, info_.bounds.x2 - 1));
        const int iy = std::clamp(
            static_cast<int>(y + 0.5f),
            info_.bounds.y1,
            std::max(info_.bounds.y1, info_.bounds.y2 - 1));
        auto* pixel = pixelAddress(base_, info_.bounds, ix, iy, info_.rowBytes);
        if (!pixel) {
            return buckswood_cinematic::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }
        return buckswood_cinematic::Pixel{pixel->r, pixel->g, pixel->b, pixel->a};
    }

private:
    const ImageInfo& info_;
    OfxRGBAColourF* base_;
};

class ByteSampler final : public buckswood_cinematic::Sampler {
public:
    explicit ByteSampler(const ImageInfo& info)
        : info_(info)
        , base_(reinterpret_cast<OfxRGBAColourB*>(info.data))
    {
    }

    buckswood_cinematic::Pixel sample(float x, float y) const override
    {
        const int ix = std::clamp(
            static_cast<int>(x + 0.5f),
            info_.bounds.x1,
            std::max(info_.bounds.x1, info_.bounds.x2 - 1));
        const int iy = std::clamp(
            static_cast<int>(y + 0.5f),
            info_.bounds.y1,
            std::max(info_.bounds.y1, info_.bounds.y2 - 1));
        auto* pixel = pixelAddress(base_, info_.bounds, ix, iy, info_.rowBytes);
        if (!pixel) {
            return buckswood_cinematic::Pixel{0.0f, 0.0f, 0.0f, 0.0f};
        }
        return buckswood_cinematic::Pixel{
            pixel->r / 255.0f,
            pixel->g / 255.0f,
            pixel->b / 255.0f,
            pixel->a / 255.0f,
        };
    }

private:
    const ImageInfo& info_;
    OfxRGBAColourB* base_;
};

double doubleParamAtTime(OfxImageEffectHandle instance, const char* name, OfxTime time, double fallback)
{
    OfxParamSetHandle paramSet = nullptr;
    OfxParamHandle param = nullptr;
    if (gEffectHost->getParamSet(instance, &paramSet) != kOfxStatOK ||
        gParamHost->paramGetHandle(paramSet, name, &param, nullptr) != kOfxStatOK) {
        return fallback;
    }
    double value = fallback;
    return gParamHost->paramGetValueAtTime(param, time, &value) == kOfxStatOK ? value : fallback;
}

int intParamAtTime(OfxImageEffectHandle instance, const char* name, OfxTime time, int fallback)
{
    OfxParamSetHandle paramSet = nullptr;
    OfxParamHandle param = nullptr;
    if (gEffectHost->getParamSet(instance, &paramSet) != kOfxStatOK ||
        gParamHost->paramGetHandle(paramSet, name, &param, nullptr) != kOfxStatOK) {
        return fallback;
    }
    int value = fallback;
    return gParamHost->paramGetValueAtTime(param, time, &value) == kOfxStatOK ? value : fallback;
}

std::string stringParamAtTime(
    OfxImageEffectHandle instance,
    const char* name,
    OfxTime time,
    const char* fallback)
{
    OfxParamSetHandle paramSet = nullptr;
    OfxParamHandle param = nullptr;
    if (gEffectHost->getParamSet(instance, &paramSet) != kOfxStatOK ||
        gParamHost->paramGetHandle(paramSet, name, &param, nullptr) != kOfxStatOK) {
        return fallback;
    }
    char* value = nullptr;
    if (gParamHost->paramGetValueAtTime(param, time, &value) != kOfxStatOK || !value) {
        return fallback;
    }
    return value;
}

buckswood_cinematic::FrameDirectorControls frameControlsAtTime(
    OfxImageEffectHandle instance,
    OfxTime time)
{
    auto controls = buckswood_cinematic::CinematicToolsCore::defaultFrameDirectorControls();
    controls.targetAspect = intParamAtTime(instance, "targetAspect", time, controls.targetAspect);
    controls.viewMode = intParamAtTime(instance, "viewMode", time, controls.viewMode);
    controls.framingMode = intParamAtTime(instance, "framingMode", time, controls.framingMode);
    controls.autoStrength = static_cast<float>(doubleParamAtTime(instance, "autoStrength", time, controls.autoStrength));
    controls.subjectWeight = static_cast<float>(doubleParamAtTime(instance, "subjectWeight", time, controls.subjectWeight));
    controls.skinWeight = static_cast<float>(doubleParamAtTime(instance, "skinWeight", time, controls.skinWeight));
    controls.subjectLock = intParamAtTime(instance, "subjectLock", time, controls.subjectLock ? 1 : 0) != 0;
    controls.lockX = static_cast<float>(doubleParamAtTime(instance, "lockX", time, controls.lockX));
    controls.lockY = static_cast<float>(doubleParamAtTime(instance, "lockY", time, controls.lockY));
    controls.cutSensitivity = static_cast<float>(doubleParamAtTime(instance, "cutSensitivity", time, controls.cutSensitivity));
    controls.temporalSmoothing = static_cast<float>(doubleParamAtTime(instance, "temporalSmoothing", time, controls.temporalSmoothing));
    controls.motionLimit = static_cast<float>(doubleParamAtTime(instance, "motionLimit", time, controls.motionLimit));
    controls.manualX = static_cast<float>(doubleParamAtTime(instance, "manualX", time, controls.manualX));
    controls.manualY = static_cast<float>(doubleParamAtTime(instance, "manualY", time, controls.manualY));
    controls.subjectVerticalPosition = static_cast<float>(doubleParamAtTime(instance, "subjectVerticalPosition", time, controls.subjectVerticalPosition));
    controls.guideOpacity = static_cast<float>(doubleParamAtTime(instance, "guideOpacity", time, controls.guideOpacity));
    controls.matteOpacity = static_cast<float>(doubleParamAtTime(instance, "matteOpacity", time, controls.matteOpacity));
    controls.cropFeather = static_cast<float>(doubleParamAtTime(instance, "cropFeather", time, controls.cropFeather));
    controls.outputMix = static_cast<float>(doubleParamAtTime(instance, "outputMix", time, controls.outputMix));
    return controls;
}

buckswood_cinematic::RadianceControls radianceControlsAtTime(
    OfxImageEffectHandle instance,
    OfxTime time)
{
    auto controls = buckswood_cinematic::CinematicToolsCore::defaultRadianceControls();
    controls.viewMode = intParamAtTime(instance, "viewMode", time, controls.viewMode);
    controls.highlightRecovery = static_cast<float>(doubleParamAtTime(instance, "highlightRecovery", time, controls.highlightRecovery));
    controls.specularRecovery = static_cast<float>(doubleParamAtTime(instance, "specularRecovery", time, controls.specularRecovery));
    controls.highlightRolloff = static_cast<float>(doubleParamAtTime(instance, "highlightRolloff", time, controls.highlightRolloff));
    controls.shadowRecovery = static_cast<float>(doubleParamAtTime(instance, "shadowRecovery", time, controls.shadowRecovery));
    controls.recoveredHeadroomStops = static_cast<float>(doubleParamAtTime(instance, "recoveredHeadroomStops", time, controls.recoveredHeadroomStops));
    controls.localDetail = static_cast<float>(doubleParamAtTime(instance, "localDetail", time, controls.localDetail));
    controls.shadowDenoise = static_cast<float>(doubleParamAtTime(instance, "shadowDenoise", time, controls.shadowDenoise));
    controls.dequantization = static_cast<float>(doubleParamAtTime(instance, "dequantization", time, controls.dequantization));
    controls.temporalConsistency = static_cast<float>(doubleParamAtTime(instance, "temporalConsistency", time, controls.temporalConsistency));
    controls.longTermConsistency = static_cast<float>(doubleParamAtTime(instance, "longTermConsistency", time, controls.longTermConsistency));
    controls.colorGuard = static_cast<float>(doubleParamAtTime(instance, "colorGuard", time, controls.colorGuard));
    controls.outputMix = static_cast<float>(doubleParamAtTime(instance, "outputMix", time, controls.outputMix));
    return controls;
}

buckswood_cinematic::TemporalIntegrityControls temporalControlsAtTime(
    OfxImageEffectHandle instance,
    OfxTime time)
{
    auto controls = buckswood_cinematic::CinematicToolsCore::defaultTemporalIntegrityControls();
    controls.viewMode = intParamAtTime(instance, "viewMode", time, controls.viewMode);
    controls.sensitivity = static_cast<float>(doubleParamAtTime(instance, "sensitivity", time, controls.sensitivity));
    controls.repairStrength = static_cast<float>(doubleParamAtTime(instance, "repairStrength", time, controls.repairStrength));
    controls.flickerRepair = static_cast<float>(doubleParamAtTime(instance, "flickerRepair", time, controls.flickerRepair));
    controls.textureStability = static_cast<float>(doubleParamAtTime(instance, "textureStability", time, controls.textureStability));
    controls.longTermStability = static_cast<float>(doubleParamAtTime(instance, "longTermStability", time, controls.longTermStability));
    controls.motionGuard = static_cast<float>(doubleParamAtTime(instance, "motionGuard", time, controls.motionGuard));
    controls.ghostGuard = static_cast<float>(doubleParamAtTime(instance, "ghostGuard", time, controls.ghostGuard));
    controls.edgeProtection = static_cast<float>(doubleParamAtTime(instance, "edgeProtection", time, controls.edgeProtection));
    controls.chromaStability = static_cast<float>(doubleParamAtTime(instance, "chromaStability", time, controls.chromaStability));
    controls.overlayStrength = static_cast<float>(doubleParamAtTime(instance, "overlayStrength", time, controls.overlayStrength));
    controls.outputMix = static_cast<float>(doubleParamAtTime(instance, "outputMix", time, controls.outputMix));
    return controls;
}

template <typename PixelType>
bool sourceLooksLikeBlankAdjustment(const ImageInfo& source)
{
    const int width = source.bounds.x2 - source.bounds.x1;
    const int height = source.bounds.y2 - source.bounds.y1;
    if (width <= 0 || height <= 0) {
        return false;
    }

    auto* base = reinterpret_cast<PixelType*>(source.data);
    float maxRgb = 0.0f;
    float alphaSum = 0.0f;
    int sampleCount = 0;
    for (int gy = 0; gy < 7; ++gy) {
        const int y = source.bounds.y1 + (height - 1) * gy / 6;
        for (int gx = 0; gx < 7; ++gx) {
            const int x = source.bounds.x1 + (width - 1) * gx / 6;
            auto* pixel = pixelAddress(base, source.bounds, x, y, source.rowBytes);
            if (!pixel) {
                continue;
            }
            if constexpr (std::is_same<PixelType, OfxRGBAColourF>::value) {
                maxRgb = std::max(maxRgb, std::max(pixel->r, std::max(pixel->g, pixel->b)));
                alphaSum += pixel->a;
            } else {
                maxRgb = std::max(maxRgb, std::max(pixel->r, std::max(pixel->g, pixel->b)) / 255.0f);
                alphaSum += pixel->a / 255.0f;
            }
            ++sampleCount;
        }
    }
    return sampleCount > 0 && maxRgb < 0.015f && alphaSum / sampleCount > 0.75f;
}

bool sourceLooksLikeBlankAdjustmentByDepth(const ImageInfo& source)
{
    if (std::strcmp(source.pixelDepth, kOfxBitDepthFloat) == 0) {
        return sourceLooksLikeBlankAdjustment<OfxRGBAColourF>(source);
    }
    if (std::strcmp(source.pixelDepth, kOfxBitDepthByte) == 0) {
        return sourceLooksLikeBlankAdjustment<OfxRGBAColourB>(source);
    }
    return false;
}

#pragma pack(push, 1)
struct RadianceCacheHeader {
    char magic[8];
    std::uint32_t version;
    std::uint32_t width;
    std::uint32_t height;
    std::int64_t frameIndex;
    std::uint32_t flags;
};
#pragma pack(pop)

static_assert(sizeof(RadianceCacheHeader) == 32, "Radiance cache header must remain stable");

float halfToFloat(std::uint16_t half)
{
    const std::uint32_t sign = (static_cast<std::uint32_t>(half & 0x8000u)) << 16u;
    std::uint32_t exponent = (half >> 10u) & 0x1fu;
    std::uint32_t mantissa = half & 0x03ffu;
    std::uint32_t result = 0;

    if (exponent == 0) {
        if (mantissa == 0) {
            result = sign;
        } else {
            const float subnormal =
                std::ldexp(static_cast<float>(mantissa), -24) *
                ((sign != 0) ? -1.0f : 1.0f);
            return subnormal;
        }
    } else if (exponent == 31) {
        result = sign | 0x7f800000u | (mantissa << 13u);
    } else {
        const std::uint32_t floatExponent = exponent + (127u - 15u);
        result = sign | (floatExponent << 23u) | (mantissa << 13u);
    }

    float value = 0.0f;
    std::memcpy(&value, &result, sizeof(value));
    return value;
}

class RadianceCacheFrame {
public:
    bool load(
        const std::string& directory,
        std::int64_t frameIndex,
        const OfxRectI& imageBounds,
        const OfxRectI& renderWindow)
    {
        valid_ = false;
        data_.clear();
        if (directory.empty()) {
            return false;
        }

        std::ostringstream fileName;
        fileName << "frame_" << std::setw(8) << std::setfill('0') << frameIndex << ".bwrcache";
        const std::filesystem::path path = std::filesystem::path(directory) / fileName.str();
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            return false;
        }

        RadianceCacheHeader header{};
        input.read(reinterpret_cast<char*>(&header), sizeof(header));
        const char expectedMagic[8] = {'B', 'W', 'R', 'C', 'V', '2', '\0', '\0'};
        if (!input ||
            std::memcmp(header.magic, expectedMagic, sizeof(expectedMagic)) != 0 ||
            header.version != 2 ||
            header.frameIndex != frameIndex ||
            header.width != static_cast<std::uint32_t>(imageBounds.x2 - imageBounds.x1) ||
            header.height != static_cast<std::uint32_t>(imageBounds.y2 - imageBounds.y1)) {
            return false;
        }

        x1_ = std::max(renderWindow.x1, imageBounds.x1);
        y1_ = std::max(renderWindow.y1, imageBounds.y1);
        x2_ = std::min(renderWindow.x2, imageBounds.x2);
        y2_ = std::min(renderWindow.y2, imageBounds.y2);
        if (x1_ >= x2_ || y1_ >= y2_) {
            return false;
        }

        width_ = x2_ - x1_;
        height_ = y2_ - y1_;
        data_.resize(static_cast<std::size_t>(width_) * height_ * 4u);
        constexpr std::size_t valuesPerPixel = 4u;
        constexpr std::size_t bytesPerPixel = valuesPerPixel * sizeof(std::uint16_t);
        if (
            x1_ == imageBounds.x1 &&
            y1_ == imageBounds.y1 &&
            x2_ == imageBounds.x2 &&
            y2_ == imageBounds.y2) {
            input.read(
                reinterpret_cast<char*>(data_.data()),
                static_cast<std::streamsize>(
                    static_cast<std::size_t>(width_) *
                    static_cast<std::size_t>(height_) *
                    bytesPerPixel));
            if (!input) {
                data_.clear();
                return false;
            }
        } else {
            const std::size_t fullWidth = header.width;
            for (int row = 0; row < height_; ++row) {
                const std::size_t sourceY =
                    static_cast<std::size_t>(
                        y1_ - imageBounds.y1 + row);
                const std::size_t sourceX =
                    static_cast<std::size_t>(
                        x1_ - imageBounds.x1);
                const std::size_t pixelOffset =
                    sourceY * fullWidth + sourceX;
                const std::streamoff byteOffset =
                    static_cast<std::streamoff>(
                        sizeof(RadianceCacheHeader) +
                        pixelOffset * bytesPerPixel);
                input.seekg(byteOffset, std::ios::beg);
                auto* destination =
                    data_.data() +
                    static_cast<std::size_t>(row) *
                        width_ *
                        valuesPerPixel;
                input.read(
                    reinterpret_cast<char*>(destination),
                    static_cast<std::streamsize>(
                        static_cast<std::size_t>(width_) *
                        bytesPerPixel));
                if (!input) {
                    data_.clear();
                    return false;
                }
            }
        }

        valid_ = true;
        return true;
    }

    bool sample(int x, int y, buckswood_cinematic::Pixel& pixel, float& confidence) const
    {
        if (!valid_ || x < x1_ || x >= x2_ || y < y1_ || y >= y2_) {
            return false;
        }
        const std::size_t index =
            (static_cast<std::size_t>(y - y1_) * width_ + static_cast<std::size_t>(x - x1_)) * 4u;
        pixel = buckswood_cinematic::Pixel{
            halfToFloat(data_[index]),
            halfToFloat(data_[index + 1]),
            halfToFloat(data_[index + 2]),
            1.0f,
        };
        confidence = std::clamp(halfToFloat(data_[index + 3]), 0.0f, 1.0f);
        return true;
    }

    bool valid() const
    {
        return valid_;
    }

    std::size_t byteSize() const
    {
        return data_.size() * sizeof(std::uint16_t);
    }

private:
    bool valid_ = false;
    int x1_ = 0;
    int y1_ = 0;
    int x2_ = 0;
    int y2_ = 0;
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint16_t> data_;
};

std::filesystem::path radianceCachePath(
    const std::string& directory,
    std::int64_t frameIndex)
{
    std::ostringstream fileName;
    fileName
        << "frame_"
        << std::setw(8)
        << std::setfill('0')
        << frameIndex
        << ".bwrcache";
    return std::filesystem::path(directory) / fileName.str();
}

class RadianceFrameStore {
public:
    std::shared_ptr<const RadianceCacheFrame> get(
        const std::string& directory,
        std::int64_t frameIndex,
        const OfxRectI& imageBounds)
    {
        if (directory.empty()) {
            return nullptr;
        }

        const auto path = radianceCachePath(directory, frameIndex);
        std::error_code error;
        const auto writeTime = std::filesystem::last_write_time(path, error);
        if (error) {
            return nullptr;
        }
        const auto fileSize = std::filesystem::file_size(path, error);
        if (error) {
            return nullptr;
        }

        const Key key{
            path.string(),
            writeTime.time_since_epoch().count(),
            fileSize,
            frameIndex,
            imageBounds,
        };
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->key == key) {
                auto frame = it->frame;
                entries_.splice(entries_.begin(), entries_, it);
                return frame;
            }
        }

        auto frame = std::make_shared<RadianceCacheFrame>();
        if (!frame->load(
                directory,
                frameIndex,
                imageBounds,
                imageBounds)) {
            return nullptr;
        }
        const std::size_t bytes = frame->byteSize();
        entries_.push_front(Entry{key, frame, bytes});
        residentBytes_ += bytes;
        while (
            entries_.size() > kMaxFrames ||
            (residentBytes_ > kMaxResidentBytes &&
             entries_.size() > 1)) {
            residentBytes_ -= entries_.back().bytes;
            entries_.pop_back();
        }
        return frame;
    }

private:
    struct Key {
        std::string path;
        std::filesystem::file_time_type::duration::rep writeTime;
        std::uintmax_t fileSize;
        std::int64_t frameIndex;
        OfxRectI bounds;

        bool operator==(const Key& other) const
        {
            return path == other.path &&
                writeTime == other.writeTime &&
                fileSize == other.fileSize &&
                frameIndex == other.frameIndex &&
                bounds.x1 == other.bounds.x1 &&
                bounds.y1 == other.bounds.y1 &&
                bounds.x2 == other.bounds.x2 &&
                bounds.y2 == other.bounds.y2;
        }
    };

    struct Entry {
        Key key;
        std::shared_ptr<const RadianceCacheFrame> frame;
        std::size_t bytes;
    };

    static constexpr std::size_t kMaxFrames = 6;
    static constexpr std::size_t kMaxResidentBytes =
        512u * 1024u * 1024u;
    std::mutex mutex_;
    std::list<Entry> entries_;
    std::size_t residentBytes_ = 0;
};

RadianceFrameStore& radianceFrameStore()
{
    static RadianceFrameStore store;
    return store;
}

struct FrameDirectorPrepared {
    buckswood_cinematic::FocusAnalysis focus;
    buckswood_cinematic::CropWindow crop;
};

bool frameDirectorControlsEqual(
    const buckswood_cinematic::FrameDirectorControls& left,
    const buckswood_cinematic::FrameDirectorControls& right)
{
    return left.targetAspect == right.targetAspect &&
        left.viewMode == right.viewMode &&
        left.framingMode == right.framingMode &&
        left.autoStrength == right.autoStrength &&
        left.subjectWeight == right.subjectWeight &&
        left.skinWeight == right.skinWeight &&
        left.subjectLock == right.subjectLock &&
        left.lockX == right.lockX &&
        left.lockY == right.lockY &&
        left.cutSensitivity == right.cutSensitivity &&
        left.temporalSmoothing == right.temporalSmoothing &&
        left.motionLimit == right.motionLimit &&
        left.manualX == right.manualX &&
        left.manualY == right.manualY &&
        left.subjectVerticalPosition == right.subjectVerticalPosition &&
        left.guideOpacity == right.guideOpacity &&
        left.matteOpacity == right.matteOpacity &&
        left.cropFeather == right.cropFeather &&
        left.outputMix == right.outputMix;
}

struct FrameDirectorCacheKey {
    OfxImageEffectHandle instance = nullptr;
    const void* sourceData = nullptr;
    std::array<const void*, 4> temporalData{};
    buckswood_cinematic::FrameDirectorControls controls{};
    std::uint64_t timeBits = 0;
    int width = 0;
    int height = 0;
    int rowBytes = 0;
    int depthCode = 0;

    bool operator==(const FrameDirectorCacheKey& other) const
    {
        return instance == other.instance &&
            sourceData == other.sourceData &&
            temporalData == other.temporalData &&
            frameDirectorControlsEqual(controls, other.controls) &&
            timeBits == other.timeBits &&
            width == other.width &&
            height == other.height &&
            rowBytes == other.rowBytes &&
            depthCode == other.depthCode;
    }
};

class FrameDirectorStore {
public:
    template <typename Builder>
    std::shared_ptr<const FrameDirectorPrepared> getOrCreate(
        const FrameDirectorCacheKey& key,
        Builder&& builder)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->key == key) {
                auto value = it->value;
                entries_.splice(entries_.begin(), entries_, it);
                return value;
            }
        }
        auto value =
            std::make_shared<const FrameDirectorPrepared>(builder());
        entries_.push_front(Entry{key, value});
        while (entries_.size() > kCapacity) {
            entries_.pop_back();
        }
        return value;
    }

private:
    struct Entry {
        FrameDirectorCacheKey key;
        std::shared_ptr<const FrameDirectorPrepared> value;
    };

    static constexpr std::size_t kCapacity = 16;
    std::mutex mutex_;
    std::list<Entry> entries_;
};

FrameDirectorStore& frameDirectorStore()
{
    static FrameDirectorStore store;
    return store;
}

buckswood_cinematic::Pixel mixPixels(
    buckswood_cinematic::Pixel a,
    buckswood_cinematic::Pixel b,
    float amount)
{
    amount = std::clamp(amount, 0.0f, 1.0f);
    return buckswood_cinematic::Pixel{
        a.r + (b.r - a.r) * amount,
        a.g + (b.g - a.g) * amount,
        a.b + (b.b - a.b) * amount,
        a.a + (b.a - a.a) * amount,
    };
}

template <typename DstPixel>
void writePixel(DstPixel* destination, buckswood_cinematic::Pixel value)
{
    if constexpr (std::is_same<DstPixel, OfxRGBAColourF>::value) {
        destination->r = value.r;
        destination->g = value.g;
        destination->b = value.b;
        destination->a = value.a;
    } else {
        destination->r = toByte(value.r);
        destination->g = toByte(value.g);
        destination->b = toByte(value.b);
        destination->a = toByte(value.a);
    }
}

template <typename DstPixel>
OfxStatus fillTransparent(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& destination)
{
    auto* base = reinterpret_cast<DstPixel*>(destination.data);
    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        if (gEffectHost->abort(instance)) {
            break;
        }
        for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
            auto* pixel = pixelAddress(base, destination.bounds, x, y, destination.rowBytes);
            if (pixel) {
                writePixel(pixel, buckswood_cinematic::Pixel{0.0f, 0.0f, 0.0f, 0.0f});
            }
        }
    }
    return kOfxStatOK;
}

OfxStatus fillTransparentByDepth(
    OfxImageEffectHandle instance,
    const OfxRectI& renderWindow,
    const ImageInfo& destination)
{
    if (std::strcmp(destination.pixelDepth, kOfxBitDepthFloat) == 0) {
        return fillTransparent<OfxRGBAColourF>(instance, renderWindow, destination);
    }
    if (std::strcmp(destination.pixelDepth, kOfxBitDepthByte) == 0) {
        return fillTransparent<OfxRGBAColourB>(instance, renderWindow, destination);
    }
    return kOfxStatErrUnsupported;
}

template <typename DstPixel, typename SamplerType>
OfxStatus renderTyped(
    PluginKind kind,
    OfxImageEffectHandle instance,
    OfxTime time,
    const OfxRectI& renderWindow,
    const ImageInfo& source,
    const ImageInfo* previous2,
    const ImageInfo* previous,
    const ImageInfo* next,
    const ImageInfo* next2,
    const ImageInfo& destination)
{
    const SamplerType sampler(source);
    const SamplerType previous2Sampler(previous2 ? *previous2 : source);
    const SamplerType previousSampler(previous ? *previous : source);
    const SamplerType nextSampler(next ? *next : source);
    const SamplerType next2Sampler(next2 ? *next2 : source);
    const buckswood_cinematic::TemporalContext temporal{
        previous2 ? static_cast<const buckswood_cinematic::Sampler*>(&previous2Sampler) : nullptr,
        previous ? static_cast<const buckswood_cinematic::Sampler*>(&previousSampler) : nullptr,
        next ? static_cast<const buckswood_cinematic::Sampler*>(&nextSampler) : nullptr,
        next2 ? static_cast<const buckswood_cinematic::Sampler*>(&next2Sampler) : nullptr,
        previous2 != nullptr,
        previous != nullptr,
        next != nullptr,
        next2 != nullptr,
    };
    const buckswood_cinematic::FrameInfo frame{
        destination.bounds.x2 - destination.bounds.x1,
        destination.bounds.y2 - destination.bounds.y1,
        static_cast<int>(time),
    };

    auto* destinationBase = reinterpret_cast<DstPixel*>(destination.data);
    auto frameControls = buckswood_cinematic::CinematicToolsCore::defaultFrameDirectorControls();
    auto radianceControls = buckswood_cinematic::CinematicToolsCore::defaultRadianceControls();
    auto temporalControls = buckswood_cinematic::CinematicToolsCore::defaultTemporalIntegrityControls();
    buckswood_cinematic::FocusAnalysis focus{0.5f, 0.46f, 0.0f};
    buckswood_cinematic::CropWindow crop{0.0f, 0.0f, static_cast<float>(frame.width), static_cast<float>(frame.height)};
    auto framePrepared =
        buckswood_cinematic::CinematicToolsCore::prepareFrameDirector(
            frame,
            frameControls,
            crop);
    auto radiancePrepared =
        buckswood_cinematic::CinematicToolsCore::prepareRadiance(
            radianceControls);
    auto temporalPrepared =
        buckswood_cinematic::CinematicToolsCore::
            prepareTemporalIntegrity(temporalControls);
    std::shared_ptr<const RadianceCacheFrame> radianceCache;
    float mlCacheBlend = 0.85f;
    float mlConfidenceThreshold = 0.20f;

    if (kind == PluginFrameDirector) {
        frameControls = frameControlsAtTime(instance, time);
        FrameDirectorCacheKey key;
        key.instance = instance;
        key.sourceData = source.data;
        key.temporalData = {
            previous2 ? previous2->data : nullptr,
            previous ? previous->data : nullptr,
            next ? next->data : nullptr,
            next2 ? next2->data : nullptr,
        };
        key.controls = frameControls;
        std::memcpy(&key.timeBits, &time, sizeof(time));
        key.width = frame.width;
        key.height = frame.height;
        key.rowBytes = source.rowBytes;
        key.depthCode =
            source.pixelDepth &&
                std::strcmp(source.pixelDepth, kOfxBitDepthFloat) == 0
            ? 1
            : 2;
        const auto prepared = frameDirectorStore().getOrCreate(
            key,
            [&]() {
                FrameDirectorPrepared value;
                const auto currentFocus =
                    buckswood_cinematic::CinematicToolsCore::
                        analyzeFocus(
                            sampler,
                            frame,
                            frameControls);
                buckswood_cinematic::FocusAnalysis previous2Focus;
                buckswood_cinematic::FocusAnalysis previousFocus;
                buckswood_cinematic::FocusAnalysis nextFocus;
                buckswood_cinematic::FocusAnalysis next2Focus;
                const buckswood_cinematic::FocusAnalysis*
                    previous2FocusPtr = nullptr;
                const buckswood_cinematic::FocusAnalysis*
                    previousFocusPtr = nullptr;
                const buckswood_cinematic::FocusAnalysis*
                    nextFocusPtr = nullptr;
                const buckswood_cinematic::FocusAnalysis*
                    next2FocusPtr = nullptr;
                if (previous2) {
                    previous2Focus =
                        buckswood_cinematic::CinematicToolsCore::
                            analyzeFocus(
                                previous2Sampler,
                                frame,
                                frameControls);
                    previous2FocusPtr = &previous2Focus;
                }
                if (previous) {
                    previousFocus =
                        buckswood_cinematic::CinematicToolsCore::
                            analyzeFocus(
                                previousSampler,
                                frame,
                                frameControls);
                    previousFocusPtr = &previousFocus;
                }
                if (next) {
                    nextFocus =
                        buckswood_cinematic::CinematicToolsCore::
                            analyzeFocus(
                                nextSampler,
                                frame,
                                frameControls);
                    nextFocusPtr = &nextFocus;
                }
                if (next2) {
                    next2Focus =
                        buckswood_cinematic::CinematicToolsCore::
                            analyzeFocus(
                                next2Sampler,
                                frame,
                                frameControls);
                    next2FocusPtr = &next2Focus;
                }
                value.focus =
                    buckswood_cinematic::CinematicToolsCore::
                        smoothFocus(
                            currentFocus,
                            previous2FocusPtr,
                            previousFocusPtr,
                            nextFocusPtr,
                            next2FocusPtr,
                            frameControls);
                value.crop =
                    buckswood_cinematic::CinematicToolsCore::
                        cropWindow(
                            frame,
                            frameControls,
                            value.focus);
                return value;
            });
        focus = prepared->focus;
        crop = prepared->crop;
        framePrepared =
            buckswood_cinematic::CinematicToolsCore::
                prepareFrameDirector(
                    frame,
                    frameControls,
                    crop);
    } else if (kind == PluginRadianceRecover) {
        radianceControls = radianceControlsAtTime(instance, time);
        radiancePrepared =
            buckswood_cinematic::CinematicToolsCore::
                prepareRadiance(radianceControls);
        const bool useMLCache = intParamAtTime(instance, "useMLCache", time, 0) != 0;
        mlCacheBlend = static_cast<float>(doubleParamAtTime(instance, "mlCacheBlend", time, 0.85));
        mlConfidenceThreshold = static_cast<float>(doubleParamAtTime(instance, "mlConfidenceThreshold", time, 0.20));
        const int cacheFrameOffset = static_cast<int>(std::llround(
            doubleParamAtTime(instance, "cacheFrameOffset", time, 0.0)));
        const std::string cacheDirectory = stringParamAtTime(instance, "cacheDirectory", time, "");
        if (useMLCache) {
            radianceCache = radianceFrameStore().get(
                cacheDirectory,
                static_cast<std::int64_t>(std::llround(time)) + cacheFrameOffset,
                destination.bounds);
        }
    } else {
        temporalControls = temporalControlsAtTime(instance, time);
        temporalPrepared =
            buckswood_cinematic::CinematicToolsCore::
                prepareTemporalIntegrity(temporalControls);
    }

    auto renderRows = [&](int firstY, int lastY) {
        for (int y = firstY; y < lastY; ++y) {
            if (gEffectHost->abort(instance)) {
                break;
            }
            for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
                auto* destinationPixel = pixelAddress(
                    destinationBase,
                    destination.bounds,
                    x,
                    y,
                    destination.rowBytes);
                if (!destinationPixel) {
                    continue;
                }

                buckswood_cinematic::Pixel output;
                if (kind == PluginFrameDirector) {
                    output = buckswood_cinematic::CinematicToolsCore::processFrameDirector(
                        sampler,
                        x,
                        y,
                        frame,
                        frameControls,
                        focus,
                        crop,
                        framePrepared);
                } else if (kind == PluginRadianceRecover) {
                    auto nativeControls = radianceControls;
                    if (nativeControls.viewMode == buckswood_cinematic::RadianceMLConfidence ||
                        nativeControls.viewMode == buckswood_cinematic::RadianceMLResult) {
                        nativeControls.viewMode = buckswood_cinematic::RadianceResult;
                    }
                    output = buckswood_cinematic::CinematicToolsCore::processRadiance(
                        sampler,
                        x,
                        y,
                        frame,
                        nativeControls,
                        radiancePrepared,
                        &temporal);
                    buckswood_cinematic::Pixel cached;
                    float confidence = 0.0f;
                    const bool hasCachedPixel =
                        radianceCache &&
                        radianceCache->sample(
                            x,
                            y,
                            cached,
                            confidence);
                    cached.a = sampler.sample(static_cast<float>(x), static_cast<float>(y)).a;
                    if (radianceControls.viewMode == buckswood_cinematic::RadianceMLConfidence) {
                        output = buckswood_cinematic::Pixel{
                            hasCachedPixel ? confidence : 0.0f,
                            hasCachedPixel ? confidence : 0.0f,
                            hasCachedPixel ? confidence : 0.0f,
                            cached.a,
                        };
                    } else if (radianceControls.viewMode == buckswood_cinematic::RadianceMLResult) {
                        if (hasCachedPixel) {
                            output = cached;
                        }
                    } else if (hasCachedPixel) {
                        const float confidenceBlend = std::clamp(
                            (confidence - mlConfidenceThreshold) /
                                std::max(0.001f, 1.0f - mlConfidenceThreshold),
                            0.0f,
                            1.0f);
                        output = mixPixels(
                            output,
                            cached,
                            mlCacheBlend * confidenceBlend * radianceControls.outputMix);
                    }
                } else {
                    output = buckswood_cinematic::CinematicToolsCore::processTemporalIntegrity(
                        sampler,
                        x,
                        y,
                        frame,
                        temporalControls,
                        temporalPrepared,
                        &temporal);
                }
                writePixel(destinationPixel, output);
            }
        }
    };

    return buckswood::ofx_runtime::parallelRows(
        gThreadHost,
        renderWindow.y1,
        renderWindow.y2,
        renderRows);
}

class NoImageException {};

OfxStatus render(
    PluginKind kind,
    OfxImageEffectHandle instance,
    OfxPropertySetHandle inArgs)
{
    OfxTime time = 0.0;
    OfxRectI renderWindow{0, 0, 0, 0};
    gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    gPropHost->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

    OfxImageClipHandle outputClip = nullptr;
    OfxImageClipHandle sourceClip = nullptr;
    gEffectHost->clipGetHandle(instance, kOfxImageEffectOutputClipName, &outputClip, nullptr);
    gEffectHost->clipGetHandle(instance, kOfxImageEffectSimpleSourceClipName, &sourceClip, nullptr);

    OfxPropertySetHandle outputImage = nullptr;
    OfxPropertySetHandle sourceImage = nullptr;
    OfxPropertySetHandle previous2Image = nullptr;
    OfxPropertySetHandle previousImage = nullptr;
    OfxPropertySetHandle nextImage = nullptr;
    OfxPropertySetHandle next2Image = nullptr;
    OfxStatus status = kOfxStatOK;

    try {
        if (gEffectHost->clipGetImage(outputClip, time, nullptr, &outputImage) != kOfxStatOK) {
            throw NoImageException();
        }

        ImageInfo destination;
        if (!readImageInfo(outputImage, destination)) {
            throw NoImageException();
        }

        if (gEffectHost->clipGetImage(sourceClip, time, nullptr, &sourceImage) != kOfxStatOK) {
            status = fillTransparentByDepth(instance, renderWindow, destination);
        } else {
            ImageInfo source;
            if (!readImageInfo(sourceImage, source)) {
                throw NoImageException();
            }

            const int adjustmentLayerGuard = intParamAtTime(instance, "adjustmentLayerGuard", time, 1);
            if (adjustmentLayerGuard && sourceLooksLikeBlankAdjustmentByDepth(source)) {
                status = fillTransparentByDepth(instance, renderWindow, destination);
            } else {
                const int temporalOffset =
                    std::max(1, intParamAtTime(instance, "temporalOffset", time, 0) + 1);
                ImageInfo previous2;
                ImageInfo previous;
                ImageInfo next;
                ImageInfo next2;
                ImageInfo* previous2Ptr = nullptr;
                ImageInfo* previousPtr = nullptr;
                ImageInfo* nextPtr = nullptr;
                ImageInfo* next2Ptr = nullptr;

                if (gEffectHost->clipGetImage(sourceClip, time - temporalOffset * 2, nullptr, &previous2Image) == kOfxStatOK &&
                    readImageInfo(previous2Image, previous2) &&
                    std::strcmp(previous2.pixelDepth, source.pixelDepth) == 0) {
                    previous2Ptr = &previous2;
                }
                if (gEffectHost->clipGetImage(sourceClip, time - temporalOffset, nullptr, &previousImage) == kOfxStatOK &&
                    readImageInfo(previousImage, previous) &&
                    std::strcmp(previous.pixelDepth, source.pixelDepth) == 0) {
                    previousPtr = &previous;
                }
                if (gEffectHost->clipGetImage(sourceClip, time + temporalOffset, nullptr, &nextImage) == kOfxStatOK &&
                    readImageInfo(nextImage, next) &&
                    std::strcmp(next.pixelDepth, source.pixelDepth) == 0) {
                    nextPtr = &next;
                }
                if (gEffectHost->clipGetImage(sourceClip, time + temporalOffset * 2, nullptr, &next2Image) == kOfxStatOK &&
                    readImageInfo(next2Image, next2) &&
                    std::strcmp(next2.pixelDepth, source.pixelDepth) == 0) {
                    next2Ptr = &next2;
                }

                if (std::strcmp(destination.pixelDepth, kOfxBitDepthFloat) == 0 &&
                    std::strcmp(source.pixelDepth, kOfxBitDepthFloat) == 0) {
                    status = renderTyped<OfxRGBAColourF, FloatSampler>(
                        kind,
                        instance,
                        time,
                        renderWindow,
                        source,
                        previous2Ptr,
                        previousPtr,
                        nextPtr,
                        next2Ptr,
                        destination);
                } else if (std::strcmp(destination.pixelDepth, kOfxBitDepthByte) == 0 &&
                           std::strcmp(source.pixelDepth, kOfxBitDepthByte) == 0) {
                    status = renderTyped<OfxRGBAColourB, ByteSampler>(
                        kind,
                        instance,
                        time,
                        renderWindow,
                        source,
                        previous2Ptr,
                        previousPtr,
                        nextPtr,
                        next2Ptr,
                        destination);
                } else {
                    status = kOfxStatErrUnsupported;
                }
            }
        }
    } catch (NoImageException&) {
        status = gEffectHost->abort(instance) ? kOfxStatOK : kOfxStatFailed;
    }

    if (sourceImage) {
        gEffectHost->clipReleaseImage(sourceImage);
    }
    if (previous2Image) {
        gEffectHost->clipReleaseImage(previous2Image);
    }
    if (previousImage) {
        gEffectHost->clipReleaseImage(previousImage);
    }
    if (nextImage) {
        gEffectHost->clipReleaseImage(nextImage);
    }
    if (next2Image) {
        gEffectHost->clipReleaseImage(next2Image);
    }
    if (outputImage) {
        gEffectHost->clipReleaseImage(outputImage);
    }
    return status;
}

void defineDoubleParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    double defaultValue,
    double minValue,
    double maxValue,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeDouble, name, &properties);
    gPropHost->propSetString(properties, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, label);
    gPropHost->propSetString(properties, kOfxParamPropDoubleType, 0, kOfxParamDoubleTypePlain);
    gPropHost->propSetDouble(properties, kOfxParamPropDefault, 0, defaultValue);
    gPropHost->propSetDouble(properties, kOfxParamPropMin, 0, minValue);
    gPropHost->propSetDouble(properties, kOfxParamPropMax, 0, maxValue);
    gPropHost->propSetDouble(properties, kOfxParamPropDisplayMin, 0, minValue);
    gPropHost->propSetDouble(properties, kOfxParamPropDisplayMax, 0, maxValue);
    gPropHost->propSetString(page, kOfxParamPropPageChild, pageIndex, name);
}

void defineChoiceParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int defaultValue,
    const char* const* options,
    int optionCount,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeChoice, name, &properties);
    gPropHost->propSetString(properties, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, label);
    gPropHost->propSetInt(properties, kOfxParamPropDefault, 0, defaultValue);
    for (int index = 0; index < optionCount; ++index) {
        gPropHost->propSetString(properties, kOfxParamPropChoiceOption, index, options[index]);
    }
    gPropHost->propSetString(page, kOfxParamPropPageChild, pageIndex, name);
}

void defineBooleanParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int defaultValue,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeBoolean, name, &properties);
    gPropHost->propSetString(properties, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, label);
    gPropHost->propSetInt(properties, kOfxParamPropDefault, 0, defaultValue);
    gPropHost->propSetString(page, kOfxParamPropPageChild, pageIndex, name);
}

void defineDirectoryParam(
    OfxParamSetHandle paramSet,
    const char* name,
    const char* label,
    int pageIndex,
    OfxPropertySetHandle page)
{
    OfxPropertySetHandle properties = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypeString, name, &properties);
    gPropHost->propSetString(properties, kOfxParamPropScriptName, 0, name);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, label);
    gPropHost->propSetString(properties, kOfxParamPropStringMode, 0, kOfxParamStringIsDirectoryPath);
    gPropHost->propSetString(properties, kOfxParamPropDefault, 0, "");
    gPropHost->propSetString(page, kOfxParamPropPageChild, pageIndex, name);
}

OfxStatus describeInContext(PluginKind kind, OfxImageEffectHandle effect)
{
    OfxPropertySetHandle properties = nullptr;
    gEffectHost->clipDefine(effect, kOfxImageEffectOutputClipName, &properties);
    gPropHost->propSetString(properties, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gEffectHost->clipDefine(effect, kOfxImageEffectSimpleSourceClipName, &properties);
    gPropHost->propSetString(properties, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropHost->propSetInt(properties, kOfxImageEffectPropTemporalClipAccess, 0, 1);

    OfxParamSetHandle paramSet = nullptr;
    gEffectHost->getParamSet(effect, &paramSet);
    OfxPropertySetHandle page = nullptr;
    gParamHost->paramDefine(paramSet, kOfxParamTypePage, "Main", &page);

    const char* temporalOffsets[] = {"1 Frame", "2 Frames", "3 Frames"};
    if (kind == PluginFrameDirector) {
        gPropHost->propSetString(page, kOfxPropLabel, 0, "Frame Director");
        const char* aspects[] = {
            "2.39:1 Scope",
            "2.00:1 Univisium",
            "1.85:1 Flat",
            "16:9",
            "4:3",
            "1:1",
            "9:16 Vertical",
        };
        const char* views[] = {
            "Cinematic Crop",
            "Composition Guides",
            "Saliency Heatmap",
            "Crop Matte",
            "Framing Confidence",
        };
        const char* modes[] = {"Auto Composition", "Center", "Left Third", "Right Third", "Manual"};
        defineChoiceParam(paramSet, "targetAspect", "Target Aspect Ratio", 0, aspects, 7, 0, page);
        defineChoiceParam(paramSet, "viewMode", "View", 0, views, 5, 1, page);
        defineChoiceParam(paramSet, "framingMode", "Framing Mode", 0, modes, 5, 2, page);
        defineDoubleParam(paramSet, "autoStrength", "Auto Framing Strength", 0.78, 0.0, 1.0, 3, page);
        defineDoubleParam(paramSet, "subjectWeight", "Subject Detail Weight", 0.75, 0.0, 2.0, 4, page);
        defineDoubleParam(paramSet, "skinWeight", "Skin Priority", 0.35, 0.0, 2.0, 5, page);
        defineBooleanParam(paramSet, "subjectLock", "Subject Lock", 0, 6, page);
        defineDoubleParam(paramSet, "lockX", "Locked Subject X", 0.50, 0.0, 1.0, 7, page);
        defineDoubleParam(paramSet, "lockY", "Locked Subject Y", 0.42, 0.0, 1.0, 8, page);
        defineDoubleParam(paramSet, "cutSensitivity", "Shot Cut Sensitivity", 0.65, 0.0, 1.0, 9, page);
        defineDoubleParam(paramSet, "temporalSmoothing", "Camera Path Smoothing", 0.70, 0.0, 1.0, 10, page);
        defineChoiceParam(paramSet, "temporalOffset", "Analysis Radius", 0, temporalOffsets, 3, 11, page);
        defineDoubleParam(paramSet, "motionLimit", "Maximum Reframe Step", 0.18, 0.01, 0.50, 12, page);
        defineDoubleParam(paramSet, "manualX", "Horizontal Offset", 0.0, -1.0, 1.0, 13, page);
        defineDoubleParam(paramSet, "manualY", "Vertical Offset", 0.0, -1.0, 1.0, 14, page);
        defineDoubleParam(paramSet, "subjectVerticalPosition", "Subject Height", 0.38, 0.20, 0.80, 15, page);
        defineDoubleParam(paramSet, "guideOpacity", "Guide Opacity", 0.78, 0.0, 1.0, 16, page);
        defineDoubleParam(paramSet, "matteOpacity", "Outside Matte", 1.0, 0.0, 1.0, 17, page);
        defineDoubleParam(paramSet, "cropFeather", "Crop Edge Feather", 0.006, 0.0, 0.05, 18, page);
        defineDoubleParam(paramSet, "outputMix", "Output Mix", 1.0, 0.0, 1.0, 19, page);
        defineBooleanParam(paramSet, "adjustmentLayerGuard", "Adjustment Layer Guard", 1, 20, page);
    } else if (kind == PluginRadianceRecover) {
        gPropHost->propSetString(page, kOfxPropLabel, 0, "Radiance Recover");
        const char* views[] = {
            "Recovered Image",
            "Recovery Confidence Matte",
            "Clipping Map",
            "Recovery Difference",
            "ML Cache Confidence",
            "ML Cache Result",
        };
        defineChoiceParam(paramSet, "viewMode", "View", 0, views, 6, 0, page);
        defineDoubleParam(paramSet, "highlightRecovery", "Highlight Reconstruction", 0.62, 0.0, 1.0, 1, page);
        defineDoubleParam(paramSet, "specularRecovery", "Specular Reconstruction", 0.35, 0.0, 1.0, 2, page);
        defineDoubleParam(paramSet, "highlightRolloff", "Highlight Energy Rolloff", 0.72, 0.0, 1.0, 3, page);
        defineDoubleParam(paramSet, "shadowRecovery", "Shadow Reconstruction", 0.28, 0.0, 1.0, 4, page);
        defineDoubleParam(paramSet, "recoveredHeadroomStops", "Recovered Headroom Stops", 2.0, 0.0, 4.0, 5, page);
        defineDoubleParam(paramSet, "localDetail", "Recovered Local Detail", 0.28, 0.0, 1.0, 6, page);
        defineDoubleParam(paramSet, "shadowDenoise", "Shadow Denoise", 0.35, 0.0, 1.0, 7, page);
        defineDoubleParam(paramSet, "dequantization", "Dequantization", 0.22, 0.0, 1.0, 8, page);
        defineDoubleParam(paramSet, "temporalConsistency", "Near Temporal Consistency", 0.55, 0.0, 1.0, 9, page);
        defineDoubleParam(paramSet, "longTermConsistency", "Long Temporal Consistency", 0.30, 0.0, 1.0, 10, page);
        defineChoiceParam(paramSet, "temporalOffset", "Temporal Radius", 0, temporalOffsets, 3, 11, page);
        defineDoubleParam(paramSet, "colorGuard", "Color Gamut Guard", 0.80, 0.0, 1.0, 12, page);
        defineBooleanParam(paramSet, "useMLCache", "Use ML Cache", 0, 13, page);
        defineDirectoryParam(paramSet, "cacheDirectory", "ML Cache Directory", 14, page);
        defineDoubleParam(paramSet, "cacheFrameOffset", "Cache Frame Offset", 0.0, -100000.0, 100000.0, 15, page);
        defineDoubleParam(paramSet, "mlCacheBlend", "ML Cache Blend", 0.85, 0.0, 1.0, 16, page);
        defineDoubleParam(paramSet, "mlConfidenceThreshold", "ML Confidence Threshold", 0.20, 0.0, 1.0, 17, page);
        defineDoubleParam(paramSet, "outputMix", "Output Mix", 1.0, 0.0, 1.0, 18, page);
        defineBooleanParam(paramSet, "adjustmentLayerGuard", "Adjustment Layer Guard", 1, 19, page);
    } else {
        gPropHost->propSetString(page, kOfxPropLabel, 0, "Temporal Integrity");
        const char* views[] = {
            "Safe Repair",
            "Temporal Error Matte",
            "Difference Overlay",
            "Motion Protection Matte",
            "Repair Confidence Matte",
            "Ghost Risk Matte",
        };
        defineChoiceParam(paramSet, "viewMode", "View", 0, views, 6, 0, page);
        defineDoubleParam(paramSet, "sensitivity", "Detection Sensitivity", 1.0, 0.25, 3.0, 1, page);
        defineDoubleParam(paramSet, "repairStrength", "Repair Strength", 0.42, 0.0, 1.0, 2, page);
        defineDoubleParam(paramSet, "flickerRepair", "Flicker Repair", 0.55, 0.0, 1.0, 3, page);
        defineDoubleParam(paramSet, "textureStability", "Texture Stability", 0.32, 0.0, 1.0, 4, page);
        defineDoubleParam(paramSet, "longTermStability", "Long-Term Stability", 0.30, 0.0, 1.0, 5, page);
        defineDoubleParam(paramSet, "motionGuard", "Motion Guard", 0.88, 0.0, 1.0, 6, page);
        defineDoubleParam(paramSet, "ghostGuard", "Ghost Guard", 0.90, 0.0, 1.0, 7, page);
        defineDoubleParam(paramSet, "edgeProtection", "Edge Protection", 0.82, 0.0, 1.0, 8, page);
        defineDoubleParam(paramSet, "chromaStability", "Chroma Stability", 0.28, 0.0, 1.0, 9, page);
        defineChoiceParam(paramSet, "temporalOffset", "Frame Radius", 0, temporalOffsets, 3, 10, page);
        defineDoubleParam(paramSet, "overlayStrength", "Overlay Strength", 0.72, 0.0, 1.0, 11, page);
        defineDoubleParam(paramSet, "outputMix", "Output Mix", 1.0, 0.0, 1.0, 12, page);
        defineBooleanParam(paramSet, "adjustmentLayerGuard", "Adjustment Layer Guard", 1, 13, page);
    }
    return kOfxStatOK;
}

const char* pluginLabel(PluginKind kind)
{
    switch (kind) {
    case PluginFrameDirector:
        return "Buckswood Frame Director v2.1";
    case PluginRadianceRecover:
        return "Buckswood Radiance Recover v2.1";
    default:
        return "Buckswood Temporal Integrity v2.1";
    }
}

OfxStatus describe(PluginKind kind, OfxImageEffectHandle effect)
{
    OfxPropertySetHandle properties = nullptr;
    gEffectHost->getPropertySet(effect, &properties);
    gPropHost->propSetInt(properties, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 0);
    gPropHost->propSetString(properties, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gPropHost->propSetString(properties, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthByte);
    gPropHost->propSetInt(properties, kOfxImageEffectPropTemporalClipAccess, 0, 1);
    gPropHost->propSetString(properties, kOfxPropLabel, 0, pluginLabel(kind));
    gPropHost->propSetString(properties, kOfxImageEffectPluginPropGrouping, 0, "Buckswood");
    gPropHost->propSetString(properties, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    return kOfxStatOK;
}

OfxStatus onLoad()
{
    if (!gHost) {
        return kOfxStatErrMissingHostFeature;
    }
    gEffectHost = const_cast<OfxImageEffectSuiteV1*>(
        reinterpret_cast<const OfxImageEffectSuiteV1*>(
            gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1)));
    gPropHost = const_cast<OfxPropertySuiteV1*>(
        reinterpret_cast<const OfxPropertySuiteV1*>(
            gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1)));
    gParamHost = const_cast<OfxParameterSuiteV1*>(
        reinterpret_cast<const OfxParameterSuiteV1*>(
            gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1)));
    gThreadHost = const_cast<OfxMultiThreadSuiteV1*>(
        reinterpret_cast<const OfxMultiThreadSuiteV1*>(
            gHost->fetchSuite(gHost->host, kOfxMultiThreadSuite, 1)));
    return gEffectHost && gPropHost && gParamHost ? kOfxStatOK : kOfxStatErrMissingHostFeature;
}

OfxStatus getFramesNeeded(
    OfxImageEffectHandle instance,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle outArgs)
{
    OfxTime time = 0.0;
    gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    const int temporalOffset = std::max(1, intParamAtTime(instance, "temporalOffset", time, 0) + 1);
    const double ranges[4] = {
        time - temporalOffset * 2,
        time - temporalOffset,
        time + temporalOffset,
        time + temporalOffset * 2,
    };
    gPropHost->propSetDoubleN(outArgs, kSourceFrameRangeProp, 4, ranges);
    return kOfxStatOK;
}

OfxStatus pluginMain(
    PluginKind kind,
    const char* action,
    const void* handle,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle outArgs)
{
    try {
        auto effect = reinterpret_cast<OfxImageEffectHandle>(const_cast<void*>(handle));
        if (std::strcmp(action, kOfxActionLoad) == 0) {
            return onLoad();
        }
        if (std::strcmp(action, kOfxActionDescribe) == 0) {
            return describe(kind, effect);
        }
        if (std::strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
            return describeInContext(kind, effect);
        }
        if (std::strcmp(action, kOfxImageEffectActionRender) == 0) {
            return render(kind, effect, inArgs);
        }
        if (std::strcmp(action, kOfxImageEffectActionGetFramesNeeded) == 0) {
            return getFramesNeeded(effect, inArgs, outArgs);
        }
        if (std::strcmp(action, kOfxActionCreateInstance) == 0 ||
            std::strcmp(action, kOfxActionDestroyInstance) == 0) {
            return kOfxStatOK;
        }
    } catch (std::bad_alloc&) {
        return kOfxStatErrMemory;
    } catch (std::exception&) {
        return kOfxStatErrUnknown;
    }
    return kOfxStatReplyDefault;
}

OfxStatus frameDirectorMain(
    const char* action,
    const void* handle,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle outArgs)
{
    return pluginMain(PluginFrameDirector, action, handle, inArgs, outArgs);
}

OfxStatus radianceRecoverMain(
    const char* action,
    const void* handle,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle outArgs)
{
    return pluginMain(PluginRadianceRecover, action, handle, inArgs, outArgs);
}

OfxStatus temporalIntegrityMain(
    const char* action,
    const void* handle,
    OfxPropertySetHandle inArgs,
    OfxPropertySetHandle outArgs)
{
    return pluginMain(PluginTemporalIntegrity, action, handle, inArgs, outArgs);
}

void setHost(OfxHost* host)
{
    gHost = host;
}

OfxPlugin plugins[] = {
    {
        kOfxImageEffectPluginApi,
        1,
        "com.buckswood.frame.director",
        kPluginMajorVersion,
        kPluginMinorVersion,
        setHost,
        frameDirectorMain,
    },
    {
        kOfxImageEffectPluginApi,
        1,
        "com.buckswood.radiance.recover",
        kPluginMajorVersion,
        kPluginMinorVersion,
        setHost,
        radianceRecoverMain,
    },
    {
        kOfxImageEffectPluginApi,
        1,
        "com.buckswood.temporal.integrity",
        kPluginMajorVersion,
        kPluginMinorVersion,
        setHost,
        temporalIntegrityMain,
    },
};

} // namespace

extern "C" {

EXPORT OfxPlugin* OfxGetPlugin(int nth)
{
    return nth >= 0 && nth < 3 ? &plugins[nth] : nullptr;
}

EXPORT int OfxGetNumberOfPlugins()
{
    return 3;
}

} // extern "C"
