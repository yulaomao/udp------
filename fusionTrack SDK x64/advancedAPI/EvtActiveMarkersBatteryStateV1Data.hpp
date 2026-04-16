/** \file EvtActiveMarkersBatteryStateV1Data.hpp
 * \brief File defining all the marker battery state event (version 1) data structures for the C++ API.
 *
 * This file is a collection of all marker battery state event data structures for the C++ API.
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

    /** \brief Class wrapping a single battery status.
     *
     * This class contains the data of \e one battery status reading. It is actually a wrapper around the
     * ::EvtActiveMarkersBatteryStateV1Payload structure.
     */
    class ActiveMarkersBatteryStateV1Item : protected EvtActiveMarkersBatteryStateV1Payload
    {
    public:
        /** \brief Default implementation of the default constructor.
         */
        ActiveMarkersBatteryStateV1Item() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        ActiveMarkersBatteryStateV1Item( const ActiveMarkersBatteryStateV1Item& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        ActiveMarkersBatteryStateV1Item( ActiveMarkersBatteryStateV1Item&& other ) = default;

        /** \brief Constructor promoting a ::EvtActiveMarkersBatteryStateV1Payload instance.
         *
         * This constructor allows to promote a ::EvtActiveMarkersBatteryStateV1Payload instance.
         *
         * \param[in] other instance to promote.
         */
        ActiveMarkersBatteryStateV1Item( const EvtActiveMarkersBatteryStateV1Payload& other );

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~ActiveMarkersBatteryStateV1Item() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        ActiveMarkersBatteryStateV1Item& operator=( const ActiveMarkersBatteryStateV1Item& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        ActiveMarkersBatteryStateV1Item& operator=( ActiveMarkersBatteryStateV1Item&& other ) = default;

        /** \brief Comparison operator.
         *
         * This method compares the image count, device ID and battery state.
         *
         * \param[in] other instance to compare.
         *
         * \retval true if the the image count, device ID and battery state match,
         * \retval false if not.
         */
        bool operator==( const ActiveMarkersBatteryStateV1Item& other ) const;

        /** \brief Getter for ::EvtActiveMarkersBatteryStateV1Payload::ImageCount.
         *
         * \retval ::EvtActiveMarkersBatteryStateV1Payload::ImageCount as value
         */
        uint32 imageCount() const;

        /** \brief Getter for ::EvtActiveMarkersBatteryStateV1Payload::DeviceID.
         *
         * \retval ::EvtActiveMarkersBatteryStateV1Payload::DeviceID as value.
         */
        uint8 deviceID() const;

        /** \brief Getter for ::EvtActiveMarkersBatteryStateV1Payload::BatteryState.
         *
         * \retval ::EvtActiveMarkersBatteryStateV1Payload::BatteryState as
         * value
         */
        uint8 batteryState() const;
    };

    /** \brief Class providing access to the active marker battery state event.
     *
     * This class contains all the information about the active marker battery state sent in the
     * FtkEventType::fetActiveMarkersBatteryStateV1 event. The instance does now if it has been properly set
     * (i.e. whether its member values make sense).
     */
    class EvtActiveMarkersBatteryStateV1Data
    {
    public:
        /** \brief Default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        EvtActiveMarkersBatteryStateV1Data() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        EvtActiveMarkersBatteryStateV1Data( const EvtActiveMarkersBatteryStateV1Data& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        EvtActiveMarkersBatteryStateV1Data( EvtActiveMarkersBatteryStateV1Data&& other ) = default;

        /** \brief Constructor promoting a ::ftkEvent instance of type
         * FtkEventType::fetActiveMarkersBatteryStateV1.
         *
         * This constructor reads the whole ::EvtActiveMarkersBatteryStateV1Payload data and populates the
         * _States container.
         *
         * \param[in] other reference on the \e first ::EvtActiveMarkersBatteryStateV1Payload instance.
         * \param[in] payload ::ftkEvent payload size.
         */
        EvtActiveMarkersBatteryStateV1Data( const EvtActiveMarkersBatteryStateV1Payload& other,
                                            uint32 payload );

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~EvtActiveMarkersBatteryStateV1Data() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        EvtActiveMarkersBatteryStateV1Data& operator=( const EvtActiveMarkersBatteryStateV1Data& other ) =
          default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        EvtActiveMarkersBatteryStateV1Data& operator=( EvtActiveMarkersBatteryStateV1Data&& other ) = default;

        /** \brief Getter for #_States.
         *
         * \retval _States as a const reference.
         */
        const std::vector< ActiveMarkersBatteryStateV1Item >& states() const;

        /** \brief Getter for #_Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Getter for #_InvalidInstance.
         *
         * \retval _InvalidInstance as a const reference.
         */
        static const EvtActiveMarkersBatteryStateV1Data& invalidInstance();

    protected:
        /** \brief Invalid instance.
         *
         * This instance is used as return value when the EvtActiveMarkersBatteryStateV1Data to be accessed
         * is not defined, either if the index is too big or there are no stored
         * EvtActiveMarkersBatteryStateV1Data instances.
         */
        static const EvtActiveMarkersBatteryStateV1Data _InvalidInstance;

        /** \brief Container for the battery states of paired markers.
         */
        std::vector< ActiveMarkersBatteryStateV1Item > _States{};

        /** Contains \c true if the instance contains valid information.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys
