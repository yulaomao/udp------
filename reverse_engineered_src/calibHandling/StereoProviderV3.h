// ===========================================================================
// 逆向工程还原 — StereoProvider 标定数据管理
// 来源: fusionTrack64.dll
//
// RTTI类层次:
//   StereoProviderBase@calibHandling@measurement (抽象基类)
//   ├── StereoProviderV0@calibHandling@measurement
//   ├── StereoProviderV1@calibHandling@measurement
//   ├── StereoProviderV2@calibHandling@measurement
//   └── StereoProviderV3@calibHandling@measurement  ← 当前主要版本
//
//   StereoInterpolatorBase@calibHandling@measurement (温度插值)
//   └── StereoInterpolatorV1@calibHandling@measurement
//
// 编译路径:
//   G:\workspace\sdk_win_build\soft.atr.meta.2cams.sdk\
//     soft.atr.2cams.sdk\dev\include\geometry\StereoProviderV3.cpp
//
// DLL字符串证据:
//   "Cannot read param stereo/"
//   "No [stereo] has no key "
//   "[stereo]"
//   "Error when registring StereoProviderV0/V1/V2/V3"
//   "Error when registring StereoInterpolatorV1"
//   "Cannot create StereoInterpolator for algo version "
//   "StereoProvider is nullptr"
//   "Calibration File"
//   "calibParamInTemperatureBin"
//   "Temperature of the sensor during geometrical calibration for the current index"
//   "Temperature compensation mode, 0 = disabled, 1 = enabled"
//   "Error getting two closest calibrations"
//   "Unknown error when interpolating the calibration parameters"
//   "Received temperature is NaN and no previous valid calibration is available"
//   "No loaded calibrations"
//   "No valid CMM calibration index"
//   "Calibration is not valid"
//   "Cannot allocate calibration file reader"
//   "Cannot get calibration at index "
//   "Given calibration is too close from an already registered one"
//   "Problematic addition was calibration index "
//
// 工厂模式:
//   .?AV?$Factory@VStereoProviderBase@calibHandling@measurement@@IU?$hash@I@std@@@factory@measurement@@
//   .?AV?$Prototype@VStereoProviderBase@calibHandling@measurement@@@factory@measurement@@
//
// 标定系统架构（推断）:
//   fusionTrack 设备内部存储多组标定参数（对应不同温度）
//   SDK 启动时从设备读取标定文件，解析为多组 ftkStereoParameters
//   运行时根据当前温度，使用 StereoInterpolatorV1 在两组最近的标定之间插值
//   得到当前温度下的最优相机参数
// ===========================================================================

#pragma once

#include <ftkInterface.h>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

namespace measurement {
namespace calibHandling {

/// 标定数据条目 — 对应一个温度点的标定参数
struct CalibrationEntry
{
    float temperature;               ///< 标定时的温度 (°C)
    ftkStereoParameters stereoParams; ///< 该温度下的双目参数
    uint32_t calibIndex;             ///< 标定索引
    bool isValid;                    ///< 是否有效
};

/// StereoProviderBase — 标定数据提供者抽象基类
///
/// 版本演进:
///   V0: 最初版本，单组标定参数
///   V1: 支持多组标定参数
///   V2: 增强的标定格式
///   V3: 当前版本，完整的温度补偿标定
class StereoProviderBase
{
public:
    virtual ~StereoProviderBase() = default;

    /// 从设备标定文件加载参数
    /// @param calibData 标定文件原始数据
    /// @param dataSize  数据大小
    /// @return true 如果加载成功
    virtual bool loadFromFile(const uint8_t* calibData, uint32_t dataSize) = 0;

    /// 获取指定索引的标定参数
    virtual bool getCalibrationAtIndex(uint32_t index, CalibrationEntry& entry) const = 0;

    /// 获取标定条目数量
    virtual uint32_t getCalibrationCount() const = 0;

    /// 获取标定类型
    virtual CalibrationType getCalibrationType() const = 0;
};

/// StereoProviderV3 — 当前主版本的标定数据提供者
///
/// 实现了温度补偿标定:
/// - 设备在工厂标定时，会在多个温度下分别标定
/// - 每个温度点对应一组完整的双目相机参数
/// - 运行时根据温度传感器读数，插值计算最优参数
class StereoProviderV3 : public StereoProviderBase
{
public:
    StereoProviderV3();
    ~StereoProviderV3() override = default;

