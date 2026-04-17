// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo9_GetGetAcceleration.cpp
 *   \brief Demonstrate how to get acceleration.
 *
 *   This sample aims to present the following driver features:
 *   - Open/close the driver
 *   - Enumerate devices
 *   - Get the currently read acceleration.
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
#include <cmath>
#include <cstring>
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
    const bool isNotFromConsole = isLaunchedFromExplorer();

    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << "This is a demonstration on how to get the accelerometer data." << endl;

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

    ftkLibrary lib( ftkInitExt( cfgFile.empty() ? nullptr : cfgFile.c_str(), &buffer ) );
    if ( lib == nullptr )
    {
        cerr << buffer.data << endl;
        error( "Cannot initialise library", !isNotFromConsole );
    }
    else
    {
        cout << "Library initialised" << endl;
    }

    DeviceData device( retrieveLastDevice( lib, false, false, !isNotFromConsole ) );
    uint64 sn( device.SerialNumber );

    map< string, uint32 > optionMapping{};
    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &optionMapping ) );

    if ( err != ftkError::FTK_OK )
    {
        checkError( lib, !isNotFromConsole );
    }
    else if ( optionMapping.find( "Shock sensor threshold" ) == optionMapping.cend() )
    {
        cerr << "Could not find option Shock sensor threshold" << endl;
    }
    else
    {
        int32 thresholdValue( 0 );
        err = ftkGetInt32( lib,
                           sn,
                           optionMapping.at( "Shock sensor threshold" ),
                           &thresholdValue,
                           ftkOptionGetter::FTK_VALUE );
        if ( err != ftkError::FTK_OK )
        {
            checkError( lib, !isNotFromConsole );
        }

        cout << "Shocks below " << static_cast< float >( thresholdValue ) * 9.81f
             << " m/s^2 won't be recorded" << endl;
    }

    ftkFrameQuery* frame( ftkCreateFrame() );
    if ( frame == nullptr )
    {
        cerr << "Could not create frame" << endl;
        ftkClose( &lib );
        return 1;
    }

    err = ftkGetLastFrame( lib, sn, frame, 100u );

    ftkDeleteFrame( frame );

    ftk3DPoint measure;

    string mess;

    char tmp[ 1024u ];

    int32_t symValue( -1 );

    while ( symValue != 1 )
    {
        if ( ++symValue == 1 )
        {
            if ( optionMapping.find( "Symmetrise coordinates" ) == optionMapping.cend() )
            {
                break;
            }
            else if ( optionMapping.find( "Symmetrise coordinates" ) != optionMapping.cend() )
            {
                err = ftkSetInt32( lib, sn, optionMapping.at( "Symmetrise coordinates" ), symValue );
                if ( err != ftkError::FTK_OK )
                {
                    checkError( lib, !isNotFromConsole );
                }

                cout << endl << "Switching to symmetrised coordinates" << endl;
            }
        }

        err = ftkGetAccelerometerData( lib, sn, 0u, &measure );

        if ( err != ftkError::FTK_OK )
        {
            ftkGetLastErrorString( lib, 1024u, tmp );
            cerr << tmp << endl;
            cerr << "Cannot get acceleration value 0" << endl;
#ifdef ATR_WIN
            if ( isNotFromConsole )
            {
                cout << "Press the \"ANY\" key to quit" << endl << flush;
                waitForKeyboardHit();
            }
#endif
            return 1;
        }

        cout << "Acceleration value 0 is ( " << measure.x << ", " << measure.y << ", " << measure.z
             << " ) => norm is "
             << sqrt( pow( measure.x, 2.f ) + pow( measure.y, 2.f ) + pow( measure.z, 2.f ) ) << " ms^-2"
             << endl;

        err = ftkGetAccelerometerData( lib, sn, 1u, &measure );

        if ( err != ftkError::FTK_OK )
        {
            ftkGetLastErrorString( lib, 1024u, tmp );
            cerr << tmp << endl;
            cerr << "Cannot get acceleration value 1" << endl;
#ifdef ATR_WIN
            if ( isNotFromConsole )
            {
                cout << "Press the \"ANY\" key to quit" << endl << flush;
                waitForKeyboardHit();
            }
#endif
            continue;
        }

        cout << "Acceleration value 1 is ( " << measure.x << ", " << measure.y << ", " << measure.z
             << " ) => norm is "
             << sqrt( pow( measure.x, 2.f ) + pow( measure.y, 2.f ) + pow( measure.z, 2.f ) ) << " ms^-2"
             << endl;
    }

    cout << endl << "\tSUCCESS" << endl;

    ftkClose( &lib );

#ifdef ATR_WIN
    if ( isNotFromConsole )
    {
        cout << "Press the \"ANY\" key to quit" << endl << flush;
        waitForKeyboardHit();
    }
#endif

    return 0;
}
