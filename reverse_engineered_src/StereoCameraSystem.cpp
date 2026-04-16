// ===========================================================================
// 逆向工程还原 — StereoCameraSystem.cpp 实现
// 来源: fusionTrack64.dll (2.7 MB)
// RTTI: .?AVStereoCameraSystem@measurement@@
//
// 这是 fusionTrack SDK 中最核心的算法类
// 实现了双目立体视觉的三角化、极线匹配、重投影
//
// 函数 DLL 地址映射:
//   undistort()             → RVA 0x1f0800 + 0x1f0884 (573 bytes)
//   distort()               → RVA 0x1f0a60 (推测)
//   computeEpipolarLine()   → RVA 0x1eff50 (680 bytes)
//   pointToEpipolarDistance()→ RVA 0x1f2840 (573 bytes)
//   closestPointOnRays()    → RVA 0x1ee990 (1209 bytes)
//   triangulate()           → RVA 0x1bee0b (1266 bytes)
//   matchAndTriangulate()   → RVA 0x1968d4 outer (655 bytes)
//                             + RVA 0x18c740 per-pair (998 bytes)
//                             + RVA 0x41350 search (2105 bytes)
// ===========================================================================

#include "StereoCameraSystem.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace measurement {

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------

StereoCameraSystem::StereoCameraSystem()
    : m_initialized(false)
    , m_leftCam{}
    , m_rightCam{}
    , m_calibrationInvalid(false)
    , m_rotation()
    , m_translation()
    , m_fundamentalMatrix()
    , m_KL()
    , m_KR()
    , m_KL_inv()
    , m_KR_inv()
    , m_epipolarMaxDistance(2.0f)  // 默认值，从DLL选项描述推断
{
}

StereoCameraSystem::~StereoCameraSystem() = default;

// ---------------------------------------------------------------------------
// 初始化
// ---------------------------------------------------------------------------

bool StereoCameraSystem::initialize(const ftkStereoParameters& params)
{
    m_leftCam = params.LeftCamera;
    m_rightCam = params.RightCamera;

    // 构建旋转矩阵（Rodrigues 向量 → 旋转矩阵）
    // ftkStereoParameters::Rotation[3] 是 Rodrigues 旋转向量
    double rx = static_cast<double>(params.Rotation[0]);
    double ry = static_cast<double>(params.Rotation[1]);
    double rz = static_cast<double>(params.Rotation[2]);
    double theta = std::sqrt(rx * rx + ry * ry + rz * rz);

    if (theta < 1e-10)
    {
        m_rotation = math::Mat3d::identity();
    }
    else
    {
        // Rodrigues 公式: R = I + sin(θ) * K + (1-cos(θ)) * K²
        // 其中 K 是反对称矩阵
        double kx = rx / theta, ky = ry / theta, kz = rz / theta;
        double c = std::cos(theta), s = std::sin(theta);
        double v = 1.0 - c;

        m_rotation(0, 0) = c + kx * kx * v;
        m_rotation(0, 1) = kx * ky * v - kz * s;
        m_rotation(0, 2) = kx * kz * v + ky * s;
        m_rotation(1, 0) = ky * kx * v + kz * s;
        m_rotation(1, 1) = c + ky * ky * v;
        m_rotation(1, 2) = ky * kz * v - kx * s;
        m_rotation(2, 0) = kz * kx * v - ky * s;
        m_rotation(2, 1) = kz * ky * v + kx * s;
        m_rotation(2, 2) = c + kz * kz * v;
    }

    m_translation[0] = static_cast<double>(params.Translation[0]);
    m_translation[1] = static_cast<double>(params.Translation[1]);
    m_translation[2] = static_cast<double>(params.Translation[2]);

    // 构建左相机内参矩阵 K_L
    //   [ fx   skew*fx   cx ]
    //   [ 0    fy        cy ]
    //   [ 0    0         1  ]
    m_KL(0, 0) = m_leftCam.FocalLength[0];
    m_KL(0, 1) = m_leftCam.Skew * m_leftCam.FocalLength[0];
    m_KL(0, 2) = m_leftCam.OpticalCentre[0];
    m_KL(1, 1) = m_leftCam.FocalLength[1];
    m_KL(1, 2) = m_leftCam.OpticalCentre[1];
    m_KL(2, 2) = 1.0;

    // 构建右相机内参矩阵 K_R
    m_KR(0, 0) = m_rightCam.FocalLength[0];
    m_KR(0, 1) = m_rightCam.Skew * m_rightCam.FocalLength[0];
    m_KR(0, 2) = m_rightCam.OpticalCentre[0];
    m_KR(1, 1) = m_rightCam.FocalLength[1];
    m_KR(1, 2) = m_rightCam.OpticalCentre[1];
    m_KR(2, 2) = 1.0;

    // 计算逆矩阵 K_L^{-1} 和 K_R^{-1}
    // 对于上三角内参矩阵，逆矩阵有解析解
    {
        double fx = m_KL(0, 0), fy = m_KL(1, 1);
        double cx = m_KL(0, 2), cy = m_KL(1, 2);
        double s = m_KL(0, 1);

        m_KL_inv(0, 0) = 1.0 / fx;
        m_KL_inv(0, 1) = -s / (fx * fy);
        m_KL_inv(0, 2) = (s * cy - fy * cx) / (fx * fy);
        m_KL_inv(1, 1) = 1.0 / fy;
        m_KL_inv(1, 2) = -cy / fy;
        m_KL_inv(2, 2) = 1.0;
    }
    {
        double fx = m_KR(0, 0), fy = m_KR(1, 1);
        double cx = m_KR(0, 2), cy = m_KR(1, 2);
        double s = m_KR(0, 1);

        m_KR_inv(0, 0) = 1.0 / fx;
        m_KR_inv(0, 1) = -s / (fx * fy);
        m_KR_inv(0, 2) = (s * cy - fy * cx) / (fx * fy);
        m_KR_inv(1, 1) = 1.0 / fy;
        m_KR_inv(1, 2) = -cy / fy;
        m_KR_inv(2, 2) = 1.0;
    }

    // 计算基础矩阵 F = K_R^{-T} * [t]_x * R * K_L^{-1}
    // 其中 [t]_x 是平移向量的反对称矩阵
    math::Mat3d tx_cross;
    tx_cross(0, 1) = -m_translation[2];
    tx_cross(0, 2) = m_translation[1];
    tx_cross(1, 0) = m_translation[2];
    tx_cross(1, 2) = -m_translation[0];
    tx_cross(2, 0) = -m_translation[1];
    tx_cross(2, 1) = m_translation[0];

    // E = [t]_x * R (本质矩阵)
    math::Mat3d E = tx_cross * m_rotation;

    // F = K_R^{-T} * E * K_L^{-1}
    m_fundamentalMatrix = m_KR_inv.transpose() * E * m_KL_inv;

    m_initialized = true;
    return true;
}

// ---------------------------------------------------------------------------
// 去畸变 — 迭代 Brown-Conrady 模型
//
// DLL函数: RVA 0x1f0800 (入口检查) + 0x1f0884 (迭代核心)
//
// 精确参数:
//   迭代次数: 20 (DLL: cmp eax, 0x14 @ 0x1f09b9)
//   阈值: 1e-7 (DLL: 从 RVA 0x248EC8 加载)
//   abs掩码: 0x7FFFFFFFFFFFFFFF (DLL: 从 RVA 0x249360 加载)
//
// DLL 中的切向畸变参数顺序 (从偏移分析确认):
//   Distorsions[0] = k1 (径向, r²)     → [cam + 0x30]
//   Distorsions[1] = k2 (径向, r⁴)     → [cam + 0x38]
//   Distorsions[2] = p1 (切向)          → [cam + 0x40]
//   Distorsions[3] = p2 (切向)          → [cam + 0x48]
//   Distorsions[4] = k3 (径向, r⁶)     → [cam + 0x50]
//
// 注意: DLL 中切向畸变的加法顺序与 OpenCV 不同!
//   DLL:   dx = p2*(r² + 2*x²) + 2*p1*x*y
//   OpenCV: dx = 2*p1*x*y + p2*(r² + 2*x²)
//   数学上等价, 但汇编中运算顺序不同
// ---------------------------------------------------------------------------

