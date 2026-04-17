// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo26_StrobeMode.cpp
 *   \brief Demonstrates how to change the strobe mode.
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

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

int main( int argc, char* argv[] )
{
    bool isNotFromConsole( isLaunchedFromExplorer() );

    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << "This is a demonstration on how to use strobe mode" << endl;

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
    int32_t strobeModeValue( 0 );

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] "
             << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-c/--config F] "
             << "JSON config file. Type "
             << "std::string, default = none" << endl;
        cout << setw( 30u ) << "[-s/--strobe N] "
             << "Sets the strobe mode, default = " << strobeModeValue << endl;
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

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-s" || val == "--strobe"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        int32_t tmp( -1 );
        try
        {
            tmp = stol( *pos );
            strobeModeValue = tmp;
        }
        catch ( ... )
        {
            cerr << "Could not read value for strobe mode from '" << *pos << "', resetting to default."
                 << endl;
        }
    }

    // ----------------------------------------------------------------------
    // Initialize driver

    ftkBuffer buffer;

    ftkLibrary lib( ftkInitExt( cfgFile.empty() ? nullptr : cfgFile.c_str(), &buffer ) );
    if ( lib == nullptr )
    {
        cerr << buffer.data << endl;
        error( "Cannot initialize driver", !isNotFromConsole );
    }

    // ----------------------------------------------------------------------
    // Creates frame

    ftkFrameQuery* frame( ftkCreateFrame() );

    if ( frame == nullptr )
    {
        cerr << "Could not create frame" << endl;
        ftkClose( &lib );
        return 1;
    }

    // ----------------------------------------------------------------------
    // Retrieve the device

    DeviceData device( retrieveLastDevice( lib, false, false, !isNotFromConsole ) );
    uint64 sn( device.SerialNumber );

    map< string, uint32 > options{};

    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &options ) );

    if ( err != ftkError::FTK_OK )
    {
        ftkDeleteFrame( frame );
        checkError( lib, !isNotFromConsole );
    }
    else if ( options.find( "Strobe mode" ) == options.cend() )
    {
        cerr << "No `Strobe mode' option available" << endl;
        ftkDeleteFrame( frame );
        ftkClose( &lib );
        return 1;
    }

    err = ftkSetInt32( lib, sn, options[ "Strobe mode" ], strobeModeValue );

    if ( err != ftkError::FTK_OK )
    {
        ftkDeleteFrame( frame );
        checkError( lib, !isNotFromConsole );
    }

    ftkFrameInfoData helper;
    helper.WantedInformation = ftkInformationType::StrobeEnabled;

    for ( size_t i( 0u ); i < 10u; ++i )
    {
        err = ftkGetLastFrame( lib, sn, frame, 20u );
        if ( err != ftkError::FTK_WAR_NO_FRAME )
        {
            err = ftkExtractFrameInfo( frame, &helper );
            if ( err == ftkError::FTK_OK )
            {
                cout << "Frame " << frame->imageHeader->counter
                     << ": left strobe =" << helper.StrobeInfo.LeftStrobeEnabled
                     << ", right strobe = " << helper.StrobeInfo.RightStrobeEnabled << endl;
            }
        }
    }

    ftkDeleteFrame( frame );
    ftkClose( &lib );

    return 0;
}
