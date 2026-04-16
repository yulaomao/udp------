// ===========================================================================
// 逆向工程还原 — MatchMarkers 工具识别与位姿估计
// 来源: fusionTrack64.dll
// 编译路径:
//   G:\workspace\sdk_win_build\soft.atr.meta.2cams.sdk\
//     soft.atr.2cams.sdk\dev\include\ftkMatchMarker.cpp
//
// RTTI:
//   .?AV?$SpecialisedGeometryExt@$02@markerReco@measurement@@  (3 fiducials)
//   .?AV?$SpecialisedGeometryExt@$03@markerReco@measurement@@  (4 fiducials)
//   ...
//   .?AV?$SpecialisedGeometryExt@$0CA@@markerReco@measurement@@  (32 fiducials)
//   ...
//   .?AV?$SpecialisedGeometryExt@$0CB@@markerReco@measurement@@  (33 fiducials)
//   (直到64个 fiducials)
//
// DLL字符串证据:
//   "Invalid MatchMarker instance"
//   "for geometry ID %u, unkown triangle ID %zu looked for in MatchMarkers::_gatherTrianglesForFourPiMarkers"
//   "unknown geometry ID %u looked for in MatchMarkers::_gatherTrianglesForFourPiMarkers"
//   "Cannot perform registration with " — Kabsch配准
//   "Registration Mean Error"
//   "Maximum mean registration error to consider a valid marker"
//   "partially obliquity corrected marker detected"
//   "Obliquity compensation enable"
//   "Four pi marker candidate registration weighting method"
//   "are deducted from the triangles"
//   "Unknown triangle appeared when saving geometry"
//   "triangles"  "triangles/[]"  "triangles/[]/[]"
//   "submarkers/[]/usedTriangles"
//   "angleOfRegistration"
//   "fiducials/[]/angleOfRegistration"
//
// 算法概述（从字符串、RTTI和API文档推断）:
//
// 工具识别算法使用"三角形不变量"方法:
// 1. 从 N 个3D fiducials 中组合出所有可能的三角形
// 2. 计算每个三角形的边长比作为不变量
// 3. 与已注册几何体的三角形不变量进行匹配
// 4. 通过多个匹配三角形确认工具身份
// 5. 使用 Kabsch 算法计算刚体变换（旋转+平移）
// 6. 可选的倾斜（obliquity）补偿
// ===========================================================================

#pragma once

#include "math/Matrix.h"
#include <ftkInterface.h>

#include <vector>
#include <map>
#include <array>
#include <cmath>
#include <algorithm>

namespace measurement {
namespace markerReco {

/// 三角形不变量 — 用于几何体快速识别
/// 三角形由三条边长定义，排序后的边长比是旋转/平移不变的
struct TriangleInvariant
{
    uint32_t idx[3];      ///< fiducial 索引（在几何体中）
    float sides[3];       ///< 三条边长（升序排列）
    float sideRatios[2];  ///< 边长比: sides[0]/sides[2], sides[1]/sides[2]
};

/// SpecialisedGeometryExt — 模板化的几何体扩展
///
/// RTTI 显示存在 SpecialisedGeometryExt<3> 到 SpecialisedGeometryExt<64>
/// 每个模板实例预计算了该数量fiducial点可能的三角形组合
///
/// @tparam N fiducial 点数量
template <uint32_t N>
class SpecialisedGeometryExt
{
public:
    SpecialisedGeometryExt() : m_geometryId(0), m_pointsCount(0) {}

    /// 从 ftkRigidBody 初始化
    void initialize(const ftkRigidBody& rigidBody);

    /// 获取几何体 ID
    uint32_t geometryId() const { return m_geometryId; }

    /// 获取预计算的三角形不变量
    const std::vector<TriangleInvariant>& triangles() const { return m_triangles; }

    /// 获取 fiducial 位置
    const math::Vec3d& fiducialPosition(uint32_t index) const { return m_positions[index]; }

    /// 获取点数
    uint32_t pointsCount() const { return m_pointsCount; }

private:
    uint32_t m_geometryId;
    uint32_t m_pointsCount;
    std::array<math::Vec3d, N> m_positions;
    std::array<math::Vec3d, N> m_normals;
    std::array<float, N> m_angleOfView;

    /// 预计算的三角形不变量
    /// C(N, 3) 个三角形
    std::vector<TriangleInvariant> m_triangles;

    /// 预计算三角形不变量
    void _precomputeTriangles();
};

// ---------------------------------------------------------------------------
// SpecialisedGeometryExt 实现
// ---------------------------------------------------------------------------

template <uint32_t N>
void SpecialisedGeometryExt<N>::initialize(const ftkRigidBody& rigidBody)
{
    m_geometryId = rigidBody.geometryId;
    m_pointsCount = std::min(rigidBody.pointsCount, N);

    for (uint32_t i = 0; i < m_pointsCount; ++i)
    {
        m_positions[i][0] = rigidBody.fiducials[i].position.x;
        m_positions[i][1] = rigidBody.fiducials[i].position.y;
        m_positions[i][2] = rigidBody.fiducials[i].position.z;

        m_normals[i][0] = rigidBody.fiducials[i].normalVector.x;
        m_normals[i][1] = rigidBody.fiducials[i].normalVector.y;
        m_normals[i][2] = rigidBody.fiducials[i].normalVector.z;

        m_angleOfView[i] = rigidBody.fiducials[i].fiducialInfo.angleOfView;
    }

    _precomputeTriangles();
}

template <uint32_t N>
void SpecialisedGeometryExt<N>::_precomputeTriangles()
{
    m_triangles.clear();

    // 生成所有 C(n, 3) 个三角形
    for (uint32_t i = 0; i < m_pointsCount; ++i)
    {
        for (uint32_t j = i + 1; j < m_pointsCount; ++j)
        {
            for (uint32_t k = j + 1; k < m_pointsCount; ++k)
            {
                TriangleInvariant tri;
                tri.idx[0] = i;
                tri.idx[1] = j;
                tri.idx[2] = k;

                // 计算三条边长
                math::Vec3d d01 = m_positions[j] - m_positions[i];
                math::Vec3d d02 = m_positions[k] - m_positions[i];
                math::Vec3d d12 = m_positions[k] - m_positions[j];

                tri.sides[0] = static_cast<float>(d01.norm());
                tri.sides[1] = static_cast<float>(d02.norm());
                tri.sides[2] = static_cast<float>(d12.norm());

                // 升序排列
                std::sort(tri.sides, tri.sides + 3);

                // 边长比（旋转+缩放不变量）
                if (tri.sides[2] > 1e-6f)
                {
                    tri.sideRatios[0] = tri.sides[0] / tri.sides[2];
                    tri.sideRatios[1] = tri.sides[1] / tri.sides[2];
                }
                else
                {
                    tri.sideRatios[0] = 0.0f;
                    tri.sideRatios[1] = 0.0f;
                }

                m_triangles.push_back(tri);
            }
        }
    }
}

// ===========================================================================
/// MatchMarkers — 工具识别与位姿估计引擎
///
/// 这是 fusionTrack SDK 的核心工具识别类
/// 编译路径: ftkMatchMarker.cpp
///
/// 处理流程:
/// 1. 接收3D fiducial点集合
/// 2. 从中组合三角形
/// 3. 用三角形不变量匹配已注册的几何体
/// 4. 确认匹配后，使用 Kabsch 算法计算刚体变换
/// 5. 可选: 倾斜补偿（obliquity compensation）
/// 6. 输出: ftkMarker（旋转矩阵 + 平移向量 + 配准误差）
// ===========================================================================
class MatchMarkers
{
public:
    MatchMarkers();
    ~MatchMarkers();

    /// 注册几何体
    /// DLL字符串: "Registering the geometry for wireless marker %u"
    bool registerGeometry(const ftkRigidBody& geometry);

    /// 移除已注册的几何体
    bool clearGeometry(uint32_t geometryId);

    /// 执行工具匹配 — 核心算法入口
    ///
    /// @param fiducials    输入3D fiducial点
    /// @param fiducialCount 点数
    /// @param markers      输出匹配结果
    /// @param maxMarkers   最大输出数
    /// @return 匹配到的marker数量
    uint32_t matchMarkers(const ftk3DFiducial* fiducials,
                          uint32_t fiducialCount,
                          ftkMarker* markers,
                          uint32_t maxMarkers);

    // 配置
    void setRegistrationMaxError(float mm) { m_registrationMaxError = mm; }
    void setObliquityCompensation(bool enabled) { m_obliquityEnabled = enabled; }

private:
    /// 已注册几何体存储
    /// DLL字符串表明使用 geometry ID 作为键
    struct RegisteredGeometry
    {
        ftkRigidBody rigidBody;
        std::vector<TriangleInvariant> triangles;
        std::vector<math::Vec3d> modelPoints;
    };
    std::map<uint32_t, RegisteredGeometry> m_geometries;

    /// 三角形匹配容差
    float m_triangleMatchTolerance;

    /// 最大配准误差 (mm) — DLL选项: "Registration Mean Error"
    float m_registrationMaxError;

    /// 倾斜补偿开关 — DLL选项: "Obliquity compensation enable"
    bool m_obliquityEnabled;

    // ------------------------------------------------------------------
    // 内部方法
    // ------------------------------------------------------------------

    /// 从3D fiducial集合中组合测量三角形
    void _buildMeasuredTriangles(const ftk3DFiducial* fiducials,
                                 uint32_t count,
                                 std::vector<TriangleInvariant>& triangles);

    /// 通过三角形不变量匹配几何体
    /// DLL字符串:
    ///   "for geometry ID %u, unkown triangle ID %zu looked for in MatchMarkers::_gatherTrianglesForFourPiMarkers"
    ///   "unknown geometry ID %u looked for in MatchMarkers::_gatherTrianglesForFourPiMarkers"
    struct MatchCandidate
    {
        uint32_t geometryId;
        uint32_t presenceMask;
        uint32_t fiducialCorresp[FTK_MAX_FIDUCIALS];
        std::vector<std::pair<uint32_t, uint32_t>> pointCorrespondences;  // (geometry idx, fiducial idx)
    };

    bool _findCandidateMatches(const std::vector<TriangleInvariant>& measuredTriangles,
                                const ftk3DFiducial* fiducials,
                                uint32_t fiducialCount,
                                std::vector<MatchCandidate>& candidates);

    /// 使用 Kabsch 算法计算刚体变换
    /// DLL字符串: "Cannot perform registration with "
    /// @return 配准均方根误差 (mm)
    float _computeRigidTransform(const RegisteredGeometry& geom,
                                  const MatchCandidate& candidate,
                                  const ftk3DFiducial* fiducials,
                                  float rotation[3][3],
                                  float translation[3]);

    /// 倾斜补偿
    /// DLL字符串: "partially obliquity corrected marker detected"
    /// DLL RTTI: .?AVObliquityCompensationEnableGetSetter@@
    void _applyObliquityCompensation(const RegisteredGeometry& geom,
                                      const MatchCandidate& candidate,
                                      const ftk3DFiducial* fiducials,
                                      ftkMarker& marker);
};

}  // namespace markerReco
}  // namespace measurement
