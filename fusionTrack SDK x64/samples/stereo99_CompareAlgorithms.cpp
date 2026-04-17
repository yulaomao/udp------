// ============================================================================
//
//   stereo99_CompareAlgorithms.cpp
//   基于 stereo99_DumpAllData.cpp 的逆向算法对比脚本
//
//   功能:
//   1. 连接 fusionTrack 设备，逐帧获取数据
//   2. 对每帧数据:
//      a) 调用 SDK 原生 API (ftkTriangulate / ftkReprojectPoint) 获取结果
//      b) 调用逆向工程封装 (ReverseEngineeredPipeline) 计算同样结果
//      c) 对比两者的差异 (3D坐标、极线误差、三角化误差、重投影像素差)
//      d) 统计执行时间
//   3. 实时打印差异摘要
//   4. 将异常数据保存到 CSV (按 stereo99_DumpAllData 格式)
//   5. 最终输出分析报告
//
//   编译:
//     确保 include path 包含 fusionTrack SDK 的 include 目录以及
//     reverse_engineered_src 目录
//
//   用法:
//     stereo99_CompareAlgorithms -g geometry072.ini [-c config.json]
//                                [-n 200] [-o ./compare_output]
//                                [--epi-threshold 0.01] [--pos-threshold 0.1]
//
// ============================================================================

#include "geometryHelper.hpp"
#include "helpers.hpp"

// 逆向工程算法封装
#include "ReverseEngineeredPipeline.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>
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
// Helper: status bits to string
static string statusToString( ftkStatus s )
{
    ostringstream oss;
    oss << static_cast< uint32 >( s );
    return oss.str();
}

// ---------------------------------------------------------------------------
// Helper: high-resolution timer
using Clock = chrono::high_resolution_clock;
using Duration = chrono::duration<double, micro>;  // microseconds

// ---------------------------------------------------------------------------
// 差异统计结构
struct DiffStats
{
    // 三角化 3D 位置差异 (mm)
    vector<double> posDiffs;

    // 极线误差差异 (pixels)
    vector<double> epiDiffs;

    // 三角化误差差异 (mm)
    vector<double> triDiffs;

    // 重投影差异 (pixels)
    vector<double> reprojLeftDiffs;
    vector<double> reprojRightDiffs;

    // 执行时间 (microseconds)
    vector<double> sdkTriangulateTime;
    vector<double> revTriangulateTime;
    vector<double> sdkReprojectTime;
    vector<double> revReprojectTime;

    // 异常计数
    uint32_t anomalyCount = 0;
    uint32_t totalSamples = 0;

    // 辅助: 计算统计量
    static void summarize( const vector<double>& v, double& mean, double& maxVal,
                           double& stddev )
    {
        if ( v.empty() )
        {
            mean = maxVal = stddev = 0.0;
            return;
        }
        double sum = accumulate( v.begin(), v.end(), 0.0 );
        mean = sum / static_cast<double>( v.size() );
        maxVal = *max_element( v.begin(), v.end() );
        double sq_sum = 0.0;
        for ( auto x : v )
            sq_sum += ( x - mean ) * ( x - mean );
        stddev = sqrt( sq_sum / static_cast<double>( v.size() ) );
    }
};

// ---------------------------------------------------------------------------
// 3D 点距离
static double point3DDistance( const ftk3DPoint& a, const ftk3DPoint& b )
{
    double dx = static_cast<double>( a.x ) - static_cast<double>( b.x );
    double dy = static_cast<double>( a.y ) - static_cast<double>( b.y );
    double dz = static_cast<double>( a.z ) - static_cast<double>( b.z );
    return sqrt( dx * dx + dy * dy + dz * dz );
}

// ---------------------------------------------------------------------------
// 2D 点距离 (像素)
static double point2DDistance( const ftk3DPoint& a, const ftk3DPoint& b )
{
    double dx = static_cast<double>( a.x ) - static_cast<double>( b.x );
    double dy = static_cast<double>( a.y ) - static_cast<double>( b.y );
    return sqrt( dx * dx + dy * dy );
}

// ===========================================================================
// main
// ===========================================================================

int main( int argc, char** argv )
{
    const bool isNotFromConsole = isLaunchedFromExplorer();

#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << "=== stereo99_CompareAlgorithms ===" << endl;
    cout << "SDK vs Reverse-Engineered Algorithm Comparison Tool" << endl;
    cout << "Reverse-Engineered Pipeline: "
         << reverse_engineered::ReverseEngineeredPipeline::version() << endl;

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
    uint32 numFrames = 200u;
    string outputDir = "D:/compare_output";
    double posThreshold = 0.1;     // mm — 3D位置差异阈值
    double epiThreshold = 0.01;    // pixels — 极线误差差异阈值
    double reprojThreshold = 0.05; // pixels — 重投影差异阈值

    if ( showHelp || args.empty() )
    {
        cout << setw( 35u ) << "[-h/--help] " << flush
             << "Displays this help and exits." << endl;
        cout << setw( 35u ) << "[-c/--config F] " << flush
             << "JSON config file." << endl;
        cout << setw( 35u ) << "[-g/--geom F (F ...)] " << flush
             << "geometry file(s)." << endl;
        cout << setw( 35u ) << "[-n/--frames N] " << flush
             << "Number of frames, default = 200" << endl;
        cout << setw( 35u ) << "[-o/--output DIR] " << flush
             << "Output directory, default = ./compare_output" << endl;
        cout << setw( 35u ) << "[--pos-threshold T] " << flush
             << "3D position diff threshold (mm), default = 0.1" << endl;
        cout << setw( 35u ) << "[--epi-threshold T] " << flush
             << "Epipolar diff threshold (px), default = 0.01" << endl;
        cout << setw( 35u ) << "[--reproj-threshold T] " << flush
             << "Reprojection diff threshold (px), default = 0.05" << endl;
    }

    cout << "Reverse-engineered comparison tool (2026)" << endl;
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

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "--pos-threshold"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        posThreshold = atof( pos->c_str() );
    }

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "--epi-threshold"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        epiThreshold = atof( pos->c_str() );
    }

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "--reproj-threshold"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        reprojThreshold = atof( pos->c_str() );
    }

    // Create output directory
    ensureDir( outputDir );

    // -----------------------------------------------------------------------
    // Initialize SDK driver

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
            ftkSetInt32( lib, sn, options[ "Enable images sending" ], 1 );
        }
        if ( options.find( "Enable embedded processing" ) != options.cend() )
        {
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
    // Initialize frame

    ftkFrameQuery* frame = ftkCreateFrame();
    if ( frame == 0 )
    {
        cerr << "Cannot create frame instance" << endl;
        checkError( lib, !isNotFromConsole );
    }

    err = ftkSetFrameOptions( true, 16u, 256u, 256u, 256u, 32u, frame );
    if ( err != ftkError::FTK_OK )
    {
        ftkDeleteFrame( frame );
        cerr << "Cannot initialise frame" << endl;
        checkError( lib, !isNotFromConsole );
    }

    // -----------------------------------------------------------------------
    // Initialize reverse-engineered pipeline (will be initialized after
    // calibration is extracted from first frame)

    reverse_engineered::ReverseEngineeredPipeline revPipeline;
    bool revInitialized = false;

    // -----------------------------------------------------------------------
    // Open output CSV files

    // 1. 异常三角化数据 (只保存计算不太对的)
    string anomalyFid3dPath = outputDir + "/anomaly_fiducials_3d.csv";
    ofstream anomalyFid3dCsv( anomalyFid3dPath.c_str() );
    anomalyFid3dCsv << fixed << setprecision( 6 );
    anomalyFid3dCsv << "frame_idx,fid3d_idx,"
                    << "sdk_pos_x_mm,sdk_pos_y_mm,sdk_pos_z_mm,"
                    << "rev_pos_x_mm,rev_pos_y_mm,rev_pos_z_mm,"
                    << "pos_diff_mm,"
                    << "sdk_epiErr_px,rev_epiErr_px,epi_diff_px,"
                    << "sdk_triErr_mm,rev_triErr_mm,tri_diff_mm,"
                    << "leftIndex,rightIndex,"
                    << "left_cx,left_cy,right_cx,right_cy,"
                    << "probability,status_bits" << endl;

    // 2. 异常重投影数据
    string anomalyReprojPath = outputDir + "/anomaly_reprojections.csv";
    ofstream anomalyReprojCsv( anomalyReprojPath.c_str() );
    anomalyReprojCsv << fixed << setprecision( 6 );
    anomalyReprojCsv << "frame_idx,fid3d_idx,"
                     << "pos3d_x_mm,pos3d_y_mm,pos3d_z_mm,"
                     << "sdk_left_x,sdk_left_y,sdk_right_x,sdk_right_y,"
                     << "rev_left_x,rev_left_y,rev_right_x,rev_right_y,"
                     << "left_diff_px,right_diff_px" << endl;

    // 3. 每帧汇总
    string frameSummaryPath = outputDir + "/frame_summary.csv";
    ofstream frameSummaryCsv( frameSummaryPath.c_str() );
    frameSummaryCsv << fixed << setprecision( 6 );
    frameSummaryCsv << "frame_idx,fid_count,"
                    << "mean_pos_diff_mm,max_pos_diff_mm,"
                    << "mean_epi_diff_px,max_epi_diff_px,"
                    << "mean_reproj_left_diff_px,mean_reproj_right_diff_px,"
                    << "sdk_triangulate_us,rev_triangulate_us,"
                    << "sdk_reproject_us,rev_reproject_us,"
                    << "anomaly_count" << endl;

    // 4. 执行时间详细日志
    string timingPath = outputDir + "/timing_detail.csv";
    ofstream timingCsv( timingPath.c_str() );
    timingCsv << fixed << setprecision( 3 );
    timingCsv << "frame_idx,fid3d_idx,operation,"
              << "sdk_time_us,rev_time_us,speedup_ratio" << endl;

    // -----------------------------------------------------------------------
    // Global statistics
    DiffStats globalStats;

    cout << "\n=== Comparison Configuration ===" << endl;
    cout << "  Frames to capture: " << numFrames << endl;
    cout << "  Position threshold: " << posThreshold << " mm" << endl;
    cout << "  Epipolar threshold: " << epiThreshold << " px" << endl;
    cout << "  Reproj threshold:   " << reprojThreshold << " px" << endl;
    cout << "  Output directory:   " << outputDir << endl;

    // -----------------------------------------------------------------------
    // Acquisition & comparison loop

    cout << "\nCapturing " << numFrames << " frames..." << endl;
    cout << "Place a marker with the loaded geometry in front of the device.\n" << endl;

    uint32 capturedFrames = 0;
    uint32 maxAttempts = numFrames * 20u;

    for ( uint32 attempt = 0; attempt < maxAttempts && capturedFrames < numFrames; ++attempt )
    {
        err = ftkGetLastFrame( lib, sn, frame, 200u );
        if ( err > ftkError::FTK_OK )
        {
            continue;
        }
        if ( err == ftkError::FTK_WAR_NO_FRAME )
        {
            continue;
        }

        // -------------------------------------------------------------------
        // Extract calibration and initialize reverse-engineered pipeline (once)
        if ( !revInitialized )
        {
            ftkFrameInfoData infoData;
            infoData.WantedInformation = ftkInformationType::CalibrationParameters;
            ftkError calibErr = ftkExtractFrameInfo( frame, &infoData );
            if ( calibErr == ftkError::FTK_OK )
            {
                if ( revPipeline.initialize( infoData.Calibration ) )
                {
                    revInitialized = true;
                    cout << "Reverse-engineered pipeline initialized with calibration data."
                         << endl;
                }
                else
                {
                    cerr << "ERROR: Failed to initialize reverse-engineered pipeline." << endl;
                }
            }
        }

        if ( !revInitialized )
        {
            // 无法初始化逆向管线，跳过此帧
            continue;
        }

        bool hasAnyData = false;

        // Per-frame statistics
        vector<double> framePosDiffs, frameEpiDiffs, frameTriDiffs;
        vector<double> frameReprojLeftDiffs, frameReprojRightDiffs;
        double frameSdkTriTime = 0.0, frameRevTriTime = 0.0;
        double frameSdkReprojTime = 0.0, frameRevReprojTime = 0.0;
        uint32 frameAnomalies = 0;

        // -------------------------------------------------------------------
        // Compare triangulation: for each 3D fiducial from SDK,
        // re-triangulate using both SDK API and reverse-engineered code

        if ( frame->threeDFiducialsStat == ftkQueryStatus::QS_OK &&
             frame->rawDataLeftStat == ftkQueryStatus::QS_OK &&
             frame->rawDataRightStat == ftkQueryStatus::QS_OK )
        {
            for ( uint32 m = 0; m < frame->threeDFiducialsCount; ++m )
            {
                const ftk3DFiducial& sdkFid = frame->threeDFiducials[ m ];
                hasAnyData = true;

                // 获取对应的左右2D检测点
                if ( sdkFid.leftIndex >= frame->rawDataLeftCount ||
                     sdkFid.rightIndex >= frame->rawDataRightCount )
                    continue;

                const ftkRawData& leftRaw = frame->rawDataLeft[ sdkFid.leftIndex ];
                const ftkRawData& rightRaw = frame->rawDataRight[ sdkFid.rightIndex ];

                ftk3DPoint leftPx = { leftRaw.centerXPixels, leftRaw.centerYPixels, 0.0f };
                ftk3DPoint rightPx = { rightRaw.centerXPixels, rightRaw.centerYPixels, 0.0f };

                // ---- SDK 三角化 (使用 ftkTriangulate) ----
                ftk3DPoint sdkTriPoint{};
                auto t0 = Clock::now();
                ftkError sdkTriErr = ftkTriangulate( lib, sn, &leftPx, &rightPx, &sdkTriPoint );
                auto t1 = Clock::now();
                double sdkTriUs = Duration( t1 - t0 ).count();

                // ---- 逆向工程三角化 ----
                ftk3DPoint revTriPoint{};
                float revEpiErr = 0.0f, revTriErrVal = 0.0f;
                auto t2 = Clock::now();
                bool revTriOk = revPipeline.triangulate(
                    leftPx, rightPx, &revTriPoint, &revEpiErr, &revTriErrVal );
                auto t3 = Clock::now();
                double revTriUs = Duration( t3 - t2 ).count();

                frameSdkTriTime += sdkTriUs;
                frameRevTriTime += revTriUs;

                // 时间日志
                double speedup = ( revTriUs > 1e-6 ) ? ( sdkTriUs / revTriUs ) : 0.0;
                timingCsv << capturedFrames << "," << m << ",triangulate,"
                          << sdkTriUs << "," << revTriUs << "," << speedup << endl;

                // 计算差异 — 与 SDK 帧数据中的 3D 位置对比
                if ( sdkTriErr == ftkError::FTK_OK && revTriOk )
                {
                    // 逆向结果 vs SDK 帧中已有的结果
                    double posDiff = point3DDistance( sdkFid.positionMM, revTriPoint );
                    double epiDiff = fabs( static_cast<double>( sdkFid.epipolarErrorPixels )
                                         - static_cast<double>( revEpiErr ) );
                    double triDiff = fabs( static_cast<double>( sdkFid.triangulationErrorMM )
                                         - static_cast<double>( revTriErrVal ) );

                    framePosDiffs.push_back( posDiff );
                    frameEpiDiffs.push_back( epiDiff );
                    frameTriDiffs.push_back( triDiff );

                    globalStats.posDiffs.push_back( posDiff );
                    globalStats.epiDiffs.push_back( epiDiff );
                    globalStats.triDiffs.push_back( triDiff );
                    globalStats.sdkTriangulateTime.push_back( sdkTriUs );
                    globalStats.revTriangulateTime.push_back( revTriUs );
                    globalStats.totalSamples++;

                    // 检查是否为异常数据
                    bool isAnomaly = ( posDiff > posThreshold ) ||
                                     ( epiDiff > epiThreshold );

                    if ( isAnomaly )
                    {
                        ++frameAnomalies;
                        ++globalStats.anomalyCount;

                        // 保存异常三角化数据
                        anomalyFid3dCsv
                            << capturedFrames << "," << m << ","
                            << sdkFid.positionMM.x << ","
                            << sdkFid.positionMM.y << ","
                            << sdkFid.positionMM.z << ","
                            << revTriPoint.x << ","
                            << revTriPoint.y << ","
                            << revTriPoint.z << ","
                            << posDiff << ","
                            << sdkFid.epipolarErrorPixels << ","
                            << revEpiErr << ","
                            << epiDiff << ","
                            << sdkFid.triangulationErrorMM << ","
                            << revTriErrVal << ","
                            << triDiff << ","
                            << sdkFid.leftIndex << ","
                            << sdkFid.rightIndex << ","
                            << leftRaw.centerXPixels << ","
                            << leftRaw.centerYPixels << ","
                            << rightRaw.centerXPixels << ","
                            << rightRaw.centerYPixels << ","
                            << sdkFid.probability << ","
                            << statusToString( sdkFid.status ) << endl;
                    }
                }

                // ---- 重投影对比 ----
                // SDK 重投影
                ftk3DPoint sdkReprojL{}, sdkReprojR{};
                auto t4 = Clock::now();
                ftkError sdkRepErr = ftkReprojectPoint(
                    lib, sn, &sdkFid.positionMM, &sdkReprojL, &sdkReprojR );
                auto t5 = Clock::now();
                double sdkRepUs = Duration( t5 - t4 ).count();

                // 逆向工程重投影
                ftk3DPoint revReprojL{}, revReprojR{};
                auto t6 = Clock::now();
                bool revRepOk = revPipeline.reproject(
                    sdkFid.positionMM, &revReprojL, &revReprojR );
                auto t7 = Clock::now();
                double revRepUs = Duration( t7 - t6 ).count();

                frameSdkReprojTime += sdkRepUs;
                frameRevReprojTime += revRepUs;

                // 时间日志
                double repSpeedup = ( revRepUs > 1e-6 ) ? ( sdkRepUs / revRepUs ) : 0.0;
                timingCsv << capturedFrames << "," << m << ",reproject,"
                          << sdkRepUs << "," << revRepUs << "," << repSpeedup << endl;

                if ( sdkRepErr == ftkError::FTK_OK && revRepOk )
                {
                    double leftDiff = point2DDistance( sdkReprojL, revReprojL );
                    double rightDiff = point2DDistance( sdkReprojR, revReprojR );

                    frameReprojLeftDiffs.push_back( leftDiff );
                    frameReprojRightDiffs.push_back( rightDiff );

                    globalStats.reprojLeftDiffs.push_back( leftDiff );
                    globalStats.reprojRightDiffs.push_back( rightDiff );
                    globalStats.sdkReprojectTime.push_back( sdkRepUs );
                    globalStats.revReprojectTime.push_back( revRepUs );

                    // 异常重投影数据保存
                    if ( leftDiff > reprojThreshold || rightDiff > reprojThreshold )
                    {
                        anomalyReprojCsv
                            << capturedFrames << "," << m << ","
                            << sdkFid.positionMM.x << ","
                            << sdkFid.positionMM.y << ","
                            << sdkFid.positionMM.z << ","
                            << sdkReprojL.x << "," << sdkReprojL.y << ","
                            << sdkReprojR.x << "," << sdkReprojR.y << ","
                            << revReprojL.x << "," << revReprojL.y << ","
                            << revReprojR.x << "," << revReprojR.y << ","
                            << leftDiff << "," << rightDiff << endl;
                    }
                }
            }
        }

        // -------------------------------------------------------------------
        // Per-frame summary output

        if ( hasAnyData )
        {
            // 计算帧级统计
            double meanPos = 0.0, maxPos = 0.0, stdPos = 0.0;
            double meanEpi = 0.0, maxEpi = 0.0, stdEpi = 0.0;
            double meanRepL = 0.0, meanRepR = 0.0;

            DiffStats::summarize( framePosDiffs, meanPos, maxPos, stdPos );
            DiffStats::summarize( frameEpiDiffs, meanEpi, maxEpi, stdEpi );

            if ( !frameReprojLeftDiffs.empty() )
            {
                meanRepL = accumulate( frameReprojLeftDiffs.begin(),
                                       frameReprojLeftDiffs.end(), 0.0 )
                         / static_cast<double>( frameReprojLeftDiffs.size() );
                meanRepR = accumulate( frameReprojRightDiffs.begin(),
                                       frameReprojRightDiffs.end(), 0.0 )
                         / static_cast<double>( frameReprojRightDiffs.size() );
            }

            // 写入帧汇总
            frameSummaryCsv << capturedFrames << ","
                            << framePosDiffs.size() << ","
                            << meanPos << "," << maxPos << ","
                            << meanEpi << "," << maxEpi << ","
                            << meanRepL << "," << meanRepR << ","
                            << frameSdkTriTime << "," << frameRevTriTime << ","
                            << frameSdkReprojTime << "," << frameRevReprojTime << ","
                            << frameAnomalies << endl;

            // 打印简要信息
            cout << "Frame " << setw( 4 ) << capturedFrames << ": "
                 << setw( 3 ) << framePosDiffs.size() << " fid | "
                 << "pos: mean=" << fixed << setprecision( 4 ) << meanPos
                 << " max=" << maxPos << " mm | "
                 << "epi: mean=" << setprecision( 5 ) << meanEpi
                 << " max=" << maxEpi << " px | "
                 << "rep: L=" << setprecision( 4 ) << meanRepL
                 << " R=" << meanRepR << " px | "
                 << "time(tri): sdk=" << setprecision( 0 ) << frameSdkTriTime
                 << " rev=" << frameRevTriTime << " us";

            if ( frameAnomalies > 0 )
            {
                cout << " | *** " << frameAnomalies << " anomalies ***";
            }
            cout << endl;

            ++capturedFrames;
        }
        else
        {
            cout << ".";
        }

        sleep( 100L );
    }

    // -----------------------------------------------------------------------
    // Generate final analysis report

    string reportPath = outputDir + "/analysis_report.txt";
    ofstream report( reportPath.c_str() );

    auto writeReport = [&]( ostream& out )
    {
        out << "============================================================" << endl;
        out << " SDK vs Reverse-Engineered Algorithm Comparison Report" << endl;
        out << "============================================================" << endl;
        out << endl;

        // 获取当前时间
        time_t now = time( nullptr );
        char timeBuf[ 64 ];
        strftime( timeBuf, sizeof( timeBuf ), "%Y-%m-%d %H:%M:%S", localtime( &now ) );
        out << "Report generated: " << timeBuf << endl;
        out << "Pipeline version: "
            << reverse_engineered::ReverseEngineeredPipeline::version() << endl;
        out << "Total frames captured: " << capturedFrames << endl;
        out << "Total fiducial samples: " << globalStats.totalSamples << endl;
        out << "Anomalous samples: " << globalStats.anomalyCount
            << " (" << fixed << setprecision( 1 )
            << ( globalStats.totalSamples > 0
                     ? 100.0 * globalStats.anomalyCount / globalStats.totalSamples
                     : 0.0 )
            << "%)" << endl;
        out << endl;

        out << "Thresholds used:" << endl;
        out << "  Position:     " << posThreshold << " mm" << endl;
        out << "  Epipolar:     " << epiThreshold << " px" << endl;
        out << "  Reprojection: " << reprojThreshold << " px" << endl;
        out << endl;

        // 三角化差异统计
        out << "--- Triangulation (3D Position) Difference ---" << endl;
        {
            double mean, mx, sd;
            DiffStats::summarize( globalStats.posDiffs, mean, mx, sd );
            out << "  Mean:   " << fixed << setprecision( 6 ) << mean << " mm" << endl;
            out << "  Max:    " << mx << " mm" << endl;
            out << "  StdDev: " << sd << " mm" << endl;
            out << "  Count:  " << globalStats.posDiffs.size() << endl;
        }
        out << endl;

        // 极线误差差异
        out << "--- Epipolar Error Difference ---" << endl;
        {
            double mean, mx, sd;
            DiffStats::summarize( globalStats.epiDiffs, mean, mx, sd );
            out << "  Mean:   " << mean << " px" << endl;
            out << "  Max:    " << mx << " px" << endl;
            out << "  StdDev: " << sd << " px" << endl;
        }
        out << endl;

        // 三角化误差差异
        out << "--- Triangulation Error Difference ---" << endl;
        {
            double mean, mx, sd;
            DiffStats::summarize( globalStats.triDiffs, mean, mx, sd );
            out << "  Mean:   " << mean << " mm" << endl;
            out << "  Max:    " << mx << " mm" << endl;
            out << "  StdDev: " << sd << " mm" << endl;
        }
        out << endl;

        // 重投影差异
        out << "--- Reprojection Difference ---" << endl;
        {
            double meanL, mxL, sdL, meanR, mxR, sdR;
            DiffStats::summarize( globalStats.reprojLeftDiffs, meanL, mxL, sdL );
            DiffStats::summarize( globalStats.reprojRightDiffs, meanR, mxR, sdR );
            out << "  Left:  mean=" << meanL << " max=" << mxL
                << " std=" << sdL << " px" << endl;
            out << "  Right: mean=" << meanR << " max=" << mxR
                << " std=" << sdR << " px" << endl;
        }
        out << endl;

        // 时间统计
        out << "--- Execution Time Statistics ---" << endl;
        {
            double sdkTriMean, sdkTriMax, sdkTriStd;
            double revTriMean, revTriMax, revTriStd;
            DiffStats::summarize( globalStats.sdkTriangulateTime,
                                  sdkTriMean, sdkTriMax, sdkTriStd );
            DiffStats::summarize( globalStats.revTriangulateTime,
                                  revTriMean, revTriMax, revTriStd );
            out << "  Triangulation (per point):" << endl;
            out << "    SDK:     mean=" << setprecision( 1 ) << sdkTriMean
                << " max=" << sdkTriMax << " us" << endl;
            out << "    Reverse: mean=" << revTriMean
                << " max=" << revTriMax << " us" << endl;
            if ( revTriMean > 0.0 )
                out << "    Speedup: " << setprecision( 2 )
                    << ( sdkTriMean / revTriMean ) << "x" << endl;

            double sdkRepMean, sdkRepMax, sdkRepStd;
            double revRepMean, revRepMax, revRepStd;
            DiffStats::summarize( globalStats.sdkReprojectTime,
                                  sdkRepMean, sdkRepMax, sdkRepStd );
            DiffStats::summarize( globalStats.revReprojectTime,
                                  revRepMean, revRepMax, revRepStd );
            out << "  Reprojection (per point):" << endl;
            out << "    SDK:     mean=" << setprecision( 1 ) << sdkRepMean
                << " max=" << sdkRepMax << " us" << endl;
            out << "    Reverse: mean=" << revRepMean
                << " max=" << revRepMax << " us" << endl;
            if ( revRepMean > 0.0 )
                out << "    Speedup: " << setprecision( 2 )
                    << ( sdkRepMean / revRepMean ) << "x" << endl;

            // 总时间
            double totalSdkTri = accumulate( globalStats.sdkTriangulateTime.begin(),
                                             globalStats.sdkTriangulateTime.end(), 0.0 );
            double totalRevTri = accumulate( globalStats.revTriangulateTime.begin(),
                                             globalStats.revTriangulateTime.end(), 0.0 );
            double totalSdkRep = accumulate( globalStats.sdkReprojectTime.begin(),
                                             globalStats.sdkReprojectTime.end(), 0.0 );
            double totalRevRep = accumulate( globalStats.revReprojectTime.begin(),
                                             globalStats.revReprojectTime.end(), 0.0 );
            out << endl;
            out << "  Total time:" << endl;
            out << "    SDK triangulation:  " << setprecision( 0 )
                << totalSdkTri / 1000.0 << " ms" << endl;
            out << "    Rev triangulation:  " << totalRevTri / 1000.0 << " ms" << endl;
            out << "    SDK reprojection:   " << totalSdkRep / 1000.0 << " ms" << endl;
            out << "    Rev reprojection:   " << totalRevRep / 1000.0 << " ms" << endl;
        }
        out << endl;

        // 结论
        out << "--- Conclusion ---" << endl;
        {
            double posMean, posMax, posStd;
            DiffStats::summarize( globalStats.posDiffs, posMean, posMax, posStd );
            double epiMean, epiMax, epiStd;
            DiffStats::summarize( globalStats.epiDiffs, epiMean, epiMax, epiStd );

            if ( posMean < posThreshold && epiMean < epiThreshold )
            {
                out << "  PASS: Reverse-engineered algorithms match SDK output within thresholds."
                    << endl;
            }
            else
            {
                out << "  ATTENTION: Some differences exceed thresholds. Review anomaly data."
                    << endl;
            }

            if ( globalStats.anomalyCount > 0 )
            {
                out << "  " << globalStats.anomalyCount << " anomalous samples saved for analysis."
                    << endl;
                out << "  See: " << anomalyFid3dPath << endl;
                out << "  See: " << anomalyReprojPath << endl;
            }
        }
        out << endl;
        out << "============================================================" << endl;
    };

    // Write to file
    writeReport( report );
    report.close();

    // Also print to console
    cout << endl;
    writeReport( cout );

    // -----------------------------------------------------------------------
    // Close all CSV files
    anomalyFid3dCsv.close();
    anomalyReprojCsv.close();
    frameSummaryCsv.close();
    timingCsv.close();

    cout << endl;
    if ( capturedFrames > 0 )
    {
        cout << "Output saved to: " << outputDir << "/" << endl;
        cout << endl;
        cout << "Output files:" << endl;
        cout << "  analysis_report.txt        - Final analysis report" << endl;
        cout << "  anomaly_fiducials_3d.csv   - Anomalous triangulation data" << endl;
        cout << "  anomaly_reprojections.csv  - Anomalous reprojection data" << endl;
        cout << "  frame_summary.csv          - Per-frame comparison summary" << endl;
        cout << "  timing_detail.csv          - Per-point execution time log" << endl;
    }
    else
    {
        cout << "WARNING: No frames captured. Ensure a marker is visible to the device."
             << endl;
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
