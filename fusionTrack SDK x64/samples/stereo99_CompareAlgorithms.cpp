// ============================================================================
//
//   stereo99_CompareAlgorithms.cpp
//   基于 stereo99_DumpAllData.cpp 的逆向算法对比脚本
//
//   功能:
//   1. 连接 fusionTrack 设备，逐帧获取数据
//   2. 对每帧数据:
//      a) 调用 SDK 原生 API (ftkTriangulate / ftkReprojectPoint) 获取结果
//      b) 调用逆向工程封装 (StereoAlgoPipeline) 计算同样结果
//      c) 对比两者的差异 (3D坐标、极线误差、三角化误差、重投影像素差)
//      d) 统计执行时间
//   3. 实时打印差异摘要
//   4. 将异常数据保存到 CSV (按 stereo99_DumpAllData 格式)
//   5. 最终输出分析报告
//
//   编译:
//     确保 include path 包含 fusionTrack SDK 的 include 目录以及
//     reverse_algo_lib 目录
//
//   用法:
//     stereo99_CompareAlgorithms -g geometry072.ini [-c config.json]
//                                [-n 200] [-o ./compare_output]
//                                [--epi-threshold 0.01] [--pos-threshold 0.1]
//
// ============================================================================

#include "geometryHelper.hpp"
#include "helpers.hpp"

// 逆向工程算法封装 (新版: 基于 verify_reverse_engineered.py 验证通过的算法)
#include "StereoAlgoPipeline.h"

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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

    // --- 新增: 圆心回环误差 (2D→3D→2D vs 原始2D检测) ---
    vector<double> centroidRoundtripLeftSdk;   // SDK: |reproject(sdk_3D) - raw_2D_left|
    vector<double> centroidRoundtripRightSdk;
    vector<double> centroidRoundtripLeftRev;   // Rev: |reproject(rev_3D) - raw_2D_left|
    vector<double> centroidRoundtripRightRev;

    // --- 新增: 批量匹配计数差异 ---
    vector<int32_t> fidCountDiffs;  // SDK_count - rev_count per frame

    // --- 新增: 批量匹配后的逐点3D差异 ---
    vector<double> batchPosDiffs;

    // --- 新增: 工具识别差异 ---
    uint32_t markerCountMatch = 0;     // 两者 marker 数量一致的帧数
    uint32_t markerCountMismatch = 0;  // 不一致的帧数
    uint32_t markerGeomIdMatch = 0;    // geometry ID 一致的 marker 数
    uint32_t markerGeomIdMismatch = 0;

    // --- 新增: 工具变换差异 ---
    vector<double> markerTransDiffs;     // 平移差异 (mm)
    vector<double> markerRotDiffs;       // 旋转差异 (degrees)
    vector<double> markerRegErrDiffs;    // 配准误差差异 (mm)

    // --- 新增: 批量匹配+工具识别执行时间 ---
    vector<double> sdkMatchTriTime;   // SDK matchAndTriangulate 总时间(帧级)
    vector<double> revMatchTriTime;
    vector<double> sdkMarkerTime;     // SDK marker 识别总时间(帧级)
    vector<double> revMarkerTime;

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

// ---------------------------------------------------------------------------
// 旋转矩阵差异 (角度, degrees) — 使用 Frobenius 范数近似
static double rotationDiffDegrees( const floatXX a[3][3], const floatXX b[3][3] )
{
    // R_diff = A * B^T, then angle = acos((trace(R_diff) - 1) / 2)
    double R[3][3] = {};
    for ( int i = 0; i < 3; ++i )
        for ( int j = 0; j < 3; ++j )
            for ( int k = 0; k < 3; ++k )
                R[i][j] += static_cast<double>( a[i][k] ) * static_cast<double>( b[j][k] );

    double trace = R[0][0] + R[1][1] + R[2][2];
    double cosAngle = ( trace - 1.0 ) / 2.0;
    // clamp to [-1, 1] for numerical safety
    cosAngle = max( -1.0, min( 1.0, cosAngle ) );
    return acos( cosAngle ) * 180.0 / M_PI;
}

// ---------------------------------------------------------------------------
// 平移向量距离 (mm)
static double translationDiff( const floatXX a[3], const floatXX b[3] )
{
    double dx = static_cast<double>( a[0] ) - static_cast<double>( b[0] );
    double dy = static_cast<double>( a[1] ) - static_cast<double>( b[1] );
    double dz = static_cast<double>( a[2] ) - static_cast<double>( b[2] );
    return sqrt( dx * dx + dy * dy + dz * dz );
}

// ---------------------------------------------------------------------------
// 2D 距离: ftkRawData centroid vs ftk3DPoint projection
static double centroid2DDistance( const ftkRawData& raw, const ftk3DPoint& proj )
{
    double dx = static_cast<double>( raw.centerXPixels ) - static_cast<double>( proj.x );
    double dy = static_cast<double>( raw.centerYPixels ) - static_cast<double>( proj.y );
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
         << stereo_algo::StereoAlgoPipeline::version() << endl;

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
    double centroidThreshold = 0.1; // pixels — 圆心回环误差阈值
    double markerTransThreshold = 0.5; // mm — 工具平移差异阈值
    double markerRotThreshold = 0.1;   // degrees — 工具旋转差异阈值

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
        cout << setw( 35u ) << "[--centroid-threshold T] " << flush
             << "Centroid round-trip threshold (px), default = 0.1" << endl;
        cout << setw( 35u ) << "[--marker-trans-threshold T] " << flush
             << "Marker translation diff threshold (mm), default = 0.5" << endl;
        cout << setw( 35u ) << "[--marker-rot-threshold T] " << flush
             << "Marker rotation diff threshold (deg), default = 0.1" << endl;
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

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "--centroid-threshold"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        centroidThreshold = atof( pos->c_str() );
    }

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "--marker-trans-threshold"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        markerTransThreshold = atof( pos->c_str() );
    }

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "--marker-rot-threshold"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        markerRotThreshold = atof( pos->c_str() );
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

    stereo_algo::StereoAlgoPipeline revPipeline;
    bool revInitialized = false;

    // Register geometry with reverse-engineered pipeline's marker matcher
    for ( const auto& geomFile : geomFiles )
    {
        ftkRigidBody tmpGeom{};
        if ( loadRigidBody( lib, geomFile, tmpGeom ) <= 1 )
        {
            if ( revPipeline.registerGeometry( tmpGeom ) )
            {
                cout << "Geometry registered with reverse-engineered MatchMarkers (ID="
                     << tmpGeom.geometryId << ", points=" << tmpGeom.pointsCount << ")" << endl;
            }
        }
    }

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
    frameSummaryCsv << "frame_idx,"
                    << "sdk_left_raw_count,sdk_right_raw_count,"
                    << "sdk_fid3d_count,rev_fid3d_count,fid_count_diff,"
                    << "mean_pos_diff_mm,max_pos_diff_mm,"
                    << "mean_epi_diff_px,max_epi_diff_px,"
                    << "mean_reproj_left_diff_px,mean_reproj_right_diff_px,"
                    << "mean_centroid_rt_left_sdk_px,mean_centroid_rt_right_sdk_px,"
                    << "mean_centroid_rt_left_rev_px,mean_centroid_rt_right_rev_px,"
                    << "sdk_marker_count,rev_marker_count,"
                    << "mean_marker_trans_diff_mm,mean_marker_rot_diff_deg,"
                    << "sdk_matchTri_us,rev_matchTri_us,"
                    << "sdk_triangulate_us,rev_triangulate_us,"
                    << "sdk_reproject_us,rev_reproject_us,"
                    << "sdk_marker_us,rev_marker_us,"
                    << "anomaly_count" << endl;

    // 4. 执行时间详细日志
    string timingPath = outputDir + "/timing_detail.csv";
    ofstream timingCsv( timingPath.c_str() );
    timingCsv << fixed << setprecision( 3 );
    timingCsv << "frame_idx,fid3d_idx,operation,"
              << "sdk_time_us,rev_time_us,speedup_ratio" << endl;

    // 5. 异常圆心回环误差数据
    string anomalyCentroidPath = outputDir + "/anomaly_centroid_roundtrip.csv";
    ofstream anomalyCentroidCsv( anomalyCentroidPath.c_str() );
    anomalyCentroidCsv << fixed << setprecision( 6 );
    anomalyCentroidCsv << "frame_idx,fid3d_idx,"
                       << "raw_left_cx,raw_left_cy,raw_right_cx,raw_right_cy,"
                       << "sdk_reproj_left_cx,sdk_reproj_left_cy,"
                       << "sdk_reproj_right_cx,sdk_reproj_right_cy,"
                       << "rev_reproj_left_cx,rev_reproj_left_cy,"
                       << "rev_reproj_right_cx,rev_reproj_right_cy,"
                       << "sdk_left_err_px,sdk_right_err_px,"
                       << "rev_left_err_px,rev_right_err_px" << endl;

    // 6. 批量匹配差异 (逐帧 fiducial 数量和匹配索引差异)
    string batchMatchPath = outputDir + "/batch_match_diff.csv";
    ofstream batchMatchCsv( batchMatchPath.c_str() );
    batchMatchCsv << fixed << setprecision( 6 );
    batchMatchCsv << "frame_idx,"
                  << "sdk_left_raw_count,sdk_right_raw_count,"
                  << "sdk_fid3d_count,rev_fid3d_count,count_diff,"
                  << "matched_pairs,mean_pair_pos_diff_mm,max_pair_pos_diff_mm,"
                  << "rev_matchTri_us" << endl;

    // 7. 异常工具识别/变换数据
    string anomalyMarkerPath = outputDir + "/anomaly_markers.csv";
    ofstream anomalyMarkerCsv( anomalyMarkerPath.c_str() );
    anomalyMarkerCsv << fixed << setprecision( 6 );
    anomalyMarkerCsv << "frame_idx,marker_idx,"
                     << "sdk_geomId,rev_geomId,geomId_match,"
                     << "sdk_tx,sdk_ty,sdk_tz,rev_tx,rev_ty,rev_tz,trans_diff_mm,"
                     << "sdk_r00,sdk_r01,sdk_r02,sdk_r10,sdk_r11,sdk_r12,"
                     << "sdk_r20,sdk_r21,sdk_r22,"
                     << "rev_r00,rev_r01,rev_r02,rev_r10,rev_r11,rev_r12,"
                     << "rev_r20,rev_r21,rev_r22,"
                     << "rot_diff_deg,"
                     << "sdk_regErr_mm,rev_regErr_mm,regErr_diff_mm,"
                     << "sdk_presenceMask,rev_presenceMask" << endl;

    // -----------------------------------------------------------------------
    // Global statistics
    DiffStats globalStats;

    cout << "\n=== Comparison Configuration ===" << endl;
    cout << "  Frames to capture:      " << numFrames << endl;
    cout << "  Position threshold:      " << posThreshold << " mm" << endl;
    cout << "  Epipolar threshold:      " << epiThreshold << " px" << endl;
    cout << "  Reproj threshold:        " << reprojThreshold << " px" << endl;
    cout << "  Centroid RT threshold:   " << centroidThreshold << " px" << endl;
    cout << "  Marker trans threshold:  " << markerTransThreshold << " mm" << endl;
    cout << "  Marker rot threshold:    " << markerRotThreshold << " deg" << endl;
    cout << "  Output directory:        " << outputDir << endl;

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
        vector<double> frameCentroidRtLeftSdk, frameCentroidRtRightSdk;
        vector<double> frameCentroidRtLeftRev, frameCentroidRtRightRev;
        vector<double> frameMarkerTransDiffs, frameMarkerRotDiffs, frameMarkerRegErrDiffs;
        double frameSdkTriTime = 0.0, frameRevTriTime = 0.0;
        double frameSdkReprojTime = 0.0, frameRevReprojTime = 0.0;
        double frameSdkMatchTriTime = 0.0, frameRevMatchTriTime = 0.0;
        double frameSdkMarkerTime = 0.0, frameRevMarkerTime = 0.0;
        uint32 frameAnomalies = 0;

        // Raw data counts
        uint32 sdkLeftRawCount = 0, sdkRightRawCount = 0;
        if ( frame->rawDataLeftStat == ftkQueryStatus::QS_OK )
            sdkLeftRawCount = frame->rawDataLeftCount;
        if ( frame->rawDataRightStat == ftkQueryStatus::QS_OK )
            sdkRightRawCount = frame->rawDataRightCount;

        // SDK fiducial count
        uint32 sdkFid3dCount = 0;
        if ( frame->threeDFiducialsStat == ftkQueryStatus::QS_OK )
            sdkFid3dCount = frame->threeDFiducialsCount;

        // SDK marker count
        uint32 sdkMarkerCount = 0;
        if ( frame->markersStat == ftkQueryStatus::QS_OK )
            sdkMarkerCount = frame->markersCount;

        // =====================================================================
        // STEP 1: 批量匹配+三角化对比
        //   SDK 已在帧中给出 threeDFiducials, 逆向工程从 raw 2D 独立匹配
        // =====================================================================

        uint32 revFid3dCount = 0;
        vector<ftk3DFiducial> revFiducials( 256 );
        uint32 matchedPairs = 0;
        vector<double> frameBatchPosDiffs;

        if ( frame->rawDataLeftStat == ftkQueryStatus::QS_OK &&
             frame->rawDataRightStat == ftkQueryStatus::QS_OK )
        {
            hasAnyData = true;

            auto tBatch0 = Clock::now();
            revFid3dCount = revPipeline.matchAndTriangulate(
                frame->rawDataLeft, frame->rawDataLeftCount,
                frame->rawDataRight, frame->rawDataRightCount,
                revFiducials.data(), static_cast<uint32_t>( revFiducials.size() ) );
            auto tBatch1 = Clock::now();
            frameRevMatchTriTime = Duration( tBatch1 - tBatch0 ).count();

            // SDK 的 matchAndTriangulate 时间近似: 帧获取已包含该步骤，
            // 无法单独计时，记为0(帧级别的对比用逆向的时间)
            frameSdkMatchTriTime = 0.0;

            int32_t fidCountDiff = static_cast<int32_t>( sdkFid3dCount )
                                 - static_cast<int32_t>( revFid3dCount );
            globalStats.fidCountDiffs.push_back( fidCountDiff );

            // 按 leftIndex+rightIndex 对比匹配的 fiducials
            // 构建 rev fiducials 的 (leftIndex, rightIndex) -> index 映射
            map<pair<uint32,uint32>, uint32> revFidMap;
            for ( uint32 ri = 0; ri < revFid3dCount; ++ri )
            {
                revFidMap[{ revFiducials[ri].leftIndex, revFiducials[ri].rightIndex }] = ri;
            }

            for ( uint32 si = 0; si < sdkFid3dCount; ++si )
            {
                const ftk3DFiducial& sf = frame->threeDFiducials[ si ];
                auto it = revFidMap.find({ sf.leftIndex, sf.rightIndex });
                if ( it != revFidMap.end() )
                {
                    const ftk3DFiducial& rf = revFiducials[ it->second ];
                    double pd = point3DDistance( sf.positionMM, rf.positionMM );
                    frameBatchPosDiffs.push_back( pd );
                    globalStats.batchPosDiffs.push_back( pd );
                    ++matchedPairs;
                }
            }

            // 写入 batch_match_diff CSV
            double batchMean = 0.0, batchMax = 0.0, batchStd = 0.0;
            DiffStats::summarize( frameBatchPosDiffs, batchMean, batchMax, batchStd );
            batchMatchCsv << capturedFrames << ","
                          << sdkLeftRawCount << "," << sdkRightRawCount << ","
                          << sdkFid3dCount << "," << revFid3dCount << ","
                          << fidCountDiff << ","
                          << matchedPairs << "," << batchMean << "," << batchMax << ","
                          << frameRevMatchTriTime << endl;

            globalStats.revMatchTriTime.push_back( frameRevMatchTriTime );
        }

        // =====================================================================
        // STEP 2: 逐点三角化对比 + 圆心回环误差 + 重投影对比
        // =====================================================================

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

                // ---- 重投影对比 + 圆心回环误差 ----

                // SDK 重投影 (使用 SDK 的 3D 结果)
                ftk3DPoint sdkReprojL{}, sdkReprojR{};
                auto t4 = Clock::now();
                ftkError sdkRepErr = ftkReprojectPoint(
                    lib, sn, &sdkFid.positionMM, &sdkReprojL, &sdkReprojR );
                auto t5 = Clock::now();
                double sdkRepUs = Duration( t5 - t4 ).count();

                // 逆向重投影 (使用 SDK 的 3D 结果，对比 SDK vs Rev 重投影算法)
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

                    // ---- 圆心回环误差 (2D→3D→2D vs 原始 2D) ----
                    // SDK: reproject(sdk_3D) vs raw 2D
                    double sdkRtLeftErr = centroid2DDistance( leftRaw, sdkReprojL );
                    double sdkRtRightErr = centroid2DDistance( rightRaw, sdkReprojR );
                    frameCentroidRtLeftSdk.push_back( sdkRtLeftErr );
                    frameCentroidRtRightSdk.push_back( sdkRtRightErr );
                    globalStats.centroidRoundtripLeftSdk.push_back( sdkRtLeftErr );
                    globalStats.centroidRoundtripRightSdk.push_back( sdkRtRightErr );

                    // Rev: reproject(rev_3D) vs raw 2D — 使用逆向三角化的3D再逆向重投影
                    ftk3DPoint revReprojFromRevL{}, revReprojFromRevR{};
                    bool revRepFromRevOk = false;
                    if ( revTriOk )
                    {
                        revRepFromRevOk = revPipeline.reproject(
                            revTriPoint, &revReprojFromRevL, &revReprojFromRevR );
                    }

                    double revRtLeftErr = 0.0, revRtRightErr = 0.0;
                    if ( revRepFromRevOk )
                    {
                        revRtLeftErr = centroid2DDistance( leftRaw, revReprojFromRevL );
                        revRtRightErr = centroid2DDistance( rightRaw, revReprojFromRevR );
                    }
                    else
                    {
                        // 如果逆向三角化失败，使用 SDK 3D 逆向重投影结果
                        revRtLeftErr = centroid2DDistance( leftRaw, revReprojL );
                        revRtRightErr = centroid2DDistance( rightRaw, revReprojR );
                    }

                    frameCentroidRtLeftRev.push_back( revRtLeftErr );
                    frameCentroidRtRightRev.push_back( revRtRightErr );
                    globalStats.centroidRoundtripLeftRev.push_back( revRtLeftErr );
                    globalStats.centroidRoundtripRightRev.push_back( revRtRightErr );

                    // 异常圆心回环保存
                    if ( sdkRtLeftErr > centroidThreshold || sdkRtRightErr > centroidThreshold ||
                         revRtLeftErr > centroidThreshold || revRtRightErr > centroidThreshold )
                    {
                        anomalyCentroidCsv
                            << capturedFrames << "," << m << ","
                            << leftRaw.centerXPixels << "," << leftRaw.centerYPixels << ","
                            << rightRaw.centerXPixels << "," << rightRaw.centerYPixels << ","
                            << sdkReprojL.x << "," << sdkReprojL.y << ","
                            << sdkReprojR.x << "," << sdkReprojR.y << ","
                            << ( revRepFromRevOk ? revReprojFromRevL.x : revReprojL.x ) << ","
                            << ( revRepFromRevOk ? revReprojFromRevL.y : revReprojL.y ) << ","
                            << ( revRepFromRevOk ? revReprojFromRevR.x : revReprojR.x ) << ","
                            << ( revRepFromRevOk ? revReprojFromRevR.y : revReprojR.y ) << ","
                            << sdkRtLeftErr << "," << sdkRtRightErr << ","
                            << revRtLeftErr << "," << revRtRightErr << endl;
                    }
                }
            }
        }

        // =====================================================================
        // STEP 3: 工具识别+变换对比
        //   SDK 已在帧中给出 markers, 逆向工程从 revFiducials 独立匹配
        // =====================================================================

        uint32 revMarkerCount = 0;
        vector<ftkMarker> revMarkers( 32 );

        if ( revFid3dCount > 0 )
        {
            auto tMk0 = Clock::now();
            revMarkerCount = revPipeline.matchMarkers(
                revFiducials.data(), revFid3dCount,
                revMarkers.data(), static_cast<uint32_t>( revMarkers.size() ) );
            auto tMk1 = Clock::now();
            frameRevMarkerTime = Duration( tMk1 - tMk0 ).count();
            globalStats.revMarkerTime.push_back( frameRevMarkerTime );
        }

        // SDK marker 时间近似为0 (已在帧获取中完成)
        frameSdkMarkerTime = 0.0;

        // 对比 marker 数量
        if ( hasAnyData )
        {
            if ( sdkMarkerCount == revMarkerCount )
                ++globalStats.markerCountMatch;
            else
                ++globalStats.markerCountMismatch;
        }

        // 对比各 marker 的 geometry ID + 变换
        // 按 geometryId 匹配 SDK marker 和 Rev marker
        for ( uint32 si = 0; si < sdkMarkerCount; ++si )
        {
            const ftkMarker& sm = frame->markers[ si ];
            bool foundMatch = false;

            for ( uint32 ri = 0; ri < revMarkerCount; ++ri )
            {
                const ftkMarker& rm = revMarkers[ ri ];
                if ( sm.geometryId == rm.geometryId )
                {
                    foundMatch = true;
                    ++globalStats.markerGeomIdMatch;

                    double tDiff = translationDiff( sm.translationMM, rm.translationMM );
                    double rDiff = rotationDiffDegrees( sm.rotation, rm.rotation );
                    double regDiff = fabs( static_cast<double>( sm.registrationErrorMM )
                                         - static_cast<double>( rm.registrationErrorMM ) );

                    frameMarkerTransDiffs.push_back( tDiff );
                    frameMarkerRotDiffs.push_back( rDiff );
                    frameMarkerRegErrDiffs.push_back( regDiff );

                    globalStats.markerTransDiffs.push_back( tDiff );
                    globalStats.markerRotDiffs.push_back( rDiff );
                    globalStats.markerRegErrDiffs.push_back( regDiff );

                    // 异常 marker 数据保存
                    if ( tDiff > markerTransThreshold || rDiff > markerRotThreshold )
                    {
                        anomalyMarkerCsv
                            << capturedFrames << "," << si << ","
                            << sm.geometryId << "," << rm.geometryId << ",1,"
                            << sm.translationMM[0] << "," << sm.translationMM[1] << ","
                            << sm.translationMM[2] << ","
                            << rm.translationMM[0] << "," << rm.translationMM[1] << ","
                            << rm.translationMM[2] << ","
                            << tDiff << ","
                            << sm.rotation[0][0] << "," << sm.rotation[0][1] << ","
                            << sm.rotation[0][2] << ","
                            << sm.rotation[1][0] << "," << sm.rotation[1][1] << ","
                            << sm.rotation[1][2] << ","
                            << sm.rotation[2][0] << "," << sm.rotation[2][1] << ","
                            << sm.rotation[2][2] << ","
                            << rm.rotation[0][0] << "," << rm.rotation[0][1] << ","
                            << rm.rotation[0][2] << ","
                            << rm.rotation[1][0] << "," << rm.rotation[1][1] << ","
                            << rm.rotation[1][2] << ","
                            << rm.rotation[2][0] << "," << rm.rotation[2][1] << ","
                            << rm.rotation[2][2] << ","
                            << rDiff << ","
                            << sm.registrationErrorMM << "," << rm.registrationErrorMM << ","
                            << regDiff << ","
                            << sm.geometryPresenceMask << "," << rm.geometryPresenceMask << endl;
                    }

                    break;
                }
            }

            if ( !foundMatch )
            {
                ++globalStats.markerGeomIdMismatch;

                // SDK marker 未在逆向结果中找到匹配
                anomalyMarkerCsv
                    << capturedFrames << "," << si << ","
                    << sm.geometryId << ",-1,0,"
                    << sm.translationMM[0] << "," << sm.translationMM[1] << ","
                    << sm.translationMM[2] << ","
                    << ",,," << "0,"
                    << sm.rotation[0][0] << "," << sm.rotation[0][1] << ","
                    << sm.rotation[0][2] << ","
                    << sm.rotation[1][0] << "," << sm.rotation[1][1] << ","
                    << sm.rotation[1][2] << ","
                    << sm.rotation[2][0] << "," << sm.rotation[2][1] << ","
                    << sm.rotation[2][2] << ","
                    << ",,,,,,,,,"
                    << "0,"
                    << sm.registrationErrorMM << ",," << "0,"
                    << sm.geometryPresenceMask << "," << endl;
            }
        }

        // =====================================================================
        // Per-frame summary output
        // =====================================================================

        if ( hasAnyData )
        {
            // 计算帧级统计
            double meanPos = 0.0, maxPos = 0.0, stdPos = 0.0;
            double meanEpi = 0.0, maxEpi = 0.0, stdEpi = 0.0;
            double meanRepL = 0.0, meanRepR = 0.0;
            double meanCrtLSdk = 0.0, meanCrtRSdk = 0.0;
            double meanCrtLRev = 0.0, meanCrtRRev = 0.0;
            double meanMkTrans = 0.0, meanMkRot = 0.0;

            DiffStats::summarize( framePosDiffs, meanPos, maxPos, stdPos );
            DiffStats::summarize( frameEpiDiffs, meanEpi, maxEpi, stdEpi );

            auto safeMean = []( const vector<double>& v ) -> double {
                return v.empty() ? 0.0
                     : accumulate( v.begin(), v.end(), 0.0 ) / static_cast<double>( v.size() );
            };

            meanRepL = safeMean( frameReprojLeftDiffs );
            meanRepR = safeMean( frameReprojRightDiffs );
            meanCrtLSdk = safeMean( frameCentroidRtLeftSdk );
            meanCrtRSdk = safeMean( frameCentroidRtRightSdk );
            meanCrtLRev = safeMean( frameCentroidRtLeftRev );
            meanCrtRRev = safeMean( frameCentroidRtRightRev );
            meanMkTrans = safeMean( frameMarkerTransDiffs );
            meanMkRot = safeMean( frameMarkerRotDiffs );

            int32_t fidCountDiff = static_cast<int32_t>( sdkFid3dCount )
                                 - static_cast<int32_t>( revFid3dCount );

            // 写入帧汇总
            frameSummaryCsv << capturedFrames << ","
                            << sdkLeftRawCount << "," << sdkRightRawCount << ","
                            << sdkFid3dCount << "," << revFid3dCount << ","
                            << fidCountDiff << ","
                            << meanPos << "," << maxPos << ","
                            << meanEpi << "," << maxEpi << ","
                            << meanRepL << "," << meanRepR << ","
                            << meanCrtLSdk << "," << meanCrtRSdk << ","
                            << meanCrtLRev << "," << meanCrtRRev << ","
                            << sdkMarkerCount << "," << revMarkerCount << ","
                            << meanMkTrans << "," << meanMkRot << ","
                            << frameSdkMatchTriTime << "," << frameRevMatchTriTime << ","
                            << frameSdkTriTime << "," << frameRevTriTime << ","
                            << frameSdkReprojTime << "," << frameRevReprojTime << ","
                            << frameSdkMarkerTime << "," << frameRevMarkerTime << ","
                            << frameAnomalies << endl;

            // 打印简要信息
            cout << "Frame " << setw( 4 ) << capturedFrames << ": "
                 << "raw L/R=" << sdkLeftRawCount << "/" << sdkRightRawCount
                 << " | fid3d: sdk=" << sdkFid3dCount << " rev=" << revFid3dCount
                 << " | " << setw( 3 ) << framePosDiffs.size() << " tri | "
                 << "pos: " << fixed << setprecision( 4 ) << meanPos << "/" << maxPos << "mm | "
                 << "epi: " << setprecision( 5 ) << meanEpi << "px | "
                 << "ctr: sdk=" << setprecision( 3 ) << meanCrtLSdk
                 << "/" << meanCrtRSdk
                 << " rev=" << meanCrtLRev << "/" << meanCrtRRev << "px | "
                 << "mk: " << sdkMarkerCount << "/" << revMarkerCount;

            if ( !frameMarkerTransDiffs.empty() )
            {
                cout << " t=" << setprecision( 3 ) << meanMkTrans << "mm"
                     << " r=" << setprecision( 3 ) << meanMkRot << "deg";
            }

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
            << stereo_algo::StereoAlgoPipeline::version() << endl;
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
        out << "  Centroid RT:  " << centroidThreshold << " px" << endl;
        out << "  Marker trans: " << markerTransThreshold << " mm" << endl;
        out << "  Marker rot:   " << markerRotThreshold << " deg" << endl;
        out << endl;

        // (1) 三角化差异统计
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

        // (2) 极线误差差异
        out << "--- Epipolar Error Difference ---" << endl;
        {
            double mean, mx, sd;
            DiffStats::summarize( globalStats.epiDiffs, mean, mx, sd );
            out << "  Mean:   " << mean << " px" << endl;
            out << "  Max:    " << mx << " px" << endl;
            out << "  StdDev: " << sd << " px" << endl;
        }
        out << endl;

        // (3) 三角化误差差异
        out << "--- Triangulation Error Difference ---" << endl;
        {
            double mean, mx, sd;
            DiffStats::summarize( globalStats.triDiffs, mean, mx, sd );
            out << "  Mean:   " << mean << " mm" << endl;
            out << "  Max:    " << mx << " mm" << endl;
            out << "  StdDev: " << sd << " mm" << endl;
        }
        out << endl;

        // (4) 重投影差异
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

        // (5) 圆心回环误差 (2D→3D→2D)
        out << "--- Centroid Round-trip Error (2D->3D->2D vs raw 2D) ---" << endl;
        {
            double meanLS, mxLS, sdLS, meanRS, mxRS, sdRS;
            double meanLR, mxLR, sdLR, meanRR, mxRR, sdRR;
            DiffStats::summarize( globalStats.centroidRoundtripLeftSdk, meanLS, mxLS, sdLS );
            DiffStats::summarize( globalStats.centroidRoundtripRightSdk, meanRS, mxRS, sdRS );
            DiffStats::summarize( globalStats.centroidRoundtripLeftRev, meanLR, mxLR, sdLR );
            DiffStats::summarize( globalStats.centroidRoundtripRightRev, meanRR, mxRR, sdRR );
            out << "  SDK (left):  mean=" << setprecision( 6 ) << meanLS
                << " max=" << mxLS << " std=" << sdLS << " px" << endl;
            out << "  SDK (right): mean=" << meanRS
                << " max=" << mxRS << " std=" << sdRS << " px" << endl;
            out << "  Rev (left):  mean=" << meanLR
                << " max=" << mxLR << " std=" << sdLR << " px" << endl;
            out << "  Rev (right): mean=" << meanRR
                << " max=" << mxRR << " std=" << sdRR << " px" << endl;
            out << "  Count: " << globalStats.centroidRoundtripLeftSdk.size() << endl;
        }
        out << endl;

        // (6) 批量匹配计数差异
        out << "--- Fiducial Count Difference (SDK - Rev, per frame) ---" << endl;
        {
            if ( !globalStats.fidCountDiffs.empty() )
            {
                int32_t minDiff = *min_element( globalStats.fidCountDiffs.begin(),
                                                globalStats.fidCountDiffs.end() );
                int32_t maxDiff = *max_element( globalStats.fidCountDiffs.begin(),
                                                globalStats.fidCountDiffs.end() );
                double sum = 0.0;
                uint32_t zeroCount = 0;
                for ( auto d : globalStats.fidCountDiffs )
                {
                    sum += d;
                    if ( d == 0 ) ++zeroCount;
                }
                double meanDiff = sum / static_cast<double>( globalStats.fidCountDiffs.size() );
                out << "  Mean diff:   " << setprecision( 2 ) << meanDiff << endl;
                out << "  Min diff:    " << minDiff << endl;
                out << "  Max diff:    " << maxDiff << endl;
                out << "  Exact match: " << zeroCount << " / "
                    << globalStats.fidCountDiffs.size() << " frames" << endl;
            }
            else
            {
                out << "  No data" << endl;
            }
        }
        out << endl;

        // (7) 批量匹配 3D 位置差异
        out << "--- Batch matchAndTriangulate 3D Position Diff ---" << endl;
        {
            double mean, mx, sd;
            DiffStats::summarize( globalStats.batchPosDiffs, mean, mx, sd );
            out << "  Mean:   " << setprecision( 6 ) << mean << " mm" << endl;
            out << "  Max:    " << mx << " mm" << endl;
            out << "  StdDev: " << sd << " mm" << endl;
            out << "  Matched pairs: " << globalStats.batchPosDiffs.size() << endl;
        }
        out << endl;

        // (8) 工具识别差异
        out << "--- Marker Identification Difference ---" << endl;
        {
            out << "  Marker count match (frames):   " << globalStats.markerCountMatch << endl;
            out << "  Marker count mismatch (frames): " << globalStats.markerCountMismatch << endl;
            out << "  GeomID match (markers):   " << globalStats.markerGeomIdMatch << endl;
            out << "  GeomID mismatch (markers): " << globalStats.markerGeomIdMismatch << endl;
        }
        out << endl;

        // (9) 工具变换差异
        out << "--- Marker Transformation Difference ---" << endl;
        {
            double meanT, mxT, sdT, meanR, mxR, sdR, meanRE, mxRE, sdRE;
            DiffStats::summarize( globalStats.markerTransDiffs, meanT, mxT, sdT );
            DiffStats::summarize( globalStats.markerRotDiffs, meanR, mxR, sdR );
            DiffStats::summarize( globalStats.markerRegErrDiffs, meanRE, mxRE, sdRE );
            out << "  Translation diff: mean=" << setprecision( 4 ) << meanT
                << " max=" << mxT << " std=" << sdT << " mm" << endl;
            out << "  Rotation diff:    mean=" << meanR
                << " max=" << mxR << " std=" << sdR << " deg" << endl;
            out << "  RegError diff:    mean=" << setprecision( 6 ) << meanRE
                << " max=" << mxRE << " std=" << sdRE << " mm" << endl;
            out << "  Matched markers:  " << globalStats.markerTransDiffs.size() << endl;
        }
        out << endl;

        // (10) 时间统计
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

            // 批量匹配时间
            double revMatchMean, revMatchMax, revMatchStd;
            DiffStats::summarize( globalStats.revMatchTriTime,
                                  revMatchMean, revMatchMax, revMatchStd );
            out << "  matchAndTriangulate (per frame):" << endl;
            out << "    Reverse: mean=" << setprecision( 1 ) << revMatchMean
                << " max=" << revMatchMax << " us" << endl;

            // marker 匹配时间
            double revMkMean, revMkMax, revMkStd;
            DiffStats::summarize( globalStats.revMarkerTime,
                                  revMkMean, revMkMax, revMkStd );
            out << "  matchMarkers (per frame):" << endl;
            out << "    Reverse: mean=" << setprecision( 1 ) << revMkMean
                << " max=" << revMkMax << " us" << endl;

            // 总时间
            double totalSdkTri = accumulate( globalStats.sdkTriangulateTime.begin(),
                                             globalStats.sdkTriangulateTime.end(), 0.0 );
            double totalRevTri = accumulate( globalStats.revTriangulateTime.begin(),
                                             globalStats.revTriangulateTime.end(), 0.0 );
            double totalSdkRep = accumulate( globalStats.sdkReprojectTime.begin(),
                                             globalStats.sdkReprojectTime.end(), 0.0 );
            double totalRevRep = accumulate( globalStats.revReprojectTime.begin(),
                                             globalStats.revReprojectTime.end(), 0.0 );
            double totalRevMatch = accumulate( globalStats.revMatchTriTime.begin(),
                                               globalStats.revMatchTriTime.end(), 0.0 );
            double totalRevMk = accumulate( globalStats.revMarkerTime.begin(),
                                             globalStats.revMarkerTime.end(), 0.0 );
            out << endl;
            out << "  Total time:" << endl;
            out << "    SDK triangulation:     " << setprecision( 0 )
                << totalSdkTri / 1000.0 << " ms" << endl;
            out << "    Rev triangulation:     " << totalRevTri / 1000.0 << " ms" << endl;
            out << "    SDK reprojection:      " << totalSdkRep / 1000.0 << " ms" << endl;
            out << "    Rev reprojection:      " << totalRevRep / 1000.0 << " ms" << endl;
            out << "    Rev matchAndTriang:    " << totalRevMatch / 1000.0 << " ms" << endl;
            out << "    Rev matchMarkers:      " << totalRevMk / 1000.0 << " ms" << endl;
        }
        out << endl;

        // (11) 结论
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

            if ( globalStats.markerGeomIdMismatch > 0 )
            {
                out << "  WARNING: " << globalStats.markerGeomIdMismatch
                    << " marker(s) with mismatched geometry ID." << endl;
            }

            if ( globalStats.markerCountMismatch > 0 )
            {
                out << "  WARNING: " << globalStats.markerCountMismatch
                    << " frame(s) with different marker count." << endl;
            }

            if ( globalStats.anomalyCount > 0 )
            {
                out << "  " << globalStats.anomalyCount << " anomalous samples saved for analysis."
                    << endl;
            }

            out << endl;
            out << "  Output files:" << endl;
            out << "    anomaly_fiducials_3d.csv        - Anomalous triangulation data" << endl;
            out << "    anomaly_reprojections.csv       - Anomalous reprojection data" << endl;
            out << "    anomaly_centroid_roundtrip.csv   - Anomalous centroid round-trip data" << endl;
            out << "    anomaly_markers.csv             - Anomalous marker data" << endl;
            out << "    batch_match_diff.csv            - Batch matching count/3D diff" << endl;
            out << "    frame_summary.csv               - Per-frame comparison summary" << endl;
            out << "    timing_detail.csv               - Per-point execution time log" << endl;
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
    anomalyCentroidCsv.close();
    anomalyMarkerCsv.close();
    batchMatchCsv.close();
    frameSummaryCsv.close();
    timingCsv.close();

    cout << endl;
    if ( capturedFrames > 0 )
    {
        cout << "Output saved to: " << outputDir << "/" << endl;
        cout << endl;
        cout << "Output files:" << endl;
        cout << "  analysis_report.txt              - Final analysis report" << endl;
        cout << "  anomaly_fiducials_3d.csv         - Anomalous triangulation data" << endl;
        cout << "  anomaly_reprojections.csv        - Anomalous reprojection data" << endl;
        cout << "  anomaly_centroid_roundtrip.csv   - Anomalous centroid round-trip data" << endl;
        cout << "  anomaly_markers.csv              - Anomalous marker identification/pose data" << endl;
        cout << "  batch_match_diff.csv             - Per-frame batch matching count and 3D diff" << endl;
        cout << "  frame_summary.csv                - Per-frame comparison summary (all metrics)" << endl;
        cout << "  timing_detail.csv                - Per-point execution time log" << endl;
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
