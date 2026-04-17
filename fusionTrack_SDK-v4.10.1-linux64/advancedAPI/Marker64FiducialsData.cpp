#include "Marker64FiducialsData.hpp"

#include "TrackingSystem.hpp"

#include <cmath>

using namespace std;

namespace atracsys
{
    bool Marker64FiducialsData::operator==( const Marker64FiducialsData& other ) const
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

    uint32 Marker64FiducialsData::index() const
    {
        return _Index;
    }

    uint32 Marker64FiducialsData::trackingId() const
    {
        return _TrackingId;
    }

    uint32 Marker64FiducialsData::geometryId() const
    {
        return _GeometryId;
    }

    uint64 Marker64FiducialsData::presenceMask() const
    {
        return _PresenceMask;
    }

    const array< float, 3u >& Marker64FiducialsData::position() const
    {
        return _Position;
    }

    const array< array< float, 3u >, 3u >& Marker64FiducialsData::rotation() const
    {
        return _Rotation;
    }

    float Marker64FiducialsData::registrationError() const
    {
        return _RegistrationError;
    }

    ftkStatus Marker64FiducialsData::status() const
    {
        return _Status;
    }

    bool Marker64FiducialsData::valid() const
    {
        return _Valid;
    }

    const FiducialData& Marker64FiducialsData::correspondingFiducial( size_t idx ) const
    {
        if ( idx >= _FiducialInstances.size() )
        {
            return FiducialData::invalidInstance();
        }

        return _FiducialInstances.at( idx );
    }

    Marker64FiducialsData::Marker64FiducialsData( const ftkMarker64Fiducials& marker,
                                                  uint32 idx,
                                                  const array< FiducialData, FTK_MAX_FIDUCIALS_64 >& fids )
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
