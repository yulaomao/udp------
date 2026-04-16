// ============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo2_AcquisitionBasic.cpp
 *   \brief Demonstrate basic acquisition
 *
 *   NOTE  THAT THIS SAMPLE WILL RETURN NO OUTPUT IF YOU DO NOT
 *   PLACE A GEOMETRY IN FRONT OF THE LOCALIZER!
 *
 *   This sample aims to present the following driver features:
 *   - Open/close the driver
 *   - Enumerate devices
 *   - Load a geometry
 *   - Acquire pose (translation + rotation) data of loaded geometries
 *
 *   How to compile this example:
 *   - Follow instructions in README.txt
 *
 *   How to run this example:
 *   - Install the fusionTrack/spryTrack driver (see documentation)
 *   - Switch on device
 *   - Run the resulting executable
 *   - Expose a marker (with a geometry) in front of the localizer
 *
 */
// ============================================================================

#include "geometryHelper.hpp"
#include "helpers.hpp"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>

// 添加头文件
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <sstream>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

#pragma execution_character_set("utf-8")

using namespace std;

// 全局变量用于控制相机
std::atomic<bool> g_running(true);
std::mutex g_frameMutex;
std::condition_variable g_frameCV;
bool g_newFrameReady = true;
ftkLibrary lib = 0;
uint64 sn(0uLL);

// 用户命令处理函数
void processUserCommands(ftkLibrary lib, uint64 sn, const std::map<std::string, uint32>& options)
{
    std::string command;

    cout << "\n可用命令:\n";
    cout << "quit - 退出程序\n";
    cout << "pause - 暂停采集\n";
    cout << "resume - 恢复采集\n";
    cout << "option - 显示可用选项\n";
    cout << "set <选项名> <值> - 设置选项值\n";

    while (g_running)
    {
        cout << "\n输入命令: ";
        std::getline(std::cin, command);

        if (command == "quit")
        {
            g_running = false;
            g_frameCV.notify_all();
        }
        else if (command == "pause")
        {
            std::lock_guard<std::mutex> lock(g_frameMutex);
            g_newFrameReady = false;
            cout << "采集已暂停\n";
        }
        else if (command == "resume")
        {
            std::lock_guard<std::mutex> lock(g_frameMutex);
            g_newFrameReady = true;
            g_frameCV.notify_all();
            cout << "采集已恢复\n";
        }
        else if (command == "option")
        {
            cout << "可用选项:\n";
            for (const auto& opt : options)
            {
                cout << opt.first << " (ID: " << opt.second << ")\n";
            }
        }
        else if (command.substr(0, 3) == "set")
        {
            std::istringstream iss(command);
            std::string cmd, optName, valueStr;
            iss >> cmd >> optName >> valueStr;

            if (options.find(optName) != options.end())
            {
                uint32 optId = options.at(optName);

                // 修复嵌套的try-catch结构
                bool valueSet = false;

                // 尝试设置整数值
                try {
                    int32 value = std::stoi(valueStr);
                    ftkError err = ftkSetInt32(lib, sn, optId, value);
                    if (err == ftkError::FTK_OK)
                    {
                        cout << "已设置 " << optName << " = " << value << "\n";
                        valueSet = true;
                    }
                    else
                    {
                        cout << "设置选项失败，错误码: " << static_cast<int>(err) << "\n";
                    }
                }
                catch (std::exception&) {
                    // 如果不是整数，尝试设置浮点值
                    if (!valueSet) {
                        try {
                            float value = std::stof(valueStr);
                            ftkError err = ftkSetFloat32(lib, sn, optId, value);
                            if (err == ftkError::FTK_OK)
                            {
                                cout << "已设置 " << optName << " = " << value << "\n";
                                valueSet = true;
                            }
                            else
                            {
                                cout << "设置选项失败，错误码: " << static_cast<int>(err) << "\n";
                            }
                        }
                        catch (std::exception&) {
                            cout << "无效的值: " << valueStr << "\n";
                        }
                    }
                }
            }
            else
            {
                cout << "未知选项: " << optName << "\n";
            }
        }
        else
        {
            cout << "未知命令: " << command << "\n";
        }
    }
}