math::Vec2d StereoCameraSystem::undistort(
    const math::Vec2d& pixelPoint,
    const ftkCameraParameters& cam) const
{
    // 步骤 1: 入口检查 — 焦距不能接近零
    // DLL: 0x1f0810 ~ 0x1f087c
    static const double kAlmostZero = 1e-7;  // RVA 0x248EC8

    double fx = cam.FocalLength[0];
    if (std::fabs(fx) < kAlmostZero)
    {
        // DLL: 0x1f0843: "The value of _fcx is almost zero."
        math::Vec2d fail;
        return fail;
    }

    double fy = cam.FocalLength[1];
    if (std::fabs(fy) < kAlmostZero)
    {
        // DLL: 0x1f0869: "The value of _fcy is almost zero."
        math::Vec2d fail;
        return fail;
    }

    double cx = cam.OpticalCentre[0];
    double cy = cam.OpticalCentre[1];

    // 步骤 2: 归一化坐标
    // DLL: 0x1f089d ~ 0x1f08fe
    double xn = (pixelPoint[0] - cx) / fx;
    double yn = (pixelPoint[1] - cy) / fy;

    // 去除 Skew 分量
    // DLL: 0x1f08e0 ~ 0x1f08fe
    // xn -= yn * Skew  (Skew = cam.Skew, 实际乘以预计算的 skew*fx/fx = skew)
    xn -= yn * cam.Skew;

    // 步骤 3: 迭代去畸变 (20次)
    // DLL: 0x1f0902 ~ 0x1f09bc
    double k1 = cam.Distorsions[0];  // 径向 r²
    double k2 = cam.Distorsions[1];  // 径向 r⁴
    double p1 = cam.Distorsions[2];  // 切向 p1
    double p2 = cam.Distorsions[3];  // 切向 p2
    double k3 = cam.Distorsions[4];  // 径向 r⁶

    double x = xn;  // 初始估计
    double y = yn;

    for (int iter = 0; iter < 20; ++iter)  // DLL: cmp eax, 0x14
    {
        // DLL: 0x1f0902 ~ 0x1f091f
        double x2 = x * x;
        double y2 = y * y;
        double xy = x * y;
        double r2 = x2 + y2;

        // DLL: 0x1f0923 ~ 0x1f0948
        double r4 = r2 * r2;
        double r6 = r4 * r2;

        // radial = 1 + k1*r² + k2*r⁴ + k3*r⁶
        double radial = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;

        // DLL: 0x1f094b ~ 0x1f0954 — 检查 radial 接近零
        if (std::fabs(radial) < kAlmostZero)
        {
            // DLL: 0x1f09fe: "The value of radial is almost zero."
            math::Vec2d fail;
            return fail;
        }

        // DLL: 0x1f096b
        double inv_radial = 1.0 / radial;

        // DLL: 0x1f095f ~ 0x1f09a9 — 切向畸变
        // 汇编精确序列:
        //   dx = p2 * (r² + 2*x²) + 2*p1*x*y
        //   dy = p1 * (r² + 2*y²) + 2*p2*x*y
        double dx = p2 * (r2 + 2.0 * x2) + 2.0 * p1 * xy;
        double dy = p1 * (r2 + 2.0 * y2) + 2.0 * p2 * xy;

        // DLL: 0x1f09a9 ~ 0x1f09bc — 更新估计
        x = (xn - dx) * inv_radial;
        y = (yn - dy) * inv_radial;
    }

    math::Vec2d result;
    result[0] = x;
    result[1] = y;
    return result;
}

// ---------------------------------------------------------------------------
// 加畸变 — 正向模型
// ---------------------------------------------------------------------------

