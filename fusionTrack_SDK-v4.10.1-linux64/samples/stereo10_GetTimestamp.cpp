// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo10_GetTimestamp.cpp
 *   \brief Demonstrate how to get RTC timestamp.
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

    cout << "This is a demonstration on how to get the RTC timestamp." << endl;

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

    DeviceData device( retrieveLastDevice( lib, true, false, !isNotFromConsole ) );
    uint64 sn( device.SerialNumber );

    uint64 ts;
    ftkError err;

    for ( uint32 i( 0u ); i < 2u; ++i )
    {
        err = ftkGetRealTimeClock( lib, sn, &ts );

        if ( err == ftkError::FTK_OK )
        {
            cout << "Timestamp is " << ts << endl;
        }
        else
        {
            checkError( lib, !isNotFromConsole );
        }

        if ( i < 1u )
        {
            cout << "Sleeping 5 seconds" << endl;
            sleep( 5000L );
        }
    }

    cout << "\tSUCCESS" << endl;

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
