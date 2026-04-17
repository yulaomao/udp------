// ===========================================================================

/**
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2018 by Atracsys LLC. All rights reserved.
 *
 *  THIS FILE CANNOT BE SHARED, MODIFIED OR REDISTRIBUTED WITHOUT THE
 *  WRITTEN PERMISSION OF ATRACSYS.
 *
 *  \file     ftkInterface.h
 *  \brief    Main interface of Atracsys Passive Tracking SDK
 *
 */
// ===========================================================================

#ifndef ftkInterface_h
#define ftkInterface_h

#ifdef GLOBAL_DOXYGEN

/**
 * \addtogroup sdk
 * \{
 */
#endif

#include "ftkErrors.h"
#include "ftkEvent.h"
#include "ftkOptions.h"
#include "ftkPlatform.h"
#include "ftkTypes.h"

#ifdef __cplusplus
#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <utility>
#endif

#if !defined( __cplusplus ) || ( defined( ATR_MSVC ) && _MSC_VER < 1900 )
#ifndef BUFFER_MAX_SIZE

/** \brief Maximal size of ftkBuffer.
 *
 * The value is 10 KiB.
 */
#define BUFFER_MAX_SIZE 10u * 1024u
#endif  // BUFFER_MAX_SIZE
#else
/** \brief Maximal size of ftkBuffer.
 *
 * The value is \f$ \SI{10}{\kibi\byte} \f$.
 */
constexpr uint32 BUFFER_MAX_SIZE( 10u * 1024u );
#endif

struct ftkLibraryImp;

/** \brief Library abstract handle
 */
typedef ftkLibraryImp* ftkLibrary;

/** \brief Available pixel formats
 */
TYPED_ENUM( uint8, ftkPixelFormat )
// clang-format off
{
    /** \brief Pixels are represented as grayscale values ranging from 0
     * (black) to 255 (white).
     */
    GRAY8 = 0,

    /** \brief Pixels are represented as grayscale values ranging from 0
     * (black) to 255 (white). This images are AR.
     */
    GRAY8_VIS = 3,
    /** \brief Pixels are represented as grayscale values ranging from 0
     * (black) to 4096 (white).
     */
    GRAY16,

    /** \brief Pixels are represented as grayscale values ranging from 0
     * (black) to 4096 (white). This images are AR.
     */
    GRAY16_VIS,

    /** \brief Pixels are represented as grayscale values ranging from 0
     * (black) to 255 (white). This images are Structured light.
     */
    GRAY8_SL,

    /** \brief Pixels are represented as grayscale values ranging from 0
     * (black) to 4096 (white). These images are Structured light.
     */
    GRAY16_SL,
};
// clang-format on

#ifdef ATR_MSVC
// Disable warning because Visual C++ bug when using signed enums
#pragma warning( push )
#pragma warning( disable : 4341 )
#endif

/** \brief Frame members status
 * \see ftkFrameQuery
 */
TYPED_ENUM( int8, ftkQueryStatus )
// clang-format off
{
    /** \brief This field was not written.
     */
    QS_WAR_SKIPPED = -1,

    /** \brief This field is initialised correctly and contains valid data.
     */
    QS_OK = 0,

    /** \brief This field is initialised correctly, but some data are missing because buffer size is too
     * small.
     */
    QS_ERR_OVERFLOW = 1,

    /** \brief The reserved size is not a multiple of the type's size.
     */
    QS_ERR_INVALID_RESERVED_SIZE = 2,

    /** \brief This field is requested to be reprocessed.
     */
    QS_REPROCESS = 10
};
#ifdef ATR_MSVC
// Disable warning because Visual C++ bug when using signed enums
#pragma warning( pop )
#endif
// clang-format on

/** \brief Type of device connected
 * \see ftkDeviceEnumCallback
 */
TYPED_ENUM( int8, ftkDeviceType )
// clang-format off
{
    /** \brief The `device' is a simulator.
     *
     * \deprecated
     */
    DEV_SIMULATOR = 0,

    /** \brief Device is an infiniTrack.
     */
    DEV_INFINITRACK = 1,

    /** \brief The device is a fusionTrack 500.
     */
    DEV_FUSIONTRACK_500 = 2,

    /** \brief The device is a fusionTrack 250.
     */
    DEV_FUSIONTRACK_250 = 3,

    /** \brief The device is a spryTrack 180.
     */
    DEV_SPRYTRACK_180 = 4,

    /** \brief The device is a spryTrack 300.
     */
    DEV_SPRYTRACK_300 = 5,

    /** \brief The device simulates a fusionTrack 500.
     */
    SIM_FUSIONTRACK_500 = 0x22,

    /** \brief The device simulates a fusionTrack 250.
     */
    SIM_FUSIONTRACK_250 = 0x23,

    /** \brief The device simulates a spryTrack 180.
     */
    SIM_SPRYTRACK_180 = 0x24,

    /** \brief The device simulates a spryTrack 300.
     */
    SIM_SPRYTRACK_300 = 0x25,

    /** \brief Unknown device type.
     */
    DEV_UNKNOWN_DEVICE = 127
};
// clang-format on

/** \brief Enumeration for the calibration type.
 *
 * This enum allows to assign a `tag' to a calibration file.
 */
TYPED_ENUM( uint32, CalibrationType )
// clang-format off
{
    /** \brief Only use for tests, the dummy calibration file could also use this.
     */
    DevOnly = 0u,

    /** \brief This calibration file contains a \e valid precalibration.
     */
    Precalibration,

    /** \brief This calibration file contains an \e invalid precalibration.
     */
    InvalidPrecalibration,

    /** \brief This calibration file contains a \e valid calibration from CMM.
     */
    GeometricCalibration,

    /** \brief This calibration file contains an \e invalid calibration from CMM.
     */
    InvalidGeometricCalibration,

    /** \brief This calibration file contains a \e valid calibration with temperature compensation.
     */
    TemperatureCompensation,

    /** \brief This calibration file contains an \e invalid calibration with temperature compensation.
     */
    InvalidTemperatureCompensation,

    /** \brief This calibration is in a unknown state (i.e. prior to version 3).
     */
    Unknown = 0xffffffffu
};
// clang-format on

/** \brief Enumeration for the data type.
 *
 * This enumeration is used to tell which information is gotten with the
 * ftkGetTotalObjectNumber function.
 */
TYPED_ENUM( uint32, ftkDataType )
// clang-format off
{
    /** \brief Left raw data.
     */
    LeftRawData = 0x1,

    /** \brief Right raw data.
     */
    RightRawData = 0x2,

    /** \brief 3D fiducial data.
     */
    FiducialData = 0x4,

    /** \brief Marker data.
     */
    MarkerData = 0x8,

    /** \brief Events.
     */
    EventData = 0x10
};
// clang-format on

/** \brief Enumeration for the various information that could be gotten.
 */
TYPED_ENUM( uint32_t, ftkInformationType )
// clang-format off
{
    /** \brief Information if the strobe is synchronised with the frame acquisition.
     *
     * Possible read values are \c 0 if the strobing is disabled on
     * the current frame or \c 1 if enabled.
     */
    StrobeEnabled,
    /** \brief Information about calibration parameters.
     *
     * The camera parameters can be read iff the event had previously
     * been enabled.
     */
    CalibrationParameters,
    Undefined = 0xFFFFFFFFu
};

/** \brief Structure carrying binary data in/out of library functions.
 *
 * This structure is used to get / set binary data. It helps reducing memory
 * leaks by taking care of the destruction of the memory.
 */
PACK1_STRUCT_BEGIN( ftkBuffer )
{
    /** \brief Data buffer.
     *
     *   This is a simple array of bytes.
     */
    union
    {
        /** \brief Data seen as plain \c char.
         */
        char data[ BUFFER_MAX_SIZE ];
        /** \brief Data seen as \c uint8.
         */
        uint8 uData[ BUFFER_MAX_SIZE ];
        /** \brief Data seen as \c int8.
         */
        int8 sData[ BUFFER_MAX_SIZE ];
    };

    /** \brief Actual size of the data.
     *
     *   This member stores the real size of the data stored in the
     *   buffer.
     */
    uint32 size;

    /** \brief Method resetting the structure.
     *
     * This method erases the data buffer and sets the size to 0.
     */
#ifdef __cplusplus
    void reset();
#endif
}
PACK1_STRUCT_END( ftkBuffer );

#ifdef __cplusplus

inline void ftkBuffer::reset()
{
    memset( sData, 0, BUFFER_MAX_SIZE );
    size = 0u;
}

#endif

/** \brief Structure holding the status of a piece of data.
 *
 * This structure stores the status of the data. The structure is used across
 * ::ftkRawData, ::ftk3DFiducial and ::ftkMarker, which is for instance
 * allowing to propagate the status of the ::ftkRawData left instance to the
 * ::ftkMarker instance.
 */
PACK1_STRUCT_BEGIN( ftkStatus )
{
    /** \brief Contains 1 if the blob touches the right edge of the picture.
     *
     * This status is related to a ::ftkRawData instance. When set to \c 1 in
     * a ::ftkRawData instance, it means the blob touches the right edge of the
     * sensor. If set in a ::ftk3DFiducial instance, it means at least one of
     * two underlying ::ftkRawData (i.e. the left or the right one) has this
     * status. Finally, when set in a ::ftkMarker instance, at least one
     * ::ftk3DFiducial instance has it, meaning at least one of the used
     * ::ftkRawData instance touches the right edge of the sensor.
     */
    uint32 RightEdge : 1;

    /** \brief Contains 1 if the blob touches the bottom edge of the picture.
     *
     * This status is related to a ::ftkRawData instance. When set to \c 1 in
     * a ::ftkRawData instance, it means the blob touches the bottom edge of
     * the sensor. If set in a ::ftk3DFiducial instance, it means at least one
     * of two underlying ::ftkRawData (i.e. the left or the right one) has this
     * status. Finally, when set in a ::ftkMarker instance, at least one
     * ::ftk3DFiducial instance has it, meaning at least one of the used
     * ::ftkRawData instance touches the bottom edge of the sensor.
     */
    uint32 BottomEdge : 1;

    /** \brief Contains 1 if the blob touches the left edge of the picture.
     *
     * This status is related to a ::ftkRawData instance. When set to \c 1 in
     * a ::ftkRawData instance, it means the blob touches the left edge of
     * the sensor. If set in a ::ftk3DFiducial instance, it means at least one
     * of two underlying ::ftkRawData (i.e. the left or the right one) has this
     * status. Finally, when set in a ::ftkMarker instance, at least one
     * ::ftk3DFiducial instance has it, meaning at least one of the used
     * ::ftkRawData instance touches the left edge of the sensor.
     */
    uint32 LeftEdge : 1;

    /** \brief Contains 1 if the blob touches the top edge of the picture.
     *
     * This status is related to a ::ftkRawData instance. When set to \c 1 in
     * a ::ftkRawData instance, it means the blob touches the top edge of
     * the sensor. If set in a ::ftk3DFiducial instance, it means at least one
     * of two underlying ::ftkRawData (i.e. the left or the right one) has this
     * status. Finally, when set in a ::ftkMarker instance, at least one
     * ::ftk3DFiducial instance has it, meaning at least one of the used
     * ::ftkRawData instance touches the top edge of the sensor.
     */
    uint32 TopEdge : 1;

    /** \brief Contains 1 if the blob lies in the part of the sensor where the
     * intensity descreases because of the lens cropping.
     *
     * This status is related to a ::ftkRawData instance. When set to \c 1 in
     * a ::ftkRawData instance, it means the blob lies in the area of the
     * sensor where the collected intensity drops due to lens cropping.
     * If set in a ::ftk3DFiducial instance, it means at least one of the two
     * underlying ::ftkRawData (i.e. the left or the right one) has this
     * status. Finally, when set in a ::ftkMarker instance, at least one
     * ::ftk3DFiducial instance has it, meaning at least one of the used
     * ::ftkRawData instance lies in the cropped area.
     */
    uint32 SensorCrop : 1;

    /** \brief Contains 1 if the blob lies in the part of the sensor not
     * included in the accuracy measurement volume.
     *
     * This status is related to a ::ftkRawData instance. When set to \c 1 in
     * a ::ftkRawData instance, it means the blob lies outside of the
     * accuracy measurement volume.
     * If set in a ::ftk3DFiducial instance, it means at least one of the two
     * underlying ::ftkRawData (i.e. the left or the right one) has this
     * status. Finally, when set in a ::ftkMarker instance, at least one
     * ::ftk3DFiducial instance has it, meaning at least one of the used
     * ::ftkRawData instance lies outside the accuracy measurement volume.
     */
    uint32 WorkingVolume : 1;

    /** \brief Contains 1 if the 3D fiducial has a z value outside the allowed
     * range.
     *
     * This status is related to a ::ftk3DFiducial instance. When set to \c 1,
     * it means the 3D pint lies outside the allowed \f$ z \f$ range (as
     * given in the device specifications). It set in a ::ftkMarker instance,
     * it means at least one of the used ::ftk3DFiducial instances lies outside
     * the allowed \f$ z \f$ range.
     */
    uint32 ThreeDcrop : 1;

    /** \brief Contains 1 if the element was corrected for the tilt bias.
     *
     * This status applies to ::ftkMarker instances which can be corrected for the bias introduced by
     * obliquity of the fiducials. The status is then applied to the used ::ftk3DFiducial instances belonging
     * to the marker and to each related ::ftkRawData instances.
     */
    uint32 Corrected : 1;
#ifdef __cplusplus
private:
#endif
    uint32 Reserved : 24;
#ifdef __cplusplus
public:
#endif

#ifdef __cplusplus

    /** \brief Conversion operator.
     *
     * This operator converts the structure into a unsigned integer.
     *
     * \return the value of the bit fields, seen as a 32-bits unsigned
     * integer.
     */
    operator uint32() const;

    /** \brief Affectation operator.
     *
     * This operator allows to promote a 32-bits unsigned integer into
     * a working instance.
     *
     * \param[in] val value to "dump" in the instance.
     *
     * \return a reference on the current instance.
     */
    ftkStatus& operator=( uint32 val );
#endif
}
PACK1_STRUCT_END( ftkStatus );

