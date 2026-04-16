#include "MarkerData.hpp"

#include "TrackingSystem.hpp"

#include <cmath>

using namespace std;

namespace atracsys
{
    bool MarkerData::operator==( const MarkerData& other ) const
    {
        if ( !other.valid() || !valid() )
        {
            return false;
        }
        else if ( other.index() != _Index )
        {
            return false;
        }
        else if ( other.trackingId() != _TrackingId )
        {
            return false;
        }
        else if ( other.geometryId() != _GeometryId )
        {
            return false;
        }
        else if ( std::abs( other.position().at( 0u ) - position().at( 0u ) ) > 1.e-2f ||
                  std::abs( other.position().at( 1u ) - position().at( 1u ) ) > 1.e-2f ||
                  std::abs( other.position().at( 2u ) - position().at( 2u ) ) > 1.e-2f )
        {
            return false;
        }

        return true;
    }

    uint32 MarkerData::index() const
    {
        return _Index;
    }

    uint32 MarkerData::trackingId() const
    {
        return _TrackingId;
    }

    uint32 MarkerData::geometryId() const
    {
        return _GeometryId;
    }

    uint32 MarkerData::presenceMask() const
    {
        return _PresenceMask;
    }

    const array< float, 3u >& MarkerData::position() const
    {
        return _Position;
    }

    const array< array< float, 3u >, 3u >& MarkerData::rotation() const
    {
        return _Rotation;
    }

    float MarkerData::registrationError() const
    {
        return _RegistrationError;
    }

    ftkStatus MarkerData::status() const
    {
        return _Status;
    }

    bool MarkerData::valid() const
    {
        return _Valid;
    }

    const FiducialData& MarkerData::correspondingFiducial( size_t idx ) const
    {
        if ( idx >= _FiducialInstances.size() )
        {
            return FiducialData::invalidInstance();
        }

        return _FiducialInstances.at( idx );
    }

    MarkerData::MarkerData( const ftkMarker& marker,
                            uint32 idx,
                            const array< FiducialData, FTK_MAX_FIDUCIALS >& fids )
        : _Index( idx )
        , _TrackingId( marker.id )
        , _GeometryId( marker.geometryId )
        , _PresenceMask( marker.geometryPresenceMask )
        , _Position( { marker.translationMM[ 0u ], marker.translationMM[ 1u ], marker.translationMM[ 2u ] } )
        , _Rotation( array< array< float, 3u >, 3u >{
            array< float, 3u >{
              marker.rotation[ 0u ][ 0u ], marker.rotation[ 0u ][ 1u ], marker.rotation[ 0u ][ 2u ] },
            array< float, 3u >{
              marker.rotation[ 1u ][ 0u ], marker.rotation[ 1u ][ 1u ], marker.rotation[ 1u ][ 2u ] },
            array< float, 3u >{
              marker.rotation[ 2u ][ 0u ], marker.rotation[ 2u ][ 1u ], marker.rotation[ 2u ][ 2u ] } } )
        , _FiducialInstances( fids )
        , _RegistrationError( marker.registrationErrorMM )
        , _Valid( true )
    {
    }
}  // namespace atracsys
