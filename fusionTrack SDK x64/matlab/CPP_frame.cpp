#include "CPP_frame.hpp"

#include "CPP_helpers.hpp"

#include <string>

#include <matrix.h>
#include <mex.h>

const char* acFrame[] = { "threeDFiducials", "markers", "imageHeader", "statuses", "events" };

const char* acTDF[] = { "positionMM", "epipolarErrorPixels", "triangulationErrorMM" };

const char* acMarker[] = { "trackingId", "geometryId",    "geometryPresenceMask", "fiducialCorresp",
                           "rotation",   "translationMM", "registrationErrorMM" };

const char* acHeader[] = { "timestamp", "counter", "height", "width" };

const char* acStatuses[] = { "headerStatus",   "markersStatus",      "eventsStatus",      "imgLeftStatus",
                             "imgRightStatus", "rawDataRightStatus", "rawDataLeftStatus", "threeDFidStatus" };

const char* acEvent[] = { "type", "data", "timestamp" };

mxArray* mrk2mat( const ftkMarker* in, const uint32 count )
{
    if ( in == nullptr || count == 0u )
    {
        return EMPTY_ARRAY;
    }

    mxArray* pMexMRK = mxCreateStructMatrix( count, 1, 7, acMarker );

    for ( uint32 u( 0u ); u < count; ++u )
    {
        const ftkMarker& marker = in[ u ];

        mxArray* pId = base2mat< uint32 >( marker.id );
        mxSetField( pMexMRK, static_cast< mwIndex >( u ), acMarker[ 0 ], pId );

        mxArray* pGeometryId = base2mat< uint32 >( marker.geometryId );
        mxSetField( pMexMRK, static_cast< mwIndex >( u ), acMarker[ 1 ], pGeometryId );

        mxArray* pGeometryPresMask = base2mat< uint32 >( marker.geometryPresenceMask );
        mxSetField( pMexMRK, static_cast< mwIndex >( u ), acMarker[ 2 ], pGeometryPresMask );

        mxArray* pFiducialCorresp = mxCreateDoubleMatrix( FTK_MAX_FIDUCIALS, 1, mxREAL );
        double* ptr = mxGetPr( pFiducialCorresp );
        for ( uint32 v( 0u ); v < FTK_MAX_FIDUCIALS; ++v )
        {
            *ptr++ = marker.fiducialCorresp[ v ] == INVALID_ID ? -1.0 : ( marker.fiducialCorresp[ v ] + 1 );
        }
        mxSetField( pMexMRK, static_cast< mwIndex >( u ), acMarker[ 3 ], pFiducialCorresp );

        mxArray* pRotation = mxCreateDoubleMatrix( 3, 3, mxREAL );
        ptr = mxGetPr( pRotation );
        for ( uint32 v( 0u ), w; v < 3u; ++v )
        {
            for ( w = 0u; w < 3u; ++w )
            {
                *ptr++ = marker.rotation[ w ][ v ];
            }
        }
        mxSetField( pMexMRK, static_cast< mwIndex >( u ), acMarker[ 4 ], pRotation );

        mxArray* pTranslationMM = mxCreateDoubleMatrix( 3, 1, mxREAL );
        ptr = mxGetPr( pTranslationMM );
        for ( uint32 v( 0u ); v < 3u; ++v )
        {
            *ptr++ = marker.translationMM[ v ];
        }
        mxSetField( pMexMRK, static_cast< mwIndex >( u ), acMarker[ 5 ], pTranslationMM );

        mxArray* pRegistrationErrorMM = base2mat< float32 >( marker.registrationErrorMM );
        mxSetField( pMexMRK, static_cast< mwIndex >( u ), acMarker[ 6 ], pRegistrationErrorMM );
    }
    return pMexMRK;
}

mxArray* tdf2mat( const ftk3DFiducial* in, const uint32 count )
{
    if ( in == nullptr || count == 0u )
    {
        return EMPTY_ARRAY;
    }

    mxArray* pMexTDF = mxCreateStructMatrix( count, 1, 3, acTDF );

    for ( uint32 u( 0u ); u < count; ++u )
    {
        const ftk3DFiducial& fid = in[ u ];
        mxArray* pEEP = base2mat< float32 >( fid.epipolarErrorPixels );
        mxArray* pTEMM = base2mat< float32 >( fid.triangulationErrorMM );
        mxArray* pPOS = mxCreateDoubleMatrix( 3, 1, mxREAL );
        double* ptr = mxGetPr( pPOS );

        *ptr++ = fid.positionMM.x;
        *ptr++ = fid.positionMM.y;
        *ptr++ = fid.positionMM.z;

        mxSetField( pMexTDF, static_cast< mwIndex >( u ), acTDF[ 0 ], pPOS );
        mxSetField( pMexTDF, static_cast< mwIndex >( u ), acTDF[ 1 ], pEEP );
        mxSetField( pMexTDF, static_cast< mwIndex >( u ), acTDF[ 2 ], pTEMM );
    }
    return pMexTDF;
}