math::Vec2d StereoCameraSystem::distort(
    const math::Vec2d& normalizedPoint,
    const ftkCameraParameters& cam) const
{
    double x = normalizedPoint[0];
    double y = normalizedPoint[1];

    double k1 = cam.Distorsions[0];
    double k2 = cam.Distorsions[1];
    double k3 = cam.Distorsions[2];
    double k4 = cam.Distorsions[3];
    double k5 = cam.Distorsions[4];

    double r2 = x * x + y * y;
    double r4 = r2 * r2;
    double r6 = r4 * r2;

    double radial = 1.0 + k1 * r2 + k2 * r4 + k5 * r6;

    double xd = x * radial + 2.0 * k3 * x * y + k4 * (r2 + 2.0 * x * x);
    double yd = y * radial + k3 * (r2 + 2.0 * y * y) + 2.0 * k4 * x * y;

    // 归一化 → 像素
    math::Vec2d result;
    result[0] = cam.FocalLength[0] * xd + cam.Skew * cam.FocalLength[0] * yd + cam.OpticalCentre[0];
    result[1] = cam.FocalLength[1] * yd + cam.OpticalCentre[1];
    return result;
}

// ---------------------------------------------------------------------------
// 极线计算
//
// DLL函数: RVA 0x1eff50 (680 bytes)
//
// 基础矩阵 F 存储在对象偏移 +0x228 (确认自 0x1efff5: add rcx, 0x228)
//
// 算法步骤:
//   1. 构建齐次坐标 p = (x, y, 1.0) — 使用归一化坐标
//   2. 但 F 使用像素坐标计算, 所以需要先转回像素
//      plPixel = KL * (x, y, 1)
//   3. line = F * plPixel
//   4. 归一化极线: 根据 |a| vs |b| 选择极线上的点
//   5. 方向: (-b, a) 归一化
//
// DLL 中的判断:
//   0x1f00cd: comisd abs_a < 1e-7 && abs_b < 1e-7 → 退化, 返回 false
//   0x1f00f7: comisd abs_b > abs_a → 选择水平/垂直方向
//
// 极线表示的输出格式:
//   line[0] = a, line[1] = b, line[2] = c  (ax + by + c = 0)
//   这个极线是在归一化坐标系中的
// ---------------------------------------------------------------------------

math::Vec3d StereoCameraSystem::computeEpipolarLine(
    const math::Vec2d& leftPointNormalized) const
{
    // 齐次坐标
    math::Vec3d pl;
    pl[0] = leftPointNormalized[0];
    pl[1] = leftPointNormalized[1];
    pl[2] = 1.0;

    // 极线 l' = F * p_L
    // DLL: 0x1f0005: call 0x37080 (3x3 矩阵-向量乘法)
    // 其中 F 已使用像素坐标计算
    // 但这里输入是归一化坐标，需要先转为像素
    math::Vec3d plPixel;
    plPixel[0] = m_KL(0, 0) * pl[0] + m_KL(0, 1) * pl[1] + m_KL(0, 2);
    plPixel[1] = m_KL(1, 1) * pl[1] + m_KL(1, 2);
    plPixel[2] = 1.0;

    // line = F * plPixel
    math::Vec3d line = m_fundamentalMatrix * plPixel;

    // DLL: 0x1f00b4 ~ 0x1f00db
    // 退化检查
    double abs_a = std::fabs(line[0]);
    double abs_b = std::fabs(line[1]);
    static const double kThresh = 1e-7;

    if (abs_a < kThresh && abs_b < kThresh)
    {
        // 退化: 极线系数都接近零
        math::Vec3d zero;
        return zero;
    }

    return line;
}

// ---------------------------------------------------------------------------
// 点到极线距离
//
// DLL函数: RVA 0x1f2840 (573 bytes)
//
// 算法:
//   distance = |a*x + b*y + c| / sqrt(a² + b²)
//
// DLL 中的精确实现:
//   1. 构建 2x2 + 偏移 的变换
//   2. 内层双循环 (r10 = 0,8; r9 = 2..0)
//   3. 最终: distance = normal[0]*result[0] + normal[1]*result[1]
//
// 简化后等价于标准的点到直线距离公式
// ---------------------------------------------------------------------------

