#include <EventData.hpp>

using namespace std;

namespace atracsys
{
    EventData::EventData()
        : _Type( FtkEventType::fetLastEvent )
        , _Payload( 0u )
        , _FansV1()
        , _TemperatureV4()
        , _ActiveMarkerMaskV1()
        , _ActiveMarkersButtonStatusesV1()
        , _ActiveMarkerBatteryStateV1()
        , _Valid( false )
    {
    }

    bool EventData::operator==( const EventData& other ) const
    {
        if ( !other.valid() || !valid() )
        {
            return false;
        }
        else if ( other.type() != _Type )
        {
            return false;
        }
        else if ( other.payload() != _Payload )
        {
            return false;
        }

        return true;
    }

    FtkEventType EventData::type() const
    {
        return _Type;
    }

    uint32 EventData::payload() const
    {
        return _Payload;
    }

    bool EventData::valid() const
    {
        return _Valid;
    }

    const EvtFansV1Data& EventData::fansV1() const
    {
        if ( _FansV1 != nullptr )
        {
            return *_FansV1;
        }
        return EvtFansV1Data::invalidInstance();
    }

    const EvtTemperatureV4Data& EventData::temperatureV4() const
    {
        if ( _TemperatureV4 != nullptr )
        {
            return *_TemperatureV4;
        }
        return EvtTemperatureV4Data::invalidInstance();
    }

    const EvtActiveMarkersMaskV1Data& EventData::activeMarkerMaskV1() const
    {
        if ( _ActiveMarkerMaskV1 != nullptr )
        {
            return *_ActiveMarkerMaskV1;
        }
        return EvtActiveMarkersMaskV1Data::invalidInstance();
    }

    const EvtActiveMarkersMaskV2Data& EventData::activeMarkerMaskV2() const
    {
        if ( _ActiveMarkerMaskV2 != nullptr )
        {
            return *_ActiveMarkerMaskV2;
        }
        return EvtActiveMarkersMaskV2Data::invalidInstance();
    }

    const EvtActiveMarkersButtonStatusesV1Data& EventData::

      activeMarkersButtonStatusesV1() const
    {
        if ( _ActiveMarkersButtonStatusesV1 != nullptr )
        {
            return *_ActiveMarkersButtonStatusesV1;
        }
        return EvtActiveMarkersButtonStatusesV1Data::invalidInstance();
    }

    const EvtActiveMarkersBatteryStateV1Data& EventData::

      activeMarkerBatteryStateV1() const
    {
        if ( _ActiveMarkerBatteryStateV1 != nullptr )
        {
            return *_ActiveMarkerBatteryStateV1;
        }
        return EvtActiveMarkersBatteryStateV1Data::invalidInstance();
        ;
    }

    const EvtSyntheticTemperatureV1Data& EventData::syntheticTemperatureV1() const
    {
        if ( _SyntheticTemperatureV1 != nullptr )
        {
            return *_SyntheticTemperatureV1;
        }
        return EvtSyntheticTemperatureV1Data::invalidInstance();
    }

    const EvtSynchronisationPTPV1Data& EventData::synchronisationPTPV1() const
    {
        if ( _SynchronisationPTPV1 != nullptr )
        {
            return *_SynchronisationPTPV1;
        }
        return EvtSynchronisationPTPV1Data::invalidInstance();
    }

    const EvtEioTaggingV1Data& EventData::eioTaggingV1Data() const
    {
        if ( _EioTaggingV1Data != nullptr )
        {
            return *_EioTaggingV1Data;
        }

        return EvtEioTaggingV1Data::invalidInstance();
    }

    const EvtEioTriggerInfoV1Data& EventData::eioTriggerInfoV1Data() const
    {
        if ( _EioTriggerInfoV1Data != nullptr )
        {
            return *_EioTriggerInfoV1Data;
        }

        return EvtEioTriggerInfoV1Data::invalidInstance();
    }

    EventData::EventData( const ftkEvent& evt )
        : _Type( evt.Type )
        , _Payload( evt.Payload )
        , _Valid( true )
    {
        switch ( _Type )
        {
        case FtkEventType::fetFansV1:
            _FansV1 = make_shared< EvtFansV1Data >( *reinterpret_cast< EvtFansV1Payload* >( evt.Data ) );
            break;
        case FtkEventType::fetTempV4:
            _TemperatureV4 = make_shared< EvtTemperatureV4Data >(
              *reinterpret_cast< EvtTemperatureV4Payload* >( evt.Data ), _Payload );
            break;
        case FtkEventType::fetActiveMarkersMaskV1:
            _ActiveMarkerMaskV1 = make_shared< EvtActiveMarkersMaskV1Data >(
              *reinterpret_cast< EvtActiveMarkersMaskV1Payload* >( evt.Data ) );
            break;
        case FtkEventType::fetActiveMarkersMaskV2:
            _ActiveMarkerMaskV2 = make_shared< EvtActiveMarkersMaskV2Data >(
              *reinterpret_cast< EvtActiveMarkersMaskV2Payload* >( evt.Data ) );
            break;
        case FtkEventType::fetActiveMarkersButtonStatusV1:
            _ActiveMarkersButtonStatusesV1 = make_shared< EvtActiveMarkersButtonStatusesV1Data >(
              *reinterpret_cast< EvtActiveMarkersButtonStatusesV1Payload* >( evt.Data ), _Payload );
            break;
        case FtkEventType::fetActiveMarkersBatteryStateV1:
            _ActiveMarkerBatteryStateV1 = make_shared< EvtActiveMarkersBatteryStateV1Data >(
              *reinterpret_cast< EvtActiveMarkersBatteryStateV1Payload* >( evt.Data ), _Payload );
            break;
        case FtkEventType::fetSyntheticTemperaturesV1:
            _SyntheticTemperatureV1 = make_shared< EvtSyntheticTemperatureV1Data >(
              *reinterpret_cast< EvtSyntheticTemperaturesV1Payload* >( evt.Data ), _Payload );
            break;
        case FtkEventType::fetSynchronisationPTPV1:
            _SynchronisationPTPV1 = make_shared< EvtSynchronisationPTPV1Data >(
              *reinterpret_cast< EvtSynchronisationPTPV1Payload* >( evt.Data ) );
            break;
        case FtkEventType::fetEioTaggingV1:
            _EioTaggingV1Data =
              make_shared< EvtEioTaggingV1Data >( *reinterpret_cast< EvtEioTaggingV1Payload* >( evt.Data ) );
            break;
        case FtkEventType::fetTriggerInfoV1:
            _EioTriggerInfoV1Data = make_shared< EvtEioTriggerInfoV1Data >(
              *reinterpret_cast< EvtEioTriggerInfoV1Payload* >( evt.Data ) );
        case FtkEventType::fetLowTemp:
        case FtkEventType::fetHighTemp:
            break;
        default:
            _Type = FtkEventType::fetLastEvent;
            _Valid = false;
        }
    }
}  // namespace atracsys
