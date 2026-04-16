// ============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo19_OpticalCommunication.cpp
 *   \brief Demonstrate how to pair active markers and retrieve data from them.
 *
 *   How to compile this example:
 *   - Follow instructions in README.txt
 *
 *   How to run this example:
 *   - Install the fusionTrack/spryTrack driver (see documentation)
 *   - Switch on device
 *   - Run the resulting executable
 *
 */
// ============================================================================

#include "ftkTypes.h"
#include "helpers.hpp"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

ftkLibrary lib = 0;
uint64 sn( 0uLL );

struct ActiveMarkerBatteryAndButtonStatus
{
    uint8 BatteryState;
    uint8 ButtonStatus;
};

// ---------------------------------------------------------------------------
// main function
int main( int argc, char** argv )
{
    const bool isNotFromConsole = isLaunchedFromExplorer();

    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << "This is a demonstration on how to retrieve a wireless marker battery and button status, using "
            "events."
         << endl;

    deque< string > args;
    for ( int i( 1 ); i < argc; ++i )
    {
        args.emplace_back( argv[ i ] );
    }

    bool showHelp( false );

    if ( !args.empty() )
    {
        showHelp =
          ( find_if( args.cbegin(),
                     args.cend(),
                     []( const string& val ) { return val == "-h" || val == "--help"; } ) != args.cend() );
    }

    string cfgFile( "" );

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-c/--config F] " << flush << "JSON config file. Type "
             << "std::string, default = none" << endl;
    }

    cout << "Copyright (c) Atracsys LLC 2003-2021" << endl;
    if ( showHelp )
    {
#ifdef ATR_WIN
        if ( isLaunchedFromExplorer() )
        {
            cout << "Press the \"ANY\" key to quit" << endl;
            waitForKeyboardHit();
        }
#endif
        return 0;
    }

    auto pos( find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-c" || val == "--config"; } ) );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        cfgFile = *pos;
    }

    // ----------------------------------------------------------------------
    // Initialize driver

    ftkBuffer buffer;

    lib = ftkInitExt( cfgFile.empty() ? nullptr : cfgFile.c_str(), &buffer );
    if ( lib == nullptr )
    {
        cerr << buffer.data << endl;
        error( "Cannot initialize driver", !isNotFromConsole );
    }

    // ------------------------------------------------------------------------
    // Retrieve the device

    DeviceData device( retrieveLastDevice( lib, true, false, !isNotFromConsole ) );
    uint64 sn( device.SerialNumber );

    // ----------------------------------------------------------------------
    // Retrieve the Device Options IDs
    map< string, uint32 > options{};

    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &options ) );

    if ( err != ftkError::FTK_OK )
    {
        checkError( lib );
    }
    if ( options.empty() )
    {
        cerr << "Could not retrieve any options." << endl;
        checkError( lib );
    }

    // ------------------------------------------------------------------------
    // When using a spryTrack, sending of the images is disabled so that the
    // sample operates on a USB2 connection
    if ( ftkDeviceType::DEV_SPRYTRACK_180 == device.Type || ftkDeviceType::DEV_SPRYTRACK_300 == device.Type )
    {
        if ( options.find( "Enable images sending" ) == options.cend() )
        {
            cerr << "Could not find option 'Enable images sending'" << endl;
            checkError( lib );
        }
        cout << "Disable images sending" << endl;
        if ( ftkSetInt32( lib, sn, options[ "Enable images sending" ], 0 ) != ftkError::FTK_OK )
        {
            error( "Cannot disable images sending on the SpryTrack.", !isNotFromConsole );
        }
    }

    // ------------------------------------------------------------------------
    // Initialize the frame.

    ftkFrameQuery* frame = ftkCreateFrame();

    if ( frame == 0 )
    {
        error( "Cannot create frame instance", !isNotFromConsole );
    }
    err = ftkSetFrameOptions( false /* No pictures */,
                              20u /* maximal number of retrieved ftkEvents */,
                              0u /* maximal number of retrieved ftkRawData */,
                              0u /* maximal number of retrieved ftkRawData */,
                              0u /* maximal number of retrieved ftk3DFiducial */,
                              16u /* maximal number of retrieved ftkMarker */,
                              frame );

    if ( err != ftkError::FTK_OK )
    {
        ftkDeleteFrame( frame );
        checkError( lib, !isNotFromConsole );
    }

    bool neededOptionPresent( true );
    string errMsg( "Missing option(s):" );
    for ( const string& optName : { "Active Wireless Mode Enable",
                                    "Active Wireless Pairing Enable",
                                    "Active Wireless button statuses streaming",
                                    "Active Wireless battery state streaming",
                                    "Active Wireless Markers reset ID" } )
    {
        if ( options.find( optName ) == options.cend() )
        {
            errMsg += " '" + optName + "'";
            neededOptionPresent = false;
        }
    }

    if ( !neededOptionPresent )
    {
        error( errMsg.c_str() );
    }

    if ( options.find( "Enables IR strobe" ) == options.cend() &&
         options.find( "Strobe mode" ) == options.cend() )
    {
        error( "No options to tune IR LED flash" );
    }

    cout << "Enable Wireless Mode" << endl;
    if ( ftkSetInt32( lib, sn, options[ "Active Wireless Mode Enable" ], 1 ) != ftkError::FTK_OK )
    {
        error( "Cannot enable Wireless Mode.", !isNotFromConsole );
    }
    cout << endl << " *** Put marker in front of the device to pair ***" << endl;

    cout << "Enable Pairing" << endl;
    if ( ftkSetInt32( lib, sn, options[ "Active Wireless Pairing Enable" ], 1 ) != ftkError::FTK_OK )
    {
        error( "Cannot enable pairing.", !isNotFromConsole );
    }
    cout << endl << " *** Put marker in front of the device to pair ***" << endl;

