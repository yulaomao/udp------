// ===========================================================================

/*!
 *   This file is part of the ATRACSYS fusiontrack library.
 *   Copyright (C) 2018-2018 by Atracsys LLC. All rights reserved.
 *
 *  THIS FILE CANNOT BE SHARED, MODIFIED OR REDISTRIBUTED WITHOUT THE
 *  WRITTEN PERMISSION OF ATRACSYS.
 *
 *  \file     ftkEvent.h
 *  \brief    Definition of the class handling fTk events.
 *
 */
// ===========================================================================
#ifndef FTKEVENT_H
#define FTKEVENT_H

#ifdef GLOBAL_DOXYGEN

/**
 * \addtogroup sdk
 * \{
 */
#else

/** \defgroup eventSDK Event Functions
 * \brief Function to manage events
 * \{
 */
#endif

#include "ftkErrors.h"
#include "ftkPlatform.h"

/** \cond PEDANTIC
 */
#ifdef __cplusplus
struct ftkLibraryImp;
#else
typedef struct ftkLibraryImp ftkLibraryImp;
#endif
typedef ftkLibraryImp* ftkLibrary;

/** \endcond
 */

/** \brief Definitions of the event types.
 *
 * The event type indicates both the type \e and the version. For instance,
 * sending the 8 temperatures is type 0, in the future, if only 6 of them are
 * sent, this could be \c detTempV4 \c = \c 42.
 */
TYPED_ENUM( uint32, FtkEventType )
// clang-format off
{
    /** \brief Temperature sending, version 1.
     *
     * \deprecated This event is not anymore forwarded to the user.
     * \see EvtTemperatureV1Payload.
     */
    fetTempV1 = 0,

    /** \brief Event indicating the current device temperature is too low.
     */
    fetLowTemp = 1,

    /** \brief Event indicating the current device temperature is too high.
     */
    fetHighTemp = 2,

    /** \brief Event indicating the device has received at least one shock above the threshold, leading to a
     * possible decalibration.
     */
    fetShockDetected = 3,

    /** \brief Event indicating the watchdog timer deactivated the data sending.
     *
     * \deprecated This event is not anymore forwarded to the user.
     */
    fetWatchdogTimeout = 4,

    /** \brief Temperature sending, version 2.
     *
     * \deprecated This event is not anymore forwarded to the user.
     *
     * The payload contains the 8 \c float32 values for the following temperatures:
     *   -# camera 0, sensor 0;
     *   -# camera 0, sensor 1;
     *   -# camera 1, sensor 0;
     *   -# camera 1, sensor 1;
     *   -# IR board 0;
     *   -# IR board 1;
     *   -# main board;
     *   -# power supply board;
     * and then two byte for the fan `frequency':
     *   -# fan 0;
     *   -# fan 1.
     */
    fetTempV2 = 5,

    /** \brief Temperature sending, version 3.
     *
     * \deprecated This event is not anymore forwarded to the user.
     *
     * The payload contains the 8 \c float32 values for the following temperatures:
     *   -# camera 0, sensor 0;
     *   -# camera 0, sensor 1;
     *   -# camera 1, sensor 0;
     *   -# camera 1, sensor 1;
     *   -# IR board 0;
     *   -# IR board 1;
     *   -# main board;
     *   -# power supply board;
     * followed by:
     *   -# one byte indicating the status of the fan readings;
     *   -# 1 byte for the fan 0 input in percent;
     *   -# 2 bytes for the fan 0 speed;
     *   -# 1 byte for the fan 1 input in percent;
     *   -# 2 bytes for the fan 1 speed;
     *   -# 1 reserved byte (unused).
     */
    fetTempV3 = 6,

    /** \brief Event related to presence and state of wireless markers.
     *
     * \deprecated This event is not anymore forwarded to the user.
     *
     * The payload contains 16 mandatory bits, indicating which marker was detected (one bit per marker). For
     * each \e present marker, 16 additional bits are present, currently not used.
     */
    fetWirelessMarkerV1 = 7,

    /** \brief Event related to availability of the calibration for a wireless marker.
     *
     * The payload contains a ftkGeometry instance.
     *
     * \deprecated This event is not anymore forwarded to the user.
     *
     * \see ftkGeometry
     */
    fetWirelessMarkerCalibV1 = 8,

    /** \brief Accelerometer data V1.
     *
     * \deprecated This event is not anymore forwarded to the user.
     */
    fetAccelerometerV1 = 10,

    /** \brief Temperatures values with sensor index.
     */
    fetTempV4 = 11,

    /** \brief Fans data.
     */
    fetFansV1 = 12,

    /** \brief Active markers mask version 1.
     */
    fetActiveMarkersMaskV1 = 13,

    /** \brief Active button state.
     */
    fetActiveMarkersButtonStatusV1 = 14,

    /** \brief Active battery state.
     */
    fetActiveMarkersBatteryStateV1 = 15,

    /** \brief Synthetic temperatures data.
     */
    fetSyntheticTemperaturesV1 = 16,

    /** \brief PTP synchronisation data.
     */
    fetSynchronisationPTPV1 = 17,

    /** \brief EIO tagging event version 1.
     */
    fetEioTaggingV1 = 18,

    /** \brief Trigger information event version 1.
     */
    fetTriggerInfoV1 = 19,

    /** \brief Active marker button read raw value.
     */
    fetActiveMarkersButtonRawValueV1 = 20,

    /** \brief Active marker mask version 2.
     */
    fetActiveMarkersMaskV2 = 21,

    /** \brief Calibration parameters data
     */
    fetCalibrationParameters = 255,

    /** \brief Unused last event type.
     */
    fetLastEvent = 0xFFFFFFFFu
};
// clang-format on

