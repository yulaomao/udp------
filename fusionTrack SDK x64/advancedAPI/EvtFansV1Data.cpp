#include "EvtFansV1Data.hpp"

using namespace std;

namespace atracsys
{
    FanStateData::FanStateData( const ftkFanState& other )
        : ftkFanState()
    {
        PwmDuty = other.PwmDuty;
        Speed = other.Speed;
    }

    bool FanStateData::operator==( const FanStateData& other ) const
    {
        return PwmDuty == other.PwmDuty && Speed == other.Speed;
    }

    uint8 FanStateData::pwmDuty() const
    {
        return PwmDuty;
    }

    uint16 FanStateData::speed() const
    {
        return Speed;
    }

    const EvtFansV1Data EvtFansV1Data::_InvalidInstance{};

    EvtFansV1Data::EvtFansV1Data( const EvtFansV1Payload& other )
        : EvtFansV1Payload()
        , _Fans()
        , _Valid( true )
    {
        FansStatus = other.FansStatus;

        _Fans.assign( other.Fans, other.Fans + 2u );
    }

    ftkFanStatus EvtFansV1Data::fansStatus() const
    {
        return FansStatus;
    }

    const vector< FanStateData >& EvtFansV1Data::fans() const
    {
        return _Fans;
    }

    bool EvtFansV1Data::valid() const
    {
        return _Valid;
    }

    const EvtFansV1Data& EvtFansV1Data::invalidInstance()
    {
        return _InvalidInstance;
    }
}  // namespace atracsys
