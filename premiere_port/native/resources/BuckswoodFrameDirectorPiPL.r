#include "BuckswoodPremierePiPLCommon.r"

#define plugInName "Buckswood Frame Director"
#define plugInCategory "Buckswood"
#define plugInMatchName "BW Frame Director"

resource 'PiPL' (16004) {
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
            bgAnimatable, fgAnimatable, geometric, noRandomness, 16, plugInMatchName
        },
        BW_FLOAT_PARAM(0, "Aspect 0-6", 1, BW_DOUBLE_ZERO, BW_DOUBLE_SIX),
        BW_FLOAT_PARAM(1, "View 0-4", 2, BW_DOUBLE_ZERO, BW_DOUBLE_FOUR),
        BW_FLOAT_PARAM(2, "Framing 0-4", 3, BW_DOUBLE_ZERO, BW_DOUBLE_FOUR),
        BW_FLOAT_PARAM(3, "Auto Strength", 4, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(4, "Subject Weight", 5, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(5, "Skin Weight", 6, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(6, "Subject Lock", 7, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(7, "Lock X", 8, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(8, "Lock Y", 9, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(9, "Manual X", 10, BW_DOUBLE_NEG_ONE, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(10, "Manual Y", 11, BW_DOUBLE_NEG_ONE, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(11, "Subject Vertical", 12, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(12, "Guide Opacity", 13, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(13, "Matte Opacity", 14, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(14, "Crop Feather", 15, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(15, "Output Mix", 16, BW_DOUBLE_ZERO, BW_DOUBLE_ONE)
    }
};
