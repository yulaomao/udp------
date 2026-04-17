#include <PictureData.hpp>

using namespace std;

namespace atracsys
{
    PictureData::PictureData()
        : _Width( 0u )
        , _Height( 0u )
        , _Format( ftkPixelFormat::GRAY8 )
        , _Storage()
        , _Valid( false )
    {
    }

    uint16 PictureData::width() const
    {
        return _Width;
    }

    uint16 PictureData::height() const
    {
        return _Height;
    }

    ftkPixelFormat PictureData::format() const
    {
        return _Format;
    }

    bool PictureData::valid() const
    {
        return _Valid;
    }

#ifdef ATR_MSVC

    /*
     * Disable the warning "not all control paths return a value"
     */
#pragma warning( push )
#pragma warning( disable : 4715 )
#elif defined( ATR_GCC )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#elif defined( ATR_CLANG )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
#endif

    size_t PictureData::pixelSize() const
    {
        switch ( _Format )
        {
        case ftkPixelFormat::GRAY8_SL:
        case ftkPixelFormat::GRAY8_VIS:
        case ftkPixelFormat::GRAY8:
            return 1u;
            break;
        case ftkPixelFormat::GRAY16_SL:
        case ftkPixelFormat::GRAY16_VIS:
        case ftkPixelFormat::GRAY16:
            return 2u;
        }
    }

#ifdef ATR_MSVC
#pragma warning( pop )
#elif defined( ATR_GCC )
#pragma GCC diagnostic pop
#elif defined( ATR_CLANG )
#pragma clang diagnostic pop
#endif

    PictureData::AccessStatus PictureData::getPixel8bits( uint16 u, uint16 v, uint8_t& val ) const
    {
        if ( !_Valid )
        {
            return AccessStatus::InvalidData;
        }
        else if ( u > _Width || v > _Height )
        {
            return AccessStatus::OutOfBounds;
        }
        else if ( _Format != ftkPixelFormat::GRAY8 && _Format != ftkPixelFormat::GRAY8_VIS &&
                  _Format != ftkPixelFormat::GRAY8_SL )
        {
            return AccessStatus::TypeError;
        }

        size_t index( ( u + v * _Width ) / 4u );

        switch ( ( u % 4u + v * ( _Width % 4u ) ) % 4u )
        {
        case 0u:
            val = _Storage.at( index ).OneByte.one;
            break;
        case 1u:
            val = _Storage.at( index ).OneByte.two;
            break;
        case 2u:
            val = _Storage.at( index ).OneByte.three;
            break;
        case 3u:
            val = _Storage.at( index ).OneByte.four;
            break;
        }

        return AccessStatus::Ok;
    }

    PictureData::AccessStatus PictureData::getPixel16bits( uint16 u, uint16 v, uint16_t& val ) const
    {
        if ( !_Valid )
        {
            return AccessStatus::InvalidData;
        }
        else if ( u > _Width || v > _Height )
        {
            return AccessStatus::OutOfBounds;
        }
        else if ( _Format == ftkPixelFormat::GRAY8 || _Format == ftkPixelFormat::GRAY8_VIS ||
                  _Format == ftkPixelFormat::GRAY8_SL )
        {
            return AccessStatus::TypeError;
        }

        size_t index( ( u + v * _Width ) / 2u );

        switch ( ( u % 2u + v * ( _Width % 2u ) ) % 2u )
        {
        case 0u:
            val = _Storage.at( index ).TwoBytes.one;
            break;
        case 1u:
            val = _Storage.at( index ).TwoBytes.two;
            break;
        }

        return AccessStatus::Ok;
    }

    PictureData::AccessStatus PictureData::getPixel32bits( uint16 u, uint16 v, uint32_t& val ) const
    {
        if ( !_Valid )
        {
            return AccessStatus::InvalidData;
        }
        else if ( u > _Width || v > _Height )
        {
            return AccessStatus::OutOfBounds;
        }
        else if ( _Format == ftkPixelFormat::GRAY8 || _Format == ftkPixelFormat::GRAY8_VIS ||
                  _Format == ftkPixelFormat::GRAY8_SL )
        {
            return AccessStatus::TypeError;
        }

        val = _Storage.at( v * _Width + u ).FourBytes;

        return AccessStatus::Ok;
    }

