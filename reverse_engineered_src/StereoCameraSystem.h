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
// ===========================================================================

#pragma once

#include "math/Matrix.h"
#include <ftkInterface.h>

#include <vector>
#include <cmath>

namespace measurement {

/// 双目相机系统 — 核心三角化引擎
///
/// 该类封装了双目系统的所有几何计算：
/// 1. 畸变校正（undistort / distort）
/// 2. 极线匹配（epipolar matching）
/// 3. 三角化（triangulation）— 将左右2D点对转换为3D坐标
/// 4. 重投影（reprojection）— 将3D点投影回左右图像
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
    /// 实现逻辑（从DLL字符串和API文档推断）:
    /// 1. 对左右2D点进行去畸变 (undistort)
    /// 2. 构建左右射线（从光心出发通过去畸变点）
    /// 3. 计算两射线的最近点作为3D坐标
    /// 4. 计算极线误差和三角化误差
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
    /// 算法步骤（从ftkGetLastFrame错误码和数据流推断）:
    /// 1. 对每个左图点，计算其在右图上的极线
    /// 2. 在极线附近搜索右图点（距离 < EpipolarMaxDistance）
    /// 3. 对找到的匹配对执行三角化
    /// 4. 计算匹配概率（多个候选时降低概率）
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
    // 内部计算方法
    // ------------------------------------------------------------------

    /// 去畸变：将带畸变的像素坐标转换为理想（无畸变）归一化坐标
    ///
    /// 使用 Brown-Conrady 畸变模型（与 OpenCV 兼容）
    /// 参数来自 ftkCameraParameters:
    ///   FocalLength[2], OpticalCentre[2], Distorsions[5], Skew
    ///
    /// DLL字符串: "Cannot undistort left", "Cannot undistort right"
    math::Vec2d undistort(const math::Vec2d& pixelPoint,
                          const ftkCameraParameters& cam) const;

    /// 加畸变：将理想归一化坐标转换为带畸变的像素坐标
    ///
    /// DLL字符串: "Cannot distort left cam", "Cannot distort right cam"
    math::Vec2d distort(const math::Vec2d& normalizedPoint,
                        const ftkCameraParameters& cam) const;

    /// 计算极线系数
    /// 利用基础矩阵 F = K_R^{-T} * [t]_x * R * K_L^{-1}
    /// 返回 ax + by + c = 0 的 (a, b, c)
    math::Vec3d computeEpipolarLine(const math::Vec2d& leftPointNormalized) const;

    /// 计算点到极线的距离（像素）
    float pointToEpipolarDistance(const math::Vec2d& rightPoint,
                                 const math::Vec3d& epipolarLine) const;

    /// 两射线最近点 — 经典中点法三角化
    ///
    /// 左射线: P_L = O_L + t * d_L
    /// 右射线: P_R = O_R + s * d_R
    /// 最近点 = (P_L(t*) + P_R(s*)) / 2
    math::Vec3d closestPointOnRays(const math::Vec3d& originL,
                                   const math::Vec3d& dirL,
                                   const math::Vec3d& originR,
                                   const math::Vec3d& dirR,
                                   double& minDistance) const;

    // ------------------------------------------------------------------
    // 内部标定数据
    // ------------------------------------------------------------------

    bool m_initialized;

    /// 左相机内参
    ftkCameraParameters m_leftCam;
    /// 右相机内参
    ftkCameraParameters m_rightCam;

    /// 右相机相对于左相机的旋转矩阵
    math::Mat3d m_rotation;
    /// 右相机相对于左相机的平移向量 (mm)
    math::Vec3d m_translation;

    /// 基础矩阵 F（用于极线计算）
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
