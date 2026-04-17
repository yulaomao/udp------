#include <FrameData.hpp>

#include "TrackingSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace std;

namespace atracsys
{
    FrameData::FrameData( ftkFrameQuery* frame )
        : _Header()
        , _LeftRaws()
        , _RightRaws()
        , _Fiducials()
        , _Markers()
        , _Events()
    {
        _buildRelations( frame );
    }

    FrameData& FrameData::operator=( ftkFrameQuery* frame )
    {
        _LeftRaws.clear();
        _RightRaws.clear();
        _Fiducials.clear();
        _Markers.clear();
        _Events.clear();

        if ( nullptr != frame )
        {
            if ( frame->imageHeaderStat == ftkQueryStatus::QS_OK )
            {
                _Header = move( HeaderData( frame ) );
            }
            else
            {
                _Header = move( HeaderData() );
            }

            if ( frame->rawDataLeft != nullptr || frame->rawDataRight != nullptr ||
                 frame->threeDFiducials != nullptr || frame->markers != nullptr || frame->events != nullptr )
            {
                _buildRelations( frame );
            }
        }
        return *this;
    }

    const HeaderData& FrameData::header() const
    {
        return _Header;
    }

    const vector< RawData >& FrameData::leftRaws() const
    {
        return _LeftRaws;
    }

    const vector< RawData >& FrameData::rightRaws() const
    {
        return _RightRaws;
    }

    const vector< FiducialData >& FrameData::fiducials() const
    {
        return _Fiducials;
    }

    const vector< MarkerData >& FrameData::markers() const
    {
        return _Markers;
    }

    const vector< Marker64FiducialsData >& FrameData::sixtyFourFiducialsMarkers() const
    {
        return _SixtyFourFiducialsMarkers;
    }

    const vector< EventData >& FrameData::events() const
    {
        return _Events;
    }

    const PictureData& FrameData::leftPicture() const
    {
        return _LeftPicture;
    }

    const PictureData& FrameData::rightPicture() const
    {
        return _RightPicture;
    }