    PictureData::AccessStatus PictureData::getPixels8bits( size_t bufferSize, uint8* buffer ) const
    {
        if ( !_Valid )
        {
            return AccessStatus::InvalidData;
        }
        else if ( ( bufferSize + 3u ) / 4u < _Storage.size() )
        {
            return AccessStatus::OutOfBounds;
        }
        else if ( _Format != ftkPixelFormat::GRAY8 && _Format != ftkPixelFormat::GRAY8_VIS &&
                  _Format != ftkPixelFormat::GRAY8_SL )
        {
            return AccessStatus::TypeError;
        }

        copy( _Storage.cbegin(), _Storage.cend(), reinterpret_cast< Pixels* >( buffer ) );

        return AccessStatus::Ok;
    }

    PictureData::AccessStatus PictureData::getPixels16bits( size_t bufferSize, uint16* buffer ) const
    {
        if ( !_Valid )
        {
            return AccessStatus::InvalidData;
        }
        else if ( ( bufferSize + 1u ) / 2u < _Storage.size() )
        {
            return AccessStatus::OutOfBounds;
        }
        else if ( _Format == ftkPixelFormat::GRAY8 || _Format == ftkPixelFormat::GRAY8_VIS ||
                  _Format == ftkPixelFormat::GRAY8_SL )
        {
            return AccessStatus::TypeError;
        }

        copy( _Storage.cbegin(), _Storage.cend(), reinterpret_cast< Pixels* >( buffer ) );

        return AccessStatus::Ok;
    }

    PictureData::AccessStatus PictureData::getPixels32bits( size_t bufferSize, uint32* buffer ) const
    {
        if ( !_Valid )
        {
            return AccessStatus::InvalidData;
        }
        else if ( bufferSize < _Storage.size() )
        {
            return AccessStatus::OutOfBounds;
        }
        else if ( _Format == ftkPixelFormat::GRAY8 || _Format == ftkPixelFormat::GRAY8_VIS ||
                  _Format == ftkPixelFormat::GRAY8_SL )
        {
            return AccessStatus::TypeError;
        }

        copy( _Storage.cbegin(), _Storage.cend(), reinterpret_cast< Pixels* >( buffer ) );

        return AccessStatus::Ok;
    }

    uint8_t* PictureData::getPixels8bits()
    {
        return _StorageForPython.data();
    }

    const uint8_t* PictureData::getPixels8bits() const
    {
        return _StorageForPython.data();
    }

    PictureData::PictureData( uint16 width, uint16 height, ftkPixelFormat format, uint8* data )
        : _Width( width )
        , _Height( height )
        , _Format( format )
        , _Storage()
        , _Valid( true )
    {
        switch ( _Format )
        {
        case ftkPixelFormat::GRAY8_VIS:
        case ftkPixelFormat::GRAY8_SL:
        case ftkPixelFormat::GRAY8:
            _Storage.resize( ( width * height + 3u ) / 4u );
            _StorageForPython.resize( width * height );
            copy( data, data + ( width * height ), _StorageForPython.begin() );
            break;
        case ftkPixelFormat::GRAY16_VIS:
        case ftkPixelFormat::GRAY16_SL:
        case ftkPixelFormat::GRAY16:
            _Storage.resize( ( width * height + 3u ) / 2u );
            _StorageForPython.resize( width * height * 2 );
            copy( data, data + ( width * height * 2 ), _StorageForPython.begin() );
            break;
        }
        Pixels* inData( reinterpret_cast< Pixels* >( data ) );
        copy( inData, inData + _Storage.size(), _Storage.begin() );
    }
}  // namespace atracsys
