// ============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo99_DumpAllData.cpp
 *   \brief Comprehensive data dumping sample for algorithm verification
 *
 *   This sample captures and saves ALL available per-frame data from the
 *   fusionTrack SDK, including:
 *   - Left/Right raw images (PGM format)
 *   - Image header (timestamp, counter, dimensions, format, stride)
 *   - Raw detection data (circle centers, area, bounding box, status) for both cameras
 *   - 3D fiducials (stereo matching results: 3D position, left/right indices,
 *     epipolar error, triangulation error, probability, status)
 *   - Marker data (pose: translation + rotation, registration error,
 *     fiducial correspondence, geometry presence mask, status)
 *   - Calibration parameters (intrinsics + extrinsics of stereo camera system)
 *   - Reprojection data (3D fiducial -> 2D pixel via ftkReprojectPoint)
 *   - Loaded geometry definition
 *
 *   Output is saved as CSV and binary files in a user-specified output directory.
 *
 *   Usage:
 *     stereo99_DumpAllData -g geometry072.ini [-c config.json] [-n 10] [-o ./output]
 *
 */
// ============================================================================

#include "geometryHelper.hpp"
#include "helpers.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

#ifdef ATR_WIN
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#define MKDIR(dir) mkdir(dir, 0755)
#endif

using namespace std;

// ---------------------------------------------------------------------------
// Helper: create directory (ignore if exists)
static void ensureDir( const string& path )
{
    MKDIR( path.c_str() );
}

// ---------------------------------------------------------------------------
// Helper: save raw image as PGM (P5 binary for speed)
static bool savePGMBinary( const string& fileName, uint16 width, uint16 height,
                           int32 strideBytes, uint8* pixels )
{
    ofstream file( fileName.c_str(), ios::binary );
    if ( !file.is_open() )
    {
        cerr << "ERROR: cannot save image to " << fileName << endl;
        return false;
    }
    file << "P5\n" << width << " " << height << "\n255\n";
    for ( uint16 row = 0; row < height; ++row )
    {
        file.write( reinterpret_cast< const char* >( pixels + row * strideBytes ), width );
    }
    file.close();
    return true;
}

// ---------------------------------------------------------------------------
// Helper: save raw image as raw binary (for lossless round-trip)
static bool saveRawBinary( const string& fileName, uint16 width, uint16 height,
                           int32 strideBytes, uint8* pixels )
{
    ofstream file( fileName.c_str(), ios::binary );
    if ( !file.is_open() )
    {
        cerr << "ERROR: cannot save raw image to " << fileName << endl;
        return false;
    }
    for ( uint16 row = 0; row < height; ++row )
    {
        file.write( reinterpret_cast< const char* >( pixels + row * strideBytes ), width );
    }
    file.close();
    return true;
}

// ---------------------------------------------------------------------------
// Helper: status bits to string
static string statusToString( ftkStatus s )
{
    ostringstream oss;
    oss << static_cast< uint32 >( s );
    return oss.str();
}

// ---------------------------------------------------------------------------
// main function

int main( int argc, char** argv )
{
    const bool isNotFromConsole = isLaunchedFromExplorer();

#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << "=== stereo99_DumpAllData ===" << endl;
    cout << "Comprehensive data dumper for reverse-engineering verification." << endl;

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

    string cfgFile( "D:/test.json" );

    deque< string > geomFiles{ "D:/geometry072.ini" };
    uint32 numFrames = 10u;
    string outputDir = "D:/dump_output";

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-c/--config F] " << flush << "JSON config file." << endl;
        cout << setw( 30u ) << "[-g/--geom F (F ...)] " << flush << "geometry file(s), default = geometry072.ini"
             << endl;
        cout << setw( 30u ) << "[-n/--frames N] " << flush << "Number of frames to capture, default = 10"
             << endl;
        cout << setw( 30u ) << "[-o/--output DIR] " << flush << "Output directory, default = ./dump_output"
             << endl;
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

    // Parse arguments
    auto pos = find_if( args.cbegin(), args.cend(),
                        []( const string& val ) { return val == "-c" || val == "--config"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        cfgFile = *pos;
    }

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "-g" || val == "--geom"; } );
    if ( pos != args.cend() )
    {
        geomFiles.clear();
        while ( pos != args.cend() && ++pos != args.cend() )
        {
            if ( pos->substr( 0u, 1u ) == "-" )
                break;
            geomFiles.emplace_back( *pos );
        }
    }

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "-n" || val == "--frames"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        numFrames = static_cast< uint32 >( atoi( pos->c_str() ) );
    }

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "-o" || val == "--output"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        outputDir = *pos;
    }

    // Create output directory
    ensureDir( outputDir );

    // -----------------------------------------------------------------------
    // Initialize driver

    ftkBuffer buffer;
    ftkLibrary lib( ftkInitExt( cfgFile.empty() ? nullptr : cfgFile.c_str(), &buffer ) );
    if ( lib == nullptr )
    {
        cerr << buffer.data << endl;
        error( "Cannot initialize driver", !isNotFromConsole );
    }

    // -----------------------------------------------------------------------
    // Retrieve the device

    DeviceData device( retrieveLastDevice( lib, true, false, !isNotFromConsole ) );
    uint64 sn( device.SerialNumber );

    map< string, uint32 > options{};
    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &options ) );
    if ( options.empty() )
    {
        error( "Cannot retrieve any options.", !isNotFromConsole );
    }

    // -----------------------------------------------------------------------
    // SpryTrack: enable image sending
    if ( ftkDeviceType::DEV_SPRYTRACK_180 == device.Type ||
         ftkDeviceType::DEV_SPRYTRACK_300 == device.Type )
    {
        if ( options.find( "Enable images sending" ) != options.cend() )
        {
            cout << "Enable images sending for SpryTrack" << endl;
            ftkSetInt32( lib, sn, options[ "Enable images sending" ], 1 );
        }
        if ( options.find( "Enable embedded processing" ) != options.cend() )
        {
            cout << "Enable onboard processing for SpryTrack" << endl;
            ftkSetInt32( lib, sn, options[ "Enable embedded processing" ], 1 );
        }
    }

    // -----------------------------------------------------------------------
    // Enable calibration export
    if ( options.find( "Calibration export" ) != options.cend() )
    {
        err = ftkSetInt32( lib, sn, options[ "Calibration export" ], 1 );
        if ( err != ftkError::FTK_OK )
        {
            cerr << "WARNING: Cannot enable calibration export" << endl;
        }
        else
        {
            cout << "Calibration export enabled" << endl;
        }
    }
    else
    {
        cerr << "WARNING: 'Calibration export' option not found" << endl;
    }

    // -----------------------------------------------------------------------
    // Set geometry

    ftkRigidBody geom{};
    for ( const auto& geomFile : geomFiles )
    {
        switch ( loadRigidBody( lib, geomFile, geom ) )
        {
        case 1:
            cout << "Loaded geometry from installation directory: " << geomFile << endl;
        case 0:
            if ( ftkError::FTK_OK != ftkSetRigidBody( lib, sn, &geom ) )
            {
                checkError( lib, !isNotFromConsole );
            }
            break;
        default:
            cerr << "Error, cannot load geometry file '" << geomFile << "'." << endl;
            ftkClose( &lib );
#ifdef ATR_WIN
            cout << endl << " *** Hit a key to exit ***" << endl;
            waitForKeyboardHit();
#endif
            return 1;
        }
    }

    // -----------------------------------------------------------------------
    // Save geometry definition
    {
        string geomPath = outputDir + "/geometry.csv";
        ofstream geomOut( geomPath.c_str() );
        if ( geomOut.is_open() )
        {
            geomOut << "# Loaded Geometry Definition" << endl;
            geomOut << "geometryId," << geom.geometryId << endl;
            geomOut << "version," << geom.version << endl;
            geomOut << "pointsCount," << geom.pointsCount << endl;
            geomOut << "divotsCount," << geom.divotsCount << endl;
            geomOut << "# fiducial_index,pos_x_mm,pos_y_mm,pos_z_mm,normal_x,normal_y,normal_z,fiducial_type,angle_of_view" << endl;
            for ( uint32 i = 0; i < geom.pointsCount && i < FTK_MAX_FIDUCIALS; ++i )
            {
                geomOut << i << ","
                        << geom.fiducials[ i ].position.x << ","
                        << geom.fiducials[ i ].position.y << ","
                        << geom.fiducials[ i ].position.z << ","
                        << geom.fiducials[ i ].normalVector.x << ","
                        << geom.fiducials[ i ].normalVector.y << ","
                        << geom.fiducials[ i ].normalVector.z << ","
                        << static_cast< uint32 >( geom.fiducials[ i ].fiducialInfo.type ) << ","
                        << geom.fiducials[ i ].fiducialInfo.angleOfView << endl;
            }
            if ( geom.divotsCount > 0 )
            {
                geomOut << "# divot_index,pos_x_mm,pos_y_mm,pos_z_mm" << endl;
                for ( uint32 i = 0; i < geom.divotsCount && i < FTK_MAX_FIDUCIALS; ++i )
                {
                    geomOut << i << ","
                            << geom.divotPositions[ i ].x << ","
                            << geom.divotPositions[ i ].y << ","
                            << geom.divotPositions[ i ].z << endl;
                }
            }
            geomOut.close();
            cout << "Geometry saved to " << geomPath << endl;
        }
    }

    // -----------------------------------------------------------------------
    // Save all device options
    {
        string optPath = outputDir + "/device_options.csv";
        ofstream optOut( optPath.c_str() );
        if ( optOut.is_open() )
        {
            optOut << "# Device Options" << endl;
            optOut << "option_name,option_id" << endl;
            for ( const auto& opt : options )
            {
                optOut << opt.first << "," << opt.second << endl;
            }
            optOut.close();
            cout << "Device options saved to " << optPath << endl;
        }
    }

    // -----------------------------------------------------------------------
    // Initialize frame with maximum data

    ftkFrameQuery* frame = ftkCreateFrame();
    if ( frame == 0 )
    {
        cerr << "Cannot create frame instance" << endl;
        checkError( lib, !isNotFromConsole );
    }

    // Request ALL data types with generous buffer sizes:
    //   pixels=true, events=16, leftRaw=256, rightRaw=256, threeDFid=256, markers=32
    err = ftkSetFrameOptions( true, 16u, 256u, 256u, 256u, 32u, frame );
    if ( err != ftkError::FTK_OK )
    {
        ftkDeleteFrame( frame );
        cerr << "Cannot initialise frame" << endl;
        checkError( lib, !isNotFromConsole );
    }

    // -----------------------------------------------------------------------
    // Open summary CSV files (append per-frame data)

    string rawLeftCsvPath = outputDir + "/raw_data_left.csv";
    string rawRightCsvPath = outputDir + "/raw_data_right.csv";
    string fid3dCsvPath = outputDir + "/fiducials_3d.csv";
    string markerCsvPath = outputDir + "/markers.csv";
    string headerCsvPath = outputDir + "/image_headers.csv";
    string reprojCsvPath = outputDir + "/reprojections.csv";
    string calibCsvPath = outputDir + "/calibration.csv";

    ofstream rawLeftCsv( rawLeftCsvPath.c_str() );
    ofstream rawRightCsv( rawRightCsvPath.c_str() );
    ofstream fid3dCsv( fid3dCsvPath.c_str() );
    ofstream markerCsv( markerCsvPath.c_str() );
    ofstream headerCsv( headerCsvPath.c_str() );
    ofstream reprojCsv( reprojCsvPath.c_str() );
    ofstream calibCsv( calibCsvPath.c_str() );

    // CSV headers
    rawLeftCsv << "frame_idx,detection_idx,centerX_pixels,centerY_pixels,pixelsCount,width,height,"
               << "status_bits" << endl;
    rawRightCsv << "frame_idx,detection_idx,centerX_pixels,centerY_pixels,pixelsCount,width,height,"
                << "status_bits" << endl;
    fid3dCsv << "frame_idx,fid3d_idx,pos_x_mm,pos_y_mm,pos_z_mm,"
             << "leftIndex,rightIndex,epipolarError_pixels,triangulationError_mm,probability,"
             << "status_bits" << endl;
    markerCsv << "frame_idx,marker_idx,geometryId,trackingId,registrationError_mm,"
              << "tx_mm,ty_mm,tz_mm,"
              << "r00,r01,r02,r10,r11,r12,r20,r21,r22,"
              << "presenceMask,"
              << "fidCorresp0,fidCorresp1,fidCorresp2,fidCorresp3,fidCorresp4,fidCorresp5,"
              << "status_bits" << endl;
    headerCsv << "frame_idx,timestamp_us,counter,format,width,height,stride_bytes,desynchro_us" << endl;
    reprojCsv << "frame_idx,fid3d_idx,pos3d_x_mm,pos3d_y_mm,pos3d_z_mm,"
              << "reproj_left_x,reproj_left_y,reproj_right_x,reproj_right_y" << endl;
    calibCsv << "# Stereo Calibration Parameters (extracted once)" << endl;

    // Set precision for all CSV files
    rawLeftCsv << fixed << setprecision( 6 );
    rawRightCsv << fixed << setprecision( 6 );
    fid3dCsv << fixed << setprecision( 6 );
    markerCsv << fixed << setprecision( 6 );
    reprojCsv << fixed << setprecision( 6 );

    bool calibSaved = false;

    // -----------------------------------------------------------------------
    // Acquisition loop

    cout << "\nCapturing " << numFrames << " frames..." << endl;
    cout << "Place a marker with the loaded geometry in front of the device." << endl;

    uint32 capturedFrames = 0;
    uint32 maxAttempts = numFrames * 20u;  // Allow many retries

    for ( uint32 attempt = 0; attempt < maxAttempts && capturedFrames < numFrames; ++attempt )
    {
        err = ftkGetLastFrame( lib, sn, frame, 200u );
        if ( err > ftkError::FTK_OK )
        {
            // Hard error
            cerr << "Frame error: " << int32( err ) << endl;
            continue;
        }
        if ( err == ftkError::FTK_WAR_NO_FRAME )
        {
            continue;
        }
        if ( err == ftkError::FTK_WAR_TEMP_INVALID )
        {
            cerr << "Temperature warning (frame still usable)" << endl;
        }
        else if ( err < ftkError::FTK_OK && err != ftkError::FTK_WAR_NO_FRAME )
        {
            cerr << "Frame warning: " << int32( err ) << endl;
        }

        // Only save frames where we have raw data at minimum
        bool hasAnyData = false;

        // -------------------------------------------------------------------
        // 1. Image Header
        if ( frame->imageHeaderStat == ftkQueryStatus::QS_OK && frame->imageHeader != nullptr )
        {
            headerCsv << capturedFrames << ","
                      << frame->imageHeader->timestampUS << ","
                      << frame->imageHeader->counter << ","
                      << static_cast< int >( frame->imageHeader->format ) << ","
                      << frame->imageHeader->width << ","
                      << frame->imageHeader->height << ","
                      << frame->imageHeader->imageStrideInBytes << ","
                      << frame->imageHeader->desynchroUS << endl;
            hasAnyData = true;
        }

        // -------------------------------------------------------------------
        // 2. Save images (both PGM and raw binary)
        if ( frame->imageLeftStat == ftkQueryStatus::QS_OK &&
             frame->imageHeaderStat == ftkQueryStatus::QS_OK &&
             frame->imageLeftPixels != nullptr && frame->imageHeader != nullptr )
        {
            ostringstream pgmName;
            pgmName << outputDir << "/frame_" << setfill( '0' ) << setw( 4 ) << capturedFrames << "_left.pgm";
            savePGMBinary( pgmName.str(), frame->imageHeader->width, frame->imageHeader->height,
                           frame->imageHeader->imageStrideInBytes, frame->imageLeftPixels );

            ostringstream rawName;
            rawName << outputDir << "/frame_" << setfill( '0' ) << setw( 4 ) << capturedFrames << "_left.raw";
            saveRawBinary( rawName.str(), frame->imageHeader->width, frame->imageHeader->height,
                           frame->imageHeader->imageStrideInBytes, frame->imageLeftPixels );
            hasAnyData = true;
        }

        if ( frame->imageRightStat == ftkQueryStatus::QS_OK &&
             frame->imageHeaderStat == ftkQueryStatus::QS_OK &&
             frame->imageRightPixels != nullptr && frame->imageHeader != nullptr )
        {
            ostringstream pgmName;
            pgmName << outputDir << "/frame_" << setfill( '0' ) << setw( 4 ) << capturedFrames << "_right.pgm";
            savePGMBinary( pgmName.str(), frame->imageHeader->width, frame->imageHeader->height,
                           frame->imageHeader->imageStrideInBytes, frame->imageRightPixels );

            ostringstream rawName;
            rawName << outputDir << "/frame_" << setfill( '0' ) << setw( 4 ) << capturedFrames << "_right.raw";
            saveRawBinary( rawName.str(), frame->imageHeader->width, frame->imageHeader->height,
                           frame->imageHeader->imageStrideInBytes, frame->imageRightPixels );
            hasAnyData = true;
        }

        // -------------------------------------------------------------------
        // 3. Raw data left (circle detections)
        if ( frame->rawDataLeftStat == ftkQueryStatus::QS_OK )
        {
            for ( uint32 m = 0; m < frame->rawDataLeftCount; ++m )
            {
                rawLeftCsv << capturedFrames << ","
                           << m << ","
                           << frame->rawDataLeft[ m ].centerXPixels << ","
                           << frame->rawDataLeft[ m ].centerYPixels << ","
                           << frame->rawDataLeft[ m ].pixelsCount << ","
                           << frame->rawDataLeft[ m ].width << ","
                           << frame->rawDataLeft[ m ].height << ","
                           << statusToString( frame->rawDataLeft[ m ].status ) << endl;
            }
            if ( frame->rawDataLeftCount > 0 )
                hasAnyData = true;

            if ( frame->rawDataLeftStat == ftkQueryStatus::QS_ERR_OVERFLOW )
            {
                cerr << "WARNING: left raw data overflow at frame " << capturedFrames << endl;
            }
        }

        // -------------------------------------------------------------------
        // 4. Raw data right (circle detections)
        if ( frame->rawDataRightStat == ftkQueryStatus::QS_OK )
        {
            for ( uint32 m = 0; m < frame->rawDataRightCount; ++m )
            {
                rawRightCsv << capturedFrames << ","
                            << m << ","
                            << frame->rawDataRight[ m ].centerXPixels << ","
                            << frame->rawDataRight[ m ].centerYPixels << ","
                            << frame->rawDataRight[ m ].pixelsCount << ","
                            << frame->rawDataRight[ m ].width << ","
                            << frame->rawDataRight[ m ].height << ","
                            << statusToString( frame->rawDataRight[ m ].status ) << endl;
            }
            if ( frame->rawDataRightCount > 0 )
                hasAnyData = true;

            if ( frame->rawDataRightStat == ftkQueryStatus::QS_ERR_OVERFLOW )
            {
                cerr << "WARNING: right raw data overflow at frame " << capturedFrames << endl;
            }
        }

        // -------------------------------------------------------------------
        // 5. 3D fiducials (stereo matching results)
        if ( frame->threeDFiducialsStat == ftkQueryStatus::QS_OK )
        {
            for ( uint32 m = 0; m < frame->threeDFiducialsCount; ++m )
            {
                fid3dCsv << capturedFrames << ","
                         << m << ","
                         << frame->threeDFiducials[ m ].positionMM.x << ","
                         << frame->threeDFiducials[ m ].positionMM.y << ","
                         << frame->threeDFiducials[ m ].positionMM.z << ","
                         << frame->threeDFiducials[ m ].leftIndex << ","
                         << frame->threeDFiducials[ m ].rightIndex << ","
                         << frame->threeDFiducials[ m ].epipolarErrorPixels << ","
                         << frame->threeDFiducials[ m ].triangulationErrorMM << ","
                         << frame->threeDFiducials[ m ].probability << ","
                         << statusToString( frame->threeDFiducials[ m ].status ) << endl;
            }
            if ( frame->threeDFiducialsCount > 0 )
                hasAnyData = true;

            if ( frame->threeDFiducialsStat == ftkQueryStatus::QS_ERR_OVERFLOW )
            {
                cerr << "WARNING: 3D fiducials overflow at frame " << capturedFrames << endl;
            }
        }

        // -------------------------------------------------------------------
        // 6. Markers (pose data)
        if ( frame->markersStat == ftkQueryStatus::QS_OK )
        {
            for ( uint32 m = 0; m < frame->markersCount; ++m )
            {
                markerCsv << capturedFrames << ","
                          << m << ","
                          << frame->markers[ m ].geometryId << ","
                          << frame->markers[ m ].id << ","
                          << frame->markers[ m ].registrationErrorMM << ","
                          << frame->markers[ m ].translationMM[ 0 ] << ","
                          << frame->markers[ m ].translationMM[ 1 ] << ","
                          << frame->markers[ m ].translationMM[ 2 ] << ","
                          << frame->markers[ m ].rotation[ 0 ][ 0 ] << ","
                          << frame->markers[ m ].rotation[ 0 ][ 1 ] << ","
                          << frame->markers[ m ].rotation[ 0 ][ 2 ] << ","
                          << frame->markers[ m ].rotation[ 1 ][ 0 ] << ","
                          << frame->markers[ m ].rotation[ 1 ][ 1 ] << ","
                          << frame->markers[ m ].rotation[ 1 ][ 2 ] << ","
                          << frame->markers[ m ].rotation[ 2 ][ 0 ] << ","
                          << frame->markers[ m ].rotation[ 2 ][ 1 ] << ","
                          << frame->markers[ m ].rotation[ 2 ][ 2 ] << ","
                          << frame->markers[ m ].geometryPresenceMask << ","
                          << frame->markers[ m ].fiducialCorresp[ 0 ] << ","
                          << frame->markers[ m ].fiducialCorresp[ 1 ] << ","
                          << frame->markers[ m ].fiducialCorresp[ 2 ] << ","
                          << frame->markers[ m ].fiducialCorresp[ 3 ] << ","
                          << frame->markers[ m ].fiducialCorresp[ 4 ] << ","
                          << frame->markers[ m ].fiducialCorresp[ 5 ] << ","
                          << statusToString( frame->markers[ m ].status ) << endl;
            }

            if ( frame->markersStat == ftkQueryStatus::QS_ERR_OVERFLOW )
            {
                cerr << "WARNING: markers overflow at frame " << capturedFrames << endl;
            }
        }

        // -------------------------------------------------------------------
        // 7. Reprojection of 3D fiducials back to 2D pixel coordinates
        if ( frame->threeDFiducialsStat == ftkQueryStatus::QS_OK )
        {
            for ( uint32 m = 0; m < frame->threeDFiducialsCount; ++m )
            {
                ftk3DPoint leftPt{}, rightPt{};
                ftkError reprojErr = ftkReprojectPoint(
                    lib, sn, &frame->threeDFiducials[ m ].positionMM, &leftPt, &rightPt );
                if ( reprojErr == ftkError::FTK_OK )
                {
                    reprojCsv << capturedFrames << ","
                              << m << ","
                              << frame->threeDFiducials[ m ].positionMM.x << ","
                              << frame->threeDFiducials[ m ].positionMM.y << ","
                              << frame->threeDFiducials[ m ].positionMM.z << ","
                              << leftPt.x << ","
                              << leftPt.y << ","
                              << rightPt.x << ","
                              << rightPt.y << endl;
                }
                else
                {
                    // Save with NaN to indicate failure
                    reprojCsv << capturedFrames << ","
                              << m << ","
                              << frame->threeDFiducials[ m ].positionMM.x << ","
                              << frame->threeDFiducials[ m ].positionMM.y << ","
                              << frame->threeDFiducials[ m ].positionMM.z << ","
                              << "NaN,NaN,NaN,NaN" << endl;
                }
            }
        }

        // -------------------------------------------------------------------
        // 8. Extract calibration parameters (once)
        if ( !calibSaved )
        {
            ftkFrameInfoData infoData;
            infoData.WantedInformation = ftkInformationType::CalibrationParameters;
            ftkError calibErr = ftkExtractFrameInfo( frame, &infoData );
            if ( calibErr == ftkError::FTK_OK )
            {
                const ftkStereoParameters& cal = infoData.Calibration;

                // Use setprecision(9) for float32 exact round-trip.
                // C++ float (IEEE 754 single) has ~7.22 decimal digits of precision.
                // 9 significant digits guarantee exact round-trip for ALL float32 values.
                // Previous default precision (6 sig digits) caused ambiguity for some parameters,
                // leading to ~20mm triangulation errors for degenerate (status>0) matches at extreme depth.
                calibCsv << setprecision( 9 );

                calibCsv << "# Left Camera" << endl;
                calibCsv << "left_focal_length," << cal.LeftCamera.FocalLength[ 0 ] << ","
                         << cal.LeftCamera.FocalLength[ 1 ] << endl;
                calibCsv << "left_optical_centre," << cal.LeftCamera.OpticalCentre[ 0 ] << ","
                         << cal.LeftCamera.OpticalCentre[ 1 ] << endl;
                calibCsv << "left_distortions,"
                         << cal.LeftCamera.Distorsions[ 0 ] << ","
                         << cal.LeftCamera.Distorsions[ 1 ] << ","
                         << cal.LeftCamera.Distorsions[ 2 ] << ","
                         << cal.LeftCamera.Distorsions[ 3 ] << ","
                         << cal.LeftCamera.Distorsions[ 4 ] << endl;
                calibCsv << "left_skew," << cal.LeftCamera.Skew << endl;

                calibCsv << "# Right Camera" << endl;
                calibCsv << "right_focal_length," << cal.RightCamera.FocalLength[ 0 ] << ","
                         << cal.RightCamera.FocalLength[ 1 ] << endl;
                calibCsv << "right_optical_centre," << cal.RightCamera.OpticalCentre[ 0 ] << ","
                         << cal.RightCamera.OpticalCentre[ 1 ] << endl;
                calibCsv << "right_distortions,"
                         << cal.RightCamera.Distorsions[ 0 ] << ","
                         << cal.RightCamera.Distorsions[ 1 ] << ","
                         << cal.RightCamera.Distorsions[ 2 ] << ","
                         << cal.RightCamera.Distorsions[ 3 ] << ","
                         << cal.RightCamera.Distorsions[ 4 ] << endl;
                calibCsv << "right_skew," << cal.RightCamera.Skew << endl;

                calibCsv << "# Stereo Extrinsics (right camera in left camera frame)" << endl;
                calibCsv << "translation," << cal.Translation[ 0 ] << ","
                         << cal.Translation[ 1 ] << "," << cal.Translation[ 2 ] << endl;
                calibCsv << "rotation," << cal.Rotation[ 0 ] << ","
                         << cal.Rotation[ 1 ] << "," << cal.Rotation[ 2 ] << endl;

                calibSaved = true;
                cout << "Calibration parameters saved to " << calibCsvPath << endl;
            }
        }

        // -------------------------------------------------------------------
        // 9. Per-frame summary with cross-references

        if ( hasAnyData )
        {
            // Save a per-frame cross-reference file linking markers -> fiducials -> raw detections
            ostringstream xrefName;
            xrefName << outputDir << "/frame_" << setfill( '0' ) << setw( 4 ) << capturedFrames << "_xref.csv";
            ofstream xrefCsv( xrefName.str().c_str() );
            if ( xrefCsv.is_open() )
            {
                xrefCsv << fixed << setprecision( 6 );
                xrefCsv << "# Cross-reference: marker -> fiducial -> raw detection" << endl;
                xrefCsv << "marker_idx,geom_fid_idx,fid3d_idx,"
                        << "fid3d_x_mm,fid3d_y_mm,fid3d_z_mm,"
                        << "left_raw_idx,left_cx,left_cy,left_area,left_w,left_h,"
                        << "right_raw_idx,right_cx,right_cy,right_area,right_w,right_h,"
                        << "epipolar_err_px,triangulation_err_mm,probability" << endl;

                if ( frame->markersStat == ftkQueryStatus::QS_OK )
                {
                    for ( uint32 mi = 0; mi < frame->markersCount; ++mi )
                    {
                        for ( uint32 fi = 0; fi < FTK_MAX_FIDUCIALS; ++fi )
                        {
                            uint32 fidId = frame->markers[ mi ].fiducialCorresp[ fi ];
                            if ( fidId == INVALID_ID ||
                                 frame->threeDFiducialsStat != ftkQueryStatus::QS_OK ||
                                 fidId >= frame->threeDFiducialsCount )
                                continue;

                            const ftk3DFiducial& fid = frame->threeDFiducials[ fidId ];

                            xrefCsv << mi << "," << fi << "," << fidId << ","
                                    << fid.positionMM.x << "," << fid.positionMM.y << ","
                                    << fid.positionMM.z << ",";

                            // Left raw data
                            if ( frame->rawDataLeftStat == ftkQueryStatus::QS_OK &&
                                 fid.leftIndex < frame->rawDataLeftCount )
                            {
                                const ftkRawData& left = frame->rawDataLeft[ fid.leftIndex ];
                                xrefCsv << fid.leftIndex << ","
                                        << left.centerXPixels << "," << left.centerYPixels << ","
                                        << left.pixelsCount << ","
                                        << left.width << "," << left.height << ",";
                            }
                            else
                            {
                                xrefCsv << ",,,,,,";
                            }

                            // Right raw data
                            if ( frame->rawDataRightStat == ftkQueryStatus::QS_OK &&
                                 fid.rightIndex < frame->rawDataRightCount )
                            {
                                const ftkRawData& right = frame->rawDataRight[ fid.rightIndex ];
                                xrefCsv << fid.rightIndex << ","
                                        << right.centerXPixels << "," << right.centerYPixels << ","
                                        << right.pixelsCount << ","
                                        << right.width << "," << right.height << ",";
                            }
                            else
                            {
                                xrefCsv << ",,,,,,";
                            }

                            xrefCsv << fid.epipolarErrorPixels << ","
                                    << fid.triangulationErrorMM << ","
                                    << fid.probability << endl;
                        }
                    }
                }

                // Also list any unmatched 3D fiducials
                if ( frame->threeDFiducialsStat == ftkQueryStatus::QS_OK )
                {
                    xrefCsv << "# Unmatched 3D fiducials (not assigned to any marker)" << endl;
                    xrefCsv << "fid3d_idx,fid3d_x_mm,fid3d_y_mm,fid3d_z_mm,"
                            << "left_raw_idx,left_cx,left_cy,"
                            << "right_raw_idx,right_cx,right_cy,"
                            << "epipolar_err_px,triangulation_err_mm,probability" << endl;

                    for ( uint32 fi = 0; fi < frame->threeDFiducialsCount; ++fi )
                    {
                        const ftk3DFiducial& fid = frame->threeDFiducials[ fi ];
                        xrefCsv << fi << ","
                                << fid.positionMM.x << ","
                                << fid.positionMM.y << ","
                                << fid.positionMM.z << ",";

                        if ( frame->rawDataLeftStat == ftkQueryStatus::QS_OK &&
                             fid.leftIndex < frame->rawDataLeftCount )
                        {
                            xrefCsv << fid.leftIndex << ","
                                    << frame->rawDataLeft[ fid.leftIndex ].centerXPixels << ","
                                    << frame->rawDataLeft[ fid.leftIndex ].centerYPixels << ",";
                        }
                        else
                        {
                            xrefCsv << ",,,";
                        }

                        if ( frame->rawDataRightStat == ftkQueryStatus::QS_OK &&
                             fid.rightIndex < frame->rawDataRightCount )
                        {
                            xrefCsv << fid.rightIndex << ","
                                    << frame->rawDataRight[ fid.rightIndex ].centerXPixels << ","
                                    << frame->rawDataRight[ fid.rightIndex ].centerYPixels << ",";
                        }
                        else
                        {
                            xrefCsv << ",,,";
                        }

                        xrefCsv << fid.epipolarErrorPixels << ","
                                << fid.triangulationErrorMM << ","
                                << fid.probability << endl;
                    }
                }

                xrefCsv.close();
            }

            cout << "Frame " << capturedFrames << ": "
                 << "left_raw=" << ( frame->rawDataLeftStat == ftkQueryStatus::QS_OK ? frame->rawDataLeftCount : 0 )
                 << " right_raw=" << ( frame->rawDataRightStat == ftkQueryStatus::QS_OK ? frame->rawDataRightCount : 0 )
                 << " fid3d=" << ( frame->threeDFiducialsStat == ftkQueryStatus::QS_OK ? frame->threeDFiducialsCount : 0 )
                 << " markers=" << ( frame->markersStat == ftkQueryStatus::QS_OK ? frame->markersCount : 0 )
                 << endl;

            ++capturedFrames;
        }
        else
        {
            cout << ".";
        }

        sleep( 100L );
    }

    // -----------------------------------------------------------------------
    // Also query total object numbers for reference
    {
        string totalPath = outputDir + "/total_object_counts.csv";
        ofstream totalCsv( totalPath.c_str() );
        if ( totalCsv.is_open() )
        {
            totalCsv << "# Total object counts from last frame (may exceed buffer sizes)" << endl;
            totalCsv << "data_type,count" << endl;

            uint32 count = 0;
            if ( ftkGetTotalObjectNumber( lib, sn, ftkDataType::LeftRawData, &count ) == ftkError::FTK_OK )
                totalCsv << "LeftRawData," << count << endl;
            if ( ftkGetTotalObjectNumber( lib, sn, ftkDataType::RightRawData, &count ) == ftkError::FTK_OK )
                totalCsv << "RightRawData," << count << endl;
            if ( ftkGetTotalObjectNumber( lib, sn, ftkDataType::FiducialData, &count ) == ftkError::FTK_OK )
                totalCsv << "FiducialData," << count << endl;
            if ( ftkGetTotalObjectNumber( lib, sn, ftkDataType::MarkerData, &count ) == ftkError::FTK_OK )
                totalCsv << "MarkerData," << count << endl;

            totalCsv.close();
        }
    }

    // -----------------------------------------------------------------------
    // Close files
    rawLeftCsv.close();
    rawRightCsv.close();
    fid3dCsv.close();
    markerCsv.close();
    headerCsv.close();
    reprojCsv.close();
    calibCsv.close();

    cout << endl;
    if ( capturedFrames > 0 )
    {
        cout << "SUCCESS: Captured " << capturedFrames << " frames." << endl;
        cout << "Output saved to: " << outputDir << "/" << endl;
        cout << endl;
        cout << "Output files:" << endl;
        cout << "  geometry.csv           - Loaded marker geometry definition" << endl;
        cout << "  device_options.csv     - All device option names and IDs" << endl;
        cout << "  calibration.csv        - Stereo camera calibration parameters" << endl;
        cout << "  image_headers.csv      - Per-frame image metadata" << endl;
        cout << "  raw_data_left.csv      - Left camera circle detections (all frames)" << endl;
        cout << "  raw_data_right.csv     - Right camera circle detections (all frames)" << endl;
        cout << "  fiducials_3d.csv       - 3D fiducial positions from stereo matching" << endl;
        cout << "  markers.csv            - Marker poses (translation + rotation)" << endl;
        cout << "  reprojections.csv      - 3D->2D reprojection via ftkReprojectPoint" << endl;
        cout << "  total_object_counts.csv- Total object counts from device" << endl;
        cout << "  frame_NNNN_left.pgm    - Left camera images (PGM format)" << endl;
        cout << "  frame_NNNN_right.pgm   - Right camera images (PGM format)" << endl;
        cout << "  frame_NNNN_left.raw    - Left camera images (raw binary)" << endl;
        cout << "  frame_NNNN_right.raw   - Right camera images (raw binary)" << endl;
        cout << "  frame_NNNN_xref.csv    - Per-frame cross-reference data" << endl;
    }
    else
    {
        cout << "WARNING: No frames captured. Ensure a marker is visible to the device." << endl;
    }

    // -----------------------------------------------------------------------
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