#ifdef __cplusplus
inline ftkStatus::operator uint32() const
{
    static_assert( sizeof( ftkStatus ) == 4u, "Problem with bitfield" );
    union
    {
        uint32 nbr;
        ftkStatus status;
    };
    status = *this;
    return nbr;
}

inline ftkStatus& ftkStatus::operator=( uint32 val )
{
    union
    {
        uint32 nbr;
        ftkStatus status;
    };
    nbr = val;
    *this = status;
    return *this;
}

#endif

#ifdef GLOBAL_DOXYGEN

/**
 * \}
 *
 * \addtogroup frame
 * \{
 */
#endif

/** \brief Structure to hold version and size
 *
 * This structure stores a version number and the reserved size for
 * an array in ftkFrameQuery.
 */
PACK1_STRUCT_BEGIN( ftkVersionSize )
{
    /** \brief Version number.
     */
    uint32 Version;

    /** \brief Size of the array in bytes.
     */
    uint32 ReservedSize;
#ifdef __cplusplus
    /** \brief Equality operator.
     *
     * This operator allows to determine if two instance are equal, which is achieved if the Version and
     * ReservedSize members are equal.
     *
     * \param[in] other instance to be compared with the current one.
     *
     * \retval true if the Version and ReservedSize are the same,
     * \retval false if at least of the fields is different.
     */
    bool operator==( const ftkVersionSize& other ) const;
    /** \brief Inequality operator.
     *
     * This operator allows to determine if two instance are different, which is achieved they are not equal.
     * Internally, operator==(const ftkVersionSize) const is called.
     *
     * \param[in] other instance to be compared with the current one.
     *
     * \retval false if the Version and ReservedSize are the same,
     * \retval true if at least of the fields is different.
     */
    bool operator!=( const ftkVersionSize& other ) const;
#endif
}
PACK1_STRUCT_END( ftkVersionSize );

#ifdef __cplusplus
inline bool ftkVersionSize::operator==( const ftkVersionSize& other ) const
{
    return Version == other.Version && ReservedSize == other.ReservedSize;
}

inline bool ftkVersionSize::operator!=( const ftkVersionSize& other ) const
{
    return !this->operator==( other );
}
#endif

/** \brief Image header
 *
 * The image header contains basic information on the picture dimensions and
 * timing information.
 */
PACK1_STRUCT_BEGIN( ftkImageHeader )
{
    /** \brief Timestamp of the image in micro-seconds.
     */
    uint64 timestampUS;

    /** \brief Desynchronisation between left and right frames (infiniTrack
     * only, 0 otherwise).
     */
    uint64 desynchroUS;

    /** \brief Image counter.
     */
    uint32 counter;

    /** \brief Pixel format.
     */
    ftkPixelFormat format;

    /** \brief Image width (in pixels).
     */
    uint16 width;

    /** \brief Image height (in pixels).
     */
    uint16 height;

    /** \brief Image width * size of pixel + padding in bytes.
     */
    int32 imageStrideInBytes;
}
PACK1_STRUCT_END( ftkImageHeader );

/** \brief Fiducial raw data
 *
 * Fiducial raw data are low-level detection results from left and right
 * images.
 */
PACK1_STRUCT_BEGIN( ftkRawData )
{
    /** \brief Horizontal position of the center of the fiducial in image
     * reference (unit pixels).
     */
    floatXX centerXPixels;

    /** \brief Vertical position of the center of the fiducial in image
     * reference (unit pixels).
     */
    floatXX centerYPixels;

    /** \brief Status of the blob.
     */
    ftkStatus status;

    /** \brief Contain the surface of pixels composing the fiducial (unit
     * pixels).
     */
    uint32 pixelsCount;

    /** \brief Width of the bounding rectangle in pixels.
     */
    uint16 width;

    /** \brief Height of the bounding rectangle in pixels.
     */
    uint16 height;
}
PACK1_STRUCT_END( ftkRawData );

/** \brief 3D point structure
 *
 * This stores the coordinates of a 3D point.
 */
PACK1_STRUCT_BEGIN( ftk3DPoint )
{
    floatXX x, /**< 3D position, x component (unit mm) */
      y,       /**< 3D position, y component (unit mm) */
      z;       /**< 3D position, z component (unit mm) */
}
PACK1_STRUCT_END( ftk3DPoint );

/** \brief Fiducial 3D data after left-right triangulation
 *
 * 3D fiducials are retrieved after triangulation and matching of raw data.
 *
 * Errors description:
 *
 * `Epipolar geometry is the geometry of stereo vision. When two cameras view a
 * 3D scene from two distinct positions, there are a number of geometric
 * relations between the 3D points and their projections onto the 2D images
 * that lead to constraints between the image points.
 *
 * These relations are derived based on the assumption that the cameras can be
 * approximated by the pinhole camera model.' [source: <a
 * href="https://en.wikipedia.org/wiki/Epipolar_geometry">wikipedia</a>]
 *
 * - epipolarErrorPixels represents the signed distance between the right
 * epipolar line (of the left fiducial) and its matching right fiducial. Units
 * are in pixels.
 * - triangulationErrorMM represents the minimum distance of the 3D lines
 * defined by the left optical center and the left projection and the right
 * optical center and the right position. Units are in millimeters.
 *
 * Probability:
 *
 * The probability is defined by the number of potential matches. Basically
 * defined by number of potential matched points that are at a specified
 * distance from the epipolar lines. This ambiguity is usually disambiguated
 * once the 3D point is matched with a marker geometry.
 *
 * \see ftkRawData
 * \see http://en.wikipedia.org/wiki/Epipolar_geometry
 */
PACK1_STRUCT_BEGIN( ftk3DFiducial )
{
    /** \brief Status of the 3D fiducial.
     */
    ftkStatus status;

    /** \brief Index of the corresponding ftkRawData in the left image.
     */
    uint32 leftIndex;

    /** \brief Index of the corresponding ftkRawData in the right image.
     */
    uint32 rightIndex;

    /** \brief 3D position of the center of the fiducial (unit mm).
     */
    ftk3DPoint positionMM;

    /** \brief Epipolar matching error (unit pixel) (see introduction).
     */
    floatXX epipolarErrorPixels;

    /** \brief Triangulation error (unit mm) (see introduction).
     */
    floatXX triangulationErrorMM;

    /** \brief Probability (range 0..1) (see introduction).
     */
    floatXX probability;
}
PACK1_STRUCT_END( ftk3DFiducial );

/** \brief Structure containing calibration parameter for augmented reality
 *
 *  This structure contains all the parameters needed to do the math for
 *  augmented reality (a mix of 3d graphics computed using the positions
 *  returned by the SDK overlayed on top of the raw images).
 *
 */
PACK1_STRUCT_BEGIN( ftkARCalibrationParameters )
{
    /** \brief Principal Point X.
     */
    double principalPointX;

    /** \brief Principal Point Y.
     */
    double principalPointY;

    /** \brief Focal Length X.
     */
    double focalLenghtX;

    /** \brief Focal Length Y.
     */
    double focalLenghtY;
}
PACK1_STRUCT_END( ftkARCalibrationParameters );

#ifdef GLOBAL_DOXYGEN

/**
 * \}
 *
 * \addtogroup geometries
 * \{
 */
#endif

#if !defined( __cplusplus ) || ( defined( ATR_MSVC ) && _MSC_VER < 1900 )
#define FTK_MAX_FIDUCIALS 6
#define FTK_MAX_FIDUCIALS_64 64
#else

/** Maximum number of fiducials that define a geometry.
 */
constexpr uint32 FTK_MAX_FIDUCIALS = 6u;

/** Maximum number of fiducials that define a 64-fiducials geometry.
 */
constexpr uint32 FTK_MAX_FIDUCIALS_64 = 64u;
#endif

/** \brief Geometric description of a marker
 *
 * The geometry can be defined in any referential. It will only influence to
 * output of the pose (which is the transformation between the geometry
 * referential and the device referential).
 *
 * \deprecated This structure does not allow to store all the needed information about a tool,
 * therefore it is advised to use ftkRigidBody and the related functions.
 */
PACK1_STRUCT_BEGIN( ftkGeometry )
{
    /** \brief Unique Id defining the geometry.
     */
    uint32 geometryId;

    /** \brief Version of the geometry structure.
     */
    uint32 version : 8;

    /** \brief Number of points defining the geometry.
     */
    uint32 pointsCount : 24;

    /** \brief 3D position of points defining the geometry (unit mm).
     */
    ftk3DPoint positions[ FTK_MAX_FIDUCIALS ];
}
PACK1_STRUCT_END( ftkGeometry );

/** \brief Type of used fiducial.
 *
 * The identifier contains the following information:
 *  - technology of fiducial on 1 byte
 *     - 0 for unspeficied;
 *     - 1 for discs;
 *     - 2 for retroreflective spheres;
 *     - 3 for glass spheres or equivalent;
 *     - 4 for LEDs;
 *  - manufacturer on 1 byte
 *     - 0 for unspecified;
 *     - 1 for Atracsys;
 *     - 2 for Smith and Nephew;
 *     - 3 for Illumark;
 *     - 4 for Axios 3D;
 *     - 5 for NDI
 *     - 6 for Brainlab
 *  - identifier of the fiducial.
 */
TYPED_ENUM( uint32_t, ftkFiducialType )
// clang-format off
{
    /** \brief Unspecified fiducial, used when no data is available.
    */
    Unspecified = 0x00000000u,
    /** \brief Generic disc fiducial.
     */
    GenericDisc = 0x01000000u,
    /** \brief Navex v1 type.
     *
     * Corresponds to SKU 1031802001.
     */
    NavexV1 = 0x01010000u,
    /** \brief Navex v2 type.
     *
     * Corresponds to SKU 1031803000.
     */
    NavexV2 = 0x01010001u,
    /** \brief Flat marker
     *
     * This is the BlueBelt Tech (Smith and Nephew) Flat marker for Navio / Cori.
     */
    FlatMarker = 0x01020000u,
    /** \brief Generic reflecting sphere fiducial.
     */
    GenericSphere = 0x02000000u,
    /** \brief Sphere screw system Illumark
     *
     * Ilumark retro-reflective Marker Shperes M3 ref. 2020 diameter 13mm.
     *
     * Corresponds to SKU 1031902001.
     */
    SphereIllumarkScrew = 0x02030000u,
    /** \brief Sphere snap system Illumark
     *
     * Ilumark Navigation Marker Snap ref. 2010 diameter 11.5mm.
     *
     * Corresponds to SKU 1031901001.
     */
    SphereIllumarkSnap = 0x02030001u,
    /** \brief Axios 3D SORT Targets
     */
    SphereAxios3dSort = 0x02040000u,
    /** \brief Sphere snap system NDI (NDI DRMS).
     */
    SphereNdiSnapSystem = 0x02050000u,
    /** \brief Generic glass sphere
     */
    GlassSphereGeneric = 0x03000000u,
    /** \brief NDI Radix sphere
     */
    GlassSphereNdiRadix = 0x03050000u,
    /** \brief Brainlab Clearlens.
     */
    GlassSphereBrainlabClearlens = 0x03060000u,
    /** \brief Generic LED fiducial.
     */
    LEDGeneric = 0x04000000u,
    /** \brief SFH4250-Z LED fiducial.
     *
     * This is the OSRAM SFH4250-Z.
     */
    LEDOsramSfh4250 = 0x04000001u,
    /** \brief Autoclavable Atracsys LED fiducial.
     */
    LEDAtracsysAutoclavableV1 = 0x04010000u
};
// clang-format on

/** \brief Structure holding the information on the properties of the fiducial.
 *
 * This structure
 */
PACK1_STRUCT_BEGIN( ftkFiducialInfo )
{
#ifdef __cplusplus
    /** \brief Default constructor.
     *
     * The default constructor builds a fiducial with no angular information
     * and unspecified fiducial type.
     */
    ftkFiducialInfo();
    /** \brief Default implementation of copy-constructor.
     *
     * \param[in] other instance to duplicate.
     */
    ftkFiducialInfo( const ftkFiducialInfo& other ) = default;
    /** \brief Default implementation of move-constructor.
     *
     * \param[in] other instance to move.
     */
    ftkFiducialInfo( ftkFiducialInfo && other ) = default;
    /** \brief Default implementation of the destructor.
     */
    ~ftkFiducialInfo() = default;
    /** \brief Default implementation of affectation operator.
     *
     * \param[in] other instance to duplicate.
     *
     * \retval *this as a reference
     */
    ftkFiducialInfo& operator=( const ftkFiducialInfo& other ) = default;
    /** \brief Default implementation of move-affectation operator.
     *
     * \param[in] other instance to move.
     *
     * \retval *this as a reference
     */
    ftkFiducialInfo& operator=( ftkFiducialInfo&& other ) = default;
#endif
    /** \brief The type of the considered fiducial.
     */
    ftkFiducialType type;
    /** \brief Maximal angle value between the location vector of the fiducial
     * and the normal to the fiducial surface.
     *
     * The unit is radian, a valud of \c -1.0f means no information.
     */
    float angleOfView;
}
PACK1_STRUCT_END( ftkFiducialInfo );

#ifdef __cplusplus
inline ftkFiducialInfo::ftkFiducialInfo()
    : type( ftkFiducialType::Unspecified )
    , angleOfView( -1.0f )
{
}
#endif

