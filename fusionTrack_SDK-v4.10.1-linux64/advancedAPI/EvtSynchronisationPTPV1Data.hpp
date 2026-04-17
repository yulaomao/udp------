/** \file EvtSynchronisationPTPV1Data.hpp
 * \brief File defining the event PTP synchronisation data structure for the C++ API.
 *
 * This file defines the C++ API equivalent of the ::EvtSynchronisationPTPV1Payload structure.
 */
#pragma once

#include <ftkEvent.h>
#include <ftkTypes.h>

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

/**
 * \addtogroup adApi
 * \{
 */

namespace atracsys
{
    class FrameData;

    /** \brief Class providing access to the PTP synchronisation event.
     *
     * This class contains the data of \e a PTP synchronisation event. It is actually a wrapper around the
     * ::EvtSynchronisationPTPV1Payload structure.
     */
    class EvtSynchronisationPTPV1Data : protected EvtSynchronisationPTPV1Payload
    {
    public:
        /** \brief Default implementation of the default constructor.
         *
         * This constructor builds an invalid instance.
         */
        EvtSynchronisationPTPV1Data() = default;

        /** \brief Default implementation of the copy-constructor.
         *
         * This constructor allows to duplicate an instance.
         *
         * \param[in] other instance to copy.
         */
        EvtSynchronisationPTPV1Data( const EvtSynchronisationPTPV1Data& other ) = default;

        /** \brief Default implementation of the move-constructor.
         *
         * This constructor allows to move an instance.
         *
         * \param[in] other instance to move.
         */
        EvtSynchronisationPTPV1Data( EvtSynchronisationPTPV1Data&& other ) = default;

        /** \brief Constructor promoting a ::ftkEvent instance of type
         * FtkEventType::fetSynchronisationPTPV1.
         *
         * This constructor reads the whole ::EvtSynchronisationPTPV1Payload data and populates the members.
         *
         * \param[in] other ::EvtSynchronisationPTPV1Payload instance.
         */
        EvtSynchronisationPTPV1Data( const EvtSynchronisationPTPV1Payload& other );

        /**\brief Default implementation of the destructor.
         */
        virtual ~EvtSynchronisationPTPV1Data() = default;
        /** \brief Default implementation of the affectation operator.
         *
         * This constructor allows to duplicate an instance.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        EvtSynchronisationPTPV1Data& operator=( const EvtSynchronisationPTPV1Data& other ) = default;

        /** \brief Default implementation of the move-affectation operator.
         *
         * This constructor allows to move an instance.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        EvtSynchronisationPTPV1Data& operator=( EvtSynchronisationPTPV1Data&& other ) = default;

        /** \brief Getter for the PTP timestamp.
         *
         * Allows to access EvtSynchronisationPTPV1Payload::Timestamp.
         *
         * \return a const reference on EvtSynchronisationPTPV1Payload::Timestamp.
         */
        const ftkTimestampPTP& timestamp() const;

        /** \brief Getter for the PTP last correction.
         *
         * Allows to access EvtSynchronisationPTPV1Payload::LastCorrection.
         *
         * \return a const reference on EvtSynchronisationPTPV1Payload::LastCorrection.
         */
        const ftkTimestampCorrectionPTP& lastCorrection() const;

        /** \brief Getter for the PTP parent ID.
         *
         * Allows to access EvtSynchronisationPTPV1Payload::ParentId.
         *
         * \return a const reference on EvtSynchronisationPTPV1Payload::ParentId.
         */
        const ftkParentId& parentId() const;

        /** \brief Getter for the PTP port state.
         *
         * This method interprets the int value of the EvtSynchronisationPTPV1Payload::Status::PortStateId
         * bitfield and translates it into a ftkPortStatePTP instance.
         *
         * \return the ftkPortStatePTP instance corresponding to the ID of the fusionTrack PTP FSM state or
         * ftkPortStatePTP::UnknownState
         */
        ftkPortStatePTP status() const;

        /** \brief Getter for the PTP error info.
         *
         * Allows to access EvtSynchronisationPTPV1Payload::ErrorId.
         *
         * \return a const reference on EvtSynchronisationPTPV1Payload::ErrorId.
         */
        ftkErrorPTP errorId() const;

        /** \brief Getter for _Valid.
         *
         * This method allows to access _Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Getter for _InvalidInstance.
         *
         * This method allows to access _InvalidInstance
         *
         * \retval _InvalidInstance as a const reference.
         */
        static const EvtSynchronisationPTPV1Data& invalidInstance();

    protected:
        /** \brief Invalid instance.
         *
         * This instance is used as return value when the EvtSynchronisationPTPV1Data to be accessed is not
         * defined.
         */
        static const EvtSynchronisationPTPV1Data _InvalidInstance;
        /** \brief Contains \c true if the instance is valid.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys
