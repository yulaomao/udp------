// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo18_DumpDeviceInfo.cpp
 *   \brief Demonstrates how to get an XML dump (TestDump.xml) containing the device informations.
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

#include "helpers.hpp"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

ftkLibrary lib = NULL;
bool isNotFromConsole = true;

// ---------------------------------------------------------------------------
// main function

int main( int argc, char** argv )
{
    isNotFromConsole = isLaunchedFromExplorer();

    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif
    cout << "This is a demonstration on how to get an XML dump (TestDump.xml) containing the device "
            "informations."
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

    // ----------------------------------------------------------------------
    // Retrieve the device

    DeviceData device( retrieveLastDevice( lib, true, false, !isNotFromConsole ) );
    uint64 sn( device.SerialNumber );

    cout << "Waiting for 2 seconds" << endl;
    sleep( 2500L );

    ftkError err( ftkOpenDumpFile( lib, sn, "TestDump.xml" ) );
    if ( err != ftkError::FTK_OK )
    {
        cout << "Could not open TestDump.xml, the console must be in administrator mode to do so." << endl;
        return 1;
    }

    err = ftkDumpInfo( lib, sn );
    if ( err != ftkError::FTK_OK )
    {
        return 1;
    }

    err = ftkDumpFrame( lib, sn );
    if ( err == ftkError::FTK_WAR_NOT_SUPPORTED )
    {
        cout << "This function is currently not implemented" << endl;
    }
    else if ( err != ftkError::FTK_OK )
    {
        return 1;
    }

    err = ftkCloseDumpFile( lib, sn );
    if ( err != ftkError::FTK_OK )
    {
        return 1;
    }

    // ----------------------------------------------------------------------
    // Close driver

    if ( ftkError::FTK_OK != ftkClose( &lib ) )
    {
        checkError( lib, !isNotFromConsole );
    }
    else
    {
        cout << "TestDump.xml created" << endl;
        cout << endl << "\tSUCCESS" << endl;
    }
#ifdef ATR_WIN
    if ( isNotFromConsole )
    {
        cout << endl << "*** Hit a key to exit ***" << endl;
        waitForKeyboardHit();
    }
#endif
    return 0;
}