/** \brief Structure containing all physical properties of a fiducial on a
 * marker.
 *
 * This structure defines a fiducial beloging to a marker. It is characterised
 * by:
 *  - a position;
 *  - an optional normal vector;
 *  - an optional angle of view;
 *  - an optional type.
 */
PACK1_STRUCT_BEGIN( ftkFiducial )
{
#ifdef __cplusplus
    /** \brief Default constructor.
     *
     * The default constructor builds a fiducial with no angular information
     * and unspecified fiducial type, at the origin and without normal
     * information.
     */
    ftkFiducial();
    /** \brief Default implementation of copy-constructor.
     *
     * \param[in] other instance to duplicate.
     */
    ftkFiducial( const ftkFiducial& other ) = default;
    /** \brief Default implementation of move-constructor.
     *
     * \param[in] other instance to move.
     */
    ftkFiducial( ftkFiducial && other ) = default;
    /** \brief Default implementation of the destructor.
     */
    ~ftkFiducial() = default;
    /** \brief Default implementation of affectation operator.
     *
     * \param[in] other instance to duplicate.
     *
     * \retval *this as a reference
     */
    ftkFiducial& operator=( const ftkFiducial& other ) = default;
    /** \brief Default implementation of move-affectation operator.
     *
     * \param[in] other instance to move.
     *
     * \retval *this as a reference
     */
    ftkFiducial& operator=( ftkFiducial&& other ) = default;
#endif
    /** \brief Position of the fiducial in the marker reference frame.
     *
     * The unit is mm.
     */
    ftk3DPoint position;
    /** \brief Vector normal to the fiducial surface.
     *
     * This vector is used to determine the angle at which the fiducial is seen
     * by a sensor. If the vector is null, it means no information is
     * available.
     */
    ftk3DPoint normalVector;
    /** \brief Information about the fiducial properties.
     */
    ftkFiducialInfo fiducialInfo;
}
PACK1_STRUCT_END( ftkFiducial );

#ifdef __cplusplus
inline ftkFiducial::ftkFiducial()
    : position( { 0.0f, 0.0f, 0.0f } )
    , normalVector( { 0.0f, 0.0f, 0.0f } )
    , fiducialInfo()
{
}
#endif

/** \brief Geometric description of a marker
 *
 * The geometry can be defined in any referential. It will only influence to
 * output of the pose (which is the transformation between the geometry
 * referential and the device referential).
 *
 * The current implementation has version \c 1u.
 */
PACK1_STRUCT_BEGIN( ftkRigidBody )
{
#ifdef __cplusplus
    /** \brief Default constructor.
     *
     * The default constructor builds an instance with:
     *  - version 1;
     *  - geometry ID 0;
     *  - 0 points and 0 divots
     *  - all ftkFiducial instances default-initialised;
     *  - all divot positions set to origin.
     */
    ftkRigidBody();
    /** \brief Default implementation of copy-constructor.
     *
     * \param[in] other instance to duplicate.
     */
    ftkRigidBody( const ftkRigidBody& other ) = default;
    /** \brief Default implementation of move-constructor.
     *
     * \param[in] other instance to move.
     */
    ftkRigidBody( ftkRigidBody && other ) = default;
    /** \brief Default implementation of the destructor.
     */
    ~ftkRigidBody() = default;
    /** \brief Default implementation of affectation operator.
     *
     * \param[in] other instance to duplicate.
     *
     * \retval *this as a reference
     */
    ftkRigidBody& operator=( const ftkRigidBody& other ) = default;
    /** \brief Default implementation of move-affectation operator.
     *
     * \param[in] other instance to move.
     *
     * \retval *this as a reference
     */
    ftkRigidBody& operator=( ftkRigidBody&& other ) = default;
    /** \brief Operator converting to ftkGeometry.
     *
     * This method allows to get a `downgraded' version of the instance as a
     * ftkGeometry instance. All members supported by ftkGeometry are copied in
     * the returned value, other are dropped.
     *
     * \return a ftkGeometry instance in which all compatible information have
     * been copied.
     */
    operator ftkGeometry() const;
    /** \brief Operator promoting a ftkGeometry.
     *
     * This operator allows to promote a ftkGeometry instance in the current
     * one. All compatible information are copied in the current instance, all
     * other are default-initialised. For instance, no divots will be present
     * in the current instance, all fiducial type will be ftkFiducialType::Unspecified, etc.
     *
     * \param[in] other instance of ftkGeometry to promote.
     *
     * \retval *this as a reference.
     */
    ftkRigidBody& operator=( const ftkGeometry& other );
#endif
    /** \brief Unique Id defining the geometry.
     */
    uint32 geometryId;

    /** \brief Version of the geometry structure.
     */
    uint32 version;

    /** \brief Number of points defining the geometry.
     */
    uint32 pointsCount;

    /** \brief Number of divots on the marker.
     */
    uint32 divotsCount;

    /** \brief 3D position and information of the fiducials.
     */
    ftkFiducial fiducials[ FTK_MAX_FIDUCIALS ];

    /** \brief 3D position of divots (unit mm).
     */
    ftk3DPoint divotPositions[ FTK_MAX_FIDUCIALS ];
}
PACK1_STRUCT_END( ftkRigidBody );

#ifdef __cplusplus
inline ftkRigidBody::ftkRigidBody()
    : geometryId( 0u )
    , version( 1u )
    , pointsCount( 0u )
    , divotsCount( 0u )
    , fiducials{}
    , divotPositions{}
{
}

inline ftkRigidBody::operator ftkGeometry() const
{
    ftkGeometry result{};
    result.geometryId = geometryId;
    result.pointsCount = pointsCount;
    const ftkFiducial* inPoint( fiducials );
    ftk3DPoint* outPoint( result.positions );
    for ( uint32_t i( 0u ); i < std::min( pointsCount, FTK_MAX_FIDUCIALS ); ++i, ++inPoint, ++outPoint )
    {
        *outPoint = inPoint->position;
    }
    return result;
}

inline ftkRigidBody& ftkRigidBody::operator=( const ftkGeometry& other )
{
    *this = std::move( ftkRigidBody() );

    version = 1u;
    geometryId = other.geometryId;
    pointsCount = other.pointsCount;
    ftkFiducial* outPoint( fiducials );
    const ftk3DPoint* inPoint( other.positions );
    for ( uint32_t i( 0u ); i < std::min( pointsCount, FTK_MAX_FIDUCIALS ); ++i, ++inPoint, ++outPoint )
    {
        outPoint->position = *inPoint;
    }

    return *this;
}
#endif

/** \brief Enumeration allowing to select which information from the considered registered rigid body must
 * be retrieved by the ftkGetRigidBodyProperty function.
 */
TYPED_ENUM( uint32, ftkRigidBodyInfoGetter )
// clang-format off
{
    /** \brief The wanted information indicates whether the rigid body description will end up in a legacy
     * marker or a 64-fiducials marker being reconstructed.
     */
    GeometryType,
    /** \brief The wanted information indicates whether the rigid body describes a 4π marker or any other
     * marker.
     */
    IsFourPi,
    /** \brief The wanted information is the geometry-specific allowed missing points (set in the geometry
     * file, available from version 2).
     */
    AllowedMissingPoint,
    /** \brief The wanted information is the position of the fiducial.
     */
    FiducialPosition,
    /** \brief The wanted information is the coordinates of the normal to the fiducial.
     */
    FiducialNormal,
    /** \brief The wanted information is the fiducial type.
     */
    FiducialType,
    /** \brief The wanted information is the position of the divot.
     */
    DivotPosition,
    /** \brief The wanted information is the geometry-wide angular threshold for registration of 4π marker,
     * additionally the fiducial-specific view angle and angular threshold can be gotten as well.
     */
    AngularThresholds,
};
// clang-format on

/** \brief Structure used when getting information from a registered rigid body.
 */
STRUCT_BEGIN( ftkRigidBodyInformation )
{
#ifdef __cplusplus
    /** \brief Default constructor.
     *
     * The default constructor builds an instance with invalid version, invalid geometry ID, 0 points and
     * 0 divots.
     */
    ftkRigidBodyInformation();
    /** \brief Default implementation of copy-constructor.
     *
     * \param[in] other instance to duplicate.
     */
    ftkRigidBodyInformation( const ftkRigidBodyInformation& other ) = default;
    /** \brief Default implementation of move-constructor.
     *
     * \param[in] other instance to move.
     */
    ftkRigidBodyInformation( ftkRigidBodyInformation && other ) = default;
    /** \brief Default implementation of the destructor.
     */
    ~ftkRigidBodyInformation() = default;
    /** \brief Default implementation of affectation operator.
     *
     * \param[in] other instance to duplicate.
     *
     * \retval *this as a reference
     */
    ftkRigidBodyInformation& operator=( const ftkRigidBodyInformation& other ) = default;
    /** \brief Default implementation of move-affectation operator.
     *
     * \param[in] other instance to move.
     *
     * \retval *this as a reference
     */
    ftkRigidBodyInformation& operator=( ftkRigidBodyInformation&& other ) = default;
#endif
    /** \brief Version of the file from which the instance was loaded.
     */
    uint32_t Version;
    /** \brief Unique ID of the geometry.
     */
    uint32_t GeometryId;
    /** \brief Number of fiducials defined by the geometry.
     */
    uint32_t NumberOfFiducials;
    /** \brief Number of divots defined by the geometry.
     */
    uint32_t NumberOfDivots;
    union
    {
        /** \brief Container for the 3 floats.
         *
         * This container is for instance used when getting the 3 coordinates of the fiducial position, or
         * normal components, or divot position.
         */
        float FloatTriplet[ 3u ];
        /** \brief Storage for (unsigned) integer values.
         *
         * This member is set when the number of allowed missing point is retrieved.
         */
        uint32_t IntegerValue;
        /** \brief Storage for bool values.
         *
         * This member is set when  is retrieved.
         */
        bool BooleanValue;
        /** \brief Storage for fiducial type value.
         *
         * This member is set when retrieving the fiducial type.
         */
        ftkFiducialType TypeOfFiducial;
    };
}
STRUCT_END( ftkRigidBodyInformation );

#ifdef __cplusplus
inline ftkRigidBodyInformation::ftkRigidBodyInformation()
    : Version{ std::numeric_limits< uint32_t >::max() }
    , GeometryId{ 0u }
    , NumberOfFiducials{ 0u }
    , NumberOfDivots{ 0u }
{
}
#endif

#ifdef GLOBAL_DOXYGEN

/**
 * \}
 *
 * \addtogroup frame
 * \{
 */
#endif

#if !defined( __cplusplus ) || ( defined( ATR_MSVC ) && _MSC_VER < 1900 )
#define INVALID_ID 0xffffffffu
#else

/** Define an invalid fiducial correspondence
 */
constexpr uint32 INVALID_ID( 0xffffffffu );
#endif

/** \brief Marker data after left-right triangulation and marker reconstruction.
 *
 * Marker are retrieved within the 3D fiducials data based on their unique
 * geometry. When several markers are used, it is recommended to use specific
 * geometries in order to provide a unique tracking.
 *
 * Tracking id is provided to differentiate between several markers of the same
 * geometry. The id is reset when less than 3 spheres composing the marker are
 * visible or if the marker cannot be found again from its last known position.
 *
 * The geometryPresenceMask allows to get the correspondence between the
 * indices of fiducials specified in the geometry and 3D fiducial
 * (ftk3DFiducial) indexes of the current measure. When a match is invalid, it
 * is set to \c INVALID_ID. Alternatively, valid matches can be retrieved via
 * the geometryPresenceMask.
 *
 * Marker rigid transformation (rotation and translation) is the transformation
 * from the geometry referential to the measures of the sensor.
 *
 * Registration error is the mean distance of geometry and measured fiducials,
 * expressed in the same referential.
 *
 * \see ftkGeometry
 * \see ftk3DFiducial
 */
PACK1_STRUCT_BEGIN( ftkMarker )
{
    /** \brief Status of the marker.
     */
    ftkStatus status;

    /** \brief Tracking id.
     */
    uint32 id;

    /** \brief Geometric id, i.e. the unique id of the used geometry.
     */
    uint32 geometryId;

    /** \brief Presence mask of fiducials expressed as their geometrical
     * indices.
     */
    uint32 geometryPresenceMask;

    /** \brief Correspondence between geometry index and 3D fiducials indices
     * or \c INVALID_ID.
     */
    uint32 fiducialCorresp[ FTK_MAX_FIDUCIALS ];

    /** \brief Rotation matrix: format [row][column].
     */
    floatXX rotation[ 3 ][ 3 ];

    /** \brief Translation vector (unit mm).
     */
    floatXX translationMM[ 3 ];

    /** \brief Registration mean error (unit mm).
     */
    floatXX registrationErrorMM;
}
PACK1_STRUCT_END( ftkMarker );

/** \brief Marker data after left-right triangulation and marker reconstruction, with up to 64 fiducials.
 *
 * This structure is the equivalent of ftkMarker, but handling up to 64 fiducials. The differences are
 * - ftkMarker64Fiducials::geometryPresenceMask is a \c uint64;
 * - ftkMarker64Fiducials::fiducialCorresp is an array of 64 \c uint32.
 */
PACK1_STRUCT_BEGIN( ftkMarker64Fiducials )
{
#ifdef __cplusplus
    ftkMarker64Fiducials();
#endif

    /** \brief Status of the marker.
     */
    ftkStatus status;

    /** \brief Tracking id.
     */
    uint32 id;

    /** \brief Geometric id, i.e. the unique id of the used geometry.
     */
    uint32 geometryId;

    /** \brief Presence mask of fiducials expressed as their geometrical
     * indices.
     */
    uint64 geometryPresenceMask;

    /** \brief Correspondence between geometry index and 3D fiducials indices
     * or \c INVALID_ID.
     */
    uint32 fiducialCorresp[ FTK_MAX_FIDUCIALS_64 ];

    /** \brief Rotation matrix: format [row][column].
     */
    floatXX rotation[ 3 ][ 3 ];

    /** \brief Translation vector (unit mm).
     */
    floatXX translationMM[ 3 ];

    /** \brief Registration mean error (unit mm).
     */
    floatXX registrationErrorMM;
}
PACK1_STRUCT_END( ftkMarker64Fiducials );

#ifdef __cplusplus
inline ftkMarker64Fiducials::ftkMarker64Fiducials()
    : id{ 0u }
    , geometryId{ 0u }
    , geometryPresenceMask{ 0uLL }
    , registrationErrorMM{ -1.f }
{
    status = 0u;
    std::fill(
      std::begin( fiducialCorresp ), std::end( fiducialCorresp ), std::numeric_limits< uint32 >::max() );
    std::for_each( std::begin( rotation ),
                   std::end( rotation ),
                   []( floatXX( &myArray )[ 3u ] )
                   { std::fill( std::begin( myArray ), std::end( myArray ), 0.f ); } );
    std::fill( std::begin( translationMM ), std::end( translationMM ), 0.f );
}
#endif

/** \brief Store all data from a pair of images
 *
 * This structure stores all the buffers that can be retrieved from a pair of
 * images.
 *
 * The control of the fields to be retrieved can be specified during
 * initialization.
 *
 * Fields can be grouped in different sections:
 *   - imageHeader: get current image information;
 *   - imageLeftPixels: get left image pixels;
 *   - imageRightPixels: get right image pixels;
 *   - rawDataLeft: retrieve the raw data from left image;
 *   - rawDataRight: retrieve the raw data from right image;
 *   - threeDFiducials: retrieve 3D positions of left-right matches;
 *   - markers: retrieve marker poses (rotation + translation);
 *   - events: retrieve events.
 *
 * Every group of fields must be initialised to retrieve corresponding
 * information. The following example presents such an initialisation:
 * \code
 * ftkFrameQuery* fq( ftkCreateFrame );
 *
 * // Initialize a buffer of 100 raw data coming from the left image
 * ftkSetFrameOptions( false, 0u, 100u, 0u, 0u, 0u, fq );
 * // fq->rawDataLeftVersionSize.Version will be set after calling ftkGetLastFrame
 * // fq->rawDataLeftCount will be set after calling ftkGetLastFrame
 * // fq->rawDataLeftStat will be set after calling ftkGetLastFrame
 * \endcode
 *
 * Notes:
 *   - imageHeader, imageLeftPixels, imageRightPixels have only one occurrence;
 *   - imageLeftVersionSize and imageRightVersionSize present the allocated
 *     size for the image;
 *   - the different status inform if the operation is successful or not.
 *
 * \warning The order of elements in the ftkFrameQuery::rawDataLeft,
 * ftkFrameQuery::rawDataRight, ftkFrameQuery::threeDFiducials,
 * ftkFrameQuery::markers and ftkFrameQuery::events containers is \e not
 * stable, i.e. it is \e not guaranteed. No application should rely on the
 * order of the element in those containers.
 */
PACK1_STRUCT_BEGIN( ftkFrameQuery )
{
    /** \brief Address where the image header will be written (input).
     */
    ftkImageHeader* imageHeader;

    /** \brief Version and \c sizeof( ftkImageHeader \c ) (input).
     */
    ftkVersionSize imageHeaderVersionSize;

    /** \brief Status of the image header, written by ftkGetLastFrame (output).
     */
    ftkQueryStatus imageHeaderStat;

    /** \brief Internal usage.
     */
    void* internalData;

    /** \brief Address of a pointer array of retrieved events (input).
     */
    ftkEvent** events;

    /** \brief Version and \c sizeof( \c events \c ) (input).
     */
    ftkVersionSize eventsVersionSize;

    /** \brief Number of events written by the SDK (output).
     */
    uint32 eventsCount;

    /** \brief Status of the events container (output).
     */
    ftkQueryStatus eventsStat;

    /** \brief Address where the left pixels will be written (input).
     */
    uint8* imageLeftPixels;

    /** \brief Version and \c sizeof( \c imageLeftPixels \c ), which must be
     * at least \f$ imageStrideInBytes \cdot height \f$ (input).
     */
    ftkVersionSize imageLeftVersionSize;

    /** \brief Status of the imageLeftPixels container (output).
     */
    ftkQueryStatus imageLeftStat;

    /** \brief Address where the right pixels will be written (input).
     */
    uint8* imageRightPixels;

    /** \brief Version and \c sizeof( \c imageLeftPixels \c ), which must be
     * at least \f$ imageStrideInBytes \cdot height \f$ (input).
     */
    ftkVersionSize imageRightVersionSize;

    /** \brief Status of the imageLeftPixels container (output).
     */
    ftkQueryStatus imageRightStat;

    /** \brief Address where the left raw data will be written (input).
     */
    ftkRawData* rawDataLeft;

    /** \brief Version and \c sizeof( \c rawDataLeft \c ) (input).
     */
    ftkVersionSize rawDataLeftVersionSize;

    /** \brief Number of left raw data written by the SDK (output).
     */
    uint32 rawDataLeftCount;

    /** \brief Status of the rawDataLeft container (output).
     */
    ftkQueryStatus rawDataLeftStat;

    /** \brief Address where the right raw data will be written (input).
     */
    ftkRawData* rawDataRight;

    /** \brief Version and \c sizeof( \c rawDataRight \c ) (input).
     */
    ftkVersionSize rawDataRightVersionSize;

    /** \brief Number of right raw data written by the SDK (output).
     */
    uint32 rawDataRightCount;

    /** \brief Status of the rawDataRight container (output).
     */
    ftkQueryStatus rawDataRightStat;

    /** \brief Address where the 3D fiducial data will be written (input).
     */
    ftk3DFiducial* threeDFiducials;

    /** \brief Version and \c sizeof( \c threeDFiducials \c ) (input).
     */
    ftkVersionSize threeDFiducialsVersionSize;

    /** \brief Number of 3D fiducial data written by the SDK (output).
     */
    uint32 threeDFiducialsCount;

    /** \brief Status of the threeDFiducials container (output).
     */
    ftkQueryStatus threeDFiducialsStat;

    /** \brief Address where the marker data will be written (input).
     */
    ftkMarker* markers;

    /** \brief Version and \c sizeof( \c markers \c ) (input).
     */
    ftkVersionSize markersVersionSize;

    /** \brief Number of marker data written by the SDK (output).
     */
    uint32 markersCount;

    /** \brief Status of the markers container (output).
     */
    ftkQueryStatus markersStat;

    /** \brief Address where the 64-fiducials marker data will be written (input).
     */
    ftkMarker64Fiducials* sixtyFourFiducialsMarkers;

    /** \brief Version and \c sizeof( \c sixtyFourFiducialsMarkers \c ) (input).
     */
    ftkVersionSize sixtyFourFiducialsMarkersVersionSize;

    /** \brief Number of 64-fidudials marker data written by the SDK (output).
     */
    uint32 sixtyFourFiducialsMarkersCount;

    /** \brief Status of the sixtyFourFiducialsMarkers container (output).
     */
    ftkQueryStatus sixtyFourFiducialsMarkersStat;
}
PACK1_STRUCT_END( ftkFrameQuery );

/** \brief Structure holding the information about the synchronisation of
 * the strobe and the exposure.
 */
PACK1_STRUCT_BEGIN( ftkStrobeInfo )
{
    /** \brief Contains 1 if the strobe for the left camera is synchronised
     * with exposure.
     */
    uint32 LeftStrobeEnabled : 1;
    /** \brief Contains 1 if the strobe for the right camera is synchronised
     * with exposure.
     */
    uint32 RightStrobeEnabled : 1;
#ifdef __cplusplus
private:
#endif
    /** \brief Reserved space, not used.
     */
    uint32 Reserved : 30;
}
PACK1_STRUCT_END( ftkStrobeInfo );

/** \brief Structure holding the parameters of \e one camera.
 *
 * \see http://www.vision.caltech.edu/bouguetj/calib_doc/htmls/parameters.html
 */
PACK1_STRUCT_BEGIN( ftkCameraParameters )
{
    /** \brief Focal length along the \f$ u \f$ and \f$ v \f$ axes.
     */
    float FocalLength[ 2u ];
    /** \brief Optical centre position.
     */
    float OpticalCentre[ 2u ];
    /** \brief Picture distorsion parameters (radial and tangential distorsions).
     */
    float Distorsions[ 5u ];
    /** \brief Skew parameter, i.e. angle between \f$ u \f$ and \f$ v \f$
     * pixels.
     */
    float Skew;
}
PACK1_STRUCT_END( ftkCameraParameters );

/** \brief Structure holding the parameters of the \e stereo camera \e system.
 */
PACK1_STRUCT_BEGIN( ftkStereoParameters )
{
    /** \brief Left camera parameters.
     */
    ftkCameraParameters LeftCamera;
    /** \brief Right camera parameters.
     */
    ftkCameraParameters RightCamera;
    /** \brief Position of the right camera in the left camera coordinate
     * system.
     */
    float Translation[ 3u ];
    /** \brief Orientation of the right camera in the left camera coordinate
     * system.
     */
    float Rotation[ 3u ];
}
PACK1_STRUCT_END( ftkStereoParameters );

/** \brief Structure used to retrieve information from the frame.
 */
PACK1_STRUCT_BEGIN( ftkFrameInfoData )
{
    /** \brief Allows to select which information will be retrieved.
     */
    ftkInformationType WantedInformation;
    union
    {
        /** \brief Structure storing strobe synchronisation with exposure.
         */
        ftkStrobeInfo StrobeInfo;
        /** \brief Structure storing calibration parameters.
         */
        ftkStereoParameters Calibration;
    };
}
PACK1_STRUCT_END( ftkFrameInfoData );

#ifdef GLOBAL_DOXYGEN

/**
 * \}
 */
#endif

/** System dependant function prefixes for DLLs
 */
#ifdef ATR_WIN
#define _CDECL_ __cdecl
#else
#define _CDECL_
#endif

#ifdef GLOBAL_DOXYGEN

/**
 * \addtogroup deviceSDK
 * \{
 */
#endif

/** \brief Callback for device enumeration.
 *
 * This is the signature for the device enumeration callback.
 *
 * \param[in] sn serial number of the enumerated device.
 * \param[in,out] user pointer on user data.
 * \param[in] type type of the device.
 */
typedef void( _CDECL_* ftkDeviceEnumCallback )( uint64 sn, void* user, ftkDeviceType type );
#ifdef GLOBAL_DOXYGEN

/**
 * \}
 */
#endif

#ifdef GLOBAL_DOXYGEN

/**
 * \addtogroup options
 * \{
 */
#endif

/** \brief Callback for options enumeration.
 *
 * This is the signature for the device option enumeration callback.
 *
 * \param[in] sn serial number of the enumerated device.
 * \param[in,out] user pointer on user data.
 * \param[in] oi pointer on the enumerated option information.
 */
typedef void( _CDECL_* ftkOptionsEnumCallback )( uint64 sn, void* user, ftkOptionsInfo* oi );
#ifdef GLOBAL_DOXYGEN

/**
 * \}
 */
#endif

#ifdef GLOBAL_DOXYGEN

/**
 * \addtogroup geometries
 * \{
 */
#endif

/** \brief Callback for geometry enumeration.
 *
 * This is the signature for the geometry enumeration callback.
 *
 * \param[in] sn serial number of the enumerated device.
 * \param[in,out] user pointer on user data.
 * \param[in] in pointer on the enumerated ftkGeometry instance.
 */
typedef void( _CDECL_* ftkGeometryEnumCallback )( uint64 sn, void* user, ftkGeometry* in );

/** \brief Callback for advanced geometry enumeration.
 *
 * This is the signature for the geometry enumeration callback.
 *
 * \param[in] sn serial number of the enumerated device.
 * \param[in,out] user pointer on user data.
 * \param[in] in pointer on the enumerated ftkRigidBody instance.
 */
typedef void( _CDECL_* ftkRigidBodyEnumCallback )( uint64 sn, void* user, ftkRigidBody* in );

/** \brief Callback for advanced geometry enumeration.
 *
 * This is the signature for the geometry enumeration callback.
 *
 * \param[in] sn serial number of the enumerated device.
 * \param[in,out] user pointer on user data.
 * \param[in] in pointer on the enumerated ftkRigidBodyInformation instance.
 */
typedef void( _CDECL_* ftkRegisteredRigidBodyEnumCallback )( uint64 sn,
                                                             void* user,
                                                             ftkRigidBodyInformation* in );

#ifdef GLOBAL_DOXYGEN

/**
 * \}
 */
#endif
// ----------------------------------------------------------------
#ifdef GLOBAL_DOXYGEN

/** \addtogroup library
 * \{
 */
#else

/** \defgroup library General Library Functions
 * \brief Functions to initialize and close the library.
 * \{
 */
#endif

/** \brief Function initialising the library.
 *
 * This function initialises the library and creates the handle needed by the
 * other library functions. It must therefore be called before any other
 * function from the library.
 *
 * \return the library handle or \c nullptr if an error occurred.
 *
 * \code
 * // Initialize and close library
 *
 * ftkLibrary handle( ftkInit() );
 * if ( handle == nullptr )
 * {
 *     ERROR( "Cannot open library" );
 * }
 *
 * // ...
 *
 * if ( ftkClose( &handle ) != ftkError::FTK_OK )
 * {
 *     ERROR( "Cannot close library" );
 * }
 * \endcode
 *
 * \see ftkClose
 */
ATR_EXPORT ftkLibrary ftkInit();

/** \brief `Extended' initialisation of the library.
 *
 * This function allows to customise the library default initialisation.
 * \if STK
 * It currently provides no customisation.
 * \else
 * It currently provides support for changing the IP address of the device it
 * connects to. Please note the device port default value cannot be changed.
 * The configuration file uses
 * <a href="http://www.json.org/" target="_blank">JSON</a> format, with the
 * following syntax:
 * \verbatim
 {
   "network" :
   {
     "version": 1,
     "interfaces": [
       {
         "address": "172.17.10.7",
         "port": 3509
       }
     ]
   }
 }
 \endverbatim
 * If several interfaces are defined, then the SDK will allow to connect to
 * multiple fusionTrack devices. This means the ftkEnumerateDevices will be
 * able to detect up to one device per defined interface. Each function
 * taking a serial number as input argument will behave correctly, i.e. will
 * interact with the device with the given serial number. When using multiple
 * fusionTrack with the same SDK, it is recommended to set them to use distinct
 * subnetworks (i.e. only the two first bytes of the IP address match).
 * \endif
 * The function also allows the caller to get information about initialisation
 * errors, as shown on the following snippet:
 * \code
 * ftkBuffer buffer;
 * ftkLibrary lib( ftkInitExt( "configuration.json", &buffer ) );
 *
 * if ( lib == nullptr )
 * {
 *     cerr << buffer.data << endl;
 *     cerr << "Cannot initialise library" << endl;
 *     return 1;
 * }
 * \endcode
 *
 * \param[in] fileName name of the JSON file from which the configuration will
 * be read.
 * \param[out] errs pointer on the buffer containing potential errors.
 *
 * \return the library handle or \c nullptr if an error occurred.
 */
ATR_EXPORT ftkLibrary ftkInitExt( const char* fileName, ftkBuffer* errs );

/** \brief Close the library.
 *
 * This function destroys the library handle and frees the resources. Any
 * call to a library function after this will fail.
 *
 * \param[in] ptr pointer an initialised library handle
 *
 * \retval ftkError::FTK_OK if the library could be closed successfully,
 * \retval ftkError::FTK_ERR_INV_PTR if the \c ptr is \c 0 or if the library handle
 * pointed by \c ptr has already been closed.
 *
 * \see ftkInit
 */
ATR_EXPORT ftkError ftkClose( ftkLibrary* ptr );

/** \brief Getter for the library version
 *
 * This function allows to access the current SDK version as a string. The
 * format is Major.Minor.Revision.Build (Unix timestamp).
 *
 * \param[out] bufferOut pointer on the output buffer, contains the data and
 * the actual data size.
 */
ATR_EXPORT void ftkVersion( ftkBuffer* bufferOut );

/** \brief Getter for the last error.
 *
 * This function allows to retrieve the last error (i.e. the error from the
 * previous call to another SDK function). The error is formatted as an XML
 * string:
 * \verbatim
 <ftkError>
   <errors> ... </errors>
   <warnings> ... </warnings>
   <messages> ... </messages>
 </ftkError>
 \endverbatim
 * The errors tag contains the error codes and strings, the warning tag
 * contains the warning codes and string. The messages tag contains optional
 * extra messages.
 *
 * \param[in] lib initialised library handle.
 * \param[in] strSize size of the string passed as argument.
 * \param[in,out] str allocated character array containing the output XML.
 *
 * \retval ftkError::FTK_OK if the retrieving could be done successfully,
 * \retval ftkError::FTK_ERR_INV_PTR if lib or \c str is \c 0.
 */
ATR_EXPORT ftkError ftkGetLastErrorString( ftkLibrary lib, size_t strSize, char str[] );

/** \brief Getter for the `explanation' of a status code.
 *
 * This function allows to get the message associated to a status code.
 *
 * \param[in] status code for which the string must be retrieved.
 * \param[out] buffer pointer on the output buffer, contains the data
 * and the actual data size.
 *
 * \retval ftkError::FTK_OK if the message could be retrieved successfully,
 * \retval ftkError::FTK_ERR_INV_PTR if \c buffer is \c nullptr,
 * \retval ftkError::FTK_ERR_INTERNAL if an unexpected error occurred (i.e. the
 * \c status value is unknown).
 */
ATR_EXPORT ftkError ftkStatusToString( ftkError status, ftkBuffer* buffer );

/** \} */

// ----------------------------------------------------------------
#ifdef GLOBAL_DOXYGEN

/** \addtogroup deviceSDK
 * \{
 */
#else

/** \defgroup deviceSDK Device Functions
 * \brief Function to enumerate devices
 * \{
 */
#endif

/** \brief Enumerate available tracker devices.
 *
 * This function allows to scan all the connected devices and to call the
 * user-defined callback function for any found device.
 *
 * \param[in] lib an initialised library handle
 * \param[in] cb the device enumeration callback
 * \param[in] user parameter of the callback
 *
 * \code
 * void deviceEnumCallback( uint64 sn, void* user, ftkDeviceType type )
 * {
 *     uint64* lastDevice = (uint64*) user;
 *     if ( lastDevice != 0 )
 *     {
 *         lastDevice = sn;
 *     }
 * }
 *
 * main()
 * {
 *     // Initialize library (see ftkInit example)
 *
 *     uint64 sn( 0uLL );
 *     if ( ftkEnumerateDevices( lib, deviceEnumCallback, &sn ) != ftkError::FTK_OK )
 *     {
 *         ERROR( "Cannot enumerate devices" );
 *     }
 *     if ( sn == 0uLL )
 *     {
 *         ERROR( "No device connected" );
 *     }
 *
 *     // ...
 * }
 * \endcode
 *
 * \see ftkInit
 * \see ftkClose
 *
 * \retval ftkError::FTK_OK if the enumeration could be done correctly,
 * \retval ftkError::FTK_ERR_INV_PTR if the \c lib handle was not correctly
 * initialised,
 * \retval ftkError::FTK_WAR_CALIB_AUTHENTICATION if the calibration file
 * authentication could not be checked.
 *
 * \critical This function is involved in the device detection chain.
 */
ATR_EXPORT ftkError ftkEnumerateDevices( ftkLibrary lib, ftkDeviceEnumCallback cb, void* user );

/** \brief Opens the XML file for the dump.
 *
 * This function opens a XML file in which the data will be dumped. If a file
 * is already opened, an error is returned.
 *
 * \param[in] lib initialised library handle.
 * \param[in] serialNbr serial number of the device for which the file is
 * created.
 * \param[in] fileName name of the XML file to create (if not provided, then a
 * file Dump_serialNumber.xml is created).
 *
 * \retval ftkError::FTK_OK if the file could be successfully opened and
 * written,
 * \retval ftkError::FTK_ERR_INV_PTR if \c lib is \c nullptr,
 * \retval ftkError::FTK_ERR_INV_SN if \c serialNbr is not a valid serial
 * number,
 * \retval ftkError::FTK_ERR_WRITE if a file is already opened or if if the file
 * cannot be opened.
 */
ATR_EXPORT ftkError ftkOpenDumpFile( ftkLibrary lib, uint64 serialNbr, const char* fileName );

/** \brief Closes the XML dump file.
 *
 * This function closes the XML file used for dump. Any call to
 * stereoDumpDeviceInfo or stereoDumpFrame will fail. If no files are opened an
 * error is returned.
 *
 * \param[in] lib initialised library handle,
 * \param[in] serialNbr device serial number.
 *
 * \retval ftkError::FTK_OK if the file could be successfully written and
 * closed,
 * \retval ftkError::FTK_ERR_INV_PTR if \c lib is \c nullptr,
 * \retval ftkError::FTK_ERR_INV_SN if \c serialNbr is not a valid serial
 * number,
 * \retval ftkError::FTK_ERR_WRITE if a file is not opened.
 */
ATR_EXPORT ftkError ftkCloseDumpFile( ftkLibrary lib, uint64 serialNbr );

/** \brief Function dumping the device information.
 *
 * The device information consist of the option and internal register values.
 * If no files are opened an error is returned.
 *
 * \param[in] lib initialised library handle,
 * \param[in] sn device serial number.
 *
 * \retval ftkError::FTK_OK if the file could be written.
 * \retval ftkError::FTK_ERR_INV_PTR if \c lib is \c nullptr,
 * \retval ftkError::FTK_ERR_INV_SN if \c sn is not valid,
 * \retval ftkError::FTK_ERR_WRITE if the file could not be written or was not
 * opened,.
 */
ATR_EXPORT ftkError ftkDumpInfo( ftkLibrary lib, uint64 sn );

/** \} */

// ----------------------------------------------------------------
#ifdef GLOBAL_DOXYGEN

/** \addtogroup frame
 * \{
 */
#else

/** \defgroup frame Frame Functions
 * \brief Functions to acquire frames (data from the tracking system)
 * \{
 */
#endif

/** \brief Creates a frame instance.
 *
 * This function allows to initialise a frame instance. The following
 * example is valid.
 *
 * \code
 * ftkFrameQuery* frame( ftkCreateFrame() );
 *
 * if ( frame == 0 )
 * {
 *     // error management.
 *     return;
 * }
 * \endcode
 *
 * \warning Not using this function may lead to problems getting the data
 * when using ftkGetLastFrame.
 *
 * \warning The user must call ftkDeleteFrame to correctly deallocate the
 * memory.
 *
 * \return a ftkFrameQuery pointer, or \c nullptr if an error occurred.
 *
 */
ATR_EXPORT ftkFrameQuery* ftkCreateFrame();

/** \brief Initialises a frame.
 *
 * This function allows to initialise a frame instance, which content can
 * be parametrised. This function can also be used to reinitialise an
 * instance, e.g. to increase the number of retrieved markers. The
 * following example is valid.
 *
 * \code
 * ftkFrameQuery* frame( ftkCreateFrame() );
 *
 * if ( frame == 0 )
 * {
 *     // error management.
 *     return;
 * }
 *
 * if ( ftkSetFrameOptions( false, 10u, 128u, 128u, 100u, 10u, frame ) != ftkError::FTK_OK )
 * {
 *   // error management
 *   return;
 * }
 *
 * if ( ftkSetFrameOptions( false, 10u, 64u, 64u, 42u, 10u, frame ) != ftkError::FTK_OK )
 * {
 *   // error management
 *   return;
 * }
 * \endcode
 *
 * \warning Not using this function will lead to problems getting the data
 * when using ftkGetLastFrame().
 *
 * \warning The user must call ftkDeleteFrame() to correctly deallocate the
 * memory.
 *
 * \param[in] pixels should be \c true for the left and right pictures to be
 * retrieved.
 * \param[in] eventsSize maximal number of retrieved ftkEvents
 * instances for the left camera.
 * \param[in] leftRawDataSize maximal number of retrieved ftkRawData
 * instances for the left camera.
 * \param[in] rightRawDataSize maximal number of retrieved ftkRawData
 * instances for the right camera.
 * \param[in] threeDFiducialsSize maximal number of retrieved ftk3DFiducial
 * instances.
 * \param[in] markersSize maximal number of retrieved ftkMarker instances.
 * \param[in,out] frame pointer on an initialised ftkFrameQuery instance.
 *
 * \retval ftkError::FTK_OK if the option setting could be successfully performed,
 * \retval ftkError::FTK_ERR_INIT if the \c frame pointer was not initialised using
 * ftkCreateFrame,
 * \retval ftkError::FTK_ERR_INV_INDEX if the wanted container size cannot be allocated (as its size in \e
 * bytes is stored in a \c uint32 number),
 * \retval ftkError::FTK_ERR_INV_PTR if an allocation failed.
 *
 * \critical This function is involved in ftkFrameQuery management.
 */
ATR_EXPORT ftkError ftkSetFrameOptions( bool pixels,
                                        uint32 eventsSize,
                                        uint32 leftRawDataSize,
                                        uint32 rightRawDataSize,
                                        uint32 threeDFiducialsSize,
                                        uint32 markersSize,
                                        ftkFrameQuery* frame );

/** \brief Frees the memory from the allocated fields.
 *
 * This function deallocates the used memory for the \e members of the
 * instance, \c not the instance itself!. The
 * following example is valid.
 *
 * \code
 * ftkFrameQuery* frame = ftkCreateFrame();
 *
 * if ( frame == 0 )
 * {
 *     // error management.
 *     return;
 * }
 *
 * if ( ftkSetFrameOptions( false, 10u, 128u, 128u, 100u, 10u, frame ) !=
 *FTK_OK )
 * {
 *   // error management
 *   return;
 * }
 *
 * if ( ftkSetFrameOptions( false, 10u, 64u, 64u, 42u, 10u, frame ) != ftkError::FTK
 *)
 * {
 *   // error management
 *   return;
 * }
 *
 * err = ftkDeleteFrame( frame );
 * if ( err != ftkError::FTK_OK )
 * {
 *     // error management
 *     return;
 * }
 *
 * // Using frame will crash the API, as it was deleted!
 * \endcode
 *
 * \param[in] frame pointer on the instance which will be deleted.
 *
 * \retval ftkError::FTK_OK if the cleaning performed successfully,
 * \retval ftkError::FTK_ERR_INIT if the \c frame pointer was not initialised using
 * ftkCreateFrame.
 *
 * \critical This function is involved in ftkFrameQuery management.
 */
ATR_EXPORT ftkError ftkDeleteFrame( ftkFrameQuery* frame );

/** \brief Retrieves the number of data of each type for the \e previous frame.
 *
 * This function allows to know how many elements of each type were available
 * in the \e previous frame. This allows to know how many data were lost when
 * a container status is ftkQueryStatus::QS_ERR_OVERFLOW.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 * \param[out] what type of info to retrieve.
 * \param[out] value pointer on the retrieved value.
 *
 * \retval ftkError::FTK_OK if the information could be successfully retrieved,
 * ftkError::FTK_ERR_INV_PTR if the \c lib handle was not correctly
 * initialised or if the \c value is null or if the internal
 * data for the picture could not be allocated,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_WAR_NOT_SUPPORTED if the desired information is not
 * handled.
 */
ATR_EXPORT ftkError ftkGetTotalObjectNumber( ftkLibrary lib, uint64 sn, ftkDataType what, uint32* value );

/** \brief Retrieve the latest available frame.
 *
 * A frame contains all the data related to a pair of image. The frame query
 * structure must be initialised prior to this function call in order to
 * specify what type of information should be retrieved.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 * \param[in,out] frameQueryInOut an initialised ftkFrameQuery structure.
 * \param[in] timeoutMS returns FTK_WAR_NO_FRAME if no frame is available
 * within X ms.
 *
 * \code
 * main ()
 * {
 *     // Initialize library (see ftkInit() example)
 *     // Get an attached device (see ftkEnumerateDevices() example)
 *
 *     ftkFrameQuery* fq( ftkCreateFrame() );
 *     if ( fq == 0 )
 *     {
 *         ERROR( "Error allocating frame" );
 *     {
 *     if ( ftkSetFrameOptions( false, 0u, 0u, 0u, 0u, 16u, fq ) != ftkError::FTK_OK )
 *     {
 *         ERROR( "Error initialising frame" );
 *     }
 *
 *     // Wait until the next frame is available
 *     if ( ftkGetLastFrame( lib, sn, fq, 10u ) != ftkError::FTK_OK )
 *     {
 *         ERROR ("Error acquiring frame");
 *     }
 *
 *     if ( fq->markersStat == ftkQueryStatus::QS_OK )
 *     {
 *         for ( uint32 u( 0u ); u < fq->markersCount; ++u )
 *         {
 *             // Access marker data here (fq->markers[ u ] ...)
 *         }
 *     }
 * }
 * \endcode
 *
 * \see ftkInit
 * \see ftkEnumerateDevices
 *
 * \retval ftkError::FTK_OK if the frame could be retrieved correctly,
 * \retval ftkError::FTK_ERR_INIT if the \c frame pointer was not initialised
 * using ftkCreateFrame,
 * \retval ftkError::FTK_ERR_INV_PTR if the \c lib handle was not correctly
 * initialised or if the \c frameQueryInOut is null or if the internal
 * data for the picture could not be allocated,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_INTERNAL if the triangulation or the marker
 * matcher class are not properly initialised  or if no image are retrieved or
 * if the image size is invalid or if a compressed image is corrupted,
 * \retval ftkError::FTK_ERR_COMP_ALGO if the temperature compensation
 * algorithm is undefined,
 * \retval ftkError::FTK_ERR_SYNC if the retrieved pictures are not
 * synchronised,
 * \retval ftkError::FTK_WAR_NO_FRAME if no frame are available,
 * \retval ftkError::FTK_ERR_SEG_OVERFLOW if an overflow occurred during image
 * segmentation,
 * \retval ftkError::FTK_ERR_ALGORITHMIC_WALLTIME if the processing time exceed
 * the set walltime for the current frame,
 * \retval ftkError::FTK_ERR_IMG_DEC if a picture cannot be decompressed,
 * \retval ftkError::FTK_ERR_IMG_FMT if the gotten picture data are not
 * compatible with the SDK,
 * \retval ftkError::FTK_ERR_IMPAIRING_CALIB if the calibration stored in the
 * device prevents triangulation,
 * \retval ftkError::FTK_WAR_REJECTED_PIC of the pictures were too big and not
 * sent by the device,
 * \retval ftkError::FTK_WAR_SHOCK_DETECTED if a shock which potentially
 * decalibrated the device has been detected,
 * \retval ftkError::FTK_WAR_SHOCK_SENSOR_OFFLINE if the shock sensor is
 * currently offline,
 * \retval ftkError::FTK_WAR_TEMP_INVALID if the last temperature reading was
 * invalid.
 */
ATR_EXPORT ftkError ftkGetLastFrame( ftkLibrary lib,
                                     uint64 sn,
                                     ftkFrameQuery* frameQueryInOut,
                                     uint32 timeoutMS );

/** \brief Reprocess the given frame.
 *
 * Frame reprocessing allows the user to reprocess only a part of the contained
 *  data, i.e. the markers, or 3D and markers. The current implementation does
 * \e not support a built-in reprocessing of the pictures, meaning that pixel
 * reprocessing must be done by a user defined function.
 *
 * The user must specify which data must be reprocessed by setting the
 * corresponding status flag to ftkQueryStatus::QS_REPROCESS before calling the
 * function. This flag is recursive, in the sense that asking for a
 * reprocessing of the raw data will trigger a reprocessing of the 3D fiducials
 * and the markers.
 *
 * \warning Does \e not supply pixel reprocessing!
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 * \param[in,out] frameQueryInOut an initialised ftkFrameQuery structure.
 *
 * \retval ftkError::FTK_OK if the frame could be reprocessed correctly (which
 * may indicate that no reprocessing was actually asked as well),
 * \retval ftkError::FTK_ERR_INV_PTR if the \c lib handle was not correctly
 * initialised or if the \c frameQueryInOut is null,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_INTERNAL if the triangulation or marker matcher
 * class are not initialised correctly or image reprocessing is requested,
 * \retval ftkError::FTK_ERR_IMPAIRING_CALIB if the calibration contained in
 * the device prevents triangulation,
 * \retval ftkError::FTK_WAR_FRAME if there are no images provided,
 * \retval ftkError::FTK_ERR_SEG_OVERFLOW if an overflow occurred during image
 * segmentation,
 * \retval ftkError::FTK_ERR_ALGORITHMIC_WALLTIME if the processing time exceed
 * the set walltime for the current frame,
 * \retval ftkError::FTK_WAR_TEMP_LOW if the current temperature is too low for
 * compensation,
 * \retval ftkError::FTK_WAR_TEMP_HIGH if the current temperature of the device
 * is too high for compensation.
 */
ATR_EXPORT ftkError ftkReprocessFrame( ftkLibrary lib, uint64 sn, ftkFrameQuery* frameQueryInOut );

/** \brief Function reading information on frame.
 *
 * This function is able to read specific information from the frame. Currently
 * the only available information is ftkInformationType::SynchronisedStrobe.
 *
 * \deprecated Please use #ftkExtractFrameInfo instead.
 *
 * \param[in] frame pointer on the frame to read.
 * \param[in] what type of information to read.
 * \param[out] leftValue pointer on the memory where the value will be written
 * for the left camera.
 * \param[out] rightValue pointer on the memory where the value will be written
 * for the right camera.
 *
 * \retval ftkError::FTK_OK if the value could be successfully read,
 * \retval ftkError::FTK_ERR_INV_PTR if \c lib, \c frame, \c leftValue or
 * \c rightValue is \c nullptr,
 * \retval ftkError::FTK_ERR_INV_OPT if the wanted information is not
 * supported,
 * \retval ftkError::FTK_ERR_INTERNAL if internal data could not be casted.
 */
ATR_EXPORT
#ifdef ATR_MSVC
__declspec( deprecated( "Please use ftkExtractFrameInfo instead" ) )
#endif
  ftkError ftkGetFrameInfo( const ftkFrameQuery* frame,
                            const ftkInformationType what,
                            int32* leftValue,
                            int32* rightValue )
#if defined( ATR_GCC ) || defined( ATR_CLANG )
    __attribute__( ( deprecated( "Please use ftkExtractFrameInfo instead" ) ) )
#endif
    ;

/** \brief Function reading information on frame.
 *
 * This function is able to read specific information from the frame. Currently
 * the only available information is ftkInformationType::SynchronisedStrobe and
 * ftkInformationType::CalibrationParameters.
 *
 * \param[in] frame pointer on the frame to read.
 * \param[in,out] userData structure specificying which info should be retrieved
 * and providing memory to  store it.
 *
 * \retval ftkError::FTK_OK if the value could be successfully read,
 * \retval ftkError::FTK_ERR_INV_PTR if \c lib, \c frame, \c userData or
 * is \c nullptr,
 * \retval ftkError::FTK_ERR_INV_OPT if the wanted information is not
 * supported,
 * \retval ftkError::FTK_ERR_INTERNAL if internal data could not be casted.
 */
ATR_EXPORT ftkError ftkExtractFrameInfo( const ftkFrameQuery* frame, ftkFrameInfoData* userData );

/** \brief Function dumping frame.
 *
 * This function allows to dump the current frame in XML format. It is meant
 * for debugging and diagnostics purpose.
 *
 * \warning This function is currently not implemented, i.e. it can be called
 * but no file will be produced.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 *
 * \retval ftkError::FTK_OK if the frame could be dumped successfully,
 * \retval ftkError::FTK_ERR_INV_PTR if the \c lib handle was not correctly
 * initialised,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_WRITE if the file could not be written,
 * \retval ftkError::FTK_WAR_NOT_SUPPORTED as long as the feature is not
 * implemented.
 */
ATR_EXPORT ftkError ftkDumpFrame( ftkLibrary lib, uint64 sn );

/** \} */

// ----------------------------------------------------------------

#if defined( ATR_FTK ) || defined( ATR_STK )
#ifdef GLOBAL_DOXYGEN

/** \addtogroup data
 * \{
 */
#else

/** \defgroup data Data Functions
 * \brief Functions read data from the device sensors.
 * \{
 */
#endif

/** \brief Getter for the accelerometer data.
 *
 * This function allows to access the current accelerometer data. The
 * acceleration are returned as a 3D vector in standard units.
 *
 * \warning This function takes some time to be completed, it should \e not be called from the acquisition
 * loop.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 * \param[in] index index of the accelerometer to read.
 * \param[out] value pointer on the output acceleration read by the first accelerometer in
 * \f$m \, s^{-2}\f$ units.
 *
 *
 * \retval ftkError::FTK_OK if the data could be retrieved successfully,
 * \retval ftkError::FTK_WAR_NOT_SUPPORTED if the fusionTrack does not support that feature,
 * \retval ftkError::FTK_WAR_DISABLED is the periodic sending of the accelerometer data has been disabled,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly initialised, or the \c value
 * pointer is \c nullptr,
 * \retval ftkError::FTK_ERR_INV_INDEX if the given index is invalid,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_INV_OPT_VAL if the accelerometer status indicates the reading is invalid,
 * \retval ftkError::FTK_ERR_READ if the wanted register cannot be read.
 */
ATR_EXPORT ftkError ftkGetAccelerometerData( ftkLibrary lib, uint64 sn, uint32 index, ftk3DPoint* value );

/** \brief Getter for the real time clock timestamp.
 *
 * This function allows to get the current timestamp of the fusionTrack, given
 * by the on board real time clock module. The value is the linux timestamp:
 * the number of seconds since January 1st 1970 at 00:00:00.
 *
 * \warning This function takes some time to be completed, it should \e not be called from the acquisition
 * loop.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 * \param[out] timestamp pointer on the written timestamp.
 *
 * \retval ftkError::FTK_OK if the data could be retrieved successfully.
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly
 * initialised, or the \c firstValue pointer is null,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_INV_OPT_PAR if the wanted register cannot be read,
 * \retval ftkError::FTK_WAR_NOT_SUPPORTED if the RTC component cannot be
 * accessed (occurs for firmware versions prior to 1.1.6.6B).
 */
ATR_EXPORT ftkError ftkGetRealTimeClock( ftkLibrary lib, uint64 sn, uint64* timestamp );

/** \}
 */
#endif

// ----------------------------------------------------------------
#ifdef GLOBAL_DOXYGEN

/** \addtogroup geometries
 * \{
 */
#else

/** \defgroup geometries Geometries Functions
 * \brief Functions to set, clear or enumerate marker geometries
 * \{
 *
 * Atracsys introduced in SDK 4.7.1 a new geometry structure, ftkRigidBody, which is meant to replace the
 * ftkGeometry one. It allows to store more information than its precedessor, the extra information will be
 * used in future developments of the SDK. The new structure required two new file formats, to allow to store
 * the additional information on the PC and on wireless tools (which have limited memory). There are now
 * three different supported geometry files:
 *   - legacy INI files, aka INI version 0;
 *   - INI files version 1;
 *   - binary files version 1.
 *
 * The version 1 INI file can be loaded as a version 0 INI file, the additional information will simply be
 * discarded/neglected. The binary version 1 file is completely equivalent to the version 1 INI file. Please
 * read section `Marker geometry files' of the user manual to get more information about the different file
 * specifications.
 *
 * In order to ease the loading of ftkRigidBody instances from geometry files, the ::ftkLoadRigidBodyFromFile
 * function was created. It automatically recognises the file type and version, and loads the available
 * information. Its counterpart ::ftkSaveRigidBodyToFile allows to save a ftkRigidBody instance in a version 1
 * INI file.
 *
 * The ::ftkGeometryFileConversion allows to convert a binary file into an INI one and vice-versa. It can be
 * for instance used to get a file that can then be uploaded in a wireless tool.
 */
#endif

/** \brief Register a new marker geometry to be detect.
 *
 * This function tells the driver to look for the given geometry in the data.
 *
 * \deprecated Please use the #ftkRegisterRigidBody function.
 *
 * The system will try to match the registered geometry with the raw data. Adding a geometry is immediate.
 * You can remove a geometry with #ftkClearGeometry or enumerate them with #ftkEnumerateGeometries.
 *
 * \if STK
 * When using a spryTrack, the geometry will be sent to the devices.
 * This way, the marker detection will operate whether onboard processing
 * is enable or not.
 * \endif
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 * \param[in] geometryIn a valid geometry to be detected.
 *
 * \code
 * // Initialize library (see ftkInit example)
 * // Get an attached device (see ftkEnumerateDevices example)
 *
 * // Attach a new geometry (id=52) composed of four fiducials
 * // Note that you can define the geometry in any referential.
 * // It will only change the pose of the marker
 *
 * ftkGeometry markerGeometry;
 * markerGeometry.geometryId = 52;
 * markerGeometry.pointsCount = 4;
 * markerGeometry.positions[0].x = 0.000000f;
 * markerGeometry.positions[0].y = 0.000000f;
 * markerGeometry.positions[0].z = 0.000000f;
 *
 * markerGeometry.positions[1].x = 78.736931f;
 * markerGeometry.positions[1].y = 0.000000f;
 * markerGeometry.positions[1].z = 0.000000f;
 *
 * markerGeometry.positions[2].x = 21.860918f;
 * markerGeometry.positions[2].y = 47.757847f;
 * markerGeometry.positions[2].z = 0.000000f;
 *
 * markerGeometry.positions[3].x = 111.273277f;
 * markerGeometry.positions[3].y = 51.558617f;
 * markerGeometry.positions[3].z = -2.107444f;
 *
 * if ( ftkSetGeometry( lib, sn, &markerGeometry ) != ftkError::FTK_OK )
 * {
 *     ERROR( "Cannot set geometry" );
 * }
 * \endcode
 *
 * \see ftkInit
 * \see ftkEnumerateDevices
 * \see ftkClearGeometry
 * \see ftkEnumerateGeometries
 *
 * \retval ftkError::FTK_OK if the geometry could be set correctly,
 * \retval ftkError::FTK_ERR_INV_PTR if the \c lib handle was not correctly initialised or if the
 * \c geometryIn pointer is null,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_GEOM_PTS if the number of points in the geometry is strictly lower than 3 or
 * larger than the maximum number of fiducials in a marker.
 */
ATR_EXPORT
#ifdef ATR_MSVC
__declspec( deprecated( "Please use the ftkRegisterRigidBody function" ) )
#endif
  ftkError ftkSetGeometry( ftkLibrary lib, uint64 sn, ftkGeometry* geometryIn )
#if defined( ATR_GCC ) || defined( ATR_CLANG )
    __attribute__( ( deprecated( "Please use the ftkRegisterRigidBody function" ) ) )
#endif
    ;

/** \brief Register a new marker geometry to be detect.
 *
 * This function tells the driver to look for the given geometry in the data.
 *
 * The system will try to match the registered geometry with the raw data. Adding a geometry is immediate.
 * You can remove a geometry with #ftkClearGeometry or enumerate them with #ftkEnumerateRigidBodies.
 *
 * \deprecated Please use the #ftkRegisterRigidBody function.
 *
 * \if STK
 * When using a spryTrack, the geometry will be sent to the devices.
 * This way, the marker detection will operate whether onboard processing
 * is enable or not.
 * \endif
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 * \param[in] geometryIn a valid geometry to be detected.
 *
 * \code
 * // Initialize library (see ftkInit example)
 * // Get an attached device (see ftkEnumerateDevices example)
 *
 * // Attach a new geometry (id=4) from geometry074.ini
 *
 * ftkRigidBody markerGeometry;
 *
 * std::ifstream file( "geometry074.ini", std::ios:ate | std::ios::binary );
 *
 * if ( ! file.is_open() )
 * {
 *     // ...
 * }
 *
 * ftkBuffer fileContent;
 * fileContent.reset();
 * fileContent.size = static_cast< uint32_t >( file.tellg() );
 * file.seekg( 0u, std::ios::beg );
 * file.read( fileContent.data, fileContent.size );
 * file.close();
 *
 * if ( ftkLoadRigidBodyFromFile( lib, &fileContent, &markerGeometry ) != ftkError::FTK_OK )
 * {
 *     // ...
 * }
 *
 * if ( ftkSetRigidBody( lib, sn, &markerGeometry ) != ftkError::FTK_OK )
 * {
 *     ERROR( "Cannot set geometry" );
 * }
 * \endcode
 *
 * \see ftkInit
 * \see ftkEnumerateDevices
 * \see ftkClearRigidBody
 * \see ftkEnumerateRigidBodies
 *
 * \retval ftkError::FTK_OK if the geometry could be set correctly,
 * \retval ftkError::FTK_ERR_INV_PTR if the \c lib handle was not correctly initialised or if the
 * \c geometryIn pointer is null,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_GEOM_PTS if the number of points in the geometry is strictly lower than 3 or
 * larger than the maximum number of fiducials in a marker.
 */
ATR_EXPORT
#ifdef ATR_MSVC
__declspec( deprecated( "Please use the ftkRegisterRigidBody function" ) )
#endif
  ftkError ftkSetRigidBody( ftkLibrary lib, uint64 sn, ftkRigidBody* geometryIn )
#if defined( ATR_GCC ) || defined( ATR_CLANG )
    __attribute__( ( deprecated( "Please use the ftkRegisterRigidBody function" ) ) )
#endif
    ;

/** \brief Function registering a geometry from a file name or file contents.
 *
 * This function was introduced in SDK 4.10.1 in order to allow to load the new JSON geometry files, which
 * can be more than 10,240 bytes contained in a #ftkBuffer. Indeed, the handling of 4π markers implied larger
 * geometry files. In order not to bloat the structure describing the geometry, an internal structure is now
 * used, not provided to the user. This is why this function was developped.
 *
 * The function accepts either a file name or the file contents as input argument. If the input string
 * corresponds to an existing file name, this file is opened and its content loaded. Otherwise, the string
 * is assumed to be a geometry file contents and processed as such.
 *
 * \warning Please note that markers reconstructed from geometries loaded by this function will end up in the
 * ftkFrameQuery::sixtyFourFiducialsMarkers container, even if the loaded geometry has 6 or less fiducials.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 * \param[in] stringSize size of the \c fileNameOrContents char buffer.
 * \param[in] fileNameOrContents path of the geometry file to be loaded or the contents of the file to be
 * loaded.
 *
 * \retval ftkError::FTK_OK if the geometry loading and registering could be successfully performed,
 * \retval ftkError::FTK_ERR_INV_PTR if \c lib or \c fileNameOrContents is \c nullptr, if \c stringSize is \c
 * 0,
 * \retval ftkError::FTK_ERR_INV_INI_FILE if the file cannot be read, or if the file version cannot be
 * determined, or if the file cannot be parsed (e.g. missing mandatory information, syntax error), or if
 * the geometry structure cannot be extracted from loaded data,
 * \retval ftkError::FTK_ERR_VERSION if the geometry file version or format is not supported,
 * \retval ftkError::FTK_ERR_INTERNAL if the buffer used to read the file cannot be allocated,
 * \retval ftkError::FTK_ERR_GEOM_REGISTERED if a geometry with the same geometry ID is already registered.
 */
ATR_EXPORT ftkError ftkRegisterRigidBody( ftkLibrary lib,
                                          uint64 sn,
                                          const size_t stringSize,
                                          const char* fileNameOrContents );

/** \brief  Clear a registered geometry (giving its geometry id).
 *
 * This function tells the driver to stop looking for the given geometry in the
 * data.
 *
 * \deprecated Please use the #ftkClearRigidBody function.
 *
 * \param[in] lib an initialised library handle
 * \param[in] sn a valid serial number of the device
 * \param[in] geometryId a valid geometry to be unregistered
 *
 * \retval ftkError::FTK_OK if the geometry could be set correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly
 * initialised,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_WAR_GEOM_ID if the wanted geometry is not registered.
 */
ATR_EXPORT ftkError ftkClearGeometry( ftkLibrary lib, uint64 sn, uint32 geometryId );

/** \brief Clear a registered geometry (giving its geometry id).
 *
 * This function tells the driver to stop looking for the given geometry in the
 * data.
 *
 * \param[in] lib an initialised library handle
 * \param[in] sn a valid serial number of the device
 * \param[in] geometryId a valid geometry to be unregistered
 *
 * \retval ftkError::FTK_OK if the geometry could be set correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly
 * initialised,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_WAR_GEOM_ID if the wanted geometry is not registered.
 */
ATR_EXPORT ftkError ftkClearRigidBody( ftkLibrary lib, uint64 sn, uint32 geometryId );

/** \brief Enumerate the registered geometries.
 *
 * This function enumerates all the registered geometries and allows to
 * apply a user-defined function on each of them.
 *
 * \deprecated Please use the #ftkEnumeratedRegisteredRigidBodies function.
 *
 * \see ftkEnumerateDevices for an example of enumeration.
 *
 * \retval ftkError::FTK_OK if the geometries could be enumerated correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly initialised,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_WAR_NOT_EXISTING if the callback is \c nullptr.
 */
ATR_EXPORT ftkError ftkEnumerateGeometries( ftkLibrary lib,
                                            uint64 sn,
                                            ftkGeometryEnumCallback cb,
                                            void* user );

/** \brief Enumerate the registered geometries.
 *
 * This function enumerates all the registered geometries and allows to
 * apply a user-defined function on each of them.
 *
 * \see ftkEnumerateDevices for an example of enumeration.
 *
 * \deprecated Please use the #ftkEnumeratedRegisteredRigidBodies function.
 *
 * \retval ftkError::FTK_OK if the geometries could be enumerated correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly initialised,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_WAR_NOT_EXISTING if the callback is \c nullptr.
 */
ATR_EXPORT ftkError ftkEnumerateRigidBodies( ftkLibrary lib,
                                             uint64 sn,
                                             ftkRigidBodyEnumCallback cb,
                                             void* user );

/** \brief Enumerate the registered geometries.
 *
 * This function enumerates all the registered geometries and allows to apply a user-defined function on
 * each of them.
 *
 * \see ftkEnumerateDevices for an example of enumeration.
 *
 * \retval ftkError::FTK_OK if the geometries could be enumerated correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly initialised,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_WAR_NOT_EXISTING if the callback is \c nullptr.
 */
ATR_EXPORT ftkError ftkEnumeratedRegisteredRigidBodies( ftkLibrary libHandle,
                                                        const uint64_t serialNumber,
                                                        ftkRegisteredRigidBodyEnumCallback cb,
                                                        void* userData );

/** \brief Function allowing to get the properties of a registered rigid body.
 *
 * This function allows to get any property of a \e registered body. As soon as the provided library
 * handle, device serial number, geometry ID and ftkRigidBodyInformation pointer are valie, the file version,
 * geometry ID, number of fiducials and divots are filled. The information that can be provided is:
 *  - ftkRigidBodyInfoGetter::GeometryType will store in ftkRigidBodyInformation::IntegerValue a \c 0 if the
 *    geometry may lead to ftkMarker instances, or a \c 1 if the geometry may lead to ftkMarker64Fiducials
 *    instances.
 *  - ftkRigidBodyInfoGetter::IsFourPi will store \c true in ftkRigidBodyInformation::BooleanValue if the
 *    rigid body describes a 4π marker.
 *  - ftkRigidBodyInfoGetter::AllowedMissingPoint will store the geometry-specific allowed number of missing
 *    fiducials in the registration computation in ftkRigidBodyInformation::IntegerValue;
 *  - ftkRigidBodyInfoGetter::FiducialPosition will store the position of the fiducial specified by \c
 *    elementIndex in ftkRigidBodyInformation::FloatTriplet;
 *  - ftkRigidBodyInfoGetter::FiducialNormal will store the normal vector of the fiducial specified by \c
 *    elementIndex in ftkRigidBodyInformation::FloatTriplet;
 *  - ftkRigidBodyInfoGetter::FiducialType will store the type of the fiducial specified by \c elementIndex
 *    in ftkRigidBodyInformation::TypeOfFiducial
 *  - ftkRigidBodyInfoGetter::DivotPosition will store the position of the divot specified by \c elementIndex
 *    in ftkRigidBodyInformation::FloatTriplet;
 *  - ftkRigidBodyInfoGetter::AngularThresholds will store \e global (i.e. geometry-wide) angle above which
 *    fiducials will be excluded from the 4π marker registration in ftkRigidBodyInformation::FloatTriplet[0u],
 *    additionally, if \c elementIndex is a valid index, the fiducial-specific registration angle will be
 *    stored in ftkRigidBodyInformation::FloatTriplet[1u] and the angle above which the fiducial will be
 *    considered as unseen in ftkRigidBodyInformation::FloatTriplet[2u].
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 * \param[in] geometryId a valid geometry ID to be inspected.
 * \param[in] elementIndex index of the element (fiducial, divot) to be retrieved. Please note that this will
 * be ignored for properties not related to a specific fiducial. In that case, the input value is just
 * ignored.
 * \param[in] what kind of information to be retrieved from the geometry.
 * \param[out] information pointer on a valid instance, which will be used to store the wanted information.
 *
 * \retval ftkError::FTK_OK if the information could be read correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle or \c information is \c nullptr,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::ftkError::FTK_ERR_UNKNOWN_GEOM is the specified geometry was not registered;
 * \retval ftkError::FTK_ERR_GEOM_INV_INDEX if the specified index does not correspond to an existing
 * fiducial or divot (e.g. \c elementIndex \f$ \le \f$ ftkRigidBodyInformation::NumberOfFiducials when
 * getting the fiducial position).
 */
ATR_EXPORT ftkError ftkGetRigidBodyProperty( ftkLibrary lib,
                                             const uint64 sn,
                                             const uint32 geometryId,
                                             const uint32 elementIndex,
                                             const ftkRigidBodyInfoGetter what,
                                             ftkRigidBodyInformation* information );

/** \} */

// ----------------------------------------------------------------

#ifdef GLOBAL_DOXYGEN

/** \addtogroup options
 * \{
 */
#else

/** \defgroup options Options Functions
 * \brief Functions to get, set or enumerate options
 *
 * Options enable getting or setting configuration information from/to the
 * driver module.
 *
 * Options may be global or specific to a type of device.
 *
 * Most options can be tested and are accessible in the demo program.
 *
 * See the different options structures and enumerators for more detailed
 * information.
 *
 * \see ftkComponent
 * \see ftkOptionType
 * \see ftkOptionGetter
 * \see ftkOptionStatus
 * \see ftkOptionsInfo
 *
 * \{
 */
#endif

/** \brief Enumerate available options.
 *
 * This function enumerates all the available options and allows to apply a
 * user-defined function on each of them.
 *
 * \param[in] lib an initialised library handle
 * \param[in] sn a valid serial number of the device (compulsory for device
 * specific options, sn=0LL for general purpose options)
 * \param[in] cb the option enumeration callback
 * \param[in] user parameter of the callback
 *
 * Example of code to display available option and their respective ids.
 *
 * \code
 * void optionEnumerator( uint64 sn, void* user, ftkOptionsInfo* oi )
 * {
 *     cout << "Option (" << oi->id << ") " oi->name << endl;
 * }
 *
 * main()
 * {
 *     // Initialize library (see ftkInit() example)
 *     // Get an attached device (see ftkEnumerateDevices() example)
 *
 *     if ( ftkEnumerateOptions( lib, sn, optionEnumerator, 0 ) != ftkError::FTK_OK )
 *     {
 *         ERROR( "Cannot enumerate options" );
 *     }
 * }
 * \endcode
 *
 * \see ftkInit
 * \see ftkEnumerateDevices
 * \see ftkOptionsInfo
 *
 * \retval ftkError::FTK_OK if the enumeration could be done correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly
 * initialised,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved.
 */
ATR_EXPORT ftkError ftkEnumerateOptions( ftkLibrary lib, uint64 sn, ftkOptionsEnumCallback cb, void* user );

/** \brief Get an integer option.
 *
 * This function allows to get an integer option value. Different values can be
 * retrieved:
 *   - the minimum value for the option;
 *   - the maximum value for the option;
 *   - the default value of the option;
 *   - the current value of the option.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device (compulsory for device
 * specific options, \c 0uLL for general purpose options).
 * \param[in] optID id of the option (can be retrieved with
 * ftkEnumerateOptions).
 * \param[out] out output value.
 * \param[in] what define what to retrieve minimum, maximum, default or actual
 * value.
 *
 * \see ftkSetInt32
 *
 * \retval ftkError::FTK_OK if the value retrieval could be done correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly
 * initialised or if \c out is null,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_INV_OPT_PAR if the option does not exist, or is
 * not of type \c int32,
 * \retval ftkError::FTK_ERR_READ if the option cannot be read from the device.
 */
ATR_EXPORT ftkError ftkGetInt32( ftkLibrary lib, uint64 sn, uint32 optID, int32* out, ftkOptionGetter what );

/** \brief Set an integer option.
 *
 * This function allows to set an integer option.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device (compulsory for device
 * specific options, \c 0uLL for general purpose options).
 * \param[in] optID id of the option (can be retrieved with
 * ftkEnumerateOptions).
 * \param[in] val value to set.
 *
 * \see ftkGetInt32
 *
 * \retval ftkError::FTK_OK if the value setting could be done correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly
 * initialised or if \c out is null,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_INV_OPT_VAL if the value is invalid,
 * \retval ftkError::FTK_ERR_INV_OPT if the option does not exist, or is not of
 * type \c int32,
 * \retval ftkError::FTK_ERR_READ if the option cannot be written to the
 * device,
 * \retval ftkError::FTK_WAR_OPT_VAL_RANGE if the set value is outside the
 * allowed range and has therefore been cropped,
 * \retval ftkError::FTK_WAR_OPT_NO_OP if the set option has currently no
 * effect.
 */
ATR_EXPORT ftkError ftkSetInt32( ftkLibrary lib, uint64 sn, uint32 optID, int32 val );

/** \brief Get a float option.
 *
 * This function allows to get a floating-point option value. Different values
 * can be retrieved:
 *   - the minimum value for the option;
 *   - the maximum value for the option;
 *   - the default value of the option;
 *   - the current value of the option.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device (compulsory for device
 * specific options, \c 0uLL for general purpose options).
 * \param[in] optID id of the option (can be retrieved with
 * ftkEnumerateOptions).
 * \param[out] out output value.
 * \param[in] what define what to retrieve minimum, maximum, default or actual
 * value.
 *
 * \see ftkSetFloat32
 *
 * \retval ftkError::FTK_OK if the value retrieval could be done correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly
 * initialised or if \c out is null,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_INV_OPT_PAR if the option does not exist, or is
 * not of type \c float32,
 * \retval ftkError::FTK_ERR_READ if the option cannot be read from the device
 */
ATR_EXPORT ftkError
  ftkGetFloat32( ftkLibrary lib, uint64 sn, uint32 optID, float32* out, ftkOptionGetter what );

/** \brief Set a float option.
 *
 * This function allows to set a floating-point option.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device (compulsory for device
 * specific options, \c 0uLL for general purpose options).
 * \param[in] optID id of the option (can be retrieved with
 * ftkEnumerateOptions).
 * \param[in] val value to set.
 *
 * \see ftkGetFloat32
 *
 * \retval ftkError::FTK_OK if the value setting could be done correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly
 * initialised or if \c out is nullptr,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_INV_OPT_VAL if the value is invalid,
 * \retval ftkError::FTK_ERR_INV_OPT if the option does not exist, or is not of
 * type \c float32,
 * \retval ftkError::FTK_ERR_READ if the option cannot be written to the
 * device,
 * \retval ftkError::FTK_WAR_OPT_VAL_RANGE if the set value is outside the
 * allowed range and has therefore been cropped,
 * \retval ftkError::FTK_WAR_OPT_NO_OP if the set option has currently no
 * effect.
 */
ATR_EXPORT ftkError ftkSetFloat32( ftkLibrary lib, uint64 sn, uint32 optID, float32 val );

/** \brief Get a binary option.
 *
 * This function allows to get the value of a binary option. Note that only the
 * current value can be retrieved, there are no min/max or default values for
 * binary options.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device (compulsory for device
 * specific options, \c 0uLL for general purpose options).
 * \param[in] optID id of the option (can be retrieved with
 * ftkEnumerateOptions).
 * \param[out] bufferOut pointer on the output buffer, contains the data and
 * the actual data size.
 *
 * \see ftkSetData
 *
 * \retval ftkError::FTK_OK if the value retrieval could be done correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly
 * initialised or if \c dataOut or \c dataSizeInBytes is \c nullptr,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_INTERNAL if the needed file cannot be read,
 * \retval ftkError::FTK_INV_OPT_PAR if the wanted register cannot be read or
 * does not exist (invalid address).
 * \retval ftkError::FTK_ERR_READ if the option cannot be read from the device.
 */
ATR_EXPORT ftkError ftkGetData( ftkLibrary lib, uint64 sn, uint32 optID, ftkBuffer* bufferOut );

/** \brief Set a binary option.
 *
 * This function allows to set an binary option.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device (compulsory for device
 * specific options, \c 0uLL for general purpose options).
 * \param[in] optID id of the option (can be retrieved with
 * ftkEnumerateOptions).
 * \param[in] bufferIn  pointer on the input buffer, contains the data and the
 * actual data size.
 *
 * \see ftkGetData
 *
 * \retval ftkError::FTK_OK if the value retrieval could be done correctly,
 * \retval ftkError::FTK_ERR_INV_PTR the \c lib handle was not correctly
 * initialised or if \c dataOut or \c dataSizeInBytes is null,
 * \retval ftkError::FTK_ERR_INV_SN if the device could not be retrieved,
 * \retval ftkError::FTK_ERR_INTERNAL if a memory allocation error occurred or
 * if the wanted file cannot be written,
 * \retval ftkError::FTK_INV_OPT_PAR if the wanted register cannot be read or
 * does not exits (invalid address),
 * \retval ftkError::FTK_ERR_INV_OPT_VAL if the value cannot be set,
 * \retval ftkError::FTK_ERR_READ if the option cannot be written to the
 * device,
 * \retval ftkError::FTK_ERR_WRITE if the environment cannot be saved because
 * the file cannot be opened,
 * \retval ftkError::FTK_ERR_VERSION when loading an environment, if the SDK or
 * firwmare version used when saving the environment is different from the SDk
 * or firmware version used when loading the environment,
 * \retval ftkError::FTK_WAR_OPT_NO_OP if the set option has currently no
 * effect.
 */
ATR_EXPORT ftkError ftkSetData( ftkLibrary lib, uint64 sn, uint32 optID, ftkBuffer* bufferIn );

/** \brief Function combining two 2D points into a 3D point.
 *
 * From This function compute the x-y-z coordinates of 2 known 2D points.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 * \param[in] leftPixel a ftk3DPoint from the left camera with z equals to 0.
 * \param[in] rightPixel a ftk3DPoint from the right camera with z equals to 0.
 * \param[out] outPoint pointer on the triangulated point, with x-y-z
 * coordinates.
 *
 * \retval ftkError::FTK_OK if the value retrieval could be done correctly,
 * \retval ftkError::FTK_ERR_INTERNAL if this function can not triangulate.
 */
ATR_EXPORT ftkError ftkTriangulate( ftkLibrary lib,
                                    uint64 sn,
                                    const ftk3DPoint* leftPixel,
                                    const ftk3DPoint* rightPixel,
                                    ftk3DPoint* outPoint );

/** \brief Function projecting a 3D point to two 2D points.
 *
 * This function projects one 3D point to the two camera planes.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] sn a valid serial number of the device.
 * \param[in] inPoint pointer on 3D point to project.
 * \param[out] outLeftData pointer on the left projected point, with
 * \f$ \left( u, v, 0 \right) \f$ coordinates.
 * \param[out] outRightData pointer on the right projected point, with
 * \f$ \left( u, v, 0 \right) \f$ coordinates.
 *
 * \retval ftkError::FTK_OK if the value retrieval could be done correctly,
 * \retval ftkError::FTK_ERR_INTERNAL if this function can not project on the
 * left or right cam.
 */
ATR_EXPORT ftkError ftkReprojectPoint(
  ftkLibrary lib, uint64 sn, const ftk3DPoint* inPoint, ftk3DPoint* outLeftData, ftk3DPoint* outRightData );

#ifdef GLOBAL_DOXYGEN
/**
 * \}
 *
 * \addtogroup geometries
 * \{
 */
#endif

/** \brief Function reading a rigid body from a file content.
 *
 * This function reads a ftkRigidBody instance from a buffer in which the
 * content of the file to read has been loaded. The function supports legacy
 * INI files, i.e. geometry INI files shipped together with previous SDKs, new
 * INI and binary files. The function first determines whether the file format
 * belongs to the list of supported formats. If yes, it tries to read and to
 * load the information into the ftkRigidBody instance.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] fileContent content of the file to be loaded.
 * \param[out] geom pointer on a valid ftkRigidBody instance, will be written
 * by the function.
 *
 * \retval ftkError::FTK_OK if the geometry could be successfully loaded,
 * \retval ftkError::FTK_ERR_INV_PTR if \c lib, \c fileContent or \c geom is
 * \c nullptr,
 * \retval ftkError::FTK_ERR_INV_INI_FILE if the file content is empty, has
 * the wrong syntax, or is missing information,
 * \retval ftkError::FTK_ERR_VERSION if the file does not belong to the
 * supported file list.
 */
ATR_EXPORT ftkError ftkLoadRigidBodyFromFile( ftkLibrary lib,
                                              const ftkBuffer* fileContent,
                                              ftkRigidBody* geom );

/** \brief Function writing a rigid body into a file.
 *
 * This function writes the parameters of a ftkRigidBody instance into a
 * buffer, which can then be written in a file for future usage. The function
 * writes binary file version 1, as it is the most vesatile format (which can
 * be used for both active and passive markers).
 *
 * \param[in] lib an initialised library handle.
 * \param[in] geom pointer on a valid ftkRigidBody instance to be saved.
 * \param[out] fileContent content of the file to be saved.
 *
 * \retval ftkError::FTK_OK if the geometry could be successfully saved,
 * \retval ftkError::FTK_ERR_INV_PTR if \c lib, \c fileContent or \c geom is
 * \c nullptr,
 * \retval ftkError::FTK_ERR_VERSION if the file does not belong to the
 * supported file list.
 */
ATR_EXPORT ftkError ftkSaveRigidBodyToFile( ftkLibrary lib,
                                            const ftkRigidBody* geom,
                                            ftkBuffer* fileContent );

/** \brief Function converting a file from INI to binary (and vice-versa).
 *
 * This function allows to convert from an INI file to a binary file, or the
 * other way around.
 *
 * \param[in] lib an initialised library handle.
 * \param[in] inFileContent pointer on an initialised ftkBuffer, containing the
 * input file content.
 * \param[out] outFileContent pointer on an initialised ftkBuffer, in which
 * the output file content will be written by the function.
 *
 * \retval ftkError::FTK_OK if the file could be successfully converted,
 * \retval ftkError::FTK_ERR_INV_PTR if \c lib, \c inFileContent or
 * \c outFileContent is \c nullptr,
 * \retval ftkError::FTK_ERR_INV_INI_FILE if the input file content does not
 * contain any valid geometry,
 * \retval ftkError::FTK_ERR_VERSION if the desired output is not a valid one,
 * e.g. a binary file for version \c 0u or if the input format is not
 * supported.
 */
ATR_EXPORT ftkError ftkGeometryFileConversion( ftkLibrary lib,
                                               const ftkBuffer* inFileContent,
                                               ftkBuffer* outFileContent );

#ifdef GLOBAL_DOXYGEN

/**
 * \}
 */
#endif

/** \} */

#endif
