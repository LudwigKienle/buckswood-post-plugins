#include "BuckswoodPremierePiPLCommon.r"

#define plugInName "Buckswood Radiance Recover"
#define plugInCategory "Buckswood"
#define plugInMatchName "BW Radiance Recover"

resource 'PiPL' (16005) {
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
            bgAnimatable, fgAnimatable, geometric, noRandomness, 11, plugInMatchName
        },
        BW_FLOAT_PARAM(0, "View 0-3", 1, BW_DOUBLE_ZERO, BW_DOUBLE_THREE),
        BW_FLOAT_PARAM(1, "Highlight Recovery", 2, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(2, "Specular Recovery", 3, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(3, "Highlight Rolloff", 4, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(4, "Shadow Recovery", 5, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(5, "Headroom Stops", 6, BW_DOUBLE_ZERO, BW_DOUBLE_FOUR),
        BW_FLOAT_PARAM(6, "Local Detail", 7, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(7, "Shadow Denoise", 8, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(8, "Dequantization", 9, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(9, "Color Guard", 10, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(10, "Output Mix", 11, BW_DOUBLE_ZERO, BW_DOUBLE_ONE)
    }
};
