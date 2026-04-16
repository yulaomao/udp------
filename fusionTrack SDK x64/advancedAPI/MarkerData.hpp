/** \file MarkerData.hpp
 * \brief File defining the marker data structures for the C++ API.
 *
 * This file defines the C++ API equivalent of the ::ftkMarker structure.
 */
#pragma once

#include "FiducialData.hpp"

#include <ftkInterface.h>

#include <array>
#include <cstdint>
#include <limits>

/**
 * \addtogroup adApi
 * \{
 */

namespace atracsys
{
    class FrameData;

    /** \brief Class holding the information for a marker.
     *
     * This class contains all the information about a marker. The instance does now if it has been properly
     * set (i.e. whether its member values make sense).
     *
     * The instance contains \e copies of all FiducialData instances used to reconstruct the marker.
     */
    class MarkerData
    {
    public:
        /** \brief Default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        MarkerData() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        MarkerData( const MarkerData& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        MarkerData( MarkerData&& other ) = default;

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~MarkerData() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        MarkerData& operator=( const MarkerData& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        MarkerData& operator=( MarkerData&& other ) = default;

        /** \brief Comparison operator.
         *
         * This method compares the index, tracking and geometry IDs, position of two instance to decide
         * whether they are the same. If any of the two instances are invalid, the comparison will fail.
         *
         * \param[in] other instance to compare.
         *
         * \retval true if indices, tracking and geometry IDs and positions match,
         * \retval false if not.
         */
        bool operator==( const MarkerData& other ) const;

        /** \brief Getter for #_Index.
         *
         * \retval _Index as value.
         */
        uint32 index() const;

        /** \brief Getter for #_TrackingId.
         *
         * \retval _TrackingId as value.
         */
        uint32 trackingId() const;

        /** \brief Getter for #_GeometryId.
         *
         * \retval _GeometryId as value.
         */
        uint32 geometryId() const;

        /** \brief Getter for #_PresenceMask.
         *
         * \retval _PresenceMask as value.
         */
        uint32 presenceMask() const;

        /** \brief Getter for #_Position.
         *
         * \retval _Position as a const reference.
         */
        const std::array< float, 3u >& position() const;

        /** \brief Getter for #_Rotation.
         *
         * \retval _Rotation as a const reference.
         */
        const std::array< std::array< float, 3u >, 3u >& rotation() const;

        /** \brief Getter for #_RegistrationError.
         *
         * \retval _RegistrationError as value.
         */
        float registrationError() const;

        /** \brief Getter for #_Status.
         *
         * \retval _Status as value.
         */
        ftkStatus status() const;

        /** \brief Getter for #_Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Getter for the wanted 3D fiducial instance.
         *
         * \retval _FiducialInstances \c [idx] as a const reference.
         */
        const FiducialData& correspondingFiducial( size_t idx ) const;

    protected:
        /** \brief Allows access from FrameData.
         */
        friend class FrameData;

        /** \brief `Promoting' constuctor.
         *
         * This constructor promotes a ::ftkMarker instance into a MarkerData one.
         *
         * \param[in] marker instance to promote.
         * \param[in] idx index of the instance in the ::ftkFrameQuery::markers container.
         * \param[in] fids FiducialData instances used to build the marker.
         */
        explicit MarkerData( const ftkMarker& marker,
                             uint32 idx,
                             const std::array< FiducialData, FTK_MAX_FIDUCIALS >& fids );

        /** \brief Index in the instance in the original ::ftkFrameQuery instance.
         */
        uint32 _Index{ std::numeric_limits< uint32_t >::max() };

        /** \brief Unique ID given by the tracking engine.
         *
         * On two different frames, markers with the same tracking ID represent the same physical object.
         */
        uint32 _TrackingId{ std::numeric_limits< uint32_t >::max() };

        /** \brief Geometry ID of the recontructed marker.
         */
        uint32 _GeometryId{ std::numeric_limits< uint32_t >::max() };

        /** \brief Bit \f$i\f$ is set to \c 1 if fiducial \f$i\f$ was found in the recontruction.
         */
        uint32 _PresenceMask{ 0uL };

        /** \brief Position of the marker, in \f$\si{\milli\metre}\f$.
         */
        std::array< float, 3u > _Position{};

        /** \brief Rotation matrix defining the marker orientation.
         */
        std::array< std::array< float, 3u >, 3u > _Rotation{};

        /** \brief Container of used FiducialData instances.
         *
         * An invalid instance indicates either that the corresponding fiducial could not be found or that
         * the geometry has no fiducial corresponding to the given index.
         */
        std::array< FiducialData, FTK_MAX_FIDUCIALS > _FiducialInstances{};

        /** \brief Marker registration error
         */
        float _RegistrationError{ -1.f };

        /** \brief _Status of the marker data.
         */
        ftkStatus _Status{};

        /** Contains \c true if the instance contains valid information.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys
