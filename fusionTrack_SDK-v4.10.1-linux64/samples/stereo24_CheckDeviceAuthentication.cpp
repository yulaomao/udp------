// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo24_CheckDeviceAuthentication.cpp
 *   \brief Demonstrates how to check device authentication.
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
#include <TrackingSystem.hpp>

#include <algorithm>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

using namespace atracsys;
using namespace std;

int main( int argc, char* argv[] )
{
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
        cout << setw( 30u ) << "[-l/--load F] " << flush << "Sets the key file to load. Type = std::string"
             << endl;
        cout << setw( 30u ) << "[-k/--key S] " << flush << "Sets the key to load. Type = std::string" << endl;
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

    string cfgFile( "" );

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-c" || val == "--config"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        cfgFile = *pos;
    }

    string key( "" );

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-k" || val == "--key"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        key = *pos;
    }

    if ( !key.empty() && !fileToLoad.empty() )
    {
        cerr << "Both key and key file provided, key will be used" << endl;
        fileToLoad = "";
    }
    else if ( key.empty() && fileToLoad.empty() )
    {
        cerr << "Neither a key nor a key file provided, aborting" << endl;
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

    ftkBuffer buffer;
    buffer.reset();

    if ( !fileToLoad.empty() )
    {
        ifstream file( fileToLoad, ifstream::ate | ifstream::binary );
        if ( !file.is_open() || file.fail() )
        {
            cerr << "Cannot open file " << fileToLoad << endl;
            return 1;
        }

        streamsize fileSize( file.tellg() );
        const streamsize expectedSize( 16u );
        if ( fileSize != expectedSize )
        {
            cerr << "The key must be 128 bits, i.e. 16 bytes. But the file "
                 << " is " << key.length() << " bytes long" << endl;
            return 1;
        }

        file.seekg( 0, file.beg );

        file.read( buffer.data, 16u );
        buffer.size = 16u;
    }
    else if ( key.length() != 32u )
    {
        cerr << "The key must be 128 bits, i.e. 16 bytes. This corresponds"
             << " to 32 hex digits (aka nibbles), " << key.length() << " were provided instead" << endl;
        return 1;
    }
    else
    {
        char* bytePtr( buffer.data );
        for ( size_t i( 0u ); i < 32u; i += 2u, ++bytePtr )
        {
            try
            {
                *bytePtr = char( stol( key.substr( i, 2u ), nullptr, 16 ) );
            }
            catch ( invalid_argument& e )
            {
                cerr << "Invalid byte: 0x" << key.substr( i, 2u ) << endl;
                cerr << e.what() << endl;
                return 1;
            }
            catch ( out_of_range& e )
            {
                cerr << "Invalid byte: 0x" << key.substr( i, 2u ) << endl;
                cerr << e.what() << endl;
                return 1;
            }
            ++buffer.size;
        }
    }

    if ( trSystem.setDataOption( "Set cypher key", buffer ) != Status::Ok )
    {
        if ( trSystem.streamLastError() != Status::Ok )
        {
            cerr << "Cannot retrieve errors" << endl;
        }
        return 1;
    }

    cout << "Successfully set key" << endl;

    default_random_engine generator;
    uniform_int_distribution< uint64_t > distribution;

    uint64_t value( distribution( generator ) );
    buffer.size = sizeof( uint64_t );
    memcpy( buffer.data, &value, sizeof( uint64_t ) );

    if ( trSystem.setDataOption( "Request challenge", buffer ) != Status::Ok )
    {
        if ( trSystem.streamLastError() != Status::Ok )
        {
            cerr << "Cannot retrieve errors" << endl;
        }
        return 1;
    }

    cout << "Successfully requested challenge" << endl;

    int32 result( -1 );

    if ( trSystem.getIntOption( "Challenge result", result ) != Status::Ok )
    {
        if ( trSystem.streamLastError() != Status::Ok )
        {
            cerr << "Cannot retrieve errors" << endl;
        }
        return 1;
    }

    if ( result == 1 )
    {
        cout << "Device successfully authenticated" << endl;
    }
    else
    {
        cerr << "Device authentication failed" << endl;
    }

    return ( result == 1 ) ? 0 : 1;
}
