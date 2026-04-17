#include <EvtEioTaggingV1Data.hpp>

using namespace std;

namespace atracsys
{
    const EvtEioTaggingV1Data EvtEioTaggingV1Data::_InvalidInstance{};

    /**
     * The input ::EvtEioTaggingV1Payload instance is simply copied (the base class is
     * ::EvtEioTaggingV1Payload) to initialise the instance fields.
     */
    EvtEioTaggingV1Data::EvtEioTaggingV1Data( const EvtEioTaggingV1Payload& other )
        : EvtEioTaggingV1Payload( other )
        , _Valid( true )
    {
    }

    bool EvtEioTaggingV1Data::tagId( const size_t portNumber, uint32_t& value ) const
    {
        if ( portNumber > 1u )
        {
            return false;
        }
        if ( TaggingInfo[ portNumber ].EioTagMode == 0u )
        {
            return false;
        }

        value = TaggingInfo[ portNumber ].EioTagId;
        return true;
    }

    bool EvtEioTaggingV1Data::tagMode( const size_t portNumber, TaggingMode& value ) const
    {
        if ( portNumber > 1u )
        {
            return false;
        }
        switch ( TaggingInfo[ portNumber ].EioTagMode )
        {
        case 0u:
            value = TaggingMode::Disabled;
            break;
        case 1u:
            value = TaggingMode::Single;
            break;
        case 2u:
            value = TaggingMode::Dual;
            break;
        default:
            return false;
        }
        return true;
    }

    bool EvtEioTaggingV1Data::timestamp( const size_t portNumber, uint64_t& value ) const
    {
        if ( portNumber > 1u )
        {
            return false;
        }
        if ( TaggingInfo[ portNumber ].EioTagMode == 0u )
        {
            return false;
        }

        value = TaggingInfo[ portNumber ].EioTagTimestamp;
        return true;
    }

    bool EvtEioTaggingV1Data::valid() const
    {
        return _Valid;
    }

    const EvtEioTaggingV1Data& EvtEioTaggingV1Data::invalidInstance()
    {
        return _InvalidInstance;
    }
}  // namespace atracsys
