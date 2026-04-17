/** \file FrameData.hpp
 * \brief File defining the frame query data structures for the C++ API.
 *
 * This file defines the C++ API equivalent of the ::ftkFrameQuery structure.
 */
#pragma once

#include "EventData.hpp"
#include "EvtActiveMarkersBatteryStateV1Data.hpp"
#include "EvtActiveMarkersButtonStatusesV1Data.hpp"
#include "EvtActiveMarkersMaskV1Data.hpp"
#include "EvtEioTaggingV1Data.hpp"
#include "EvtEioTriggerInfoV1Data.hpp"
#include "EvtFansV1Data.hpp"
#include "EvtSynchronisationPTPV1Data.hpp"
#include "EvtSyntheticTemperatureV1Data.hpp"
#include "EvtTemperatureV4Data.hpp"
#include "FiducialData.hpp"
#include "HeaderData.hpp"
#include "Marker64FiducialsData.hpp"
#include "MarkerData.hpp"
#include "PictureData.hpp"
#include "RawData.hpp"

#include <ftkInterface.h>

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
    /** \brief Class containing all the frame data.
     *
     * This class contains all the information about a frame. The instance does know if it has been properly
     * set (i.e. whether its member values make sense).
     *
     * If a field has an invalid status (exception: ftkQueryStatus::QS_ERR_OVERFLOW) in the original
     * ::ftkFrameQuery instance, the corresponding field in FrameData will be invalid / empty.
     *
     * As FiducialData contains copies of the two used RawData instances and MarkerData contains copies of
     * the used FiducialData instances, several FiducialData and RawData instances are contained more than
     * once in the FrameData instance. The implementation choice is to privilege convenience over memory
     * consumption.
     *
     * \warning The order of elements in the FrameData::_LeftRaws, FrameData::_RightRaws,
     * FrameData::_Fiducials, FrameData::_Markers and FrameData::_Events containers is \e not stable, i.e.
     * it is \e not guaranteed. No application should rely on the order of the element in those containers.
     */
    class FrameData
    {
    public:
        /** \brief Default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        FrameData() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        FrameData( const FrameData& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        FrameData( FrameData&& other ) = default;

        /** \brief `Promoting' constuctor.
         *
         * This constructor promotes a ::ftkFrameQuery instance into a
         * FrameData one.
         *
         * \param[in] frame instance to promote.
         */
        FrameData( ftkFrameQuery* frame );

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~FrameData() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        FrameData& operator=( const FrameData& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        FrameData& operator=( FrameData&& other ) = default;

        /** \brief Assignment promoting operator.
         *
         * This method allows to promote a ::ftkFrameQuery instance into the current one.
         *
         * \param[in] frame instance to promote.
         *
         * \retval *this as a reference.
         */
        FrameData& operator=( ftkFrameQuery* frame );

        /** \brief Getter for #_Header.
         *
         * \retval _Header as a const reference.
         */
        const HeaderData& header() const;

        /** \brief Getter for #_LeftRaws.
         *
         * \retval _LeftRaws as a const reference.
         */
        const std::vector< RawData >& leftRaws() const;

        /** \brief Getter for #_RightRaws.
         *
         * \retval _RightRaws as a const reference.
         */
        const std::vector< RawData >& rightRaws() const;

        /** \brief Getter for #_Fiducials.
         *
         * \retval _Fiducials as a const reference.
         */
        const std::vector< FiducialData >& fiducials() const;

        /** \brief Getter for #_Markers.
         *
         * \retval _Markers as a const reference.
         */
        const std::vector< MarkerData >& markers() const;

        /** \brief Getter for #_SixtyFourFiducialsMarkers.
         *
         * \retval _SixtyFourFiducialsMarkers as a const reference.
         */
        const std::vector< Marker64FiducialsData >& sixtyFourFiducialsMarkers() const;

        /** \brief Getter for #_Events.
         *
         * \retval _Events as a const reference.
         */
        const std::vector< EventData >& events() const;

        /** \brief Getter for #_LeftPicture.
         *
         * \retval _LeftPicture as a const reference.
         */
        const PictureData& leftPicture() const;

        /** \brief Getter for #_RightPicture.
         *
         * \retval _RightPicture as a const reference.
         */
        const PictureData& rightPicture() const;

    protected:
        /** \brief Helper method to promote a ::ftkFrameQuery instance.
         *
         * This method performs the operation needed to convert a ::ftkFrameQuery instance into a FrameData
         * one. It is called by the FrameData::operator=(ftkFrameQuery*) and
         * FrameData::FrameData(ftkFrameQuery*) methods.
         *
         * \param[in] frame instance to promote.
         */
        void _buildRelations( ftkFrameQuery* frame );

        /** \brief Frame header.
         */
        HeaderData _Header{};

        /** \brief Container for the left camera raw data instances.
         */
        std::vector< RawData > _LeftRaws{};

        /** \brief Container for the right camera raw data instances.
         */
        std::vector< RawData > _RightRaws{};

        /** \brief Container for the fiducial data instances.
         */
        std::vector< FiducialData > _Fiducials{};

        /** \brief Container for the marker data instances.
         */
        std::vector< MarkerData > _Markers{};

        /** \brief Container for the 64-fiducials marker data instances.
         */
        std::vector< Marker64FiducialsData > _SixtyFourFiducialsMarkers{};

        /** \brief Container for the event data instances.
         */
        std::vector< EventData > _Events{};

        /** \brief Left picture.
         */
        PictureData _LeftPicture{};

        /** \brief Left picture.
         */
        PictureData _RightPicture{};
    };

    // class StereoCalibrationData : protected ftkStereoParameters

}  // namespace atracsys

/**
 * \}
 */
