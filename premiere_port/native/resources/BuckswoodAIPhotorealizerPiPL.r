#ifndef PRWIN_ENV
#include <CoreServices/CoreServices.r>
#define MAC_ENV
#include "PrSDKPiPLVer.h"
#include "PrSDKPiPL.r"
#endif

#define plugInName "Buckswood AI Photorealizer"
#define plugInCategory "Buckswood"
#define plugInMatchName "BW AI Photorealizer"

#define DOUBLE_ZERO 0x00000000, 0x00000000
#define DOUBLE_ONE 0x00000000, 0x3ff00000
#define DOUBLE_THOUSAND 0x00000000, 0x408f4000

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
            11,
            plugInMatchName
        },
        ANIM_ParamAtom {
            0,
            "Plastic Reduction",
            1,
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
            1,
            "Skin Realism",
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
            "Highlight Realism",
            3,
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
            3,
            "Color Naturalize",
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
            "Sensor Texture",
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
            "Micro Contrast",
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
            "Gamut Guard",
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
            "Shadow Depth",
            8,
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
            8,
            "Smoothness Breakup",
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
            "Output Mix",
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
            "Texture Seed",
            11,
            ANIM_DT_FLOAT_32,
            ANIM_UI_SLIDER,
            DOUBLE_ONE,
            DOUBLE_THOUSAND,
            DOUBLE_ONE,
            DOUBLE_THOUSAND,
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