float StereoCameraSystem::pointToEpipolarDistance(
    const math::Vec2d& rightPoint,
    const math::Vec3d& epipolarLine) const
{
    // 点到直线距离: |ax + by + c| / sqrt(a² + b²)
    double a = epipolarLine[0];
    double b = epipolarLine[1];
    double c = epipolarLine[2];

    // 右图点的像素坐标
    math::Vec2d rpPixel = distort(rightPoint, m_rightCam);

    double dist = std::abs(a * rpPixel[0] + b * rpPixel[1] + c)
                / std::sqrt(a * a + b * b);

    return static_cast<float>(dist);
}

// ---------------------------------------------------------------------------
// 两射线最近点 — 中点法三角化
//
// DLL函数: RVA 0x1ee990 (1209 bytes)
//
// 内部调用链:
//   0x1ee610 — buildLeftRay  (构建左射线: 原点=(0,0,0), 方向=normalize(x,y,1))
//   0x1ee740 — buildRightRay (构建右射线: 原点=-R^T*t, 方向=R^T*normalize(x,y,1))
//   0x1ede00 — solveClosestPoint (1525 bytes, 线性代数求解)
//   0x519d0  — crossProduct (叉积)
//   0x51ad0  — vectorSubtract (向量减法)
//
// 关键常量:
//   0.5 = RVA 0x248EE8 (中点计算)
//
// DLL 中的精确算法:
//   1. 检查 calibration_invalid flag
//   2. 构建两条射线
//   3. 线性代数求解最近点对
//   4. 中点 = (pointL + pointR) * 0.5
//   5. 距离 = ||pointL - pointR||
//
// 中点计算的汇编序列 (0x1eebc2 ~ 0x1eebf8):
//   movsd xmm1, [rip + 0x5a317]   → 0.5
//   循环 3 次:
//     movsd xmm0, [pointL + i*8]
//     mulsd xmm0, xmm1             → pointL[i] * 0.5
//     movsd [midpoint + i*8], xmm0
//   循环 3 次:
//     movsd xmm0, [pointR + i*8]
//     mulsd xmm0, xmm1             → pointR[i] * 0.5
//     addsd xmm0, [midpoint + i*8] → += midpoint[i]
//     movsd [midpoint + i*8], xmm0
//
// 距离计算的汇编序列 (0x1eed82 ~ 0x1eedaf):
//   movsd xmm0, [diff + 0]
//   mulsd xmm0, xmm0               → diff[0]²
//   xorps xmm1, xmm1               → 0.0
//   addsd xmm0, xmm1               → 初始累加
//   movsd xmm2, [diff + 8]
//   mulsd xmm2, xmm2
//   addsd xmm0, xmm2
//   movsd xmm1, [diff + 16]
//   mulsd xmm1, xmm1
//   addsd xmm0, xmm1
//   call sqrt                       → 0x201d63
//   movsd [r14], xmm0              → *outDistance
// ---------------------------------------------------------------------------

