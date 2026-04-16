/** \file FiducialData.hpp
 * \brief File defining the 3D fiducial data structures for the C++ API.
 *
 * This file defines the C++ API equivalent of the ::ftk3DFiducial structure.
 */
#pragma once

#include "RawData.hpp"

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

    /** \brief Class holding the information for a 3D fiducial.
     *
     * This class contains all the information about a 3D fiducial. The instance does now if it has been
     * properly set (i.e. whether its member values make sense).
     *
     * The instance contains \e copies of the two RawData instances used to reconstruct the FiducialData one.
     */
    class FiducialData
    {
    public:
        /** \brief Default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        FiducialData() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        FiducialData( const FiducialData& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        FiducialData( FiducialData&& other ) = default;

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~FiducialData() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        FiducialData& operator=( const FiducialData& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        FiducialData& operator=( FiducialData&& other ) = default;

        /** \brief Comparison operator.
         *
         * This method compares the index, position of two instances to decide wether they are the same. If
         * any of the two instances are invalid, the comparison will fail.
         *
         * \param[in] other instance to compare.
         *
         * \retval true if both indices and position match,
         * \retval false if not.
         */
        bool operator==( const FiducialData& other ) const;

        /** \brief Getter for #_InvalidInstance.
         *
         * \retval _InvalidInstance as a const reference.
         */
        static const FiducialData& invalidInstance();

        /** \brief Getter for #_Index.
         *
         * \retval _Index as value.
         */
        uint32 index() const;

        /** \brief Getter for the left raw data instance.
         *
         * \retval *_LeftInstance as a const reference.
         */
        const RawData& leftInstance() const;

        /** \brief Getter for the right raw data instance.
         *
         * \retval *_RightInstance as a const reference.
         */
        const RawData& rightInstance() const;

        /** \brief Getter for #_Position.
         *
         * \retval _Position as a const reference.
         */
        const std::array< float, 3u >& position() const;

        /** \brief Getter for #_EpipolarError.
         *
         * \retval _EpipolarError as value.
         */
        float epipolarError() const;

        /** \brief Getter for #_TriangulationError.
         *
         * \retval _TriangulationError as value.
         */
        float triangulationError() const;

        /** \brief Getter for #_Probability.
         *
         * \retval _Probability as value.
         */
        float probability() const;

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

    protected:
        /** \brief Allows access from FrameData.
         */
        friend class FrameData;

        /** \brief Invalid instance.
         *
         * This instance is used as return value when the FiducialData to be accessed is not defined, either
         * if the index is too big or there are no stored FiducialData instances.
         */
        static const FiducialData _InvalidInstance;

        /** \brief `Promoting' constuctor.
         *
         * This constructor promotes a ::ftk3DFiducial instance into a FiducialData one.
         *
         * \param[in] fid instance to promote.
         * \param[in] idx index of the fiducial in the ::ftkFrameQuery::threeDFiducials container.
         * \param[in] left pointer on the RawData instance which is the corresponding raw data on the left
         * camera.
         * \param[in] right pointer on the RawData instance which is the corresponding raw data on the right
         * camera.
         */
        explicit FiducialData( const ftk3DFiducial& fid,
                               uint32 idx,
                               const RawData& left = RawData::invalidInstance(),
                               const RawData& right = RawData::invalidInstance() );

        /** \brief Index in the instance in the original ::ftkFrameQuery
         * instance.
         */
        uint32 _Index{ std::numeric_limits< uint32_t >::max() };

        /** \brief RawData instance from the left camera used to reconstruct the 3D point.
         *
         * If this member is RawData::invalidInstance(), it means no raw data has been retrieved from the
         * tracking system.
         */
        RawData _LeftInstance{ { RawData::invalidInstance() } };

        /** \brief RawData instance from the right camera used to reconstruct the 3D point.
         *
         * If this member is RawData::invalidInstance(), it means no raw data has been retrieved from the
         * tracking system.
         */
        RawData _RightInstance{ { RawData::invalidInstance() } };

        /** \brief Position of the 3D point, in \f$\si{\milli\metre}\f$.
         */
        std::array< float, 3u > _Position = { { 0.f, 0.f, 0.f } };

        /** \brief Epipolar error, in \f$\si{\milli\metre}\f$.
         */
        float _EpipolarError{ -1.f };

        /** \brief Triangulation error, in \f$\si{\milli\metre}\f$.
         */
        float _TriangulationError{ -1.f };

        /** \brief Probability of the 3D fiducial.
         *
         * The probability is defined as the inverse of the multiplicities of the used raw data. For
         * instance, if the left raw data is used in two different fiducials and the right raw data is used
         * in only one fiducial, the multiplicity is \f$ 2 \times 1 \f$, and therefore the probability is
         * \f$0.5\f$.
         */
        float _Probability{ -1.f };

        /** \brief _Status of the fiducial data.
         */
        ftkStatus _Status{};

        /** Contains \c true if the instance contains valid information.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys
