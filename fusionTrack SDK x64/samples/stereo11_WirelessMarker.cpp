// ============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo11_WirelessMarker.cpp
 *   \brief Demonstrate how to enable wireless markers.
 *
 *   This sample aims to present the following driver features:
 *   - Open/close the driver
 *   - Enumerate devices
 *   - Activate and detect wireless markers
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
// ============================================================================
#include <TrackingSystem.hpp>

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

int main( int argc, char* argv[] )
{
    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    deque< string > args;
    for ( int i( 1 ); i < argc; ++i )
    {
        args.emplace_back( argv[ i ] );
    }

    bool showHelp( false );

    if ( !args.empty() )
    {
        showHelp = ( find_if( args.cbegin(), args.cend(), []( const string& val ) {
                         return val == "-h" || val == "--help";
                     } ) != args.cend() );
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
        atracsys::continueOnUserInput();
        return 0;
    }

    auto pos( find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-c" || val == "--config"; } ) );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        cfgFile = *pos;
    }

    /*
     * Initialisation sequence: first the library, then retrieving the device.
     */
    atracsys::TrackingSystem wrapper( 100u, false );

    atracsys::Status retCode( wrapper.initialise( cfgFile ) );

    if ( retCode != atracsys::Status::Ok )
    {
        atracsys::reportError( "Cannot initialise library" );
    }

    cout << "Library initialised" << endl;

    retCode = wrapper.enumerateDevices();

    if ( retCode != atracsys::Status::Ok )
    {
        atracsys::reportError( "No devices detected" );
    }

    vector< atracsys::DeviceInfo > devices;

    retCode = wrapper.getEnumeratedDevices( devices );
    if ( retCode != atracsys::Status::Ok )
    {
        atracsys::reportError( "No devices detected" );
    }
    uint64 sn( devices.front().serialNumber() );

    retCode = wrapper.setIntOption( sn, "Active Wireless Mode Enable", 1 );
    if ( retCode != atracsys::Status::Ok )
    {
        atracsys::reportError( "Cannot enable wireless mode" );
    }

    retCode = wrapper.setIntOption( sn, "Active Wireless Pairing Enable", 1 );
    if ( retCode != atracsys::Status::Ok )
    {
        atracsys::reportError( "Cannot enable pairing" );
    }

    /*
     * Frame creation and initialisation.
     */
    retCode = wrapper.createFrame( false, 10u, 10u, 10u, 10u );
    if ( retCode != atracsys::Status::Ok )
    {
        atracsys::reportError( "Cannot allocate frame" );
    }

    wrapper.setIntOption( sn, "Enables IR strobe", 0 );

    uint16 savedMask( 0u ), currentMask;
    uint32 j, k;

    atracsys::FrameData frame;

    for ( uint32 i( 0u ); i < 1000u; ++i )
    {
        retCode = wrapper.getLastFrameForDevice( sn, frame );
        if ( retCode != atracsys::Status::Ok )
        {
            wrapper.streamLastError( cerr );
        }
        for ( const auto& evt : frame.events() )
        {
            if ( evt.type() == FtkEventType::fetActiveMarkersMaskV1 )
            {
                currentMask = evt.activeMarkerMaskV1().activeMarkersMask();
                if ( currentMask != savedMask )
                {
                    k = 0u;
                    for ( j = 1u; j < 0x10000; j <<= 1, ++k )
                    {
                        if ( ( currentMask & j ) != 0u && ( savedMask & j ) == 0u )
                        {
                            savedMask |= j;
                            cout << "Detected marker short ID " << k << endl;
                        }
                    }
                }
            }
        }
        for ( const auto& marker : frame.markers() )
        {
            cout << "Detected marker ID " << marker.geometryId() << ", located at ("
                 << marker.position()[ 0u ] << ", " << marker.position()[ 1u ] << ", "
                 << marker.position()[ 2u ] << ")" << endl;
            break;
        }
    }

    wrapper.setIntOption( sn, "Enables IR strobe", 1 );

    cout << "\tSUCCESS" << endl;

    atracsys::continueOnUserInput();

    return 0;
}
