// ===========================================================================
// 逆向工程还原 — 极线匹配 (Epipolar Matching) 完整头文件
// 来源: fusionTrack64.dll
//
// 还原函数:
//   undistortPoint()        — RVA 0x1f0800 + 0x1f0884 (573 bytes)
//   computeEpipolarLine()   — RVA 0x1eff50 (680 bytes)
//   pointToEpipolarLine()   — RVA 0x1f2840 (573 bytes)
//   closestPointOnRays()    — RVA 0x1ee990 (1209 bytes)
//   symmetriseResult()      — RVA 0x1a6080 (458 bytes)
//   triangulatePoint()      — RVA 0x1bee0b (1266 bytes)
//   matchSinglePair()       — RVA 0x18c740 (998 bytes)
//   findEpipolarMatches()   — RVA 0x41350 + 0x413b6 (2105 bytes)
//   matchAndTriangulate()   — RVA 0x1968d4 outer loop (655 bytes)
//
// DLL字符串证据:
//   "Cannot undistort left"
//   "Cannot undistort right"
//   "Cannot triangulate"
//   "Cannot symmetrise result"
//   "The value of _fcx is almost zero."
//   "The value of _fcy is almost zero."
//   "The value of radial is almost zero."
//   "Could not find binning for point %i at (%f, %f)"
//   "list<T> too long"
//
// RTTI类:
//   .?AVStereoCameraSystem@measurement@@
//   .?AVMatch2D3D@matching@measurement@@
//   .?AVEpipolarMaxGetSetter@@
//
// 编译路径推断:
//   G:\workspace\sdk_win_build\soft.atr.meta.2cams.sdk\
//     soft.atr.2cams.sdk\dev\include\StereoCameraSystem.cpp
// ===========================================================================

#pragma once

#include "../math/Matrix.h"
#include <cmath>
#include <cstring>
#include <vector>
#include <list>
#include <limits>

namespace measurement {

// ---------------------------------------------------------------------------
// 相机内参结构体 (与 ftkCameraParameters 对齐)
//
// 内存布局从 DLL 偏移分析:
//   +0x00: FocalLength[0] (double, _fcx)  — 水平焦距
//   +0x08: FocalLength[1] (double, _fcy)  — 垂直焦距
//   +0x10: Skew           (double)        — 倾斜系数
//   +0x18: OpticalCentre[0] (double, _ccx) — 光心x
//   +0x20: OpticalCentre[1] (double, _ccy) — 光心y
//   +0x28: Distorsions[0] (double, k1)    — 径向畸变k1 (Skew*fcx 计算中)
//   +0x30: Distorsions[1] (double, k2)    — 径向畸变k2
//   +0x38: Distorsions[2] (double, k3)    — 径向畸变k3
//   +0x40: Distorsions[3] (double, p1)    — 切向畸变p1
//   +0x48: Distorsions[4] (double, p2)    — 切向畸变p2
//   +0x50: Distorsions[5] (double, k4)    — 径向畸变k4 (6th order)
//
// 结构体总大小: 0x58 字节 (88 bytes)
//
// 从 DLL undistort 函数内存访问确认:
//   [rcx + 0x00] = FocalLength[0]  (= _fcx)
//   [rcx + 0x08] = FocalLength[1]  (= _fcy)
//   [rcx + 0x10] = Skew
//   [rcx + 0x18] = OpticalCentre[0]
//   [rcx + 0x20] = OpticalCentre[1]
//   [rcx + 0x28] = Skew * FocalLength[0] (预计算)
//   [rcx + 0x30] = Distorsions[0] (k1)
//   [rcx + 0x38] = Distorsions[1] (k2)
//   [rcx + 0x40] = Distorsions[2] (p1)
//   [rcx + 0x48] = Distorsions[3] (p2)
//   [rcx + 0x50] = Distorsions[4] (k3/k4 6th order)
// ---------------------------------------------------------------------------
struct CameraIntrinsics
{
    double fcx;           // +0x00: 水平焦距
    double fcy;           // +0x08: 垂直焦距
    double skew;          // +0x10: 倾斜系数
    double ccx;           // +0x18: 光心x
    double ccy;           // +0x20: 光心y
    double skew_fcx;      // +0x28: Skew 系数 (注意: 虽名为 skew_fcx, 实为纯 Skew 值)
                          //         DLL 中 [rcx+0x28] 配合 [rcx+0x08]=fcx 一起使用
                          //         在 undistort 输出中: *outX += Skew * fcx * y
    double k1;            // +0x30: 径向畸变k1 (r^2)
    double k2;            // +0x38: 径向畸变k2 (r^4)
    double p1;            // +0x40: 切向畸变p1
    double p2;            // +0x48: 切向畸变p2
    double k3;            // +0x50: 径向畸变k3 (r^6)
};

// ---------------------------------------------------------------------------
// 极线搜索参数 — 从 DLL 结构体偏移分析
//
// 在 StereoCameraSystem 对象中的布局:
//   +0x000: vtable指针
//   +0x008: initialized flag (bool)
//   +0x090: 左相机内参 CameraIntrinsics (0x58 bytes)
//   +0x0E8: 右相机内参 CameraIntrinsics (0x58 bytes)
//   +0x140: calibration_invalid flag (bool)
//   +0x1A8: 指向温度补偿器的指针
//   +0x1ED: use_symmetrize flag (bool)
//   +0x228: 基础矩阵 F (从 computeEpipolarLine 中 rcx+0x228 确认)
//   +0x298: 左→右旋转矩阵组件
//   +0x2D8: 左→右平移向量组件
//   +0x5B0: closestPointOnRays 工作结构
//   +0x640: 左相机完整参数结构 (用于 undistort)
//   +0x698: 右相机完整参数结构 (用于 undistort)
//   +0x6F0: flag: 是否跳过 closestPointOnRays
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Match2D3D 匹配器内部结构
//
// 从函数 0x18c740 的成员访问:
//   [rbx + 0x00]: 指向 StereoCameraSystem 对象
//   [rbx + 0x10]: 指向温度补偿器
//   [rbx + 0x18]: 左匹配结果链表头
//   [rbx + 0x20]: 左匹配结果数量
//   [rbx + 0x28]: 右匹配结果链表头
//   [rbx + 0x30]: 右匹配结果数量
//   [rbx + 0x38]: 3D fiducial 输出缓冲区起始
//   [rbx + 0x40]: 3D fiducial 输出缓冲区当前位置
//   [rbx + 0x48]: 3D fiducial 输出缓冲区结束
//   [rbx + 0x50]: 极线最大距离 (float)
//   [rbx + 0x54]: allowMultipleMatches (bool)
//   [rbx + 0x55]: useSymmetrize (bool)
//   [rbx + 0x68]: epipolar line cache (Vec3d, 24 bytes)
//   [rbx + 0xC8]: 距离 cache
//   [rbx + 0xE0]: 指向右图点对
//   [rbx + 0xF8]: closestPointOnRays 结构
//   [rbx + 0x118]: 指向额外数据 (24 bytes)
//
// 单个3D fiducial 输出记录大小: 0xD8 (216 bytes)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 2D检测点 (左/右相机)
//
// 从 outer loop 的 r12+r9 偏移分析:
//   +0x00: 像素坐标 (float x, float y)  -- 4+4 = 8 bytes
//   +0x08: 子像素 (double u, double v)   -- 8+8 = 16 bytes
//   +0x10: 额外属性 (uint32 + ...)
//   +0x20: 去畸变后坐标 (double u, double v)
//   +0x28: 去畸变后坐标 y
//   +0x30: 链表/引用信息
//   +0x38: 点索引 (uint32)
//   +0x3C: matched flag (bool)
//
// 总大小约 0x40 (64 bytes)
// ---------------------------------------------------------------------------
struct RawPoint2D
{
    float  pixelX;          // +0x00
    float  pixelY;          // +0x04
    double subPixelX;       // +0x08
    double subPixelY;       // +0x10
    uint8_t  padding1[8];   // +0x18
    double undistortedX;    // +0x20
    double undistortedY;    // +0x28
    void*  matchLink;       // +0x30
    uint32_t pointIndex;    // +0x38
    bool   matched;         // +0x3C
    uint8_t  padding2[3];   // +0x3D
};

// ---------------------------------------------------------------------------
// 3D Fiducial 输出记录
//
// 大小 0xD8 (216 bytes) 从 imul 常数 0x4bda12f684bda13 确认
// (这是 magic number 用于除以 0xD8)
//
// 布局从 matchSinglePair 的 memcpy 偏移:
//   +0x00 ~ +0x0B: 浮点坐标区 (float x, y, z)
//   +0x0C ~ +0x23: 4x float 数据 (从 movups 指令确认 16 bytes)
//   +0x1C ~ +0x23: 8 bytes 数据 (movsd)
//   +0x50: float epipolarMaxDistance (极线最大距离)
//   +0x54: uint32 = 0 (match quality)
//   +0x58: uint32 rightPointIndex
//   +0x78: float epipolarDistance (极线距离)
//   +0x80 ~ +0xD7: 右图点数据 + 附加信息
//   +0xC5: bool  matched flag
//   +0xC6: uint32 leftFrameIndex
//   +0xCA: uint32 rightFrameIndex
//   +0xCE: uint32 rightPointLoopIndex
// ---------------------------------------------------------------------------
struct Fiducial3DRecord
{
    uint8_t data[0xD8];     // 完整记录

    // 访问方法 (基于偏移分析)
    float& x()        { return *reinterpret_cast<float*>(data + 0x00); }
    float& y()        { return *reinterpret_cast<float*>(data + 0x04); }
    float& z()        { return *reinterpret_cast<float*>(data + 0x08); }

    float& epipolarDistance() { return *reinterpret_cast<float*>(data + 0x78); }

    bool& matchedFlag()    { return *reinterpret_cast<bool*>(data + 0xC5); }
    uint32_t& leftFrameIdx()  { return *reinterpret_cast<uint32_t*>(data + 0xC6); }
    uint32_t& rightFrameIdx() { return *reinterpret_cast<uint32_t*>(data + 0xCA); }
    uint32_t& rightLoopIdx()  { return *reinterpret_cast<uint32_t*>(data + 0xCE); }
};

// ---------------------------------------------------------------------------
// 核心算法函数声明
// 以下函数的实现在 EpipolarMatcher.cpp 中，完全基于 DLL 反汇编还原
// ---------------------------------------------------------------------------

/// undistortPoint — 迭代去畸变
///
/// DLL位置: RVA 0x1f0800 (入口检查) + 0x1f0884 (迭代核心)
/// DLL字符串: "The value of _fcx is almost zero."
///            "The value of _fcy is almost zero."
///            "The value of radial is almost zero."
///
/// 参数 (从寄存器映射):
///   rcx = 指向 CameraIntrinsics
///   rdx = 输出去畸变后 x 坐标指针
///   r8  = 输出去畸变后 y 坐标指针
///   r9  = 输入像素 x 坐标指针
///   [rsp+0x20] = 输入像素 y 坐标指针
///
///   xmm3 = 输入像素 x 坐标 (double)
///   xmm2 = 输入像素 y 坐标 (double)
///   xmm0 = 输入像素 x 坐标 (double) (重复)
///   xmm1 = 输入像素 y 坐标 (double) (重复)
///
/// 算法 (从 DLL 汇编精确还原):
///   1. 检查 fcx 和 fcy 是否接近零 (|val| < 1e-7)
///   2. 计算归一化坐标: xn = (px - ccx) / fcx, yn = (py - ccy) / fcy
///   3. 减去倾斜分量: xn -= skew_fcx/fcx * yn  (即 xn -= skew * yn)
///   4. 迭代求解 (最多20次):
///      r² = x² + y²
///      radial = 1 + k1*r² + k2*r⁴ + k3*r⁶
///      if |radial| < 1e-7: 报错返回
///      inv_radial = 1 / radial
///      dx_tangential = 2*p1*x*y + p2*(r² + 2*x²)  [2*p2*xy + p1*(r²+2y²) for dy]
///      x_new = (xn - dx_tangential) * inv_radial
///      y_new = (yn - dy_tangential) * inv_radial
///   5. 输出: outX = fcx * x_final + ccx, 加上 skew 修正
///           outY = fcy * y_final + ccy
///
/// 返回: bool (true=成功)
bool undistortPoint(const CameraIntrinsics* cam,
                    double* outX,
                    double* outY,
                    const double* inPixelX,
                    const double* inPixelY);

/// computeEpipolarLine — 计算极线系数
///
/// DLL位置: RVA 0x1eff50 (680 bytes)
///
/// 算法:
///   1. 构建齐次坐标 p = (x, y, 1.0)
///   2. 通过基础矩阵变换: l = F * p
///   3. 归一化极线: 如果 |a| > |b|, 则 line = (0, -c/b)方向
///                  否则 line = (-c/a, 0)方向
///   4. 计算极线上的第二个点
///
/// 参数 (从寄存器映射):
///   rcx = StereoCameraSystem 对象指针
///   rdx = 输出极线结构指针
///   xmm2 = 左图去畸变 x 坐标
///   xmm3 = 左图去畸变 y 坐标
///
/// 返回: bool (true=成功)
bool computeEpipolarLine(const void* stereoSystem,
                         void* outEpipolarLine,
                         double leftUndistortedX,
                         double leftUndistortedY);

/// pointToEpipolarLineDistance — 计算点到极线距离
///
/// DLL位置: RVA 0x1f2840 (573 bytes)
///
/// 算法 (从汇编精确还原):
///   1. 将右图去畸变点转换为归一化齐次坐标 p = (x, y)
///   2. 通过极线方程矩阵变换
///   3. 计算: distance = line[0]*p[0] + line[1]*p[1] + line[2]
///      (其中 line 已归一化，所以不需要额外除以 sqrt(a²+b²))
///
/// 参数:
///   rcx = 极线结构指针
///   rdx = 距离输出结构指针
///
/// 返回: float (有符号距离)
double pointToEpipolarLineDistance(const void* epipolarLine,
                                   const void* distanceStruct);

/// closestPointOnRays — 两射线最近点三角化
///
/// DLL位置: RVA 0x1ee990 (1209 bytes)
///
/// 这是核心三角化算法。从汇编分析确认使用了以下步骤:
///
///   1. 检查 calibration_invalid flag ([rcx + 0x140])
///   2. 通过左相机参数构建左射线方向 (调用 0x1ee610)
///   3. 通过右相机参数构建右射线方向 (调用 0x1ee740)
///   4. 构建射线方程并求解最近点 (调用 0x1ede00)
///   5. 计算左右射线方向的叉积 (调用 0x519d0)
///   6. 结果 = 中点 (pointL + pointR) * 0.5
///   7. 计算最小距离 = |pointL - pointR| (调用 0x51ad0)
///   8. 输出距离到 [r14]
///
/// 关键常量:
///   0.5 (中点计算)
///
/// 参数:
///   rcx = StereoCameraSystem 对象 (+0x5B0 offset)
///   rdx = 输出结构 (3D点 + 距离)
///   r8  = 距离输出指针
///   xmm3 = 参数 (极线相关)
///   [rsp+0x20] = 右图去畸变 x
///   [rsp+0x28] = 右图去畸变 y
///   [rsp+0x30] = 左图去畸变 y
///
/// 返回: bool (true=成功)
bool closestPointOnRays(const void* stereoSystem,
                        void* outResult,
                        double* outDistance,
                        double leftUndistX,
                        double leftUndistY,
                        double rightUndistX,
                        double rightUndistY);

/// symmetriseResult — 对称化三角化结果
///
/// DLL位置: RVA 0x1a6080 (458 bytes)
///
/// 当 [stereo + 0x1ED] != 0 且温度补偿器有效时调用。
/// 使用 4x4 变换矩阵对 3D 点进行对称变换。
///
/// 算法 (从汇编分析):
///   1. 读取 3D 点 (24 bytes = 3 doubles)
///   2. 构建 4x1 齐次向量 [x, y, z, 1.0]
///   3. 应用 4x4 对称矩阵乘法
///   4. 输出变换后的 3D 点
///
/// 参数:
///   rcx = 输入 3D 点
///   rdx = 变换结果
///   r8  = 输出 3D 点
///
/// 返回: bool (true=成功)
bool symmetriseResult(const double* inputPoint3D,
                      const double* transformMatrix,
                      double* outputPoint3D);

}  // namespace measurement
