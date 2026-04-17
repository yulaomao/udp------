// ===========================================================================

/*!
 *   This file is part of the ATRACSYS fusiontrack library.
 *   Copyright (C) 2003-2018 by Atracsys LLC. All rights reserved.
 *
 *  THIS FILE CANNOT BE SHARED, MODIFIED OR REDISTRIBUTED WITHOUT THE
 *  WRITTEN PERMISSION OF ATRACSYS.
 *
 *  \file     ftkErrors.h
 *  \brief    Device error codes
 *
 */
// ===========================================================================

#ifndef ftkErrors_h
#define ftkErrors_h

#ifdef GLOBAL_DOXYGEN

/**
 * \addtogroup sdk
 * \{
 */
#endif

#include "ftkTypes.h"

/** \brief Error codes.
 *
 * Negative values are used for warnings (i.e. non-fatal issues),
 * whilst positive values are used for errors. An error should stop the
 * normal execution, as the data may be corrupted, missing, old, etc.
 */
TYPED_ENUM( int32, ftkError )
// clang-format off
{
    /** \brief Status when an available feature was disabled by the user.
     *
     * This is for instance returned when the user tries to read the acceleration value when they disabled
     * the periodic sending of the accelerometer data.
     */
    FTK_WAR_DISABLED = -200,

    /** \brief Error when ftkEnumerateOptions is called with an invalid serial
     * number.
     */
    FTK_WAR_OPT_GLOBAL_ONLY = -103,

    /** \brief This option has no effect
     */
    FTK_WAR_OPT_NO_OP = -102,

    /** \brief The input value was out of range
     */
    FTK_WAR_OPT_VAL_RANGE = -101,

    /** \brief The shock sensor was offline and enabled back automatically.
     */
    FTK_WAR_SHOCK_SENSOR_AUTO_ENABLE = -41,

    /** \brief The shock sensor is currently offline.
     */
    FTK_WAR_SHOCK_SENSOR_OFFLINE = -40,

    /** \brief The calibration file cannot be authenticated.
     */
    FTK_WAR_CALIB_AUTHENTICATION = -30,

    /** \brief The device is connected in USB2 or below.
     */
    FTK_WAR_USB_TOO_SLOW = -22,

    /** \brief The callback was not present
     */
    FTK_WAR_NOT_EXISTING = -21,

    /** \brief The callback was already present
     */
    FTK_WAR_ALREADY_PRESENT = -20,

    /** \brief The pictures were discarded due to their size
     */
    FTK_WAR_REJECTED_PIC = -18,

    /** \brief A shock as been detected and calibration may be obsolete
     */
    FTK_WAR_SHOCK_DETECTED = -17,

    /** \brief The device serial number is not in the calibration file
     */
    FTK_WAR_SN_ABSENT = -16,

    /** \brief Interpolation of geometrical model not possible as internal temperature is too high
     */
    FTK_WAR_TEMP_HIGH = -15,

    /** \brief Interpolation of geometrical model not possible as internal temperature is too low
     */
    FTK_WAR_TEMP_LOW = -14,

    /** \brief Interpolation of geometrical model not possible due to a bad
     * reading of the internal temperatures.
     */
    FTK_WAR_TEMP_INVALID = -13,

    /** \brief File is not found
     */
    FTK_WAR_FILE_NOT_FOUND = -12,

    /** \brief Geometry ID is not registered
     */
    FTK_WAR_GEOM_ID = -11,

    /** \brief Error when updating the frame in ftkGetLastFrame
     */
    FTK_WAR_FRAME = -10,

    /** \brief Not supported
     */
    FTK_WAR_NOT_SUPPORTED = -2,

    /** \brief No new frame available
     */
    FTK_WAR_NO_FRAME = -1,

    /** \brief No error
     */
    FTK_OK = 0,

    /** \brief Invalid pointer
     */
    FTK_ERR_INV_PTR = 1,

    /** \brief Invalid serial number
     */
    FTK_ERR_INV_SN = 2,

    /** \brief Invalid index
     */
    FTK_ERR_INV_INDEX = 3,

    /** \brief Internal error, usually this should not happen and be reported to Atracsys
     */
    FTK_ERR_INTERNAL = 4,

    /** \brief Error when writing
     */
    FTK_ERR_WRITE = 5,

    /** \brief Error when reading
     */
    FTK_ERR_READ = 6,

    /** \brief Image decompression error
     */
    FTK_ERR_IMG_DEC = 7,

    /** \brief Image format error
     */
    FTK_ERR_IMG_FMT = 8,

    /** \brief Invalid version
     */
    FTK_ERR_VERSION = 9,

    /** \brief Frame query instance not properly initialised
     */
    FTK_ERR_INIT = 10,

    /** \brief Image size too small
     */
    FTK_ERR_IM_TOO_SMALL = 11,

    /** \brief Internal or external INI file syntax error
     */
    FTK_ERR_INV_INI_FILE = 12,

    /** \brief Overflow during image segmentation
     */
    FTK_ERR_SEG_OVERFLOW = 13,

    /** \brief The device contains a calibration file preventing triangulation
     */
    FTK_ERR_IMPAIRING_CALIB = 14,

    /** \brief The marker reconstruction algorithm exhausted the allowed processing time.
     */
    FTK_ERR_ALGORITHMIC_WALLTIME = 15,

    /** \brief Geometry has too few / too much points
     */
    FTK_ERR_GEOM_PTS = 20,

    /** \brief A geometry with the same ID is already registered.
     */
    FTK_ERR_GEOM_REGISTERED = 21,

    /** \brief No such geometry registered.
     */
    FTK_ERR_UNKNOWN_GEOM = 22,

    /** \brief The index of the wanted property is invalid for the geometry.
     */
    FTK_ERR_GEOM_INV_INDEX = 23,

    /** \brief Synchronisation error
     */
    FTK_ERR_SYNC = 30,

    /** \brief Temperature compensation algorithm not implemented
     */
    FTK_ERR_COMP_ALGO = 40,

    /** \brief Disconnection detected
     */
    FTK_ERR_DISCONNECTION = 50,

    /** \brief Invalid option
     */
    FTK_ERR_INV_OPT = 100,

    /** \brief Invalid option access
     */
    FTK_ERR_INV_OPT_ACC = 101,

    /** \brief Invalid option value
     */
    FTK_ERR_INV_OPT_VAL = 102,

    /** \brief Invalid option parameter
     */
    FTK_ERR_INV_OPT_PAR = 103
};
// clang-format on

#undef ERROR_ENUM

#ifdef GLOBAL_DOXYGEN

/**
 * \}
 */
#endif

#endif
