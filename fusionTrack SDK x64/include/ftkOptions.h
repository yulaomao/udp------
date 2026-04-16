// ===========================================================================

/**
 *   This file is part of the ATRACSYS fusiontrack library.
 *   Copyright (C) 2003-2018 by Atracsys LLC. All rights reserved.
 *
 *  THIS FILE CANNOT BE SHARED, MODIFIED OR REDISTRIBUTED WITHOUT THE
 *  WRITTEN PERMISSION OF ATRACSYS.
 *
 *  \file     ftkOptions.h
 *  \brief    tracker options structures
 *
 */
// ===========================================================================

#ifndef ftkOptions_h
#define ftkOptions_h

#ifdef GLOBAL_DOXYGEN

/**
 * \addtogroup sdk
 * \{
 */
#endif

#include "ftkPlatform.h"
#include "ftkTypes.h"

/** \brief offboard option ID corresponding
 *         onboard ID difference
 */
#ifndef __cplusplus
#define FTK_OFFBOARD_TO_ONBOARD_OPTION_IDS_DIFFERENCE 7000
#else
constexpr uint32 FTK_OFFBOARD_TO_ONBOARD_OPTION_IDS_DIFFERENCE( 7000u );
#endif  // !__cplusplus

// WARNING - do not change these structure
// otherwise the Device library compatibility cannot be guaranteed
// See devOptions.h for more info

/** \brief Module to which the option belongs.
 */
TYPED_ENUM( uint8, ftkComponent )
// clang-format off
{
    FTK_DEVICE = 0,           /**< Option is specific to the device  */
    FTK_DETECTOR = 1,         /**< Option is specific to the detection stage  */
    FTK_MATCH2D3D = 2,        /**< Option is specific to the 3d matching (including
                               * marker matching)  */
    FTK_DEVICE_DETECTOR = 3,  /**< Option is specific to the device optical
                               * communication */
    FTK_DEVICE_MATCH2D3D = 4, /**< Option is specific to the detection stage
                               * when done on the device */
    FTK_DEVICE_WIRELESS = 5,  /**< Option is specific to the 3d matching (including
                               * marker matching) when done on the device  */
    FTK_DEVICE_DEVICE = 6,    /**< Option is specific to the device when done
                               * on the device */
    FTK_DEVICE_PTP = 7,        /**< Option is specific to the PTP handling. */
    FTK_DEVICE_EIO = 8,
    FTK_LIBRARY = 100        /**< Option is specific to the library  */
};
// clang-format on

/** \brief Type of option
 */
TYPED_ENUM( uint8, ftkOptionType )
// clang-format off
{
    FTK_INT32 = 0,   /**< Option is 32 bits signed integer  */
    FTK_FLOAT32 = 1, /**< Option is 32 bits signed float */
    FTK_DATA = 3     /**< Option is raw data (text or binary) */
};
// clang-format on

/** \brief Type of value to retrieve (only for FTK_INT32 or FTK_FLOAT32 options)
 */
TYPED_ENUM( uint8, ftkOptionGetter )
// clang-format off
{
    FTK_MIN_VAL = 0, /**< Retrieve the minimum value of the option */
    FTK_MAX_VAL = 1, /**< Retrieve the maximum value of the option */
    FTK_DEF_VAL = 2, /**< Retrieve the default value of the option */
    FTK_VALUE = 3    /**< Retrieve the actual value of the option */
};
// clang-format on

/** \brief Option accessibility
 *
 * This status indicates the read / write property of an option.
 */
PACK1_STRUCT_BEGIN( ftkOptionStatus )
{
    uint32 read : 1;            /**< Option can be read */
    uint32 write : 1;           /**< Option can be written */
    uint32 accessProtected : 1; /**< Option is protected. */
    uint32 accessPrivate : 1;   /**< Option is private. */
    uint32 globalOption : 1;    /**< Option is not related to a device, i.e.
                                     SN must be \c 0uLL to call it. */
}
PACK1_STRUCT_END( ftkOptionStatus );

/** \brief Detailed description of an option
 *
 * This holds the information about an option.
 */
PACK1_STRUCT_BEGIN( ftkOptionsInfo )
{
    uint32 id;               /**< Unique id of the option */
    ftkComponent component;  /**< Driver component linked to the option */
    ftkOptionStatus status;  /**< Option accessibility */
    ftkOptionType type;      /**< Type of the option */
    const char* name;        /**< Name of the option */
    const char* description; /**< Detailed description of the option */
    const char* unit;        /**< Unit of the option (if available) */
}
PACK1_STRUCT_END( ftkOptionsInfo );

#ifdef GLOBAL_DOXYGEN

/**
 * \}
 */
#endif

#endif
