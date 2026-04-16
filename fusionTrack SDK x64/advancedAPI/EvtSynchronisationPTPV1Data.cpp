#include "EvtSynchronisationPTPV1Data.hpp"

using namespace std;

namespace atracsys
{
    const EvtSynchronisationPTPV1Data EvtSynchronisationPTPV1Data::_InvalidInstance{};

    EvtSynchronisationPTPV1Data::EvtSynchronisationPTPV1Data( const EvtSynchronisationPTPV1Payload& other )
        : EvtSynchronisationPTPV1Payload( other )
        , _Valid( true )
    {
    }

    const ftkTimestampPTP& EvtSynchronisationPTPV1Data::timestamp() const
    {
        return Timestamp;
    }

    const ftkTimestampCorrectionPTP& EvtSynchronisationPTPV1Data::lastCorrection() const
    {
        return LastCorrection;
    }

    const ftkParentId& EvtSynchronisationPTPV1Data::parentId() const
    {
        return ParentId;
    }

    ftkPortStatePTP EvtSynchronisationPTPV1Data::status() const
    {
        switch ( EvtSynchronisationPTPV1Payload::Status.PortStateId )
        {
        case 1u:
            return ftkPortStatePTP::Initialising;
        case 2u:
            return ftkPortStatePTP::Faulty;
        case 4u:
            return ftkPortStatePTP::Listening;
        case 8u:
            return ftkPortStatePTP::Uncalibrate;
        case 9u:
            return ftkPortStatePTP::Slave;
        default:
            break;
        }

        return ftkPortStatePTP::UnknownState;
    }

    ftkErrorPTP EvtSynchronisationPTPV1Data::errorId() const
    {
        switch ( EvtSynchronisationPTPV1Payload::Status.ErrorId )
        {
        case 0u:
            return ftkErrorPTP::Ok;
        case 1u:
            return ftkErrorPTP::NoMasterDetected;
        case 2u:
            return ftkErrorPTP::NetworkTimeoutError;
        case 3u:
            return ftkErrorPTP::AnnounceWrongIntervalError;
        case 4u:
            return ftkErrorPTP::SyncWrongInternalError;
        case 5u:
            return ftkErrorPTP::TooMuchMastersError;
        case 20u:
            return ftkErrorPTP::SyncMessageTimeout;
        case 21u:
            return ftkErrorPTP::NoSyncFromMaster;
        case 22u:
            return ftkErrorPTP::SyncMessageError;
        case 23u:
            return ftkErrorPTP::NoFollowUp;
        case 24u:
            return ftkErrorPTP::FollowUpSyntaxError;
        case 25u:
            return ftkErrorPTP::NoDelayAnswer;
        case 26u:
            return ftkErrorPTP::DelayAnswerSyntaxError;
        case 27u:
            return ftkErrorPTP::UnstableSynchronisationError;

        case 41u:
            return ftkErrorPTP::WrongDomain;
        case 42u:
            return ftkErrorPTP::UnsupportedVersion;
        case 43u:
            return ftkErrorPTP::UnsupportedFlags;
        default:
            break;
        }
        return ftkErrorPTP::UnknownError;
    }

    bool EvtSynchronisationPTPV1Data::valid() const
    {
        return _Valid;
    }

    const EvtSynchronisationPTPV1Data& EvtSynchronisationPTPV1Data::invalidInstance()
    {
        return _InvalidInstance;
    }
}  // namespace atracsys