    bool loadFromFile(const uint8_t* calibData, uint32_t dataSize) override;
    bool getCalibrationAtIndex(uint32_t index, CalibrationEntry& entry) const override;
    uint32_t getCalibrationCount() const override;
    CalibrationType getCalibrationType() const override;

    /// 获取设备序列号
    const char* getSerialNumber() const { return m_serialNumber; }

private:
    std::vector<CalibrationEntry> m_entries;
    CalibrationType m_calibType;
    char m_serialNumber[64];
    uint32_t m_fileVersion;
};

/// StereoInterpolatorV1 — 温度插值器
///
/// DLL字符串:
///   "Error getting two closest calibrations"
///   "Unknown error when interpolating the calibration parameters"
///   "Received temperature is NaN and no previous valid calibration is available"
///
/// 算法:
/// 1. 根据当前温度找到最近的两个标定温度点
/// 2. 在两组标定参数之间线性插值
/// 3. 返回插值后的 ftkStereoParameters
class StereoInterpolatorV1
{
public:
    StereoInterpolatorV1() : m_lastValidTemperature(std::numeric_limits<float>::quiet_NaN()) {}

    /// 设置标定数据
    void setCalibrationEntries(const std::vector<CalibrationEntry>& entries);

    /// 根据温度获取插值后的标定参数
    ///
    /// @param temperature  当前设备温度 (°C)
    /// @param params       输出的插值标定参数
    /// @return true 如果插值成功
    bool getInterpolatedParameters(float temperature, ftkStereoParameters& params);

private:
    std::vector<CalibrationEntry> m_entries;
    float m_lastValidTemperature;
    ftkStereoParameters m_lastValidParams;

    /// 在两组参数之间线性插值
    void _interpolate(const ftkStereoParameters& a,
                      const ftkStereoParameters& b,
                      float t,
                      ftkStereoParameters& result);

    /// 找到最近的两个标定温度索引
    bool _findTwoClosest(float temperature, uint32_t& idx1, uint32_t& idx2, float& t);
};

// ===========================================================================
// StereoProviderV3 实现
// ===========================================================================

inline StereoProviderV3::StereoProviderV3()
    : m_calibType(CalibrationType::Unknown)
    , m_fileVersion(0)
{
    std::memset(m_serialNumber, 0, sizeof(m_serialNumber));
}

inline bool StereoProviderV3::loadFromFile(const uint8_t* calibData, uint32_t dataSize)
{
    // 标定文件格式（从DLL字符串推断）:
    //
    // 文件为 JSON 格式（V3版本），包含:
    //   "calibrationType": 标定类型 (uint32)
    //   "calibParamInTemperatureBin": 温度标定数据数组
    //   "stereo": 双目参数部分
    //     "stereo/focal": 焦距
    //     "stereo/distortions": 畸变参数
    //   "geometricalCalibrationBinId": CMM标定索引
    //
    // 此处简化为二进制解析示意

    if (!calibData || dataSize < 8)
        return false;

    // 实际实现会解析 JSON 或二进制格式
    // 从 DLL 字符串可知使用 JSON 格式:
    //   "cannot parse JSON input"
    //   "cannot find attribute \"environment\" in configuration file"

    return true;
}

inline bool StereoProviderV3::getCalibrationAtIndex(uint32_t index, CalibrationEntry& entry) const
{
    // DLL字符串: "Cannot get calibration at index "
    if (index >= m_entries.size())
        return false;

    entry = m_entries[index];
    return true;
}

inline uint32_t StereoProviderV3::getCalibrationCount() const
{
    return static_cast<uint32_t>(m_entries.size());
}

inline CalibrationType StereoProviderV3::getCalibrationType() const
{
    return m_calibType;
}

// ===========================================================================
// StereoInterpolatorV1 实现
// ===========================================================================

inline void StereoInterpolatorV1::setCalibrationEntries(
    const std::vector<CalibrationEntry>& entries)
{
    m_entries = entries;

    // 按温度排序
    std::sort(m_entries.begin(), m_entries.end(),
              [](const CalibrationEntry& a, const CalibrationEntry& b) {
                  return a.temperature < b.temperature;
              });
}

inline bool StereoInterpolatorV1::_findTwoClosest(
    float temperature, uint32_t& idx1, uint32_t& idx2, float& t)
{
    // DLL字符串: "Error getting two closest calibrations"

    if (m_entries.empty())
        return false;

    if (m_entries.size() == 1)
    {
        idx1 = idx2 = 0;
        t = 0.0f;
        return true;
    }

    // 找到温度最近的两个标定点
    for (uint32_t i = 0; i < m_entries.size() - 1; ++i)
    {
        if (temperature >= m_entries[i].temperature &&
            temperature <= m_entries[i + 1].temperature)
        {
            idx1 = i;
            idx2 = i + 1;
            float range = m_entries[i + 1].temperature - m_entries[i].temperature;
            t = (range > 1e-6f) ? (temperature - m_entries[i].temperature) / range : 0.0f;
            return true;
        }
    }

    // 超出范围 — 使用最近的端点
    if (temperature < m_entries.front().temperature)
    {
        idx1 = idx2 = 0;
        t = 0.0f;
    }
    else
    {
        idx1 = idx2 = static_cast<uint32_t>(m_entries.size() - 1);
        t = 0.0f;
    }
    return true;
}

inline void StereoInterpolatorV1::_interpolate(
    const ftkStereoParameters& a,
    const ftkStereoParameters& b,
    float t,
    ftkStereoParameters& result)
{
    // 线性插值所有参数
    float w1 = 1.0f - t;
    float w2 = t;

    // 左相机
    for (int i = 0; i < 2; ++i)
    {
        result.LeftCamera.FocalLength[i] = w1 * a.LeftCamera.FocalLength[i] + w2 * b.LeftCamera.FocalLength[i];
        result.LeftCamera.OpticalCentre[i] = w1 * a.LeftCamera.OpticalCentre[i] + w2 * b.LeftCamera.OpticalCentre[i];
    }
    for (int i = 0; i < 5; ++i)
    {
        result.LeftCamera.Distorsions[i] = w1 * a.LeftCamera.Distorsions[i] + w2 * b.LeftCamera.Distorsions[i];
    }
    result.LeftCamera.Skew = w1 * a.LeftCamera.Skew + w2 * b.LeftCamera.Skew;

    // 右相机
    for (int i = 0; i < 2; ++i)
    {
        result.RightCamera.FocalLength[i] = w1 * a.RightCamera.FocalLength[i] + w2 * b.RightCamera.FocalLength[i];
        result.RightCamera.OpticalCentre[i] = w1 * a.RightCamera.OpticalCentre[i] + w2 * b.RightCamera.OpticalCentre[i];
    }
    for (int i = 0; i < 5; ++i)
    {
        result.RightCamera.Distorsions[i] = w1 * a.RightCamera.Distorsions[i] + w2 * b.RightCamera.Distorsions[i];
    }
    result.RightCamera.Skew = w1 * a.RightCamera.Skew + w2 * b.RightCamera.Skew;

    // 外参
    for (int i = 0; i < 3; ++i)
    {
        result.Translation[i] = w1 * a.Translation[i] + w2 * b.Translation[i];
        result.Rotation[i] = w1 * a.Rotation[i] + w2 * b.Rotation[i];
    }
}

inline bool StereoInterpolatorV1::getInterpolatedParameters(
    float temperature, ftkStereoParameters& params)
{
    // DLL字符串: "Received temperature is NaN and no previous valid calibration is available"
    if (std::isnan(temperature))
    {
        if (std::isnan(m_lastValidTemperature))
            return false;

        params = m_lastValidParams;
        return true;
    }

    // DLL字符串: "No loaded calibrations"
    if (m_entries.empty())
        return false;

    uint32_t idx1, idx2;
    float t;
    if (!_findTwoClosest(temperature, idx1, idx2, t))
        return false;

    if (idx1 == idx2)
    {
        params = m_entries[idx1].stereoParams;
    }
    else
    {
        _interpolate(m_entries[idx1].stereoParams,
                     m_entries[idx2].stereoParams,
                     t, params);
    }

    m_lastValidTemperature = temperature;
    m_lastValidParams = params;

    return true;
}

}  // namespace calibHandling
}  // namespace measurement