mxArray* head2mat( const ftkImageHeader* in )
{
    if ( in == nullptr )
    {
        return EMPTY_ARRAY;
    }

    mxArray* pMexHead( mxCreateStructMatrix( 1, 1, 4, acHeader ) );

    mxArray* pTs( base2mat< uint64 >( in->timestampUS ) );
    mxArray* pCnt( base2mat< uint32 >( in->counter ) );
    mxArray* pHeight( base2mat< uint16 >( in->height ) );
    mxArray* pWidth( base2mat< uint16 >( in->width ) );

    mxSetField( pMexHead, static_cast< mwIndex >( 0u ), acHeader[ 0 ], pTs );
    mxSetField( pMexHead, static_cast< mwIndex >( 0u ), acHeader[ 1 ], pCnt );
    mxSetField( pMexHead, static_cast< mwIndex >( 0u ), acHeader[ 2 ], pHeight );
    mxSetField( pMexHead, static_cast< mwIndex >( 0u ), acHeader[ 3 ], pWidth );

    return pMexHead;
}

mxArray* evts2mat( ftkEvent** const in, const uint32 count )
{
    if ( in == nullptr )
    {
        return EMPTY_ARRAY;
    }

    mxArray* pMexEvts = mxCreateStructMatrix( count, 1, 3, acEvent );
    mxArray* pData( nullptr );

    const char* acTemperature[] = { "sensorId", "sensorValue" };

    const char* acFanEvt[] = { "fansStatus", "fans" };
    const char* acFan[] = { "pwmDuty", "speed" };

    const char* acActMrkrBtnStatEvt[] = { "imageCount", "deviceID", "buttonStatus" };

    const char* acActMrkrBtryStatEvt[] = { "imageCount", "deviceID", "batteryState" };

    const char* acActMrkMaskVersion2[] = { "Active Markers Presence Mask", "Active Markers Error Mask" };

    const char* acSynthTempEvt[] = { "currentValue", "referenceValue" };

    const char* acPTPEvt[] = { "status", "timestamp", "lastCorrection", "parentId" };
    const char* acPTPTimeStamp[] = { "nanoSeconds", "seconds" };
    const char* acPTPParentId[] = { "clockId", "sourcePort" };

    // Fill the event field from events received
    for ( uint32 u( 0u ); u < count; ++u )
    {
        const ftkEvent* event = *( in + u );
        switch ( event->Type )
        {
        /*In C++ it is illegal to skip a scalar declaration with initializer,
              but it is perfectly fine to skip a scalar declaration without
              initializer. That's why you'll see no initialiazation on declaration in switch. */
        case FtkEventType::fetTempV4:

            EvtTemperatureV4Payload* temperature;
            temperature = reinterpret_cast< EvtTemperatureV4Payload* >( event->Data );

            uint32 nTempSensors;
            nTempSensors = event->Payload / sizeof( EvtTemperatureV4Payload );
            pData = mxCreateStructMatrix( nTempSensors, 1, 2, acTemperature );

            for ( uint32 iSensor( 0u ); iSensor < nTempSensors; ++iSensor, ++temperature )
            {
                mxSetField( pData,
                            static_cast< mwIndex >( iSensor ),
                            acTemperature[ 0 ],
                            base2mat( temperature->SensorId ) );
                mxSetField( pData,
                            static_cast< mwIndex >( iSensor ),
                            acTemperature[ 1 ],
                            base2mat( temperature->SensorValue ) );
            }

            mxSetField(
              pMexEvts, static_cast< mwIndex >( u ), acEvent[ 0 ], mxCreateString( "Temperatures" ) );
            mxSetField( pMexEvts, static_cast< mwIndex >( u ), acEvent[ 1 ], pData );

            break;
        case FtkEventType::fetFansV1:

            EvtFansV1Payload* fanEvt;
            fanEvt = reinterpret_cast< EvtFansV1Payload* >( event->Data );

            // Create event's matlab structure
            pData = mxCreateStructMatrix( 1, 1, 2, acFanEvt );
            mxArray* pFan;
            pFan = mxCreateStructMatrix( FTK_NUM_FANS_PER_EVENT, 1, 2, acFan );

            for ( uint32 iFan( 0u ); iFan < FTK_NUM_FANS_PER_EVENT; ++iFan )
            {
                // Fill Fan fields{ "pwmDuty", "speed" }
                mxSetField( pFan,
                            static_cast< mwIndex >( iFan ),
                            acFan[ 0 ],
                            base2mat< uint8 >( fanEvt->Fans[ iFan ].PwmDuty ) );
                mxSetField( pFan,
                            static_cast< mwIndex >( iFan ),
                            acFan[ 1 ],
                            base2mat< uint16 >( fanEvt->Fans[ iFan ].Speed ) );
            }

            // Fill Fan event fields { "fanStatus", "fans" }
            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acFanEvt[ 0 ],
                        base2mat< uint8 >( static_cast< uint8 >( fanEvt->FansStatus ) ) );
            mxSetField( pData, static_cast< mwIndex >( 0u ), acFanEvt[ 1 ], pFan );

            // Fill event fields { "type", "data" }
            mxSetField( pMexEvts, static_cast< mwIndex >( u ), acEvent[ 0 ], mxCreateString( "Fans" ) );
            mxSetField( pMexEvts, static_cast< mwIndex >( u ), acEvent[ 1 ], pData );

            break;
        case FtkEventType::fetActiveMarkersMaskV1:

            EvtActiveMarkersMaskV1Payload* pMrkrMaskEvtV1;
            pMrkrMaskEvtV1 = reinterpret_cast< EvtActiveMarkersMaskV1Payload* >( event->Data );

            pData = base2mat< uint16 >( pMrkrMaskEvtV1->ActiveMarkersMask );

            mxSetField(
              pMexEvts, static_cast< mwIndex >( u ), acEvent[ 0 ], mxCreateString( "Active Markers Mask" ) );
            mxSetField( pMexEvts, static_cast< mwIndex >( u ), acEvent[ 1 ], pData );

            break;
        case FtkEventType::fetActiveMarkersMaskV2:

            EvtActiveMarkersMaskV2Payload* pMrkrMaskEvtV2;
            pMrkrMaskEvtV2 = reinterpret_cast< EvtActiveMarkersMaskV2Payload* >( event->Data );

            pData = mxCreateStructMatrix( 1, 1, 2, acActMrkMaskVersion2 );

            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acActMrkrBtnStatEvt[ 0 ],
                        base2mat< uint16 >( pMrkrMaskEvtV2->ActiveMarkersMask ) );
            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acActMrkrBtnStatEvt[ 1 ],
                        base2mat< uint16 >( pMrkrMaskEvtV2->ActiveMarkersErrorMask ) );

            mxSetField(
              pMexEvts, static_cast< mwIndex >( u ), acEvent[ 0 ], mxCreateString( "Active Markers Mask" ) );
            mxSetField( pMexEvts, static_cast< mwIndex >( u ), acEvent[ 1 ], pData );

            break;
        case FtkEventType::fetActiveMarkersButtonStatusV1:

            EvtActiveMarkersButtonStatusesV1Payload* pMrkrBtnStatEvt;
            pMrkrBtnStatEvt = reinterpret_cast< EvtActiveMarkersButtonStatusesV1Payload* >( event->Data );

            pData = mxCreateStructMatrix( 1, 1, 3, acActMrkrBtnStatEvt );
            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acActMrkrBtnStatEvt[ 0 ],
                        base2mat< uint32 >( pMrkrBtnStatEvt->ImageCount ) );
            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acActMrkrBtnStatEvt[ 1 ],
                        base2mat< uint8 >( pMrkrBtnStatEvt->DeviceID ) );
            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acActMrkrBtnStatEvt[ 2 ],
                        base2mat< uint8 >( pMrkrBtnStatEvt->ButtonStatus ) );

            mxSetField( pMexEvts,
                        static_cast< mwIndex >( u ),
                        acEvent[ 0 ],
                        mxCreateString( "Active Markers Button Statuses" ) );
            mxSetField( pMexEvts, static_cast< mwIndex >( u ), acEvent[ 1 ], pData );

            break;
        case FtkEventType::fetActiveMarkersBatteryStateV1:

            EvtActiveMarkersBatteryStateV1Payload* pMrkrBtryStatEvt;
            pMrkrBtryStatEvt = reinterpret_cast< EvtActiveMarkersBatteryStateV1Payload* >( event->Data );

            pData = mxCreateStructMatrix( 1, 1, 3, acActMrkrBtryStatEvt );
            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acActMrkrBtnStatEvt[ 0 ],
                        base2mat< uint32 >( pMrkrBtryStatEvt->ImageCount ) );
            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acActMrkrBtnStatEvt[ 1 ],
                        base2mat< uint8 >( pMrkrBtryStatEvt->DeviceID ) );
            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acActMrkrBtnStatEvt[ 2 ],
                        base2mat< uint8 >( pMrkrBtryStatEvt->BatteryState ) );

            mxSetField( pMexEvts,
                        static_cast< mwIndex >( u ),
                        acEvent[ 0 ],
                        mxCreateString( "Active Markers Battery State" ) );
            mxSetField( pMexEvts, static_cast< mwIndex >( u ), acEvent[ 1 ], pData );

            break;
        case FtkEventType::fetSyntheticTemperaturesV1:

            EvtSyntheticTemperaturesV1Payload* pSynthTemp;
            pSynthTemp = reinterpret_cast< EvtSyntheticTemperaturesV1Payload* >( event->Data );

            pData = mxCreateStructMatrix( 1, 1, 2, acSynthTempEvt );

            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acSynthTempEvt[ 0 ],
                        base2mat< float >( pSynthTemp->CurrentValue ) );
            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acSynthTempEvt[ 1 ],
                        base2mat< float >( pSynthTemp->ReferenceValue ) );

            mxSetField( pMexEvts,
                        static_cast< mwIndex >( u ),
                        acEvent[ 0 ],
                        mxCreateString( "Synthetic Temperatures" ) );
            mxSetField( pMexEvts, static_cast< mwIndex >( u ), acEvent[ 1 ], pData );

            break;
        case FtkEventType::fetSynchronisationPTPV1:

            EvtSynchronisationPTPV1Payload* pSynchPTP;
            pSynchPTP = reinterpret_cast< EvtSynchronisationPTPV1Payload* >( event->Data );

            pData = mxCreateStructMatrix( 1, 1, 4, acPTPEvt );

            mxArray* pTimestamp;
            mxArray* pCorrection;
            mxArray* pParentId;
            pTimestamp = mxCreateStructMatrix( 1, 1, 2, acPTPTimeStamp );
            pCorrection = mxCreateStructMatrix( 1, 1, 2, acPTPTimeStamp );
            pParentId = mxCreateStructMatrix( 1, 1, 2, acPTPParentId );

            mxSetField( pTimestamp,
                        static_cast< mwIndex >( 0u ),
                        acPTPTimeStamp[ 0 ],
                        base2mat< uint32 >( static_cast< uint32 >( pSynchPTP->Timestamp.NanoSeconds ) ) );
            mxSetField( pTimestamp,
                        static_cast< mwIndex >( 0u ),
                        acPTPTimeStamp[ 1 ],
                        base2mat< uint64 >( static_cast< uint64 >( pSynchPTP->Timestamp.Seconds ) ) );

            mxSetField(
              pCorrection,
              static_cast< mwIndex >( 0u ),
              acPTPTimeStamp[ 0 ],
              base2mat< uint32 >( static_cast< uint32 >( pSynchPTP->LastCorrection.NanoSeconds ) ) );
            mxSetField( pCorrection,
                        static_cast< mwIndex >( 0u ),
                        acPTPTimeStamp[ 1 ],
                        base2mat< uint64 >( static_cast< uint64 >( pSynchPTP->LastCorrection.Seconds ) ) );

            mxSetField( pParentId,
                        static_cast< mwIndex >( 0u ),
                        acPTPParentId[ 0 ],
                        base2mat< uint64 >( static_cast< uint64 >( pSynchPTP->ParentId.ClockId ) ) );
            mxSetField( pParentId,
                        static_cast< mwIndex >( 0u ),
                        acPTPParentId[ 1 ],
                        base2mat< uint16 >( static_cast< uint16 >( pSynchPTP->ParentId.SourcePortId ) ) );

            mxSetField( pData,
                        static_cast< mwIndex >( 0u ),
                        acPTPEvt[ 0 ],
                        base2mat< uint16 >( static_cast< uint16 >( pSynchPTP->Status ) ) );
            mxSetField( pData, static_cast< mwIndex >( 0u ), acPTPEvt[ 1 ], pTimestamp );
            mxSetField( pData, static_cast< mwIndex >( 0u ), acPTPEvt[ 2 ], pCorrection );
            mxSetField( pData, static_cast< mwIndex >( 0u ), acPTPEvt[ 3 ], pParentId );

            mxSetField(
              pMexEvts, static_cast< mwIndex >( u ), acEvent[ 0 ], mxCreateString( "Synchronisation PTP" ) );
            mxSetField( pMexEvts, static_cast< mwIndex >( u ), acEvent[ 1 ], nullptr );

            break;
        case FtkEventType::fetLowTemp:

            mxSetField(
              pMexEvts, static_cast< mwIndex >( u ), acEvent[ 0 ], mxCreateString( "Low Temperature" ) );

            break;
        case FtkEventType::fetHighTemp:

            mxSetField(
              pMexEvts, static_cast< mwIndex >( u ), acEvent[ 0 ], mxCreateString( "High Temperature" ) );

            break;
        default:

            mxSetField(
              pMexEvts, static_cast< mwIndex >( u ), acEvent[ 0 ], mxCreateString( "Unhandled Event" ) );
            mxSetField( pMexEvts,
                        static_cast< mwIndex >( u ),
                        acEvent[ 1 ],
                        base2mat< uint32 >( static_cast< uint32 >( event->Type ) ) );

            break;
        }
        mxSetField(
          pMexEvts, static_cast< mwIndex >( u ), acEvent[ 2 ], base2mat< uint64 >( event->Timestamp ) );
    }
    return pMexEvts;
}