math::Vec3d StereoCameraSystem::closestPointOnRays(
    const math::Vec3d& originL,
    const math::Vec3d& dirL,
    const math::Vec3d& originR,
    const math::Vec3d& dirR,
    double& minDistance) const
{
    // 两直线最近点对的参数解:
    //   L(t) = originL + t * dirL
    //   R(s) = originR + s * dirR
    //
    // 令 w = originL - originR
    // a = dirL · dirL, b = dirL · dirR, c = dirR · dirR
    // d = dirL · w,    e = dirR · w
    // denom = a*c - b*b
    //
    // t* = (b*e - c*d) / denom
    // s* = (a*e - b*d) / denom

    math::Vec3d w = originL - originR;

    double a = dirL.dot(dirL);
    double b = dirL.dot(dirR);
    double c = dirR.dot(dirR);
    double d = dirL.dot(w);
    double e = dirR.dot(w);

    double denom = a * c - b * b;

    if (std::abs(denom) < 1e-15)
    {
        // 射线平行 — 退化情况
        minDistance = std::numeric_limits<double>::max();
        math::Vec3d zero;
        return zero;
    }

    double t = (b * e - c * d) / denom;
    double s = (a * e - b * d) / denom;

    math::Vec3d pointL = originL + dirL * t;
    math::Vec3d pointR = originR + dirR * s;

    // DLL: 中点 = (pointL + pointR) * 0.5
    // 常量 0.5 从 RVA 0x248EE8 加载
    math::Vec3d midpoint = (pointL + pointR) * 0.5;

    // DLL: 距离 = sqrt(diff[0]² + diff[1]² + diff[2]²)
    math::Vec3d diff = pointL - pointR;
    minDistance = diff.norm();

    return midpoint;
}

// ---------------------------------------------------------------------------
// 单点三角化
// ---------------------------------------------------------------------------

bool StereoCameraSystem::triangulate(
    const ftk3DPoint& leftPixel,
    const ftk3DPoint& rightPixel,
    ftk3DPoint& outPoint,
    float& epipolarError,
    float& triangulationError) const
{
    if (!m_initialized)
        return false;

    // 步骤 1: 去畸变
    math::Vec2d leftPx, rightPx;
    leftPx[0] = leftPixel.x;
    leftPx[1] = leftPixel.y;
    rightPx[0] = rightPixel.x;
    rightPx[1] = rightPixel.y;

    math::Vec2d leftNorm = undistort(leftPx, m_leftCam);
    math::Vec2d rightNorm = undistort(rightPx, m_rightCam);

    // 步骤 2: 计算极线误差
    math::Vec3d epiLine = computeEpipolarLine(leftNorm);
    epipolarError = pointToEpipolarDistance(rightNorm, epiLine);

    // 步骤 3: 构建射线
    // 左相机原点 = (0,0,0)（参考坐标系）
    // 左射线方向 = (x_norm, y_norm, 1) 归一化
    math::Vec3d originL;  // (0,0,0)
    math::Vec3d dirL;
    dirL[0] = leftNorm[0];
    dirL[1] = leftNorm[1];
    dirL[2] = 1.0;
    dirL = dirL.normalized();

    // 右相机原点 = R^T * (-t) = -R^T * t
    math::Mat3d Rt = m_rotation.transpose();
    math::Vec3d neg_t = m_translation * (-1.0);
    math::Vec3d originR = Rt * neg_t;

    // 右射线方向: R^T * (x_norm_r, y_norm_r, 1) 归一化
    math::Vec3d dirR_cam;
    dirR_cam[0] = rightNorm[0];
    dirR_cam[1] = rightNorm[1];
    dirR_cam[2] = 1.0;
    math::Vec3d dirR = Rt * dirR_cam;
    dirR = dirR.normalized();

    // 步骤 4: 求最近点
    double minDist = 0.0;
    math::Vec3d point3D = closestPointOnRays(originL, dirL, originR, dirR, minDist);

    outPoint.x = static_cast<float>(point3D[0]);
    outPoint.y = static_cast<float>(point3D[1]);
    outPoint.z = static_cast<float>(point3D[2]);

    triangulationError = static_cast<float>(minDist);

    return true;
}

// ---------------------------------------------------------------------------
// 批量极线匹配 + 三角化
// ---------------------------------------------------------------------------

