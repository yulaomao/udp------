#include "EvtActiveMarkersMaskV1Data.hpp"

using namespace std;

namespace atracsys
{
    // --------------------------------------------------------------------- //
    //                                                                       //
    //                        Event data definitions                         //
    //                                                                       //
    // --------------------------------------------------------------------- //

    const EvtActiveMarkersMaskV1Data EvtActiveMarkersMaskV1Data::_InvalidInstance{};

    EvtActiveMarkersMaskV1Data::EvtActiveMarkersMaskV1Data( EvtActiveMarkersMaskV1Payload other )
        : EvtActiveMarkersMaskV1Payload( other )
        , _Valid( true )
    {
    }

    uint16 EvtActiveMarkersMaskV1Data::activeMarkersMask() const
    {
        return ActiveMarkersMask;
    }

    bool EvtActiveMarkersMaskV1Data::isMarkerPaired( uint32 idx ) const
    {
        if ( idx >= 16u )
        {
            return false;
        }

        return ( ActiveMarkersMask & ( 1 << idx ) ) != 0u;
    }

    bool EvtActiveMarkersMaskV1Data::isMarkerPaired( const uint32 idx, bool& isPaired ) const
    {
        if ( idx >= 16u )
        {
            return false;
        }

        isPaired = ( ActiveMarkersMask & ( 1 << idx ) ) != 0u;

        return true;
    }

    bool EvtActiveMarkersMaskV1Data::valid() const
    {
        return _Valid;
    }

    const EvtActiveMarkersMaskV1Data& EvtActiveMarkersMaskV1Data::invalidInstance()
    {
        return _InvalidInstance;
    }

    const EvtActiveMarkersMaskV2Data EvtActiveMarkersMaskV2Data::_InvalidInstance{};

    EvtActiveMarkersMaskV2Data::EvtActiveMarkersMaskV2Data( EvtActiveMarkersMaskV2Payload other )
        : EvtActiveMarkersMaskV2Payload( other )
        , _Valid( true )
    {
    }

    uint16 EvtActiveMarkersMaskV2Data::activeMarkersMask() const
    {
        return ActiveMarkersMask;
    }

    uint16 EvtActiveMarkersMaskV2Data::activeMarkersErrorMask() const
    {
        return ActiveMarkersErrorMask;
    }

    bool EvtActiveMarkersMaskV2Data::isMarkerPaired( const uint32 idx, bool& isPaired ) const
    {
        if ( idx >= 16u )
        {
            return false;
        }

        isPaired = ( ActiveMarkersMask & ( 1 << idx ) ) != 0u;

        return true;
    }

    bool EvtActiveMarkersMaskV2Data::isMarkerInError( const uint32 idx, bool& isInError ) const
    {
        bool isPaired{ false };
        if ( !isMarkerPaired( idx, isPaired ) || !isPaired )
        {
            return false;
        }
        else
        {
            isInError = ( ActiveMarkersErrorMask & ( 1 << idx ) ) != 0u;
        }

        return true;
    }

    bool EvtActiveMarkersMaskV2Data::valid() const
    {
        return _Valid;
    }

    const EvtActiveMarkersMaskV2Data& EvtActiveMarkersMaskV2Data::invalidInstance()
    {
        return _InvalidInstance;
    }
}  // namespace atracsys