    void FrameData::_buildRelations( ftkFrameQuery* frame )
    {
        uint32 idx( 0u );

        if ( frame->rawDataLeft != nullptr && ( frame->rawDataLeftStat == ftkQueryStatus::QS_OK ||
                                                frame->rawDataLeftStat == ftkQueryStatus::QS_ERR_OVERFLOW ) )
        {
            for ( auto rawIt( frame->rawDataLeft ); rawIt != frame->rawDataLeft + frame->rawDataLeftCount;
                  ++rawIt )
            {
                RawData tmp( *rawIt, idx++ );
                _LeftRaws.emplace_back( move( tmp ) );
            }
        }

        if ( frame->rawDataRight != nullptr &&
             ( frame->rawDataRightStat == ftkQueryStatus::QS_OK ||
               frame->rawDataRightStat == ftkQueryStatus::QS_ERR_OVERFLOW ) )
        {
            idx = 0u;
            for ( auto rawIt( frame->rawDataRight ); rawIt != frame->rawDataRight + frame->rawDataRightCount;
                  ++rawIt )
            {
                RawData tmp( *rawIt, idx++ );
                _RightRaws.emplace_back( move( tmp ) );
            }
        }

        if ( frame->threeDFiducials != nullptr &&
             ( frame->threeDFiducialsStat == ftkQueryStatus::QS_OK ||
               frame->threeDFiducialsStat == ftkQueryStatus::QS_ERR_OVERFLOW ) )
        {
            idx = 0u;
            for ( auto fidIt( frame->threeDFiducials );
                  fidIt != frame->threeDFiducials + frame->threeDFiducialsCount;
                  ++fidIt, ++idx )
            {
                FiducialData tmp(
                  *fidIt,
                  idx,
                  fidIt->leftIndex >= frame->rawDataLeftCount ? RawData::invalidInstance()
                                                              : _LeftRaws.at( fidIt->leftIndex ),
                  fidIt->rightIndex >= frame->rawDataRightCount ? RawData::invalidInstance()
                                                                : _RightRaws.at( fidIt->rightIndex ) );
                _Fiducials.emplace_back( move( tmp ) );
            }
        }

        if ( frame->markers != nullptr && ( frame->markersStat == ftkQueryStatus::QS_OK ||
                                            frame->markersStat == ftkQueryStatus::QS_ERR_OVERFLOW ) )
        {
            idx = 0u;
            for ( auto markerIt( frame->markers ); markerIt != frame->markers + frame->markersCount;
                  ++markerIt )
            {
                array< FiducialData, FTK_MAX_FIDUCIALS > fids = {
                    markerIt->fiducialCorresp[ 0u ] >= frame->threeDFiducialsCount
                      ? FiducialData::invalidInstance()
                      : _Fiducials.at( markerIt->fiducialCorresp[ 0u ] ),
                    markerIt->fiducialCorresp[ 1u ] >= frame->threeDFiducialsCount
                      ? FiducialData::invalidInstance()
                      : _Fiducials.at( markerIt->fiducialCorresp[ 1u ] ),
                    markerIt->fiducialCorresp[ 2u ] >= frame->threeDFiducialsCount
                      ? FiducialData::invalidInstance()
                      : _Fiducials.at( markerIt->fiducialCorresp[ 2u ] ),
                    markerIt->fiducialCorresp[ 3u ] >= frame->threeDFiducialsCount
                      ? FiducialData::invalidInstance()
                      : _Fiducials.at( markerIt->fiducialCorresp[ 3u ] ),
                    markerIt->fiducialCorresp[ 4u ] >= frame->threeDFiducialsCount
                      ? FiducialData::invalidInstance()
                      : _Fiducials.at( markerIt->fiducialCorresp[ 4u ] ),
                    markerIt->fiducialCorresp[ 5u ] >= frame->threeDFiducialsCount
                      ? FiducialData::invalidInstance()
                      : _Fiducials.at( markerIt->fiducialCorresp[ 5u ] ),
                };
                MarkerData tmp( *markerIt, idx++, fids );
                _Markers.emplace_back( move( tmp ) );
            }
        }

        if ( frame->sixtyFourFiducialsMarkers != nullptr &&
             ( frame->sixtyFourFiducialsMarkersStat == ftkQueryStatus::QS_OK ||
               frame->sixtyFourFiducialsMarkersStat == ftkQueryStatus::QS_ERR_OVERFLOW ) )
        {
            idx = 0u;
            for ( auto markerIt( frame->sixtyFourFiducialsMarkers );
                  markerIt != frame->sixtyFourFiducialsMarkers + frame->sixtyFourFiducialsMarkersCount;
                  ++markerIt )
            {
                array< FiducialData, FTK_MAX_FIDUCIALS_64 > fids{};
                transform( begin( markerIt->fiducialCorresp ),
                           end( markerIt->fiducialCorresp ),
                           fids.begin(),
                           [ this, frame ]( const uint32 fidId )
                           {
                               return fidId >= frame->threeDFiducialsCount ? FiducialData::invalidInstance()
                                                                           : _Fiducials.at( fidId );
                           } );
                Marker64FiducialsData tmp( *markerIt, idx++, fids );
                _SixtyFourFiducialsMarkers.emplace_back( move( tmp ) );
            }
        }

        if ( frame->events != nullptr && ( frame->eventsStat == ftkQueryStatus::QS_OK ||
                                           frame->eventsStat == ftkQueryStatus::QS_ERR_OVERFLOW ) )
        {
            for ( auto eventIt( frame->events ); eventIt != frame->events + frame->eventsCount; ++eventIt )
            {
                if ( *eventIt == nullptr )
                {
                    continue;
                }
                EventData tmp( **eventIt );
                if ( tmp.valid() )
                {
                    _Events.emplace_back( move( tmp ) );
                }
            }
        }

        if ( frame->imageLeftPixels != nullptr && frame->imageLeftStat == ftkQueryStatus::QS_OK &&
             _Header.valid() )
        {
            _LeftPicture = move(
              PictureData( _Header.width(), _Header.height(), _Header.format(), frame->imageLeftPixels ) );
        }
        else
        {
            _LeftPicture = move( PictureData() );
        }

        if ( frame->imageRightPixels != nullptr && frame->imageRightStat == ftkQueryStatus::QS_OK &&
             _Header.valid() )
        {
            _RightPicture = move(
              PictureData( _Header.width(), _Header.height(), _Header.format(), frame->imageRightPixels ) );
        }
        else
        {
            _RightPicture = move( PictureData() );
        }
    }
}  // namespace atracsys
