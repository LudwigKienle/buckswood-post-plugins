#include "LookDNACore.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace buckswood_lookdna {
namespace {

constexpr std::array<float, 7> kQuantilePositions{
    0.01f, 0.05f, 0.25f, 0.50f, 0.75f, 0.95f, 0.99f,
};
constexpr std::size_t kProfileFloatCount = 35;
constexpr char kProfileMagic[8] = {'B', 'W', 'L', 'O', 'O', 'K', '1', '\0'};

struct AnalysisSample {
    Pixel pixel;
    float y;
    float u;
    float v;
    float saturation;
    float localContrast;
    float grain;
};

struct Matrix2 {
    float m00;
    float m01;
    float m10;
    float m11;
};

class RegionSampler final : public Sampler {
public:
    RegionSampler(const Sampler& source, Bounds region)
        : source_(source)
        , region_(region)
    {
    }

    Pixel sample(float x, float y) const override
    {
        return source_.sample(x, y);
    }

    Bounds bounds() const override
    {
        return region_;
    }

private:
    const Sampler& source_;
    Bounds region_;
};

Matrix2 multiply(Matrix2 a, Matrix2 b)
{
    return Matrix2{
        a.m00 * b.m00 + a.m01 * b.m10,
        a.m00 * b.m01 + a.m01 * b.m11,
        a.m10 * b.m00 + a.m11 * b.m10,
        a.m10 * b.m01 + a.m11 * b.m11,
    };
}

Matrix2 symmetricSqrt(float a, float b, float d)
{
    a = std::max(a, 0.00002f);
    d = std::max(d, 0.00002f);
    const float determinant = std::max(0.000000001f, a * d - b * b);
    const float s = std::sqrt(determinant);
    const float t = std::sqrt(std::max(0.000001f, a + d + 2.0f * s));
    return Matrix2{(a + s) / t, b / t, b / t, (d + s) / t};
}

Matrix2 inverse(Matrix2 matrix)
{
    const float determinant = matrix.m00 * matrix.m11 - matrix.m01 * matrix.m10;
    if (std::fabs(determinant) < 0.000001f) {
        return Matrix2{1.0f, 0.0f, 0.0f, 1.0f};
    }
    const float scale = 1.0f / determinant;
    return Matrix2{
        matrix.m11 * scale,
        -matrix.m01 * scale,
        -matrix.m10 * scale,
        matrix.m00 * scale,
    };
}

float percentile(const std::vector<float>& sorted, float position)
{
    if (sorted.empty()) {
        return 0.0f;
    }
    const float index = position * static_cast<float>(sorted.size() - 1);
    const std::size_t low = static_cast<std::size_t>(std::floor(index));
    const std::size_t high = std::min(sorted.size() - 1, low + 1);
    const float amount = index - static_cast<float>(low);
    return sorted[low] + (sorted[high] - sorted[low]) * amount;
}

void writeU32(std::ofstream& stream, std::uint32_t value)
{
    const unsigned char bytes[4] = {
        static_cast<unsigned char>(value & 0xffu),
        static_cast<unsigned char>((value >> 8u) & 0xffu),
        static_cast<unsigned char>((value >> 16u) & 0xffu),
        static_cast<unsigned char>((value >> 24u) & 0xffu),
    };
    stream.write(reinterpret_cast<const char*>(bytes), 4);
}

bool readU32(std::ifstream& stream, std::uint32_t& value)
{
    unsigned char bytes[4]{};
    stream.read(reinterpret_cast<char*>(bytes), 4);
    if (!stream) {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8u) |
        (static_cast<std::uint32_t>(bytes[2]) << 16u) |
        (static_cast<std::uint32_t>(bytes[3]) << 24u);
    return true;
}

void writeFloat(std::ofstream& stream, float value)
{
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float size mismatch");
    std::memcpy(&bits, &value, sizeof(bits));
    writeU32(stream, bits);
}

bool readFloat(std::ifstream& stream, float& value)
{
    std::uint32_t bits = 0;
    if (!readU32(stream, bits)) {
        return false;
    }
    std::memcpy(&value, &bits, sizeof(value));
    return std::isfinite(value);
}

std::array<float, kProfileFloatCount> flattenProfile(const LookProfile& profile)
{
    std::array<float, kProfileFloatCount> values{};
    std::size_t index = 0;
    for (float value : profile.lumaQuantiles) {
        values[index++] = value;
    }
    values[index++] = profile.lumaMean;
    values[index++] = profile.lumaStd;
    values[index++] = profile.saturationMean;
    values[index++] = profile.localContrast;
    values[index++] = profile.grainEstimate;
    values[index++] = profile.chromaMeanU;
    values[index++] = profile.chromaMeanV;
    values[index++] = profile.chromaCovUU;
    values[index++] = profile.chromaCovUV;
    values[index++] = profile.chromaCovVV;
    for (const SemanticZone& zone : profile.zones) {
        values[index++] = zone.meanU;
        values[index++] = zone.meanV;
        values[index++] = zone.weight;
    }
    return values;
}

bool expandProfile(
    const std::array<float, kProfileFloatCount>& values,
    std::uint32_t sampleCount,
    LookProfile& profile)
{
    std::size_t index = 0;
    profile = LookProfile{};
    for (float& value : profile.lumaQuantiles) {
        value = values[index++];
    }
    profile.lumaMean = values[index++];
    profile.lumaStd = values[index++];
    profile.saturationMean = values[index++];
    profile.localContrast = values[index++];
    profile.grainEstimate = values[index++];
    profile.chromaMeanU = values[index++];
    profile.chromaMeanV = values[index++];
    profile.chromaCovUU = values[index++];
    profile.chromaCovUV = values[index++];
    profile.chromaCovVV = values[index++];
    for (SemanticZone& zone : profile.zones) {
        zone.meanU = values[index++];
        zone.meanV = values[index++];
        zone.weight = values[index++];
    }
    profile.sampleCount = static_cast<int>(sampleCount);
    profile.valid = profile.sampleCount > 8 &&
        profile.lumaQuantiles[6] >= profile.lumaQuantiles[0];
    return profile.valid;
}

LookProfile weightedProfiles(
    const LookProfile& current,
    const LookProfile* previous,
    const LookProfile* next,
    float previousWeight,
    float nextWeight)
{
    const auto currentValues = flattenProfile(current);
    std::array<float, kProfileFloatCount> result = currentValues;
    float weightSum = 1.0f;
    if (previous && previous->valid && previousWeight > 0.0f) {
        const auto values = flattenProfile(*previous);
        for (std::size_t index = 0; index < result.size(); ++index) {
            result[index] += values[index] * previousWeight;
        }
        weightSum += previousWeight;
    }
    if (next && next->valid && nextWeight > 0.0f) {
        const auto values = flattenProfile(*next);
        for (std::size_t index = 0; index < result.size(); ++index) {
            result[index] += values[index] * nextWeight;
        }
        weightSum += nextWeight;
    }
    for (float& value : result) {
        value /= weightSum;
    }
    LookProfile profile;
    expandProfile(result, static_cast<std::uint32_t>(current.sampleCount), profile);
    return profile;
}

LookProfile weightedProfileList(
    const LookProfile& fallback,
    const std::array<const LookProfile*, 5>& profiles,
    const std::array<float, 5>& weights)
{
    std::array<float, kProfileFloatCount> result{};
    float weightSum = 0.0f;
    float sampleSum = 0.0f;
    for (std::size_t profileIndex = 0; profileIndex < profiles.size(); ++profileIndex) {
        const LookProfile* profile = profiles[profileIndex];
        const float weight = std::max(0.0f, weights[profileIndex]);
        if (!profile || !profile->valid || weight <= 0.0f) {
            continue;
        }
        const auto values = flattenProfile(*profile);
        for (std::size_t valueIndex = 0; valueIndex < result.size(); ++valueIndex) {
            result[valueIndex] += values[valueIndex] * weight;
        }
        weightSum += weight;
        sampleSum += static_cast<float>(profile->sampleCount) * weight;
    }
    if (weightSum <= 0.000001f) {
        return fallback;
    }
    for (float& value : result) {
        value /= weightSum;
    }
    LookProfile profile;
    expandProfile(
        result,
        static_cast<std::uint32_t>(std::max(16.0f, sampleSum / weightSum)),
        profile);
    for (std::size_t index = 1; index < profile.lumaQuantiles.size(); ++index) {
        profile.lumaQuantiles[index] =
            std::max(profile.lumaQuantiles[index], profile.lumaQuantiles[index - 1] + 0.0001f);
    }
    return profile;
}

MatchContext weightedMatchList(
    const std::array<const MatchContext*, 5>& matches,
    const std::array<float, 5>& weights,
    const MatchContext& fallback)
{
    std::array<const LookProfile*, 5> sources{};
    std::array<const LookProfile*, 5> references{};
    float validWeight = 0.0f;
    for (std::size_t index = 0; index < matches.size(); ++index) {
        if (matches[index] && matches[index]->valid && weights[index] > 0.0f) {
            sources[index] = &matches[index]->source;
            references[index] = &matches[index]->reference;
            validWeight += weights[index];
        }
    }
    if (validWeight <= 0.000001f) {
        return fallback;
    }

    MatchContext result;
    result.source = weightedProfileList(fallback.source, sources, weights);
    result.reference = weightedProfileList(fallback.reference, references, weights);
    result.chromaM00 = 0.0f;
    result.chromaM11 = 0.0f;
    result.referenceWeights = {0.0f, 0.0f, 0.0f};
    result.temporalConfidence = 0.0f;
    for (std::size_t index = 0; index < matches.size(); ++index) {
        if (!matches[index] || !matches[index]->valid || weights[index] <= 0.0f) {
            continue;
        }
        const float weight = weights[index] / validWeight;
        result.chromaM00 += matches[index]->chromaM00 * weight;
        result.chromaM01 += matches[index]->chromaM01 * weight;
        result.chromaM10 += matches[index]->chromaM10 * weight;
        result.chromaM11 += matches[index]->chromaM11 * weight;
        result.confidence += matches[index]->confidence * weight;
        result.guardedStrength += matches[index]->guardedStrength * weight;
        result.temporalConfidence += matches[index]->temporalConfidence * weight;
        result.spatialInfluence += matches[index]->spatialInfluence * weight;
        for (std::size_t referenceIndex = 0; referenceIndex < result.referenceWeights.size(); ++referenceIndex) {
            result.referenceWeights[referenceIndex] +=
                matches[index]->referenceWeights[referenceIndex] * weight;
        }
    }
    float referenceWeightSum = 0.0f;
    for (float value : result.referenceWeights) {
        referenceWeightSum += value;
    }
    if (referenceWeightSum > 0.0001f) {
        for (float& value : result.referenceWeights) {
            value /= referenceWeightSum;
        }
    } else {
        result.referenceWeights = fallback.referenceWeights;
    }
    result.valid = result.source.valid && result.reference.valid;
    return result.valid ? result : fallback;
}

} // namespace

