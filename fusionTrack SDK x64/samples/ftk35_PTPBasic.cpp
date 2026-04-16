// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2022 by Atracsys LLC. All rights reserved.
 *
 *   \file ftk35_PTPBasic.cpp
 *   \brief Demonstrate how to use PTP with the fusionTrack.
 *
 *   How to compile this example:
 *   - Follow instructions in README.txt
 *
 *   How to run this example:
 *   - Install the fusionTrack driver (see documentation)
 *   - Switch on device
 *   - Run the resulting executable
 *
 */
// =============================================================================
#include "helpers.hpp"

#include <ftkInterface.h>
#include <ftkPlatform.h>

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

void localDeviceEnumerator( uint64 sn, void* user, ftkOptionsInfo* oi );

int main( int argc, char* argv[] )
{
    const bool isNotFromConsole = isLaunchedFromExplorer();

    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << "This is a demonstration on how to use PTP with the fusionTrack." << endl;

    // ----------------------------------------------------------------------
    // Initialize driver

    ftkBuffer buffer{};

    ftkLibrary lib( ftkInitExt( nullptr, &buffer ) );
    if ( lib == nullptr )
    {
        cerr << buffer.data << endl;
        error( "Cannot initialise library", !isNotFromConsole );
    }
    else
    {
        cout << "Library initialised" << endl;
    }

    DeviceData device( retrieveLastDevice( lib, true, false, !isNotFromConsole ) );
    uint64 sn( device.SerialNumber );

    map< string, uint32 > ptpOptions;

    ftkError err( ftkEnumerateOptions( lib, sn, localDeviceEnumerator, &ptpOptions ) );

    if ( err != ftkError::FTK_OK )
    {
        checkError( lib, !isNotFromConsole );
    }
    if ( ptpOptions.size() < 2u )  // TODO: change this if more options are needed.
    {
        cerr << "Could not retrieve the 2 wanted options." << endl;
        checkError( lib, !isNotFromConsole );
    }

    ftkFrameQuery* frame( ftkCreateFrame() );
    err = ftkSetFrameOptions( false, 10u, 0u, 0u, 0u, 0u, frame );

    // ----------------------------------------------------------------------
    // Options configuration
    buffer.reset();
    ErrorReader errReader;

    // TODO: Use this to set options. The names of the options are available by running the sample 1:
    // ListOptions.
    uint32 optId( ptpOptions[ "PTP module enable" ] );
    err = ftkSetInt32( lib, sn, optId, 1 );
    if ( err != ftkError::FTK_OK )
    {
        ftkGetLastErrorString( lib, sizeof( buffer.data ), buffer.data );
        if ( !errReader.parseErrorString( buffer.data ) )
        {
            cerr << "Cannot interpret error string:" << endl << buffer.data << endl;
            return 1;
        }
        else
        {
            errReader.display();
        }
        return 1;
    }

    optId = ptpOptions[ "PTP synchronisation enable" ];
    err = ftkSetInt32( lib, sn, optId, 1 );
    if ( err != ftkError::FTK_OK )
    {
        ftkGetLastErrorString( lib, sizeof( buffer.data ), buffer.data );
        if ( !errReader.parseErrorString( buffer.data ) )
        {
            cerr << "Cannot interpret error string:" << endl << buffer.data << endl;
            return 1;
        }
        else
        {
            errReader.display();
        }
        return 1;
    }

    // ----------------------------------------------------------------------
    // Frame acquisition, PTP events analysis

    const ftkEvent* evt( nullptr );
    const EvtSynchronisationPTPV1Payload* ptpData( nullptr );
    uint32_t counter( 0u );
    bool finished( false );

    for ( unsigned int n( 0u ); n < 1000u; ++n )
    {
        err = ftkGetLastFrame( lib, sn, frame, 100u );
        if ( err > ftkError::FTK_OK )
        /* block until next frame is available */
        {
            cout << ".";
            continue;
        }
        else if ( err == ftkError::FTK_WAR_NO_FRAME )
        {
            continue;
        }

        for ( uint32_t iEvt( 0u ); iEvt < frame->eventsCount; ++iEvt )
        {
            evt = frame->events[ iEvt ];
            if ( evt->Type == FtkEventType::fetSynchronisationPTPV1 )
            {
                ptpData = reinterpret_cast< EvtSynchronisationPTPV1Payload* >( evt->Data );
                cout << endl;
                cout << "Port State ID: " << ptpData->Status.PortStateId << endl;
                cout << "Error ID: " << ptpData->Status.ErrorId << endl;
                cout << "Parent ID: 0x" << hex << ptpData->ParentId.SourcePortId << " (source), 0x"
                     << ptpData->ParentId.ClockId << " (clock)" << endl
                     << dec;
                cout << "Timestamp: " << ptpData->Timestamp.Seconds << " s, "
                     << ptpData->Timestamp.NanoSeconds << " ns" << endl;
                cout << "Last correction: " << ptpData->LastCorrection.Seconds << " s, "
                     << ptpData->LastCorrection.NanoSeconds << " ns" << endl;
                if ( ++counter == 20u )
                {
                    finished = true;
                    break;
                }
            }
        }

        if ( finished )
        {
            break;
        }

        sleep( 1000 );
    }

    cout << "\tSUCCESS" << endl;

    optId = ptpOptions[ "PTP synchronisation enable" ];
    err = ftkSetInt32( lib, sn, optId, 0 );
    if ( err != ftkError::FTK_OK )
    {
        ftkGetLastErrorString( lib, sizeof( buffer.data ), buffer.data );
        if ( !errReader.parseErrorString( buffer.data ) )
        {
            cerr << "Cannot interpret error string:" << endl << buffer.data << endl;
            return 1;
        }
        else
        {
            errReader.display();
        }
        return 1;
    }

    optId = ptpOptions[ "PTP module enable" ];
    err = ftkSetInt32( lib, sn, optId, 0 );
    if ( err != ftkError::FTK_OK )
    {
        ftkGetLastErrorString( lib, sizeof( buffer.data ), buffer.data );
        if ( !errReader.parseErrorString( buffer.data ) )
        {
            cerr << "Cannot interpret error string:" << endl << buffer.data << endl;
            return 1;
        }
        else
        {
            errReader.display();
        }
        return 1;
    }

    ftkDeleteFrame( frame );
    ftkClose( &lib );

#ifdef ATR_WIN
    if ( isNotFromConsole )
    {
        cout << endl << "*** Hit a key to exit ***" << endl;
        waitForKeyboardHit();
    }
#endif

    return 0;
}

void localDeviceEnumerator( uint64 sn, void* user, ftkOptionsInfo* oi )
{
    if ( user == 0 )
    {
        return;
    }

    if ( string( oi->name ).find( "PTP module enable" ) == string::npos &&
         string( oi->name ).find( "PTP synchronisation enable" ) == string::npos )
    {
        return;
    }

    map< string, uint32 >* mapping = reinterpret_cast< map< string, uint32 >* >( user );

    if ( mapping == 0 )
    {
        return;
    }

    mapping->insert( make_pair( oi->name, oi->id ) );
}
