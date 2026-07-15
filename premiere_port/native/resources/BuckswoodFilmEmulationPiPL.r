#include "BuckswoodPremierePiPLCommon.r"

#define plugInName "Buckswood Film Emulation"
#define plugInCategory "Buckswood"
#define plugInMatchName "BW Film Emulation"

resource 'PiPL' (16003) {
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
            bgAnimatable, fgAnimatable, geometric, noRandomness, 27, plugInMatchName
        },
        BW_FLOAT_PARAM(0, "Input Space 0-7", 1, BW_DOUBLE_ZERO, BW_DOUBLE_SEVEN),
        BW_FLOAT_PARAM(1, "Film Stock 0-15", 2, BW_DOUBLE_ZERO, BW_DOUBLE_FIFTEEN),
        BW_FLOAT_PARAM(2, "Print Stock 0-8", 3, BW_DOUBLE_ZERO, BW_DOUBLE_EIGHT),
        BW_FLOAT_PARAM(3, "Process Mode 0-2", 4, BW_DOUBLE_ZERO, BW_DOUBLE_TWO),
        BW_FLOAT_PARAM(4, "View 0-4", 5, BW_DOUBLE_ZERO, BW_DOUBLE_FOUR),
        BW_FLOAT_PARAM(5, "Exposure Stops", 6, BW_DOUBLE_NEG_FOUR, BW_DOUBLE_FOUR),
        BW_FLOAT_PARAM(6, "Push Pull", 7, BW_DOUBLE_NEG_ONE, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(7, "Density", 8, BW_DOUBLE_NEG_ONE, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(8, "Contrast", 9, BW_DOUBLE_ZERO, BW_DOUBLE_TWO),
        BW_FLOAT_PARAM(9, "Saturation", 10, BW_DOUBLE_ZERO, BW_DOUBLE_TWO),
        BW_FLOAT_PARAM(10, "Temperature", 11, BW_DOUBLE_NEG_ONE, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(11, "Tint", 12, BW_DOUBLE_NEG_ONE, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(12, "Color Separation", 13, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(13, "Subtractive Color", 14, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(14, "Film Compression", 15, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(15, "Highlight Rolloff", 16, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(16, "Black Lift", 17, BW_DOUBLE_NEG_ONE, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(17, "Printer Cyan", 18, BW_DOUBLE_NEG_ONE, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(18, "Printer Magenta", 19, BW_DOUBLE_NEG_ONE, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(19, "Printer Yellow", 20, BW_DOUBLE_NEG_ONE, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(20, "Halation", 21, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(21, "Bloom", 22, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(22, "Grain", 23, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(23, "Grain Size", 24, BW_DOUBLE_QUARTER, BW_DOUBLE_THREE),
        BW_FLOAT_PARAM(24, "Film Resolution", 25, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(25, "Skin Protect", 26, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(26, "Output Mix", 27, BW_DOUBLE_ZERO, BW_DOUBLE_ONE)
    }
};
