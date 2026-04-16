#include "HeaderData.hpp"

#include "TrackingSystem.hpp"

using namespace std;

namespace atracsys
{
    // --------------------------------------------------------------------- //
    //                                                                       //
    //                        Header data definitions                        //
    //                                                                       //
    // --------------------------------------------------------------------- //

    uint64 HeaderData::timestamp() const
    {
        return _Timestamp;
    }

    uint32 HeaderData::counter() const
    {
        return _Counter;
    }

    ftkPixelFormat HeaderData::format() const
    {
        return _Format;
    }

    uint16 HeaderData::width() const
    {
        return _Width;
    }

    uint16 HeaderData::height() const
    {
        return _Height;
    }

    int32 HeaderData::pictureStride() const
    {
        return _PictureStride;
    }

    bool HeaderData::synchronisedStrobeLeft() const
    {
        return _SynchronisedStrobe.at( 0u );
    }

    bool HeaderData::synchronisedStrobeRight() const
    {
        return _SynchronisedStrobe.at( 1u );
    }

    bool HeaderData::valid() const
    {
        return _Valid;
    }

    HeaderData::HeaderData( const ftkFrameQuery* frame )
        : _Timestamp( frame->imageHeader->timestampUS )
        , _Counter( frame->imageHeader->counter )
        , _Format( frame->imageHeader->format )
        , _Width( frame->imageHeader->width )
        , _Height( frame->imageHeader->height )
        , _PictureStride( frame->imageHeader->imageStrideInBytes )
        , _SynchronisedStrobe( 2u, false )
        , _Valid( false )
    {
        ftkFrameInfoData tmp{};
        tmp.WantedInformation = ftkInformationType::StrobeEnabled;
        ftkError err( ftkExtractFrameInfo( frame, &tmp ) );
        if ( err == ftkError::FTK_OK )
        {
            if ( tmp.StrobeInfo.LeftStrobeEnabled == 1u )
            {
                _SynchronisedStrobe.at( 0u ) = true;
            }
            if ( tmp.StrobeInfo.RightStrobeEnabled == 1u )
            {
                _SynchronisedStrobe.at( 1u ) = true;
            }
            _Valid = true;
        }
    }
}  // namespace atracsys