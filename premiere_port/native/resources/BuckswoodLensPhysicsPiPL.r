#ifndef PRWIN_ENV
#include <CoreServices/CoreServices.r>
#define MAC_ENV
#include "PrSDKPiPLVer.h"
#include "PrSDKPiPL.r"
#endif

#define plugInName "Buckswood Lens Physics"
#define plugInCategory "Buckswood"
#define plugInMatchName "BW Lens Physics"

#define DOUBLE_NEG_ONE 0x00000000, 0xbff00000
#define DOUBLE_ZERO 0x00000000, 0x00000000
#define DOUBLE_035 0x66666666, 0x3fd66666
#define DOUBLE_ONE 0x00000000, 0x3ff00000
#define DOUBLE_THREE 0x00000000, 0x40080000
#define DOUBLE_NINETEEN 0x00000000, 0x40330000

resource 'PiPL' (16000) {
    {
        Kind {PrEffect},
        Name {plugInName},
        AE_Effect_Match_Name {plugInMatchName},
        Category {plugInCategory},
        AE_PiPL_Version {PiPLVerMajor, PiPLVerMinor},
        ANIM_FilterInfo {
            0,
#ifdef PiPLVer2p3
            notUnityPixelAspectRatio,
            anyPixelAspectRatio,
            reserved4False,
            reserved3False,
            reserved2False,
#endif
            reserved1False,
            reserved0False,
            driveMe,
            needsDialog,
            paramsNotPointer,
            paramsNotHandle,
            paramsNotMacHandle,
            dialogNotInRender,
            paramsNotInGlobals,
            bgAnimatable,
            fgAnimatable,
            geometric,
            noRandomness,
            14,
            plugInMatchName
        },
        ANIM_ParamAtom {
            0,
            "Lens Preset",
            1,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ZERO,
            DOUBLE_NINETEEN,
            DOUBLE_ZERO,
            DOUBLE_NINETEEN,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            1,
            "Effect Strength",
            2,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ZERO,
            DOUBLE_ONE,
            DOUBLE_ZERO,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            2,
            "Distortion Trim",
            3,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_NEG_ONE,
            DOUBLE_ONE,
            DOUBLE_NEG_ONE,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            3,
            "Chromatic Aberration",
            4,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ZERO,
            DOUBLE_ONE,
            DOUBLE_ZERO,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            4,
            "High Contrast Fringe",
            5,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ZERO,
            DOUBLE_ONE,
            DOUBLE_ZERO,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            5,
            "Edge Coma",
            6,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ZERO,
            DOUBLE_ONE,
            DOUBLE_ZERO,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            6,
            "Highlight Bloom",
            7,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ZERO,
            DOUBLE_ONE,
            DOUBLE_ZERO,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            7,
            "Bloom Threshold",
            8,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_035,
            DOUBLE_ONE,
            DOUBLE_035,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            8,
            "Vignette",
            9,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ZERO,
            DOUBLE_ONE,
            DOUBLE_ZERO,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            9,
            "Corner Color Cast",
            10,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ZERO,
            DOUBLE_ONE,
            DOUBLE_ZERO,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            10,
            "Swirl",
            11,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ZERO,
            DOUBLE_ONE,
            DOUBLE_ZERO,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            11,
            "F-Stop Sharpener",
            12,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_NEG_ONE,
            DOUBLE_ONE,
            DOUBLE_NEG_ONE,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            12,
            "Overdrive",
            13,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ZERO,
            DOUBLE_THREE,
            DOUBLE_ZERO,
            DOUBLE_THREE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        },
        ANIM_ParamAtom {
            13,
            "Output Mix",
            14,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ZERO,
            DOUBLE_ONE,
            DOUBLE_ZERO,
            DOUBLE_ONE,
#ifdef PiPLVer2p3
            dontScaleUIRange,
#endif
            animateParam,
            dontRestrictBounds,
            spaceIsAbsolute,
            resIndependent,
            4
        }
    }
};
