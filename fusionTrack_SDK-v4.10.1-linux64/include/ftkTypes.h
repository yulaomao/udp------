// ===========================================================================

/*!
 *   This file is part of the ATRACSYS fusiontrack library.
 *   Copyright (C) 2003-2018 by Atracsys LLC. All rights reserved.
 *
 *  THIS FILE CANNOT BE SHARED, MODIFIED OR REDISTRIBUTED WITHOUT THE
 *  WRITTEN PERMISSION OF ATRACSYS.
 *
 *  \file     ftkTypes.h
 *  \brief    Standard types
 *
 */
// ===========================================================================

#ifndef ftk_ftkTypes_h
#define ftk_ftkTypes_h

#include "ftkPlatform.h"

#if defined( ATR_LIN ) || defined( ATR_OSX )

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#endif

#include <inttypes.h>
#include <stdint.h>

typedef uint8_t uint8;   /*!< \brief  8 bits unsigned */
typedef uint16_t uint16; /*!< \brief 16 bits unsigned */
typedef uint32_t uint32; /*!< \brief 32 bits unsigned */
typedef uint64_t uint64; /*!< \brief 64 bits unsigned */
typedef int8_t int8;     /*!< \brief  8 bits signed */
typedef int16_t int16;   /*!< \brief 16 bits signed */
typedef int32_t int32;   /*!< \brief 32 bits signed */
typedef int64_t int64;   /*!< \brief 64 bits signed */
typedef float float32;   /*!< \brief single precision floating point (32 bits) */
typedef double float64;  /*!< \brief double precision floating point (32 bits) */
typedef uint8_t bool8;   /*!< \brief boolean values */
typedef float floatXX;   /*!< \brief Generic type for floats. Is now float32, may be float64 in the future */

#if defined( ATR_LIN ) || defined( ATR_OSX )

#include <stddef.h>

#endif

#ifndef __cplusplus
typedef _Bool bool;
#endif

#endif