mxArray* stat2mat( const ftkFrameQuery& in )
{
    mxArray* pMexStat = mxCreateStructMatrix( 1, 1, 8, acStatuses );

    mxArray* pHead( base2mat< int8 >( static_cast< int8 >( in.imageHeaderStat ) ) );
    mxArray* pMrk( base2mat< int8 >( static_cast< int8 >( in.markersStat ) ) );
    mxArray* pEvts( base2mat< int8 >( static_cast< int8 >( in.eventsStat ) ) );
    mxArray* pImgLeft( base2mat< int8 >( static_cast< int8 >( in.imageLeftStat ) ) );
    mxArray* pImgRight( base2mat< int8 >( static_cast< int8 >( in.imageRightStat ) ) );
    mxArray* pRawLeft( base2mat< int8 >( static_cast< int8 >( in.rawDataLeftStat ) ) );
    mxArray* pRawRight( base2mat< int8 >( static_cast< int8 >( in.rawDataRightStat ) ) );
    mxArray* pTDF( base2mat< int8 >( static_cast< int8 >( in.threeDFiducialsStat ) ) );

    mxSetField( pMexStat, static_cast< mwIndex >( 0u ), acStatuses[ 0 ], pHead );
    mxSetField( pMexStat, static_cast< mwIndex >( 0u ), acStatuses[ 1 ], pMrk );
    mxSetField( pMexStat, static_cast< mwIndex >( 0u ), acStatuses[ 2 ], pEvts );
    mxSetField( pMexStat, static_cast< mwIndex >( 0u ), acStatuses[ 3 ], pImgLeft );
    mxSetField( pMexStat, static_cast< mwIndex >( 0u ), acStatuses[ 4 ], pImgRight );
    mxSetField( pMexStat, static_cast< mwIndex >( 0u ), acStatuses[ 5 ], pRawLeft );
    mxSetField( pMexStat, static_cast< mwIndex >( 0u ), acStatuses[ 6 ], pRawRight );
    mxSetField( pMexStat, static_cast< mwIndex >( 0u ), acStatuses[ 7 ], pTDF );

    return pMexStat;
}