// 帧处理线程
void frameProcessingThread(ftkLibrary lib, uint64 sn)
{
    ftkFrameQuery* frame = ftkCreateFrame();
    if (frame == 0)
    {
        error("Cannot create frame instance", false);
        return;
    }
    
    ftkError err = ftkSetFrameOptions(false, false, 16u, 16u, 0u, 16u, frame);
    if (err != ftkError::FTK_OK)
    {
        ftkDeleteFrame(frame);
        checkError(lib, false);
        return;
    }
    
    while (g_running)
    {
        {
            std::unique_lock<std::mutex> lock(g_frameMutex);
            g_frameCV.wait(lock, [](){ return g_newFrameReady || !g_running; });
            
            if (!g_running)
                break;
        }
        
        err = ftkGetLastFrame(lib, sn, frame, 100u);
        if (err > ftkError::FTK_OK)
        {
            cout << ".";
            continue;
        }
        else if (err == ftkError::FTK_WAR_TEMP_INVALID)
        {
            cout << "temperature warning" << endl;
        }
        else if (err < ftkError::FTK_OK)
        {
            cout << "warning: " << int32(err) << endl;
            if (err == ftkError::FTK_WAR_NO_FRAME)
            {
                continue;
            }
        }
        
        switch (frame->markersStat)
        {
        case ftkQueryStatus::QS_WAR_SKIPPED:
            cerr << "marker fields in the frame are not set correctly" << endl;
            continue;
            
        case ftkQueryStatus::QS_ERR_INVALID_RESERVED_SIZE:
            cerr << "frame -> markersVersionSize is invalid" << endl;
            continue;
            
        case ftkQueryStatus::QS_OK:
            break;
            
        default:
            cerr << "invalid status" << endl;
            continue;
        }
        
        if (frame->markersCount == 0u)
        {
            cout << ".";
            sleep(1000L);
            continue;
        }
        
        if (frame->markersStat == ftkQueryStatus::QS_ERR_OVERFLOW)
        {
            cerr << "WARNING: marker overflow. Please increase cstMarkersCount" << endl;
        }
        
        {
            std::lock_guard<std::mutex> lock(g_frameMutex);
            
            for (uint32 i = 0u; i < frame->markersCount; ++i)
            {
                cout << endl;
                cout.precision(2u);
                /*cout << "geometry " << frame->markers[i].geometryId << ", trans ("
                     << frame->markers[i].translationMM[0] << " " << frame->markers[i].translationMM[1]
                     << " " << frame->markers[i].translationMM[2] << "), error ";*/
                printf("%.3f,%.3f,%.3f\n",
                    frame->markers[i].translationMM[0],
                    frame->markers[i].translationMM[1],
                    frame->markers[i].translationMM[2]);
                printf("%.3f,%.3f,%.3f\n",
                    frame->markers[i].rotation[0][0],
                    frame->markers[i].rotation[0][1],
                    frame->markers[i].rotation[0][2]);
                printf("%.3f,%.3f,%.3f\n",
                    frame->markers[i].rotation[1][0],
                    frame->markers[i].rotation[1][1],
                    frame->markers[i].rotation[1][2]);
                printf("%.3f,%.3f,%.3f\n",
                    frame->markers[i].rotation[2][0],
                    frame->markers[i].rotation[2][1],
                    frame->markers[i].rotation[2][2]);
                cout.precision(3u);
                //cout << frame->markers[i].registrationErrorMM << endl;
            }
        }
        
        sleep(100L); // 减少帧率，使输出更易读
    }
    
    ftkDeleteFrame(frame);
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

    cout << "This is a demonstration on how to load a geometry from a file and how to get the marker's "
            "position and orientation."
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

    string cfgFile( "D:/test.json" );

    deque< string > geomFiles{ "D:/geometry072.ini" };

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
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

    // ------------------------------------------------------------------------
    // Initialize driver

    ftkBuffer buffer;

    lib = ftkInitExt( cfgFile.empty() ? nullptr : cfgFile.c_str(), &buffer );
    if ( lib == nullptr )
    {
        cerr << buffer.data << endl;
        error( "Cannot initialize driver", !isNotFromConsole );
    }

    // ------------------------------------------------------------------------
    // Retrieve the device

    DeviceData device( retrieveLastDevice( lib, true, false, !isNotFromConsole ) );
    sn = device.SerialNumber;

    map< string, uint32 > options{};

    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &options ) );
    if ( options.empty() )
    {
        error( "Cannot retrieve any options.", !isNotFromConsole );
    }

    // ------------------------------------------------------------------------
    // When using a spryTrack, onboard processing of the images is preferred.
    // Sending of the images is disabled so that the sample operates on a USB2
    // connection
    if ( ftkDeviceType::DEV_SPRYTRACK_180 == device.Type || ftkDeviceType::DEV_SPRYTRACK_300 == device.Type )
    {
        bool neededOptionsPresent( true );
        string errMsg( "Could not find needed option(s):" );
        for ( const string& item : { "Enable embedded processing", "Enable images sending" } )
        {
            if ( options.find( item ) == options.cend() )
            {
                errMsg += " '" + item + "'";
                neededOptionsPresent = false;
            }
        }
        if ( !neededOptionsPresent )
        {
            error( errMsg.c_str(), !isNotFromConsole );
        }
        cout << "Enable onboard processing" << endl;
        if ( ftkSetInt32( lib, sn, options[ "Enable embedded processing" ], 1 ) != ftkError::FTK_OK )
        {
            cerr << "Cannot process data directly on the SpryTrack." << endl;
            checkError( lib, !isNotFromConsole );
        }

        cout << "Disable images sending" << endl;
        if ( ftkSetInt32( lib, sn, options[ "Enable images sending" ], 0 ) != ftkError::FTK_OK )
        {
            cerr << "Cannot disable images sending on the SpryTrack." << endl;
            checkError( lib, !isNotFromConsole );
        }
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

    // 在初始化设备后，启动控制线程
    g_newFrameReady = true;
    
    // 启动帧处理线程
    std::thread frameThread(frameProcessingThread, lib, sn);
    
    // 启动用户命令处理
    cout << "\n相机已初始化完成，现在可以随时控制相机。\n";
    processUserCommands(lib, sn, options);
    
    // 等待帧处理线程结束
    if (frameThread.joinable())
        frameThread.join();

    // ------------------------------------------------------------------------
    // Close driver

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