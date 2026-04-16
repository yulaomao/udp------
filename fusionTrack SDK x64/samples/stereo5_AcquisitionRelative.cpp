// ============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo5_AcquisitionRelative.cpp
 *   \brief Demonstrate how to use one marker as reference for the transformation
 *          of a second one.
 *
 *   This sample aims to present the following driver features:
 *   - Open/close the driver
 *   - Enumerate devices
 *   - Get the relative position of a marker with respect to another one.
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

#include "geometryHelper.hpp"
#include "helpers.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

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

    cout << "This is a demonstration on how to use one marker as reference for the transformation of a "
            "second one. "
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
    string outFile( "" );
    deque< string > geomFiles;

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-c/--config F] " << flush << "JSON config file. Type "
             << "std::string, default = none" << endl;
        cout << setw( 30u ) << "[-g/--geom F F] " << flush
             << "geometry files to load (2), default = none. First geometry "
             << "will be used as the reference marker" << endl;
        cout << setw( 30u ) << "[-o/--output] " << flush << "output logfile, default = none" << endl;
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
    while ( pos != args.cend() && ++pos != args.cend() )
    {
        if ( pos->substr( 0u, 1u ) == "-" )
        {
            break;
        }
        geomFiles.emplace_back( *pos );
    }

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-o" || val == "--output"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        outFile = *pos;
    }

    if ( geomFiles.size() != 2u )
    {
        cerr << "Two geometry files are expected, but " << geomFiles.size() << " were given" << endl;
#ifdef ATR_WIN
        if ( isLaunchedFromExplorer() )
        {
            cout << "Press the \"ANY\" key to quit" << endl;
            waitForKeyboardHit();
        }
#endif
        return 1;
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

    ofstream outputFile;

    if ( !outFile.empty() )
    {
        outputFile.open( outFile );
        if ( !outputFile.is_open() || outputFile.fail() )
        {
            cerr << "Cannot create file " << outFile << " please check "
                 << "your write permissions on the containing folder" << endl;
#ifdef ATR_WIN
            cout << endl << " *** Hit a key to exit ***" << endl;
            waitForKeyboardHit();
#endif
            return 1;
        }
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
    // Set the wanted geometries

    ftkRigidBody geometryOne{};

    switch ( loadRigidBody( lib, geomFiles.at( 0u ), geometryOne ) )
    {
    case 1:
        cout << "Loaded from installation directory." << endl;

    case 0:
        if ( ftkError::FTK_OK != ftkSetRigidBody( lib, sn, &geometryOne ) )
        {
            checkError( lib, !isNotFromConsole );
        }
        break;

    default:

        cerr << "Error, cannot load geometry file '" << geomFiles.at( 0u ) << "'." << endl;
        if ( ftkError::FTK_OK != ftkClose( &lib ) )
        {
            checkError( lib, !isNotFromConsole );
        }
    }

    ftkRigidBody geometryTwo{};

    switch ( loadRigidBody( lib, geomFiles.at( 1u ), geometryTwo ) )
    {
    case 1:
        cout << "Loaded from installation directory." << endl;

    case 0:
        if ( ftkError::FTK_OK != ftkSetRigidBody( lib, sn, &geometryTwo ) )
        {
            checkError( lib, !isNotFromConsole );
        }
        break;

    default:

        cerr << "Error, cannot load geometry file '" << geomFiles.at( 1u ) << "'." << endl;
        if ( ftkError::FTK_OK != ftkClose( &lib ) )
        {
            checkError( lib, !isNotFromConsole );
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

    err = ftkSetFrameOptions( false, false, 0u, 0u, 0u, 4u, frame );

    if ( err != ftkError::FTK_OK )
    {
        ftkDeleteFrame( frame );
        cerr << "Cannot initialize frame" << endl;
        checkError( lib, !isNotFromConsole );
    }

    err = ftkGetLastFrame( lib, sn, frame, 1000 );

    uint32 counter( 20u );

    ftkMarker *markerOne = 0, *markerTwo = 0;

    uint32 i, j, k;

    floatXX translation[ 3u ];
    floatXX rotation[ 3u ][ 3u ];
    floatXX tmp;

    cout.setf( ios::fixed, ios::floatfield );
    cout.precision( 2u );

    for ( uint32 u( 0u ); u < 100u; ++u )
    {
        markerOne = 0;
        markerTwo = 0;

        if ( err == ftkError::FTK_OK )
        {
            switch ( frame->markersStat )
            {
            case ftkQueryStatus::QS_WAR_SKIPPED:
                cerr << "No markers found." << endl;
                err = ftkGetLastFrame( lib, sn, frame, 1000 );
                continue;

            case ftkQueryStatus::QS_ERR_INVALID_RESERVED_SIZE:
                cerr << "Reserved size is not a multiple of the size type." << endl;
#ifdef ATR_WIN
                cout << endl << " *** Hit a key to exit ***" << endl;
                waitForKeyboardHit();
#endif
                if ( outputFile.is_open() )
                {
                    outputFile.close();
                }
                return 1;

            case ftkQueryStatus::QS_ERR_OVERFLOW:
                cerr << "Missing data, buffer is too small." << endl;

            case ftkQueryStatus::QS_REPROCESS:
            case ftkQueryStatus::QS_OK:
                break;
            }
        }
        else
        {
            cout << ".";
        }

        for ( i = 0u; i < frame->markersCount; ++i )
        {
            if ( frame->markers[ i ].geometryId == geometryOne.geometryId )
            {
                markerOne = &( frame->markers[ i ] );
            }
            else if ( frame->markers[ i ].geometryId == geometryTwo.geometryId )
            {
                markerTwo = &( frame->markers[ i ] );
            }
        }

        if ( markerOne == 0 || markerTwo == 0 )
        {
            cerr << endl << "At least one marker is missing from data." << endl;
            sleep( 1000L );
            err = ftkGetLastFrame( lib, sn, frame, 1000 );
            continue;
        }

        for ( i = 0u; i < 3u; ++i )
        {
            tmp = 0.;
            for ( k = 0u; k < 3u; ++k )
            {
                // Since the rotation is a matrix from SO(3) the inverse
                // is the transposed matrix.
                // Therefore R^-1 * t = R^T * t
                tmp += markerOne->rotation[ k ][ i ] *
                       ( markerTwo->translationMM[ k ] - markerOne->translationMM[ k ] );
            }
            translation[ i ] = tmp;
        }

        for ( i = 0u; i < 3u; ++i )
        {
            for ( j = 0u; j < 3u; ++j )
            {
                tmp = 0.;
                for ( k = 0u; k < 3u; ++k )
                {
                    // Since the rotation is a matrix from SO(3) the inverse
                    // is the transposed matrix.
                    // Therefore R^-1 * R' = R^T * R'
                    tmp += markerOne->rotation[ k ][ i ] * markerTwo->rotation[ k ][ j ];
                }

                rotation[ i ][ j ] = tmp;
            }
        }

        cout.precision( 3u );
        cout << fixed;
        cout << "geometry " << markerOne->geometryId << " (reference marker):" << endl
             << "\tposition: ( " << markerOne->translationMM[ 0u ] << " " << markerOne->translationMM[ 1u ]
             << " " << markerOne->translationMM[ 2u ] << " )" << endl
             << "\trotation: ( ";
        cout.precision( 4u );
        cout << markerOne->rotation[ 0u ][ 0u ] << " " << markerOne->rotation[ 0u ][ 1u ] << " "
             << markerOne->rotation[ 0u ][ 2u ] << " " << markerOne->rotation[ 1u ][ 0u ] << " "
             << markerOne->rotation[ 1u ][ 1u ] << " " << markerOne->rotation[ 1u ][ 2u ] << " "
             << markerOne->rotation[ 2u ][ 0u ] << " " << markerOne->rotation[ 2u ][ 1u ] << " "
             << markerOne->rotation[ 2u ][ 2u ] << " )" << endl;

        cout.precision( 3u );
        cout << "geometry " << markerTwo->geometryId << " (probe marker):" << endl
             << "\tposition: ( " << markerTwo->translationMM[ 0u ] << " " << markerTwo->translationMM[ 1u ]
             << " " << markerTwo->translationMM[ 2u ] << " )" << endl
             << "\trotation: ( ";
        cout.precision( 4u );
        cout << markerTwo->rotation[ 0u ][ 0u ] << " " << markerTwo->rotation[ 0u ][ 1u ] << " "
             << markerTwo->rotation[ 0u ][ 2u ] << " " << markerTwo->rotation[ 1u ][ 0u ] << " "
             << markerTwo->rotation[ 1u ][ 1u ] << " " << markerTwo->rotation[ 1u ][ 2u ] << " "
             << markerTwo->rotation[ 2u ][ 0u ] << " " << markerTwo->rotation[ 2u ][ 1u ] << " "
             << markerTwo->rotation[ 2u ][ 2u ] << " )" << endl;

        cout.precision( 3u );
        cout << "geometry " << markerTwo->geometryId << " relative to geometry " << markerOne->geometryId
             << ":\n\ttranslation ( " << translation[ 0u ] << " " << translation[ 1u ] << " "
             << translation[ 2u ] << " )" << endl
             << "\trotation: ( ";
        cout.precision( 4u );
        cout << rotation[ 0u ][ 0u ] << " " << rotation[ 0u ][ 1u ] << " " << rotation[ 0u ][ 2u ] << " "
             << rotation[ 1u ][ 0u ] << " " << rotation[ 1u ][ 1u ] << " " << rotation[ 1u ][ 2u ] << " "
             << rotation[ 2u ][ 0u ] << " " << rotation[ 2u ][ 1u ] << " " << rotation[ 2u ][ 2u ] << " )"
             << endl;

        cout.precision( 3u );
        cout << "Distance between marker " << markerOne->geometryId << " and marker " << markerTwo->geometryId
             << ": "
             << sqrt( pow( markerTwo->translationMM[ 0u ] - markerOne->translationMM[ 0u ], 2.f ) +
                      pow( markerTwo->translationMM[ 1u ] - markerOne->translationMM[ 1u ], 2.f ) +
                      pow( markerTwo->translationMM[ 2u ] - markerOne->translationMM[ 2u ], 2.f ) )
             << endl;
        cout << "Length of relative position: "
             << sqrt( pow( translation[ 0u ], 2.f ) + pow( translation[ 1u ], 2.f ) +
                      pow( translation[ 2u ], 2.f ) )
             << endl
             << endl;

        cout.unsetf( ios::fixed );

        if ( outputFile.is_open() )
        {
            outputFile << "0x" << setfill( '0' ) << setw( 16u ) << hex << frame->imageHeader->timestampUS
                       << dec << " trans ( ";
            outputFile.precision( 3u );
            outputFile << fixed << translation[ 0u ] << " " << translation[ 1u ] << " " << translation[ 2u ]
                       << " ) rot ( ";
            outputFile.precision( 4u );
            outputFile << fixed << rotation[ 0u ][ 0u ] << " " << rotation[ 0u ][ 1u ] << " "
                       << rotation[ 0u ][ 2u ] << " " << rotation[ 1u ][ 0u ] << " " << rotation[ 1u ][ 1u ]
                       << " " << rotation[ 1u ][ 2u ] << " " << rotation[ 2u ][ 0u ] << " "
                       << rotation[ 2u ][ 1u ] << " " << rotation[ 2u ][ 2u ] << " )" << endl;
            outputFile.unsetf( ios::fixed );
        }

        if ( --counter == 0u )
        {
            break;
        }

        sleep( 1000L );

        err = ftkGetLastFrame( lib, sn, frame, 1000 );
    }

    if ( counter != 0u )
    {
        cout << "Acquisition loop aborted after too many vain trials" << endl;
    }

    // ----------------------------------------------------------------------
    // Close driver

    cout << "\tSUCCESS" << endl;

    ftkDeleteFrame( frame );

    if ( ftkError::FTK_OK != ftkClose( &lib ) )
    {
        checkError( lib, !isNotFromConsole );
    }

    if ( outputFile.is_open() )
    {
        outputFile.close();
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
