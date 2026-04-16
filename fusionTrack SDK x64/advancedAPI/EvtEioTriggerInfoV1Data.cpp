#include <EvtEioTriggerInfoV1Data.hpp>

using namespace std;

namespace atracsys
{
    const EvtEioTriggerInfoV1Data EvtEioTriggerInfoV1Data::_InvalidInstance{};

    /**
     * The input ::EvtEioTriggerInfoV1Payload instance is simply copied (the base class is
     * ::EvtEioTriggerInfoV1Payload) to initialise the instance fields.
     */
    EvtEioTriggerInfoV1Data::EvtEioTriggerInfoV1Data( const EvtEioTriggerInfoV1Payload& other )
        : EvtEioTriggerInfoV1Payload( other )
        , _Valid( true )
    {
    }

    uint64 EvtEioTriggerInfoV1Data::triggerStartTime() const
    {
        return TriggerStartTime;
    }

    uint64 EvtEioTriggerInfoV1Data::triggerActualDurationTime() const
    {
        return TriggerActualDurationTime;
    }

    bool EvtEioTriggerInfoV1Data::triggerEnabledDuringExposure() const
    {
        return TriggerEnabledDuringExposure == 1u;
    }

    uint32 EvtEioTriggerInfoV1Data::triggerIdEio1() const
    {
        return TriggerIdEio1;
    }

    uint32 EvtEioTriggerInfoV1Data::triggerIdEio2() const
    {
        return TriggerIdEio2;
    }

    bool EvtEioTriggerInfoV1Data::valid() const
    {
        return _Valid;
    }

    const EvtEioTriggerInfoV1Data& EvtEioTriggerInfoV1Data::invalidInstance()
    {
        return _InvalidInstance;
    }
}  // namespace atracsys
