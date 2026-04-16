#include "geometryHelper.hpp"
#include "helpers.hpp"

#include <ftkInterface.h>

#include <chrono>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <tuple>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

typedef void ( *ReprocessItemFunc )( ftkLibrary lib,
                                     const uint64_t sn,
                                     const ftkFrameQuery* inFrame,
                                     ftkFrameQuery* outFrame );

tuple< bool, string > applyCustomOperationsOnFrame( ftkLibrary lib,
                                                    const uint64_t sn,
                                                    const ftkFrameQuery* inFrame,
                                                    ReprocessItemFunc rawReprocess,
                                                    ReprocessItemFunc fiducialReprocess,
                                                    ftkFrameQuery* outFrame );

int main( int argc, char* argv[] )
{
    const bool isNotFromConsole = isLaunchedFromExplorer();

    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << "This is a demonstration on how to reprocess data with user-defined function operating of data."
         << endl;

    deque< string > args( argv + 1u, argv + argc );

    bool showHelp( find_if( args.cbegin(), args.cend(), []( const string& val ) {
                       return val == "-h" || val == "--help";
                   } ) != args.cend() );

    string cfgFile( "" );
    size_t nbrOfLoops( 10u );
    deque< string > geomFiles{ "geometry072.ini" };

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-c/--config F] " << flush << "JSON config file. Type "
             << "std::string, default = none" << endl;
        cout << setw( 30u ) << "[-l/--loops] " << flush << "Sets the number of loops to perform. Type "
             << "size_t, default = " << nbrOfLoops << endl;
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

    if ( sn == 0uLL )
    {
        cerr << "No devices detected" << endl;
        ftkClose( &lib );
#ifdef ATR_WIN
        if ( isLaunchedFromExplorer() )
        {
            cout << "Press the \"ANY\" key to quit" << endl;
            waitForKeyboardHit();
        }
#endif
        return 1;
    }

    // ------------------------------------------------------------------------
    // Set geometry

    ftkRigidBody geom{};

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

    // ----------------------------------------------------------------------
    // Initialise the frame

    ftkFrameQuery* frame( ftkCreateFrame() );
    if ( frame == nullptr )
    {
        cerr << "Could not allocate basic frame structure" << endl;
        ftkClose( &lib );
#ifdef ATR_WIN
        if ( isLaunchedFromExplorer() )
        {
            cout << "Press the \"ANY\" key to quit" << endl;
            waitForKeyboardHit();
        }
#endif
        return 1;
    }

    ftkError status( ftkSetFrameOptions( true, 5u, 16u, 16u, 32u, 16u, frame ) );
    if ( status != ftkError::FTK_OK )
    {
        cerr << "Could not allocate frame fields" << endl;
        ftkDeleteFrame( frame );
        ftkClose( &lib );
#ifdef ATR_WIN
        if ( isLaunchedFromExplorer() )
        {
            cout << "Press the \"ANY\" key to quit" << endl;
            waitForKeyboardHit();
        }
#endif
        return 1;
    }

    tuple< bool, string > reprocessRetCode{ true, "" };

    // Note: currently to have new ftk3DFiducials, ftkRawData must be changed!
    ReprocessItemFunc oneCentiMetreDisplacement =
      []( ftkLibrary lib, const uint64_t sn, const ftkFrameQuery* in, ftkFrameQuery* out ) {
          const uint32_t nRawLeft( in->rawDataLeftCount );
          const uint32_t nRawRight( in->rawDataRightCount );
          if ( in != out )
          {
              for ( uint32_t i( 0u ); i < nRawLeft; ++i )
              {
                  out->rawDataLeft[ i ] = in->rawDataLeft[ i ];
              }
              for ( uint32_t i( 0u ); i < nRawRight; ++i )
              {
                  out->rawDataRight[ i ] = in->rawDataRight[ i ];
              }
              out->threeDFiducialsCount = in->threeDFiducialsCount;
          }
          ftk3DPoint leftPt{}, rightPt{};

          for ( uint32_t i( 0u ); i < in->threeDFiducialsCount; ++i )
          {
              ftk3DFiducial& pt = in->threeDFiducials[ i ];
              pt.positionMM.x += 10.f;
              if ( pt.leftIndex < nRawLeft && pt.rightIndex < nRawRight )
              {
                  if ( ftkReprojectPoint( lib, sn, &pt.positionMM, &leftPt, &rightPt ) == ftkError::FTK_OK )
                  {
                      out->rawDataLeft[ pt.leftIndex ] = in->rawDataLeft[ pt.leftIndex ];
                      out->rawDataLeft[ pt.leftIndex ].centerXPixels = leftPt.x;
                      out->rawDataLeft[ pt.leftIndex ].centerYPixels = leftPt.y;

                      out->rawDataRight[ pt.rightIndex ] = in->rawDataRight[ pt.rightIndex ];
                      out->rawDataRight[ pt.rightIndex ].centerXPixels = rightPt.x;
                      out->rawDataRight[ pt.rightIndex ].centerYPixels = rightPt.y;
                  }
              }
          }
          out->threeDFiducialsCount = 0u;
          out->markersCount = 0u;
      };

    ReprocessItemFunc onePixelDisplacementOnRight =
      []( ftkLibrary, const uint64_t, const ftkFrameQuery* in, ftkFrameQuery* out ) {
          if ( in == out )
          {
              for ( uint32_t i( 0u ); i < out->rawDataRightCount; ++i )
              {
                  out->rawDataRight[ i ].centerXPixels += 1.f;
              }
          }
          else
          {
              out->rawDataLeftCount = in->rawDataLeftCount;
              for ( uint32_t i( 0u ); i < in->rawDataLeftCount; ++i )
              {
                  out->rawDataLeft[ i ] = in->rawDataLeft[ i ];
              }
              out->rawDataRightCount = in->rawDataRightCount;
              for ( uint32_t i( 0u ); i < in->rawDataRightCount; ++i )
              {
                  out->rawDataRight[ i ] = in->rawDataRight[ i ];
                  in->rawDataRight[ i ].centerXPixels += 1.f;
              }
          }
          out->threeDFiducialsCount = 0u;
          out->markersCount = 0u;
      };

    uint32 iMarker( numeric_limits< uint32 >::max() );
    bool displayMarker( false );

    for ( size_t i( 0u ); i < nbrOfLoops; ++i )
    {
        cout << "------------------------------------------------------------" << endl << endl;
        this_thread::sleep_for( chrono::seconds( 1u ) );
        status = ftkGetLastFrame( lib, sn, frame, 100u );
        if ( status > ftkError::FTK_OK )
        {
            status = ftkGetLastErrorString( lib, sizeof( buffer.data ), buffer.data );
            if ( status != ftkError::FTK_OK )
            {
                cerr << "Unknown error getting last frame" << endl;
            }
            else
            {
                cerr << buffer.data << endl;
            }
            continue;
        }
        displayMarker = frame->markersCount > 0;
        if ( displayMarker )
        {
            cout << "Input markers:" << endl;
            for ( iMarker = 0u; iMarker < frame->markersCount; ++iMarker )
            {
                cout << "\t" << iMarker << " geom ID " << frame->markers[ iMarker ].geometryId << ", [";
                for ( float val : frame->markers[ iMarker ].translationMM )
                {
                    cout << " " << val;
                }
                cout << "]" << endl;
            }
        }
        else
        {
            cout << "Input 3D fiducials:" << endl;
            for ( iMarker = 0u; iMarker < frame->threeDFiducialsCount; ++iMarker )
            {
                cout << "\t" << iMarker << " [";
                cout << " " << frame->threeDFiducials[ iMarker ].positionMM.x;
                cout << " " << frame->threeDFiducials[ iMarker ].positionMM.y;
                cout << " " << frame->threeDFiducials[ iMarker ].positionMM.z;
                cout << "]" << endl;
            }
        }

        // Insert here reprocessing function(s)
        reprocessRetCode =
          applyCustomOperationsOnFrame( lib, sn, frame, oneCentiMetreDisplacement, nullptr, frame );
        if ( !get< 0u >( reprocessRetCode ) )
        {
            cerr << get< 1u >( reprocessRetCode ) << endl;
            continue;
        }

        status = ftkReprocessFrame( lib, sn, frame );
        if ( status > ftkError::FTK_OK )
        {
            status = ftkGetLastErrorString( lib, sizeof( buffer.data ), buffer.data );
            if ( status != ftkError::FTK_OK )
            {
                cerr << "Unknown error reprocessing frame" << endl;
            }
            else
            {
                cerr << buffer.data << endl;
            }
            continue;
        }
        else
        {
            cout << "Reprocessed successfully frame " << i << endl;
        }

        if ( displayMarker )
        {
            cout << "Output markers:" << endl;
            for ( iMarker = 0u; iMarker < frame->markersCount; ++iMarker )
            {
                cout << "\t" << iMarker << " geom ID " << frame->markers[ iMarker ].geometryId << ", [";
                for ( float val : frame->markers[ iMarker ].translationMM )
                {
                    cout << " " << val;
                }
                cout << "]" << endl;
            }
        }
        else
        {
            cout << "Input 3D fiducials:" << endl;
            for ( iMarker = 0u; iMarker < frame->threeDFiducialsCount; ++iMarker )
            {
                cout << "\t" << iMarker << " [";
                cout << " " << frame->threeDFiducials[ iMarker ].positionMM.x;
                cout << " " << frame->threeDFiducials[ iMarker ].positionMM.y;
                cout << " " << frame->threeDFiducials[ iMarker ].positionMM.z;
                cout << "]" << endl;
            }
        }
    }

    ftkDeleteFrame( frame );
    ftkClose( &lib );
