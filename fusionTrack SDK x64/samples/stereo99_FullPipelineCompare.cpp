// ============================================================================
//
//   stereo99_FullPipelineCompare.cpp
//   完整逆向算法管线 vs SDK 全流程对比
//
//   与 stereo99_CompareAlgorithms.cpp 的区别:
//   - CompareAlgorithms: 使用 SDK 提供的 2D 检测结果作为逆向算法输入
//   - FullPipelineCompare: 数据来源完全由逆向算法从原始图像开始走完整流程
//
//   完整管线流程 (reverse_algo_lib):
//     1. 原始图像 -> detectBlobs      (圆心拟合)
//     2. 检测结果 -> matchEpipolar    (极线匹配 + 三角化)
//     3. 3D 点    -> matchMarkers     (工具识别 / Kabsch 配准)
//
//   每步结果与 SDK 输出对比:
//     Step 1: 我们检测到的圆心 vs SDK rawData (2D 检测)
//     Step 2: 我们的极线匹配对 vs SDK threeDFiducials
//     Step 3: 我们的工具识别 vs SDK markers
//
//   配置:
//     cstKeepRejectedPointsDef = 1 (保留被 WorkingVolume 拒绝的点)
//     被拒绝的点 (status != 0) 不参与对比统计
//
//   重点监测:
//     - 工具识别状态不一致 (SDK 识别到 / 我们没识别到, 或反之)
//     - 工具变换差异大 (平移/旋转)
//     - 圆心检测数量差异
//     - 极线匹配对差异
//
//   异常数据保存:
//     - 所有异常帧的左右相机图像 (PGM)
//     - 各步骤对比异常 CSV
//     - 最终分析报告
//
//   时间统计:
//     - 我们完整管线处理一帧的时间 (检测 + 匹配 + 识别)
//     - SDK 帧处理时间 (近似)
//     - 各步骤分别计时
//
//   比较及保存在最后进行: 每帧先积累结果, 最终统一对比和保存
//
//   编译:
//     需要 fusionTrack SDK include 目录 + reverse_algo_lib 目录
//
//   用法:
//     stereo99_FullPipelineCompare -g geometry072.ini [-c config.json]
//                                  [-n 200] [-o ./fullpipe_output]
//
// ============================================================================

#include "geometryHelper.hpp"
#include "helpers.hpp"

// 核心逆向算法库 (独立于 SDK)
#include "StereoAlgoLib.h"
// SDK 兼容管线封装 (用于工具匹配)
#include "StereoAlgoPipeline.h"

#include <algorithm>
#include <cerrno>
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
#include <set>
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
// High-resolution timer
using Clock = chrono::high_resolution_clock;
using Duration = chrono::duration<double, micro>;  // microseconds

// ---------------------------------------------------------------------------
// Helper: create directory (ignore if exists)
static void ensureDir( const string& path )
{
    if ( path.empty() ) return;

    errno = 0;
    const int rc = MKDIR( path.c_str() );
    if ( rc != 0 && errno != EEXIST )
        cerr << "WARNING: Cannot create directory '" << path
             << "' (errno=" << errno << ")" << endl;
}

// ---------------------------------------------------------------------------
// Helper: save raw image as PGM (P5 binary)
static bool savePGMBinary( const string& fileName, uint16 width, uint16 height,
                           const uint8_t* pixels )
{
    ofstream file( fileName.c_str(), ios::binary );
    if ( !file.is_open() ) return false;
    file << "P5\n" << width << " " << height << "\n255\n";
    file.write( reinterpret_cast< const char* >( pixels ),
                static_cast< streamsize >( width ) * height );
    file.close();
    return true;
}

// ---------------------------------------------------------------------------
// Helper: save device image (with stride) as PGM
static bool savePGMWithStride( const string& fileName, uint16 width, uint16 height,
                               int32 strideBytes, const uint8* pixels )
{
    ofstream file( fileName.c_str(), ios::binary );
    if ( !file.is_open() ) return false;
    file << "P5\n" << width << " " << height << "\n255\n";
    for ( uint16 row = 0; row < height; ++row )
        file.write( reinterpret_cast< const char* >( pixels + row * strideBytes ), width );
    file.close();
    return true;
}

// ---------------------------------------------------------------------------
// Helper: copy image pixels from strided device buffer to contiguous buffer
static vector<uint8_t> copyImageContiguous( uint16 width, uint16 height,
                                             int32 strideBytes, const uint8* pixels )
{
    vector<uint8_t> img( static_cast<size_t>( width ) * height );
    for ( uint16 row = 0; row < height; ++row )
    {
        memcpy( img.data() + row * width,
                pixels + row * strideBytes,
                width );
    }
    return img;
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
// 3D point distance
static double point3DDistance( const ftk3DPoint& a, const ftk3DPoint& b )
{
    double dx = static_cast<double>( a.x ) - static_cast<double>( b.x );
    double dy = static_cast<double>( a.y ) - static_cast<double>( b.y );
    double dz = static_cast<double>( a.z ) - static_cast<double>( b.z );
    return sqrt( dx * dx + dy * dy + dz * dz );
}

// ---------------------------------------------------------------------------
// 旋转矩阵差异 (degrees)
static double rotationDiffDegrees( const floatXX a[3][3], const floatXX b[3][3] )
{
    double R[3][3] = {};
    for ( int i = 0; i < 3; ++i )
        for ( int j = 0; j < 3; ++j )
            for ( int k = 0; k < 3; ++k )
                R[i][j] += static_cast<double>( a[i][k] ) * static_cast<double>( b[j][k] );
    double trace = R[0][0] + R[1][1] + R[2][2];
    double cosAngle = ( trace - 1.0 ) / 2.0;
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
// 检查 fiducial 状态是否被 WorkingVolume 拒绝
// SDK status bits: 0x40 = ftk3DStatus_OutOfVolume, 0x20 = ftk3DStatus_NotEnoughCandidates
static bool isRejectedByWorkingVolume( uint32 statusBits )
{
    return ( statusBits & 0x40 ) != 0;
}

// ===========================================================================
// 对比配置常量
// ===========================================================================

static constexpr double kInfiniteDistance         = 1e9;     // 哨兵值: 表示无匹配
static constexpr double kMaxCentroidMatchDistPx   = 10.0;    // 圆心匹配最大搜索距离 (pixels)
static constexpr double kMaxFidPairingDistMM      = 100.0;   // 3D fiducial 配对最大距离 (mm)
static constexpr size_t kNotFound                 = SIZE_MAX; // 未找到索引哨兵值

// ===========================================================================
// 每帧存储结构 - 积累所有数据, 最后统一对比
// ===========================================================================

/// 我们检测到的单个 blob
struct OurBlob
{
    float centerX, centerY;
    uint32_t area;
    uint16_t width, height;
};

/// 我们的极线匹配结果
struct OurFiducial
{
    uint32_t leftBlobIdx;
    uint32_t rightBlobIdx;
    float posX, posY, posZ;       // mm
    float epipolarError;          // pixels
    float triangulationError;     // mm
    float probability;
};

/// 我们的工具识别结果
struct OurMarker
{
    uint32_t geometryId;
    float translationMM[3];
    float rotation[3][3];
    float registrationErrorMM;
    uint32_t geometryPresenceMask;
};

/// SDK 单帧数据快照
struct SdkFrameData
{
    // raw detections
    struct RawDet { float cx, cy; uint32_t pixelCount; uint16_t w, h; uint32 statusBits; };
    vector<RawDet> leftRaw, rightRaw;

    // 3D fiducials
    struct Fid3D {
        float px, py, pz;
        uint32_t leftIndex, rightIndex;
        float epiErr, triErr;
        float probability;
        uint32 statusBits;
    };
    vector<Fid3D> fiducials;

    // markers
    struct Marker {
        uint32_t geometryId;
        float translationMM[3];
        float rotation[3][3];
        float registrationErrorMM;
        uint32_t geometryPresenceMask;
        uint32 statusBits;
    };
    vector<Marker> markers;
};

/// 我们的完整管线结果
struct OurFrameData
{
    vector<OurBlob> leftBlobs, rightBlobs;
    vector<OurFiducial> fiducials;
    vector<OurMarker> markers;

    // 分步计时 (microseconds)
    double detectLeftUs  = 0.0;
    double detectRightUs = 0.0;
    double matchTriUs    = 0.0;
    double markerUs      = 0.0;
    double totalUs       = 0.0;  // 全管线总耗时
};

/// 单帧完整数据
struct FrameRecord
{
    uint32_t frameIdx;
    SdkFrameData sdk;
    OurFrameData ours;

    // 图像 (仅在需要保存时填充)
    vector<uint8_t> leftImage, rightImage;
    uint16 imageWidth = 0, imageHeight = 0;

    bool hasImages = false;
    bool anomalous = false;  // 标记为异常帧
    string anomalyReason;    // 异常原因描述
};

// ===========================================================================
// 统计辅助
// ===========================================================================
struct StatsSummary
{
    double mean = 0.0, maxVal = 0.0, stddev = 0.0;
    size_t count = 0;
};

static StatsSummary computeStats( const vector<double>& v )
{
    StatsSummary s;
    s.count = v.size();
    if ( v.empty() ) return s;
    double sum = accumulate( v.begin(), v.end(), 0.0 );
    s.mean = sum / static_cast<double>( v.size() );
    s.maxVal = *max_element( v.begin(), v.end() );
    double sqSum = 0.0;
    for ( auto x : v ) sqSum += ( x - s.mean ) * ( x - s.mean );
    s.stddev = sqrt( sqSum / static_cast<double>( v.size() ) );
    return s;
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

    cout << "=== stereo99_FullPipelineCompare ===" << endl;
    cout << "Full Reverse-Engineered Pipeline vs SDK Comparison" << endl;
    cout << "Reverse-Engineered Library: "
         << stereo_algo::StereoVision::version() << endl;

    deque< string > args;
    for ( int i( 1 ); i < argc; ++i )
        args.emplace_back( argv[ i ] );

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
    string outputDir = "D:/fullpipe_output";
    double centroidThreshold  = 1.0;   // pixels - 圆心检测差异阈值
    double posThreshold       = 0.5;   // mm - 3D 位置差异阈值
    double epiThreshold       = 0.1;   // pixels - 极线误差差异阈值
    double markerTransThreshold = 1.0; // mm - 工具平移差异阈值
    double markerRotThreshold   = 0.5; // degrees - 工具旋转差异阈值

    if ( showHelp || args.empty() )
    {
        cout << setw( 40u ) << "[-h/--help] " << flush
             << "Displays this help and exits." << endl;
        cout << setw( 40u ) << "[-c/--config F] " << flush
             << "JSON config file." << endl;
        cout << setw( 40u ) << "[-g/--geom F (F ...)] " << flush
             << "geometry file(s)." << endl;
        cout << setw( 40u ) << "[-n/--frames N] " << flush
             << "Number of frames, default = 200" << endl;
        cout << setw( 40u ) << "[-o/--output DIR] " << flush
             << "Output directory, default = ./fullpipe_output" << endl;
        cout << setw( 40u ) << "[--centroid-threshold T] " << flush
             << "Centroid diff threshold (px), default = 1.0" << endl;
        cout << setw( 40u ) << "[--pos-threshold T] " << flush
             << "3D position diff threshold (mm), default = 0.5" << endl;
        cout << setw( 40u ) << "[--epi-threshold T] " << flush
             << "Epipolar diff threshold (px), default = 0.1" << endl;
        cout << setw( 40u ) << "[--marker-trans-threshold T] " << flush
             << "Marker translation diff threshold (mm), default = 1.0" << endl;
        cout << setw( 40u ) << "[--marker-rot-threshold T] " << flush
             << "Marker rotation diff threshold (deg), default = 0.5" << endl;
    }

    cout << "Full pipeline comparison tool (2026)" << endl;
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
        cfgFile = *pos;

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "-g" || val == "--geom"; } );
    if ( pos != args.cend() )
    {
        geomFiles.clear();
        while ( pos != args.cend() && ++pos != args.cend() )
        {
            if ( pos->substr( 0u, 1u ) == "-" ) break;
            geomFiles.emplace_back( *pos );
        }
    }

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "-n" || val == "--frames"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
        numFrames = static_cast< uint32 >( atoi( pos->c_str() ) );

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "-o" || val == "--output"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
        outputDir = *pos;

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "--centroid-threshold"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
        centroidThreshold = atof( pos->c_str() );

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "--pos-threshold"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
        posThreshold = atof( pos->c_str() );

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "--epi-threshold"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
        epiThreshold = atof( pos->c_str() );

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "--marker-trans-threshold"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
        markerTransThreshold = atof( pos->c_str() );

    pos = find_if( args.cbegin(), args.cend(),
                   []( const string& val ) { return val == "--marker-rot-threshold"; } );
    if ( pos != args.cend() && ++pos != args.cend() )
        markerRotThreshold = atof( pos->c_str() );

    // Create output directories
    ensureDir( outputDir );
    string anomalyImagesDir = outputDir + "/anomaly_images";
    ensureDir( anomalyImagesDir );

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
        error( "Cannot retrieve any options.", !isNotFromConsole );

    // -----------------------------------------------------------------------
    // SpryTrack: enable image sending + embedded processing
    if ( ftkDeviceType::DEV_SPRYTRACK_180 == device.Type ||
         ftkDeviceType::DEV_SPRYTRACK_300 == device.Type )
    {
        if ( options.find( "Enable images sending" ) != options.cend() )
            ftkSetInt32( lib, sn, options[ "Enable images sending" ], 1 );
        if ( options.find( "Enable embedded processing" ) != options.cend() )
            ftkSetInt32( lib, sn, options[ "Enable embedded processing" ], 1 );
    }

    // -----------------------------------------------------------------------
    // Enable calibration export
    if ( options.find( "Calibration export" ) != options.cend() )
    {
        err = ftkSetInt32( lib, sn, options[ "Calibration export" ], 1 );
        if ( err != ftkError::FTK_OK )
            cerr << "WARNING: Cannot enable calibration export" << endl;
        else
            cout << "Calibration export enabled" << endl;
    }

    // -----------------------------------------------------------------------
    // cstKeepRejectedPointsDef = 1 (保留被 WorkingVolume 拒绝的点)
    // 通过 "Ignore hard working volume cuts" 选项设置
    // 被拒绝的点保留在输出中但不参与对比
    if ( options.find( "Ignore hard working volume cuts" ) != options.cend() )
    {
        err = ftkSetInt32( lib, sn, options[ "Ignore hard working volume cuts" ], 1 );
        if ( err == ftkError::FTK_OK )
            cout << "Ignore hard working volume cuts: ENABLED (keep rejected points)" << endl;
    }

    // -----------------------------------------------------------------------
    // Set geometry with SDK

    ftkRigidBody geom{};
    for ( const auto& geomFile : geomFiles )
    {
        const int loadResult = loadRigidBody( lib, geomFile, geom );
        if ( loadResult == 1 )
            cout << "Loaded geometry from installation directory: " << geomFile << endl;

        if ( loadResult == 0 || loadResult == 1 )
        {
            if ( ftkError::FTK_OK != ftkSetRigidBody( lib, sn, &geom ) )
                checkError( lib, !isNotFromConsole );
        }
        else
        {
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

    // 请求所有数据: pixels=true, events=16, leftRaw=256, rightRaw=256, fid3d=256, markers=32
    err = ftkSetFrameOptions( true, 16u, 256u, 256u, 256u, 32u, frame );
    if ( err != ftkError::FTK_OK )
    {
        ftkDeleteFrame( frame );
        cerr << "Cannot initialise frame" << endl;
        checkError( lib, !isNotFromConsole );
    }

    // -----------------------------------------------------------------------
    // Initialize reverse-engineered pipeline
    // StereoVision: 核心算法 (blob 检测, 三角化, 极线匹配)
    // StereoAlgoPipeline: SDK 兼容封装 (工具匹配)

    stereo_algo::StereoVision sv;
    stereo_algo::StereoAlgoPipeline revPipeline;
    bool revInitialized = false;

    // 注册几何体到逆向管线的 marker 匹配器
    for ( const auto& geomFile : geomFiles )
    {
        ftkRigidBody tmpGeom{};
        if ( loadRigidBody( lib, geomFile, tmpGeom ) <= 1 )
        {
            if ( revPipeline.registerGeometry( tmpGeom ) )
            {
                cout << "Geometry registered with reverse pipeline (ID="
                     << tmpGeom.geometryId << ", points=" << tmpGeom.pointsCount << ")" << endl;
            }
        }
    }

    // -----------------------------------------------------------------------
    // 帧数据积累 (所有帧结果暂存, 最终统一对比)

    vector<FrameRecord> allFrames;
    allFrames.reserve( numFrames );

    cout << "\n=== Full Pipeline Comparison Configuration ===" << endl;
    cout << "  Frames to capture:        " << numFrames << endl;
    cout << "  Centroid threshold:        " << centroidThreshold << " px" << endl;
    cout << "  Position threshold:        " << posThreshold << " mm" << endl;
    cout << "  Epipolar threshold:        " << epiThreshold << " px" << endl;
    cout << "  Marker trans threshold:    " << markerTransThreshold << " mm" << endl;
    cout << "  Marker rot threshold:      " << markerRotThreshold << " deg" << endl;
    cout << "  Output directory:          " << outputDir << endl;
    cout << "  KeepRejectedPoints:        ON (excluded from comparison)" << endl;

    // =======================================================================
    //  PHASE 1: 逐帧采集 + 管线执行
    // =======================================================================

    cout << "\nCapturing " << numFrames << " frames..." << endl;
    cout << "Place a marker with the loaded geometry in front of the device.\n" << endl;

    uint32 capturedFrames = 0;
    uint32 maxAttempts = numFrames * 20u;

    for ( uint32 attempt = 0; attempt < maxAttempts && capturedFrames < numFrames; ++attempt )
    {
        err = ftkGetLastFrame( lib, sn, frame, 200u );
        if ( err > ftkError::FTK_OK || err == ftkError::FTK_WAR_NO_FRAME )
            continue;

        // ------------------------------------------------------------------
        // 从第一帧提取标定并初始化逆向管线
        if ( !revInitialized )
        {
            ftkFrameInfoData infoData;
            infoData.WantedInformation = ftkInformationType::CalibrationParameters;
            ftkError calibErr = ftkExtractFrameInfo( frame, &infoData );
            if ( calibErr == ftkError::FTK_OK )
            {
                if ( revPipeline.initialize( infoData.Calibration ) )
                {
                    // 也初始化核心 StereoVision (从 ftkStereoParameters)
                    stereo_algo::StereoCalibration cal = {};
                    cal.leftCam.focalLength[0] = infoData.Calibration.LeftCamera.FocalLength[0];
                    cal.leftCam.focalLength[1] = infoData.Calibration.LeftCamera.FocalLength[1];
                    cal.leftCam.opticalCentre[0] = infoData.Calibration.LeftCamera.OpticalCentre[0];
                    cal.leftCam.opticalCentre[1] = infoData.Calibration.LeftCamera.OpticalCentre[1];
                    for ( int i = 0; i < 5; ++i )
                        cal.leftCam.distortion[i] = infoData.Calibration.LeftCamera.Distorsions[i];
                    cal.leftCam.skew = infoData.Calibration.LeftCamera.Skew;

                    cal.rightCam.focalLength[0] = infoData.Calibration.RightCamera.FocalLength[0];
                    cal.rightCam.focalLength[1] = infoData.Calibration.RightCamera.FocalLength[1];
                    cal.rightCam.opticalCentre[0] = infoData.Calibration.RightCamera.OpticalCentre[0];
                    cal.rightCam.opticalCentre[1] = infoData.Calibration.RightCamera.OpticalCentre[1];
                    for ( int i = 0; i < 5; ++i )
                        cal.rightCam.distortion[i] = infoData.Calibration.RightCamera.Distorsions[i];
                    cal.rightCam.skew = infoData.Calibration.RightCamera.Skew;

                    for ( int i = 0; i < 3; ++i ) cal.translation[i] = infoData.Calibration.Translation[i];
                    for ( int i = 0; i < 3; ++i ) cal.rotation[i] = infoData.Calibration.Rotation[i];

                    sv.initialize( cal );
                    revInitialized = true;
                    cout << "Reverse pipeline initialized with calibration." << endl;
                }
            }
        }
        if ( !revInitialized ) continue;

        // ------------------------------------------------------------------
        // 检查是否有图像数据 (完整管线需要从图像开始)
        bool hasImages = ( frame->imageLeftStat == ftkQueryStatus::QS_OK &&
                           frame->imageRightStat == ftkQueryStatus::QS_OK &&
                           frame->imageHeaderStat == ftkQueryStatus::QS_OK &&
                           frame->imageLeftPixels != nullptr &&
                           frame->imageRightPixels != nullptr &&
                           frame->imageHeader != nullptr );

        if ( !hasImages ) continue;

        uint16 imgW = frame->imageHeader->width;
        uint16 imgH = frame->imageHeader->height;
        int32 imgStride = frame->imageHeader->imageStrideInBytes;

        FrameRecord rec;
        rec.frameIdx = capturedFrames;
        rec.imageWidth = imgW;
        rec.imageHeight = imgH;
        rec.hasImages = true;

        // ------------------------------------------------------------------
        // 复制图像到连续内存 (供 detectBlobs 使用, 同时暂存到 FrameRecord
        // 以便后续 Phase 3 对异常帧保存图像)
        vector<uint8_t> leftImg = copyImageContiguous( imgW, imgH, imgStride,
                                                        frame->imageLeftPixels );
        vector<uint8_t> rightImg = copyImageContiguous( imgW, imgH, imgStride,
                                                         frame->imageRightPixels );

        // 暂存图像到 FrameRecord (Phase 3 只保存异常帧)
        rec.leftImage = leftImg;
        rec.rightImage = rightImg;

        // ==================================================================
        // 我们的完整管线: image -> blobs -> match -> triangulate -> markers
        // ==================================================================

        auto tPipeStart = Clock::now();

        // ---- Step 1: 圆心拟合 (detectBlobs) ----
        const uint32_t kMaxBlobs = 256;

        vector<stereo_algo::BlobDetection> leftBlobs( kMaxBlobs );
        auto tDL0 = Clock::now();
        uint32_t leftBlobCount = sv.detectBlobs(
            leftImg.data(), imgW, imgH,
            leftBlobs.data(), kMaxBlobs,
            10, 4, 10000, 0.3f );
        auto tDL1 = Clock::now();

        vector<stereo_algo::BlobDetection> rightBlobs( kMaxBlobs );
        auto tDR0 = Clock::now();
        uint32_t rightBlobCount = sv.detectBlobs(
            rightImg.data(), imgW, imgH,
            rightBlobs.data(), kMaxBlobs,
            10, 4, 10000, 0.3f );
        auto tDR1 = Clock::now();

        rec.ours.detectLeftUs = Duration( tDL1 - tDL0 ).count();
        rec.ours.detectRightUs = Duration( tDR1 - tDR0 ).count();

        // 保存检测结果
        rec.ours.leftBlobs.resize( leftBlobCount );
        for ( uint32_t i = 0; i < leftBlobCount; ++i )
        {
            rec.ours.leftBlobs[i] = { leftBlobs[i].centerX, leftBlobs[i].centerY,
                                      leftBlobs[i].area, leftBlobs[i].width, leftBlobs[i].height };
        }
        rec.ours.rightBlobs.resize( rightBlobCount );
        for ( uint32_t i = 0; i < rightBlobCount; ++i )
        {
            rec.ours.rightBlobs[i] = { rightBlobs[i].centerX, rightBlobs[i].centerY,
                                       rightBlobs[i].area, rightBlobs[i].width, rightBlobs[i].height };
        }

        // ---- Step 2: 极线匹配 + 三角化 (matchEpipolar) ----
        // 将 BlobDetection 转换为 Detection2D
        vector<stereo_algo::Detection2D> leftDets( leftBlobCount );
        vector<stereo_algo::Detection2D> rightDets( rightBlobCount );
        for ( uint32_t i = 0; i < leftBlobCount; ++i )
        {
            leftDets[i].centerX = leftBlobs[i].centerX;
            leftDets[i].centerY = leftBlobs[i].centerY;
            leftDets[i].index = i;
        }
        for ( uint32_t i = 0; i < rightBlobCount; ++i )
        {
            rightDets[i].centerX = rightBlobs[i].centerX;
            rightDets[i].centerY = rightBlobs[i].centerY;
            rightDets[i].index = i;
        }

        uint32_t maxMatchOut = max( leftBlobCount * rightBlobCount,
                                    static_cast<uint32_t>( 256 ) );
        if ( maxMatchOut > 10000 ) maxMatchOut = 10000;
        vector<stereo_algo::EpipolarMatchResult> matchResults( maxMatchOut );

        auto tMT0 = Clock::now();
        uint32_t matchCount = sv.matchEpipolar(
            leftDets.data(), leftBlobCount,
            rightDets.data(), rightBlobCount,
            matchResults.data(), maxMatchOut,
            5.0 );  // SDK 默认极线距离
        auto tMT1 = Clock::now();

        rec.ours.matchTriUs = Duration( tMT1 - tMT0 ).count();

        // 保存匹配结果为 OurFiducial
        rec.ours.fiducials.resize( matchCount );
        for ( uint32_t i = 0; i < matchCount; ++i )
        {
            rec.ours.fiducials[i].leftBlobIdx = matchResults[i].leftIndex;
            rec.ours.fiducials[i].rightBlobIdx = matchResults[i].rightIndex;
            rec.ours.fiducials[i].posX = static_cast<float>( matchResults[i].position.x );
            rec.ours.fiducials[i].posY = static_cast<float>( matchResults[i].position.y );
            rec.ours.fiducials[i].posZ = static_cast<float>( matchResults[i].position.z );
            rec.ours.fiducials[i].epipolarError = static_cast<float>( matchResults[i].epipolarError );
            rec.ours.fiducials[i].triangulationError = static_cast<float>( matchResults[i].triangulationError );
            rec.ours.fiducials[i].probability = static_cast<float>( matchResults[i].probability );
        }

        // ---- Step 3: 工具识别 (matchMarkers) ----
        // 转换为 ftk3DFiducial 格式给 StereoAlgoPipeline
        vector<ftk3DFiducial> ourFid3d( matchCount );
        for ( uint32_t i = 0; i < matchCount; ++i )
        {
            memset( &ourFid3d[i], 0, sizeof( ftk3DFiducial ) );
            ourFid3d[i].positionMM.x = rec.ours.fiducials[i].posX;
            ourFid3d[i].positionMM.y = rec.ours.fiducials[i].posY;
            ourFid3d[i].positionMM.z = rec.ours.fiducials[i].posZ;
            ourFid3d[i].leftIndex = rec.ours.fiducials[i].leftBlobIdx;
            ourFid3d[i].rightIndex = rec.ours.fiducials[i].rightBlobIdx;
            ourFid3d[i].epipolarErrorPixels = rec.ours.fiducials[i].epipolarError;
            ourFid3d[i].triangulationErrorMM = rec.ours.fiducials[i].triangulationError;
            ourFid3d[i].probability = rec.ours.fiducials[i].probability;
        }

        vector<ftkMarker> ourMarkers( 32 );
        auto tMk0 = Clock::now();
        uint32_t ourMarkerCount = 0;
        if ( matchCount >= 3 )
        {
            ourMarkerCount = revPipeline.matchMarkers(
                ourFid3d.data(), matchCount,
                ourMarkers.data(), static_cast<uint32_t>( ourMarkers.size() ) );
        }
        auto tMk1 = Clock::now();

        rec.ours.markerUs = Duration( tMk1 - tMk0 ).count();

        auto tPipeEnd = Clock::now();
        rec.ours.totalUs = Duration( tPipeEnd - tPipeStart ).count();

        // 保存 marker 结果
        rec.ours.markers.resize( ourMarkerCount );
        for ( uint32_t i = 0; i < ourMarkerCount; ++i )
        {
            rec.ours.markers[i].geometryId = ourMarkers[i].geometryId;
            for ( int d = 0; d < 3; ++d )
                rec.ours.markers[i].translationMM[d] = ourMarkers[i].translationMM[d];
            for ( int r = 0; r < 3; ++r )
                for ( int c = 0; c < 3; ++c )
                    rec.ours.markers[i].rotation[r][c] = ourMarkers[i].rotation[r][c];
            rec.ours.markers[i].registrationErrorMM = ourMarkers[i].registrationErrorMM;
            rec.ours.markers[i].geometryPresenceMask = ourMarkers[i].geometryPresenceMask;
        }

        // ==================================================================
        // SDK 数据快照
        // ==================================================================

        // Left raw data
        if ( frame->rawDataLeftStat == ftkQueryStatus::QS_OK )
        {
            rec.sdk.leftRaw.resize( frame->rawDataLeftCount );
            for ( uint32 m = 0; m < frame->rawDataLeftCount; ++m )
            {
                rec.sdk.leftRaw[m].cx = frame->rawDataLeft[m].centerXPixels;
                rec.sdk.leftRaw[m].cy = frame->rawDataLeft[m].centerYPixels;
                rec.sdk.leftRaw[m].pixelCount = frame->rawDataLeft[m].pixelsCount;
                rec.sdk.leftRaw[m].w = frame->rawDataLeft[m].width;
                rec.sdk.leftRaw[m].h = frame->rawDataLeft[m].height;
                rec.sdk.leftRaw[m].statusBits = static_cast<uint32>( frame->rawDataLeft[m].status );
            }
        }

        // Right raw data
        if ( frame->rawDataRightStat == ftkQueryStatus::QS_OK )
        {
            rec.sdk.rightRaw.resize( frame->rawDataRightCount );
            for ( uint32 m = 0; m < frame->rawDataRightCount; ++m )
            {
                rec.sdk.rightRaw[m].cx = frame->rawDataRight[m].centerXPixels;
                rec.sdk.rightRaw[m].cy = frame->rawDataRight[m].centerYPixels;
                rec.sdk.rightRaw[m].pixelCount = frame->rawDataRight[m].pixelsCount;
                rec.sdk.rightRaw[m].w = frame->rawDataRight[m].width;
                rec.sdk.rightRaw[m].h = frame->rawDataRight[m].height;
                rec.sdk.rightRaw[m].statusBits = static_cast<uint32>( frame->rawDataRight[m].status );
            }
        }

        // 3D fiducials
        if ( frame->threeDFiducialsStat == ftkQueryStatus::QS_OK )
        {
            rec.sdk.fiducials.resize( frame->threeDFiducialsCount );
            for ( uint32 m = 0; m < frame->threeDFiducialsCount; ++m )
            {
                rec.sdk.fiducials[m].px = frame->threeDFiducials[m].positionMM.x;
                rec.sdk.fiducials[m].py = frame->threeDFiducials[m].positionMM.y;
                rec.sdk.fiducials[m].pz = frame->threeDFiducials[m].positionMM.z;
                rec.sdk.fiducials[m].leftIndex = frame->threeDFiducials[m].leftIndex;
                rec.sdk.fiducials[m].rightIndex = frame->threeDFiducials[m].rightIndex;
                rec.sdk.fiducials[m].epiErr = frame->threeDFiducials[m].epipolarErrorPixels;
                rec.sdk.fiducials[m].triErr = frame->threeDFiducials[m].triangulationErrorMM;
                rec.sdk.fiducials[m].probability = frame->threeDFiducials[m].probability;
                rec.sdk.fiducials[m].statusBits = static_cast<uint32>( frame->threeDFiducials[m].status );
            }
        }

        // Markers
        if ( frame->markersStat == ftkQueryStatus::QS_OK )
        {
            rec.sdk.markers.resize( frame->markersCount );
            for ( uint32 m = 0; m < frame->markersCount; ++m )
            {
                rec.sdk.markers[m].geometryId = frame->markers[m].geometryId;
                for ( int d = 0; d < 3; ++d )
                    rec.sdk.markers[m].translationMM[d] = frame->markers[m].translationMM[d];
                for ( int r = 0; r < 3; ++r )
                    for ( int c = 0; c < 3; ++c )
                        rec.sdk.markers[m].rotation[r][c] = frame->markers[m].rotation[r][c];
                rec.sdk.markers[m].registrationErrorMM = frame->markers[m].registrationErrorMM;
                rec.sdk.markers[m].geometryPresenceMask = frame->markers[m].geometryPresenceMask;
                rec.sdk.markers[m].statusBits = static_cast<uint32>( frame->markers[m].status );
            }
        }

        // ------------------------------------------------------------------
        // 简要打印进度 (不做比较)
        cout << "Frame " << setw( 4 ) << capturedFrames << ": "
             << "blobs L/R=" << leftBlobCount << "/" << rightBlobCount
             << " (sdk " << rec.sdk.leftRaw.size() << "/" << rec.sdk.rightRaw.size() << ")"
             << " | fid3d=" << matchCount << " (sdk " << rec.sdk.fiducials.size() << ")"
             << " | mk=" << ourMarkerCount << " (sdk " << rec.sdk.markers.size() << ")"
             << " | pipe=" << fixed << setprecision( 0 ) << rec.ours.totalUs << "us"
             << endl;

        allFrames.push_back( move( rec ) );
        ++capturedFrames;

        sleep( 100L );
    }

    cout << "\nCapture complete: " << capturedFrames << " frames.\n" << endl;

    // =======================================================================
    //  PHASE 2: 统一对比 + 异常检测 + 保存
    // =======================================================================

    cout << "=== Phase 2: Comparison and anomaly detection ===" << endl;

    // --- 全局统计向量 ---

    // Step 1: 圆心检测
    vector<double> blobCountDiffLeft, blobCountDiffRight;
    vector<double> centroidMatchDists;   // 匹配上的圆心距离
    uint32_t centroidMatchCount = 0;
    uint32_t centroidMissCount = 0;

    // Step 2: 极线匹配+三角化
    vector<double> fidCountDiffs;      // SDK - ours 每帧 fiducial 数量差
    vector<double> matchedPosDiffs;    // 匹配上的 3D 位置差
    vector<double> matchedEpiDiffs;    // 极线误差差
    vector<double> matchedTriDiffs;    // 三角化误差差
    uint32_t pairMatchCount = 0;

    // Step 3: 工具识别
    uint32_t markerCountMatch = 0, markerCountMismatch = 0;
    uint32_t markerGeomIdMatch = 0, markerGeomIdMismatch = 0;
    uint32_t sdkDetectedWeNot = 0;     // SDK 识别到工具, 我们没有
    uint32_t weDetectedSdkNot = 0;     // 我们识别到工具, SDK 没有
    vector<double> markerTransDiffs, markerRotDiffs, markerRegErrDiffs;

    // 时间统计
    vector<double> pipelineTotalUs;
    vector<double> detectTotalUs;
    vector<double> matchTriTotalUs;
    vector<double> markerTotalUs;

    // 异常帧索引列表 (用于保存图像)
    vector<size_t> anomalyFrameIndices;

    // --- 逐帧对比 ---
    for ( size_t fi = 0; fi < allFrames.size(); ++fi )
    {
        FrameRecord& fr = allFrames[fi];
        bool frameAnomaly = false;
        ostringstream anomalyReasons;

        // == Timing ==
        pipelineTotalUs.push_back( fr.ours.totalUs );
        detectTotalUs.push_back( fr.ours.detectLeftUs + fr.ours.detectRightUs );
        matchTriTotalUs.push_back( fr.ours.matchTriUs );
        markerTotalUs.push_back( fr.ours.markerUs );

        // == Step 1: 圆心检测对比 ==
        // 计数差异
        int32_t leftDiff = static_cast<int32_t>( fr.ours.leftBlobs.size() )
                          - static_cast<int32_t>( fr.sdk.leftRaw.size() );
        int32_t rightDiff = static_cast<int32_t>( fr.ours.rightBlobs.size() )
                           - static_cast<int32_t>( fr.sdk.rightRaw.size() );
        blobCountDiffLeft.push_back( leftDiff );
        blobCountDiffRight.push_back( rightDiff );

        // 按面积匹配圆心, 计算质心距离 (左)
        for ( const auto& sdkDet : fr.sdk.leftRaw )
        {
            double bestDist = kInfiniteDistance;
            for ( const auto& ourBlob : fr.ours.leftBlobs )
            {
                if ( ourBlob.area != sdkDet.pixelCount ) continue;
                double dx = ourBlob.centerX - sdkDet.cx;
                double dy = ourBlob.centerY - sdkDet.cy;
                double dist = sqrt( dx * dx + dy * dy );
                if ( dist < bestDist ) bestDist = dist;
            }
            if ( bestDist < kMaxCentroidMatchDistPx )
            {
                centroidMatchDists.push_back( bestDist );
                ++centroidMatchCount;
                if ( bestDist > centroidThreshold )
                {
                    frameAnomaly = true;
                    anomalyReasons << "CentroidL>" << centroidThreshold << "px ";
                }
            }
            else
            {
                ++centroidMissCount;
            }
        }

        // (右)
        for ( const auto& sdkDet : fr.sdk.rightRaw )
        {
            double bestDist = 1e9;
            for ( const auto& ourBlob : fr.ours.rightBlobs )
            {
                if ( ourBlob.area != sdkDet.pixelCount ) continue;
                double dx = ourBlob.centerX - sdkDet.cx;
                double dy = ourBlob.centerY - sdkDet.cy;
                double dist = sqrt( dx * dx + dy * dy );
                if ( dist < bestDist ) bestDist = dist;
            }
            if ( bestDist < 10.0 )
            {
                centroidMatchDists.push_back( bestDist );
                ++centroidMatchCount;
                if ( bestDist > centroidThreshold )
                {
                    frameAnomaly = true;
                    anomalyReasons << "CentroidR>" << centroidThreshold << "px ";
                }
            }
            else
            {
                ++centroidMissCount;
            }
        }

        // == Step 2: 极线匹配 + 三角化对比 ==
        // 因为我们从图像检测的圆心做极线匹配, blob 索引与 SDK 的 rawData 索引不同。
        // 所以我们按照 "同一个左圆心+同一个右圆心" 的方式通过距离来匹配。
        //
        // 策略: 对 SDK 每个 fiducial, 找我们结果中 3D 位置最近的对应。
        // 只对比 status=0 (未被 WorkingVolume 拒绝) 的点。

        int32_t fidDiff = static_cast<int32_t>( fr.sdk.fiducials.size() )
                         - static_cast<int32_t>( fr.ours.fiducials.size() );
        fidCountDiffs.push_back( fidDiff );

        // 构建我们结果的 3D 位置 -> 索引映射 (用最近邻)
        vector<bool> ourFidUsed( fr.ours.fiducials.size(), false );
        for ( const auto& sdkFid : fr.sdk.fiducials )
        {
            // 跳过被 WorkingVolume 拒绝的点
            if ( isRejectedByWorkingVolume( sdkFid.statusBits ) ) continue;

            double bestDist = 1e9;
            size_t bestIdx = SIZE_MAX;
            for ( size_t oi = 0; oi < fr.ours.fiducials.size(); ++oi )
            {
                if ( ourFidUsed[oi] ) continue;
                double dx = sdkFid.px - fr.ours.fiducials[oi].posX;
                double dy = sdkFid.py - fr.ours.fiducials[oi].posY;
                double dz = sdkFid.pz - fr.ours.fiducials[oi].posZ;
                double dist = sqrt( dx * dx + dy * dy + dz * dz );
                if ( dist < bestDist )
                {
                    bestDist = dist;
                    bestIdx = oi;
                }
            }

            if ( bestIdx != SIZE_MAX && bestDist < 100.0 )  // 100mm 最大配对距离
            {
                ourFidUsed[bestIdx] = true;
                matchedPosDiffs.push_back( bestDist );
                matchedEpiDiffs.push_back( fabs( sdkFid.epiErr - fr.ours.fiducials[bestIdx].epipolarError ) );
                matchedTriDiffs.push_back( fabs( sdkFid.triErr - fr.ours.fiducials[bestIdx].triangulationError ) );
                ++pairMatchCount;

                if ( bestDist > posThreshold )
                {
                    frameAnomaly = true;
                    anomalyReasons << "Pos3D>" << posThreshold << "mm ";
                }
            }
        }

        // == Step 3: 工具识别对比 (重点监测) ==
        uint32_t sdkMkCount = static_cast<uint32_t>( fr.sdk.markers.size() );
        uint32_t ourMkCount = static_cast<uint32_t>( fr.ours.markers.size() );

        if ( sdkMkCount == ourMkCount )
            ++markerCountMatch;
        else
        {
            ++markerCountMismatch;
            frameAnomaly = true;
            anomalyReasons << "MkCount:sdk=" << sdkMkCount << "/ours=" << ourMkCount << " ";
        }

        // SDK 识别到但我们没有
        if ( sdkMkCount > 0 && ourMkCount == 0 )
        {
            ++sdkDetectedWeNot;
            frameAnomaly = true;
            anomalyReasons << "SDK_DETECTED_WE_NOT ";
        }
        // 我们识别到但 SDK 没有
        if ( ourMkCount > 0 && sdkMkCount == 0 )
        {
            ++weDetectedSdkNot;
            frameAnomaly = true;
            anomalyReasons << "WE_DETECTED_SDK_NOT ";
        }

        // 按 geometryId 匹配
        for ( uint32_t si = 0; si < sdkMkCount; ++si )
        {
            const auto& sm = fr.sdk.markers[si];
            bool found = false;
            for ( uint32_t ri = 0; ri < ourMkCount; ++ri )
            {
                const auto& rm = fr.ours.markers[ri];
                if ( sm.geometryId == rm.geometryId )
                {
                    found = true;
                    ++markerGeomIdMatch;

                    // 平移差异
                    double dx = sm.translationMM[0] - rm.translationMM[0];
                    double dy = sm.translationMM[1] - rm.translationMM[1];
                    double dz = sm.translationMM[2] - rm.translationMM[2];
                    double tDiff = sqrt( dx*dx + dy*dy + dz*dz );

                    // 旋转差异
                    double Rd[3][3] = {};
                    for ( int i = 0; i < 3; ++i )
                        for ( int j = 0; j < 3; ++j )
                            for ( int k = 0; k < 3; ++k )
                                Rd[i][j] += sm.rotation[i][k] * rm.rotation[j][k];
                    double trace = Rd[0][0] + Rd[1][1] + Rd[2][2];
                    double cosA = max( -1.0, min( 1.0, ( trace - 1.0 ) / 2.0 ) );
                    double rDiff = acos( cosA ) * 180.0 / M_PI;

                    double regDiff = fabs( sm.registrationErrorMM - rm.registrationErrorMM );

                    markerTransDiffs.push_back( tDiff );
                    markerRotDiffs.push_back( rDiff );
                    markerRegErrDiffs.push_back( regDiff );

                    if ( tDiff > markerTransThreshold || rDiff > markerRotThreshold )
                    {
                        frameAnomaly = true;
                        anomalyReasons << "MkPose:t=" << fixed << setprecision(2) << tDiff
                                       << "mm/r=" << rDiff << "deg ";
                    }
                    break;
                }
            }
            if ( !found )
            {
                ++markerGeomIdMismatch;
                frameAnomaly = true;
                anomalyReasons << "MkGeomMissing:" << sm.geometryId << " ";
            }
        }

        // ------------------------------------------------------------------
        // 记录异常状态
        if ( frameAnomaly )
        {
            fr.anomalous = true;
            fr.anomalyReason = anomalyReasons.str();
            anomalyFrameIndices.push_back( fi );
        }
    }

    // =======================================================================
    //  PHASE 3: 保存对比数据 + 异常帧图像
    //
    //  Phase 1 采集时已将图像暂存在 FrameRecord 中,
    //  Phase 2 标记了异常帧。此处统一输出 CSV 和异常帧图像。
    //  对于大帧数场景, 内存占用 = 帧数 x 2 x (width x height) 字节。
    // =======================================================================

    cout << "\n=== Phase 3: Saving anomaly data ===" << endl;
    cout << "Total anomalous frames: " << anomalyFrameIndices.size()
         << " / " << allFrames.size() << endl;

    // =====================================================================
    // 保存 CSV: 圆心检测对比
    // =====================================================================
    {
        string path = outputDir + "/centroid_comparison.csv";
        ofstream csv( path.c_str() );
        csv << fixed << setprecision( 6 );
        csv << "frame_idx,side,"
            << "sdk_det_count,our_blob_count,count_diff,"
            << "sdk_cx,sdk_cy,sdk_area,"
            << "our_cx,our_cy,our_area,"
            << "centroid_dist_px" << endl;

        for ( const auto& fr : allFrames )
        {
            // Left
            for ( size_t si = 0; si < fr.sdk.leftRaw.size(); ++si )
            {
                const auto& sd = fr.sdk.leftRaw[si];
                double bestDist = 1e9;
                size_t bestIdx = SIZE_MAX;
                for ( size_t oi = 0; oi < fr.ours.leftBlobs.size(); ++oi )
                {
                    const auto& ob = fr.ours.leftBlobs[oi];
                    if ( ob.area != sd.pixelCount ) continue;
                    double dx = ob.centerX - sd.cx;
                    double dy = ob.centerY - sd.cy;
                    double d = sqrt( dx*dx + dy*dy );
                    if ( d < bestDist ) { bestDist = d; bestIdx = oi; }
                }
                if ( bestIdx != SIZE_MAX && bestDist < 10.0 )
                {
                    const auto& ob = fr.ours.leftBlobs[bestIdx];
                    csv << fr.frameIdx << ",left,"
                        << fr.sdk.leftRaw.size() << "," << fr.ours.leftBlobs.size() << ","
                        << ( static_cast<int>(fr.ours.leftBlobs.size()) - static_cast<int>(fr.sdk.leftRaw.size()) ) << ","
                        << sd.cx << "," << sd.cy << "," << sd.pixelCount << ","
                        << ob.centerX << "," << ob.centerY << "," << ob.area << ","
                        << bestDist << endl;
                }
            }

            // Right
            for ( size_t si = 0; si < fr.sdk.rightRaw.size(); ++si )
            {
                const auto& sd = fr.sdk.rightRaw[si];
                double bestDist = 1e9;
                size_t bestIdx = SIZE_MAX;
                for ( size_t oi = 0; oi < fr.ours.rightBlobs.size(); ++oi )
                {
                    const auto& ob = fr.ours.rightBlobs[oi];
                    if ( ob.area != sd.pixelCount ) continue;
                    double dx = ob.centerX - sd.cx;
                    double dy = ob.centerY - sd.cy;
                    double d = sqrt( dx*dx + dy*dy );
                    if ( d < bestDist ) { bestDist = d; bestIdx = oi; }
                }
                if ( bestIdx != SIZE_MAX && bestDist < 10.0 )
                {
                    const auto& ob = fr.ours.rightBlobs[bestIdx];
                    csv << fr.frameIdx << ",right,"
                        << fr.sdk.rightRaw.size() << "," << fr.ours.rightBlobs.size() << ","
                        << ( static_cast<int>(fr.ours.rightBlobs.size()) - static_cast<int>(fr.sdk.rightRaw.size()) ) << ","
                        << sd.cx << "," << sd.cy << "," << sd.pixelCount << ","
                        << ob.centerX << "," << ob.centerY << "," << ob.area << ","
                        << bestDist << endl;
                }
            }
        }
        csv.close();
    }

    // =====================================================================
    // 保存 CSV: 极线匹配 + 三角化对比
    // =====================================================================
    {
        string path = outputDir + "/fiducial_comparison.csv";
        ofstream csv( path.c_str() );
        csv << fixed << setprecision( 6 );
        csv << "frame_idx,"
            << "sdk_fid_count,our_fid_count,count_diff,"
            << "sdk_px,sdk_py,sdk_pz,sdk_epiErr,sdk_triErr,sdk_prob,sdk_status,"
            << "our_px,our_py,our_pz,our_epiErr,our_triErr,our_prob,"
            << "pos_diff_mm,epi_diff_px,tri_diff_mm" << endl;

        for ( const auto& fr : allFrames )
        {
            vector<bool> used( fr.ours.fiducials.size(), false );
            for ( const auto& sf : fr.sdk.fiducials )
            {
                if ( isRejectedByWorkingVolume( sf.statusBits ) ) continue;

                double bestDist = 1e9;
                size_t bestIdx = SIZE_MAX;
                for ( size_t oi = 0; oi < fr.ours.fiducials.size(); ++oi )
                {
                    if ( used[oi] ) continue;
                    double dx = sf.px - fr.ours.fiducials[oi].posX;
                    double dy = sf.py - fr.ours.fiducials[oi].posY;
                    double dz = sf.pz - fr.ours.fiducials[oi].posZ;
                    double d = sqrt( dx*dx + dy*dy + dz*dz );
                    if ( d < bestDist ) { bestDist = d; bestIdx = oi; }
                }

                if ( bestIdx != SIZE_MAX && bestDist < 100.0 )
                {
                    used[bestIdx] = true;
                    const auto& of = fr.ours.fiducials[bestIdx];
                    csv << fr.frameIdx << ","
                        << fr.sdk.fiducials.size() << "," << fr.ours.fiducials.size() << ","
                        << ( static_cast<int>(fr.sdk.fiducials.size()) - static_cast<int>(fr.ours.fiducials.size()) ) << ","
                        << sf.px << "," << sf.py << "," << sf.pz << ","
                        << sf.epiErr << "," << sf.triErr << "," << sf.probability << "," << sf.statusBits << ","
                        << of.posX << "," << of.posY << "," << of.posZ << ","
                        << of.epipolarError << "," << of.triangulationError << "," << of.probability << ","
                        << bestDist << ","
                        << fabs( sf.epiErr - of.epipolarError ) << ","
                        << fabs( sf.triErr - of.triangulationError ) << endl;
                }
            }
        }
        csv.close();
    }

    // =====================================================================
    // 保存 CSV: 工具识别对比 (重点)
    // =====================================================================
    {
        string path = outputDir + "/marker_comparison.csv";
        ofstream csv( path.c_str() );
        csv << fixed << setprecision( 6 );
        csv << "frame_idx,anomaly,"
            << "sdk_marker_count,our_marker_count,"
            << "sdk_geomId,our_geomId,geomId_match,"
            << "sdk_tx,sdk_ty,sdk_tz,"
            << "our_tx,our_ty,our_tz,"
            << "trans_diff_mm,"
            << "sdk_r00,sdk_r01,sdk_r02,sdk_r10,sdk_r11,sdk_r12,sdk_r20,sdk_r21,sdk_r22,"
            << "our_r00,our_r01,our_r02,our_r10,our_r11,our_r12,our_r20,our_r21,our_r22,"
            << "rot_diff_deg,"
            << "sdk_regErr,our_regErr,regErr_diff_mm,"
            << "sdk_presenceMask,our_presenceMask,"
            << "anomaly_reason" << endl;

        for ( const auto& fr : allFrames )
        {
            uint32_t sdkMkCount = static_cast<uint32_t>( fr.sdk.markers.size() );
            uint32_t ourMkCount = static_cast<uint32_t>( fr.ours.markers.size() );

            if ( sdkMkCount == 0 && ourMkCount == 0 ) continue;

            // 对每个 SDK marker, 找匹配的 our marker
            for ( uint32_t si = 0; si < sdkMkCount; ++si )
            {
                const auto& sm = fr.sdk.markers[si];
                bool found = false;
                for ( uint32_t ri = 0; ri < ourMkCount; ++ri )
                {
                    const auto& rm = fr.ours.markers[ri];
                    if ( sm.geometryId == rm.geometryId )
                    {
                        found = true;
                        double dx = sm.translationMM[0] - rm.translationMM[0];
                        double dy = sm.translationMM[1] - rm.translationMM[1];
                        double dz = sm.translationMM[2] - rm.translationMM[2];
                        double tDiff = sqrt( dx*dx + dy*dy + dz*dz );

                        double Rd[3][3] = {};
                        for ( int a = 0; a < 3; ++a )
                            for ( int b = 0; b < 3; ++b )
                                for ( int c = 0; c < 3; ++c )
                                    Rd[a][b] += sm.rotation[a][c] * rm.rotation[b][c];
                        double trace = Rd[0][0] + Rd[1][1] + Rd[2][2];
                        double cosA = max( -1.0, min( 1.0, ( trace - 1.0 ) / 2.0 ) );
                        double rDiff = acos( cosA ) * 180.0 / M_PI;

                        double regDiff = fabs( sm.registrationErrorMM - rm.registrationErrorMM );

                        bool isAnomaly = ( tDiff > markerTransThreshold ||
                                           rDiff > markerRotThreshold );

                        csv << fr.frameIdx << "," << ( isAnomaly ? 1 : 0 ) << ","
                            << sdkMkCount << "," << ourMkCount << ","
                            << sm.geometryId << "," << rm.geometryId << ",1,"
                            << sm.translationMM[0] << "," << sm.translationMM[1] << "," << sm.translationMM[2] << ","
                            << rm.translationMM[0] << "," << rm.translationMM[1] << "," << rm.translationMM[2] << ","
                            << tDiff << ","
                            << sm.rotation[0][0] << "," << sm.rotation[0][1] << "," << sm.rotation[0][2] << ","
                            << sm.rotation[1][0] << "," << sm.rotation[1][1] << "," << sm.rotation[1][2] << ","
                            << sm.rotation[2][0] << "," << sm.rotation[2][1] << "," << sm.rotation[2][2] << ","
                            << rm.rotation[0][0] << "," << rm.rotation[0][1] << "," << rm.rotation[0][2] << ","
                            << rm.rotation[1][0] << "," << rm.rotation[1][1] << "," << rm.rotation[1][2] << ","
                            << rm.rotation[2][0] << "," << rm.rotation[2][1] << "," << rm.rotation[2][2] << ","
                            << rDiff << ","
                            << sm.registrationErrorMM << "," << rm.registrationErrorMM << "," << regDiff << ","
                            << sm.geometryPresenceMask << "," << rm.geometryPresenceMask << ","
                            << ( isAnomaly ? "POSE_DIFF" : "" ) << endl;
                        break;
                    }
                }
                if ( !found )
                {
                    // SDK 有此 marker, 我们没有
                    csv << fr.frameIdx << ",1,"
                        << sdkMkCount << "," << ourMkCount << ","
                        << sm.geometryId << ",-1,0,"
                        << sm.translationMM[0] << "," << sm.translationMM[1] << "," << sm.translationMM[2] << ","
                        << ",,,"
                        << "0,"
                        << sm.rotation[0][0] << "," << sm.rotation[0][1] << "," << sm.rotation[0][2] << ","
                        << sm.rotation[1][0] << "," << sm.rotation[1][1] << "," << sm.rotation[1][2] << ","
                        << sm.rotation[2][0] << "," << sm.rotation[2][1] << "," << sm.rotation[2][2] << ","
                        << ",,,,,,,,,"
                        << "0,"
                        << sm.registrationErrorMM << ",,0,"
                        << sm.geometryPresenceMask << ",,"
                        << "SDK_ONLY" << endl;
                }
            }

            // 我们有但 SDK 没有的 marker
            for ( uint32_t ri = 0; ri < ourMkCount; ++ri )
            {
                const auto& rm = fr.ours.markers[ri];
                bool foundInSdk = false;
                for ( uint32_t si = 0; si < sdkMkCount; ++si )
                {
                    if ( fr.sdk.markers[si].geometryId == rm.geometryId )
                    {
                        foundInSdk = true;
                        break;
                    }
                }
                if ( !foundInSdk )
                {
                    csv << fr.frameIdx << ",1,"
                        << sdkMkCount << "," << ourMkCount << ","
                        << "-1," << rm.geometryId << ",0,"
                        << ",,,"
                        << rm.translationMM[0] << "," << rm.translationMM[1] << "," << rm.translationMM[2] << ","
                        << "0,"
                        << ",,,,,,,,,"
                        << rm.rotation[0][0] << "," << rm.rotation[0][1] << "," << rm.rotation[0][2] << ","
                        << rm.rotation[1][0] << "," << rm.rotation[1][1] << "," << rm.rotation[1][2] << ","
                        << rm.rotation[2][0] << "," << rm.rotation[2][1] << "," << rm.rotation[2][2] << ","
                        << "0,"
                        << "," << rm.registrationErrorMM << ",0,"
                        << "," << rm.geometryPresenceMask << ","
                        << "OUR_ONLY" << endl;
                }
            }
        }
        csv.close();
    }

    // =====================================================================
    // 保存 CSV: 帧级汇总
    // =====================================================================
    {
        string path = outputDir + "/frame_summary.csv";
        ofstream csv( path.c_str() );
        csv << fixed << setprecision( 6 );
        csv << "frame_idx,"
            << "sdk_left_det,our_left_blob,sdk_right_det,our_right_blob,"
            << "sdk_fid_count,our_fid_count,fid_diff,"
            << "sdk_marker_count,our_marker_count,"
            << "detect_us,matchTri_us,marker_us,total_pipeline_us,"
            << "anomaly,anomaly_reason" << endl;

        for ( const auto& fr : allFrames )
        {
            csv << fr.frameIdx << ","
                << fr.sdk.leftRaw.size() << "," << fr.ours.leftBlobs.size() << ","
                << fr.sdk.rightRaw.size() << "," << fr.ours.rightBlobs.size() << ","
                << fr.sdk.fiducials.size() << "," << fr.ours.fiducials.size() << ","
                << ( static_cast<int>(fr.sdk.fiducials.size()) - static_cast<int>(fr.ours.fiducials.size()) ) << ","
                << fr.sdk.markers.size() << "," << fr.ours.markers.size() << ","
                << ( fr.ours.detectLeftUs + fr.ours.detectRightUs ) << ","
                << fr.ours.matchTriUs << ","
                << fr.ours.markerUs << ","
                << fr.ours.totalUs << ","
                << ( fr.anomalous ? 1 : 0 ) << ","
                << "\"" << fr.anomalyReason << "\"" << endl;
        }
        csv.close();
    }

    // =====================================================================
    // 保存 CSV: 计时详情
    // =====================================================================
    {
        string path = outputDir + "/timing_summary.csv";
        ofstream csv( path.c_str() );
        csv << fixed << setprecision( 3 );
        csv << "frame_idx,"
            << "detect_left_us,detect_right_us,detect_total_us,"
            << "matchTri_us,marker_us,total_pipeline_us" << endl;

        for ( const auto& fr : allFrames )
        {
            csv << fr.frameIdx << ","
                << fr.ours.detectLeftUs << ","
                << fr.ours.detectRightUs << ","
                << ( fr.ours.detectLeftUs + fr.ours.detectRightUs ) << ","
                << fr.ours.matchTriUs << ","
                << fr.ours.markerUs << ","
                << fr.ours.totalUs << endl;
        }
        csv.close();
    }

    // =====================================================================
    // 保存异常帧图像
    // Phase 1 已将图像暂存在 FrameRecord 中,
    // 此处仅将异常帧的图像保存到 anomaly_images 目录 (最多 100 帧)。
    // =====================================================================

    uint32_t savedImageCount = 0;
    for ( size_t ai = 0; ai < anomalyFrameIndices.size() && savedImageCount < 100; ++ai )
    {
        size_t fi = anomalyFrameIndices[ai];
        const auto& fr = allFrames[fi];

        if ( !fr.leftImage.empty() && !fr.rightImage.empty() )
        {
            ostringstream leftName, rightName;
            leftName << anomalyImagesDir << "/frame_" << setfill('0') << setw(4) << fr.frameIdx << "_left.pgm";
            rightName << anomalyImagesDir << "/frame_" << setfill('0') << setw(4) << fr.frameIdx << "_right.pgm";

            savePGMBinary( leftName.str(), fr.imageWidth, fr.imageHeight, fr.leftImage.data() );
            savePGMBinary( rightName.str(), fr.imageWidth, fr.imageHeight, fr.rightImage.data() );
            ++savedImageCount;
        }
    }
    cout << "Anomaly images saved: " << savedImageCount << endl;

    // =======================================================================
    //  PHASE 4: 生成分析报告
    // =======================================================================

    string reportPath = outputDir + "/analysis_report.txt";
    ofstream report( reportPath.c_str() );

    auto writeReport = [&]( ostream& out )
    {
        out << "============================================================" << endl;
        out << " Full Pipeline Comparison Report" << endl;
        out << " Reverse-Engineered Algorithm vs SDK" << endl;
        out << "============================================================" << endl;
        out << endl;

        time_t now = time( nullptr );
        char timeBuf[64];
        strftime( timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", localtime(&now) );
        out << "Report generated: " << timeBuf << endl;
        out << "Library version:  " << stereo_algo::StereoVision::version() << endl;
        out << "Total frames:     " << allFrames.size() << endl;
        out << "Anomalous frames: " << anomalyFrameIndices.size()
            << " (" << fixed << setprecision(1)
            << ( allFrames.empty() ? 0.0
                 : 100.0 * anomalyFrameIndices.size() / allFrames.size() )
            << "%)" << endl;
        out << endl;

        out << "Thresholds:" << endl;
        out << "  Centroid:      " << centroidThreshold << " px" << endl;
        out << "  3D Position:   " << posThreshold << " mm" << endl;
        out << "  Epipolar:      " << epiThreshold << " px" << endl;
        out << "  Marker trans:  " << markerTransThreshold << " mm" << endl;
        out << "  Marker rot:    " << markerRotThreshold << " deg" << endl;
        out << "  KeepRejected:  ON (rejected points excluded from comparison)" << endl;
        out << endl;

        // --- Step 1: 圆心检测 ---
        out << "=== Step 1: Circle Centroid Detection (Image -> Blobs) ===" << endl;
        {
            auto ls = computeStats( blobCountDiffLeft );
            auto rs = computeStats( blobCountDiffRight );
            out << "  Blob count diff (ours - SDK):" << endl;
            out << "    Left:  mean=" << setprecision(2) << ls.mean
                << " max=" << ls.maxVal << " std=" << ls.stddev << endl;
            out << "    Right: mean=" << rs.mean
                << " max=" << rs.maxVal << " std=" << rs.stddev << endl;

            auto cs = computeStats( centroidMatchDists );
            out << "  Centroid match (by area):" << endl;
            out << "    Matched:    " << centroidMatchCount << endl;
            out << "    Unmatched:  " << centroidMissCount << endl;
            out << "    Dist mean:  " << setprecision(6) << cs.mean << " px" << endl;
            out << "    Dist max:   " << cs.maxVal << " px" << endl;
            out << "    Dist std:   " << cs.stddev << " px" << endl;

            if ( centroidMatchDists.size() > 0 )
            {
                size_t under001 = 0;
                for ( auto d : centroidMatchDists ) if ( d < 0.01 ) ++under001;
                out << "    <0.01px rate: " << setprecision(1)
                    << 100.0 * under001 / centroidMatchDists.size() << "%" << endl;
            }
        }
        out << endl;

        // --- Step 2: 极线匹配 + 三角化 ---
        out << "=== Step 2: Epipolar Matching + Triangulation ===" << endl;
        {
            auto fd = computeStats( fidCountDiffs );
            out << "  Fiducial count diff (SDK - ours, per frame):" << endl;
            out << "    Mean: " << setprecision(2) << fd.mean << endl;
            out << "    Max:  " << fd.maxVal << endl;
            uint32_t exactMatch = 0;
            for ( auto d : fidCountDiffs ) if ( fabs(d) < 0.5 ) ++exactMatch;
            out << "    Exact match: " << exactMatch << " / " << fidCountDiffs.size() << " frames" << endl;

            auto ps = computeStats( matchedPosDiffs );
            auto es = computeStats( matchedEpiDiffs );
            auto ts = computeStats( matchedTriDiffs );
            out << "  Matched fiducials: " << pairMatchCount << endl;
            out << "  3D pos diff:  mean=" << setprecision(6) << ps.mean
                << " max=" << ps.maxVal << " std=" << ps.stddev << " mm" << endl;
            out << "  Epi err diff: mean=" << es.mean
                << " max=" << es.maxVal << " std=" << es.stddev << " px" << endl;
            out << "  Tri err diff: mean=" << ts.mean
                << " max=" << ts.maxVal << " std=" << ts.stddev << " mm" << endl;
        }
        out << endl;

        // --- Step 3: 工具识别 (重点) ---
        out << "=== Step 3: Tool Recognition (KEY MONITORING) ===" << endl;
        {
            out << "  Marker count match (frames):    " << markerCountMatch << endl;
            out << "  Marker count mismatch (frames):  " << markerCountMismatch << endl;
            out << "  GeomID match (markers):          " << markerGeomIdMatch << endl;
            out << "  GeomID mismatch (markers):       " << markerGeomIdMismatch << endl;
            out << endl;
            out << "  *** SDK detected, we did NOT:    " << sdkDetectedWeNot << " frames ***" << endl;
            out << "  *** We detected, SDK did NOT:    " << weDetectedSdkNot << " frames ***" << endl;
            out << endl;

            if ( !markerTransDiffs.empty() )
            {
                auto mt = computeStats( markerTransDiffs );
                auto mr = computeStats( markerRotDiffs );
                auto me = computeStats( markerRegErrDiffs );
                out << "  Translation diff: mean=" << setprecision(4) << mt.mean
                    << " max=" << mt.maxVal << " std=" << mt.stddev << " mm" << endl;
                out << "  Rotation diff:    mean=" << mr.mean
                    << " max=" << mr.maxVal << " std=" << mr.stddev << " deg" << endl;
                out << "  RegError diff:    mean=" << setprecision(6) << me.mean
                    << " max=" << me.maxVal << " std=" << me.stddev << " mm" << endl;
                out << "  Matched markers:  " << markerTransDiffs.size() << endl;
            }
            else
            {
                out << "  No matching markers found for comparison." << endl;
            }
        }
        out << endl;

        // --- 时间统计 ---
        out << "=== Timing Statistics ===" << endl;
        {
            auto ptotal = computeStats( pipelineTotalUs );
            auto pdetect = computeStats( detectTotalUs );
            auto pmatch = computeStats( matchTriTotalUs );
            auto pmarker = computeStats( markerTotalUs );

            out << "  Full pipeline (per frame):" << endl;
            out << "    Total:     mean=" << setprecision(1) << ptotal.mean
                << " max=" << ptotal.maxVal << " us"
                << " (" << setprecision(2) << ptotal.mean / 1000.0 << " ms)" << endl;

            out << "  Breakdown:" << endl;
            out << "    Detect:    mean=" << setprecision(1) << pdetect.mean
                << " max=" << pdetect.maxVal << " us" << endl;
            out << "    Match+Tri: mean=" << pmatch.mean
                << " max=" << pmatch.maxVal << " us" << endl;
            out << "    Markers:   mean=" << pmarker.mean
                << " max=" << pmarker.maxVal << " us" << endl;

            out << "  Total time (all frames):" << endl;
            double totalAll = accumulate( pipelineTotalUs.begin(), pipelineTotalUs.end(), 0.0 );
            double totalDet = accumulate( detectTotalUs.begin(), detectTotalUs.end(), 0.0 );
            double totalMT  = accumulate( matchTriTotalUs.begin(), matchTriTotalUs.end(), 0.0 );
            double totalMk  = accumulate( markerTotalUs.begin(), markerTotalUs.end(), 0.0 );
            out << "    Pipeline: " << setprecision(0) << totalAll / 1000.0 << " ms" << endl;
            out << "    Detect:   " << totalDet / 1000.0 << " ms" << endl;
            out << "    Match:    " << totalMT / 1000.0 << " ms" << endl;
            out << "    Markers:  " << totalMk / 1000.0 << " ms" << endl;

            if ( !pipelineTotalUs.empty() )
            {
                double fps = 1e6 / ptotal.mean;
                out << "  Estimated throughput: " << setprecision(1) << fps << " fps" << endl;
            }
        }
        out << endl;

        // --- 结论 ---
        out << "=== Conclusion ===" << endl;
        {
            auto ps = computeStats( matchedPosDiffs );
            auto cs = computeStats( centroidMatchDists );

            bool centroidOk = cs.mean < centroidThreshold;
            bool posOk = ps.mean < posThreshold;
            bool markerOk = ( sdkDetectedWeNot == 0 );

            if ( centroidOk && posOk && markerOk )
            {
                out << "  PASS: Full pipeline matches SDK output within thresholds." << endl;
            }
            else
            {
                out << "  ATTENTION: Differences detected. Review anomaly data." << endl;
            }

            if ( !centroidOk )
                out << "  WARNING: Centroid detection mean diff (" << setprecision(4) << cs.mean
                    << " px) exceeds threshold (" << centroidThreshold << " px)." << endl;

            if ( !posOk )
                out << "  WARNING: 3D position mean diff (" << setprecision(4) << ps.mean
                    << " mm) exceeds threshold (" << posThreshold << " mm)." << endl;

            if ( sdkDetectedWeNot > 0 )
                out << "  *** CRITICAL: " << sdkDetectedWeNot
                    << " frame(s) where SDK detected tool but our pipeline did NOT. ***" << endl;

            if ( weDetectedSdkNot > 0 )
                out << "  NOTE: " << weDetectedSdkNot
                    << " frame(s) where our pipeline detected tool but SDK did NOT." << endl;

            if ( markerGeomIdMismatch > 0 )
                out << "  WARNING: " << markerGeomIdMismatch
                    << " marker(s) with mismatched geometry ID." << endl;

            out << endl;
            out << "  Output files:" << endl;
            out << "    centroid_comparison.csv   - Centroid detection comparison" << endl;
            out << "    fiducial_comparison.csv   - Fiducial matching comparison" << endl;
            out << "    marker_comparison.csv     - Marker identification comparison (KEY)" << endl;
            out << "    frame_summary.csv         - Per-frame summary" << endl;
            out << "    timing_summary.csv        - Per-frame timing breakdown" << endl;
            out << "    anomaly_images/           - Anomalous frame images (PGM)" << endl;
            out << "    analysis_report.txt       - This report" << endl;
        }
        out << endl;
        out << "============================================================" << endl;
    };

    // Write to file and console
    writeReport( report );
    report.close();
    cout << endl;
    writeReport( cout );

    // -----------------------------------------------------------------------
    // Summary output

    cout << endl;
    if ( !allFrames.empty() )
    {
        cout << "Output saved to: " << outputDir << "/" << endl;
    }
    else
    {
        cout << "WARNING: No frames captured. Ensure a marker is visible and images are enabled." << endl;
    }

    // -----------------------------------------------------------------------
    // Close driver

    ftkDeleteFrame( frame );

    if ( ftkError::FTK_OK != ftkClose( &lib ) )
        checkError( lib, !isNotFromConsole );

#ifdef ATR_WIN
    if ( isNotFromConsole )
    {
        cout << endl << "*** Hit a key to exit ***" << endl;
        waitForKeyboardHit();
    }
#endif

    return EXIT_SUCCESS;
}
