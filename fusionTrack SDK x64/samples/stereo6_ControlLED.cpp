// ============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo6_controlLED
 *   \brief Demonstrate how to control the user LED
 *
 *   This sample aims to present the following driver features:
 *   - Open/close the driver
 *   - Enumerate devices
 *   - Control the user LED
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
#include "helpers.hpp"

#include <ftkInterface.h>
#include <ftkPlatform.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

uint8 frequency( 0u );
uint8 blue( 0u );
uint8 red( 0u );
uint8 green( 0u );

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

int process_uint8_arg( std::string option, std::string optionName, char* argv[], int argc, int arg )
{
    if ( arg + 1 > argc )
    {
        cerr << "Wrong syntax: " << option << " option sets the " << optionName << " and requires a number."
             << endl;
#ifdef ATR_WIN
        waitForKeyboardHit();
#endif
        exit( 1 );
    }
    int result;
    stringstream convert;
    convert.str( argv[ arg ] );
    if ( convert.str().find( "0x" ) == 0 )
    {
        convert << hex;
    }
    convert >> result;
    if ( convert.fail() )
    {
        cerr << "Cannot read number from string \"" << argv[ arg ] << "\"." << endl;
#ifdef ATR_WIN
        waitForKeyboardHit();
#endif
        exit( 1 );
    }

    if ( result < 0 || result > 255 )
    {
        cerr << "Wrong syntax: " << optionName << " and requires a number between 0 and 255." << endl;
#ifdef ATR_WIN
        waitForKeyboardHit();
#endif
        exit( 1 );
    }
    return result;
}

int main( int argc, char* argv[] )
{
    const bool isNotFromConsole = isLaunchedFromExplorer();

    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << "This is a demonstration on how to control the user LED." << endl;

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
    uint16 red( 0u ), green( 0u ), blue( 0u ), frequency( 0u );

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-c/--config F] " << flush << "JSON config file. Type "
             << "std::string, default = none" << endl;
        cout << setw( 30u ) << "[-r/--red R]" << flush
             << "sets the red component, default = " << unsigned( red ) << endl;
        cout << setw( 30u ) << "[-g/--green G]" << flush
             << "sets the green component, default = " << unsigned( green ) << endl;
        cout << setw( 30u ) << "[-b/--blue B]" << flush
             << "sets the blue component, default = " << unsigned( blue ) << endl;
        cout << setw( 30u ) << "[-f/--freq F]" << flush
             << "sets the frequency, default = " << unsigned( frequency ) << endl;
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

    // ------------------------------------------------------------------------
    // Handling arguments
    // ------------------------------------------------------------------------

    auto pos( find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-c" || val == "--config"; } ) );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        cfgFile = *pos;
    }

    stringstream convert;
    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-r" || val == "--red"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        stringstream convert( *pos );
        if ( pos->substr( 0u, 2u ) == "0x" || pos->substr( 0u, 2u ) == "0X" )
        {
            convert >> hex;
        }
        if ( ( convert >> red ).fail() )
        {
            cerr << "Cannot read red component from " << *pos << endl;
#ifdef ATR_WIN
            if ( isLaunchedFromExplorer() )
            {
                cout << "Press the \"ANY\" key to quit" << endl;
                waitForKeyboardHit();
            }
#endif
            return 1;
        }
    }

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-g" || val == "--green"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        stringstream convert( *pos );
        if ( pos->substr( 0u, 2u ) == "0x" || pos->substr( 0u, 2u ) == "0X" )
        {
            convert >> hex;
        }
        if ( ( convert >> green ).fail() )
        {
            cerr << "Cannot read green component from " << *pos << endl;
#ifdef ATR_WIN
            if ( isLaunchedFromExplorer() )
            {
                cout << "Press the \"ANY\" key to quit" << endl;
                waitForKeyboardHit();
            }
#endif
            return 1;
        }
    }

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-b" || val == "--blue"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        stringstream convert( *pos );
        if ( pos->substr( 0u, 2u ) == "0x" || pos->substr( 0u, 2u ) == "0X" )
        {
            convert >> hex;
        }
        if ( ( convert >> blue ).fail() )
        {
            cerr << "Cannot read blue component from " << *pos << endl;
#ifdef ATR_WIN
            if ( isLaunchedFromExplorer() )
            {
                cout << "Press the \"ANY\" key to quit" << endl;
                waitForKeyboardHit();
            }
#endif
            return 1;
        }
    }

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-f" || val == "--freq"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        stringstream convert( *pos );
        if ( pos->substr( 0u, 2u ) == "0x" || pos->substr( 0u, 2u ) == "0X" )
        {
            convert << hex;
        }
        if ( ( convert >> frequency ).fail() )
        {
            cerr << "Cannot read frequency from " << *pos << endl;
#ifdef ATR_WIN
            if ( isLaunchedFromExplorer() )
            {
                cout << "Press the \"ANY\" key to quit" << endl;
                waitForKeyboardHit();
            }
#endif
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // Start processing
    // ------------------------------------------------------------------------

    ftkBuffer buffer;

    ftkLibrary lib( ftkInitExt( cfgFile.empty() ? nullptr : cfgFile.c_str(), &buffer ) );
    if ( lib == nullptr )
    {
        cerr << buffer.data << endl;
        cerr << "Cannot initialise library" << endl;
#ifdef ATR_WIN
        cout << endl << " *** Hit a key to exit ***" << endl;
        waitForKeyboardHit();
#endif
        return 1;
    }

    DeviceData device( retrieveLastDevice( lib, false, false, !isNotFromConsole ) );
    uint64 sn( device.SerialNumber );
    if ( sn == 0uLL )
    {
        cerr << "No device detected" << endl;
#ifdef ATR_WIN
        cout << endl << " *** Hit a key to exit ***" << endl;
        waitForKeyboardHit();
#endif
        ftkClose( &lib );
        return 1;
    }

    map< string, uint32 > options{};

    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &options ) );
    if ( options.empty() )
    {
        error( "Cannot retrieve any options.", !isNotFromConsole );
    }

    bool neededOptionsPresent( true );
    string errMsg( "Could not find needed option(s):" );
    for ( const string& item : { "Enables the user-LED",
                                 "User-LED red component",
                                 "User-LED blue component",
                                 "User-LED green component",
                                 "User-LED frequency" } )
    {
        if ( options.find( item ) == options.cend() )
        {
            errMsg += " '" + item + "'";
            neededOptionsPresent = false;
        }
    }
    if ( !neededOptionsPresent )
    {
        error( errMsg.c_str(), !isNotFromConsole );
    }

    char tmp[ 1024u ];

    bool success( true );
    success &= ( ftkError::FTK_OK == ftkSetInt32( lib, sn, options[ "Enables the user-LED" ], 1 ) );
    success &= ( ftkError::FTK_OK == ftkSetInt32( lib, sn, options[ "User-LED red component" ], red ) );
    success &= ( ftkError::FTK_OK == ftkSetInt32( lib, sn, options[ "User-LED blue component" ], blue ) );
    success &= ( ftkError::FTK_OK == ftkSetInt32( lib, sn, options[ "User-LED green component" ], green ) );
    success &= ( ftkError::FTK_OK == ftkSetInt32( lib, sn, options[ "User-LED frequency" ], frequency ) );

    if ( !success )
    {
        ftkGetLastErrorString( lib, 1024u, tmp );
        cerr << tmp << endl;
        ftkClose( &lib );

#ifdef ATR_WIN
        cout << "Press the \"ANY\" key to exit" << endl << flush;
        waitForKeyboardHit();
#endif
        return 1;
    }

    cout << endl << "\tSUCCESS" << endl;

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
