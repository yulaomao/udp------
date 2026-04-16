#include "EvtActiveMarkersButtonStatusesV1Data.hpp"

using namespace std;

namespace atracsys
{
    ActiveMarkersButtonStatusesV1Item::ActiveMarkersButtonStatusesV1Item(
      const EvtActiveMarkersButtonStatusesV1Payload& other )
        : EvtActiveMarkersButtonStatusesV1Payload( other )
    {
    }

    bool ActiveMarkersButtonStatusesV1Item::operator==( const ActiveMarkersButtonStatusesV1Item& other ) const
    {
        return ImageCount == other.ImageCount && DeviceID == other.DeviceID &&
               ButtonStatus == other.ButtonStatus;
    }

    uint32 ActiveMarkersButtonStatusesV1Item::imageCount() const
    {
        return ImageCount;
    }

    uint8 ActiveMarkersButtonStatusesV1Item::deviceID() const
    {
        return DeviceID;
    }

    uint8 ActiveMarkersButtonStatusesV1Item::buttonStatus() const
    {
        return ButtonStatus;
    }

    const EvtActiveMarkersButtonStatusesV1Data EvtActiveMarkersButtonStatusesV1Data::_InvalidInstance{};

    EvtActiveMarkersButtonStatusesV1Data::EvtActiveMarkersButtonStatusesV1Data(
      const EvtActiveMarkersButtonStatusesV1Payload& other, uint32 payload )
        : _Statuses()
        , _Valid( true )
    {
        size_t nMarkers( payload / sizeof( EvtActiveMarkersButtonStatusesV1Payload ) );
        const EvtActiveMarkersButtonStatusesV1Payload* ptr( &other );
        for ( size_t i( 0u ); i < nMarkers; ++i, ++ptr )
        {
            _Statuses.emplace_back( *ptr );
        }
    }

    const std::vector< ActiveMarkersButtonStatusesV1Item >&

      EvtActiveMarkersButtonStatusesV1Data::statuses() const
    {
        return _Statuses;
    }

    bool EvtActiveMarkersButtonStatusesV1Data::valid() const
    {
        return _Valid;
    }

    const EvtActiveMarkersButtonStatusesV1Data&

      EvtActiveMarkersButtonStatusesV1Data::invalidInstance()
    {
        return _InvalidInstance;
    }
}  // namespace atracsys
