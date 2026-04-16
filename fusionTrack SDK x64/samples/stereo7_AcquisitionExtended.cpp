// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo7_AcquisitionExtended.cpp
 *   \brief Demonstrates how to use the ftkFrameQueryExt structure to retrieve
 *          extra information.
 *
 *   This sample aims to present the following driver features:
 *   - Open/close the driver
 *   - Enumerate devices
 *   - Get total number of detected raw data and 3D fiducials in the scene.
 *
 *   How to compile this example:
 *   - Follow instructions in README.txt
 *
 *   How to run this example:
 *   - Install the fusionTrack/spryTrack driver (see documentation)
 *   - Switch on device
 *   - Run the resulting executable
 *   - Expose a marker or a set of fiducials in front of the fusionTrack
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
    const bool isNotFromConsole = isLaunchedFromExplorer();

    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << "This is a demonstration how to get additional information on the current frame such as the real "
            "number of reconstructed 3D fiducials,even if no storage has been reserved for the 3D fiducials."
         << endl;

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

    DeviceData device( retrieveLastDevice( lib, true, false, !isNotFromConsole ) );
    uint64 sn( device.SerialNumber );

    map< string, uint32 > options{};

    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &options ) );
    if ( options.empty() )
    {
        error( "Cannot retrieve any options.", !isNotFromConsole );
    }

    // ------------------------------------------------------------------------
    // When using a spryTrack, this code will not work when embedding processing
    // is enabled. (The raw data are never sent).
    if ( ftkDeviceType::DEV_SPRYTRACK_180 == device.Type || ftkDeviceType::DEV_SPRYTRACK_300 == device.Type )
    {
        bool neededOptionsPresent( true );
        string errMsg( "Could not find needed option(s):" );
        for ( const string& item : { "Enable embedded processing", "Enable images sending" } )
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
        cout << "Disable onboard processing" << endl;
        if ( ftkSetInt32( lib, sn, options[ "Enable embedded processing" ], 0 ) != ftkError::FTK_OK )
        {
            cerr << "Cannot process data on the host." << endl;
            checkError( lib, !isNotFromConsole );
        }

        cout << "Enable images sending" << endl;
        if ( ftkSetInt32( lib, sn, options[ "Enable images sending" ], 1 ) != ftkError::FTK_OK )
        {
            error( "Cannot enable images sending on the SpryTrack.", !isNotFromConsole );
        }
    }

    ftkFrameQuery* frame = ftkCreateFrame();

    if ( frame == 0 )
    {
        error( "Cannot create frame instance", !isNotFromConsole );
    }

    err = ftkSetFrameOptions( true, true, 0u, 0u, 0u, 0u, frame );

    if ( err != ftkError::FTK_OK )
    {
        ftkDeleteFrame( frame );
        error( "Cannot initialise frame", !isNotFromConsole );
    }

    err = ftkGetLastFrame( lib, sn, frame, 100u );

    while ( err > ftkError::FTK_OK )
    {
        err = ftkGetLastFrame( lib, sn, frame, 100u );
    }

    uint32 value;

    err = ftkGetTotalObjectNumber( lib, sn, ftkDataType::LeftRawData, &value );

    if ( err == ftkError::FTK_OK )
    {
        cout << "The left camera has detected " << value << " raw data." << endl;
    }
    else
    {
        cerr << "Problem getting the total number of raw data for the left camera" << endl;
    }

    err = ftkGetTotalObjectNumber( lib, sn, ftkDataType::RightRawData, &value );

    if ( err == ftkError::FTK_OK )
    {
        cout << "The right camera has detected " << value << " raw data." << endl;
    }
    else
    {
        cerr << "Problem getting the total number of raw data for the right camera" << endl;
    }

    err = ftkGetTotalObjectNumber( lib, sn, ftkDataType::FiducialData, &value );

    if ( err == ftkError::FTK_OK )
    {
        cout << "The device detected " << value << " 3D fiducials." << endl;
    }
    else
    {
        cerr << "Problem getting the total number of 3D fiducials" << endl;
    }

    cout << "\tSUCCESS" << endl;

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
