// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo29_ExtractCalibrationParameters.cpp
 *   \brief Demonstrates how to export calibration parameters and read them.
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
#include "geometryHelper.hpp"
#include "helpers.hpp"

#include <ftkPlatform.h>

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <numeric>
#include <string>
#include <vector>

using namespace std;

int main( int argc, char** argv )
{
    string cfgFile( "" );
    cout << "This is a demonstration on how to perform the extraction of the calibration parameters from a "
            "ftkFrameQuery instance."
         << endl;
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

    if ( showHelp )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-c/--config F] " << flush << "JSON config file. Type "
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

    // ----------------------------------------------------------------------
    // Initialize driver

    ftkBuffer buffer;

    ftkLibrary lib( ftkInitExt( cfgFile.empty() ? nullptr : cfgFile.c_str(), &buffer ) );
    if ( lib == nullptr )
    {
        cerr << buffer.data << endl;
        error( "Cannot initialize driver" );
    }

    // ----------------------------------------------------------------------
    // Retrieve the device

    DeviceData device( retrieveLastDevice( lib, false ) );
    uint64 sn( device.SerialNumber );

    // ----------------------------------------------------------------------
    // Initialize the frame

    ftkFrameQuery* frame( ftkCreateFrame() );
    if ( frame == nullptr )
    {
        ftkClose( &lib );
        return 1;
    }

    // set options for data acquisition

    map< string, uint32 > options{};

    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &options ) );

    if ( err != ftkError::FTK_OK )
    {
        checkError( lib );
    }
    else if ( options.find( "Calibration export" ) == options.cend() )
    {
        cerr << "Could not locate option \"Calibration export\"" << endl;
        ftkDeleteFrame( frame );
        ftkClose( &lib );
        return 1;
    }

    err = ftkSetInt32( lib, sn, options[ "Calibration export" ], 1 );
    if ( err != ftkError::FTK_OK )
    {
        checkError( lib );
    }

    err = ftkGetLastFrame( lib, sn, frame, 100u );
    while ( err > ftkError::FTK_OK )
    {
        err = ftkGetLastFrame( lib, sn, frame, 100u );
    }

    ftkFrameInfoData infoData;
    infoData.WantedInformation = ftkInformationType::CalibrationParameters;

    err = ftkExtractFrameInfo( frame, &infoData );
    if ( err != ftkError::FTK_OK )
    {
        checkError( lib );
    }

    cout << "Left focal : [" << infoData.Calibration.LeftCamera.FocalLength[ 0u ] << ", "
         << infoData.Calibration.LeftCamera.FocalLength[ 1u ] << "]" << endl;
    cout << "Right focal: [" << infoData.Calibration.RightCamera.FocalLength[ 0u ] << ", "
         << infoData.Calibration.RightCamera.FocalLength[ 1u ] << "]" << endl;
    cout << "Translation: [" << infoData.Calibration.Translation[ 0u ] << ", "
         << infoData.Calibration.Translation[ 1u ] << ", " << infoData.Calibration.Translation[ 2u ] << "]"
         << endl;
    cout << "Rotation: [" << infoData.Calibration.Rotation[ 0u ] << ", "
         << infoData.Calibration.Rotation[ 1u ] << ", " << infoData.Calibration.Rotation[ 2u ] << "]" << endl;

    cout << "\tSUCCESS" << endl;

    ftkDeleteFrame( frame );
    ftkClose( &lib );
    return 0;
}
