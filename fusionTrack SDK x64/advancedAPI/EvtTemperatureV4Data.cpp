#include "AdditionalDataStructures.hpp"
#include <cmath>

using namespace std;

namespace atracsys
{
    // --------------------------------------------------------------------- //
    //                                                                       //
    //                        Event data definitions                         //
    //                                                                       //
    // --------------------------------------------------------------------- //

    TempV4Item::TempV4Item( const EvtTemperatureV4Payload& other )
        : EvtTemperatureV4Payload()
    {
        SensorId = other.SensorId;
        SensorValue = other.SensorValue;
    }

    uint32 TempV4Item::sensorId() const
    {
        return SensorId;
    }

    float TempV4Item::sensorValue() const
    {
        return SensorValue;
    }

    bool TempV4Item::operator==( const TempV4Item& other ) const
    {
        return SensorId == other.SensorId && std::abs( SensorValue - other.SensorValue ) < 0.05f;
    }

    const EvtTemperatureV4Data EvtTemperatureV4Data::_InvalidInstance{};

    EvtTemperatureV4Data::EvtTemperatureV4Data( const EvtTemperatureV4Payload& other, uint32 payload )
        : _Sensors()
        , _Valid( true )
    {
        size_t nSensors( payload / sizeof( EvtTemperatureV4Payload ) );
        const EvtTemperatureV4Payload* ptr( &other );
        for ( size_t i( 0u ); i < nSensors; ++i, ++ptr )
        {
            _Sensors.emplace_back( *ptr );
        }
    }

    const vector< TempV4Item >& EvtTemperatureV4Data::sensors() const
    {
        return _Sensors;
    }

    bool EvtTemperatureV4Data::valid() const
    {
        return _Valid;
    }

    const EvtTemperatureV4Data& EvtTemperatureV4Data::invalidInstance()
    {
        return _InvalidInstance;
    }
}  // namespace atracsys