#ifdef ATR_WIN
    if ( isLaunchedFromExplorer() )
    {
        cout << "Press the \"ANY\" key to quit" << endl;
        waitForKeyboardHit();
    }
#endif
    return 0;
}

tuple< bool, string > applyCustomOperationsOnFrame( ftkLibrary lib,
                                                    const uint64_t sn,
                                                    const ftkFrameQuery* inFrame,
                                                    ReprocessItemFunc rawReprocess,
                                                    ReprocessItemFunc fiducialReprocess,
                                                    ftkFrameQuery* outFrame )
{
    tuple< bool, string > result{ true, "" };
    string errMsg( "" );
    if ( inFrame == nullptr )
    {
        get< 0u >( result ) = false;
        get< 1u >( result ) += "inFrame pointer is nullptr\n";
    }
    if ( outFrame == nullptr )
    {
        get< 0u >( result ) = false;
        get< 1u >( result ) += "outFrame pointer is nullptr\n";
    }

    if ( get< 0u >( result ) )
    {
        if ( rawReprocess != nullptr )
        {
            // Reprocess left and right raw from picture
            if ( inFrame->imageLeftStat == ftkQueryStatus::QS_OK &&
                 outFrame->imageLeftVersionSize == inFrame->imageLeftVersionSize &&
                 inFrame->imageRightStat == ftkQueryStatus::QS_OK &&
                 outFrame->imageRightVersionSize == inFrame->imageRightVersionSize )
            {
                rawReprocess( lib, sn, inFrame, outFrame );
                outFrame->threeDFiducialsStat = ftkQueryStatus::QS_REPROCESS;
                outFrame->markersStat = ftkQueryStatus::QS_REPROCESS;
            }
            else
            {
                if ( inFrame->imageLeftStat != ftkQueryStatus::QS_OK )
                {
                    get< 0u >( result ) = false;
                    get< 1u >( result ) += "inFrame->imageLeftPixels was not retrieved\n";
                }
                if ( outFrame->imageLeftVersionSize != inFrame->imageLeftVersionSize )
                {
                    get< 0u >( result ) = false;
                    get< 1u >( result ) += "outFrame->imageLeftPixels was not correctly initialised\n";
                }
                if ( inFrame->imageRightStat != ftkQueryStatus::QS_OK )
                {
                    get< 0u >( result ) = false;
                    get< 1u >( result ) += "inFrame->imageRightPixels was not retrieved\n";
                }
                if ( outFrame->imageRightVersionSize != inFrame->imageRightVersionSize )
                {
                    get< 0u >( result ) = false;
                    get< 1u >( result ) += "outFrame->imageRightPixels was not correctly initialised\n";
                }
            }
        }
        // Reprocess 3D fiducials
        if ( fiducialReprocess != nullptr )
        {
            if ( inFrame->threeDFiducialsStat == ftkQueryStatus::QS_OK &&
                 outFrame->threeDFiducialsVersionSize == inFrame->threeDFiducialsVersionSize )
            {
                fiducialReprocess( lib, sn, inFrame, outFrame );
                outFrame->markersStat = ftkQueryStatus::QS_REPROCESS;
            }
            else
            {
                if ( inFrame->imageRightStat != ftkQueryStatus::QS_OK )
                {
                    get< 0u >( result ) = false;
                    get< 1u >( result ) += "inFrame->imageRightPixels was not retrieved\n";
                }
                if ( outFrame->threeDFiducialsVersionSize != inFrame->threeDFiducialsVersionSize )
                {
                    get< 0u >( result ) = false;
                    get< 1u >( result ) += "outFrame->threeDFiducials was not correctly initialised\n";
                }
            }
        }
    }

    return result;
}
