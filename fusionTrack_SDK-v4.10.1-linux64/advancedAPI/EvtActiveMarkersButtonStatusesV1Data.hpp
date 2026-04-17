/** \file ActiveMarkersButtonStatusesV1Item.hpp
 * \brief File defining all the active marker button status (version 1) data structures for the C++ API.
 *
 * This file is a collection of all marker button status event data structures for the C++ API.
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

    /** \brief Class wrapping a single button status.
     *
     * This class contains the data of \e one button status reading. It is actually a wrapper around the
     * ::EvtActiveMarkersButtonStatusesV1Payload structure.
     */
    class ActiveMarkersButtonStatusesV1Item : protected EvtActiveMarkersButtonStatusesV1Payload
    {
    public:
        /** \brief Default implementation of the default constructor.
         */
        ActiveMarkersButtonStatusesV1Item() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        ActiveMarkersButtonStatusesV1Item( const ActiveMarkersButtonStatusesV1Item& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        ActiveMarkersButtonStatusesV1Item( ActiveMarkersButtonStatusesV1Item&& other ) = default;

        /** \brief Constructor promoting a ::EvtActiveMarkersButtonStatusesV1Payload instance.
         *
         * This constructor allows to promote a ::EvtActiveMarkersButtonStatusesV1Payload
         * instance.
         *
         * \param[in] other instance to promote.
         */
        ActiveMarkersButtonStatusesV1Item( const EvtActiveMarkersButtonStatusesV1Payload& other );

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~ActiveMarkersButtonStatusesV1Item() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        ActiveMarkersButtonStatusesV1Item& operator=( const ActiveMarkersButtonStatusesV1Item& other ) =
          default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        ActiveMarkersButtonStatusesV1Item& operator=( ActiveMarkersButtonStatusesV1Item&& other ) = default;

        /** \brief Comparison operator.
         *
         * This method compares the image count, device ID and button status.
         *
         * \param[in] other instance to compare.
         *
         * \retval true if the the image count, device ID and button status match,
         * \retval false if not.
         */
        bool operator==( const ActiveMarkersButtonStatusesV1Item& other ) const;

        /** \brief Getter for ::EvtActiveMarkersButtonStatusesV1Payload::ImageCount.
         *
         * \retval ::EvtActiveMarkersButtonStatusesV1Payload::ImageCount as value
         */
        uint32 imageCount() const;

        /** \brief Getter for ::EvtActiveMarkersButtonStatusesV1Payload::DeviceID.
         *
         * \retval ::EvtActiveMarkersButtonStatusesV1Payload::DeviceID as value.
         */
        uint8 deviceID() const;

        /** \brief Getter for ::EvtActiveMarkersButtonStatusesV1Payload::ButtonStatus.
         *
         * \retval ::EvtActiveMarkersButtonStatusesV1Payload::ButtonStatus as
         * value.
         */
        uint8 buttonStatus() const;
    };

    /** \brief Class providing access to the active marker button status event.
     *
     * This class contains all the information about the active marker button status sent in the
     * FtkEventType::fetActiveMarkersButtonStatusV1 event. The instance does know if it has been properly set
     * (i.e. whether its member values make sense).
     */
    class EvtActiveMarkersButtonStatusesV1Data
    {
    public:
        /** \brief Default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        EvtActiveMarkersButtonStatusesV1Data() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        EvtActiveMarkersButtonStatusesV1Data( const EvtActiveMarkersButtonStatusesV1Data& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        EvtActiveMarkersButtonStatusesV1Data( EvtActiveMarkersButtonStatusesV1Data&& other ) = default;

        /** \brief Constructor promoting a ::ftkEvent instance of type
         * FtkEventType::EvtActiveMarkersButtonStatusesV1.
         *
         * This constructor reads the whole ::EvtActiveMarkersButtonStatusesV1Payload data and populates the
         * _Statuses container.
         *
         * \param[in] other reference on the \e first ::EvtActiveMarkersButtonStatusesV1Payload instance.
         * \param[in] payload ::ftkEvent payload size.
         */
        EvtActiveMarkersButtonStatusesV1Data( const EvtActiveMarkersButtonStatusesV1Payload& other,
                                              uint32 payload );

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~EvtActiveMarkersButtonStatusesV1Data() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        EvtActiveMarkersButtonStatusesV1Data& operator=( const EvtActiveMarkersButtonStatusesV1Data& other ) =
          default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        EvtActiveMarkersButtonStatusesV1Data& operator=( EvtActiveMarkersButtonStatusesV1Data&& other ) =
          default;

        /** \brief Getter for #_Statuses.
         *
         * \retval _Statuses as a const reference.
         */
        const std::vector< ActiveMarkersButtonStatusesV1Item >& statuses() const;

        /** \brief Getter for #_Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Getter for #_InvalidInstance.
         *
         * \retval _InvalidInstance as a const reference.
         */
        static const EvtActiveMarkersButtonStatusesV1Data& invalidInstance();

    protected:
        /** \brief Invalid instance.
         *
         * This instance is used as return value when the EvtActiveMarkersButtonStatusesV1Data to be accessed
         * is not defined, either if the index is too big or there are no stored
         * EvtActiveMarkersButtonStatusesV1Data instances.
         */
        static const EvtActiveMarkersButtonStatusesV1Data _InvalidInstance;

        /** \brief Container for the button statuses of paired markers.
         */
        std::vector< ActiveMarkersButtonStatusesV1Item > _Statuses{};

        /** Contains \c true if the instance contains valid information.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys
