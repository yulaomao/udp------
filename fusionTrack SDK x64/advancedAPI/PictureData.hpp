/** \file PictureData.hpp
 * \brief File defining the picture data structures of the C++ API.
 *
 * This file defines a C++ API which manipulates the ::ftkFrameQuery::imageLeftPixel or
 * ::ftkFrameQuery::imageRightPixel and picture sizes.
 */
#pragma once

#include <ftkInterface.h>

#include <vector>

/**
 * \addtogroup adApi
 * \{
 */

namespace atracsys
{
    class FrameData;

    /** \brief Class containing the pixels data.
     *
     * This class contains all information to obtain the picture from the cameras. The instance does now if
     * it has been properly set (i.e. whether its member values make sense).
     */
    class PictureData
    {
    public:
        /** \brief Default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        PictureData();

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        PictureData( const PictureData& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        PictureData( PictureData&& other ) = default;

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~PictureData() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        PictureData& operator=( const PictureData& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        PictureData& operator=( PictureData&& other ) = default;

        /** \brief Status code for single pixel access.
         *
         * Those are the possible returned values for the pixel accessing
         * methods.
         */
        enum class AccessStatus
        {
            /** \brief No errors.
             */
            Ok,

            /** \brief Instance is invalid.
             */
            InvalidData,

            /** \brief Unmatching pixel type.
             */
            TypeError,

            /** \brief Pixel coordinate outside picture.
             */
            OutOfBounds
        };

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

        /** \brief Getter for #_Format.
         *
         * \retval _Format as value.
         */
        ftkPixelFormat format() const;

        /** \brief Getter for #_Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Getter for the pixel size.
         *
         * This method returns the size in bytes of one pixel.
         *
         * \retval 1u for \c uint8 pixels,
         * \retval 2u for \c uint16 pixels,
         * \retval 4u for \c uint32 pixels.
         */
        size_t pixelSize() const;

        /** \brief Getter for the value of a pixel.
         *
         * This method tries to read the given pixel value.
         *
         * \param[in] u coordinate along the horizontal axis.
         * \param[in] v coordinate along the vertical axis.
         * \param[out] val value of the pixel.
         *
         * \retval AccessStatus::Ok if the reading could be done successfully,
         * \retval AccessStatus::InvalidData if the instance is not valid,
         * \retval AccessStatus::TypeError if the stored pixels are not 8-bits values,
         * \retval AccessStatus::OutOfBounds if the given coordinate does not belong to the picture.
         */
        AccessStatus getPixel8bits( uint16 u, uint16 v, uint8& val ) const;

        /** \brief Getter for the value of a pixel.
         *
         * This method tries to read the given pixel value.
         *
         * \param[in] u coordinate along the horizontal axis.
         * \param[in] v coordinate along the vertical axis.
         * \param[out] val value of the pixel.
         *
         * \retval AccessStatus::Ok if the reading could be done successfully,
         * \retval AccessStatus::InvalidData if the instance is not valid,
         * \retval AccessStatus::TypeError if the stored pixels are not 16-bits values,
         * \retval AccessStatus::OutOfBounds if the given coordinate does not belong to the picture.
         */
        AccessStatus getPixel16bits( uint16 u, uint16 v, uint16& val ) const;

        /** \brief Getter for the value of a pixel.
         *
         * This method tries to read the given pixel value.
         *
         * \param[in] u coordinate along the horizontal axis.
         * \param[in] v coordinate along the vertical axis.
         * \param[out] val value of the pixel.
         *
         * \retval AccessStatus::Ok if the reading could be done successfully,
         * \retval AccessStatus::InvalidData if the instance is not valid,
         * \retval AccessStatus::TypeError if the stored pixels are not 32-bits values,
         * \retval AccessStatus::OutOfBounds if the given coordinate does not belong to the picture.
         */
        AccessStatus getPixel32bits( uint16 u, uint16 v, uint32& val ) const;

        /** \brief Getter for all pixels in a buffer.
         *
         * This method tries copy the pixels in an \e allocated buffer.
         *
         * \param[in] bufferSize number of pixels.
         * \param[out] buffer pixel buffer, which size is \c bufferSize.
         *
         * \retval AccessStatus::Ok if the reading could be done successfully,
         * \retval AccessStatus::InvalidData if the instance is not valid,
         * \retval AccessStatus::TypeError if the stored pixels are not 8-bits values,
         * \retval AccessStatus::OutOfBounds if the size of \c buffer is too small.
         */
        AccessStatus getPixels8bits( size_t bufferSize, uint8* buffer ) const;

