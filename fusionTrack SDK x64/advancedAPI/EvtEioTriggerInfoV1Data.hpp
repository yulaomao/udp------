/** \file EvtEioTriggerInfoV1Data.hpp
 * \brief File defining the event EIO trigger (version 1) data structure for the C++ API.
 *
 * This file defines the C++ API equivalent of the ::EvtEioTriggerInfoV1Data structure.
 */
#pragma once

#include <ftkEvent.h>
#include <ftkTypes.h>

/**
 * \addtogroup adApi
 * \{
 */

namespace atracsys
{
    class FrameData;

    /** \brief Class providing access to the trigger information event.
     *
     * This class contains the data of \e a EIO trigger  event. It is actually a wrapper around the
     * ::EvtEioTriggerInfoV1Payload structure.
     */
    class EvtEioTriggerInfoV1Data : protected EvtEioTriggerInfoV1Payload
    {
    public:
        /** \brief Default implementation of the default constructor.
         *
         * This constructor builds an invalid instance.
         */
        EvtEioTriggerInfoV1Data() = default;

        /** \brief Default implementation of the copy-constructor.
         *
         * This constructor allows to duplicate an instance.
         *
         * \param[in] other instance to copy.
         */
        EvtEioTriggerInfoV1Data( const EvtEioTriggerInfoV1Data& other ) = default;

        /** \brief Default implementation of the move-constructor.
         *
         * This constructor allows to move an instance.
         *
         * \param[in] other instance to move.
         */
        EvtEioTriggerInfoV1Data( EvtEioTriggerInfoV1Data&& other ) = default;

        /** \brief Constructor promoting a ::ftkEvent instance of type FtkEventType::fetEioTriggerV1.
         *
         * This constructor reads the whole ::EvtEioTriggerInfoV1Payload data and populates the members.
         *
         * \param[in] other ::EvtEioTriggerInfoV1Payload instance.
         */
        EvtEioTriggerInfoV1Data( const EvtEioTriggerInfoV1Payload& other );

        /**\brief Default implementation of the destructor.
         */
        virtual ~EvtEioTriggerInfoV1Data() = default;

        /** \brief Default implementation of the affectation operator.
         *
         * This constructor allows to duplicate an instance.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        EvtEioTriggerInfoV1Data& operator=( const EvtEioTriggerInfoV1Data& other ) = default;

        /** \brief Default implementation of the move-affectation operator.
         *
         * This constructor allows to move an instance.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        EvtEioTriggerInfoV1Data& operator=( EvtEioTriggerInfoV1Data&& other ) = default;

        /** \brief Getter for TriggerStartTime.
         *
         * This method allows to access TriggerStartTime.
         *
         * \retval TriggerStartTime as value.
         */
        uint64 triggerStartTime() const;
        /** \brief Getter for TriggerActualDurationTime.
         *
         * This method allows to access TriggerActualDurationTime.
         *
         * \retval TriggerActualDurationTime as value.
         */
        uint64 triggerActualDurationTime() const;
        /** \brief Getter for TriggerEnabledDuringExposure.
         *
         * This method allows to access TriggerEnabledDuringExposure.
         *
         * \retval true if ::TriggerEnabledDuringExposure is \c 1u,
         * \retval false if ::TriggerEnabledDuringExposure is different from \c 1u.
         */
        bool triggerEnabledDuringExposure() const;
        /** \brief Getter for TriggerIdEio1.
         *
         * This method allows to access TriggerIdEio1.
         *
         * \retval TriggerIdEio1 as value.
         */
        uint32 triggerIdEio1() const;
        /** \brief Getter for TriggerIdEio2.
         *
         * This method allows to access TriggerIdEio2.
         *
         * \retval TriggerIdEio2 as value.
         */
        uint32 triggerIdEio2() const;

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
        static const EvtEioTriggerInfoV1Data& invalidInstance();

    protected:
        /** \brief Invalid instance.
         *
         * This instance is used as return value when the EvtSynchronisationPTPV1Data to be accessed is not
         * defined.
         */
        static const EvtEioTriggerInfoV1Data _InvalidInstance;
        /** \brief Contains \c true if the instance is valid.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys
