// ===========================================================================
// 逆向工程还原 — StereoCameraSystem 核心三角化
// 来源: fusionTrack64.dll
// RTTI: .?AVStereoCameraSystem@measurement@@
//
// 编译路径: (推断自同一命名空间的其他文件)
//   G:\workspace\sdk_win_build\soft.atr.meta.2cams.sdk\
//     soft.atr.2cams.sdk\dev\include\StereoCameraSystem.cpp
//
// DLL字符串证据:
//   "Cannot triangulate"
//   "Cannot undistort left"
//   "Cannot undistort right"
//   "Cannot distort left cam"
//   "Cannot distort right cam"
//   "Maximum distance to the epipolar line to consider a match"
//   "Epipolar Maximum Distance"
//   "The value of _fcx is almost zero."
//   "The value of _fcy is almost zero."
//   "The value of radial is almost zero."
//
// 头文件数据结构来源:
//   ftkCameraParameters    — 单相机内参（焦距、光心、畸变、倾斜）
//   ftkStereoParameters    — 双目系统参数（左右内参 + 外参R/T）
//   ftk3DFiducial          — 3D点 + 极线误差 + 三角化误差 + 概率
//   ftkRawData             — 2D检测点（亚像素质心）
//
// API函数签名来源:
//   ATR_EXPORT ftkError ftkTriangulate(ftkLibrary, uint64, const ftk3DPoint*, const ftk3DPoint*, ftk3DPoint*)
//   ATR_EXPORT ftkError ftkReprojectPoint(ftkLibrary, uint64, const ftk3DPoint*, ftk3DPoint*, ftk3DPoint*)
//
// ╔══════════════════════════════════════════════════════════════════╗
// ║  对象内存布局 (从 DLL 偏移分析精确还原)                         ║
// ╠══════════════════════════════════════════════════════════════════╣
// ║  偏移     大小   描述                                           ║
// ║  +0x000   8      vtable 指针                                    ║
// ║  +0x008   1      initialized flag (bool)                        ║
// ║  +0x010   ?      [padding / reserved]                           ║
// ║  +0x090   88     左相机内参 CameraIntrinsics (从偏移+8开始)      ║
// ║  +0x0E8   88     右相机内参 CameraIntrinsics                    ║
// ║  +0x140   1      calibration_invalid flag                       ║
// ║  +0x228   72     基础矩阵 F (3x3 double, 行主序)               ║
// ║  +0x270   ?      [epipolar line working area]                   ║
// ║  +0x298   72     旋转矩阵 R (3x3 double, 行主序)               ║
// ║  +0x2D8   24     平移向量 t (3 doubles)                         ║
// ╚══════════════════════════════════════════════════════════════════╝
// ===========================================================================

#pragma once

#include "math/Matrix.h"
#include "epipolar/EpipolarMatcher.h"
#include <ftkInterface.h>

#include <vector>
#include <cmath>

namespace measurement {

/// 双目相机系统 — 核心三角化引擎
///
/// 该类封装了双目系统的所有几何计算：
/// 1. 畸变校正（undistort / distort）— 迭代 Brown-Conrady 模型
/// 2. 极线匹配（epipolar matching）— 基础矩阵计算 + 极线距离检验
/// 3. 三角化（triangulation）— 两射线最近点中点法
/// 4. 重投影（reprojection）— 3D点投影回左右图像
///
/// 从 RTTI 中可知此类属于 measurement 命名空间顶层
/// 从 DLL 字符串分析可知它由 StereoProviderV0-V3 提供标定参数
class StereoCameraSystem
{
public:
    // ------------------------------------------------------------------
    // 构造与初始化
    // ------------------------------------------------------------------

    StereoCameraSystem();
    ~StereoCameraSystem();

    /// 从标定参数初始化双目系统
    /// @param params 包含左右相机内参及外参的结构体（来自 ftkInterface.h）
    /// @return 是否成功
    bool initialize(const ftkStereoParameters& params);

    /// 检查系统是否已初始化
    bool isInitialized() const { return m_initialized; }

    // ------------------------------------------------------------------
    // 核心三角化
    // ------------------------------------------------------------------

    /// 从左右2D像素点三角化出3D坐标
    ///
    /// DLL 实现链路 (从反汇编确认):
    ///   1. undistortPoint(leftCam, ...) — RVA 0x1f0800
    ///   2. undistortPoint(rightCam, ...) — RVA 0x1f0800
    ///   3. computeEpipolarLine(...) — RVA 0x1eff50
    ///   4. pointToEpipolarDistance(...) — RVA 0x1f2840
    ///   5. closestPointOnRays(...) — RVA 0x1ee990
    ///
    /// @param leftPixel   左图2D检测点 (u, v) — 来自 ftkRawData
    /// @param rightPixel  右图2D检测点 (u, v) — 来自 ftkRawData
    /// @param outPoint    输出3D坐标 (x, y, z) 单位 mm
    /// @param epipolarError  输出极线误差 (像素)
    /// @param triangulationError  输出三角化误差 (mm)
    /// @return true 如果三角化成功
    bool triangulate(const ftk3DPoint& leftPixel,
                     const ftk3DPoint& rightPixel,
                     ftk3DPoint& outPoint,
                     float& epipolarError,
                     float& triangulationError) const;

    /// 批量三角化：匹配左右2D点对并输出3D fiducials
    ///
    /// DLL 实现链路:
    ///   外层循环 — RVA 0x1968d4 (655 bytes)
    ///   极线搜索 — RVA 0x41350 + 0x413b6 (2105 bytes) × 2 (双向)
    ///   单对匹配 — RVA 0x18c740 (998 bytes)
    ///
    /// 算法步骤（从DLL精确还原）:
    /// 1. 对每个右图点, 计算其在左图上的极线 (双向验证)
    /// 2. 使用空间分箱加速搜索极线附近的匹配 (距离 < EpipolarMaxDistance)
    /// 3. 如果双向均找到唯一匹配, 执行三角化
    /// 4. 可选: 应用对称化变换 (symmetriseResult)
    /// 5. 最大输出限制: 50000 (从 cmp rdx, 0xC350 确认)
    ///
    /// @param leftRawData  左图2D检测结果
    /// @param leftCount    左图点数
    /// @param rightRawData 右图2D检测结果
    /// @param rightCount   右图点数
    /// @param fiducials    输出3D fiducial数组
    /// @param maxFiducials 最大输出数量
    /// @return 实际输出的fiducial数量
    uint32_t matchAndTriangulate(const ftkRawData* leftRawData,
                                 uint32_t leftCount,
                                 const ftkRawData* rightRawData,
                                 uint32_t rightCount,
                                 ftk3DFiducial* fiducials,
                                 uint32_t maxFiducials) const;

    /// 3D点重投影到左右相机像面
    ///
    /// @param point3D 输入3D坐标
    /// @param outLeft 输出左图2D投影
    /// @param outRight 输出右图2D投影
    /// @return true 如果投影成功
    bool reproject(const ftk3DPoint& point3D,
                   ftk3DPoint& outLeft,
                   ftk3DPoint& outRight) const;

    // ------------------------------------------------------------------
    // 配置
    // ------------------------------------------------------------------

    /// 设置极线最大距离（像素）— 对应选项 "Epipolar Maximum Distance"
    /// DLL字符串: .?AVEpipolarMaxGetSetter@@
    void setEpipolarMaxDistance(float pixels) { m_epipolarMaxDistance = pixels; }
    float getEpipolarMaxDistance() const { return m_epipolarMaxDistance; }

private:
    // ------------------------------------------------------------------
    // 内部计算方法 — 每个方法对应一个 DLL 函数
    // ------------------------------------------------------------------

    /// 去畸变：迭代 Brown-Conrady 模型 (20次迭代)
    ///
    /// DLL函数: RVA 0x1f0800 + 0x1f0884 (573 bytes)
    /// 常量:
    ///   阈值 1e-7 (RVA 0x248EC8) — "almost zero" 检查
    ///   迭代次数 20 (cmp eax, 0x14)
    ///
    /// DLL字符串: "The value of _fcx is almost zero."
    ///            "The value of _fcy is almost zero."
    ///            "The value of radial is almost zero."
    math::Vec2d undistort(const math::Vec2d& pixelPoint,
                          const ftkCameraParameters& cam) const;

    /// 加畸变：正向 Brown-Conrady 模型
    ///
    /// DLL字符串: "Cannot distort left cam", "Cannot distort right cam"
    math::Vec2d distort(const math::Vec2d& normalizedPoint,
                        const ftkCameraParameters& cam) const;

    /// 计算极线系数
    ///
    /// DLL函数: RVA 0x1eff50 (680 bytes)
    /// 基础矩阵 F 在对象偏移 +0x228 (72 bytes = 3x3 double)
    ///
    /// 算法:
    ///   1. p = (x, y, 1.0)  齐次坐标
    ///   2. line = F * p     (调用 0x37080, 3x3矩阵-向量乘)
    ///   3. 归一化: 如果 |b|>|a|, 取 (0, -c/b); 否则 (-c/a, 0)
    ///   4. 方向: (-b, a) 归一化
    math::Vec3d computeEpipolarLine(const math::Vec2d& leftPointNormalized) const;

    /// 计算点到极线的距离（像素）
    ///
    /// DLL函数: RVA 0x1f2840 (573 bytes)
    /// 算法: (a*x + b*y + c) / sqrt(a²+b²)
    float pointToEpipolarDistance(const math::Vec2d& rightPoint,
                                 const math::Vec3d& epipolarLine) const;

    /// 两射线最近点 — 经典中点法三角化
    ///
    /// DLL函数: RVA 0x1ee990 (1209 bytes)
    /// 内部调用:
    ///   0x1ee610 — buildLeftRay
    ///   0x1ee740 — buildRightRay
    ///   0x1ede00 — solveClosestPoint (1525 bytes)
    /// 常量:
    ///   0.5 (RVA 0x248EE8) — 中点计算
    ///
    /// 左射线: P_L = O_L + t * d_L  (O_L = 原点)
    /// 右射线: P_R = O_R + s * d_R  (O_R = -R^T*t)
    /// 最近点 = (P_L(t*) + P_R(s*)) / 2
    math::Vec3d closestPointOnRays(const math::Vec3d& originL,
                                   const math::Vec3d& dirL,
                                   const math::Vec3d& originR,
                                   const math::Vec3d& dirR,
                                   double& minDistance) const;

    // ------------------------------------------------------------------
    // 内部标定数据 — 与 DLL 对象布局对应
    // ------------------------------------------------------------------

    bool m_initialized;                 // +0x008

    /// 左相机内参 (在对象中位于 +0x090)
    ftkCameraParameters m_leftCam;
    /// 右相机内参 (在对象中位于 +0x0E8)
    ftkCameraParameters m_rightCam;

    /// 标定无效标志 (在对象中位于 +0x140)
    bool m_calibrationInvalid;

    /// 右相机相对于左相机的旋转矩阵 (在对象中位于 +0x298)
    math::Mat3d m_rotation;
    /// 右相机相对于左相机的平移向量 mm (在对象中位于 +0x2D8)
    math::Vec3d m_translation;

    /// 基础矩阵 F (在对象中位于 +0x228)
    /// F = K_R^{-T} * [t]_x * R * K_L^{-1}
    math::Mat3d m_fundamentalMatrix;

    /// 左相机内参矩阵 K_L
    math::Mat3d m_KL;
    /// 右相机内参矩阵 K_R
    math::Mat3d m_KR;
    /// 左相机内参矩阵逆 K_L^{-1}
    math::Mat3d m_KL_inv;
    /// 右相机内参矩阵逆 K_R^{-1}
    math::Mat3d m_KR_inv;

    /// 极线匹配最大距离（像素）
    float m_epipolarMaxDistance;
};

}  // namespace measurement
