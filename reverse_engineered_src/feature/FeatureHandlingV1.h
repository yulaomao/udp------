// ===========================================================================
// 逆向工程还原 — FeatureHandlingV1 特征处理与温度补偿
// 来源: fusionTrack64.dll
//
// RTTI:
//   .?AVFeatureHandlingBase@feature@measurement@@
//   .?AVFeatureHandlingV1@feature@measurement@@
//   .?AVFeatureVersionAnalyser@feature@measurement@@
//   .?AVTemperatureCompensationGetSetter@@
//
// 工厂模式:
//   .?AV?$Factory@VFeatureHandlingBase@feature@measurement@@IU?$hash@I@std@@@factory@measurement@@
//   .?AV?$Prototype@VFeatureHandlingBase@feature@measurement@@@factory@measurement@@
//
// DLL字符串证据:
//   "Error registring FeatureHandlingV1"
//   "cannot instanciate reader for feature version %u"
//   "could not register features reading classes"
//   "./features.dat"
//   "\features.dat"
//   "Temperature compensation algorithm not implemented"
//   "Temperature compensation mode"
//   "Temperature compensation mode, 0 = disabled, 1 = enabled"
//   "This feature is not yet implemented"
//   "This feature was explicitely disabled by a user-option"
//   "Unsupported value for Temperature compensation state"
//
// 功能:
//   从 features.dat 文件读取设备特征描述（图像分辨率、帧率等硬件参数）
//   管理温度补偿的开启/关闭状态
//   为 StereoInterpolator 提供温度数据
// ===========================================================================

#pragma once

#include <cstdint>
#include <string>

namespace measurement {
namespace feature {

/// FeatureHandlingBase — 特征处理基类
class FeatureHandlingBase
{
public:
    virtual ~FeatureHandlingBase() = default;

    /// 从特征文件加载
    virtual bool loadFromFile(const uint8_t* data, uint32_t size) = 0;

    /// 获取特征版本
    virtual uint32_t getVersion() const = 0;

    /// 温度补偿配置
    virtual void setTemperatureCompensation(bool enabled) = 0;
    virtual bool isTemperatureCompensationEnabled() const = 0;
};

/// FeatureHandlingV1 — 版本1特征处理
///
/// DLL字符串: "Error registring FeatureHandlingV1"
///
/// 负责:
/// 1. 解析 features.dat 文件
/// 2. 管理温度补偿模式
/// 3. 提供设备硬件参数（分辨率、帧率等）
class FeatureHandlingV1 : public FeatureHandlingBase
{
public:
    FeatureHandlingV1();
    ~FeatureHandlingV1() override = default;

    bool loadFromFile(const uint8_t* data, uint32_t size) override;
    uint32_t getVersion() const override { return 1u; }

    void setTemperatureCompensation(bool enabled) override;
    bool isTemperatureCompensationEnabled() const override;

    /// 图像参数
    uint16_t getImageWidth() const { return m_imageWidth; }
    uint16_t getImageHeight() const { return m_imageHeight; }
    float getFrameRate() const { return m_frameRate; }

    /// 获取当前温度 (°C)
    /// 用于传递给 StereoInterpolatorV1
    float getCurrentTemperature() const { return m_currentTemperature; }
    void setCurrentTemperature(float temp) { m_currentTemperature = temp; }

private:
    bool m_temperatureCompensation;
    uint16_t m_imageWidth;
    uint16_t m_imageHeight;
    float m_frameRate;
    float m_currentTemperature;
};

// ===========================================================================
// FeatureHandlingV1 实现
// ===========================================================================

inline FeatureHandlingV1::FeatureHandlingV1()
    : m_temperatureCompensation(true)
    , m_imageWidth(640)
    , m_imageHeight(480)
    , m_frameRate(335.0f)
    , m_currentTemperature(25.0f)
{
}

inline bool FeatureHandlingV1::loadFromFile(const uint8_t* data, uint32_t size)
{
    // 解析 features.dat 二进制文件
    // 包含设备硬件特征参数

    if (!data || size < 8)
        return false;

    // 实际文件格式未完全已知
    // 从 DLL 字符串推断包含:
    //   - 版本号
    //   - 图像分辨率
    //   - 帧率
    //   - 支持的功能标志

    return true;
}

inline void FeatureHandlingV1::setTemperatureCompensation(bool enabled)
{
    // DLL字符串:
    //   "Temperature compensation mode, 0 = disabled, 1 = enabled"
    //   "Unsupported value for Temperature compensation state"
    m_temperatureCompensation = enabled;
}

inline bool FeatureHandlingV1::isTemperatureCompensationEnabled() const
{
    return m_temperatureCompensation;
}

/// FeatureVersionAnalyser — 特征文件版本分析
/// RTTI: .?AVFeatureVersionAnalyser@feature@measurement@@
class FeatureVersionAnalyser
{
public:
    /// 分析特征文件版本
    /// @param data 文件数据
    /// @param size 数据大小
    /// @return 版本号 (0 表示无效)
    static uint32_t analyseVersion(const uint8_t* data, uint32_t size)
    {
        if (!data || size < 4)
            return 0;

        // 从前4字节读取版本号（小端）
        uint32_t version = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        return version;
    }
};

}  // namespace feature
}  // namespace measurement
