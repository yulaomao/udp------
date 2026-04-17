// ===========================================================================
// StereoAlgoPipeline.h - 基于 StereoAlgoLib 的 SDK 兼容接口封装
//
// 将 stereo_algo::StereoVision 封装为与 SDK 类型 (ftkXxx) 兼容的管线接口，
// 方便被 stereo99_CompareAlgorithms.cpp 直接调用。
//
// 替代旧的 ReverseEngineeredPipeline.h，消除对
// reverse_engineered_src/ 中间层的依赖。
//
// 命名空间: stereo_algo  (与 StereoAlgoLib 一致)
// ===========================================================================

#pragma once

#include "StereoAlgoLib.h"
#include <ftkInterface.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace stereo_algo {

// ===========================================================================
// StereoAlgoPipeline - SDK 兼容管线封装
// ===========================================================================

class StereoAlgoPipeline
{
public:
    StereoAlgoPipeline();
    ~StereoAlgoPipeline();

    // ------------------------------------------------------------------
    // 初始化
    // ------------------------------------------------------------------

    /// 从 SDK ftkStereoParameters 初始化
    bool initialize(const ftkStereoParameters& params);

    /// 检查是否已初始化
    bool isInitialized() const;

    // ------------------------------------------------------------------
    // 核心算法 - 与 SDK API 对应
    // ------------------------------------------------------------------

    /// 单点三角化 - 等价于 ftkTriangulate
    bool triangulate(const ftk3DPoint& leftPixel,
                     const ftk3DPoint& rightPixel,
                     ftk3DPoint* outPoint,
                     float* outEpipolarError = nullptr,
                     float* outTriangulationError = nullptr) const;

    /// 重投影 - 等价于 ftkReprojectPoint
    bool reproject(const ftk3DPoint& point3D,
                   ftk3DPoint* outLeft,
                   ftk3DPoint* outRight) const;

    /// 批量极线匹配+三角化
    uint32_t matchAndTriangulate(const ftkRawData* leftRawData,
                                  uint32_t leftCount,
                                  const ftkRawData* rightRawData,
                                  uint32_t rightCount,
                                  ftk3DFiducial* fiducials,
                                  uint32_t maxFiducials) const;

    // ------------------------------------------------------------------
    // 工具识别
    // ------------------------------------------------------------------

    /// 注册几何体
    bool registerGeometry(const ftkRigidBody& geometry);

    /// 移除已注册的几何体
    bool clearGeometry(uint32_t geometryId);

    /// 执行工具匹配
    uint32_t matchMarkers(const ftk3DFiducial* fiducials,
                          uint32_t fiducialCount,
                          ftkMarker* markers,
                          uint32_t maxMarkers);

    // ------------------------------------------------------------------
    // 配置
    // ------------------------------------------------------------------

    /// 设置极线最大距离 (pixels, 默认 3.0)
    void setEpipolarMaxDistance(float pixels);

    /// 获取极线最大距离
    float getEpipolarMaxDistance() const;

    /// 获取版本信息
    static std::string version();

private:
    StereoVision m_sv;

    // 几何体注册
    struct RegisteredGeometry
    {
        uint32_t geometryId;
        uint32_t pointsCount;
        Vec3 points[64];   // 最多64个 fiducial
    };
    std::map<uint32_t, RegisteredGeometry> m_geometries;

    // 配置
    float m_epipolarMaxDist;

    // 标定数据
    StereoCalibration m_cal;
};

}  // namespace stereo_algo
