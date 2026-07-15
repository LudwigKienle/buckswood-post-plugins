#include "BuckswoodPremierePiPLCommon.r"

#define plugInName "Buckswood Fake Diagnostic"
#define plugInCategory "Buckswood"
#define plugInMatchName "BW Fake Diagnostic"

resource 'PiPL' (16002) {
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
            bgAnimatable, fgAnimatable, geometric, noRandomness, 15, plugInMatchName
        },
        BW_FLOAT_PARAM(0, "View 0-4", 1, BW_DOUBLE_ZERO, BW_DOUBLE_FOUR),
        BW_FLOAT_PARAM(1, "Sensitivity", 2, BW_DOUBLE_ZERO, BW_DOUBLE_TWO),
        BW_FLOAT_PARAM(2, "Overlay Strength", 3, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(3, "Assist Strength", 4, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(4, "Plastic Weight", 5, BW_DOUBLE_ZERO, BW_DOUBLE_TWO),
        BW_FLOAT_PARAM(5, "Highlight Weight", 6, BW_DOUBLE_ZERO, BW_DOUBLE_TWO),
        BW_FLOAT_PARAM(6, "Edge Weight", 7, BW_DOUBLE_ZERO, BW_DOUBLE_TWO),
        BW_FLOAT_PARAM(7, "Grade Weight", 8, BW_DOUBLE_ZERO, BW_DOUBLE_TWO),
        BW_FLOAT_PARAM(8, "Texture Weight", 9, BW_DOUBLE_ZERO, BW_DOUBLE_TWO),
        BW_FLOAT_PARAM(9, "Micro Contrast", 10, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(10, "Edge Soften", 11, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(11, "Highlight Rolloff", 12, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(12, "Gamut Guard", 13, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(13, "Sensor Texture", 14, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(14, "Texture Seed", 15, BW_DOUBLE_ONE, BW_DOUBLE_THOUSAND)
    }
};
