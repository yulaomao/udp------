// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo21_MultipleAcquisition.cpp
 *   \brief Demonstrates how to get data from several trackers.
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
#include "geometryHelper.hpp"
#include "helpers.hpp"

#include <AdditionalDataStructures.hpp>
#include <TrackingSystem.hpp>

#include <ftkInterface.h>

#include <array>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

// ---------------------------------------------------------------------------

int main( int argc, char** argv )
{
    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << "This is a demonstration on how to use multiple devices." << endl;

    vector< string > args;
    for ( int i( 1 ); i < argc; ++i )
    {
        args.push_back( argv[ i ] );
    }

    bool showHelp( false );

    if ( !args.empty() )
    {
        showHelp = ( find_if( args.cbegin(), args.cend(), []( const string& val ) {
                         return val == "-h" || val == "--help";
                     } ) != args.cend() );
    }

    string cfgFile( "" );
    uint32 framesToGet( 100u );
    deque< string > geomFiles;

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-c/--config F] " << flush << "JSON config file. Type "
             << "std::string, default = " << cfgFile << endl;
        cout << setw( 30u ) << "[-n/--number N] " << flush << "Number of frames to acquire. Type "
             << "uint32, default = " << framesToGet << endl;
        cout << setw( 30u ) << "[-g/--geometry F] " << flush << "Geometry file(s) to use. Type "
             << "std::string, default = none" << endl;
    }

    cout << "Copyright (c) Atracsys LLC 2021" << endl;
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
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-n" || val == "--number"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        try
        {
            framesToGet = stoul( *pos );
        }
        catch ( invalid_argument& e )
        {
            cerr << e.what() << endl;
            atracsys::reportError( "Cannot init. nb frames per measure." );
        }
        catch ( out_of_range& e )
        {
            cerr << e.what() << endl;
            atracsys::reportError( "Cannot init. nb frames per measure." );
        }
    }
    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-g" || val == "--geometry"; } );
    if ( pos != args.cend() && ++pos != args.cend() && pos->substr( 0u, 0u ) != "-" )
    {
        geomFiles.push_back( *pos );
    }

    /*
     * Initialisation sequence: first the library, then retrieving the device.
     */
    atracsys::TrackingSystem wrapper;

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

    vector< atracsys::DeviceInfo > getEnumeratedDevices;

    retCode = wrapper.getEnumeratedDevices( getEnumeratedDevices );
    if ( retCode != atracsys::Status::Ok )
    {
        atracsys::reportError( "No devices detected" );
    }

    for ( const auto& file : geomFiles )
    {
        retCode = wrapper.setGeometry( file );
        if ( retCode != atracsys::Status::Ok )
        {
            cerr << "Cannot load geometry file " << file << endl;
        }
    }

    retCode = wrapper.createFrames( false, 10u, 20u, 10u, 4u );
    if ( retCode != atracsys::Status::Ok )
    {
        atracsys::reportError( "Cannot allocate frames" );
    }

    uint32 gottenFrames( 0u ), i( 0u );
    vector< atracsys::FrameData > niceFrames;

    while ( ++gottenFrames < framesToGet )
    {
        i = 0u;

        cout << "  ----------------------------- " << endl << endl;
        cout << "Frame " << gottenFrames << endl;

        retCode = wrapper.getLastFrames( 100u, niceFrames );

        if ( retCode != atracsys::Status::Ok )
        {
            retCode = wrapper.streamLastError( cerr );
            break;
        }

        for ( auto frame : niceFrames )
        {
            cout << "Device " << i++ << endl;
            if ( frame.header().valid() )
            {
                cout << "Pic counter  : " << frame.header().counter() << endl;
                cout << "Pic timestamp: " << frame.header().timestamp() << endl;
                cout << "Strobe L/R: " << ( frame.header().synchronisedStrobeLeft() ? 0 : 1 ) << "/"
                     << ( frame.header().synchronisedStrobeRight() ? 0 : 1 ) << endl;
                if ( !geomFiles.empty() )
                {
                    cout << endl;
                    for ( const auto& marker : frame.markers() )
                    {
                        cout << "Marker " << marker.index() << endl;
                        cout << "\t(" << marker.position()[ 0u ] << ", " << marker.position()[ 1u ] << ", "
                             << marker.position()[ 2u ] << ")" << endl;
                        for ( uint32 fidIt( 0u ); fidIt < FTK_MAX_FIDUCIALS; ++fidIt )
                        {
                            const auto& theFid = marker.correspondingFiducial( fidIt );
                            if ( theFid.valid() )
                            {
                                cout << "\tFiducial " << fidIt << endl;
                                cout << "\t\t(" << theFid.position()[ 0u ] << ", " << theFid.position()[ 1u ]
                                     << ", " << theFid.position()[ 2u ] << ")" << endl;
                                const auto& leftRaw = theFid.leftInstance();
                                if ( leftRaw.valid() )
                                {
                                    cout << "\t\tLeft blob " << leftRaw.index() << endl;
                                    cout << "\t\t\t(" << leftRaw.position()[ 0u ] << ", "
                                         << leftRaw.position()[ 1u ] << ")" << endl;
                                }
                                else
                                {
                                    cout << "\t\tNo left blob" << endl;
                                }
                                const auto& rightRaw = theFid.rightInstance();
                                if ( rightRaw.valid() )
                                {
                                    cout << "\t\tLeft blob " << rightRaw.index() << endl;
                                    cout << "\t\t\t(" << rightRaw.position()[ 0u ] << ", "
                                         << rightRaw.position()[ 1u ] << ")" << endl;
                                }
                                else
                                {
                                    cout << "\t\tNo left blob" << endl;
                                }
                            }
                            else
                            {
                                cout << "\tNo fiducial " << fidIt << endl;
                            }
                        }
                    }
                }
            }
            else
            {
                cerr << "Error getting frame image header" << endl;
            }
            cout << endl;
        }
    }

    atracsys::continueOnUserInput();

    return 0;
}
