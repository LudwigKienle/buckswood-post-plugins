#include "BuckswoodPremierePiPLCommon.r"

#define plugInName "Buckswood Temporal Integrity"
#define plugInCategory "Buckswood"
#define plugInMatchName "BW Temporal Integrity"

resource 'PiPL' (16006) {
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
            bgAnimatable, fgAnimatable, geometric, noRandomness, 10, plugInMatchName
        },
        BW_FLOAT_PARAM(0, "View 0-5", 1, BW_DOUBLE_ZERO, BW_DOUBLE_FIVE),
        BW_FLOAT_PARAM(1, "Sensitivity", 2, BW_DOUBLE_ZERO, BW_DOUBLE_TWO),
        BW_FLOAT_PARAM(2, "Repair Strength", 3, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(3, "Luma Stability", 4, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(4, "Texture Stability", 5, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(5, "Edge Protection", 6, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(6, "Chroma Stability", 7, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(7, "Overlay Strength", 8, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(8, "Output Mix", 9, BW_DOUBLE_ZERO, BW_DOUBLE_ONE),
        BW_FLOAT_PARAM(9, "Spatial Radius", 10, BW_DOUBLE_QUARTER, BW_DOUBLE_THREE)
    }
};
