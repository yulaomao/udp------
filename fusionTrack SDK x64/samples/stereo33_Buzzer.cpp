#include <TrackingSystem.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

bool playSound( atracsys::TrackingSystem& tracker, const int32 period );

int main( int argc, char* argv[] )
{
    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    deque< string > args{};
    for ( int i( 0 ); i < argc; ++i )
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
        cout << setw( 30u ) << "[-a/--ascend]" << flush << "Play ascending scale (default)" << endl;
        cout << setw( 30u ) << "[-d/--descend]" << flush << "Play descending scale" << endl;
        cout << setw( 30u ) << "[--major]" << flush << "Play major scale (default)" << endl;
        cout << setw( 30u ) << "[--minor]" << flush << "Play minor scale" << endl;
    }

    cout << "Copyright (c) Atracsys LLC 2021" << endl;
    if ( showHelp )
    {
        atracsys::continueOnUserInput();
        return 0;
    }

    auto pos( find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-c" || val == "--config"; } ) );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        cfgFile = *pos;
    }

    bool ascendingScale( true ), readScale( find_if( args.cbegin(), args.cend(), []( const string&val ) {
                                                return val == "-a" || val == "--ascend";
                                            } ) != args.cend() );
    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-d" || val == "--descend"; } );
    if ( pos != args.cend() )
    {
        ascendingScale = false;
        if ( readScale )
        {
            cerr << "Could not specify ascending and descendig scale at the same time" << endl;
            atracsys::continueOnUserInput();
            return 1;
        }
    }

    bool majorTone( true );
    bool moodRead( find_if( args.cbegin(), args.cend(), []( const string& val ) {
                       return val == "--major";
                   } ) != args.cend() );
    pos = find_if( args.cbegin(), args.cend(), []( const string& val ) { return val == "--minor"; } );
    if ( pos != args.cend() )
    {
        majorTone = false;
        if ( moodRead )
        {
            cerr << "Could not specify minor and major scale at the same time" << endl;
            atracsys::continueOnUserInput();
            return 1;
        }
    }

    /*
     * Initialisation sequence: first the library, then retrieving the device.
     */
    atracsys::TrackingSystem wrapper( 100u, false );

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

    vector< atracsys::DeviceInfo > devices;

    retCode = wrapper.getEnumeratedDevices( devices );
    if ( retCode != atracsys::Status::Ok )
    {
        wrapper.streamLastError();
        atracsys::reportError( "No devices detected" );
    }

    cout << "Will try to play a" << ( ascendingScale ? "n ascending" : " descending" ) << " "
         << ( majorTone ? "major" : "minor" ) << " scale" << endl;

    retCode = wrapper.setIntOption( "Buzzer duration", 20 );
    if ( retCode != atracsys::Status::Ok )
    {
        wrapper.streamLastError();
        atracsys::reportError( "No buzzer options available" );
    }

    const double basePeriod( 340. );
    deque< double > factors{};

    const double semiTone( pow( 2., 1. / 12. ) );

    if ( majorTone )  // major scale
    {
        factors = { 1., 2., 2., 2., 1., 2., 2. };
    }
    else  // minor scale
    {
        factors = { 2., 2., 1., 2., 2., 1., 2. };
    }

    deque< double > octave{};
    octave.resize( factors.size() + 1u );

    size_t note( 0u );

    octave[ note ] = basePeriod;

    for ( auto factorIt( factors.cbegin() ); factorIt != factors.cend(); ++factorIt, ++note )
    {
        octave[ note + 1u ] = octave[ note ] * pow( semiTone, *factorIt );
    }

    if ( ascendingScale )
    {
        reverse( octave.begin(), octave.end() );
    }

    deque< int32 > scale{};
    scale.resize( octave.size() );

    transform( octave.cbegin(), octave.cend(), scale.begin(), []( const double val ) {
        return static_cast< uint32 >( round( val ) );
    } );

    for ( const int32 value : scale )
    {
        if ( !playSound( wrapper, value ) )
        {
            wrapper.streamLastError();
            atracsys::continueOnUserInput();
            return 1;
        }
    }

    atracsys::continueOnUserInput();
    return 0;
}

bool playSound( atracsys::TrackingSystem& tracker, const int32 period )
{
    this_thread::sleep_for( chrono::seconds( 3u ) );
    if ( tracker.setIntOption( "Buzzer period", period ) != atracsys::Status::Ok )
    {
        return false;
    }
    if ( tracker.setIntOption( "Buzzer repetition", 1 ) != atracsys::Status::Ok )
    {
        return false;
    }

    return true;
}