        /** \brief Getter for all pixels in a buffer.
         *
         * This method tries copy the pixels in an \e allocated buffer.
         *
         * \param[in] bufferSize number of pixels.
         * \param[out] buffer pixel buffer, which size is \c bufferSize.
         *
         * \retval AccessStatus::Ok if the reading could be done successfully,
         * \retval AccessStatus::InvalidData if the instance is not valid,
         * \retval AccessStatus::TypeError if the stored pixels are not 8-bits values,
         * \retval AccessStatus::OutOfBounds if the size of \c buffer is too small.
         */
        AccessStatus getPixels16bits( size_t bufferSize, uint16* buffer ) const;

        /** \brief Getter for all pixels in a buffer.
         *
         * This method tries copy the pixels in an \e allocated buffer.
         *
         * \param[in] bufferSize number of pixels.
         * \param[out] buffer pixel buffer, which size is \c bufferSize.
         *
         * \retval AccessStatus::Ok if the reading could be done successfully,
         * \retval AccessStatus::InvalidData if the instance is not valid,
         * \retval AccessStatus::TypeError if the stored pixels are not 8-bits values,
         * \retval AccessStatus::OutOfBounds if the size of \c buffer is too small.
         */
        AccessStatus getPixels32bits( size_t bufferSize, uint32* buffer ) const;

        /** \brief Getter for the internal 8 bits storage.
         *
         * This method is meant to be used with the Python wrapper. It cannot be \c const to be compatible
         * with pybind11.
         */
        uint8* getPixels8bits();

        /** \brief Getter for the internal 8 bits storage.
         *
         * This method is meant to be used with the Python wrapper. It cannot be \c const to be compatible
         * with pybind11.
         */
        const uint8* getPixels8bits() const;

    protected:
        /** \brief Allows access from FrameData.
         */
        friend class FrameData;

        /** \brief `Promoting' constuctor.
         *
         * This constructor promotes the pixel data read from a
         * ::ftkFrameQuery instance into a PictureData one.
         *
         * \param[in] width picture width.
         * \param[in] height picture height.
         * \param[in] format used pixel type.
         * \param[in] data pointer on the pixel data.
         */
        PictureData( uint16 width, uint16 height, ftkPixelFormat format, uint8* data );

        /** \brief Picture width in pixels.
         */
        uint16 _Width;

        /** \brief Picture height in pixels.
         */
        uint16 _Height;

        /** \brief Pixel format, indicating how many bytes are used for a pixel.
         */
        ftkPixelFormat _Format;

        /** \brief Helper structure, allowing to pack 4 1-byte pixels on 32 bits.
         */
        PACK1_STRUCT_BEGIN( Four8bits )
        {
            /** \brief First pixel in the group of four.
             */
            uint32 one : 8;
            /** \brief Second pixel in the group of four.
             */
            uint32 two : 8;
            /** \brief Third pixel in the group of four.
             */
            uint32 three : 8;
            /** \brief Fourth pixel in the group of four.
             */
            uint32 four : 8;
        }
        PACK1_STRUCT_END( Four8bits );

        /** \brief Helper structure, allowing to pack 2 2-bytes pixels on 32 bits.
         */
        PACK1_STRUCT_BEGIN( Two16bits )
        {
            /** \brief First pixel in the group of two.
             */
            uint32 one : 16;
            /** \brief Second pixel in the group of two.
             */
            uint32 two : 16;
        }
        PACK1_STRUCT_END( Two16bits );

        /** \brief Generic representation of pixels.
         */
        union Pixels
        {
            /** \brief Group of 4 \f$ \SI{1}{\byte} \f$ pixels.
             */
            Four8bits OneByte;
            /** \brief Group of 2 \f$ \SI{2}{\byte} \f$ pixels.
             */
            Two16bits TwoBytes;
            /** \brief \f$ \SI{4}{\byte} \f$ pixel.
             */
            uint32 FourBytes;
        };

        /** \brief Storage for the pixels.
         */
        std::vector< Pixels > _Storage;

        /** \brief Temporary storage used only for python bindings. Memory is cheap.
         */
        std::vector< uint8 > _StorageForPython;

        /** \brief Contains \c true if the instance contains valid information.
         */
        bool _Valid;
    };
}  // namespace atracsys
