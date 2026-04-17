// ============================================================================

/*!
 *
 *   This file is part of the Atracsys fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo20_DiffLatency.cpp
 *   \brief Check the acquisition latency and update rate with heavy pictures
 *
 *   How to compile this example:
 *   - Install the fusionTrack driver (see documentation)
 *   - Add fusionTrack driver  "include/api/" directory in your project
 *   - Add this file in your project
 *
 *   How to run this example:
 *   - Install the fusionTrack driver (see documentation)
 *   - Switch on device
 *   - Run the resulting executable
 *
 *   Comment :
 *   - The generation of heavy pictures is currently generated manually by an operator waving a slip of paper
 * in front of the cameras
 *   -  generation of heavy pictures by lowering back and forth the image compression threshold may not
 * work because it takes several milliseconds for each command setting the threshold. Therefore it may slow
 * down the update period artificially.
 *
 */
// ===========================================================================
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

// Function to read last received frame and update some time statistics
void getDataAndIncrementCounters( ftkLibrary lib,
                                  uint64 sn,
                                  ftkFrameQuery* frame,
                                  uint64& OKFrameTimeStamp,
                                  chrono::time_point< chrono::high_resolution_clock >& PCTime,
                                  chrono::time_point< chrono::high_resolution_clock >& lastOKFramePCTime,
                                  uint32& frameId,
                                  vector< double >& ftkPeriodStat,
                                  vector< double >& responseTimeStat,
                                  vector< double >& PCextraTimeStatRaw,
                                  vector< double >& ftkSendingPeriodStat,
                                  uint32& lostCounter,
                                  uint32& sameFrame,
                                  uint32& receivedFrames,
                                  uint32& notProcessedCounter,
                                  uint32& ignoredFrames,
                                  uint32& droppedFrames,
                                  uint32 timeOut,
                                  bool process = true );

template< typename T >
void computeStats( const vector< T >& data,
                   double& mean,
                   double& stdDev,
                   double& minValue,
                   double& maxValue,
                   double& medianValue );

// ---------------------------------------------------------------------------
// main function

int main( int argc, char** argv )
{
    // ----------------------------------------------------------------------
    // How much time without receiving any update from the tracker [ms] can you tolerate?

    double maxUpdateTime( 30000. );
    double maxLatencyTime( 20000. );
    int32 picMaxSize( 153600 );
    uint32 nbLoops( 100u );
    uint32 nbFrames( 1000u );
    string cfgFile( "" );

    cout << "This is the Atracsys latency test and update rate program." << endl;

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

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-f/--nbframes M] " << flush
             << "Number of frames recorded for a measure. The raw and "
             << "effective acquisition rates (mean and std) are computed. Type "
             << "uint32, default = " << nbFrames << endl;
        cout << setw( 30u ) << "[-n/--nbloops N] "
             << "Number of measures. At each measure, nbframes are captured."
             << "Type uint32, default = " << nbLoops << endl;
        cout << setw( 30u ) << "[-u/--maxUpdateTime T] "
             << "Tolerance on the maximal time allowed without receiving any "
             << "update from the tracker in microseconds. Type double, "
             << "default = " << maxUpdateTime << endl;
        cout << setw( 30u ) << "[-l/--maxLatencyTime T] "
             << "Tolerance on the maximal allowed difference between the "
             << "period separating two consecutive transmitted frames on the "
             << "camera sensor level and the corresponding period after "
             << "delivering to the PC user. Type float, default = " << maxLatencyTime << endl;
        cout << setw( 30u ) << "[-s/--picMaxSize S] "
             << "Maximal size of a picture, above which the tracking device "
             << "does not send it to the PC. Type uint32, default = " << picMaxSize << endl;
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
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-f" || val == "--nbframes"; } ) );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        try
        {
            nbFrames = stoul( *pos );
        }
        catch ( invalid_argument& e )
        {
            cerr << e.what() << endl;
            error( "Cannot init. default nb frames per measure." );
        }
        catch ( out_of_range& e )
        {
            cerr << e.what() << endl;
            error( "Cannot init. default nb frames per measure." );
        }
    }

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-n" || val == "--nbloops"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        try
        {
            nbLoops = stoul( *pos );
        }
        catch ( invalid_argument& e )
        {
            cerr << e.what() << endl;
            error( "Cannot init. default nb loops." );
        }
        catch ( out_of_range& e )
        {
            cerr << e.what() << endl;
            error( "Cannot init. default nb loops." );
        }
    }

    pos = find_if( args.cbegin(), args.cend(), []( const string& val ) {
        return val == "-u" || val == "--maxUpdateTime";
    } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        try
        {
            maxUpdateTime = stod( *pos );
        }
        catch ( invalid_argument& e )
        {
            cerr << e.what() << endl;
            error( "Cannot read max update time." );
        }
        catch ( out_of_range& e )
        {
            cerr << e.what() << endl;
            error( "Cannot read max update time." );
        }
    }

    pos = find_if( args.cbegin(), args.cend(), []( const string& val ) {
        return val == "-l" || val == "--maxLatencyTime";
    } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        try
        {
            maxLatencyTime = stod( *pos );
        }
        catch ( invalid_argument& e )
        {
            cerr << e.what() << endl;
            error( "Cannot read max latency." );
        }
        catch ( out_of_range& e )
        {
            cerr << e.what() << endl;
            error( "Cannot read max latency." );
        }
    }

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-s" || val == "--picMaxSize"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        try
        {
            picMaxSize = stoi( *pos );
        }
        catch ( invalid_argument& e )
        {
            cerr << e.what() << endl;
            error( "Cannot read max picture size." );
        }
        catch ( out_of_range& e )
        {
            cerr << e.what() << endl;
            error( "Cannot read max picture size." );
        }
    }

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-c" || val == "--config"; } );
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
    // Initialize the frame to get marker pose

    ftkFrameQuery* frame = ftkCreateFrame();
    if ( frame == 0 )
    {
        ftkClose( &lib );
        return 1;
    }

    // Reserve enough space for 2D fiducials, 3D fiducials and reconstructed markers:
    if ( ftkSetFrameOptions( false, 0u, 0u, 0u, 1000u, 100u, frame ) != ftkError::FTK_OK )
    {
        ftkDeleteFrame( frame );
        ftkClose( &lib );
        return 1;
    }

    // set options for data acquisition

    map< string, uint32 > options{};

    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &options ) );
    if ( options.empty() )
    {
        ftkClose( &lib );
        cerr << "Could not find any options" << endl;
        return 1;
    }

    if ( err != ftkError::FTK_OK )
    {
        checkError( lib );
    }

    bool neededOptionsPresent( true );
    string errMsg( "Could not find following option(s):" );
    for ( const string& optName : { "Image Integration Time",
                                    "Image Compression Threshold",
                                    "Picture rejection threshold",
                                    "Acquisition frequency" } )
    {
        if ( options.find( optName ) == options.cend() )
        {
            neededOptionsPresent = false;
            errMsg += " '" + optName + "'";
        }
    }

    if ( !neededOptionsPresent )
    {
        ftkClose( &lib );
        cerr << errMsg << endl;
        return 1;
    }

    ftkError status = ftkError::FTK_ERR_INTERNAL;
    if ( ftkDeviceType::DEV_FUSIONTRACK_500 == device.Type ||
         ftkDeviceType::DEV_FUSIONTRACK_250 == device.Type )
    {
        status = ftkSetInt32( lib, sn, options[ "Picture rejection threshold" ], picMaxSize );
        if ( status != ftkError::FTK_OK )
        {
            error( "Cannot set Picture rejection threshold" );
        }
    }

    int32 frequency( 0 );
    status =
      ftkGetInt32( lib, sn, options[ "Acquisition frequency" ], &frequency, ftkOptionGetter::FTK_VALUE );
    if ( status != ftkError::FTK_OK )
    {
        error( "Cannot read device frequency" );
    }

    // Stat accumulators keep track of the mean, min and max of a population:
    vector< double > statEff, ftkPeriodStat, responseTimeStat, PCExtraTimeStat, ftkSendingPeriodStat;

    statEff.reserve( nbLoops * nbFrames );
    ftkPeriodStat.reserve( nbLoops * nbFrames );
    responseTimeStat.reserve( nbLoops * nbFrames );
    PCExtraTimeStat.reserve( nbLoops * nbFrames );
    ftkSendingPeriodStat.reserve( nbLoops * nbFrames );

    // Get a first frame
    status = ftkGetLastFrame( lib, sn, frame, 3000 );

    uint32 frameId( frame->imageHeader->counter ), lostCounter( 0u ), notProcessedCounter, sameFrame;
    uint64 timeStamp( 0uLL );
    chrono::time_point< chrono::high_resolution_clock > PCStartTime, lastOKFramePCTime;

    uint32 receivedFrames( 0u ), ignoredFrames( 0u ), droppedFrames( 0u );

    // picture reception loop:
    for ( unsigned i( 0u ), j( 0u ); j < nbLoops; j++ )
    {
        if ( j == 0u )
        {
            PCStartTime = chrono::high_resolution_clock::now();
            lastOKFramePCTime = chrono::high_resolution_clock::now();
        }
        getDataAndIncrementCounters( lib,
                                     sn,
                                     frame,
                                     timeStamp,
                                     PCStartTime,
                                     lastOKFramePCTime,
                                     frameId,
                                     ftkPeriodStat,
                                     responseTimeStat,
                                     PCExtraTimeStat,
                                     ftkSendingPeriodStat,
                                     lostCounter,
                                     sameFrame,
                                     receivedFrames,
                                     notProcessedCounter,
                                     ignoredFrames,
                                     droppedFrames,
                                     150,
                                     false );
        notProcessedCounter = 0u;
        sameFrame = 0u;
        receivedFrames = 0u;
        ignoredFrames = 0u;
        droppedFrames = 0u;
        lostCounter = 0u;
        for ( i = 0u; i < nbFrames; ++i )
        {
            getDataAndIncrementCounters( lib,
                                         sn,
                                         frame,
                                         timeStamp,
                                         PCStartTime,
                                         lastOKFramePCTime,
                                         frameId,
                                         ftkPeriodStat,
                                         responseTimeStat,
                                         PCExtraTimeStat,
                                         ftkSendingPeriodStat,
                                         lostCounter,
                                         sameFrame,
                                         receivedFrames,
                                         notProcessedCounter,
                                         ignoredFrames,
                                         droppedFrames,
                                         150 );
        } /* for nb frames */

        cout << setw( 3u ) << j + 1 << "/" << nbLoops << ": " << setw( 4u ) << receivedFrames
             << " received frames among which " << setw( 4u ) << ignoredFrames
             << " have been ignored (size cutoff), " << setw( 4u ) << droppedFrames << " dropped frames, "
             << setw( 4u ) << receivedFrames - ignoredFrames << " processed frames " << setw( 4u )
             << lostCounter << " missed frames";

        if ( sameFrame > 0u )
        {
            cout << ", " << sameFrame << " frames processed more than once";
        }

        cout << endl;
    }

    int32 period( 1000000 / frequency );

    cout << "Closing device connection..." << endl;
    if ( ftkError::FTK_OK != ftkClose( &lib ) )
    {
        checkError( lib );
    }

    cout << "\tTheoretical acquisition period: " << period << " us." << endl;

    double meanValue, stdValue, minValue, maxValue, medianValue;
    computeStats( ftkPeriodStat, meanValue, stdValue, minValue, maxValue, medianValue );

    cout << "\tDevice raw acquisition period: mean = " << meanValue << " us, STD = " << stdValue
         << " us,  min = " << minValue << " us,  max = " << maxValue << " us, median = " << medianValue
         << " us." << endl;

    computeStats( ftkSendingPeriodStat, meanValue, stdValue, minValue, maxValue, medianValue );
    cout << "\tDevice sending period: mean = " << meanValue << " us, STD = " << stdValue
         << " us,  min = " << minValue << " us,  max = " << maxValue << " us, median = " << medianValue
         << " us." << endl;

    computeStats( responseTimeStat, meanValue, stdValue, minValue, maxValue, medianValue );
    cout << endl << "UPDATE TIME TEST:" << endl;
    cout << "\tHost User PC time between updates: mean = " << meanValue << " us, STD = " << stdValue
         << " us,  min = " << minValue << " us,  max = " << maxValue << " us, median = " << medianValue
         << " us." << endl;
    cout << "Acceptance criteria: Update time maximum < " << maxUpdateTime
         << " us. Test result: " << ( ( maxValue < maxUpdateTime ) ? "PASSED" : "FAILED" ) << endl;

    computeStats( PCExtraTimeStat, meanValue, stdValue, minValue, maxValue, medianValue );
    cout << endl << "MAX downstream latency TEST:" << endl;
    cout << "\tHost Latency time added downstream from the device for "
         << "measurement frames : mean = " << meanValue << " us,  max = " << maxValue
         << " us, median = " << medianValue << " us." << endl;
    cout << "Acceptance criteria: Update time maximum < " << maxLatencyTime
         << " us. Test result: " << ( ( maxValue < maxLatencyTime ) ? "PASSED" : "FAILED" ) << endl;

    cout << endl;

#ifdef ATR_WIN
    if ( isLaunchedFromExplorer() )
    {
        cout << "Press the \"ANY\" key to quit" << endl;
        waitForKeyboardHit();
    }
#endif

    return ( maxValue < maxUpdateTime && maxValue < maxLatencyTime ) ? 0 : 1;
}

// Picture reception function called in loop:
void getDataAndIncrementCounters( ftkLibrary lib,
                                  uint64 sn,
                                  ftkFrameQuery* frame,
                                  uint64& lastOKFrameTimeStamp,
                                  chrono::time_point< chrono::high_resolution_clock >& PCTime,
                                  chrono::time_point< chrono::high_resolution_clock >& lastOKFramePCTime,
                                  uint32& frameId,
                                  vector< double >& ftkPeriodStat,
                                  vector< double >& responseTimeStat,
                                  vector< double >& PCExtraTimeStat,
                                  vector< double >& ftkSendingPeriodStat,
                                  uint32& lostCounter,
                                  uint32& sameFrame,
                                  uint32& receivedFrames,
                                  uint32& notProcessedCounter,
                                  uint32& ignoredFrames,
                                  uint32& droppedFrames,
                                  uint32 timeOut,
                                  bool process )
{
    ftkError status( ftkGetLastFrame( lib, sn, frame, timeOut ) );

    // Compute time between two consecutive ftkGetLastFrame responses and update new PC time:
    auto tnow = chrono::high_resolution_clock::now();
    chrono::duration< double, micro > diff = tnow - PCTime;
    PCTime = tnow;

    if ( process )
    {
        responseTimeStat.push_back( static_cast< double >( diff.count() ) );
    }

    uint64 newfTkTimeStamp;

    switch ( status )
    {
    case ftkError::FTK_OK:
    case ftkError::FTK_WAR_SHOCK_DETECTED:
    case ftkError::FTK_WAR_TEMP_INVALID:
    case ftkError::FTK_WAR_REJECTED_PIC:
        // These are received frames containing updated measurements

        newfTkTimeStamp = frame->imageHeader->timestampUS;
        if ( lastOKFrameTimeStamp > 0uLL )
        {
            uint32 delta( frame->imageHeader->counter - frameId );

            chrono::duration< double, micro > diff = tnow - lastOKFramePCTime;
            if ( process )
            {
                PCExtraTimeStat.push_back( diff.count() - double( newfTkTimeStamp - lastOKFrameTimeStamp ) );
                ftkSendingPeriodStat.push_back( double( newfTkTimeStamp - lastOKFrameTimeStamp ) );
                ftkPeriodStat.push_back( double( newfTkTimeStamp - lastOKFrameTimeStamp ) / double( delta ) );
            }
            if ( delta > 1u && process )
            {
                lostCounter += delta - 1u;
            }
            else if ( delta == 0u )
            {
                ++sameFrame;
            }
        }
        //

        // Update all frame-OK cursors:
        if ( process )
        {
            if ( status == ftkError::FTK_WAR_REJECTED_PIC )
            {
                ++notProcessedCounter;
                ++ignoredFrames;
                ++receivedFrames;
            }
            else
            {
                ++receivedFrames;
            }
        }
        frameId = frame->imageHeader->counter;
        lastOKFrameTimeStamp = newfTkTimeStamp;
        lastOKFramePCTime = tnow;

        break;

    case ftkError::FTK_WAR_NO_FRAME:
        ++notProcessedCounter;

        break;

    case ftkError::FTK_WAR_FRAME:
        ++droppedFrames;
        ++receivedFrames;
        break;

    default:
        break;
    }
}

template< typename T >
void computeStats( const vector< T >& data,
                   double& mean,
                   double& stdDev,
                   double& minValue,
                   double& maxValue,
                   double& medianValue )
{
    vector< T > sortedData( data.cbegin(), data.cend() );

    sort( sortedData.begin(), sortedData.end() );

    mean = accumulate( sortedData.cbegin(), sortedData.cend(), 0.0 ) / double( data.size() );
    minValue = sortedData.front();
    maxValue = sortedData.back();

    stdDev = sqrt( accumulate( sortedData.cbegin(),
                               sortedData.cend(),
                               0.0,
                               [&mean]( T init, T val ) { return init + ( val - mean ) * ( val - mean ); } ) /
                   double( data.size() ) );

    const size_t nElements( sortedData.size() );
    if ( nElements % 2u == 0u )
    {
        medianValue = ( sortedData.at( nElements / 2u - 1u ) + sortedData.at( nElements / 2u ) ) / 2.0;
    }
    else
    {
        medianValue = sortedData.at( nElements / 2u - 1u );
    }
}