#ifdef ATR_WIN
    cout << endl << " ***   Hit a key once pairing is completed  ***" << endl;
    waitForKeyboardHit();
#else
    cout << endl << " ***   You have 10 seconds.  ***" << endl;
    sleep( 10 * 1000 );
#endif

    cout << "Disable Pairing" << endl;
    if ( ftkSetInt32( lib, sn, options[ "Active Wireless Pairing Enable" ], 0 ) != ftkError::FTK_OK )
    {
        error( "Cannot disable pairing.", !isNotFromConsole );
    }

    cout << "Enable button statuses streaming" << endl;
    if ( ftkSetInt32( lib, sn, options[ "Active Wireless button statuses streaming" ], 1 ) !=
         ftkError::FTK_OK )
    {
        error( "Cannot enable button statuses streaming.", !isNotFromConsole );
    }

    cout << "Enable battery state streaming" << endl;
    if ( ftkSetInt32( lib, sn, options[ "Active Wireless battery state streaming" ], 1 ) != ftkError::FTK_OK )
    {
        error( "Cannot enable battery state streaming.", !isNotFromConsole );
    }

    cout << "Disable IR Strobes" << endl;
    if ( options.find( "Enables IR strobe" ) != options.cend() )
    {
        if ( ftkSetInt32( lib, sn, options[ "Enables IR strobe" ], 0 ) != ftkError::FTK_OK )
        {
            cerr << "Cannot disable IR strobes." << endl;
            checkError( lib, !isNotFromConsole );
        }
    }
    else
    {
        if ( ftkSetInt32( lib, sn, options[ "Strobe mode" ], 1 ) != ftkError::FTK_OK )
        {
            cerr << "Cannot set strobe mode to 1" << endl;
            checkError( lib, !isNotFromConsole );
        }
    }

    std::map< uint8, ActiveMarkerBatteryAndButtonStatus > markersState;

    int cyclesToPerform = 3240;
    while ( cyclesToPerform > 0 )
    {
        err = ftkGetLastFrame( lib, sn, frame, 100u );
        if ( err == ftkError::FTK_WAR_NO_FRAME )
        {
            continue;
        }
        else if ( err > ftkError::FTK_OK )
        {
            cerr << "Error while trying to aquire frames." << endl;
            break;
        }
        cyclesToPerform--;
        for ( unsigned int i = 0; i < frame->eventsCount; i++ )
        {
            ftkEvent* frameEvent = *( frame->events + i );
            // Did someone press a button ?
            if ( frameEvent && frameEvent->Type == FtkEventType::fetActiveMarkersButtonStatusV1 )
            {
                const unsigned int n =
                  ( frameEvent->Payload / sizeof( EvtActiveMarkersButtonStatusesV1Payload ) );
                for ( unsigned int j = 0; j < n; j++ )
                {
                    EvtActiveMarkersButtonStatusesV1Payload buttonStatuses;
                    memcpy( &buttonStatuses,
                            frameEvent->Data + ( j * sizeof( EvtActiveMarkersButtonStatusesV1Payload ) ),
                            sizeof( EvtActiveMarkersButtonStatusesV1Payload ) );
                    if ( markersState.find( buttonStatuses.DeviceID ) == markersState.end() )
                    {
                        // First time, insert in the map.
                        markersState[ buttonStatuses.DeviceID ] = ActiveMarkerBatteryAndButtonStatus();
                    }
                    if ( markersState[ buttonStatuses.DeviceID ].ButtonStatus != buttonStatuses.ButtonStatus )
                    {
                        cout << "Marker " << static_cast< int >( buttonStatuses.DeviceID )
                             << " new button status: " << static_cast< int >( buttonStatuses.ButtonStatus )
                             << endl;
                        markersState[ buttonStatuses.DeviceID ].ButtonStatus = buttonStatuses.ButtonStatus;
                    }
                }
            }
            // Did the battery level change ?
            if ( frameEvent && frameEvent->Type == FtkEventType::fetActiveMarkersBatteryStateV1 )
            {
                const unsigned int n =
                  ( frameEvent->Payload / sizeof( EvtActiveMarkersBatteryStateV1Payload ) );
                for ( unsigned int j = 0; j < n; j++ )
                {
                    EvtActiveMarkersBatteryStateV1Payload batteryState;
                    memcpy( &batteryState,
                            frameEvent->Data + ( j * sizeof( EvtActiveMarkersBatteryStateV1Payload ) ),
                            sizeof( EvtActiveMarkersBatteryStateV1Payload ) );
                    if ( markersState.find( batteryState.DeviceID ) == markersState.end() )
                    {
                        // First time, insert in the map.
                        markersState[ batteryState.DeviceID ] = ActiveMarkerBatteryAndButtonStatus();
                    }
                    if ( markersState[ batteryState.DeviceID ].BatteryState != batteryState.BatteryState )
                    {
                        cout << "Marker " << static_cast< int >( batteryState.DeviceID )
                             << " new battery status: " << static_cast< int >( batteryState.BatteryState )
                             << endl;
                        markersState[ batteryState.DeviceID ].BatteryState = batteryState.BatteryState;
                    }
                }
            }
        }
    }

    cout << "Additional info about paired markers:" << endl;

    if ( ftkGetData( lib, sn, options[ "Active Wireless Markers info" ], &buffer ) != ftkError::FTK_OK )
    {
        error( "Cannot get additional infos on the markers.", !isNotFromConsole );
    }
    std::string info( buffer.data, buffer.data + buffer.size );
    cout << info << endl;

    for ( std::map< uint8, ActiveMarkerBatteryAndButtonStatus >::iterator iter = markersState.begin();
          iter != markersState.end();
          ++iter )
    {
        cout << "Reset marker " << static_cast< int >( iter->first ) << endl;
        if ( ftkSetInt32( lib, sn, options[ "Active Wireless Markers reset ID" ], iter->first ) !=
             ftkError::FTK_OK )
        {
            error( "Cannot reset marker.", !isNotFromConsole );
        }
    }

    sleep( 3000 );
    // ------------------------------------------------------------------------
    // Close driver

    ftkDeleteFrame( frame );

    if ( ftkError::FTK_OK != ftkClose( &lib ) )
    {
        checkError( lib, !isNotFromConsole );
    }

    cout << endl << "\tSUCCESS" << endl;

#ifdef ATR_WIN
    if ( isNotFromConsole )
    {
        cout << endl << " *** Hit a key to exit ***" << endl;
        waitForKeyboardHit();
    }
#endif
    return EXIT_SUCCESS;
}
