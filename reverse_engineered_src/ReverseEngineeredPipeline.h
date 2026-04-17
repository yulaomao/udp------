// ===========================================================================
// ReverseEngineeredPipeline.h — 逆向工程算法封装（产品级接口）
//
// 本类将逆向工程还原的双目视觉算法封装为易用的 C++ API，
// 提供与 fusionTrack SDK 等价的功能，方便直接对比调用。
//
// 功能:
//   1. 标定参数初始化 (从 ftkStereoParameters)
//   2. 单点三角化 (等价于 ftkTriangulate)
//   3. 批量极线匹配+三角化 (等价于 SDK 内部的 matchAndTriangulate)
//   4. 重投影 (等价于 ftkReprojectPoint)
//   5. 去畸变 / 加畸变 (内部辅助)
//
// 用法:
//   ReverseEngineeredPipeline pipeline;
//   pipeline.initialize(stereoParams);
//   pipeline.triangulate(leftPx, rightPx, &outPoint, &epiErr, &triErr);
//   pipeline.reproject(point3D, &outLeft, &outRight);
// ===========================================================================

#pragma once

#include "StereoCameraSystem.h"
#include "markerReco/MatchMarkers.h"
#include <ftkInterface.h>

#include <vector>
#include <string>

namespace reverse_engineered {

/// 单点三角化结果
struct TriangulationResult
{
    ftk3DPoint position;            ///< 3D坐标 (mm)
    float epipolarError;            ///< 极线误差 (pixels)
    float triangulationError;       ///< 三角化误差 (mm)
    bool success;                   ///< 是否成功
};

/// 重投影结果
struct ReprojectionResult
{
    ftk3DPoint leftPixel;           ///< 左图投影 (pixels)
    ftk3DPoint rightPixel;          ///< 右图投影 (pixels)
    bool success;                   ///< 是否成功
};

/// 批量三角化结果 (与 ftk3DFiducial 对应)
struct BatchTriangulationResult
{
    std::vector<ftk3DFiducial> fiducials;   ///< 输出的3D fiducials
    uint32_t count;                          ///< 实际匹配数量
};

// ===========================================================================
/// ReverseEngineeredPipeline — 逆向工程算法产品级封装
///
/// 内部使用 measurement::StereoCameraSystem 执行所有计算，
/// 对外提供简洁的 C 风格接口，与 SDK API 一一对应。
// ===========================================================================
class ReverseEngineeredPipeline
{
public:
    ReverseEngineeredPipeline();
    ~ReverseEngineeredPipeline();

    // ------------------------------------------------------------------
    // 初始化
    // ------------------------------------------------------------------

    /// 从 SDK 标定参数初始化
    /// @param params 从 ftkExtractFrameInfo 获取的标定参数
    /// @return true 如果初始化成功
    bool initialize(const ftkStereoParameters& params);

    /// 检查是否已初始化
    bool isInitialized() const;

    // ------------------------------------------------------------------
    // 核心算法 — 与 SDK API 对应
    // ------------------------------------------------------------------

    /// 单点三角化 — 等价于 ftkTriangulate
    ///
    /// @param leftPixel   左图2D检测点 (u, v, 0)
    /// @param rightPixel  右图2D检测点 (u, v, 0)
    /// @param outPoint    输出3D坐标
    /// @param outEpipolarError    输出极线误差 (pixels)
    /// @param outTriangulationError 输出三角化误差 (mm)
    /// @return true 如果三角化成功
    bool triangulate(const ftk3DPoint& leftPixel,
                     const ftk3DPoint& rightPixel,
                     ftk3DPoint* outPoint,
                     float* outEpipolarError = nullptr,
                     float* outTriangulationError = nullptr) const;

    /// 重投影 — 等价于 ftkReprojectPoint
    ///
    /// @param point3D    输入3D坐标
    /// @param outLeft    输出左图2D投影
    /// @param outRight   输出右图2D投影
    /// @return true 如果投影成功
    bool reproject(const ftk3DPoint& point3D,
                   ftk3DPoint* outLeft,
                   ftk3DPoint* outRight) const;

    /// 批量极线匹配+三角化
    ///
    /// @param leftRawData   左图2D检测结果
    /// @param leftCount     左图点数
    /// @param rightRawData  右图2D检测结果
    /// @param rightCount    右图点数
    /// @param fiducials     输出3D fiducial数组 (预分配)
    /// @param maxFiducials  最大输出数量
    /// @return 实际输出的fiducial数量
    uint32_t matchAndTriangulate(const ftkRawData* leftRawData,
                                 uint32_t leftCount,
                                 const ftkRawData* rightRawData,
                                 uint32_t rightCount,
                                 ftk3DFiducial* fiducials,
                                 uint32_t maxFiducials) const;

    // ------------------------------------------------------------------
    // 工具识别 — 与 SDK 内部 MatchMarkers 对应
    // ------------------------------------------------------------------

    /// 注册几何体 (等价于 ftkSetRigidBody)
    ///
    /// @param geometry 几何体定义
    /// @return true 如果注册成功
    bool registerGeometry(const ftkRigidBody& geometry);

    /// 移除已注册的几何体
    bool clearGeometry(uint32_t geometryId);

    /// 执行工具匹配 — 等价于 SDK 内部的 marker 识别流程
    ///
    /// @param fiducials     输入3D fiducial点 (来自 matchAndTriangulate)
    /// @param fiducialCount 点数
    /// @param markers       输出匹配结果 (预分配)
    /// @param maxMarkers    最大输出数量
    /// @return 匹配到的marker数量
    uint32_t matchMarkers(const ftk3DFiducial* fiducials,
                          uint32_t fiducialCount,
                          ftkMarker* markers,
                          uint32_t maxMarkers);

    // ------------------------------------------------------------------
    // 配置
    // ------------------------------------------------------------------

    /// 设置极线最大距离 (pixels)
    void setEpipolarMaxDistance(float pixels);

    /// 获取极线最大距离
    float getEpipolarMaxDistance() const;

    /// 获取版本信息
    static std::string version();

private:
    measurement::StereoCameraSystem m_stereo;
    measurement::markerReco::MatchMarkers m_matchMarkers;
};

}  // namespace reverse_engineered
