// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo32_InterpolateFrames.cpp
 *   \brief Demonstrates how to interpolate the marker position in between two frames.
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

#include <ftkInterface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <iomanip>
#include <iostream>
#include <set>
#include <string>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

enum class InterpolateStatus : int32_t
{
    Ok = 0,
    InvalidPointer = 1,
    InvalidFrames = 2,
    InvalidTime = 3,
    InvalidMatching = 10,
    ReprocessingError = 20
};

InterpolateStatus interpolateFrames( ftkLibrary lib,
                                     const uint64_t sn,
                                     const ftkFrameQuery* before,
                                     const ftkFrameQuery* after,
                                     const uint64_t intermediateTime,
                                     ftkFrameQuery* computed,
                                     bool debug = false );

void displayMarkerFrames( const ftkFrameQuery* frame );

int main( int argc, char* argv[] )
{
    bool isNotFromConsole( isLaunchedFromExplorer() );

    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif
    cout << "This is a demonstration on how to interpolate the marker position in between two frames."
         << endl;

    deque< string > args( argv + 1u, argv + argc );

    bool showHelp( find_if( args.cbegin(), args.cend(), []( const string& val ) {
                       return val == "-h" || val == "--help";
                   } ) != args.cend() );

    string cfgFile( "" );

    bool debug( find_if( args.cbegin(), args.cend(), []( const string& val ) {
                    return val == "-d" || val == "--debug";
                } ) != args.cend() );

    deque< string > geomFiles{ "geometry072.ini" };

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-d/--debug] " << flush << "Sets debug mode (more output)." << endl;
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

    // ----------------------------------------------------------------------
    // Initialize driver

    ftkBuffer buffer{};

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

    // ------------------------------------------------------------------------
    // Set geometry

    ftkRigidBody geom;

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

#ifdef ATR_WIN
            cout << endl << " *** Hit a key to exit ***" << endl;
            waitForKeyboardHit();
#endif
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // Initialize the frames to get marker pose

    array< ftkFrameQuery*, 3u > frames{};
    ftkError err( ftkError::FTK_OK );

    for ( auto& frame : frames )
    {
        frame = ftkCreateFrame();

        if ( frame == nullptr )
        {
            error( "Cannot create frame instance", !isNotFromConsole );
        }

        err = ftkSetFrameOptions( false, false, 16u, 16u, 16u, 16u, frame );

        if ( err != ftkError::FTK_OK )
        {
            ftkDeleteFrame( frame );
            checkError( lib, !isNotFromConsole );
        }
    }

    uint32 counter( 10u );

    cout.setf( std::ios::fixed, std::ios::floatfield );

    set< ftkError > allowedStatuses{ ftkError::FTK_OK, ftkError::FTK_WAR_TEMP_INVALID };

    uint64_t intermediateTime;
    InterpolateStatus statusOfInterpolation;

    ftkFrameQuery* lastFrame( frames[ 0u ] );
    ftkFrameQuery* currentFrame( frames[ 1u ] );
    ftkFrameQuery* interpolatedFrame( frames[ 2u ] );

    for ( uint32 u( 0u ); u < 100u; ++u )
    {
        /* block up to 100 ms if next frame is not available*/
        err = ftkGetLastFrame( lib, sn, currentFrame, 100u );

        if ( allowedStatuses.count( err ) == 0u )
        {
            continue;
        }

        if ( lastFrame->imageHeader->counter == 0u )
        {
            swap( currentFrame, lastFrame );
            continue;
        }

        intermediateTime =
          ( lastFrame->imageHeader->timestampUS + currentFrame->imageHeader->timestampUS ) / 2uLL;

        cout << setfill( ' ' );
        cout << setw( 35u ) << "Initial"
             << " " << setw( 31u ) << "Final"
             << " " << setw( 31u ) << "Interpolated" << endl;
        cout << setw( 35u ) << lastFrame->imageHeader->timestampUS << " " << setw( 31u )
             << currentFrame->imageHeader->timestampUS << " " << setw( 31u ) << intermediateTime << endl;

        statusOfInterpolation =
          interpolateFrames( lib, sn, lastFrame, currentFrame, intermediateTime, frames.at( 2u ), debug );

        if ( statusOfInterpolation != InterpolateStatus::Ok )
        {
            swap( currentFrame, lastFrame );
            continue;
        }

        cout.setf( ios::fixed, ios::floatfield );
        cout.precision( 3u );
        cout << setfill( ' ' );

        const uint32_t nMrk( max( max( frames.at( 0u )->markersCount, frames.at( 1u )->markersCount ),
                                  frames.at( 2u )->markersCount ) );

        cout << "Markers:" << endl;

        for ( uint32_t iMrk( 0u ), iFrame; iMrk < nMrk; ++iMrk )
        {
            cout << setw( 3u ) << iMrk;
            for ( iFrame = 0u; iFrame < 3u; ++iFrame )
            {
                cout << "  [";
                if ( iMrk < frames.at( iFrame )->markersCount )
                {
                    cout << setw( 8u ) << frames.at( iFrame )->markers[ iMrk ].translationMM[ 0u ] << ", "
                         << setw( 8u ) << frames.at( iFrame )->markers[ iMrk ].translationMM[ 1u ] << ", "
                         << setw( 8u ) << frames.at( iFrame )->markers[ iMrk ].translationMM[ 2u ] << "]";
                }
                else
                {
                    cout << setw( 8u ) << "--"
                         << ", " << setw( 8u ) << "--"
                         << ", " << setw( 8u ) << "--"
                         << "]";
                }
            }
            cout << endl;
        }

        cout << endl << "-----------------------" << endl;

        cout.unsetf( ios::floatfield );

        if ( --counter == 0uLL )
        {
            break;
        }

        swap( currentFrame, lastFrame );

        sleep( 1000L );
    }

    for ( auto& frame : frames )
    {
        ftkDeleteFrame( frame );
    }
    ftkClose( &lib );
    return 0;
}

float computeNorm( const float ( &a )[ 3u ], const float ( &b )[ 3u ] );

bool seedFrameFromFoundMarker( const ftkFrameQuery* before,
                               const ftkFrameQuery* after,
                               const uint64_t intermediateTime,
                               const ftkMarker* marker1,
                               const ftkMarker* marker2,
                               ftkFrameQuery* computed,
                               bool debug );

InterpolateStatus interpolateFrames( ftkLibrary lib,
                                     const uint64_t sn,
                                     const ftkFrameQuery* before,
                                     const ftkFrameQuery* after,
                                     const uint64_t intermediateTime,
                                     ftkFrameQuery* computed,
                                     bool debug )
{
    bool allOk( true );
    if ( before == nullptr )
    {
        cerr << "Initial frame is nullptr" << endl;
        allOk = false;
    }
    if ( after == nullptr )
    {
        cerr << "Final frame is nullptr" << endl;
        allOk = false;
    }
    if ( computed == nullptr )
    {
        cerr << "Target frame is nullptr" << endl;
        allOk = false;
    }

    if ( !allOk )
    {
        return InterpolateStatus::InvalidPointer;
    }

    if ( before->imageHeaderStat != ftkQueryStatus::QS_OK )
    {
        cerr << "Initial frame has invalid image header" << endl;
        allOk = false;
    }
    if ( before->rawDataLeftStat != ftkQueryStatus::QS_OK )
    {
        cerr << "Initial frame has invalid raw left data" << endl;
        allOk = false;
    }
    if ( before->rawDataRightStat != ftkQueryStatus::QS_OK )
    {
        cerr << "Initial frame has invalid raw right data" << endl;
        allOk = false;
    }
    if ( before->threeDFiducialsStat != ftkQueryStatus::QS_OK )
    {
        cerr << "Initial frame has invalid fiducials data" << endl;
        allOk = false;
    }
    if ( before->markersStat != ftkQueryStatus::QS_OK )
    {
        cerr << "Initial frame has invalid marker data" << endl;
        allOk = false;
    }
    if ( after->imageHeaderStat != ftkQueryStatus::QS_OK )
    {
        cerr << "Final frame has invalid image header" << endl;
        allOk = false;
    }
    if ( after->rawDataLeftStat != ftkQueryStatus::QS_OK )
    {
        cerr << "Final frame has invalid raw left data" << endl;
        allOk = false;
    }
    if ( after->rawDataRightStat != ftkQueryStatus::QS_OK )
    {
        cerr << "Final frame has invalid raw right data" << endl;
        allOk = false;
    }
    if ( after->threeDFiducialsStat != ftkQueryStatus::QS_OK )
    {
        cerr << "Final frame has invalid fiducials data" << endl;
        allOk = false;
    }
    if ( after->markersStat != ftkQueryStatus::QS_OK )
    {
        cerr << "Final frame has invalid marker data" << endl;
        allOk = false;
    }

    if ( !allOk )
    {
        return InterpolateStatus::InvalidFrames;
    }

    if ( after->imageHeader->timestampUS <= before->imageHeader->timestampUS )
    {
        cerr << "First frame was acquired after the second one" << endl;
        allOk = false;
    }
    if ( before->imageHeader->timestampUS > intermediateTime )
    {
        cerr << "Intermediate time is before first frame" << endl;
        allOk = false;
    }
    if ( after->imageHeader->timestampUS < intermediateTime )
    {
        cerr << "Intermediate time is after second frame" << endl;
        allOk = false;
    }

    // Find the markers that are on both of the two input frames
    const uint32_t nMarkerFrame1( before->markersCount );
    const uint32_t nMarkerFrame2( after->markersCount );
    const ftkMarker* marker1( before->markers );
    const ftkMarker* marker2( nullptr );

    // We want to copy the header data from the latest frame (and not moving the pointer).
    *( computed->imageHeader ) = *( after->imageHeader );
    computed->rawDataLeftCount = 0u;
    computed->rawDataRightCount = 0u;
    computed->threeDFiducialsCount = 0u;
    computed->markersCount = 0u;

    for ( uint32_t frame1MarkerIt( 0u ), frame2MarkerIt; frame1MarkerIt < nMarkerFrame1;
          ++frame1MarkerIt, ++marker1 )
    {
        marker2 = after->markers;
        for ( frame2MarkerIt = 0u; frame2MarkerIt != nMarkerFrame2; ++frame2MarkerIt, ++marker2 )
        {
            if ( marker1->geometryId != marker2->geometryId )
            {
                continue;
            }
            if ( marker1->id == marker2->id )
            {
                // Found a match!
                cout << "Found matching by tracking ID" << endl;
                if ( !seedFrameFromFoundMarker(
                       before, after, intermediateTime, marker1, marker2, computed, debug ) )
                {
                    return InterpolateStatus::InvalidMatching;
                }
                break;
            }
            if ( marker1->geometryPresenceMask && marker2->geometryPresenceMask &&
                 computeNorm( marker1->translationMM, marker2->translationMM ) < 1.e-1f &&
                 computeNorm( marker1->rotation[ 0u ], marker2->rotation[ 0u ] ) < 1.e-1f &&
                 computeNorm( marker1->rotation[ 1u ], marker2->rotation[ 1u ] ) < 1.e-1f &&
                 computeNorm( marker1->rotation[ 2u ], marker2->rotation[ 2u ] ) < 1.e-1f )
            {
                // Found a match!
                cout << "Found matching by position and rotation" << endl;
                if ( !seedFrameFromFoundMarker(
                       before, after, intermediateTime, marker1, marker2, computed, debug ) )
                {
                    return InterpolateStatus::InvalidMatching;
                }
                break;
            }
            else
            {
                cerr << "No matches found" << endl;
            }
        }
    }

    computed->rawDataLeftStat = ftkQueryStatus::QS_OK;
    computed->rawDataRightStat = ftkQueryStatus::QS_OK;
    computed->threeDFiducialsStat = ftkQueryStatus::QS_REPROCESS;
    computed->markersStat = ftkQueryStatus::QS_REPROCESS;

    ftkError err( ftkReprocessFrame( lib, sn, computed ) );

    if ( err != ftkError::FTK_OK )
    {
        return InterpolateStatus::ReprocessingError;
    }

    if ( debug )
    {
        cout << setfill( ' ' );
        cout.setf( ios::fixed, ios::floatfield );
        cout.precision( 3u );

        cout << "3D fids:" << endl;

        const uint32_t nMarkers(
          max( max( before->markersCount, after->markersCount ), computed->markersCount ) );

        for ( uint32_t iMrk( 0u ); iMrk < nMarkers; ++iMrk )
        {
            for ( uint32_t iFid( 0u ); iFid < FTK_MAX_FIDUCIALS; ++iFid )
            {
                if ( before->markers[ iMrk ].fiducialCorresp[ iFid ] == 0xFFFFFFFFu )
                {
                    continue;
                }
                cout << setw( 3u ) << iFid << "  [";
                cout
                  << setw( 8u )
                  << before->threeDFiducials[ before->markers[ iMrk ].fiducialCorresp[ iFid ] ].positionMM.x
                  << ", " << setw( 8u )
                  << before->threeDFiducials[ before->markers[ iMrk ].fiducialCorresp[ iFid ] ].positionMM.y
                  << ", " << setw( 8u )
                  << before->threeDFiducials[ before->markers[ iMrk ].fiducialCorresp[ iFid ] ].positionMM.z
                  << "]";
                cout << "  ["; /*
                 if ( iFid >= after->threeDFiducialsCount )
                 {
                     cout << setw( 8u ) << "--" << ", " << setw( 8u ) << "--" <<
                         ", "
                          << setw( 8u ) << "--" << "]";
                 }
                 else
                 {*/
                cout << setw( 8u )
                     << after->threeDFiducials[ after->markers[ iMrk ].fiducialCorresp[ iFid ] ].positionMM.x
                     << ", " << setw( 8u )
                     << after->threeDFiducials[ after->markers[ iMrk ].fiducialCorresp[ iFid ] ].positionMM.y
                     << ", " << setw( 8u )
                     << after->threeDFiducials[ after->markers[ iMrk ].fiducialCorresp[ iFid ] ].positionMM.z
                     << "]";
                // }
                cout << "  ["; /*
                 if ( iFid >= computed->threeDFiducialsCount )
                 {
                     cout << setw( 8u ) << "--" << ", " << setw( 8u ) << "--" <<
                         ", "
                          << setw( 8u ) << "--" << "]";
                 }
                 else
                 {*/
                cout << setw( 8u )
                     << computed->threeDFiducials[ computed->markers[ iMrk ].fiducialCorresp[ iFid ] ]
                          .positionMM.x
                     << ", " << setw( 8u )
                     << computed->threeDFiducials[ computed->markers[ iMrk ].fiducialCorresp[ iFid ] ]
                          .positionMM.y
                     << ", " << setw( 8u )
                     << computed->threeDFiducials[ computed->markers[ iMrk ].fiducialCorresp[ iFid ] ]
                          .positionMM.z
                     << "]";
                // }
                cout << " marker " << iMrk << endl;
            }
        }
    }

    cout.unsetf( ios::floatfield );

    computed->imageHeader->timestampUS = intermediateTime;

    return InterpolateStatus::Ok;
}