float LookDNACore::clamp(float value, float low, float high)
{
    return std::min(high, std::max(low, value));
}

float LookDNACore::clamp01(float value)
{
    return clamp(value, 0.0f, 1.0f);
}

float LookDNACore::smoothstep(float edge0, float edge1, float value)
{
    const float amount = clamp01((value - edge0) / std::max(0.000001f, edge1 - edge0));
    return amount * amount * (3.0f - 2.0f * amount);
}

float LookDNACore::luma(Pixel pixel)
{
    return 0.2627f * pixel.r + 0.6780f * pixel.g + 0.0593f * pixel.b;
}

Pixel LookDNACore::mix(Pixel a, Pixel b, float amount)
{
    amount = clamp01(amount);
    return Pixel{
        a.r + (b.r - a.r) * amount,
        a.g + (b.g - a.g) * amount,
        a.b + (b.b - a.b) * amount,
        a.a + (b.a - a.a) * amount,
    };
}

float LookDNACore::decodeTransfer(float value, int colorSpace)
{
    const float v = clamp(value, 0.0f, 64.0f);
    switch (colorSpace) {
    case Rec709Gamma24:
        return std::pow(v, 2.4f);
    case SRGBDisplay:
        return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
    case ArriLogC3:
        return std::exp2((v - 0.391f) * 6.65f) * 0.18f;
    case ArriLogC4:
        return std::exp2((v - 0.278f) * 7.85f) * 0.18f;
    case SonySLog3:
        return std::exp2((v - 0.410557f) * 7.5f) * 0.18f;
    case AppleLog:
        return std::exp2((v - 0.405f) * 7.05f) * 0.18f;
    case BmdFilmGen5:
        return std::exp2((v - 0.380f) * 6.45f) * 0.18f;
    case SceneLinear:
    case TimelineDisplay:
    default:
        return v;
    }
}

float LookDNACore::encodeTransfer(float value, int colorSpace)
{
    const float v = std::max(0.0f, value);
    const float logValue = std::log2(std::max(0.000001f, v / 0.18f));
    switch (colorSpace) {
    case Rec709Gamma24:
        return std::pow(v, 1.0f / 2.4f);
    case SRGBDisplay:
        return v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
    case ArriLogC3:
        return logValue / 6.65f + 0.391f;
    case ArriLogC4:
        return logValue / 7.85f + 0.278f;
    case SonySLog3:
        return logValue / 7.5f + 0.410557f;
    case AppleLog:
        return logValue / 7.05f + 0.405f;
    case BmdFilmGen5:
        return logValue / 6.45f + 0.380f;
    case SceneLinear:
    case TimelineDisplay:
    default:
        return v;
    }
}

float LookDNACore::linearToWorking(float value)
{
    return std::log2(1.0f + 16.0f * std::max(0.0f, value)) / std::log2(17.0f);
}

float LookDNACore::workingToLinear(float value)
{
    return (std::exp2(std::max(0.0f, value) * std::log2(17.0f)) - 1.0f) / 16.0f;
}

Pixel LookDNACore::encodedToWorking(Pixel pixel, int colorSpace)
{
    return Pixel{
        linearToWorking(decodeTransfer(pixel.r, colorSpace)),
        linearToWorking(decodeTransfer(pixel.g, colorSpace)),
        linearToWorking(decodeTransfer(pixel.b, colorSpace)),
        pixel.a,
    };
}

Pixel LookDNACore::workingToEncoded(Pixel pixel, int colorSpace)
{
    return Pixel{
        clamp(encodeTransfer(workingToLinear(pixel.r), colorSpace), 0.0f, 64.0f),
        clamp(encodeTransfer(workingToLinear(pixel.g), colorSpace), 0.0f, 64.0f),
        clamp(encodeTransfer(workingToLinear(pixel.b), colorSpace), 0.0f, 64.0f),
        clamp01(pixel.a),
    };
}

float LookDNACore::hueDegrees(Pixel pixel)
{
    const float maximum = std::max(pixel.r, std::max(pixel.g, pixel.b));
    const float minimum = std::min(pixel.r, std::min(pixel.g, pixel.b));
    const float delta = maximum - minimum;
    if (delta < 0.000001f) {
        return 0.0f;
    }
    float hue = 0.0f;
    if (maximum == pixel.r) {
        hue = 60.0f * std::fmod((pixel.g - pixel.b) / delta, 6.0f);
    } else if (maximum == pixel.g) {
        hue = 60.0f * ((pixel.b - pixel.r) / delta + 2.0f);
    } else {
        hue = 60.0f * ((pixel.r - pixel.g) / delta + 4.0f);
    }
    return hue < 0.0f ? hue + 360.0f : hue;
}

float LookDNACore::saturation(Pixel pixel)
{
    const float maximum = std::max(pixel.r, std::max(pixel.g, pixel.b));
    const float minimum = std::min(pixel.r, std::min(pixel.g, pixel.b));
    return maximum > 0.000001f ? (maximum - minimum) / maximum : 0.0f;
}

float LookDNACore::skinMask(Pixel pixel)
{
    const float hue = hueDegrees(pixel);
    const float sat = saturation(pixel);
    const float y = luma(pixel);
    const float hueMask =
        smoothstep(350.0f, 360.0f, hue) +
        (1.0f - smoothstep(18.0f, 62.0f, hue)) * smoothstep(0.0f, 18.0f, hue);
    const float saturationMask = smoothstep(0.055f, 0.16f, sat) * (1.0f - smoothstep(0.72f, 0.96f, sat));
    const float lumaMask = smoothstep(0.07f, 0.18f, y) * (1.0f - smoothstep(0.88f, 1.08f, y));
    const float redDominance = smoothstep(-0.02f, 0.08f, pixel.r - pixel.b);
    return clamp01(hueMask * saturationMask * lumaMask * redDominance);
}

std::array<float, SemanticZoneCount> LookDNACore::semanticMasks(
    Pixel pixel,
    const LookProfile& source)
{
    const float hue = hueDegrees(pixel);
    const float sat = saturation(pixel);
    const float y = luma(pixel);
    const float skyHue = smoothstep(165.0f, 195.0f, hue) * (1.0f - smoothstep(245.0f, 275.0f, hue));
    const float foliageHue = smoothstep(52.0f, 82.0f, hue) * (1.0f - smoothstep(150.0f, 182.0f, hue));
    const float warmHue =
        (1.0f - smoothstep(32.0f, 72.0f, hue)) + smoothstep(330.0f, 360.0f, hue);
    const float colorPresence = smoothstep(0.04f, 0.20f, sat);
    const float q25 = source.lumaQuantiles[2];
    const float q75 = source.lumaQuantiles[4];
    const float q95 = source.lumaQuantiles[5];
    return std::array<float, SemanticZoneCount>{
        skinMask(pixel),
        clamp01(skyHue * colorPresence * smoothstep(0.16f, 0.42f, y)),
        clamp01(foliageHue * colorPresence * smoothstep(0.08f, 0.30f, y)),
        clamp01(warmHue * colorPresence * smoothstep(q75, std::max(q75 + 0.02f, q95), y)),
        1.0f - smoothstep(q25 * 0.55f, std::max(q25, 0.04f), y),
        smoothstep(q75, std::max(q75 + 0.02f, q95), y),
    };
}

LookProfile LookDNACore::analyze(
    const Sampler& sampler,
    int colorSpace,
    int quality)
{
    const Bounds imageBounds = sampler.bounds();
    const int width = imageBounds.x2 - imageBounds.x1;
    const int height = imageBounds.y2 - imageBounds.y1;
    if (width < 2 || height < 2) {
        return LookProfile{};
    }

    int gridX = 72;
    int gridY = 42;
    if (quality <= 0) {
        gridX = 40;
        gridY = 24;
    } else if (quality >= 2) {
        gridX = 120;
        gridY = 68;
    }
    gridX = std::min(gridX, width);
    gridY = std::min(gridY, height);
    const float wideRadius = static_cast<float>(std::max(2, std::min(width, height) / 180));

    std::vector<AnalysisSample> samples;
    samples.reserve(static_cast<std::size_t>(gridX * gridY));
    std::vector<float> lumaValues;
    lumaValues.reserve(static_cast<std::size_t>(gridX * gridY));

    auto workingSample = [&](float x, float y) {
        return encodedToWorking(sampler.sample(x, y), colorSpace);
    };

    for (int gy = 0; gy < gridY; ++gy) {
        const float y = static_cast<float>(imageBounds.y1) +
            (static_cast<float>(gy) + 0.5f) * static_cast<float>(height) / static_cast<float>(gridY);
        for (int gx = 0; gx < gridX; ++gx) {
            const float x = static_cast<float>(imageBounds.x1) +
                (static_cast<float>(gx) + 0.5f) * static_cast<float>(width) / static_cast<float>(gridX);
            const Pixel encoded = sampler.sample(x, y);
            if (encoded.a < 0.02f) {
                continue;
            }
            const Pixel center = encodedToWorking(encoded, colorSpace);
            const float centerY = luma(center);
            const Pixel left = workingSample(x - wideRadius, y);
            const Pixel right = workingSample(x + wideRadius, y);
            const Pixel up = workingSample(x, y - wideRadius);
            const Pixel down = workingSample(x, y + wideRadius);
            const float wideAverage = (luma(left) + luma(right) + luma(up) + luma(down)) * 0.25f;

            const Pixel left1 = workingSample(x - 1.0f, y);
            const Pixel right1 = workingSample(x + 1.0f, y);
            const Pixel up1 = workingSample(x, y - 1.0f);
            const Pixel down1 = workingSample(x, y + 1.0f);
            const float nearAverage = (luma(left1) + luma(right1) + luma(up1) + luma(down1)) * 0.25f;
            const float nearGradient =
                (std::fabs(luma(right1) - luma(left1)) + std::fabs(luma(down1) - luma(up1))) * 0.25f;
            const float grain = std::max(0.0f, std::fabs(centerY - nearAverage) - nearGradient * 0.42f);
            const float u = center.r - centerY;
            const float v = center.b - centerY;
            samples.push_back(AnalysisSample{
                center,
                centerY,
                u,
                v,
                saturation(center),
                std::fabs(centerY - wideAverage),
                grain,
            });
            lumaValues.push_back(centerY);
        }
    }

    if (samples.size() < 16) {
        return LookProfile{};
    }
    std::sort(lumaValues.begin(), lumaValues.end());

    LookProfile profile;
    profile.valid = true;
    profile.sampleCount = static_cast<int>(samples.size());
    for (std::size_t index = 0; index < kQuantilePositions.size(); ++index) {
        profile.lumaQuantiles[index] = percentile(lumaValues, kQuantilePositions[index]);
    }

    double sumY = 0.0;
    double sumY2 = 0.0;
    double sumSaturation = 0.0;
    double sumLocal = 0.0;
    double sumGrain = 0.0;
    double sumU = 0.0;
    double sumV = 0.0;
    double sumUU = 0.0;
    double sumUV = 0.0;
    double sumVV = 0.0;
    std::array<double, SemanticZoneCount> zoneWeights{};
    std::array<double, SemanticZoneCount> zoneU{};
    std::array<double, SemanticZoneCount> zoneV{};

    for (const AnalysisSample& sample : samples) {
        sumY += sample.y;
        sumY2 += sample.y * sample.y;
        sumSaturation += sample.saturation;
        sumLocal += sample.localContrast;
        sumGrain += sample.grain;
        sumU += sample.u;
        sumV += sample.v;
        sumUU += sample.u * sample.u;
        sumUV += sample.u * sample.v;
        sumVV += sample.v * sample.v;
        const auto masks = semanticMasks(sample.pixel, profile);
        for (int zone = 0; zone < SemanticZoneCount; ++zone) {
            const double weight = masks[zone];
            zoneWeights[zone] += weight;
            zoneU[zone] += sample.u * weight;
            zoneV[zone] += sample.v * weight;
        }
    }

    const double count = static_cast<double>(samples.size());
    profile.lumaMean = static_cast<float>(sumY / count);
    profile.lumaStd = static_cast<float>(std::sqrt(std::max(0.0, sumY2 / count - (sumY / count) * (sumY / count))));
    profile.saturationMean = static_cast<float>(sumSaturation / count);
    profile.localContrast = static_cast<float>(sumLocal / count);
    profile.grainEstimate = static_cast<float>(sumGrain / count);
    profile.chromaMeanU = static_cast<float>(sumU / count);
    profile.chromaMeanV = static_cast<float>(sumV / count);
    profile.chromaCovUU = static_cast<float>(std::max(0.00002, sumUU / count - (sumU / count) * (sumU / count)));
    profile.chromaCovUV = static_cast<float>(sumUV / count - (sumU / count) * (sumV / count));
    profile.chromaCovVV = static_cast<float>(std::max(0.00002, sumVV / count - (sumV / count) * (sumV / count)));

    for (int zone = 0; zone < SemanticZoneCount; ++zone) {
        const double weight = zoneWeights[zone];
        profile.zones[zone].weight = static_cast<float>(weight / count);
        if (weight > 0.0001) {
            profile.zones[zone].meanU = static_cast<float>(zoneU[zone] / weight);
            profile.zones[zone].meanV = static_cast<float>(zoneV[zone] / weight);
        }
    }
    return profile;
}

ProfileGrid LookDNACore::analyzeGrid(
    const Sampler& sampler,
    int colorSpace,
    int quality)
{
    ProfileGrid grid;
    const Bounds bounds = sampler.bounds();
    const int width = bounds.x2 - bounds.x1;
    const int height = bounds.y2 - bounds.y1;
    if (width < 48 || height < 32) {
        return grid;
    }

    const LookProfile global = analyze(sampler, colorSpace, quality);
    if (!global.valid) {
        return grid;
    }
    const int overlapX = std::max(2, width / 24);
    const int overlapY = std::max(2, height / 24);
    int validCells = 0;
    for (int row = 0; row < SpatialGridSize; ++row) {
        for (int column = 0; column < SpatialGridSize; ++column) {
            Bounds region{
                bounds.x1 + width * column / SpatialGridSize - (column > 0 ? overlapX : 0),
                bounds.y1 + height * row / SpatialGridSize - (row > 0 ? overlapY : 0),
                bounds.x1 + width * (column + 1) / SpatialGridSize +
                    (column + 1 < SpatialGridSize ? overlapX : 0),
                bounds.y1 + height * (row + 1) / SpatialGridSize +
                    (row + 1 < SpatialGridSize ? overlapY : 0),
            };
            region.x1 = std::max(bounds.x1, region.x1);
            region.y1 = std::max(bounds.y1, region.y1);
            region.x2 = std::min(bounds.x2, region.x2);
            region.y2 = std::min(bounds.y2, region.y2);
            RegionSampler regionSampler(sampler, region);
            LookProfile profile = analyze(regionSampler, colorSpace, std::max(0, quality - 1));
            if (profile.valid) {
                ++validCells;
            } else {
                profile = global;
            }
            grid.cells[static_cast<std::size_t>(row * SpatialGridSize + column)] = profile;
        }
    }
    grid.valid = validCells >= 6;
    return grid;
}

float LookDNACore::profileDistance(const LookProfile& a, const LookProfile& b)
{
    if (!a.valid || !b.valid) {
        return 1.0f;
    }
    float quantileDistance = 0.0f;
    for (std::size_t index = 0; index < a.lumaQuantiles.size(); ++index) {
        quantileDistance += std::fabs(a.lumaQuantiles[index] - b.lumaQuantiles[index]);
    }
    quantileDistance /= static_cast<float>(a.lumaQuantiles.size());
    const float chromaDistance = std::sqrt(
        (a.chromaMeanU - b.chromaMeanU) * (a.chromaMeanU - b.chromaMeanU) +
        (a.chromaMeanV - b.chromaMeanV) * (a.chromaMeanV - b.chromaMeanV));
    const float contrastDistance = std::fabs(a.lumaStd - b.lumaStd) +
        std::fabs(a.localContrast - b.localContrast) * 2.0f;
    return clamp(quantileDistance * 1.8f + chromaDistance * 1.4f + contrastDistance, 0.0f, 1.0f);
}

LookProfile LookDNACore::stabilizeProfile(
    const LookProfile& current,
    const LookProfile* previous,
    const LookProfile* next,
    float temporalStability)
{
    if (!current.valid || temporalStability <= 0.0001f) {
        return current;
    }
    float previousWeight = 0.0f;
    float nextWeight = 0.0f;
    if (previous && previous->valid) {
        const float cutGuard = 1.0f - smoothstep(0.16f, 0.46f, profileDistance(current, *previous));
        previousWeight = temporalStability * 0.65f * cutGuard;
    }
    if (next && next->valid) {
        const float cutGuard = 1.0f - smoothstep(0.16f, 0.46f, profileDistance(current, *next));
        nextWeight = temporalStability * 0.65f * cutGuard;
    }
    return weightedProfiles(current, previous, next, previousWeight, nextWeight);
}