uint32_t StereoCameraSystem::matchAndTriangulate(
    const ftkRawData* leftRawData,
    uint32_t leftCount,
    const ftkRawData* rightRawData,
    uint32_t rightCount,
    ftk3DFiducial* fiducials,
    uint32_t maxFiducials) const
{
    if (!m_initialized)
        return 0u;

    uint32_t fiducialCount = 0u;

    // 预计算: 去畸变所有右图点
    std::vector<math::Vec2d> rightNormalized(rightCount);
    for (uint32_t j = 0u; j < rightCount; ++j)
    {
        math::Vec2d rp;
        rp[0] = rightRawData[j].centerXPixels;
        rp[1] = rightRawData[j].centerYPixels;
        rightNormalized[j] = undistort(rp, m_rightCam);
    }

    // 对每个左图点尝试匹配右图点
    for (uint32_t i = 0u; i < leftCount && fiducialCount < maxFiducials; ++i)
    {
        math::Vec2d lp;
        lp[0] = leftRawData[i].centerXPixels;
        lp[1] = leftRawData[i].centerYPixels;
        math::Vec2d leftNorm = undistort(lp, m_leftCam);

        // 计算极线
        math::Vec3d epiLine = computeEpipolarLine(leftNorm);

        // 在右图中搜索极线附近的点
        int bestMatch = -1;
        float bestEpiError = m_epipolarMaxDistance;
        int matchCount = 0;

        for (uint32_t j = 0u; j < rightCount; ++j)
        {
            float dist = pointToEpipolarDistance(rightNormalized[j], epiLine);

            if (dist < m_epipolarMaxDistance)
            {
                ++matchCount;
                if (dist < bestEpiError)
                {
                    bestEpiError = dist;
                    bestMatch = static_cast<int>(j);
                }
            }
        }

        if (bestMatch >= 0)
        {
            // 执行三角化
            ftk3DPoint lp3d = { leftRawData[i].centerXPixels, leftRawData[i].centerYPixels, 0.0f };
            ftk3DPoint rp3d = { rightRawData[bestMatch].centerXPixels,
                                rightRawData[bestMatch].centerYPixels, 0.0f };

            ftk3DFiducial& fid = fiducials[fiducialCount];

            float epiErr = 0.0f, triErr = 0.0f;
            if (triangulate(lp3d, rp3d, fid.positionMM, epiErr, triErr))
            {
                fid.leftIndex = i;
                fid.rightIndex = static_cast<uint32_t>(bestMatch);
                fid.epipolarErrorPixels = epiErr;
                fid.triangulationErrorMM = triErr;

                // 概率: 唯一匹配 = 1.0，多个候选则降低
                // 来自头文件注释: "probability is defined by the number of potential matches"
                fid.probability = (matchCount == 1) ? 1.0f : (1.0f / static_cast<float>(matchCount));

                // 合并左右状态位
                fid.status = leftRawData[i].status;

                ++fiducialCount;
            }
        }
    }

    return fiducialCount;
}

// ---------------------------------------------------------------------------
// 重投影
// ---------------------------------------------------------------------------

bool StereoCameraSystem::reproject(
    const ftk3DPoint& point3D,
    ftk3DPoint& outLeft,
    ftk3DPoint& outRight) const
{
    if (!m_initialized)
        return false;

    math::Vec3d p;
    p[0] = point3D.x;
    p[1] = point3D.y;
    p[2] = point3D.z;

    // 左相机投影（左相机是参考坐标系）
    if (std::abs(p[2]) < 1e-6)
        return false;

    math::Vec2d leftNorm;
    leftNorm[0] = p[0] / p[2];
    leftNorm[1] = p[1] / p[2];
    math::Vec2d leftPixel = distort(leftNorm, m_leftCam);
    outLeft.x = static_cast<float>(leftPixel[0]);
    outLeft.y = static_cast<float>(leftPixel[1]);
    outLeft.z = 0.0f;

    // 右相机投影: p_R = R * p + t
    math::Vec3d pR = m_rotation * p + m_translation;
    if (std::abs(pR[2]) < 1e-6)
        return false;

    math::Vec2d rightNorm;
    rightNorm[0] = pR[0] / pR[2];
    rightNorm[1] = pR[1] / pR[2];
    math::Vec2d rightPixel = distort(rightNorm, m_rightCam);
    outRight.x = static_cast<float>(rightPixel[0]);
    outRight.y = static_cast<float>(rightPixel[1]);
    outRight.z = 0.0f;

    return true;
}

}  // namespace measurement
