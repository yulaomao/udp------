// ===========================================================================
// 逆向工程还原 — ftkDevice.cpp (SDK初始化与设备管理层)
// 来源: fusionTrack64.dll
// 编译路径:
//   G:\workspace\sdk_win_build\soft.atr.meta.2cams.sdk\
//     soft.atr.2cams.sdk\dev\include\ftkDevice.cpp
//
// 这是 ftkInit / ftkInitExt 的实现入口
//
// DLL字符串证据:
//   "ERROR: cannot allocate library object"
//   "cannot parse JSON input"
//   "cannot find attribute \"environment\" in configuration file"
//   "cannot find attribute \"environment\":version in configuration file"
//   "cannot set data directory '"
//   "Error cannot cast pointer to ftkDevice"
//   "cannot enumerate devices"
//   "cannot check options"
//   "cannot find option '%s'"
//   "cannot get analyser for device type %i"
//   "cannot finalise the device instance"
//   "cannot disable data sending for device %llx"
//   "cannot allocate internal data"
//   "cannot cast internal data pointer"
//   "cannot allocate Wireless manager."
//   "cannot initialise Wireless manager."
//
// 导出函数:
//   ftkInit        → ftkLibrary ftkInit()
//   ftkInitExt     → ftkLibrary ftkInitExt(const char* cfgPath, ftkBuffer* errors)
//   ftkClose       → ftkError ftkClose(ftkLibrary* lib)
//   ftkEnumerateDevices → ftkError ftkEnumerateDevices(ftkLibrary, ftkDeviceEnumCallback, void*)
//   ftkGetLastError     → ftkError ftkGetLastError(ftkLibrary, ftkBuffer*)
//   ftkGetLastWarning   → ftkError ftkGetLastWarning(ftkLibrary, ftkBuffer*)
//   ftkGetSdkVersion    → ftkError ftkGetSdkVersion(ftkVersionInfo*)
//   ftkVersion          → (deprecated)
//
// 初始化流程（推断）:
//   1. 分配库对象（ftkLibrary）
//   2. 如果提供了配置文件，解析 JSON 配置
//   3. 初始化 device64.dll（加载 devInit）
//   4. 枚举 UDP 设备
//   5. 对每个设备:
//      a. 读取标定文件（calibration file）
//      b. 初始化 StereoProvider（根据标定版本选择V0-V3）
//      c. 初始化 StereoInterpolator
//      d. 初始化 Segmenter
//      e. 初始化 MatchMarkers
//      f. 启动 Heartbeat
//      g. 启动 ImageProcessor
// ===========================================================================

#pragma once

#include "StereoCameraSystem.h"
#include "calibHandling/StereoProviderV3.h"
#include "segmenter/SegmenterV21.h"
#include "compressor/PictureCompressor.h"
#include "markerReco/MatchMarkers.h"
#include "device/UdpTransport.h"
#include "feature/FeatureHandlingV1.h"

#include <ftkInterface.h>
#include <ftkOptions.h>

#include <map>
#include <vector>
#include <memory>
#include <string>
#include <mutex>

namespace measurement {

/// ftkDevice — 单个追踪设备的完整状态
///
/// RTTI: (匿名内部类，但从字符串可推断)
/// 编译路径: ftkDevice.cpp
///
/// 每个连接的 fusionTrack 设备对应一个 ftkDevice 实例
/// 包含了该设备的所有处理组件
struct ftkDevice
{
    uint64_t serialNumber;
    ftkDeviceType deviceType;

    /// 标定数据
    std::unique_ptr<calibHandling::StereoProviderV3> stereoProvider;
    std::unique_ptr<calibHandling::StereoInterpolatorV1> stereoInterpolator;

    /// 双目系统（根据温度更新）
    StereoCameraSystem stereoCamera;

    /// 图像处理
    compressor::PictureCompressor compressor;
    std::unique_ptr<segmenter::Segmenter8> segmenterLeft;
    std::unique_ptr<segmenter::Segmenter8> segmenterRight;

    /// 工具匹配
    markerReco::MatchMarkers matchMarkers;

    /// 特征处理
    std::unique_ptr<feature::FeatureHandlingV1> featureHandler;

    /// 通讯层
    UdpCom udpCom;
    Heartbeat heartbeat;
    ImageProcessor imageProcessor;

    /// 选项存储
    std::map<uint32_t, int32_t> intOptions;
    std::map<uint32_t, float> floatOptions;

    /// 最近一帧数据缓存
    struct FrameCache
    {
        std::vector<ftkRawData> leftRawData;
        std::vector<ftkRawData> rightRawData;
        std::vector<ftk3DFiducial> fiducials;
        std::vector<ftkMarker> markers;
        uint64_t timestamp;
        uint32_t counter;
    } frameCache;

    /// 当前温度
    float currentTemperature;
};

/// ftkLibraryData — 库全局状态
///
/// ftkLibrary 实际上是指向此结构体的不透明指针
///
/// DLL字符串: "ERROR: cannot allocate library object"
struct ftkLibraryData
{
    /// 已连接的设备
    std::map<uint64_t, std::unique_ptr<ftkDevice>> devices;

    /// 全局选项
    std::map<uint32_t, int32_t> globalIntOptions;

    /// 错误信息缓冲
    std::string lastError;
    std::string lastWarning;

    /// 线程保护
    mutable std::mutex mutex;

    /// 数据目录（标定文件等）
    std::string dataDirectory;

    /// 配置
    std::string configFilePath;

    /// SDK版本信息
    ftkVersionInfo sdkVersion;
};

// ===========================================================================
// ftkInit / ftkInitExt 实现
// ===========================================================================

/// ftkInit — 初始化 SDK（无配置文件）
///
/// 导出: ATR_EXPORT ftkLibrary ftkInit()
/// 实际调用 ftkInitExt(nullptr, nullptr)
inline ftkLibrary ftkInitImpl()
{
    return ftkInitExtImpl(nullptr, nullptr);
}

/// ftkInitExt — 初始化 SDK（使用配置文件）
///
/// 导出: ATR_EXPORT ftkLibrary ftkInitExt(const char* cfgPath, ftkBuffer* errors)
///
/// DLL字符串:
///   "ERROR: cannot allocate library object"
///   "cannot parse JSON input"
///   "cannot find attribute \"environment\" in configuration file"
ftkLibrary ftkInitExtImpl(const char* cfgPath, ftkBuffer* errors)
{
    // 步骤 1: 分配库对象
    ftkLibraryData* lib = new (std::nothrow) ftkLibraryData();
    if (!lib)
    {
        if (errors)
        {
            const char* msg = "ERROR: cannot allocate library object";
            std::strncpy(errors->data, msg, BUFFER_MAX_SIZE - 1);
            errors->size = static_cast<uint32_t>(std::strlen(msg));
        }
        return nullptr;
    }

    // 步骤 2: 解析配置文件
    if (cfgPath && cfgPath[0] != '\0')
    {
        lib->configFilePath = cfgPath;

        // 读取 JSON 配置文件
        // 从 DLL 字符串推断的配置项:
        //   "environment": { "version": ..., "dataDirectory": ... }
        //   设备地址、端口等网络配置
    }

    // 步骤 3: 初始化 device64.dll 通讯层
    // 调用 devInit()

    // 步骤 4: 设置 SDK 版本
    lib->sdkVersion.Major = 4;
    lib->sdkVersion.Minor = 7;
    lib->sdkVersion.Revision = 2;

    return reinterpret_cast<ftkLibrary>(lib);
}

/// ftkClose — 关闭 SDK
///
/// 导出: ATR_EXPORT ftkError ftkClose(ftkLibrary* lib)
///
/// 清理流程:
/// 1. 停止所有设备的 heartbeat
/// 2. 停止所有设备的 imageProcessor
/// 3. 断开 UDP 连接
/// 4. 释放所有内存
ftkError ftkCloseImpl(ftkLibrary* lib)
{
    if (!lib || !*lib)
        return ftkError::FTK_ERR_INV_PTR;

    ftkLibraryData* libData = reinterpret_cast<ftkLibraryData*>(*lib);

    // 清理所有设备
    for (auto& pair : libData->devices)
    {
        auto& dev = pair.second;

        dev->heartbeat.stop();
        dev->imageProcessor.stop();
        dev->udpCom.disconnect();
    }

    libData->devices.clear();

    delete libData;
    *lib = nullptr;

    return ftkError::FTK_OK;
}

/// ftkEnumerateDevices — 枚举已连接的设备
///
/// 导出: ATR_EXPORT ftkError ftkEnumerateDevices(ftkLibrary, ftkDeviceEnumCallback, void*)
///
/// DLL字符串: "cannot enumerate devices"
ftkError ftkEnumerateDevicesImpl(ftkLibrary lib,
                                 ftkDeviceEnumCallback cb,
                                 void* user)
{
    if (!lib)
        return ftkError::FTK_ERR_INV_PTR;

    ftkLibraryData* libData = reinterpret_cast<ftkLibraryData*>(lib);
    std::lock_guard<std::mutex> lock(libData->mutex);

    // 调用 devEnumerateDevices() 从 device64.dll
    // 对每个发现的设备，初始化 ftkDevice 并回调

    for (const auto& pair : libData->devices)
    {
        if (cb)
            cb(pair.first, user, pair.second->deviceType);
    }

    return ftkError::FTK_OK;
}

}  // namespace measurement
