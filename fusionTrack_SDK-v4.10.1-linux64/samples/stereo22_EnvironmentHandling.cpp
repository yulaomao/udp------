// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo22_EnvironmentHandling.cpp
 *   \brief Demonstrates how to save and load environments.
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
// =============================================================================
#include <TrackingSystem.hpp>

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <string>

using namespace atracsys;
using namespace std;

int main( int argc, char* argv[] )
{
    cout << "This is a demonstration on how to save and load environment using the C++ API" << endl;
    cout << "Copyright (c) Atracsys LLC 2021" << endl;

    deque< string > args;
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

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-l/--load F] " << flush
             << "Sets the environment file to load. Type = std::string" << endl;
        cout << setw( 30u ) << "[-s/--save F] " << flush
             << "Sets the environment file to save. Type = std::string" << endl;
        cout << setw( 30u ) << "[-c/--config F] " << flush << "JSON config file. Type "
             << "std::string, default = none" << endl;
    }

    if ( showHelp )
    {
        continueOnUserInput();
        return 0;
    }

    string fileToLoad( "" );

    auto pos( find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-l" || val == "--load"; } ) );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        fileToLoad = *pos;
    }

    string fileToSave( "" );

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-s" || val == "--save"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        fileToSave = *pos;
    }

    string cfgFile( "" );

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-c" || val == "--config"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        cfgFile = *pos;
    }

    if ( !fileToLoad.empty() && !fileToSave.empty() )
    {
        cerr << "Cannot specify a file to save to and to load from." << endl;
        return 1;
    }
    else if ( fileToLoad.empty() && fileToSave.empty() )
    {
        cerr << "No specifies file to save to or to load from." << endl;
        return 1;
    }

    TrackingSystem trSystem;

    if ( trSystem.initialise( cfgFile ) != Status::Ok )
    {
        if ( trSystem.streamLastError() != Status::Ok )
        {
            cerr << "Cannot retrieve errors" << endl;
        }
        return 1;
    }

    if ( trSystem.enumerateDevices() != Status::Ok )
    {
        if ( trSystem.streamLastError() != Status::Ok )
        {
            cerr << "Cannot retrieve errors" << endl;
        }
        return 1;
    }

    vector< DeviceInfo > devices;
    if ( trSystem.getEnumeratedDevices( devices ) != Status::Ok )
    {
        if ( trSystem.streamLastError() != Status::Ok )
        {
            cerr << "Cannot retrieve errors" << endl;
        }
        return 1;
    }

    if ( !fileToSave.empty() )
    {
        Status retCode( trSystem.setDataOption( "Save environment", fileToSave ) );
        if ( retCode != Status::Ok )
        {
            if ( retCode == Status::SdkError || retCode == Status::SdkWarning )
            {
                trSystem.streamLastError();
            }
            cerr << "Error writing the environment" << endl;
        }
        else
        {
            cout << "\tSUCCESS" << endl;
        }
    }
    else
    {
        Status retCode( trSystem.setDataOption( "Load environment", fileToLoad ) );
        if ( retCode != Status::Ok )
        {
            if ( retCode == Status::SdkError || retCode == Status::SdkWarning )
            {
                trSystem.streamLastError();
            }
            cerr << "Error loading the environment" << endl;
        }
        else
        {
            cout << "\tSUCCESS" << endl;
        }
    }

    return 0;
}
