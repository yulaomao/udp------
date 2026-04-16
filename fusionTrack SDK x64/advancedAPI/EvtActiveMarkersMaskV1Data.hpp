/** \file EvtActiveMarkersMaskV1Data.hpp
 * \brief File defining all the active marker mask (version 1) data structure for the C++ API.
 *
 * This file defines the C++ API equivalent of the ::EvtActiveMarkersMaskV1Payload structure.
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

    /** \brief Class providing access to the wireless marker mask event.
     *
     * This class contains all the information about the wireless marker mask sent in the
     * FtkEventType::fetActiveMarkersMaskV1 event. The instance does now if it has been properly set (i.e.
     * whether its member values make sense).
     */
    class EvtActiveMarkersMaskV1Data : protected EvtActiveMarkersMaskV1Payload
    {
    public:
        /** \brief Default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        EvtActiveMarkersMaskV1Data() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        EvtActiveMarkersMaskV1Data( const EvtActiveMarkersMaskV1Data& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        EvtActiveMarkersMaskV1Data( EvtActiveMarkersMaskV1Data&& other ) = default;

        /** \brief Constructor promoting a ::EvtActiveMarkersMaskV1Payload
         * instance.
         *
         * \param[in] other instance to promote.
         */
        EvtActiveMarkersMaskV1Data( EvtActiveMarkersMaskV1Payload other );

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~EvtActiveMarkersMaskV1Data() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        EvtActiveMarkersMaskV1Data& operator=( const EvtActiveMarkersMaskV1Data& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        EvtActiveMarkersMaskV1Data& operator=( EvtActiveMarkersMaskV1Data&& other ) = default;

        /** \brief Getter for ::EvtActiveMarkersMaskV1Payload::ActiveMarkersMask.
         *
         * \retval EvtActiveMarkersMaskV1Payload::ActiveMarkersMask as value.
         */
        uint16 activeMarkersMask() const;

        /** \brief Function allowing to determine if a given wireless marker has been paired.
         *
         * This function allows to check if a given short ID has been paired, by inspecting the received
         * mask.
         *
         * \deprecated Please use the bool isMarkerPaired(const uint32, bool&) const method instead of this
         * one.
         *
         * \param[in] idx short ID of the marker to check (valid short IDs are from 0 to 15).
         *
         * \retval true if the given short ID has been paired,
         * \retval false if the given short ID has not been paired \e or if the given short ID is invalid.
         */
#ifdef ATR_MSVC
        __declspec( deprecated( "Please use the bool isMarkerPaired(const uint32, bool&) const method" ) )
#endif
          bool isMarkerPaired( uint32 idx ) const
#if defined( ATR_GCC ) || defined( ATR_CLANG )
          __attribute__( ( deprecated ) )
#endif
          ;

        /** \brief Function allowing to determine if a given wireless marker has been paired.
         *
         * This function allows to check if a given short ID has been paired, by inspecting the received
         * mask.
         *
         * \param[in] idx short ID of the marker to check (valid short IDs are from 0 to 15).
         * \param[out] isPaired will contain \c true if the wanted marker is pa.
         *
         * \retval true if the given short ID is valid,
         * \retval false the given short ID is invalid (i.e. \c idx \f$ \ge 15 \f$).
         */
        bool isMarkerPaired( const uint32 idx, bool& isPaired ) const;

        /** \brief Getter for #_Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Getter for #_InvalidInstance.
         *
         * \retval _InvalidInstance as a const reference.
         */
        static const EvtActiveMarkersMaskV1Data& invalidInstance();

    protected:
        /** \brief Invalid instance.
         *
         * This instance is used as return value when the EvtActiveMarkersMaskV1Data to be accessed is not
         * defined, either if the index is too big or there are no stored EvtActiveMarkersMaskV1Data
         * instances.
         */
        static const EvtActiveMarkersMaskV1Data _InvalidInstance;

        /** Contains \c true if the instance contains valid information.
         */
        bool _Valid{ false };
    };

    /** \brief Class providing access to the wireless marker mask event.
     *
     * This class contains all the information about the wireless marker mask sent in the
     * FtkEventType::fetActiveMarkersMaskV1 event. The instance does now if it has been properly set (i.e.
     * whether its member values make sense).
     */
    class EvtActiveMarkersMaskV2Data : protected EvtActiveMarkersMaskV2Payload
    {
    public:
        /** \brief Default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        EvtActiveMarkersMaskV2Data() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        EvtActiveMarkersMaskV2Data( const EvtActiveMarkersMaskV2Data& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        EvtActiveMarkersMaskV2Data( EvtActiveMarkersMaskV2Data&& other ) = default;

        /** \brief Constructor promoting a ::EvtActiveMarkersMaskV2Payload
         * instance.
         *
         * \param[in] other instance to promote.
         */
        EvtActiveMarkersMaskV2Data( EvtActiveMarkersMaskV2Payload other );

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~EvtActiveMarkersMaskV2Data() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        EvtActiveMarkersMaskV2Data& operator=( const EvtActiveMarkersMaskV2Data& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        EvtActiveMarkersMaskV2Data& operator=( EvtActiveMarkersMaskV2Data&& other ) = default;

        /** \brief Getter for ::EvtActiveMarkersMaskV2Payload::ActiveMarkersMask.
         *
         * \retval EvtActiveMarkersMaskV2Payload::ActiveMarkersMask as value.
         */
        uint16 activeMarkersMask() const;

        /** \brief Getter for ::EvtActiveMarkersMaskV2Payload::ActiveMarkersErrorMask.
         *
         * \retval EvtActiveMarkersMaskV2Payload::ActiveMarkersErrorMask as value.
         */
        uint16 activeMarkersErrorMask() const;

        /** \brief Function allowing to determine if a given wireless marker has been paired.
         *
         * This function allows to check if a given short ID has been paired, by inspecting the received
         * mask.
         *
         * \param[in] idx short ID of the marker to check (valid short IDs are from 0 to 15).
         * \param[out] isPaired will contain \c true if the wanted marker is pa.
         *
         * \retval true if the given short ID is valid,
         * \retval false the given short ID is invalid (i.e. \c idx \f$ \ge 15 \f$).
         */
        bool isMarkerPaired( const uint32 idx, bool& isPaired ) const;

        /** \brief Function allowing to determine if a given wireless marker has an error status.
         *
         * This function allows to check if a given short ID has an error status. It first checks the
         * wanted marker is paired.
         *
         * \param[in] idx short ID of the marker to check (valid short IDs are from 0 to 15).
         * \param[out] isInError will contain \c true if an error is set for the wanted marker.
         *
         * \retval true if the given short ID is valid,
         * \retval false if the given short ID has not been paired \e or if the given short ID is invalid.
         */
        bool isMarkerInError( const uint32 idx, bool& isInError ) const;

        /** \brief Getter for #_Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Getter for #_InvalidInstance.
         *
         * \retval _InvalidInstance as a const reference.
         */
        static const EvtActiveMarkersMaskV2Data& invalidInstance();

    protected:
        /** \brief Invalid instance.
         *
         * This instance is used as return value when the EvtActiveMarkersMaskV2Data to be accessed is not
         * defined, either if the index is too big or there are no stored EvtActiveMarkersMaskV2Data
         * instances.
         */
        static const EvtActiveMarkersMaskV2Data _InvalidInstance;

        /** Contains \c true if the instance contains valid information.
         */
        bool _Valid{ false };
    };

}  // namespace atracsys