void displayMarkerFrames( const ftkFrameQuery* frame )
{
    cout << "Frame " << frame->imageHeader->timestampUS << endl;
    cout << fixed << setprecision( 3u );
    for ( uint32_t i( 0u ); i < frame->markersCount; ++i )
    {
        const ftkMarker& theMarker = frame->markers[ i ];
        cout << "  Marker " << i << " geom ID " << theMarker.geometryId << ", position = ["
             << theMarker.translationMM[ 0u ] << ", " << theMarker.translationMM[ 1u ] << ", "
             << theMarker.translationMM[ 2u ] << "]" << endl;
    }
    cout.unsetf( std::ios::floatfield );
}

float computeNorm( const float ( &a )[ 3u ], const float ( &b )[ 3u ] );

float interpolate( const float a, const float b, const float time );

bool seedFrameFromFoundMarker( const ftkFrameQuery* before,
                               const ftkFrameQuery* after,
                               const uint64_t intermediateTime,
                               const ftkMarker* marker1,
                               const ftkMarker* marker2,
                               ftkFrameQuery* computed,
                               bool debug = false )
{
    const float factor(
      static_cast< float >( intermediateTime - before->imageHeader->timestampUS ) /
      static_cast< float >( after->imageHeader->timestampUS - before->imageHeader->timestampUS ) );

    if ( debug )
    {
        cout << setfill( ' ' );
        cout.setf( ios::fixed, ios::floatfield );
        cout.precision( 3u );

        cout << "2D fids:" << endl;
    }

    for ( uint32_t i( 0u ); i < FTK_MAX_FIDUCIALS; ++i )
    {
        if ( marker1->fiducialCorresp[ i ] == 0xFFFFFFFFu )
        {
            continue;
        }
        if ( marker1->fiducialCorresp[ i ] >= before->threeDFiducialsCount )
        {
            return false;
        }
        const ftk3DFiducial& beforeFid = before->threeDFiducials[ marker1->fiducialCorresp[ i ] ];
        if ( marker2->fiducialCorresp[ i ] == 0xFFFFFFFFu )
        {
            continue;
        }
        if ( marker2->fiducialCorresp[ i ] >= after->threeDFiducialsCount )
        {
            return false;
        }
        const ftk3DFiducial& afterFid = after->threeDFiducials[ marker2->fiducialCorresp[ i ] ];

        if ( beforeFid.leftIndex >= before->rawDataLeftCount )
        {
            return false;
        }
        if ( beforeFid.rightIndex >= before->rawDataRightCount )
        {
            return false;
        }
        const ftkRawData& beforeLeftRaw = before->rawDataLeft[ beforeFid.leftIndex ];
        const ftkRawData& beforeRightRaw = before->rawDataRight[ beforeFid.rightIndex ];

        if ( afterFid.leftIndex >= after->rawDataLeftCount )
        {
            return false;
        }
        if ( afterFid.rightIndex >= after->rawDataRightCount )
        {
            return false;
        }
        const ftkRawData& afterLeftRaw = after->rawDataLeft[ afterFid.leftIndex ];
        const ftkRawData& afterRightRaw = after->rawDataRight[ afterFid.rightIndex ];

        if ( computed->rawDataLeftCount >=
             computed->rawDataLeftVersionSize.ReservedSize / sizeof( ftkRawData ) )
        {
            return false;
        }
        if ( computed->rawDataRightCount >=
             computed->rawDataRightVersionSize.ReservedSize / sizeof( ftkRawData ) )
        {
            return false;
        }

        computed->rawDataLeft[ computed->rawDataLeftCount ] = beforeLeftRaw;
        computed->rawDataLeft[ computed->rawDataLeftCount ].centerXPixels =
          interpolate( beforeLeftRaw.centerXPixels, afterLeftRaw.centerXPixels, factor );
        computed->rawDataLeft[ computed->rawDataLeftCount ].centerYPixels =
          interpolate( beforeLeftRaw.centerYPixels, afterLeftRaw.centerYPixels, factor );

        computed->rawDataRight[ computed->rawDataRightCount ] = beforeRightRaw;
        computed->rawDataRight[ computed->rawDataRightCount ].centerXPixels =
          interpolate( beforeRightRaw.centerXPixels, afterRightRaw.centerXPixels, factor );
        computed->rawDataRight[ computed->rawDataRightCount ].centerYPixels =
          interpolate( beforeRightRaw.centerYPixels, afterRightRaw.centerYPixels, factor );

        if ( debug )
        {
            cout << setw( 3u ) << i << setw( 10u ) << "L"
                 << "  [" << setw( 8u ) << beforeLeftRaw.centerXPixels << ", " << setw( 8u )
                 << beforeLeftRaw.centerYPixels << "] " << setw( 10u ) << " "
                 << " [" << setw( 8u ) << afterLeftRaw.centerXPixels << ", " << setw( 8u )
                 << afterLeftRaw.centerYPixels << "] " << setw( 10u ) << " "
                 << " [" << setw( 8u ) << computed->rawDataLeft[ computed->rawDataLeftCount ].centerXPixels
                 << ", " << setw( 8u ) << computed->rawDataLeft[ computed->rawDataLeftCount ].centerYPixels
                 << "]" << endl;

            cout << setw( 3u ) << i << setw( 10u ) << "R"
                 << "  [" << setw( 8u ) << beforeRightRaw.centerXPixels << ", " << setw( 8u )
                 << beforeRightRaw.centerYPixels << "] " << setw( 10u ) << " "
                 << " [" << setw( 8u ) << afterRightRaw.centerXPixels << ", " << setw( 8u )
                 << afterRightRaw.centerYPixels << "] " << setw( 10u ) << " "
                 << " [" << setw( 8u ) << computed->rawDataRight[ computed->rawDataRightCount ].centerXPixels
                 << ", " << setw( 8u ) << computed->rawDataRight[ computed->rawDataRightCount ].centerYPixels
                 << "]" << endl;
        }

        ++computed->rawDataLeftCount;
        ++computed->rawDataRightCount;
    }

    if ( debug )
    {
        cout.unsetf( ios::floatfield );
    }

    return true;
}

float computeNorm( const float ( &a )[ 3u ], const float ( &b )[ 3u ] )
{
    float result( 0.0f );
    for ( size_t i( 0u ); i < 3u; ++i )
    {
        result += ( a[ i ] - b[ i ] ) * ( a[ i ] - b[ i ] );
    }

    return sqrt( result );
}

float interpolate( const float a, const float b, const float time )
{
    return a * time + b * ( 1.f - time );
}
