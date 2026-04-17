#include "RawData.hpp"

#include "TrackingSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

using namespace std;

namespace atracsys
{
    const RawData RawData::_InvalidInstance{};

    bool RawData::operator==( const RawData& other ) const
    {
        if ( !other.valid() || !valid() )
        {
            return false;
        }
        else if ( other.index() != _Index )
        {
            return false;
        }
        else if ( std::abs( other.position().at( 0u ) - position().at( 0u ) ) > 1.e-2f ||
                  std::abs( other.position().at( 1u ) - position().at( 1u ) ) > 1.e-2f )
        {
            return false;
        }

        return true;
    }

    uint32 RawData::index() const
    {
        return _Index;
    }

    const array< float, 2u >& RawData::position() const
    {
        return _Position;
    }

    const array< uint32, 2u >& RawData::dimensions() const
    {
        return _Dimensions;
    }

    float RawData::aspectRatio() const
    {
        if ( *max_element( _Dimensions.cbegin(), _Dimensions.cend() ) == 0u )
        {
            return numeric_limits< float >::quiet_NaN();
        }
        return static_cast< float >( *min_element( _Dimensions.cbegin(), _Dimensions.cend() ) ) /
               static_cast< float >( *max_element( _Dimensions.cbegin(), _Dimensions.cend() ) );
    }

    ftkStatus RawData::status() const
    {
        return _Status;
    }

    uint32 RawData::surface() const
    {
        return _Surface;
    }

    bool RawData::valid() const
    {
        return _Valid;
    }

    const RawData& RawData::invalidInstance()
    {
        return _InvalidInstance;
    }

    RawData::RawData( const ftkRawData& raw, uint32 idx )
        : _Index( idx )
        , _Position( { raw.centerXPixels, raw.centerYPixels } )
        , _Dimensions( { raw.width, raw.height } )
        , _Status( raw.status )
        , _Surface( raw.pixelsCount )
        , _Valid( true )
    {
    }
}  // namespace atracsys
