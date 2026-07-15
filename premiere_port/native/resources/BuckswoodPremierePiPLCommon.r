#ifndef PRWIN_ENV
#include <CoreServices/CoreServices.r>
#define MAC_ENV
#include "PrSDKPiPLVer.h"
#include "PrSDKPiPL.r"
#endif

#define BW_DOUBLE_NEG_FOUR 0x00000000, 0xc0100000
#define BW_DOUBLE_NEG_ONE 0x00000000, 0xbff00000
#define BW_DOUBLE_ZERO 0x00000000, 0x00000000
#define BW_DOUBLE_QUARTER 0x00000000, 0x3fd00000
#define BW_DOUBLE_ONE 0x00000000, 0x3ff00000
#define BW_DOUBLE_TWO 0x00000000, 0x40000000
#define BW_DOUBLE_THREE 0x00000000, 0x40080000
#define BW_DOUBLE_FOUR 0x00000000, 0x40100000
#define BW_DOUBLE_FIVE 0x00000000, 0x40140000
#define BW_DOUBLE_SIX 0x00000000, 0x40180000
#define BW_DOUBLE_SEVEN 0x00000000, 0x401c0000
#define BW_DOUBLE_EIGHT 0x00000000, 0x40200000
#define BW_DOUBLE_TEN 0x00000000, 0x40240000
#define BW_DOUBLE_FIFTEEN 0x00000000, 0x402e0000
#define BW_DOUBLE_NINETEEN 0x00000000, 0x40330000
#define BW_DOUBLE_THOUSAND 0x00000000, 0x408f4000

#ifdef PiPLVer2p3
#define BW_UI_SCALE_FLAG dontScaleUIRange,
#else
#define BW_UI_SCALE_FLAG
#endif

#define BW_FLOAT_PARAM(INDEX, LABEL, IDENTIFIER, MINIMUM, MAXIMUM) \
    ANIM_ParamAtom { \
        INDEX, \
        LABEL, \
        IDENTIFIER, \
        ANIM_DT_FLOAT_32, \
        ANIM_UI_SLIDER, \
        MINIMUM, \
        MAXIMUM, \
        MINIMUM, \
        MAXIMUM, \
        BW_UI_SCALE_FLAG \
        animateParam, \
        dontRestrictBounds, \
        spaceIsAbsolute, \
        resIndependent, \
        4 \
    }
