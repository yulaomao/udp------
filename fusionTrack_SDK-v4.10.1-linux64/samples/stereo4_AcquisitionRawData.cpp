// ============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo4_AcquisitionRawData.cpp
 *   \brief Demonstrate how to get raw data from the device
 *
 *   This sample aims to present the following driver features:
 *   - Open/close the driver
 *   - Enumerate devices
 *   - Display raw data from the frame
 *
 *   How to compile this example:
 *   - Follow instructions in README.txt
 *
 *   How to run this example:
 *   - Install the fusionTrack/spryTrack driver (see documentation)
 *   - Switch on device
 *   - Run the resulting executable
 *   - Expose a marker or a set of fiducials in front of the
 *     fusionTrack/spryTrack
 *
 */
// ============================================================================

#include "helpers.hpp"

#include <algorithm>
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

bool savePGMImage( std::string fileName, uint16 width, uint16 height, uint8* pixels )
{
    ofstream file( fileName.c_str() );
    if ( file.fail() || !file.is_open() )
    {
        cerr << "ERROR: cannot save image to file " << fileName << endl;
        return false;
    }
    file << "P2" << endl << width << " " << height << endl << 255 << endl;

    uint32 size( width * height );
    for ( uint32 s( 0u ); s < size; ++s )
    {
        file << unsigned( pixels[ s ] ) << " ";
        if ( ( s > 0 ) && ( s % width == 0 ) )
        {
            file << endl;
        }
    }
    file << endl;
    file.close();
    return true;
}

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

    cout << "This is a demonstration of how to load geometry from a file and how to get the marker position "
            "and orientation."
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
    // When using a spryTrack, we force the sending of images.
    if ( ftkDeviceType::DEV_SPRYTRACK_180 == device.Type || ftkDeviceType::DEV_SPRYTRACK_300 == device.Type )
    {
        if ( options.find( "Enable images sending" ) == options.cend() )
        {
            error( "Could not find option 'Enable images sending'", !isNotFromConsole );
        }
        cout << "Enable images sending" << endl;
        if ( ftkSetInt32( lib, sn, options[ "Enable images sending" ], 1 ) != ftkError::FTK_OK )
        {
            cerr << "Cannot enable images sending on the SpryTrack." << endl;
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

    err = ftkSetFrameOptions( true, true, 128u, 128u, 256u, 4u, frame );

    if ( err != ftkError::FTK_OK )
    {
        ftkDeleteFrame( frame );
        cerr << "Cannot initialise frame" << endl;
        checkError( lib, !isNotFromConsole );
    }

    cout.setf( ios::fixed, ios::floatfield );
    cout.precision( 2u );

    err = ftkGetLastFrame( lib, sn, frame, 1000 );

    char tmp[ 1024u ];
    ftkGetLastErrorString( lib, 1024u, tmp );
    bool status( true );
    ErrorReader errReader;
    if ( !errReader.parseErrorString( tmp ) )
    {
        cerr << "Cannot interpret error string:" << endl << tmp << endl;
        ftkDeleteFrame( frame );
        ftkClose( &lib );
#ifdef ATR_WIN
        cout << "Press the \"ANY\" key to exit" << endl;
        waitForKeyboardHit();
#endif
        return 1;
    }

    if ( errReader.isOk() )
    {
        // --- Raw left ---

        switch ( frame->rawDataLeftStat )
        {
        case ftkQueryStatus::QS_WAR_SKIPPED:
            ftkDeleteFrame( frame );
            cerr << "raw data left status fields in the frame is not set correctly" << endl;
            checkError( lib, !isNotFromConsole );

        case ftkQueryStatus::QS_ERR_INVALID_RESERVED_SIZE:
            ftkDeleteFrame( frame );
            cerr << "frame -> rawDataLeftVersionSize is invalid" << endl;
            checkError( lib, !isNotFromConsole );

        default:
            ftkDeleteFrame( frame );
            cerr << "invalid status" << endl;
            checkError( lib, !isNotFromConsole );

        case ftkQueryStatus::QS_OK:
            break;
        }

        cout << "LEFT image:" << endl;
        for ( uint32 m = 0; m < frame->rawDataLeftCount; m++ )
        {
            cout << "\tX: " << frame->rawDataLeft[ m ].centerXPixels
                 << "\tY: " << frame->rawDataLeft[ m ].centerYPixels
                 << "\tSURFACE: " << frame->rawDataLeft[ m ].pixelsCount
                 << "\tWIDTH: " << frame->rawDataLeft[ m ].width
                 << "\tHEIGHT: " << frame->rawDataLeft[ m ].height << endl;
        }

        // --- Raw right ---

        switch ( frame->rawDataRightStat )
        {
        case ftkQueryStatus::QS_WAR_SKIPPED:
            ftkDeleteFrame( frame );
            cerr << "raw data right status fields in the frame is not set correctly" << endl;
            checkError( lib, !isNotFromConsole );

        case ftkQueryStatus::QS_ERR_INVALID_RESERVED_SIZE:
            ftkDeleteFrame( frame );
            cerr << "frame -> rawDataRightVersionSize is invalid" << endl;
            checkError( lib, !isNotFromConsole );

        default:
            ftkDeleteFrame( frame );
            cerr << "invalid status" << endl;
            checkError( lib, !isNotFromConsole );

        case ftkQueryStatus::QS_OK:
            break;
        }

        cout << "RIGHT image:" << endl;
        for ( uint32 m = 0; m < frame->rawDataRightCount; m++ )
        {
            cout << "\tX: " << frame->rawDataRight[ m ].centerXPixels
                 << "\tY: " << frame->rawDataRight[ m ].centerYPixels
                 << "\tSURFACE: " << frame->rawDataRight[ m ].pixelsCount
                 << "\tWIDTH: " << frame->rawDataRight[ m ].width
                 << "\tHEIGHT: " << frame->rawDataRight[ m ].height << endl;
        }

        // ---- 3D ---

        switch ( frame->threeDFiducialsStat )
        {
        case ftkQueryStatus::QS_WAR_SKIPPED:
            ftkDeleteFrame( frame );
            cerr << "3D status fields in the frame is not set correctly" << endl;
            checkError( lib, !isNotFromConsole );

        case ftkQueryStatus::QS_ERR_INVALID_RESERVED_SIZE:
            ftkDeleteFrame( frame );
            cerr << "frame -> threeDFiducialsVersionSize is invalid" << endl;
            checkError( lib, !isNotFromConsole );

        default:
            ftkDeleteFrame( frame );
            cerr << "invalid status" << endl;
            checkError( lib, !isNotFromConsole );

        case ftkQueryStatus::QS_OK:
            break;
        }

        cout << "3D fiducials:" << endl;
        for ( uint32 m = 0; m < frame->threeDFiducialsCount; m++ )
        {
            cout << "\tINDEXES (" << frame->threeDFiducials[ m ].leftIndex << " "
                 << frame->threeDFiducials[ m ].rightIndex << ")\t"
                 << "XYZ (" << frame->threeDFiducials[ m ].positionMM.x << " "
                 << frame->threeDFiducials[ m ].positionMM.y << " "
                 << frame->threeDFiducials[ m ].positionMM.z << ")" << endl
                 << "\t\tEPI_ERR: " << frame->threeDFiducials[ m ].epipolarErrorPixels
                 << "\tTRI_ERR: " << frame->threeDFiducials[ m ].triangulationErrorMM
                 << "\tPROB: " << frame->threeDFiducials[ m ].probability << endl;
        }

        if ( frame->imageLeftStat == ftkQueryStatus::QS_OK &&
             frame->imageHeaderStat == ftkQueryStatus::QS_OK )
        {
            if ( savePGMImage( "./left.pgm",
                               frame->imageHeader->width,
                               frame->imageHeader->height,
                               frame->imageLeftPixels ) )
            {
                cout << "Left image saved" << endl;
            }
            else
            {
                cout << "Cannot save the left picture, please check you have write permission on the current "
                        "folder."
                     << endl;
                status &= false;
            }
        }
        if ( frame->imageRightStat == ftkQueryStatus::QS_OK &&
             frame->imageHeaderStat == ftkQueryStatus::QS_OK )
        {
            if ( savePGMImage( "./right.pgm",
                               frame->imageHeader->width,
                               frame->imageHeader->height,
                               frame->imageRightPixels ) )
            {
                cout << "Right image saved" << endl;
            }
            else
            {
                cout << "Cannot save the left picture, please check you have write permission on the current "
                        "folder."
                     << endl;
                status &= false;
            }
        }
    }
    else
    {
        ftkGetLastErrorString( lib, 1024u, tmp );
        cerr << tmp << endl;
    }

    if ( status )
    {
        cout << "\tSUCCESS" << endl;
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