mxArray* frame2mat( const ftkFrameQuery& in )
{
    mxArray* pMexFrame = mxCreateStructMatrix( 1, 1, 5, acFrame );

    if ( in.threeDFiducialsStat == ftkQueryStatus::QS_OK )
    {
        mxArray* pTDF = tdf2mat( in.threeDFiducials, in.threeDFiducialsCount );
        mxSetField( pMexFrame, 0, acFrame[ 0 ], pTDF );
    }

    if ( in.markersStat == ftkQueryStatus::QS_OK )
    {
        mxArray* pMarkers = mrk2mat( in.markers, in.markersCount );
        mxSetField( pMexFrame, 0, acFrame[ 1 ], pMarkers );
    }

    if ( in.imageHeaderStat == ftkQueryStatus::QS_OK )
    {
        mxArray* pHead( head2mat( in.imageHeader ) );
        mxSetField( pMexFrame, 0, acFrame[ 2 ], pHead );
    }

    mxArray* pStatus( stat2mat( in ) );
    mxSetField( pMexFrame, 0, acFrame[ 3 ], pStatus );

    if ( in.eventsStat == ftkQueryStatus::QS_OK )
    {
        mxArray* pEvents( evts2mat( in.events, in.eventsCount ) );
        mxSetField( pMexFrame, 0, acFrame[ 4 ], pEvents );
    }

    return pMexFrame;
}
