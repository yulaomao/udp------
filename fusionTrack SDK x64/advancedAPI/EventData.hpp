/** \file EventData.hpp
 * \brief File defining the event data structure for the C++ API.
 *
 * This file defines the C++ API equivalent of the ::ftkEvent structure.
 */
#pragma once

#include "EvtActiveMarkersBatteryStateV1Data.hpp"
#include "EvtActiveMarkersButtonStatusesV1Data.hpp"
#include "EvtActiveMarkersMaskV1Data.hpp"
#include "EvtEioTaggingV1Data.hpp"
#include "EvtEioTriggerInfoV1Data.hpp"
#include "EvtFansV1Data.hpp"
#include "EvtSynchronisationPTPV1Data.hpp"
#include "EvtSyntheticTemperatureV1Data.hpp"
#include "EvtTemperatureV4Data.hpp"

#include <ftkEvent.h>
#include <ftkTypes.h>

#include <cstdint>
#include <memory>

/**
 * \addtogroup adApi
 * \{
 */

namespace atracsys
{
    class FrameData;

    /** \brief Class containing the device event data.
     *
     * This class contains the SDK sent event data. The instance knows if it is properly set.
     */
    class EventData
    {
    public:
        /** \brief Default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        EventData();

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        EventData( const EventData& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        EventData( EventData&& other ) = default;

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~EventData() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        EventData& operator=( const EventData& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        EventData& operator=( EventData&& other ) = default;

        /** \brief Comparison operator.
         *
         * This method compares the index, type and payload of two instance to decide wether they are the
         * same. If any of the two instances are invalid, the comparison will fail.
         *
         * \param[in] other instance to compare.
         *
         * \retval true if index, type and payload match,
         * \retval false if not.
         */
        bool operator==( const EventData& other ) const;

        /** \brief Getter for #_Type.
         *
         * \retval _Type as value.
         */
        FtkEventType type() const;

        /** \brief Getter for #_Payload.
         *
         * \retval _Payload as value.
         */
        uint32 payload() const;

        /** \brief Getter for #_Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Maximal number of temperature sensors sent in a event of type FtkEventType::fetTempV4.
         */
        static constexpr size_t MaxTemperatureSensors = 20u;

        /** \brief Maximal number of active wireless markers, managed by events of type
         * FtkEventType::fetActiveMarkersMaskV1, FtkEventType::fetActiveMarkersButtonStatusV1 and
         * FtkEventType::fetActiveMarkersBatteryStateV1.
         */
        static constexpr size_t MaxActiveMarkerNumber = 16u;

        /** \brief Getter for #_FansV1.
         *
         * \retval _FansV1 as a const reference.
         */
        const EvtFansV1Data& fansV1() const;

        /** \brief Getter for #_TemperatureV4.
         *
         * \retval _TemperatureV4 as a const reference.
         */
        const EvtTemperatureV4Data& temperatureV4() const;

        /** \brief Getter for #_ActiveMarkerMaskV1.
         *
         * \retval _ActiveMarkerMaskV1 as a const reference.
         */
        const EvtActiveMarkersMaskV1Data& activeMarkerMaskV1() const;

        /** \brief Getter for #_ActiveMarkerMaskV2.
         *
         * \retval _ActiveMarkerMaskV2 as a const reference.
         */
        const EvtActiveMarkersMaskV2Data& activeMarkerMaskV2() const;

        /** \brief Getter for #_ActiveMarkersButtonStatusesV1.
         *
         * \retval _ActiveMarkersButtonStatusesV1 as a const reference.
         */
        const EvtActiveMarkersButtonStatusesV1Data& activeMarkersButtonStatusesV1() const;

        /** \brief Getter for #_ActiveMarkerBatteryStateV1.
         *
         * \retval _ActiveMarkerBatteryStateV1 as a const reference.
         */
        const EvtActiveMarkersBatteryStateV1Data& activeMarkerBatteryStateV1() const;

        /** \brief Getter for #_SyntheticTemperatureV1.
         *
         * \retval _SyntheticTemperatureV1 as a const reference.
         */
        const EvtSyntheticTemperatureV1Data& syntheticTemperatureV1() const;

        /** \brief Getter for #_SynchronisationPTPV1.
         *
         * \retval _SynchronisationPTPV1 as a const reference.
         */
        const EvtSynchronisationPTPV1Data& synchronisationPTPV1() const;

        /** \brief Getter for #_EioTaggingV1Data.
         *
         * \retval _EioTaggingV1Data as a const reference.
         */
        const EvtEioTaggingV1Data& eioTaggingV1Data() const;

        /** \brief Getter for #_EioTriggerInfoV1Data.
         *
         * \retval _EioTriggerInfoV1Data as a const reference.
         */
        const EvtEioTriggerInfoV1Data& eioTriggerInfoV1Data() const;

    protected:
        /** \brief Allows access from FrameData.
         */
        friend class FrameData;

        /** \brief `Promoting' constuctor.
         *
         * This constructor promotes a ::ftkEvent instance into an EventData one.
         *
         * \param[in] evt instance to promote.
         */
        explicit EventData( const ftkEvent& evt );

        /** \brief _Type of the contained event.
         */
        FtkEventType _Type;

        /** \brief Size of the event payload (in bytes).
         *
         * The size is used to know how many elements are stored in the payload.
         */
        uint32 _Payload;

        /** \brief Fan status and speed data.
         */
        std::shared_ptr< EvtFansV1Data > _FansV1{ nullptr };

        /** \brief Temperature data.
         */
        std::shared_ptr< EvtTemperatureV4Data > _TemperatureV4{ nullptr };

        /** \brief Discovered active markers.
         */
        std::shared_ptr< EvtActiveMarkersMaskV1Data > _ActiveMarkerMaskV1{ nullptr };

        /** \brief Discovered active markers.
         */
        std::shared_ptr< EvtActiveMarkersMaskV2Data > _ActiveMarkerMaskV2{ nullptr };

        /** \brief Buttons statuses for discovered active markers.
         */
        std::shared_ptr< EvtActiveMarkersButtonStatusesV1Data > _ActiveMarkersButtonStatusesV1{ nullptr };

        /** \brief Battery status for discovered active markers.
         */
        std::shared_ptr< EvtActiveMarkersBatteryStateV1Data > _ActiveMarkerBatteryStateV1{ nullptr };

        /** \brief Synthetic temperature events.
         */
        std::shared_ptr< EvtSyntheticTemperatureV1Data > _SyntheticTemperatureV1{ nullptr };

        /** \brief PTP synchronisation event.
         */
        std::shared_ptr< EvtSynchronisationPTPV1Data > _SynchronisationPTPV1{ nullptr };

        /** \brief EIO tagging event.
         */
        std::shared_ptr< EvtEioTaggingV1Data > _EioTaggingV1Data{ nullptr };

        /** \brief EIO trigger event.
         */
        std::shared_ptr< EvtEioTriggerInfoV1Data > _EioTriggerInfoV1Data{ nullptr };

        /** \brief Contains \c true if the instance contains valid information.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys
