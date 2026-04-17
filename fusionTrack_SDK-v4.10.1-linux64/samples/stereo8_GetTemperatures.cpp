// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo8_GetTemperatures.cpp
 *   \brief Demonstrate how to get temperatures.
 *
 *   This sample aims to present the following driver features:
 *   - Open/close the driver
 *   - Enumerate devices
 *   - Get the different temperatures in two different ways.
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
#include <ftkPlatform.h>

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

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

    cout << "This is a demonstration on how to get the temperatures." << endl;

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

    map< string, uint32 > temperaturesOptions{};

    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &temperaturesOptions ) );

    if ( err != ftkError::FTK_OK )
    {
        checkError( lib, !isNotFromConsole );
    }
    if ( temperaturesOptions.empty() )
    {
        cerr << "Could not retrieve any options." << endl;
        checkError( lib, !isNotFromConsole );
    }

    uint32 optId;
    float32 temperature, refTemperature;

    ftkFrameQuery* frame( ftkCreateFrame() );
    err = ftkSetFrameOptions( false, 10u, 0u, 0u, 0u, 0u, frame );

    long waitTime( 2000L );
    cout << "Waiting for " << double( waitTime ) / 1000. << " seconds" << endl;
    sleep( waitTime );

    err = ftkGetLastFrame( lib, sn, frame, 100u );

    string mess;
    char tmp[ 1024u ];
    ErrorReader errReader;

    /*
     * Direct access, not meant to be called repeatedly, as setting the
     * wanted ID and reading the value reprensents actually 2 option accesses
     * for one temperature.
     * Please note this is the way to access the reference temperatures (if needed).
     */

    bool neededOptionPresent( true );
    string errMsg( "Missing option(s):" );
    for ( const string& optName :
          { "Temperature sensor location", "Temperature sensor index", "Temperature sensor value" } )
    {
        if ( temperaturesOptions.find( optName ) == temperaturesOptions.cend() )
        {
            errMsg += " '" + optName + "'";
            neededOptionPresent = false;
        }
    }

    if ( !neededOptionPresent )
    {
        error( errMsg.c_str() );
    }

    for ( int32 i( 1 ); i < 0x1FFF; i <<= 1 )
    {
        cout << "--------------------------------------------------" << endl;

        optId = temperaturesOptions[ "Temperature sensor index" ];
        err = ftkSetInt32( lib, sn, optId, i );
        if ( err != ftkError::FTK_OK )
        {
            ftkGetLastErrorString( lib, 1024u, tmp );
            if ( !errReader.parseErrorString( tmp ) )
            {
                cerr << "Cannot interpret error string:" << endl << tmp << endl;
                continue;
            }
            else if ( errReader.hasError( ftkError::FTK_ERR_INV_OPT_VAL ) )
            {
                cout << "Invalid temperature sensor index: " << i << endl;
                continue;
            }
            else
            {
                cerr << tmp << endl;
            }
            continue;
        }

        optId = temperaturesOptions[ "Temperature sensor location" ];
        err = ftkGetData( lib, sn, optId, &buffer );
        if ( err != ftkError::FTK_OK )
        {
            ftkGetLastErrorString( lib, 1024u, tmp );
            cerr << tmp << endl;
            cerr << "Problem when getting temperature sensor name" << endl;
            continue;
        }

        optId = temperaturesOptions[ "Temperature sensor value" ];
        err = ftkGetFloat32( lib, sn, optId, &temperature, ftkOptionGetter::FTK_VALUE );
        if ( err != ftkError::FTK_OK )
        {
            ftkGetLastErrorString( lib, 1024u, tmp );
            cerr << tmp << endl;
            cerr << "Problem when getting temperature sensor value" << endl;
            continue;
        }

        optId = temperaturesOptions[ "Temperature sensor reference" ];
        err = ftkGetFloat32( lib, sn, optId, &refTemperature, ftkOptionGetter::FTK_VALUE );
        if ( err != ftkError::FTK_OK )
        {
            ftkGetLastErrorString( lib, 1024u, tmp );
            cerr << tmp << endl;
            cerr << "Problem when getting reference temperature sensor value" << endl;
            continue;
        }

        cout << "Sensor id " << i << " is \"" << buffer.uData << "\" and measured temperature is "
             << temperature << " and reference is " << refTemperature << " degree Celsius" << endl;
    }

    /*
     * Preferred access, using the corresponding in-frame event.
     */
    const ftkEvent* evt( nullptr );
    const EvtTemperatureV4Payload* tempData( nullptr );
    for ( uint32_t iEvt( 0u ); iEvt < frame->eventsCount; ++iEvt )
    {
        evt = frame->events[ iEvt ];
        if ( evt->Type == FtkEventType::fetTempV4 )
        {
            tempData = reinterpret_cast< EvtTemperatureV4Payload* >( evt->Data );
            const size_t nTempSensors( evt->Payload / sizeof( EvtTemperatureV4Payload ) );
            for ( size_t iTemp( 0u ); iTemp < nTempSensors; ++iTemp, ++tempData )
            {
                cout << "Sensor id is " << tempData->SensorId << " and measured temperature is "
                     << tempData->SensorValue << " degree Celsius" << endl;
            }
        }
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
