/** \file RawData.hpp
 * \brief File defining the raw data structure for the C++ API.
 *
 * This file defines the C++ API equivalent of the ::ftkRawData structure.
 */
#pragma once

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

    /** \brief Class holding the information for a raw data.
     *
     * This class contains all the information about a raw data. The instance does know if it has been
     * properly set (i.e. whether its member values make sense).
     */
    class RawData
    {
    public:
        /** \brief Default implementation of default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        RawData() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        RawData( const RawData& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        RawData( RawData&& other ) = default;

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~RawData() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        RawData& operator=( const RawData& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        RawData& operator=( RawData&& other ) = default;

        /** \brief Comparison operator.
         *
         * This method compares the index, position of two instance to decide whether they are the same. If
         * any of the two instances are invalid, the comparison will fail.
         *
         * \param[in] other instance to compare.
         *
         * \retval true if both indices and position match,
         * \retval false if not.
         */
        bool operator==( const RawData& other ) const;

        /** \brief Getter for #_Index.
         *
         * \retval _Index as value.
         */
        uint32 index() const;

        /** \brief Getter for #_Position.
         *
         * \retval _Position as a const reference.
         */
        const std::array< float, 2u >& position() const;

        /** \brief Getter for #_Dimensions.
         *
         * \retval _Dimensions as a const reference.
         */
        const std::array< uint32, 2u >& dimensions() const;

        /** \brief Getter for the aspect ration.
         *
         * \return the value of the
         * \f$ \frac{\operatorname{min}( width, height )}{\operatorname{max}( width, height )} \f$.
         */
        float aspectRatio() const;

        /** \brief Getter for #_Status.
         *
         * \retval _Status as value.
         */
        ftkStatus status() const;

        /** \brief Getter for #_Surface.
         *
         * \retval _Surface as value.
         */
        uint32 surface() const;

        /** \brief Getter for #_Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Getter for #_InvalidInstance.
         *
         * \retval _InvalidInstance as a const reference.
         */
        static const RawData& invalidInstance();

    protected:
        /** \brief Allows access from FrameData.
         */
        friend class FrameData;

        /** \brief `Promoting' constuctor.
         *
         * This constructor promotes a ::ftkRawData instance into a RawData one.
         *
         * \param[in] raw instance to promote.
         * \param[in] idx index of the ::ftkRawData in the original ::ftkFrameQuery instance.
         */
        explicit RawData( const ftkRawData& raw, uint32 idx );

        /** \brief Invalid instance.
         *
         * This instance is used as return value when the RawData to be accessed is not defined, either if
         * the index is too big or there are no stored RawData instances.
         */
        static const RawData _InvalidInstance;

        /** \brief Index in the instance in the original ::ftkFrameQuery
         * instance.
         */
        uint32 _Index{ std::numeric_limits< uint32 >::max() };

        /** \brief Position of the raw data on the picture, in pixels.
         */
        std::array< float, 2u > _Position{};

        /** \brief Width and height of the raw data, in pixels.
         */
        std::array< uint32, 2u > _Dimensions{};

        /** \brief _Status of the raw data.
         */
        ftkStatus _Status{};

        /** \brief Number of pixels composing the raw data.
         */
        uint32 _Surface{ 0uL };

        /** Contains \c true if the instance contains valid information.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys
