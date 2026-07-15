#include "FilmEmulationCore.h"

namespace buckswood_film {

FilmEmulationCore::StockModel FilmEmulationCore::stockForPreset(int preset)
{
    switch (preset) {
    case 1: // 35mm Clean Negative
        return StockModel{1.06f, 0.98f, 0.18f, 0.18f, 0.42f, 0.010f, -0.002f, 0.000f, 0.24f, 0.12f, 0.16f, 0.00f, 0.00f, 0.05f};
    case 2: // 500T Tungsten Inspired
        return StockModel{1.10f, 1.03f, 0.26f, 0.23f, 0.48f, 0.030f, 0.004f, 0.020f, 0.34f, 0.24f, 0.34f, 0.00f, 0.00f, 0.02f};
    case 3: // 250D Daylight Inspired
        return StockModel{1.05f, 1.02f, 0.20f, 0.16f, 0.36f, 0.010f, -0.004f, -0.010f, 0.28f, 0.14f, 0.22f, 0.00f, 0.00f, 0.04f};
    case 4: // Soft Eterna Inspired
        return StockModel{0.96f, 0.88f, 0.12f, 0.24f, 0.30f, 0.004f, 0.012f, 0.010f, 0.18f, 0.10f, 0.18f, 0.00f, 0.00f, 0.24f};
    case 5: // 16mm Documentary
        return StockModel{1.15f, 0.96f, 0.32f, 0.30f, 0.55f, 0.020f, 0.008f, 0.012f, 0.30f, 0.20f, 0.62f, 0.04f, 0.00f, 0.00f};
    case 6: // Bleach Bypass
        return StockModel{1.24f, 0.72f, 0.28f, 0.18f, 0.50f, -0.004f, 0.002f, -0.006f, 0.12f, 0.08f, 0.22f, 0.72f, 0.00f, 0.00f};
    case 7: // B/W Panchromatic
        return StockModel{1.18f, 0.00f, 0.22f, 0.24f, 0.46f, 0.000f, 0.000f, 0.000f, 0.00f, 0.06f, 0.34f, 0.42f, 1.00f, 0.00f};
    case 8: // AI Footage Deplastic Film
        return StockModel{1.04f, 0.92f, 0.16f, 0.20f, 0.40f, 0.012f, -0.006f, -0.004f, 0.46f, 0.12f, 0.20f, 0.00f, 0.00f, 0.16f};
    case 9: // AI Skin Recovery Negative
        return StockModel{1.00f, 0.90f, 0.12f, 0.18f, 0.34f, 0.018f, -0.012f, -0.006f, 0.38f, 0.10f, 0.16f, 0.00f, 0.00f, 0.20f};
    case 10: // AI Dream Grain
        return StockModel{0.98f, 0.96f, 0.18f, 0.28f, 0.52f, 0.025f, 0.006f, 0.018f, 0.30f, 0.22f, 0.46f, 0.00f, 0.00f, 0.18f};
    case 11: // AI Clean Commercial Film
        return StockModel{1.03f, 0.94f, 0.10f, 0.12f, 0.26f, 0.006f, -0.004f, -0.004f, 0.26f, 0.06f, 0.08f, 0.00f, 0.00f, 0.10f};
    case 12: // Large Format Clean
        return StockModel{1.02f, 0.99f, 0.10f, 0.10f, 0.28f, 0.006f, -0.002f, 0.000f, 0.18f, 0.06f, 0.08f, 0.00f, 0.00f, 0.02f};
    case 13: // Desert Epic Negative
        return StockModel{1.08f, 0.90f, 0.18f, 0.20f, 0.48f, 0.038f, -0.014f, -0.026f, 0.34f, 0.18f, 0.18f, 0.00f, 0.00f, 0.08f};
    case 14: // Green Cyber Print Negative
        return StockModel{1.12f, 0.88f, 0.20f, 0.22f, 0.46f, -0.020f, 0.036f, -0.020f, 0.30f, 0.08f, 0.20f, 0.20f, 0.00f, 0.02f};
    case 15: // Gritty Night 500T
        return StockModel{1.18f, 0.90f, 0.34f, 0.34f, 0.58f, 0.016f, 0.000f, 0.024f, 0.34f, 0.28f, 0.48f, 0.18f, 0.00f, 0.00f};
    case 0:
    default:
        return StockModel{1.00f, 1.00f, 0.00f, 0.00f, 0.24f, 0.000f, 0.000f, 0.000f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f};
    }
}

FilmEmulationCore::PrintModel FilmEmulationCore::printForPreset(int preset)
{
    switch (preset) {
    case 1: // 2383 Theater Print Inspired
        return PrintModel{1.10f, 0.96f, 0.20f, 0.44f, 0.018f, 0.018f, 0.14f, 0.00f};
    case 2: // 3513 Softer Print Inspired
        return PrintModel{1.02f, 0.92f, 0.16f, 0.34f, 0.010f, 0.010f, 0.08f, 0.00f};
    case 3: // Clean Scan Print
        return PrintModel{1.00f, 0.98f, 0.08f, 0.24f, 0.004f, 0.006f, 0.04f, 0.00f};
    case 4: // Warm Show Print
        return PrintModel{1.06f, 0.94f, 0.18f, 0.38f, 0.036f, 0.012f, 0.10f, 0.00f};
    case 5: // Cool Silver Retention
        return PrintModel{1.14f, 0.76f, 0.22f, 0.48f, -0.006f, 0.040f, 0.18f, 0.60f};
    case 6: // AI Soft Print
        return PrintModel{0.98f, 0.88f, 0.14f, 0.30f, 0.012f, 0.014f, 0.06f, 0.00f};
    case 7: // Dense Theatrical
        return PrintModel{1.18f, 0.90f, 0.24f, 0.54f, 0.020f, 0.020f, 0.22f, 0.12f};
    case 8: // B/W Silver Print
        return PrintModel{1.16f, 0.00f, 0.28f, 0.42f, 0.000f, 0.000f, 0.18f, 0.84f};
    case 0:
    default:
        return PrintModel{1.00f, 1.00f, 0.00f, 0.18f, 0.000f, 0.000f, 0.00f, 0.00f};
    }
}

float FilmEmulationCore::hash(float x, float y, float seed)
{
    const float value = std::sin(x * 12.9898f + y * 78.233f + seed * 37.719f) * 43758.5453f;
    return value - std::floor(value);
}

float FilmEmulationCore::decodeTransfer(float value, int inputSpace)
{
    const float v = clamp(value, 0.0f, 64.0f);
    switch (inputSpace) {
    case 0: // Rec.709 Gamma 2.4
    case 1: // AI Footage Rec.709
        return safePow(v, 2.4f);
    case 2: // Timeline / scene linear
        return v;
    case 3: // Sony S-Log3 approximation
        return std::exp2((v - 0.410557f) * 7.5f) * 0.18f;
    case 4: // ARRI LogC3 approximation
        return std::exp2((v - 0.391f) * 6.65f) * 0.18f;
    case 5: // ARRI LogC4 approximation
        return std::exp2((v - 0.278f) * 7.85f) * 0.18f;
    case 6: // Apple Log approximation
        return std::exp2((v - 0.405f) * 7.05f) * 0.18f;
    case 7: // BMD Film Gen 5 / DWG approximation
        return std::exp2((v - 0.380f) * 6.45f) * 0.18f;
    default:
        return safePow(v, 2.4f);
    }
}

float FilmEmulationCore::encodeTransfer(float value, int inputSpace)
{
    const float v = std::max(0.0f, value);
    switch (inputSpace) {
    case 0:
    case 1:
        return safePow(v, 1.0f / 2.4f);
    case 2:
        return v;
    case 3:
        return safeLog2(v / 0.18f) / 7.5f + 0.410557f;
    case 4:
        return safeLog2(v / 0.18f) / 6.65f + 0.391f;
    case 5:
        return safeLog2(v / 0.18f) / 7.85f + 0.278f;
    case 6:
        return safeLog2(v / 0.18f) / 7.05f + 0.405f;
    case 7:
        return safeLog2(v / 0.18f) / 6.45f + 0.380f;
    default:
        return safePow(v, 1.0f / 2.4f);
    }
}

Pixel FilmEmulationCore::decodeInput(Pixel pixel, int inputSpace)
{
    return Pixel{
        decodeTransfer(pixel.r, inputSpace),
        decodeTransfer(pixel.g, inputSpace),
        decodeTransfer(pixel.b, inputSpace),
        pixel.a,
    };
}

Pixel FilmEmulationCore::encodeOutput(Pixel pixel, int inputSpace)
{
    return Pixel{
        clamp(encodeTransfer(pixel.r, inputSpace), 0.0f, inputSpace == 2 ? 64.0f : 1.0f),
        clamp(encodeTransfer(pixel.g, inputSpace), 0.0f, inputSpace == 2 ? 64.0f : 1.0f),
        clamp(encodeTransfer(pixel.b, inputSpace), 0.0f, inputSpace == 2 ? 64.0f : 1.0f),
        pixel.a,
    };
}

Pixel FilmEmulationCore::applyFilmCurve(Pixel pixel, float contrast, float toe, float shoulder, float pivot)
{
    auto curve = [&](float value) {
        float v = std::max(0.0f, value);
        const float logPivot = safeLog2(pivot);
        float logValue = safeLog2(v + 0.00001f);
        logValue = logPivot + (logValue - logPivot) * contrast;
        v = std::exp2(logValue);
        if (toe > 0.0001f) {
            const float t = toe * 0.20f;
            v = (v * (1.0f + t)) / (1.0f + t / std::max(v, 0.00001f));
        }
        if (shoulder > 0.0001f) {
            v = v / (1.0f + v * shoulder * 0.34f);
        }
        return std::max(0.0f, v);
    };
    return Pixel{curve(pixel.r), curve(pixel.g), curve(pixel.b), pixel.a};
}

Pixel FilmEmulationCore::applySaturation(Pixel pixel, float amount)
{
    const float y = luma(pixel);
    return Pixel{
        y + (pixel.r - y) * amount,
        y + (pixel.g - y) * amount,
        y + (pixel.b - y) * amount,
        pixel.a,
    };
}

float FilmEmulationCore::skinMask(Pixel pixel)
{
    const float sum = std::max(0.0001f, pixel.r + pixel.g + pixel.b);
    const float nr = pixel.r / sum;
    const float ng = pixel.g / sum;
    const float nb = pixel.b / sum;
    const float y = luma(pixel);
    const float hueZone =
        smoothstep(0.32f, 0.46f, nr) *
        (1.0f - smoothstep(0.54f, 0.68f, nr)) *
        smoothstep(0.24f, 0.36f, ng) *
        (1.0f - smoothstep(0.40f, 0.52f, nb));
    const float brightness = smoothstep(0.035f, 0.14f, y) * (1.0f - smoothstep(1.25f, 2.2f, y));
    const float skinOrder = smoothstep(-0.06f, 0.08f, pixel.r - pixel.b);
    return clamp01(hueZone * brightness * skinOrder);
}

Pixel FilmEmulationCore::applySubtractiveColor(Pixel pixel, float amount, float skin)
{
    const float a = clamp01(amount) * (1.0f - skin * 0.62f);
    if (a <= 0.0001f) {
        return pixel;
    }

    const float y = std::max(0.0001f, luma(pixel));
    const float chromaR = (pixel.r - y) / y;
    const float chromaG = (pixel.g - y) / y;
    const float chromaB = (pixel.b - y) / y;
    const float sat = std::max(std::fabs(chromaR), std::max(std::fabs(chromaG), std::fabs(chromaB)));
    const float density = 1.0f - clamp01(sat * a * 0.24f);

    Pixel out{
        pixel.r * (density - a * std::max(0.0f, -chromaR) * 0.08f),
        pixel.g * (density - a * std::max(0.0f, -chromaG) * 0.06f),
        pixel.b * (density - a * std::max(0.0f, -chromaB) * 0.10f),
        pixel.a,
    };
    out.r += y * a * 0.010f;
    out.g += y * a * 0.002f;
    out.b -= y * a * 0.008f;
    return out;
}

Pixel FilmEmulationCore::applyTemperatureTint(Pixel pixel, float temperature, float tint, float skin)
{
    const float protect = 1.0f - skin * 0.54f;
    const float temp = clamp(temperature, -1.0f, 1.0f) * protect;
    const float tn = clamp(tint, -1.0f, 1.0f) * protect;
    pixel.r *= 1.0f + temp * 0.055f - tn * 0.010f;
    pixel.g *= 1.0f + tn * 0.040f;
    pixel.b *= 1.0f - temp * 0.060f - tn * 0.016f;
    return pixel;
}

Pixel FilmEmulationCore::applyDeveloper(Pixel pixel, const Controls& controls, float skin)
{
    const float contrast = 1.0f + clamp(controls.developerContrast, -1.0f, 1.0f) * 0.42f;
    const float gamma = 1.0f / std::max(0.12f, 1.0f + clamp(controls.developerGamma, -1.0f, 1.0f) * 0.55f);
    pixel = applyFilmCurve(pixel, contrast, 0.08f * std::max(0.0f, controls.developerContrast), 0.12f, 0.18f);
    pixel.r = safePow(pixel.r, gamma);
    pixel.g = safePow(pixel.g, gamma);
    pixel.b = safePow(pixel.b, gamma);

    const float y = luma(pixel);
    const float sep = clamp01(controls.colorSeparation) * (1.0f - skin * 0.50f);
    if (sep > 0.0001f) {
        pixel.r = y + (pixel.r - y) * (1.0f + sep * 0.42f);
        pixel.g = y + (pixel.g - y) * (1.0f + sep * 0.30f);
        pixel.b = y + (pixel.b - y) * (1.0f + sep * 0.46f);
        pixel.r -= std::max(0.0f, pixel.g - pixel.r) * sep * 0.035f;
        pixel.b -= std::max(0.0f, pixel.g - pixel.b) * sep * 0.025f;
    }

    const float boost = clamp(controls.colorBoost, -1.0f, 1.0f);
    if (std::fabs(boost) > 0.0001f) {
        const float sat = 1.0f + boost * 0.36f * (1.0f - skin * 0.45f);
        pixel = applySaturation(pixel, sat);
    }

    return pixel;
}

Pixel FilmEmulationCore::applyFilmCompression(Pixel pixel, const Controls& controls)
{
    const float impact = clamp01(controls.filmCompression);
    if (impact <= 0.0001f) {
        return pixel;
    }

    const float whitePoint = clamp(controls.compressionWhitePoint, 0.35f, 4.0f);
    const float range = clamp(controls.compressionRange, 0.05f, 1.0f);
    const float density = clamp01(controls.compressionColorDensity);

    auto compress = [&](float value) {
        const float threshold = whitePoint * (1.0f - range * 0.42f);
        if (value <= threshold) {
            return value;
        }
        const float over = value - threshold;
        const float soft = threshold + over / (1.0f + over * impact * (1.8f / whitePoint));
        return value + (soft - value) * impact;
    };

    Pixel compressed{compress(pixel.r), compress(pixel.g), compress(pixel.b), pixel.a};
    if (density > 0.0001f) {
        const float beforeY = std::max(0.0001f, luma(pixel));
        const float afterY = std::max(0.0001f, luma(compressed));
        const float chromaLift = clamp(beforeY / afterY, 0.75f, 1.35f);
        const float y = afterY;
        compressed.r = y + (compressed.r - y) * (1.0f + density * (chromaLift - 1.0f) * 0.72f);
        compressed.g = y + (compressed.g - y) * (1.0f + density * (chromaLift - 1.0f) * 0.58f);
        compressed.b = y + (compressed.b - y) * (1.0f + density * (chromaLift - 1.0f) * 0.66f);
    }
    return compressed;
}

Pixel FilmEmulationCore::applyExpand(Pixel pixel, const Controls& controls)
{
    const float black = clamp(controls.expandBlackPoint, -1.0f, 1.0f) * 0.055f;
    const float white = clamp(controls.expandWhitePoint, -1.0f, 1.0f) * 0.16f;
    const float scale = 1.0f + white - black;
    pixel.r = (pixel.r - black) * scale;
    pixel.g = (pixel.g - black) * scale;
    pixel.b = (pixel.b - black) * scale;
    return pixel;
}

Pixel FilmEmulationCore::applyPrinterLights(Pixel pixel, const Controls& controls)
{
    const float cyan = clamp(controls.printerCyan, -1.0f, 1.0f);
    const float magenta = clamp(controls.printerMagenta, -1.0f, 1.0f);
    const float yellow = clamp(controls.printerYellow, -1.0f, 1.0f);

    pixel.r *= std::pow(2.0f, -cyan * 0.115f + magenta * 0.018f + yellow * 0.012f);
    pixel.g *= std::pow(2.0f, cyan * 0.014f - magenta * 0.110f + yellow * 0.018f);
    pixel.b *= std::pow(2.0f, cyan * 0.020f + magenta * 0.016f - yellow * 0.118f);
    return pixel;
}

Pixel FilmEmulationCore::applyStock(Pixel pixel, const StockModel& stock, const Controls& controls, float skin)
{
    const float density = clamp(controls.density + stock.density, -1.0f, 2.0f);
    pixel.r *= std::pow(2.0f, -density * 0.085f);
    pixel.g *= std::pow(2.0f, -density * 0.070f);
    pixel.b *= std::pow(2.0f, -density * 0.090f);

    pixel = applyTemperatureTint(
        pixel,
        controls.temperature + stock.warmth * 8.0f,
        controls.tint + stock.greenBias * 10.0f,
        skin);
    pixel.b *= 1.0f + stock.blueBias;

    const float neutral = clamp01(controls.neutralizeCurves);
    const float contrast = clamp(controls.contrast * (stock.contrast + (1.0f - stock.contrast) * neutral), 0.25f, 2.8f);
    const float toe = clamp((stock.toe + std::max(0.0f, controls.density) * 0.08f) * (1.0f - neutral * 0.72f), 0.0f, 1.2f);
    const float shoulder = clamp((stock.shoulder + controls.highlightRolloff * 0.28f) * (1.0f - neutral * 0.58f), 0.0f, 1.5f);
    pixel = applyFilmCurve(pixel, contrast, toe, shoulder, 0.18f);

    const float sat = clamp(controls.saturation * stock.saturation, 0.0f, 2.2f);
    pixel = applySaturation(pixel, sat);
    const float interlayer = clamp01(controls.interlayer);
    if (interlayer > 0.0001f) {
        const float y = luma(pixel);
        const float rInhibit = std::max(0.0f, pixel.r - y) * interlayer;
        const float gInhibit = std::max(0.0f, pixel.g - y) * interlayer;
        const float bInhibit = std::max(0.0f, pixel.b - y) * interlayer;
        pixel.r -= (gInhibit * 0.025f + bInhibit * 0.045f) * (1.0f - skin * 0.35f);
        pixel.g -= (rInhibit * 0.026f + bInhibit * 0.020f) * (1.0f - skin * 0.35f);
        pixel.b -= (rInhibit * 0.048f + gInhibit * 0.018f) * (1.0f - skin * 0.35f);
        pixel = applySaturation(pixel, 1.0f + interlayer * 0.08f);
    }
    pixel = applySubtractiveColor(pixel, clamp01(controls.subtractiveColor + stock.subtractive), skin);

    if (stock.pastel > 0.0001f) {
        const float p = clamp01(stock.pastel);
        const float y = luma(pixel);
        Pixel pastel{
            y + (pixel.r - y) * (1.0f - p * 0.32f) + p * 0.010f,
            y + (pixel.g - y) * (1.0f - p * 0.26f) + p * 0.008f,
            y + (pixel.b - y) * (1.0f - p * 0.28f) + p * 0.006f,
            pixel.a,
        };
        pixel = mix(pixel, pastel, p);
    }

    if (stock.monochrome > 0.0001f) {
        const float y = 0.30f * pixel.r + 0.59f * pixel.g + 0.11f * pixel.b;
        const Pixel mono{y, y, y, pixel.a};
        pixel = mix(pixel, mono, clamp01(stock.monochrome));
    }

    const float bleach = clamp01(controls.bleachBypass + stock.silver);
    if (bleach > 0.0001f) {
        const float silver = clamp01(bleach);
        const Pixel gray = applySaturation(pixel, 0.0f);
        const Pixel dense = applyFilmCurve(pixel, 1.0f + silver * 0.22f, silver * 0.12f, silver * 0.12f, 0.18f);
        pixel = mix(dense, gray, silver * 0.42f);
    }

    return pixel;
}

Pixel FilmEmulationCore::applyPrint(Pixel pixel, const PrintModel& print, const Controls& controls)
{
    pixel = applyFilmCurve(
        pixel,
        clamp(print.contrast, 0.25f, 2.8f),
        print.toe,
        print.shoulder,
        0.18f);

    const float y = luma(pixel);
    const float shadow = 1.0f - smoothstep(0.03f, 0.28f, y);
    const float highlight = smoothstep(0.30f, 1.20f, y);
    pixel.r += highlight * print.warmHighlights * 0.55f - shadow * print.coolShadows * 0.014f;
    pixel.g += highlight * print.warmHighlights * 0.12f + shadow * print.coolShadows * 0.004f;
    pixel.b -= highlight * print.warmHighlights * 0.18f - shadow * print.coolShadows * 0.036f;

    const float blackDensity = clamp01(print.blackDensity + std::max(0.0f, controls.density) * 0.05f);
    pixel.r *= 1.0f - shadow * blackDensity * 0.10f;
    pixel.g *= 1.0f - shadow * blackDensity * 0.10f;
    pixel.b *= 1.0f - shadow * blackDensity * 0.10f;

    pixel = applySaturation(pixel, print.saturation);

    if (print.silver > 0.0001f) {
        const Pixel gray = applySaturation(pixel, 0.0f);
        pixel = mix(pixel, gray, print.silver * 0.34f);
    }

    return pixel;
}

float FilmEmulationCore::grainValue(int x, int y, int frameIndex, float size, int channel, int mode)
{
    const float scale = 0.62f + clamp(size, 0.0f, 4.0f) * 2.35f;
    const float fx = std::floor(static_cast<float>(x) / scale);
    const float fy = std::floor(static_cast<float>(y) / scale);
    const float frame = static_cast<float>(frameIndex);
    float n = hash(fx + channel * 19.0f, fy + frame * 1.73f, 11.0f + channel * 3.1f);
    n += hash(fx * 2.1f + frame * 0.37f, fy * 2.1f + channel * 5.0f, 23.0f) * 0.5f;
    n = n / 1.5f;
    float centered = n * 2.0f - 1.0f;
    if (mode == 1) { // Fine scan grain
        const float fine = hash(static_cast<float>(x) + frame * 5.0f, static_cast<float>(y), 91.0f + channel);
        centered = centered * 0.55f + (fine * 2.0f - 1.0f) * 0.45f;
    } else if (mode == 2) { // Chunky 16mm
        centered = centered > 0.0f ? safePow(centered, 0.72f) : -safePow(-centered, 0.72f);
    } else if (mode == 3) { // Monochrome tight
        const float mono = hash(fx + frame * 1.21f, fy, 55.0f);
        centered = mono * 2.0f - 1.0f;
    }
    return clamp(centered, -1.0f, 1.0f);
}

float FilmEmulationCore::toneGrainMask(float value, const Controls& controls)
{
    const float shadows = clamp01(controls.grainShadows);
    const float mids = clamp01(controls.grainMidtones);
    const float highs = clamp01(controls.grainHighlights);
    const float shadowMask = 1.0f - smoothstep(0.08f, 0.34f, value);
    const float highMask = smoothstep(0.48f, 1.45f, value);
    const float midMask = clamp01(1.0f - shadowMask * 0.85f - highMask * 0.65f);

    float profileBias = 1.0f;
    switch (controls.grainProfile) {
    case 1: // 8mm 500
        profileBias = 1.55f;
        break;
    case 2: // 16mm 500
        profileBias = 1.28f;
        break;
    case 3: // 35mm 500
        profileBias = 1.05f;
        break;
    case 4: // 35mm 250
        profileBias = 0.82f;
        break;
    case 5: // 65mm 50
        profileBias = 0.48f;
        break;
    default:
        break;
    }
    return profileBias * (shadowMask * shadows + midMask * mids + highMask * highs);
}

Pixel FilmEmulationCore::temporalReconstruct(
    Pixel scene,
    int x,
    int y,
    const Controls& controls,
    const TemporalContext* temporal)
{
    const float amount = clamp01(controls.temporalReconstruction);
    if (amount <= 0.0001f || !temporal) {
        return scene;
    }

    Pixel sum{0.0f, 0.0f, 0.0f, scene.a};
    float weight = 0.0f;
    auto accumulate = [&](const Sampler* sampler, bool valid, float w) {
        if (!valid || !sampler) {
            return;
        }
        const Pixel sample = decodeInput(sampler->sample(static_cast<float>(x), static_cast<float>(y)), controls.inputSpace);
        sum.r += sample.r * w;
        sum.g += sample.g * w;
        sum.b += sample.b * w;
        weight += w;
    };

    accumulate(temporal->previous2, temporal->hasPrevious2, 0.45f);
    accumulate(temporal->previous, temporal->hasPrevious, 1.00f);
    accumulate(temporal->next, temporal->hasNext, 1.00f);
    accumulate(temporal->next2, temporal->hasNext2, 0.45f);
    if (weight <= 0.0001f) {
        return scene;
    }

    Pixel average{sum.r / weight, sum.g / weight, sum.b / weight, scene.a};
    const float motion =
        std::fabs(luma(scene) - luma(average)) +
        std::fabs(scene.r - average.r) * 0.16f +
        std::fabs(scene.g - average.g) * 0.16f +
        std::fabs(scene.b - average.b) * 0.16f;
    const float guard = smoothstep(0.025f, 0.24f, motion) * clamp01(controls.temporalMotionGuard);
    const float blend = amount * clamp01(controls.temporalStability) * (1.0f - guard);
    const float aiRec709Bias = controls.inputSpace == 1 ? 1.18f : 0.82f;
    return mix(scene, average, clamp01(blend * 0.32f * aiRec709Bias));
}

} // namespace buckswood_film
