// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo3_AcquisitionAdvanced.cpp
 *   \brief Demonstrate how to get marker’s position and orientation,
 *          fiducials positions and the raw data.
 *
 *   NOTE  THAT THIS SAMPLE WILL RETURN NO OUTPUT IF YOU DO NOT
 *   EDIT AND RECOMPILE WITH YOUR OWN GEOMETRY!
 *
 *   This sample aims to present the following driver features:
 *   - Open/close the driver
 *   - Enumerate devices
 *   - Set a specific geometry
 *   - Display different data from the frame
 *
 *   How to compile this example:
 *   - Follow instructions in README.txt
 *
 *   How to run this example:
 *   - Install the fusionTrack/spryTrack driver (see documentation)
 *   - Switch on device
 *   - Run the resulting executable
 *   - Expose a marker with the correct geometry in front of the fusionTrack/spryTrack, by
 *     default the geometry072.ini file is loaded.
 *
 */
// =============================================================================

#include "geometryHelper.hpp"
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

    cout << "This is a demonstration on how to get the marker’s position and orientation, the fiducials "
            "positions and the raw data."
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

    deque< string > geomFiles{ "C:/Users/32158/Desktop/geometry020.ini" };

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-c/--config F] " << flush << "JSON config file. Type "
             << "std::string, default = none" << endl;
        cout << setw( 30u ) << "[-g/--geom F (F ...)] " << flush << "geometry file(s) to load, default = ";
        for ( const auto& file : geomFiles )
        {
            cout << file << " ";
        }
        cout << endl;
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
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-g" || val == "--geom"; } );
    if ( pos != args.cend() )
    {
        geomFiles.clear();
        while ( pos != args.cend() && ++pos != args.cend() )
        {
            if ( pos->substr( 0u, 1u ) == "-" )
            {
                break;
            }
            geomFiles.emplace_back( *pos );
        }
    }

    // ------------------------------------------------------------------------
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

    map< string, uint32 > options{};

    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &options ) );
    if ( options.empty() )
    {
        error( "Cannot retrieve any options.", !isNotFromConsole );
    }

    // ------------------------------------------------------------------------
    // When using a spryTrack, onboard processing of the images is preferred.
    // Sending of the images is disabled so that the sample operates on a USB2
    // connection
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
        cout << "Enable onboard processing" << endl;
        if ( ftkSetInt32( lib, sn, options[ "Enable embedded processing" ], 1 ) != ftkError::FTK_OK )
        {
            cerr << "Cannot process data directly on the SpryTrack." << endl;
            checkError( lib, !isNotFromConsole );
        }

        cout << "Disable images sending" << endl;
        if ( ftkSetInt32( lib, sn, options[ "Enable images sending" ], 0 ) != ftkError::FTK_OK )
        {
            cerr << "Cannot disable images sending on the SpryTrack." << endl;
            checkError( lib, !isNotFromConsole );
        }
    }

    // ----------------------------------------------------------------------
    // Set geometry

    ftkRigidBody geom{};
    for ( const auto& geomFile : geomFiles )
    {
        switch ( loadRigidBody( lib, geomFile, geom ) )
        {
        case 1:
            cout << "Loaded from installation directory." << endl;

        case 0:
            if ( ftkError::FTK_OK != ftkSetRigidBody( lib, sn, &geom ) )
            {
                checkError( lib, !isNotFromConsole );
            }
            break;

        default:

            cerr << "Error, cannot load geometry file '" << geomFile << "'." << endl;
            if ( ftkError::FTK_OK != ftkClose( &lib ) )
            {
                checkError( lib, !isNotFromConsole );
            }
        }
    }

    // ----------------------------------------------------------------------
    // Initialize the frame to get marker pose

    ftkFrameQuery* frame = ftkCreateFrame();

    if ( frame == 0 )
    {
        cerr << "Cannot create frame instance" << endl;
        checkError( lib, !isNotFromConsole );
    }

    err = ftkSetFrameOptions( false, false, 128u, 128u, 4u * FTK_MAX_FIDUCIALS, 4u, frame );

    if ( err != ftkError::FTK_OK )
    {
        ftkDeleteFrame( frame );
        cerr << "Cannot initialise frame" << endl;
        checkError( lib, !isNotFromConsole );
    }

    cout.setf( std::ios::fixed, std::ios::floatfield );

    uint32 counter( 10u );

    for ( uint32 u( 0u ); u < 100u; ++u )
    {
        if ( ftkGetLastFrame( lib, sn, frame, 100u ) > ftkError::FTK_OK )
        /* block until next frame is available */
        {
            cout << ".";
            continue;
        }
        else if ( err == ftkError::FTK_WAR_TEMP_INVALID )
        {
            cout << "temperature warning" << endl;
        }
        else if ( err < ftkError::FTK_OK )
        {
            cout << "warning: " << int32( err ) << endl;
            if ( err == ftkError::FTK_WAR_NO_FRAME )
            {
                continue;
            }
        }

        switch ( frame->markersStat )
        {
        case ftkQueryStatus::QS_WAR_SKIPPED:
            ftkDeleteFrame( frame );
            cerr << "marker fields in the frame are not set correctly" << endl;
            checkError( lib, !isNotFromConsole );

        case ftkQueryStatus::QS_ERR_INVALID_RESERVED_SIZE:
            ftkDeleteFrame( frame );
            cerr << "frame -> markersVersionSize is invalid" << endl;
            checkError( lib, !isNotFromConsole );

        default:
            ftkDeleteFrame( frame );
            cerr << "invalid status" << endl;
            checkError( lib, !isNotFromConsole );

        case ftkQueryStatus::QS_OK:
            break;
        }

        if ( frame->markersCount == 0u )
        {
            cout << ".";
            sleep( 1000L );
            continue;
        }

        if ( frame->markersStat == ftkQueryStatus::QS_ERR_OVERFLOW )
        {
            cerr << "WARNING: marker overflow. Please increase cstMarkersCount" << endl;
        }

        for ( size_t m = 0; m < frame->markersCount; m++ )
        {
            cout.precision( 3u );
            cout << "geometry " << frame->markers[ m ].geometryId << ", error "
                 << frame->markers[ m ].registrationErrorMM << ", tracking ID " << frame->markers[ m ].id
                 << endl;

            for ( size_t n = 0; n < FTK_MAX_FIDUCIALS; n++ )
            {
                uint32 fidId = frame->markers[ m ].fiducialCorresp[ n ];
                if ( fidId != INVALID_ID && frame->threeDFiducialsStat == ftkQueryStatus::QS_OK &&
                     fidId < frame->threeDFiducialsCount )
                {
                    // Fiducials are displayed with geometry ids
                    ftk3DFiducial& fid = frame->threeDFiducials[ fidId ];
                    cout.precision( 2u );
                    cout << "\tfiducial " << n << ", xyz " << fid.positionMM.x << " " << fid.positionMM.y
                         << " " << fid.positionMM.z << endl;

                    if ( frame->imageLeftStat == ftkQueryStatus::QS_OK &&
                         fid.leftIndex < frame->rawDataLeftCount )
                    {
                        ftkRawData& left = frame->rawDataLeft[ fid.leftIndex ];
                        cout << "\t\tleft,  image " << left.centerXPixels << " " << left.centerYPixels
                             << endl;
                    }
                    if ( frame->imageRightStat == ftkQueryStatus::QS_OK &&
                         fid.rightIndex < frame->rawDataRightCount )
                    {
                        ftkRawData& right = frame->rawDataRight[ fid.rightIndex ];
                        cout << "\t\tright, image " << right.centerXPixels << " " << right.centerYPixels
                             << endl;
                    }
                }
            }
        }

        if ( --counter == 0uLL )
        {
            break;
        }

        // Check for other marker fields to get rotation, fiducial id, etc.
        sleep( 1000L );
    }

    if ( counter != 0u )
    {
        cout << endl << "Acquisition loop aborted after too many vain trials" << endl;
    }
    else
    {
        cout << endl << "\tSUCCESS" << endl;
    }

    // ----------------------------------------------------------------------
    // Close driver

    ftkDeleteFrame( frame );

    if ( ftkError::FTK_OK != ftkClose( &lib ) )
    {
        checkError( lib, !isNotFromConsole );
    }

#ifdef ATR_WIN
    if ( isNotFromConsole )
    {
        cout << endl << "*** Hit a key to exit ***" << endl;
        waitForKeyboardHit();
    }
#endif

    return EXIT_SUCCESS;
}