/** \brief Structure holding an event as sent by the tracker.
 *
 * This structure implements the device-oriented data structure of an event.
 *
 * Events are generated and sent by the tracker itself. Some are handled
 * directly by the driver itself, others are forwarded to the user. In order to
 * get those event in a custom made program, the ftkReadEvent() function is
 * used.
 *
 * \cond STK
 * \warning When using a spryTrack, events are only available as part of a
 *          ftkFrameQuery object retrieved through ftkGetLastFrame(). The
 *          ftkReadEvent() function will return nothing.
 * \endcond
 */
PACK1_STRUCT_BEGIN( ftkEvent )
{
    /** \brief Type of the event.
     */
    FtkEventType Type;

    /** \brief Timestamp of the event
     *
     * This is created by the fusionTrack, it is the \f$ \mu{}s \f$ counter
     * value at the generation of the event.
     */
    uint64 Timestamp;

    /** \brief Serial number of the sending device.
     */
    uint64 SerialNumber;

    /** \brief Size of the data contained in the payload.
     */
    uint32 Payload;

    /** \brief Pointer on the payload data.
     */
    uint8* Data;
}
PACK1_STRUCT_END( ftkEvent );

/** \brief Structure holding the fan status.
 *
 * This structure holds the various status bits related to fan control.
 */
PACK1_STRUCT_BEGIN( ftkFanStatus )
{
    /** \brief Contains 1 if the module is enabled.
     */
    uint8 FanModuleEnabled : 1;

    /** \brief Contains 1 if the fan 0 is on.
     */
    uint8 Fan0PWMEnabled : 1;

    /** \brief Contains 1 if the fan 1 is on.
     */
    uint8 Fan1PWMEnabled : 1;

    /** \brief Contains 1 if the speed reading for fan 0 is valid.
     */
    uint8 Fan0SpeedValid : 1;

    /** \brief Contains 1 if the speed reading for fan 1 is valid.
     */
    uint8 Fan1SpeedValid : 1;
#ifdef __cplusplus

    /** \brief Conversion operator.
     *
     * This operator converts a ftkFanStatus to a uint8.
     *
     * \retval the content of the instance, as a uint8.
     */
    operator uint8() const
    {
        union
        {
            uint8 nbr;
            ftkFanStatus status;
        };
        status = *this;
        return nbr;
    }

private:
    /** \brief Reserved unused bits.
     */
    uint8 Reserved : 3;
#endif
}
PACK1_STRUCT_END( ftkFanStatus );

#if !defined( __cplusplus ) || ( defined( ATR_MSVC ) && _MSC_VER < 1900 )
// Using a C compiler or MSCV version older than Visual Studio 2015 (i.e. no full C++11 support)
#define FTK_MEASURE_ACCELEROMETER 3u
#define FTK_NUM_FANS_PER_EVENT 2u
#define FTK_MAX_NUM_TEMP_PER_EVENT 20u
#define FTK_MAX_ACC_PER_EVENT 2u
#define FTK_MAX_ACTIVE_MARKERS 16u
#define MINIMUM_EVENTS_NBR 20u
#else

/** \brief Number of fans in the fusionTrack.
 */
constexpr uint32 FTK_NUM_FANS_PER_EVENT = 2u;

/** \brief Maximum number of temperature measurements in an event.
 */
constexpr uint32 FTK_MAX_NUM_TEMP_PER_EVENT = 20u;

/** \brief Maximum number of paired active markers.
 */
constexpr uint32 FTK_MAX_ACTIVE_MARKERS = 16u;

/** \brief Minimum number of events allocated when ftkCreateFrame is called.
 */
constexpr uint32 MINIMUM_EVENTS_NBR = 20u;
#endif

/** \brief Structure holding the parameter of a fan.
 */
PACK1_STRUCT_BEGIN( ftkFanState )
{
    /** \brief Percentage of the voltage applied to the fan.
     */
    uint8 PwmDuty;

    /** \brief Fan rotation speed, in rpm.
     */
    uint16 Speed;
}
PACK1_STRUCT_END( ftkFanState );

/** \brief Structure holding the payload for events of type
 * FtkEventType::fetFansV1.
 */
PACK1_STRUCT_BEGIN( EvtFansV1Payload )
{
    /** \brief Status of the fans.
     */
    ftkFanStatus FansStatus;

    /** \brief Fan parameters.
     */
    ftkFanState Fans[ FTK_NUM_FANS_PER_EVENT ];
}
PACK1_STRUCT_END( EvtFansV1Payload );

/** \brief Structure holding \e one temperature measurement.
 */
PACK1_STRUCT_BEGIN( EvtTemperatureV4Payload )
{
    /** \brief ID of the sensor.
     */
    uint32 SensorId;

    /** \brief Temperature value in degree Celsius.
     */
    float32 SensorValue;
}
PACK1_STRUCT_END( EvtTemperatureV4Payload );

/** \brief Structure holding the payload for events of type
 * FtkEventType::fetActiveMarkersMaskV1.
 */
PACK1_STRUCT_BEGIN( EvtActiveMarkersMaskV1Payload )
{
    /** \brief Active marker mask.
     *
     * Bit \f$ i \f$ is set to \c 1 if short id \f$ i \f$ is currently paired.
     */
    uint16 ActiveMarkersMask;
}
PACK1_STRUCT_END( EvtActiveMarkersMaskV1Payload );

/** \brief Structure holding the payload for events of type
 * FtkEventType::fetActiveMarkersMaskV2.
 */
PACK1_STRUCT_BEGIN( EvtActiveMarkersMaskV2Payload )
{
    /** \brief Active marker mask.
     *
     * Bit \f$ i \f$ is set to \c 1 if short id \f$ i \f$ is currently paired.
     */
    uint16 ActiveMarkersMask;
    /** \brief Active marker error mask.
     *
     * Bit \f$ i \f$ is set to \c 1 if short id \f$ i \f$ is currently in error state.
     */
    uint16 ActiveMarkersErrorMask;
}
PACK1_STRUCT_END( EvtActiveMarkersMaskV2Payload );

/**
 * \brief Structure describing the payload for a FtkEventType::fetActiveMarkersButtonStatusV1
 * event.
 */
PACK1_STRUCT_BEGIN( EvtActiveMarkersButtonStatusesV1Payload )
{
    uint32 ImageCount;   ///< Imagecounter at the time the button state was retrieved.
    uint8 DeviceID;      ///<  The Active Marker short ID
    uint8 ButtonStatus;  ///<  The state of the button (mask)
}
PACK1_STRUCT_END( EvtActiveMarkersButtonStatusesV1Payload );

/**
 * \brief Structure describing the payload for a FtkEventType::fetActiveMarkersBatteryStateV1
 * event.
 */
PACK1_STRUCT_BEGIN( EvtActiveMarkersBatteryStateV1Payload )
{
    uint32 ImageCount;  ///< Imagecounter at the time the battery state was retrieved.
    uint8 DeviceID;     ///< The Active Marker short ID

    /** \brief The state of the battery.
     *
     *  This number can be converted to Volts using the following formula:
     *
     *  18.5 * 10^-3 * BatteryState.
     *
     *  For all Atracsys Active Markers, the maximum is 3.6 [V].
     *
     *  A Marker will stop functioning when this value reaches 2.2 [V].
     */
    uint8 BatteryState;
}
PACK1_STRUCT_END( EvtActiveMarkersBatteryStateV1Payload );

/** \brief Structure describing the payload for a FtkEventType::fetActiveMarkersButtonRawValueV1 event.
 */
PACK1_STRUCT_BEGIN( EvtActiveMarkerButtonRawValueV1Payload )
{
    uint32 ImageCount;  ///< Imagecounter at the time the battery state was retrieved.
    uint8 DeviceID;     ///< The Active Marker short ID
    /** \brief Raw value for button 0.
     */
    uint32 RawValueButton0;
    /** \brief Raw value for button 1.
     */
    uint32 RawValueButton1;
}
PACK1_STRUCT_END( EvtActiveMarkerButtonRawValueV1Payload );

/** \brief Structure describing the payload for a
 * FtkEventType::fetSyntheticTemperaturesV1 event.
 */
PACK1_STRUCT_BEGIN( EvtSyntheticTemperaturesV1Payload )
{
    /** \brief Current value of the synthetic temperature.
     */
    float CurrentValue;

    /** \brief Value of the synthetic temperature during geometrical
     * calibration (in a 20°C environment).
     */
    float ReferenceValue;
}
PACK1_STRUCT_END( EvtSyntheticTemperaturesV1Payload );

/** \brief This lists the possible returned error by the PTP protocol.
 *
 * \warning The error is actually 6 bits wide.
 */
TYPED_ENUM( uint8, ftkErrorPTP ){
    /** \brief No Error
     */
    Ok = 0u,
    /** \brief No master detected.
     */
    NoMasterDetected = 1u,
    /** \brief Too long time waiting packets in the switch or in the master.
     */
    NetworkTimeoutError = 2u,
    /** \brief Wrong interval between Announce message detected, please check if the setup value of `Log
     * Announce Interval' is correct.
     */
    AnnounceWrongIntervalError = 3u,
    /** \brief Wrong interval between Sync message detected, please check if “Log Sync Interval” is correct.
     */
    SyncWrongInternalError = 4u,
    /** \brief There are too much foreign master (more than five) communicating in the network.
     */
    TooMuchMastersError = 5u,

    /** \brief Sync message timeout (no sync message received since detection of the parent Master).
     */
    SyncMessageTimeout = 20u,
    /** \brief No sync message detected from the parent Master (or in wrong order).
     */
    NoSyncFromMaster = 21u,
    /** \brief Problem encounter with the sync messages (check length and sequence ID).
     */
    SyncMessageError = 22u,
    /** \brief No follow-up message detected but the two-step flag was set.
     */
    NoFollowUp = 23u,
    /** \brief Problem encounter with the follow-up messages (check length and sequence ID).
     */
    FollowUpSyntaxError = 24u,
    /** \brief No delay-resp message detected from the parent Master (or in wrong order).
     */
    NoDelayAnswer = 25u,
    /** \brief Problem encounter with the delay-resp messages (check length and sequence ID).
     */
    DelayAnswerSyntaxError = 26u,
    /** \brief The Precision Time Protocol can be applied but the synchronization appears to be unstable.
     */
    UnstableSynchronisationError = 27u,

    /** \brief Wrong PTP domain (check if the Domain Number has been correctly set).
     */
    WrongDomain = 41u,
    /** \brief The PTP version is not supported (only v2.0 supported).
     */
    UnsupportedVersion = 42u,
    /** \brief Messages flags do not correspond to fTk PTP usage.
     *
     * Please note the alternate master, unicast and profile specific flags should be zero as not supported.
     */
    UnsupportedFlags = 43u,

    /** \brief Unknown error, used if the error ID cannot be transformed in a ftkErrorPTP.
     */
    UnknownError = 62u,
    /** \brief Last error (currently max size for Error ID is 6 bits).
     */
    LastError = 63u
};

/** \brief Structure holding a PTP timestamp.
 *
 * \warning Expected to work only on little-endian architectures.
 */
PACK1_STRUCT_BEGIN( ftkTimestampPTP )
{
    /** \brief Nanoseconds part of the PTP timestamp.
     */
    uint32 NanoSeconds;
    /** \brief Seconds part of the PTP timestamp.
     */
    uint64 Seconds;
}
PACK1_STRUCT_END( ftkTimestampPTP );

/** \brief Structure holding a PTP correction.
 *
 * \warning Expected to work only on little-endian architectures.
 */
PACK1_STRUCT_BEGIN( ftkTimestampCorrectionPTP )
{
    /** \brief Nanoseconds part of the PTP correction.
     */
    int32 NanoSeconds;
    /** \brief Seconds part of the PTP correction.
     */
    int64 Seconds;
}
PACK1_STRUCT_END( ftkTimestampCorrectionPTP );

/** \brief Structure holding the PTP clock master ID.
 *
 * \warning Expected to work only on little-endian architectures.
 */
PACK1_STRUCT_BEGIN( ftkParentId )
{
    /** \brief ID of the used master clock.
     */
    uint64 ClockId;
    /** \brief ID of the master source port.
     */
    uint16 SourcePortId;
}
PACK1_STRUCT_END( ftkParentId );

/** \brief Status of the PTP module.
 */
PACK1_STRUCT_BEGIN( ftkStatusPTP )
{
    /** \brief ID of the fusionTrack PTP FSM state.
     *
     * See ::ftkPortStatePTP.
     */
    uint16 PortStateId : 4;
    /** \brief ID of the detected error which stopped PTP synchronisation.
     *
     * See ::ftkErrorPTP.
     */
    uint16 ErrorId : 6;
#ifdef __cplusplus
private:
#endif
    /** \brief Unused.
     */
    uint16 Reserved : 6;
#ifdef __cplusplus
public:
    /** \brief Operator promoting a \c uint16 value to a ftkStatusPTP instance.
     *
     * The conversion is performed using a union.
     *
     * \param[in] value value to be promoted.
     *
     * \retval *this as value.
     */
    ftkStatusPTP operator=( uint16 value );
    /** \brief Operator returning the corresponding \c uint16 value.
     *
     * The conversion is performed using a union.
     *
     * \return the corresponding \c uint16 value.
     */
    operator uint16() const;
#endif
}
PACK1_STRUCT_END( ftkStatusPTP );

#ifdef __cplusplus
inline ftkStatusPTP ftkStatusPTP::operator=( uint16 value )
{
    union
    {
        ftkStatusPTP bitfield;
        uint16 number;
    };

    number = value;
    *this = bitfield;
    return *this;
}

inline ftkStatusPTP::operator uint16() const
{
    static_assert( sizeof( ftkStatusPTP ) == 2u, "Problem with bitfield" );
    union
    {
        ftkStatusPTP bitfield;
        uint16 number;
    };

    bitfield = *this;
    return number;
}
#endif

/** \brief Enumeration representing the PTP module FSM state.
 */
TYPED_ENUM( uint8, ftkPortStatePTP )
// clang-format off
{
    /** \brief State to initialize the data sets (instance not sync, no PTP communication).
     */
    Initialising = 1u,
    /** \brief State to indicate failure (instance not sync, no PTP communication).
     */
    Faulty = 2u,
    /** \brief State in which the instance listen to Announce messages and apply the BMCA (instance not sync,
     * PTP communication only for reception).
     */
    Listening = 4u,
    /** \brief State used when a new master is detected. Initialize the synchronisation servos and getting
     * synchronised (instance not sync, slave PTP communication active).
     */
    Uncalibrate = 8u,
    /** \brief State in which the instance is synchronised to the selected master port (instance sync, slave
     * PTP communication active)
     */
    Slave = 9u,
    /** \brief Unknown state, used if the convertion could not be performed.
     */
    UnknownState = 15u
};
// clang-format on

/** \brief Structure describing the payload for a FtkEventType::fetSynchronisationPTPV1 event.
 */
PACK1_STRUCT_BEGIN( EvtSynchronisationPTPV1Payload )
{
    /** \brief PTP instance status.
     *
     * This field indicates the PTP instance global status and occurring errors.
     */
    ftkStatusPTP Status;
    /** \brief PTP timestamp associated to a frame, on 10 bytes.
     */
    ftkTimestampPTP Timestamp;
    /** \brief Last calculated Offset Correction.
     */
    ftkTimestampCorrectionPTP LastCorrection;
    /** \brief Identity of the current Parent Master.
     */
    ftkParentId ParentId;
}
PACK1_STRUCT_END( EvtSynchronisationPTPV1Payload );

/** \brief Structure describing a single item of the payload for a FtkEventType::fetEioTaggingV1 event.
 */
PACK1_STRUCT_BEGIN( EvtEioTaggingV1Item )
{
    /** \brief Tag ID of the last received TAG for the current port.
     */
    uint32_t EioTagId : 30;
    /** \brief Tagging mode of the current port.
     *
     * The value is interpreted as follows:
     * - \c 0u means disabled;
     * - \c 1u means single tagging mode;
     * - \c 2u means dual tagging mode.
     */
    uint32_t EioTagMode : 2;
    /** \brief Timestamp (in microseconds) of the last received TAG for the current port.
     */
    uint64_t EioTagTimestamp;
}
PACK1_STRUCT_END( EvtEioTaggingV1Item );

/** \brief Structure describing the payload for a FtkEventType::fetEioTaggingV1 event.
 */
PACK1_STRUCT_BEGIN( EvtEioTaggingV1Payload )
{
    /** \brief The two tagging information items.
     *
     * The first element corresponds to EIO_1 port, the second one to EIO_2.
     */
    EvtEioTaggingV1Item TaggingInfo[ 2u ];
}
PACK1_STRUCT_END( EvtEioTaggingV1Payload );

/** \brief Structure describing the payload for a FtkEventType::fetTriggerInfoV1 event.
 */
PACK1_STRUCT_BEGIN( EvtEioTriggerInfoV1Payload )
{
    /** \brief Give the starting time (delay after the exposure start) of the trigger sending.
     */
    uint64 TriggerStartTime : 23;
#ifdef __cplusplus
private:
#endif
    uint64 ReservedBit : 1;
#ifdef __cplusplus
public:
#endif
    /** \brief Duration of the trigger (high value length).
     */
    uint64 TriggerActualDurationTime : 24;
    /** \brief Give the information if the trigger has been sent with a frame (delayed or not).
     */
    uint64 TriggerEnabledDuringExposure : 1;
#ifdef __cplusplus
private:
#endif
    uint64 ReservedBits : 15;
#ifdef __cplusplus
public:
#endif
    /** \brief Trigger ID for the trigger sent on the EIO_1 port.
     */
    uint32 TriggerIdEio1;
    /** \brief Trigger ID for the trigger sent on the EIO_2 port.
     */
    uint32 TriggerIdEio2;
}
PACK1_STRUCT_END( EvtEioTriggerInfoV1Payload );

/** \brief Function creating a ftkEvent instance.
 *
 * This function allows to create an instance of an Event. If needed, the
 * Event::Data pointer is allocated, but has to be manually set.
 *
 * \param[in] type type of the received (or sent) event.
 * \param[in] timestamp fTk timestamp corresponding to the creation of the
 * event.
 * \param[in] serial serial number of the sending device.
 * \param[in] payload size of the additional data.
 *
 * \return a pointer on the allocated instance, or \c 0 if an error occurred.
 *
 * \critical This function is involved in device event management.
 */
ATR_EXPORT ftkEvent* ftkCreateEvent( FtkEventType type, uint64 timestamp, uint64 serial, uint32 payload );

/** \brief Function deleting a ftkEvent instance.
 *
 * This function allows to free the allocated memory for an Event.
 *
 * \param[in] evt instance to delete.
 *
 * \retval ftkError::FTK_OK if the deletion could be successfully performed,
 * \retval ftkError::FTK_ERR_INV_PTR if \c evt is null,
 * \retval ftkError::FTK_ERR_INIT if \c evt was not created with ftkCreateEvent.
 *
 * \critical This function is involved in device event management.
 */
ATR_EXPORT ftkError ftkDeleteEvent( ftkEvent* evt );

/**
 * \}
 */

#endif  // FTKEVENT_H
