/** \file EvtFansV1Data.hpp
 * \brief File defining all the fans-related data structures (version 1) of the C++ API.
 *
 * This file is a collection of all fans-related data structures used in the C++ API.
 */
#pragma once

#include <ftkEvent.h>
#include <ftkTypes.h>

#include <vector>

/**
 * \addtogroup adApi
 * \{
 */

namespace atracsys
{
    class FrameData;

    /** \brief Class wrapping the ::ftkFanState structure.
     *
     * This class provides the interface to read the fan state (defined by the set point and the measured
     * speed), sent in FtkEventType::fetFansV1 event.
     */
    class FanStateData : protected ftkFanState
    {
    public:
        /** \brief Default implementation of the default constructor.
         */
        FanStateData() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        FanStateData( const FanStateData& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        FanStateData( FanStateData&& other ) = default;

        /** \brief Constructor promoting a ::ftkFanState instance.
         *
         * This constructor allows to promote a ::ftkFanState instance.
         *
         * \param[in] other instance to promote.
         */
        FanStateData( const ftkFanState& other );

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~FanStateData() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        FanStateData& operator=( const FanStateData& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        FanStateData& operator=( FanStateData&& other ) = default;

        /** \brief Comparison operator.
         *
         * This method compares the set point and speed
         *
         * \param[in] other instance to compare.
         *
         * \retval true if the speed and set point match,
         * \retval false if not.
         */
        bool operator==( const FanStateData& other ) const;

        /** \brief Getter for the fan set point.
         *
         * \retval ftkFanState::PwmDuty as value.
         */
        uint8 pwmDuty() const;

        /** \brief Getter for the fan speed in rpm.
         *
         * \retval ftkFanState::Speed as value.
         */
        uint16 speed() const;
    };

    /** \brief Class providing access to the fan state event.
     *
     * This class contains all the information about the accelerometer data sent in the
     * FtkEventType::fetFansV1 event. The instance does know if it has been properly set (i.e. whether its
     * member values make sense).
     */
    class EvtFansV1Data : public EvtFansV1Payload
    {
    public:
        /** \brief Default implementation of default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        EvtFansV1Data() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        EvtFansV1Data( const EvtFansV1Data& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        EvtFansV1Data( EvtFansV1Data&& other ) = default;

        /** \brief Constructor promoting a ::EvtFansV1Payload instance.
         *
         * \param[in] other instance to promote.
         */
        EvtFansV1Data( const EvtFansV1Payload& other );

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~EvtFansV1Data() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        EvtFansV1Data& operator=( const EvtFansV1Data& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        EvtFansV1Data& operator=( EvtFansV1Data&& other ) = default;

        /** \brief Getter for EvtFansV1Payload::FansStatus.
         *
         * \retval EvtFansV1Payload::FansStatus as value.
         */
        ftkFanStatus fansStatus() const;

        /** \brief Getter for #_Fans.
         *
         * \retval _Fans as a const reference.
         */
        const std::vector< FanStateData >& fans() const;

        /** \brief Getter for #_Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Getter for #_InvalidInstance.
         *
         * \retval _InvalidInstance as a const reference.
         */
        static const EvtFansV1Data& invalidInstance();

    protected:
        /** \brief Invalid instance.
         *
         * This instance is used as return value when the EvtAccelerometerV1Data to be accessed is not
         * defined, either if the index is too big or there are no stored EvtAccelerometerV1Data instances.
         */
        static const EvtFansV1Data _InvalidInstance;

        /** \brief Container for the different fans data.
         */
        std::vector< FanStateData > _Fans{};

        /** Contains \c true if the instance contains valid information.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys
