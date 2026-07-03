#include "LensPhysicsCore.h"

namespace buckswood_lens {

LensPhysicsCore::LensModel LensPhysicsCore::modelForPreset(int preset)
{
    switch (preset) {
    case 1: // Clean Cinema Prime
        return LensModel{0.015f, 0.16f, 0.08f, 0.04f, 0.18f, 0.18f, 0.06f, 0.00f, 0.16f, 0.01f, 0.00f};
    case 2: // Vintage Spherical
        return LensModel{-0.075f, 0.36f, 0.25f, 0.28f, 0.42f, 0.35f, 0.22f, 0.04f, 0.04f, 0.03f, -0.01f};
    case 3: // Petzval Swirl
        return LensModel{-0.120f, 0.30f, 0.18f, 0.34f, 0.32f, 0.42f, 0.20f, 0.42f, -0.02f, 0.02f, 0.02f};
    case 4: // Anamorphic Soft
        return LensModel{0.030f, 0.46f, 0.35f, 0.18f, 0.52f, 0.26f, 0.14f, 0.02f, 0.02f, 0.01f, 0.04f};
    case 5: // Soviet Glow
        return LensModel{-0.105f, 0.42f, 0.30f, 0.38f, 0.60f, 0.48f, 0.30f, 0.16f, -0.04f, 0.05f, -0.02f};
    case 6: // AI Deplastic Glass
        return LensModel{-0.035f, 0.22f, 0.14f, 0.12f, 0.28f, 0.24f, 0.10f, 0.03f, 0.08f, 0.02f, 0.01f};
    case 7: // Lensfun: KMZ Helios-44 58mm f/2, M42, full frame
        return LensModel{0.035f, 0.10f, 0.18f, 0.30f, 0.42f, 0.46f, 0.28f, 0.24f, -0.03f, 0.035f, -0.01f};
    case 8: // Lensfun: KMZ MC Helios-44M-4 58mm f/2, M42, full frame
        return LensModel{0.026f, 0.07f, 0.14f, 0.26f, 0.36f, 0.48f, 0.30f, 0.18f, -0.02f, 0.030f, -0.01f};
    case 9: // Lensfun: Zeiss Batis 25mm f/2, Sony E
        return LensModel{-0.055f, 0.04f, 0.06f, 0.06f, 0.20f, 0.39f, 0.12f, 0.00f, 0.12f, 0.005f, 0.01f};
    case 10: // Lensfun: Voigtlander Nokton 58mm f/1.4 SLII
        return LensModel{-0.030f, 0.06f, 0.12f, 0.18f, 0.30f, 0.34f, 0.18f, 0.08f, 0.02f, 0.020f, 0.00f};
    case 11: // Lensfun: Voigtlander Nokton 28mm f/1.5 Aspherical
        return LensModel{0.006f, 0.22f, 0.20f, 0.14f, 0.34f, 0.58f, 0.24f, 0.04f, 0.04f, 0.010f, 0.01f};
    case 12: // Dune Part One: Panavision H Series, spherical IMAX 1.43 inspiration
        return LensModel{-0.012f, 0.10f, 0.09f, 0.08f, 0.24f, 0.20f, 0.07f, 0.00f, 0.02f, 0.018f, 0.004f};
    case 13: // Dune Part One: Panavision Ultra Vista 1.6x anamorphic inspiration
        return LensModel{0.020f, 0.34f, 0.30f, 0.10f, 0.45f, 0.24f, 0.11f, 0.02f, -0.02f, 0.012f, 0.020f};
    case 14: // Dune Part Two: ARRI Rental Moviecam spherical inspiration
        return LensModel{-0.018f, 0.12f, 0.10f, 0.16f, 0.28f, 0.22f, 0.10f, 0.03f, 0.00f, 0.020f, 0.005f};
    case 15: // Dune Part Two: IronGlass Soviet-era spherical inspiration
        return LensModel{-0.070f, 0.28f, 0.26f, 0.34f, 0.48f, 0.42f, 0.24f, 0.18f, -0.06f, 0.035f, -0.010f};
    case 16: // IMAX 15/70: 50mm clean wide face sweet spot inspiration
        return LensModel{-0.006f, 0.08f, 0.06f, 0.04f, 0.18f, 0.08f, 0.03f, 0.00f, 0.08f, 0.006f, 0.004f};
    case 17: // IMAX 15/70: 80mm portrait sweet spot inspiration
        return LensModel{-0.004f, 0.06f, 0.05f, 0.03f, 0.16f, 0.06f, 0.025f, 0.00f, 0.10f, 0.004f, 0.003f};
    case 18: // Dune Part Three: Atlas golden single-coating IMAX inspiration
        return LensModel{-0.004f, 0.12f, 0.18f, 0.10f, 0.30f, 0.02f, 0.14f, 0.00f, 0.06f, 0.045f, 0.000f};
    case 19: // Buckswood Realistic Match: subtle AI-footage lens correction default
        return LensModel{-0.006f, 0.030f, 0.020f, 0.020f, 0.055f, 0.040f, 0.014f, 0.000f, 0.055f, 0.003f, 0.001f};
    case 0:
    default:
        return LensModel{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    }
}

} // namespace buckswood_lens
