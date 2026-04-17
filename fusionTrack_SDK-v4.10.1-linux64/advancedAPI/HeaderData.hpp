/** \file HeaderData.hpp
 * \brief File defining the image header data structures for the C++ API.
 *
 * This file defines the C++ API equivalent of the ::ftkImageHeader structure.
 */
#pragma once

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
    class FrameData;

    /** \brief Class holding the frame header data.
     *
     * This class contains all the frame header data. The instance does know if it has been properly set (i.e.
     * whether its member values make sense).
     */
    class HeaderData
    {
    public:
        /** \brief Default implementation of default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        HeaderData() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        explicit HeaderData( const HeaderData& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        HeaderData( HeaderData&& other ) = default;

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~HeaderData() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        HeaderData& operator=( const HeaderData& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        HeaderData& operator=( HeaderData&& other ) = default;

        /** \brief Getter for #_Timestamp.
         *
         * \retval _Timestamp as value.
         */
        uint64 timestamp() const;

        /** \brief Getter for #_Counter.
         *
         * \retval _Counter as value.
         */
        uint32 counter() const;

        /** \brief Getter for #_Format.
         *
         * \retval _Format as value.
         */
        ftkPixelFormat format() const;

        /** \brief Getter for #_Width.
         *
         * \retval _Width as value.
         */
        uint16 width() const;

        /** \brief Getter for #_Height.
         *
         * \retval _Height as value.
         */
        uint16 height() const;

        /** \brief Getter for #_PictureStride.
         *
         * \retval _PictureStride as value.
         */
        int32 pictureStride() const;

        /** \brief Getter for #_SynchronisedStrobe index 0.
         *
         * \retval _SynchronisedStrobe.at(0) as value.
         */
        bool synchronisedStrobeLeft() const;

        /** \brief Getter for #_SynchronisedStrobe index 1.
         *
         * \retval _SynchronisedStrobe.at(1) as value.
         */
        bool synchronisedStrobeRight() const;

        /** \brief Getter for #_Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

    protected:
        /** \brief Allows access from FrameData.
         */
        friend class FrameData;

        /** \brief `Promoting' constuctor.
         *
         * This constructor promotes a ::ftkImageHeader instance into a HeaderData one, from the given
         * ::ftkFrameQuery instance.
         *
         * \param[in] frame pointer on the frame containing the instance to promote.
         */
        explicit HeaderData( const ftkFrameQuery* frame );

        /** \brief Frame timestamp, given by the device clock (in \f$\si{\micro\second}\f$).
         */
        uint64 _Timestamp{ 0uLL };

        /** \brief Frame counter, given by the device.
         */
        uint32 _Counter{ 0uL };

        /** \brief Format of the pixels.
         */
        ftkPixelFormat _Format{ ftkPixelFormat::GRAY8 };

        /** \brief Picture width (in pixels).
         */
        uint16 _Width{ 0u };

        /** \brief Picture height (in pixels).
         */
        uint16 _Height{ 0u };

        /** \brief Picture stride (in bytes).
         */
        int32 _PictureStride{ 0 };

        /** \brief Was the strobing synchronised with frame acquisition.
         *
         * Indices \c 0 and \c 1 contain data for the left and pictures respectively.
         */
        std::vector< bool > _SynchronisedStrobe{ false, false };

        /** \brief Contains \c true if the instance contains valid information.
         */
        bool _Valid{ false };
    };
}