LookProfile LookDNACore::stabilizeProfile5(
    const LookProfile& current,
    const LookProfile* previous2,
    const LookProfile* previous1,
    const LookProfile* next1,
    const LookProfile* next2,
    float temporalStability,
    float* temporalConfidence)
{
    if (temporalConfidence) {
        *temporalConfidence = 1.0f;
    }
    if (!current.valid || temporalStability <= 0.0001f) {
        return current;
    }

    const std::array<const LookProfile*, 5> profiles{
        &current,
        previous1,
        next1,
        previous2,
        next2,
    };
    std::array<float, 5> weights{1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float guardSum = 0.0f;
    float guardCount = 0.0f;
    for (std::size_t index = 1; index < profiles.size(); ++index) {
        if (!profiles[index] || !profiles[index]->valid) {
            continue;
        }
        const float guard = 1.0f - smoothstep(
            0.14f,
            0.42f,
            profileDistance(current, *profiles[index]));
        const bool nearFrame = index <= 2;
        const float distanceWeight = nearFrame ? 0.58f : 0.30f;
        weights[index] = clamp01(temporalStability) * distanceWeight * guard;
        guardSum += guard * (nearFrame ? 1.0f : 0.55f);
        guardCount += nearFrame ? 1.0f : 0.55f;
    }
    if (temporalConfidence && guardCount > 0.0f) {
        *temporalConfidence = clamp01(guardSum / guardCount);
    }
    return weightedProfileList(current, profiles, weights);
}

LookProfile LookDNACore::blendReferenceProfiles(
    const LookProfile& source,
    const std::array<const LookProfile*, 3>& references,
    const std::array<float, 3>& weights,
    float adaptivity,
    std::array<float, 3>* normalizedWeights)
{
    std::array<const LookProfile*, 5> profileList{
        references[0], references[1], references[2], nullptr, nullptr,
    };
    std::array<float, 5> effectiveWeights{};
    float weightSum = 0.0f;
    for (std::size_t index = 0; index < references.size(); ++index) {
        if (!references[index] || !references[index]->valid) {
            continue;
        }
        const float compatibility = std::exp(-profileDistance(source, *references[index]) * 3.2f);
        const float adaptiveScale =
            1.0f - clamp01(adaptivity) +
            clamp01(adaptivity) * (0.15f + compatibility * 0.85f);
        effectiveWeights[index] = std::max(0.0f, weights[index]) * adaptiveScale;
        weightSum += effectiveWeights[index];
    }
    if (normalizedWeights) {
        normalizedWeights->fill(0.0f);
        if (weightSum > 0.000001f) {
            for (std::size_t index = 0; index < references.size(); ++index) {
                (*normalizedWeights)[index] = effectiveWeights[index] / weightSum;
            }
        }
    }
    const LookProfile fallback = references[0] && references[0]->valid
        ? *references[0]
        : LookProfile{};
    return weightedProfileList(fallback, profileList, effectiveWeights);
}

MatchContext LookDNACore::buildMatch(
    const LookProfile& source,
    const LookProfile& reference,
    const Controls& controls)
{
    MatchContext match;
    match.source = source;
    match.reference = reference;
    if (!source.valid || !reference.valid) {
        return match;
    }

    const Matrix2 sourceRoot = symmetricSqrt(
        source.chromaCovUU,
        source.chromaCovUV,
        source.chromaCovVV);
    const Matrix2 referenceRoot = symmetricSqrt(
        reference.chromaCovUU,
        reference.chromaCovUV,
        reference.chromaCovVV);
    Matrix2 chroma = multiply(referenceRoot, inverse(sourceRoot));
    chroma.m00 = clamp(chroma.m00, 0.55f, 1.70f);
    chroma.m11 = clamp(chroma.m11, 0.55f, 1.70f);
    chroma.m01 = clamp(chroma.m01, -0.42f, 0.42f);
    chroma.m10 = clamp(chroma.m10, -0.42f, 0.42f);
    match.chromaM00 = chroma.m00;
    match.chromaM01 = chroma.m01;
    match.chromaM10 = chroma.m10;
    match.chromaM11 = chroma.m11;

    const float exposureScore = std::exp(-std::fabs(source.lumaQuantiles[3] - reference.lumaQuantiles[3]) * 3.2f);
    const float sourceRange = source.lumaQuantiles[5] - source.lumaQuantiles[1];
    const float referenceRange = reference.lumaQuantiles[5] - reference.lumaQuantiles[1];
    const float rangeScore = std::exp(-std::fabs(sourceRange - referenceRange) * 2.6f);
    const float chromaScore = std::exp(-std::sqrt(
        (source.chromaMeanU - reference.chromaMeanU) * (source.chromaMeanU - reference.chromaMeanU) +
        (source.chromaMeanV - reference.chromaMeanV) * (source.chromaMeanV - reference.chromaMeanV)) * 5.0f);
    float semanticScore = 0.0f;
    for (int zone = 0; zone < 4; ++zone) {
        const float sourceWeight = source.zones[zone].weight;
        const float referenceWeight = reference.zones[zone].weight;
        if (sourceWeight < 0.005f && referenceWeight < 0.005f) {
            semanticScore += 0.65f;
        } else {
            semanticScore += std::sqrt(
                std::min(sourceWeight, referenceWeight) /
                std::max(0.005f, std::max(sourceWeight, referenceWeight)));
        }
    }
    semanticScore *= 0.25f;
    match.confidence = clamp(
        0.12f + exposureScore * 0.22f + rangeScore * 0.20f + chromaScore * 0.16f + semanticScore * 0.30f,
        0.0f,
        1.0f);
    const float confidenceFloor = std::max(0.38f, match.confidence);
    const float safety = 1.0f + (confidenceFloor - 1.0f) * clamp01(controls.sceneIdentityGuard);
    match.guardedStrength = clamp01(controls.matchStrength) * safety;
    match.valid = true;
    return match;
}

MatchContext LookDNACore::sampleSpatialMatch(
    const MatchContext& globalMatch,
    const std::array<MatchContext, SpatialProfileCount>& spatialMatches,
    float normalizedX,
    float normalizedY,
    float spatialStrength)
{
    if (!globalMatch.valid || spatialStrength <= 0.0001f) {
        return globalMatch;
    }
    const float gridX = clamp01(normalizedX) * static_cast<float>(SpatialGridSize - 1);
    const float gridY = clamp01(normalizedY) * static_cast<float>(SpatialGridSize - 1);
    const int x0 = std::min(SpatialGridSize - 1, static_cast<int>(std::floor(gridX)));
    const int y0 = std::min(SpatialGridSize - 1, static_cast<int>(std::floor(gridY)));
    const int x1 = std::min(SpatialGridSize - 1, x0 + 1);
    const int y1 = std::min(SpatialGridSize - 1, y0 + 1);
    const float tx = gridX - static_cast<float>(x0);
    const float ty = gridY - static_cast<float>(y0);

    const std::array<const MatchContext*, 5> localMatches{
        &spatialMatches[static_cast<std::size_t>(y0 * SpatialGridSize + x0)],
        &spatialMatches[static_cast<std::size_t>(y0 * SpatialGridSize + x1)],
        &spatialMatches[static_cast<std::size_t>(y1 * SpatialGridSize + x0)],
        &spatialMatches[static_cast<std::size_t>(y1 * SpatialGridSize + x1)],
        nullptr,
    };
    const std::array<float, 5> localWeights{
        (1.0f - tx) * (1.0f - ty),
        tx * (1.0f - ty),
        (1.0f - tx) * ty,
        tx * ty,
        0.0f,
    };
    MatchContext local = weightedMatchList(localMatches, localWeights, globalMatch);
    local.spatialInfluence = clamp01(spatialStrength) * local.confidence;

    const float localAmount = clamp01(spatialStrength) *
        (0.35f + 0.65f * std::min(globalMatch.confidence, local.confidence));
    const std::array<const MatchContext*, 5> finalMatches{
        &globalMatch, &local, nullptr, nullptr, nullptr,
    };
    const std::array<float, 5> finalWeights{
        1.0f - localAmount, localAmount, 0.0f, 0.0f, 0.0f,
    };
    MatchContext result = weightedMatchList(finalMatches, finalWeights, globalMatch);
    result.spatialInfluence = localAmount;
    return result;
}

float LookDNACore::mapTone(float value, const MatchContext& match, float exposureLock)
{
    std::array<float, 7> target = match.reference.lumaQuantiles;
    const float exposureOffset = match.source.lumaQuantiles[3] - match.reference.lumaQuantiles[3];
    for (float& knot : target) {
        knot += exposureOffset * clamp01(exposureLock);
    }
    for (std::size_t index = 1; index < target.size(); ++index) {
        target[index] = std::max(target[index], target[index - 1] + 0.0002f);
    }

    const auto& source = match.source.lumaQuantiles;
    if (value <= source.front()) {
        return target.front() + (value - source.front());
    }
    if (value >= source.back()) {
        return target.back() + (value - source.back()) * 0.55f;
    }
    for (std::size_t index = 0; index + 1 < source.size(); ++index) {
        if (value <= source[index + 1]) {
            const float amount = (value - source[index]) /
                std::max(0.0002f, source[index + 1] - source[index]);
            const float eased = amount * amount * (3.0f - 2.0f * amount);
            return target[index] + (target[index + 1] - target[index]) * eased;
        }
    }
    return value;
}

float LookDNACore::hash(int x, int y, int frameIndex)
{
    const float value = std::sin(
        static_cast<float>(x) * 12.9898f +
        static_cast<float>(y) * 78.233f +
        static_cast<float>(frameIndex) * 37.719f) * 43758.5453f;
    return value - std::floor(value);
}

Pixel LookDNACore::processPixel(
    const Sampler& sampler,
    int x,
    int y,
    const FrameInfo& frame,
    const Controls& controls,
    const MatchContext& match,
    const Sampler* referencePreview)
{
    const Pixel sourceEncoded = sampler.sample(static_cast<float>(x), static_cast<float>(y));
    if (controls.viewMode == ReferencePreviewView) {
        if (!referencePreview) {
            return Pixel{0.08f, 0.02f, 0.03f, sourceEncoded.a};
        }
        const Bounds referenceBounds = referencePreview->bounds();
        const float nx = (static_cast<float>(x) + 0.5f) / std::max(1.0f, static_cast<float>(frame.width));
        const float ny = (static_cast<float>(y) + 0.5f) / std::max(1.0f, static_cast<float>(frame.height));
        const float rx = referenceBounds.x1 + nx * std::max(1, referenceBounds.x2 - referenceBounds.x1 - 1);
        const float ry = referenceBounds.y1 + ny * std::max(1, referenceBounds.y2 - referenceBounds.y1 - 1);
        Pixel preview = referencePreview->sample(rx, ry);
        preview.a = sourceEncoded.a;
        return preview;
    }

    if (!match.valid) {
        if (controls.viewMode == ConfidenceView) {
            return Pixel{0.30f, 0.02f, 0.02f, sourceEncoded.a};
        }
        return sourceEncoded;
    }

    const Pixel sourceWorking = encodedToWorking(sourceEncoded, controls.inputSpace);
    const float sourceY = luma(sourceWorking);
    const auto masks = semanticMasks(sourceWorking, match.source);
    const float skin = masks[SkinZone];
    const float highlight = masks[HighlightZone];
    const float shadowBand = 1.0f - smoothstep(
        match.source.lumaQuantiles[2],
        match.source.lumaQuantiles[3],
        sourceY);
    const float highlightBand = smoothstep(
        match.source.lumaQuantiles[3],
        match.source.lumaQuantiles[4],
        sourceY);
    const float midtoneBand = clamp01(1.0f - std::max(shadowBand, highlightBand));
    const float rangeTransfer = clamp01(
        shadowBand * controls.shadowMatch +
        midtoneBand * controls.midtoneMatch +
        highlightBand * controls.highlightMatch);
    const float guarded = match.guardedStrength;
    const bool applyTone = controls.matchMode == FullLook || controls.matchMode == ToneOnly;
    const bool applyColor = controls.matchMode == FullLook || controls.matchMode == ColorOnly;
    const bool applyTexture = controls.matchMode == FullLook || controls.matchMode == TextureOnly;

    Pixel tonePixel = sourceWorking;
    float mappedY = sourceY;
    if (applyTone) {
        mappedY = mapTone(sourceY, match, controls.exposureLock);
        const float highlightGuard = 1.0f - highlight * clamp01(controls.highlightProtect) * 0.72f;
        const float toneAmount = guarded * clamp01(controls.toneMatch) * highlightGuard * rangeTransfer;
        const float delta = (mappedY - sourceY) * toneAmount;
        tonePixel.r += delta;
        tonePixel.g += delta;
        tonePixel.b += delta;
    }

    Pixel matched = tonePixel;
    if (applyColor) {
        const float currentY = luma(matched);
        const float sourceU = matched.r - currentY;
        const float sourceV = matched.b - currentY;
        const float centeredU = sourceU - match.source.chromaMeanU;
        const float centeredV = sourceV - match.source.chromaMeanV;
        const float targetU = match.reference.chromaMeanU +
            match.chromaM00 * centeredU + match.chromaM01 * centeredV;
        const float targetV = match.reference.chromaMeanV +
            match.chromaM10 * centeredU + match.chromaM11 * centeredV;
        const float colorGuard =
            (1.0f - skin * clamp01(controls.skinProtect) * 0.84f) *
            (1.0f - highlight * clamp01(controls.highlightProtect) * 0.78f);
        const float colorAmount = guarded * clamp01(controls.paletteMatch) * colorGuard * rangeTransfer;
        const float u = sourceU + (targetU - sourceU) * colorAmount;
        const float v = sourceV + (targetV - sourceV) * colorAmount;
        matched.r = currentY + u;
        matched.b = currentY + v;
        matched.g = (currentY - 0.2627f * matched.r - 0.0593f * matched.b) / 0.6780f;

        float semanticU = 0.0f;
        float semanticV = 0.0f;
        float semanticWeight = 0.0f;
        for (int zone = 0; zone < SemanticZoneCount; ++zone) {
            const SemanticZone& sourceZone = match.source.zones[zone];
            const SemanticZone& referenceZone = match.reference.zones[zone];
            if (sourceZone.weight < 0.004f || referenceZone.weight < 0.004f) {
                continue;
            }
            const float overlap = std::sqrt(
                std::min(sourceZone.weight, referenceZone.weight) /
                std::max(sourceZone.weight, referenceZone.weight));
            float zoneGuard = 1.0f;
            if (zone == SkinZone) {
                zoneGuard -= clamp01(controls.skinProtect) * 0.78f;
            }
            if (zone == HighlightZone) {
                zoneGuard -= clamp01(controls.highlightProtect) * 0.72f;
            }
            const float weight = masks[zone] * overlap * zoneGuard;
            semanticU += (referenceZone.meanU - sourceZone.meanU) * weight;
            semanticV += (referenceZone.meanV - sourceZone.meanV) * weight;
            semanticWeight += weight;
        }
        if (semanticWeight > 0.0001f) {
            const float semanticAmount = guarded * clamp01(controls.semanticMatch) * rangeTransfer;
            const float du = clamp(semanticU, -0.12f, 0.12f) * semanticAmount;
            const float dv = clamp(semanticV, -0.12f, 0.12f) * semanticAmount;
            const float semanticY = luma(matched);
            const float adjustedU = matched.r - semanticY + du;
            const float adjustedV = matched.b - semanticY + dv;
            matched.r = semanticY + adjustedU;
            matched.b = semanticY + adjustedV;
            matched.g = (semanticY - 0.2627f * matched.r - 0.0593f * matched.b) / 0.6780f;
        }

        const float densityY = luma(matched);
        const float densityRatio = clamp(
            match.reference.saturationMean / std::max(0.015f, match.source.saturationMean),
            0.72f,
            1.34f);
        const float densityAmount = guarded * clamp01(controls.densityMatch) *
            rangeTransfer * colorGuard * 0.55f;
        const float densityScale = 1.0f + (densityRatio - 1.0f) * densityAmount;
        matched.r = densityY + (matched.r - densityY) * densityScale;
        matched.g = densityY + (matched.g - densityY) * densityScale;
        matched.b = densityY + (matched.b - densityY) * densityScale;
    }

    if (applyTexture) {
        const float radius = std::max(1.5f, static_cast<float>(std::min(frame.width, frame.height)) / 540.0f);
        const Pixel left = encodedToWorking(sampler.sample(x - radius, static_cast<float>(y)), controls.inputSpace);
        const Pixel right = encodedToWorking(sampler.sample(x + radius, static_cast<float>(y)), controls.inputSpace);
        const Pixel up = encodedToWorking(sampler.sample(static_cast<float>(x), y - radius), controls.inputSpace);
        const Pixel down = encodedToWorking(sampler.sample(static_cast<float>(x), y + radius), controls.inputSpace);
        const float blurY = (luma(left) + luma(right) + luma(up) + luma(down)) * 0.25f;
        const float detail = sourceY - blurY;
        const float detailRatio = clamp(
            match.reference.localContrast / std::max(0.0004f, match.source.localContrast),
            0.68f,
            1.38f);
        const float edgeGuard = 1.0f - smoothstep(0.035f, 0.16f, std::fabs(detail));
        const float detailDelta = clamp(
            detail * (detailRatio - 1.0f) * guarded * clamp01(controls.localContrastMatch) * edgeGuard,
            -0.035f,
            0.035f);
        matched.r += detailDelta;
        matched.g += detailDelta;
        matched.b += detailDelta;

        const float extraGrain = std::max(0.0f, match.reference.grainEstimate - match.source.grainEstimate);
        const float grainAmount = clamp(
            extraGrain * 1.8f * guarded * clamp01(controls.grainMatch) * clamp01(controls.textureMatch),
            0.0f,
            0.018f);
        const float noise = hash(x, y, frame.frameIndex) - 0.5f;
        const float grainTone = 1.0f - smoothstep(0.82f, 1.05f, sourceY) * 0.65f;
        matched.r += noise * grainAmount * grainTone * 1.04f;
        matched.g += noise * grainAmount * grainTone * 0.96f;
        matched.b += noise * grainAmount * grainTone * 1.01f;
    }

    matched = mix(matched, tonePixel, skin * clamp01(controls.skinProtect) * 0.62f);
    const Pixel beforeGamut = matched;
    const float gamutStrength = clamp01(controls.gamutGuard);
    auto gamutCompress = [&](float value) {
        if (value < 0.0f) {
            return value * (1.0f - gamutStrength);
        }
        if (value > 1.0f) {
            const float excess = value - 1.0f;
            const float compressed = 1.0f + excess / (1.0f + excess * (2.0f + 5.0f * gamutStrength));
            return value + (compressed - value) * gamutStrength;
        }
        return value;
    };
    matched.r = gamutCompress(matched.r);
    matched.g = gamutCompress(matched.g);
    matched.b = gamutCompress(matched.b);
    matched.a = sourceWorking.a;

    Pixel result = workingToEncoded(matched, controls.inputSpace);
    result = mix(sourceEncoded, result, clamp01(controls.outputMix));
    result.a = sourceEncoded.a;

    switch (controls.viewMode) {
    case SplitView: {
        const float normalizedX = (static_cast<float>(x) + 0.5f) /
            std::max(1.0f, static_cast<float>(frame.width));
        return normalizedX < clamp01(controls.splitPosition) ? sourceEncoded : result;
    }
    case DifferenceView:
        return Pixel{
            clamp01(0.5f + (result.r - sourceEncoded.r) * 2.0f),
            clamp01(0.5f + (result.g - sourceEncoded.g) * 2.0f),
            clamp01(0.5f + (result.b - sourceEncoded.b) * 2.0f),
            sourceEncoded.a,
        };
    case ConfidenceView:
        return Pixel{
            1.0f - match.confidence,
            match.confidence,
            0.12f + match.confidence * 0.18f,
            sourceEncoded.a,
        };
    case SkinMaskView:
        return Pixel{skin, skin, skin, sourceEncoded.a};
    case SemanticZonesView: {
        Pixel zones{0.0f, 0.0f, 0.0f, sourceEncoded.a};
        zones.r += masks[SkinZone] * 1.0f + masks[WarmLightZone] * 0.95f + masks[HighlightZone];
        zones.g += masks[FoliageZone] * 0.92f + masks[WarmLightZone] * 0.58f + masks[HighlightZone];
        zones.b += masks[SkyZone] * 1.0f + masks[ShadowZone] * 0.72f + masks[HighlightZone];
        zones.r = clamp01(zones.r);
        zones.g = clamp01(zones.g);
        zones.b = clamp01(zones.b);
        return zones;
    }
    case ToneMapView:
        return Pixel{
            clamp01(sourceY),
            clamp01(mapTone(sourceY, match, controls.exposureLock)),
            clamp01(std::fabs(mappedY - sourceY) * 5.0f),
            sourceEncoded.a,
        };
    case GamutWarningView: {
        const float violation = std::max(
            std::max(-beforeGamut.r, std::max(-beforeGamut.g, -beforeGamut.b)),
            std::max(beforeGamut.r - 1.0f, std::max(beforeGamut.g - 1.0f, beforeGamut.b - 1.0f)));
        const float warning = smoothstep(0.0f, 0.08f, violation);
        return mix(result, Pixel{1.0f, 0.03f, 0.02f, sourceEncoded.a}, warning);
    }
    case SpatialMapView: {
        const float influence = clamp01(match.spatialInfluence);
        return Pixel{
            0.08f + influence * 0.82f,
            0.12f + match.confidence * 0.72f,
            0.92f - influence * 0.58f,
            sourceEncoded.a,
        };
    }
    case ReferenceBalanceView:
        return Pixel{
            clamp01(match.referenceWeights[0]),
            clamp01(match.referenceWeights[1]),
            clamp01(match.referenceWeights[2]),
            sourceEncoded.a,
        };
    case CutGuardView:
        return Pixel{
            1.0f - clamp01(match.temporalConfidence),
            clamp01(match.temporalConfidence),
            0.08f,
            sourceEncoded.a,
        };
    case ResultView:
    default:
        return result;
    }
}

bool LookDNACore::saveProfile(
    const std::string& path,
    const LookProfile& profile,
    std::string* error)
{
    if (!profile.valid) {
        if (error) {
            *error = "Cannot save an invalid look profile";
        }
        return false;
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        if (error) {
            *error = "Could not open profile for writing";
        }
        return false;
    }
    stream.write(kProfileMagic, sizeof(kProfileMagic));
    writeU32(stream, 1u);
    writeU32(stream, static_cast<std::uint32_t>(kProfileFloatCount));
    writeU32(stream, static_cast<std::uint32_t>(std::max(0, profile.sampleCount)));
    for (float value : flattenProfile(profile)) {
        writeFloat(stream, value);
    }
    if (!stream) {
        if (error) {
            *error = "Could not finish writing profile";
        }
        return false;
    }
    return true;
}

bool LookDNACore::loadProfile(
    const std::string& path,
    LookProfile& profile,
    std::string* error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        if (error) {
            *error = "Could not open look profile";
        }
        return false;
    }
    char magic[8]{};
    stream.read(magic, sizeof(magic));
    std::uint32_t version = 0;
    std::uint32_t floatCount = 0;
    std::uint32_t sampleCount = 0;
    if (!stream || std::memcmp(magic, kProfileMagic, sizeof(magic)) != 0 ||
        !readU32(stream, version) || !readU32(stream, floatCount) || !readU32(stream, sampleCount) ||
        version != 1u || floatCount != kProfileFloatCount) {
        if (error) {
            *error = "Unsupported or corrupt BWLOOK profile";
        }
        return false;
    }
    std::array<float, kProfileFloatCount> values{};
    for (float& value : values) {
        if (!readFloat(stream, value)) {
            if (error) {
                *error = "Truncated BWLOOK profile";
            }
            return false;
        }
    }
    if (!expandProfile(values, sampleCount, profile)) {
        if (error) {
            *error = "BWLOOK profile did not contain valid analysis data";
        }
        return false;
    }
    return true;
}

} // namespace buckswood_lookdna
