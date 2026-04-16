#include "EvtSyntheticTemperatureV1Data.hpp"

#include <cmath>

using namespace std;

namespace atracsys
{
    SyntheticTemperatureV1Item::SyntheticTemperatureV1Item( const EvtSyntheticTemperaturesV1Payload& other )
        : EvtSyntheticTemperaturesV1Payload( other )
    {
    }

    bool SyntheticTemperatureV1Item::operator==( const SyntheticTemperatureV1Item& other ) const
    {
        static const float minimalDelta( 1.f / 16.f );
        return std::abs( currentTemperature() - other.currentTemperature() ) < minimalDelta &&
               std::abs( referenceTemperature() - other.referenceTemperature() ) < minimalDelta;
    }

    float SyntheticTemperatureV1Item::currentTemperature() const
    {
        return CurrentValue;
    }

    float SyntheticTemperatureV1Item::referenceTemperature() const
    {
        return ReferenceValue;
    }

    const EvtSyntheticTemperatureV1Data EvtSyntheticTemperatureV1Data::_InvalidInstance{};

    EvtSyntheticTemperatureV1Data::EvtSyntheticTemperatureV1Data(
      const EvtSyntheticTemperaturesV1Payload& other, uint32 payload )
        : _Measurements()
        , _Valid( true )
    {
        size_t nData( payload / sizeof( EvtSyntheticTemperaturesV1Payload ) );
        const EvtSyntheticTemperaturesV1Payload* ptr( &other );
        for ( size_t i( 0u ); i < nData; ++i, ++ptr )
        {
            _Measurements.emplace_back( *ptr );
        }
    }

    const std::vector< SyntheticTemperatureV1Item > EvtSyntheticTemperatureV1Data::measurements() const
    {
        return _Measurements;
    }

    bool EvtSyntheticTemperatureV1Data::valid() const
    {
        return _Valid;
    }

    const EvtSyntheticTemperatureV1Data& EvtSyntheticTemperatureV1Data::invalidInstance()
    {
        return _InvalidInstance;
    }
}  // namespace atracsys
