#include "FiducialData.hpp"

#include "TrackingSystem.hpp"

#include <cmath>

using namespace std;

namespace atracsys
{
    const FiducialData FiducialData::_InvalidInstance{};

    bool FiducialData::operator==( const FiducialData& other ) const
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
                  std::abs( other.position().at( 1u ) - position().at( 1u ) ) > 1.e-2f ||
                  std::abs( other.position().at( 2u ) - position().at( 2u ) ) > 1.e-2f )
        {
            return false;
        }

        return true;
    }

    const FiducialData& FiducialData::invalidInstance()
    {
        return _InvalidInstance;
    }

    uint32 FiducialData::index() const
    {
        return _Index;
    }

    const RawData& FiducialData::leftInstance() const
    {
        return _LeftInstance;
    }

    const RawData& FiducialData::rightInstance() const
    {
        return _RightInstance;
    }

    const array< float, 3u >& FiducialData::position() const
    {
        return _Position;
    }

    float FiducialData::epipolarError() const
    {
        return _EpipolarError;
    }

    float FiducialData::triangulationError() const
    {
        return _TriangulationError;
    }

    float FiducialData::probability() const
    {
        return _Probability;
    }

    ftkStatus FiducialData::status() const
    {
        return _Status;
    }

    bool FiducialData::valid() const
    {
        return _Valid;
    }

    FiducialData::FiducialData( const ftk3DFiducial& fid,
                                uint32 idx,
                                const RawData& left,
                                const RawData& right )
        : _Index( idx )
        , _LeftInstance( left )
        , _RightInstance( right )
        , _Position( { fid.positionMM.x, fid.positionMM.y, fid.positionMM.z } )
        , _EpipolarError( fid.epipolarErrorPixels )
        , _TriangulationError( fid.triangulationErrorMM )
        , _Probability( fid.probability )
        , _Valid( true )
    {
    }
}  // namespace atracsys
