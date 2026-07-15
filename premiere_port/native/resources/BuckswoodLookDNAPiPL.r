#include "BuckswoodPremierePiPLCommon.r"

#define plugInName "Buckswood Look DNA"
#define plugInCategory "Buckswood"
#define plugInMatchName "BW Look DNA"

resource 'PiPL' (16007) {
    {
        Kind {PrEffect}, Name {plugInName}, AE_Effect_Match_Name {plugInMatchName},
        Category {plugInCategory}, AE_PiPL_Version {PiPLVerMajor, PiPLVerMinor},
        ANIM_FilterInfo {
            0,
#ifdef PiPLVer2p3
            notUnityPixelAspectRatio, anyPixelAspectRatio, reserved4False, reserved3False, reserved2False,
#endif
            reserved1False, reserved0False, driveMe, needsDialog, paramsNotPointer,
            paramsNotHandle, paramsNotMacHandle, dialogNotInRender, paramsNotInGlobals,
            bgAnimatable, fgAnimatable, geometric, noRandomness, 24, plugInMatchName
        },
        BW_FLOAT_PARAM(0, "Reference 0-3", 1, BW_DOUBLE_ZERO, BW_DOUBLE_THREE),
        BW_FLOAT_PARAM(1, "Reload Token", 2, BW_DOUBLE_ZERO, BW_DOUBLE_THOUSAND),
        BW_FLOAT_PARAM(2, "Input Space 0-8", 3, BW_DOUBLE_ZERO, BW_DOUBLE_EIGHT),
        BW_FLOAT_PARAM(3, "Match Mode 0-3", 4, BW_DOUBLE_ZERO, BW_DOUBLE_THREE),
        BW_FLOAT_PARAM(4, "Quality 0-2", 5, BW_DOUBLE_ZERO, BW_DOUBLE_TWO),
        BW_FLOAT_PARAM(5, "View 0-7", 6, BW_DOUBLE_ZERO, BW_DOUBLE_SEVEN),
        BW_FLOAT_PARAM(6, "Reference B Mix", 7, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(7, "Reference C Mix", 8, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(8, "Reference Adaptivity", 9, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(9, "Match Strength", 10, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(10, "Tone Match", 11, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(11, "Palette Match", 12, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(12, "Semantic Match", 13, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(13, "Local Contrast", 14, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(14, "Texture Match", 15, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(15, "Grain Match", 16, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(16, "Density Match", 17, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(17, "Exposure Lock", 18, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(18, "Skin Protect", 19, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(19, "Highlight Protect", 20, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(20, "Scene Identity Guard", 21, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(21, "Gamut Guard", 22, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(22, "Split Position", 23, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(23, "Output Mix", 24, BW_DOUBLE_ZERO, BW_DOUBLE_ONE)
    }
};
