#include "EvtActiveMarkersBatteryStateV1Data.hpp"

using namespace std;

namespace atracsys
{
    ActiveMarkersBatteryStateV1Item::ActiveMarkersBatteryStateV1Item(
      const EvtActiveMarkersBatteryStateV1Payload& other )
        : EvtActiveMarkersBatteryStateV1Payload( other )
    {
    }

    bool ActiveMarkersBatteryStateV1Item::operator==( const ActiveMarkersBatteryStateV1Item& other ) const
    {
        return ImageCount == other.ImageCount && DeviceID == other.DeviceID &&
               BatteryState == other.BatteryState;
    }

    uint32 ActiveMarkersBatteryStateV1Item::imageCount() const
    {
        return ImageCount;
    }

    uint8 ActiveMarkersBatteryStateV1Item::deviceID() const
    {
        return DeviceID;
    }

    uint8 ActiveMarkersBatteryStateV1Item::batteryState() const
    {
        return BatteryState;
    }

    const EvtActiveMarkersBatteryStateV1Data EvtActiveMarkersBatteryStateV1Data ::_InvalidInstance{};

    EvtActiveMarkersBatteryStateV1Data::EvtActiveMarkersBatteryStateV1Data(
      const EvtActiveMarkersBatteryStateV1Payload& other, uint32 payload )
        : _States()
        , _Valid( true )
    {
        size_t nMarkers( payload / sizeof( EvtActiveMarkersBatteryStateV1Payload ) );
        const EvtActiveMarkersBatteryStateV1Payload* ptr( &other );
        for ( size_t i( 0u ); i < nMarkers; ++i, ++ptr )
        {
            _States.emplace_back( *ptr );
        }
    }

    const std::vector< ActiveMarkersBatteryStateV1Item >& EvtActiveMarkersBatteryStateV1Data::states() const
    {
        return _States;
    }

    bool EvtActiveMarkersBatteryStateV1Data::valid() const
    {
        return _Valid;
    }

    const EvtActiveMarkersBatteryStateV1Data& EvtActiveMarkersBatteryStateV1Data

      ::invalidInstance()
    {
        return _InvalidInstance;
    }
}  // namespace atracsys
