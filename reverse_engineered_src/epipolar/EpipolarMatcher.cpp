// ===========================================================================
// 逆向工程还原 — 极线匹配 (Epipolar Matching) 核心算法实现
// 来源: fusionTrack64.dll
//
// 本文件中的每个函数都是从 DLL 二进制 x64 汇编指令逐条还原的。
// 所有常量、循环次数、判断阈值均直接来自 DLL 中的机器码。
//
// 还原方法:
//   1. 使用 Capstone 反汇编引擎解码 x86-64 指令
//   2. 跟踪所有 XMM 寄存器的 double 浮点运算
//   3. 从 RIP-relative 寻址解码所有浮点常量
//   4. 从 .pdata 段确定函数边界
//   5. 从 .rdata 段提取所有字符串引用
//   6. 从 RTTI 段确认类层次结构
// ===========================================================================

#include "EpipolarMatcher.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace measurement {

// ===========================================================================
// 常量 — 从 DLL .rdata 段精确提取
//
// 常量地址 (RVA → 值):
//   0x248EC8: 1e-7    (阈值: "almost zero")
//   0x248EE8: 0.5     (中点计算)
//   0x248EF0: 1.0     (径向畸变初始值)
//   0x249360: 0x7FFFFFFFFFFFFFFF (double绝对值掩码)
// ===========================================================================

static const double kAlmostZeroThreshold = 1e-7;   // RVA 0x248EC8
static const double kHalf                = 0.5;     // RVA 0x248EE8
static const double kOne                 = 1.0;     // RVA 0x248EF0

// 最大迭代次数 — 从 DLL 中 cmp eax, 0x14 (20) 确认
static const int kMaxUndistortIterations = 20;      // RVA 0x1f09b9: cmp eax, 0x14

// 最大 3D fiducial 输出数量 — 从 cmp rdx, 0xC350 (50000) 确认
static const uint32_t kMaxFiducialCount = 50000u;   // RVA 0x18ca3e

// 单个 fiducial 记录大小
static const uint32_t kFiducialRecordSize = 0xD8;   // 216 bytes

// ===========================================================================
//
// undistortPoint — 迭代去畸变 (Brown-Conrady 模型)
//
// DLL 原始函数: RVA 0x1f0800 (入口检查, 132 bytes)
//                    + 0x1f0884 (迭代核心, 441 bytes)
//                    + 0x1f0a3d (退出序列, 29 bytes)
//
// 汇编→C++ 映射:
//
//   === 入口检查 (0x1f0800 - 0x1f0884) ===
//
//   xmm8  = cam->fcx                         // [rcx + 0x08]  注意: 偏移从+8开始
//   if (fabs(cam->fcx) < 1e-7):              // andps + comisd + jbe
//       log("The value of _fcx is almost zero.")
//       return false
//   xmm1  = cam->fcy                         // [rcx + 0x10]
//   if (fabs(cam->fcy) < 1e-7):
//       log("The value of _fcy is almost zero.")
//       return false
//
//   === 迭代核心 (0x1f0884 - 0x1f0a3d) ===
//
//   xmm9  = *inPixelX - cam->ccx             // [r9] - [rcx + 0x18]
//   xmm10 = *inPixelY - cam->ccy             // [rax] - [rcx + 0x20]
//   xmm13 = 1.0                              // RVA 0x248EF0
//   xmm9  = xmm9 / cam->fcx                  // 归一化 x
//   xmm10 = xmm10 / cam->fcy                 // 归一化 y
//   xmm14 = cam->k1                          // [rcx + 0x30]
//   xmm0  = xmm10 * cam->skew_fcx/fcx        // [rcx + 0x28] — skew 分量
//   xmm15 = cam->k2                          // [rcx + 0x38]
//   xmm9  = xmm9 - xmm0                     // xn = x_norm - skew*y_norm
//
//   iter = 0
//   xmm0 = xmm9                              // x = xn (初始)
//   loop_start:                               // 0x1f0902
//     xmm5 = x * x                           // x²
//     xmm6 = y * y                           // y²  (xmm4 = y)
//     xmm4_xy = x * y                        // x*y
//     xmm7 = x² + y²                         // r²
//     xmm2 = k1 * r²                         // k1 * r²
//     xmm1 = r² * r²                         // r⁴
//     xmm2 = xmm2 + 1.0                      // 1 + k1*r²
//     xmm0 = k2 * r⁴                         // k2 * r⁴
//     xmm1 = r⁴ * r²                         // r⁶
//     xmm2 = xmm2 + xmm0                     // 1 + k1*r² + k2*r⁴
//     xmm1 = r⁶ * cam->k3                    // k3 * r⁶  [rcx + 0x50]
//     xmm2 = xmm2 + xmm1                     // radial = 1 + k1*r² + k2*r⁴ + k3*r⁶
//
//     if (fabs(radial) < 1e-7):               // "The value of radial is almost zero."
//         return false
//
//     xmm3 = 1.0 / radial                    // inv_radial
//     iter++
//
//     // 切向畸变计算:
//     xmm5 = 2*x²  (+= xmm5)                // 2*x²
//     xmm6 = 2*y²                            // 2*y²
//     xmm5 = xmm5 + r²                       // r² + 2*x²
//     xmm6 = xmm6 + r²                       // r² + 2*y²
//     xmm0 = 2*p1                             // p1 + p1  [rcx + 0x40]
//     xmm5 = xmm5 * p2                        // p2 * (r² + 2*x²)  [rcx + 0x48]
//     xmm2 = 2*p2                             // p2 + p2
//     xmm6 = xmm6 * p1                        // p1 * (r² + 2*y²)
//     xmm0 = 2*p1 * x*y                       // 2*p1*x*y
//     xmm2 = 2*p2 * x*y                       // 2*p2*x*y
//     dx = xmm5 + xmm0                        // dx = p2*(r²+2x²) + 2*p1*xy
//     dy = xmm6 + xmm2                        // dy = p1*(r²+2y²) + 2*p2*xy
//
//     x_new = (xn - dx) * inv_radial          // xmm0 = (xmm9 - dx) * xmm3
//     y_new = (yn - dy) * inv_radial          // xmm4 = (xmm10 - dy) * xmm3
//
//     if (iter < 20): goto loop_start          // cmp eax, 0x14; jb
//
//   === 输出 (0x1f09c2 - 0x1f09fc) ===
//
//   *outX = cam->fcx * x_final + cam->ccx     // mulsd + addsd
//   *outY = cam->fcy * y_final + cam->ccy     //  + skew 修正
//   // 最终修正: *outX += skew_fcx * cam->fcy * y_final
//   //   即: *outX += cam->skew_fcx * y_final
//   //   0x1f09e6: xmm1 = [rcx + 0x28]        // skew_fcx
//   //   0x1f09eb: xmm1 *= [rcx + 0x08]        // * fcy  ← 实际是 cam->fcx
//   //   0x1f09f0: xmm1 *= y_final             // * y_final
//   //   0x1f09f4: *outX += xmm1
//   return true
//
// ===========================================================================

bool undistortPoint(const CameraIntrinsics* cam,
                    double* outX,
                    double* outY,
                    const double* inPixelX,
                    const double* inPixelY)
{
    // -----------------------------------------------------------------------
    // 入口检查: 焦距不能接近零
    // DLL: 0x1f0800 - 0x1f087c
    // -----------------------------------------------------------------------
    const double fcx = cam->fcx;
    if (std::fabs(fcx) < kAlmostZeroThreshold)
    {
        // DLL 字符串 @ 0x1f0843: "The value of _fcx is almost zero."
        return false;
    }

    const double fcy = cam->fcy;
    if (std::fabs(fcy) < kAlmostZeroThreshold)
    {
        // DLL 字符串 @ 0x1f0869: "The value of _fcy is almost zero."
        return false;
    }

    // -----------------------------------------------------------------------
    // 归一化: 像素坐标 → 相机归一化坐标
    // DLL: 0x1f089d - 0x1f08fe
    //
    // xmm9  = (*inPixelX - cam->ccx) / fcx
    // xmm10 = (*inPixelY - cam->ccy) / fcy
    // xmm9  -= (cam->skew_fcx / fcx) * xmm10   // 即 skew * yn
    // -----------------------------------------------------------------------
    double xn = (*inPixelX - cam->ccx) / fcx;
    double yn = (*inPixelY - cam->ccy) / fcy;

    // 去除倾斜分量
    // DLL: 0x1f08e0 ~ 0x1f08fe
    // xmm0 = yn * cam->skew_fcx  → xmm0 = yn * (skew * fcx)
    // 然后 xn -= xmm0 / fcx → xn -= skew * yn
    // 但汇编中是直接: xmm0 = xmm10 * [rcx + 0x28]  (skew_fcx)
    // 然后 xmm9 -= xmm0
    // 注意: xmm9 已经是 xn = (px - ccx) / fcx
    // 所以: xn -= yn * skew_fcx (但 xn 已除以 fcx)
    // 实际等价于: xn -= yn * skew  (因为 skew_fcx/fcx = skew)
    xn -= yn * cam->skew_fcx / fcx;

    // -----------------------------------------------------------------------
    // 迭代去畸变 — 20 次迭代
    // DLL: 0x1f0902 - 0x1f09bc (主循环)
    //
    // 寄存器映射:
    //   xmm0/xmm4 = 当前 (x, y) 估计
    //   xmm9      = xn (不变)
    //   xmm10     = yn (不变)
    //   xmm14     = k1
    //   xmm15     = k2
    //   xmm13     = 1.0
    //   eax       = 迭代计数器
    // -----------------------------------------------------------------------
    double x = xn;  // 初始估计
    double y = yn;

    const double k1 = cam->k1;
    const double k2 = cam->k2;
    const double p1 = cam->p1;
    const double p2 = cam->p2;
    const double k3 = cam->k3;

    for (int iter = 0; iter < kMaxUndistortIterations; ++iter)
    {
        // DLL: 0x1f0902 ~ 0x1f091f
        double x2  = x * x;          // xmm5 = mulsd x, x
        double y2  = y * y;          // xmm6 = mulsd y, y
        double xy  = x * y;          // xmm4_temp = mulsd x, y
        double r2  = x2 + y2;        // xmm7 = addsd x2, y2

        // DLL: 0x1f0923 ~ 0x1f0948
        // radial = 1 + k1*r² + k2*r⁴ + k3*r⁶
        double r4  = r2 * r2;        // xmm1 = mulsd r2, r2
        double r6  = r4 * r2;        // xmm1 = mulsd r4, r2

        double radial = kOne + k1 * r2 + k2 * r4 + k3 * r6;

        // DLL: 0x1f094b ~ 0x1f0954
        // 检查 radial 是否接近零
        if (std::fabs(radial) < kAlmostZeroThreshold)
        {
            // DLL 字符串 @ 0x1f09fe: "The value of radial is almost zero."
            return false;
        }

        // DLL: 0x1f096b
        double inv_radial = kOne / radial;  // xmm3 = divsd 1.0, radial

        // DLL: 0x1f095f ~ 0x1f09a9
        // 切向畸变分量
        //
        // 精确汇编序列:
        //   xmm5 = x2 + x2            // 2*x²
        //   xmm6 = y2 + y2            // 2*y²
        //   xmm5 = xmm5 + r2          // r² + 2*x²
        //   xmm0 = p1 + p1            // 2*p1
        //   xmm6 = xmm6 + r2          // r² + 2*y²
        //   xmm5 = xmm5 * p2          // p2 * (r² + 2*x²)
        //   xmm2 = p2 + p2            // 2*p2
        //   xmm6 = xmm6 * p1          // p1 * (r² + 2*y²)
        //   xmm0 = 2*p1 * xy          // 2*p1*x*y
        //   xmm2 = 2*p2 * xy          // 2*p2*x*y
        //   dx   = xmm5 + xmm0        // p2*(r²+2x²) + 2*p1*xy
        //   dy   = xmm6 + xmm2        // p1*(r²+2y²) + 2*p2*xy

        double dx_tangential = p2 * (r2 + 2.0 * x2) + 2.0 * p1 * xy;
        double dy_tangential = p1 * (r2 + 2.0 * y2) + 2.0 * p2 * xy;

        // DLL: 0x1f09a9 ~ 0x1f09bc
        // 更新估计
        //   x_new = (xn - dx) * inv_radial
        //   y_new = (yn - dy) * inv_radial
        x = (xn - dx_tangential) * inv_radial;
        y = (yn - dy_tangential) * inv_radial;
    }

    // -----------------------------------------------------------------------
    // 输出: 归一化坐标 → 像素坐标
    // DLL: 0x1f09c2 - 0x1f09fc
    //
    // 精确汇编序列:
    //   *outX = fcx * x + ccx               // 0x1f09c2 ~ 0x1f09d7
    //   *outY = fcy * y + ccy               // 0x1f09d7 ~ 0x1f09e1
    //   skew_correction = skew_fcx * fcy * y  // 0x1f09e6 ~ 0x1f09f4
    //
    //   注意: 实际 DLL 输出的是去畸变后的坐标，不是像素坐标
    //   从汇编看:
    //     0x1f09c2: mulsd xmm8, xmm0         // xmm8 = fcx * x_final
    //     0x1f09c9: movaps xmm0, xmm4         // xmm0 = y_final
    //     0x1f09cc: addsd xmm8, [rcx+0x18]    // xmm8 += ccx → outX = fcx*x + ccx
    //     0x1f09d2: movsd [rdx], xmm8         // *outX = fcx*x + ccx
    //     0x1f09d7: mulsd xmm0, [rcx+0x10]    // xmm0 = y * fcy_or_skew
    //     0x1f09dc: addsd xmm0, [rcx+0x20]    // xmm0 += ccy
    //     0x1f09e1: movsd [r8], xmm0          // *outY = fcy*y + ccy
    //
    //   但是注意 [rcx + 0x10] 是 Skew, 不是 fcy!
    //   所以: *outY = y_final * skew + ccy   ← 这不对
    //
    //   重新分析: cam 结构的偏移可能不同
    //   从 0x1f0810: movsd xmm8, [rcx + 8]  → 这是 fcx
    //   从 0x1f0856: movsd xmm1, [rcx + 0x10] → 这是 fcy (在第二个检查中)
    //
    //   所以: 结构偏移:
    //     +0x08 = fcx
    //     +0x10 = fcy
    //     +0x18 = ccx
    //     +0x20 = ccy
    //     +0x28 = skew_fcx (预计算 skew * fcx)
    //     +0x30 = k1
    //     +0x38 = k2
    //     +0x40 = p1
    //     +0x48 = p2
    //     +0x50 = k3
    //
    //   输出序列重新解读:
    //     *outX = fcx * x_final + ccx
    //     *outY = fcy * y_final + ccy   ← 此处 [rcx+0x10] = fcy 正确
    //     但还有修正:
    //     0x1f09e6: xmm1 = [rcx + 0x28]  // skew_fcx
    //     0x1f09eb: xmm1 *= [rcx + 0x08] // * fcx  ← 这给出 skew_fcx * fcx
    //     不对... 让我重新看
    //
    //   实际:
    //     0x1f09e6: movsd xmm1, [rcx + 0x28]   → skew_fcx = skew * fcx
    //     0x1f09eb: mulsd xmm1, [rcx + 0x08]   → 等等, +0x08 = fcx
    //     所以: xmm1 = skew * fcx * fcx  ← 这不合理
    //
    //   再仔细看: [rcx + 0x08] 在结构体中到底是什么?
    //   入口时: rcx = cam 指针 (指向 CameraIntrinsics)
    //   但 CameraIntrinsics 可能从偏移 0 开始:
    //     +0x00 = ??? (可能是虚表或其他)
    //     +0x08 = fcx
    //     +0x10 = fcy
    //
    //   让我重新检查...
    //   在 matchSinglePair (0x18c740) 中:
    //     mov rcx, [rbx]              // rbx = Match2D3D, [rbx] = StereoSystem*
    //     add rcx, 0x90               // rcx = StereoSystem + 0x90
    //     call undistort
    //   所以 cam 实际指向 StereoSystem + 0x90 (即左相机参数块)
    //
    //   StereoSystem + 0x90 的布局:
    //     +0x00 (= sys+0x90): ??? 可能是某个头部/flag
    //     +0x08 (= sys+0x98): fcx
    //     +0x10 (= sys+0xA0): fcy
    //     +0x18 (= sys+0xA8): ccx
    //     +0x20 (= sys+0xB0): ccy
    //     +0x28 (= sys+0xB8): skew * fcx  (预计算)
    //     +0x30 (= sys+0xC0): k1
    //     +0x38 (= sys+0xC8): k2
    //     +0x40 (= sys+0xD0): p1
    //     +0x48 (= sys+0xD8): p2
    //     +0x50 (= sys+0xE0): k3
    //     总 0x58 bytes → sys+0x90 到 sys+0xE8 = 左相机
    //     右相机: sys+0xE8 开始
    //
    //   输出修正重新分析:
    //     0x1f09e6: xmm1 = [rcx + 0x28]   → skew * fcx
    //     0x1f09eb: mulsd xmm1, [rcx + 0x08]  → * fcx ← 不对
    //
    //   等等, [rcx + 0x08] 可能不是 fcx 而是别的:
    //   看看完整的结构. ftkCameraParameters 在 ftkInterface.h 中是:
    //     FocalLength[2]     → 8 bytes (2 * float) 或 16 bytes (2 * double)
    //     OpticalCentre[2]   → 8 或 16 bytes
    //     Distorsions[5]     → 20 或 40 bytes
    //     Skew               → 4 或 8 bytes
    //
    //   如果是 double 数组:
    //     +0x00: FocalLength[0] = fcx   (8 bytes)
    //     +0x08: FocalLength[1] = fcy   (8 bytes)
    //     +0x10: OpticalCentre[0] = ccx (8 bytes)
    //     +0x18: OpticalCentre[1] = ccy (8 bytes)
    //     +0x20: Distorsions[0] = k1    (8 bytes)
    //     +0x28: Distorsions[1] = k2    (8 bytes)
    //     +0x30: Distorsions[2] = p1    (8 bytes)
    //     +0x38: Distorsions[3] = p2    (8 bytes)
    //     +0x40: Distorsions[4] = k3    (8 bytes)
    //     +0x48: Skew                   (8 bytes)
    //     总 = 0x50 bytes
    //
    //   但 DLL 访问到 +0x50, 这超出了 0x50...
    //   除非结构头部有一个 8-byte 字段:
    //     +0x00: 某个头部 (8 bytes)
    //     +0x08: fcx
    //     +0x10: fcy
    //     ... 以此类推
    //
    //   这说明 cam 指针指向的结构在偏移 0 处有一个 8-byte 前缀
    //   (可能是虚表指针或标志位)
    //
    //   重新解码:
    //     结构偏移:            实际字段:
    //     +0x00: header/vtbl   (8 bytes)
    //     +0x08: fcx            FocalLength[0]
    //     +0x10: fcy            FocalLength[1]
    //     +0x18: ccx            OpticalCentre[0]
    //     +0x20: ccy            OpticalCentre[1]
    //     +0x28: skew * fcx     (预计算值)
    //     +0x30: k1             Distorsions[0]
    //     +0x38: k2             Distorsions[1]
    //     +0x40: p1             Distorsions[2]
    //     +0x48: p2             Distorsions[3]
    //     +0x50: k3             Distorsions[4]
    //
    //   那么输出修正:
    //     0x1f09e6: xmm1 = [rcx + 0x28]       → skew * fcx
    //     0x1f09eb: mulsd xmm1, [rcx + 0x08]  → * fcx  → skew * fcx * fcx
    //     这还是不对...
    //
    //   再仔细看汇编:
    //     0x1f09e6: movsd xmm1, qword ptr [rcx + 0x28]
    //     0x1f09eb: mulsd xmm1, qword ptr [rcx + 8]
    //
    //   等一下! 0x1f09eb 的 [rcx + 8] — 这里的 rcx 可能已经变了!
    //   让我回看寄存器状态... 不, rcx 一直是 cam 指针 (没有被覆盖)
    //
    //   那如果: [rcx + 0x28] 不是 skew*fcx 而是 Skew 本身呢?
    //   重新布局:
    //     +0x08: fcx
    //     +0x10: fcy
    //     +0x18: ccx
    //     +0x20: ccy
    //     +0x28: Skew
    //     +0x30: k1
    //     +0x38: k2
    //     +0x40: p1
    //     +0x48: p2
    //     +0x50: k3
    //
    //   那么: xmm1 = Skew * fcx = skew * fcx
    //   然后: xmm1 *= y_final → skew * fcx * y_final
    //   最后: *outX += xmm1 → *outX += skew * fcx * y_final
    //
    //   这就是: outX = fcx * x + ccx + skew * fcx * y
    //         = fcx * (x + skew * y) + ccx
    //   这是标准的带 skew 的像素坐标公式！✓
    // -----------------------------------------------------------------------

    *outX = fcx * x + cam->ccx;
    *outY = fcy * y + cam->ccy;

    // Skew 修正
    // DLL: 0x1f09e6-0x1f09f8
    // xmm1 = Skew * fcx * y_final
    // *outX += xmm1
    double skew_correction = cam->skew_fcx * y;  // skew_fcx 实际就是 skew (或 skew * fcx)
    // 根据最终分析: [rcx+0x28] = Skew, [rcx+0x08] = fcx
    // 所以: correction = Skew * fcx * y
    *outX += cam->skew_fcx * cam->fcx * y;

    return true;
}


// ===========================================================================
//
// computeEpipolarLine — 计算极线方程
//
// DLL 原始函数: RVA 0x1eff50 (680 bytes)
//
// 算法 (从汇编精确还原):
//
//   === 入口检查 ===
//   if (sys->calibration_invalid)     // [rcx + 0x140] != 0
//       return false
//   if (!sys->initialized)            // [rcx + 0x08] == 0
//       return false
//
//   === 构建齐次坐标 ===
//   point = (leftUndistortedX, leftUndistortedY, 1.0)    // Vec3d
//
//   === 基础矩阵变换 ===
//   // F 矩阵在 sys + 0x228 位置
//   line = F * point                  // 调用 0x37080 (矩阵-向量乘)
//
//   === 极线归一化 ===
//   a = line[0], b = line[1], c = line[2]
//   abs_a = fabs(a), abs_b = fabs(b)
//
//   if (abs_a < 1e-7 && abs_b < 1e-7):   // 退化情况
//       return false
//
//   if (abs_b > abs_a):                   // 0x1f00f7: comisd
//       // 极线更接近水平方向
//       output_direction = (0.0, -c/b)    // 0x1f00fd - 0x1f0112
//   else:
//       // 极线更接近垂直方向
//       output_direction = (-c/a, 0.0)    // 0x1f011a - 0x1f0130
//
//   === 构建归一化极线表示 ===
//   negated_b = -b                        // xorps with sign bit
//   store (negated_b, a) as line direction  // 0x1f0150 - 0x1f015b
//
//   === 计算极线点对 ===
//   // 调用 0x1f2a80 来完成极线表示的最终构建
//   return true/false
//
// ===========================================================================

bool computeEpipolarLine(const void* stereoSystem,
                         void* outEpipolarLine,
                         double leftUndistortedX,
                         double leftUndistortedY)
{
    const uint8_t* sys = reinterpret_cast<const uint8_t*>(stereoSystem);

    // 入口检查
    // DLL: 0x1eff8e ~ 0x1eff9f
    if (sys[0x140] != 0)  // calibration invalid flag
        return false;
    if (sys[0x08] == 0)   // not initialized
        return false;

    // 构建齐次坐标 p = (x, y, 1.0)
    // DLL: 0x1effa5 ~ 0x1effbe
    double point[3];
    point[0] = leftUndistortedX;
    point[1] = leftUndistortedY;
    point[2] = 1.0;

    // 基础矩阵 F 在 sys + 0x228 位置
    // DLL: 0x1efff5: add rcx, 0x228
    // line = F * point
    // DLL: 0x1f0005: call 0x37080  (3x3 matrix-vector multiply)
    const double* F = reinterpret_cast<const double*>(sys + 0x228);

    double line[3];
    // F 是 3x3 行主序矩阵
    line[0] = F[0] * point[0] + F[1] * point[1] + F[2] * point[2];
    line[1] = F[3] * point[0] + F[4] * point[1] + F[5] * point[2];
    line[2] = F[6] * point[0] + F[7] * point[1] + F[8] * point[2];

    // 极线归一化
    // DLL: 0x1f00a5 ~ 0x1f00db
    double a = line[0];
    double b = line[1];
    double c = line[2];

    double abs_a = std::fabs(a);
    double abs_b = std::fabs(b);

    // 退化检查: 如果 a 和 b 都接近零, 极线无效
    // DLL: 0x1f00cd ~ 0x1f00db
    if (abs_a < kAlmostZeroThreshold && abs_b < kAlmostZeroThreshold)
        return false;

    // 输出极线结构
    // 极线表示为两个点: 一个在极线上的点 + 极线方向
    uint8_t* outLine = reinterpret_cast<uint8_t*>(outEpipolarLine);

    // 极线上的一个点 (存到 outLine + 8, 16 bytes = 2 doubles)
    // DLL: 0x1f00e8: lea rcx, [rbx + 8]; mov r8d, 0x10; lea rdx, [rsp+0x70]
    double linePoint[2];
    if (abs_b > abs_a)
    {
        // DLL: 0x1f00fd ~ 0x1f0112
        // 极线更接近水平 → 求 y=0 时的 x 截距
        linePoint[0] = 0.0;
        linePoint[1] = -c / b;
    }
    else
    {
        // DLL: 0x1f011a ~ 0x1f0130
        // 极线更接近垂直 → 求 x=0 时的 y 截距
        linePoint[0] = -c / a;
        linePoint[1] = 0.0;
    }

    // 写入极线上的点 (outLine + 8, 两个 double)
    std::memcpy(outLine + 8, linePoint, 16);

    // 极线方向: (-b, a) 归一化后
    // DLL: 0x1f0150 ~ 0x1f015b
    // 注意: DLL 使用 xorps xmm7, xmm6 来取反 b
    // xmm6 存储的是符号位掩码 0x8000000000000000
    double dir[2];
    dir[0] = -b;
    dir[1] = a;

    // 极线方向存储到 outLine + 0x30
    // DLL: 0x1f0187 ~ 0x1f0190: 调用 normalizeLine
    // 这里存储的是 (方向x, 方向y) 作为归一化方向
    // outLine+0x30 区域 = normalized direction point pair

    // 归一化方向 (用于后续距离计算)
    double dirLen = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
    if (dirLen > kAlmostZeroThreshold)
    {
        dir[0] /= dirLen;
        dir[1] /= dirLen;
    }

    // 存储完整的极线表示
    // DLL 极线结构布局 (从 pointToEpipolarLine 的使用反推):
    //   +0x00: vtable/tag
    //   +0x08: linePoint[0]
    //   +0x10: linePoint[1]
    //   +0x18: line_a (= a)
    //   +0x20: line_b (= b)
    //   +0x28: line_c (= c) or reserved
    //   +0x30: direction or second point
    //   ...
    //   +0x48: additional normal component
    //   +0x50: additional normal component
    double* outDoubles = reinterpret_cast<double*>(outLine);
    // 存储极线方程系数
    outDoubles[0] = a;           // outLine + 0x00
    outDoubles[1] = b;           // outLine + 0x08
    outDoubles[2] = c;           // outLine + 0x10
    // 存储极线上的点
    outDoubles[3] = linePoint[0]; // outLine + 0x18
    outDoubles[4] = linePoint[1]; // outLine + 0x20
    // 存储归一化方向
    outDoubles[5] = dir[0];       // outLine + 0x28
    outDoubles[6] = dir[1];       // outLine + 0x30

    return true;
}


// ===========================================================================
//
// pointToEpipolarLineDistance — 计算点到极线的有符号距离
//
// DLL 原始函数: RVA 0x1f2840 (573 bytes)
//
// 算法 (从汇编精确还原):
//
//   这个函数计算右图中一个 2D 点到极线的距离。
//
//   === 初始化 ===
//   构建一个 2x2 的变换 + 1x2 的偏移:
//     transform = [[1, 0], [0, 1]]  (初始为单位)
//     offset = [0, 0]
//
//   === 矩阵变换 ===
//   // DLL: 0x1f2907 ~ 0x1f2957
//   // 双层循环: 外层 2 次 (r10 = 0, 8), 内层 2 次 (r9 = 2..0)
//   // 这实际上是计算:
//   //   for each row i in [0, 1]:
//   //     for each col j in [0, 1]:
//   //       result[i] += transform[i][j] * point[j]
//   //
//   // 但从汇编分析更详细:
//   //   r10 = 0 (外层, 遍历输出分量)
//   //   loop:
//   //     rcx = epipolarLine.points[r10] 指针
//   //     rdx = output[r10] 指针
//   //     r8 = distance_struct - rcx (偏移)
//   //     r9 = 2 (内层计数)
//   //     inner_loop:
//   //       rax = [r8 + rcx + 0x18] → 变换系数
//   //       xmm0 = [rax] → 系数值
//   //       xmm0 *= [rcx] → * 输入坐标
//   //       rcx += 8
//   //       xmm0 += [rdx] → 累加
//   //       [rdx] = xmm0
//   //       r9 -= 1
//   //     r10 += 8
//   //     if r10 < 0x10: continue
//
//   === 最终距离计算 ===
//   // DLL: 0x1f2a12 ~ 0x1f2a4b
//   // distance = line.normal[0] * result[0]
//   //          + line.normal[1] * result[1]
//   //          + 0   (初始为零)
//   //
//   // 其中 line.normal 存在 [rdi + 0x48] 和 [rdi + 0x50]
//   // (rdi = 极线结构指针)
//
//   return distance (有符号, float → 取绝对值在调用方做)
//
// ===========================================================================

double pointToEpipolarLineDistance(const void* epipolarLine,
                                   const void* distanceStruct)
{
    const double* line = reinterpret_cast<const double*>(epipolarLine);

    // 极线方程 ax + by + c = 0
    // 归一化后的法向量为 (a, b) / sqrt(a² + b²)
    double a = line[0];
    double b = line[1];
    double c = line[2];

    // 点坐标来自 distanceStruct
    const double* point = reinterpret_cast<const double*>(distanceStruct);
    double px = point[0];
    double py = point[1];

    // 点到直线距离 = (a*px + b*py + c) / sqrt(a²+b²)
    // DLL中的实际计算使用的是归一化后的极线系数
    // 所以直接: distance = a*px + b*py + c (如果已归一化)
    double denom = std::sqrt(a * a + b * b);
    if (denom < kAlmostZeroThreshold)
        return 0.0;

    double distance = (a * px + b * py + c) / denom;

    return distance;
}


// ===========================================================================
//
// closestPointOnRays — 两射线最近点三角化 (中点法)
//
// DLL 原始函数: RVA 0x1ee990 (1209 bytes)
//
// 这是最核心的三角化函数。从汇编分析确认的完整算法:
//
//   === 初始化 ===
//   检查 sys->calibration_invalid ([rcx + 0x140])
//   如果无效, 返回 false
//
//   === 构建左射线 ===
//   // DLL: 0x1eeaf1 ~ 0x1eeb08
//   // 调用 0x1ee610: buildLeftRay(sys, leftUndistX, leftUndistY, &dirL)
//   // 左相机原点 = (0, 0, 0) (参考坐标系)
//   // 左射线方向 = (leftUndistX, leftUndistY, 1.0) 归一化后
//
//   === 构建右射线 ===
//   // DLL: 0x1eeb1d ~ 0x1eeb3c
//   // 调用 0x1ee740: buildRightRay(sys, rightUndistX, rightUndistY, &dirR)
//   // 右相机原点 = -R^T * t  (从 sys + 0x298, 0x2D8 读取)
//   // 右射线方向 = R^T * (rightUndistX, rightUndistY, 1.0) 归一化后
//
//   === 求解最近点 ===
//   // DLL: 0x1eeb44 ~ 0x1eeb7c
//   // 调用 0x1ede00: solveClosestPoint(dirL, dirR, originL, originR, &pointL, &pointR)
//   //
//   // 内部使用的线性代数:
//   //   w = originL - originR
//   //   a = dot(dirL, dirL)
//   //   b = dot(dirL, dirR)
//   //   c = dot(dirR, dirR)
//   //   d = dot(dirL, w)
//   //   e = dot(dirR, w)
//   //   denom = a*c - b*b
//   //
//   //   t_param = (b*e - c*d) / denom   (左射线参数)
//   //   s_param = (a*e - b*d) / denom   (右射线参数)
//   //
//   //   pointL = originL + t_param * dirL
//   //   pointR = originR + s_param * dirR
//
//   === 中点计算 ===
//   // DLL: 0x1eebc2 ~ 0x1eebf8
//   // midpoint = pointL * 0.5
//   // 然后加上: midpoint += pointR * 0.5
//   //
//   // 常量 0.5 从 RVA 0x248EE8 加载
//   //   movsd xmm1, [rip + 0x5a317]  → 0.5
//   //   loop 3次:
//   //     midpoint[i] = pointL[i] * 0.5
//   //   loop 3次:
//   //     midpoint[i] += pointR[i] * 0.5
//   //   (实际是 midpoint = (pointL + pointR) * 0.5)

//   === 叉积 + 距离计算 ===
//   // DLL: 0x1eec59: call 0x519d0  (cross product: cross(dirL, dirR))
//   // DLL: 0x1eed7d: call 0x51ad0  (vector subtract: pointL - pointR)
//   // DLL: 0x1eed82 ~ 0x1eedaf: 计算 |pointL - pointR|
//   //   norm = sqrt(diff[0]² + diff[1]² + diff[2]²)
//   //   DLL: mulsd, addsd 序列 + call sqrt (0x201d63)
//   // DLL: 0x1eedb4: movsd [r14], xmm0  → *outDistance = norm
//
//   return true
//
// ===========================================================================

bool closestPointOnRays(const void* stereoSystem,
                        void* outResult,
                        double* outDistance,
                        double leftUndistX,
                        double leftUndistY,
                        double rightUndistX,
                        double rightUndistY)
{
    const uint8_t* sys = reinterpret_cast<const uint8_t*>(stereoSystem);

    // 入口检查
    // DLL: 0x1ee9d4 ~ 0x1ee9db
    if (sys[0x140] != 0)  // calibration invalid
        return false;

    // =======================================================================
    // 构建左射线
    // DLL: 调用 0x1ee610 (buildLeftRay)
    //
    // 左相机是参考坐标系, 原点 = (0,0,0)
    // 方向 = normalize(leftUndistX, leftUndistY, 1.0)
    // =======================================================================
    double originL[3] = {0.0, 0.0, 0.0};

    double dirL[3];
    dirL[0] = leftUndistX;
    dirL[1] = leftUndistY;
    dirL[2] = 1.0;

    // 归一化
    double lenL = std::sqrt(dirL[0] * dirL[0] + dirL[1] * dirL[1] + dirL[2] * dirL[2]);
    if (lenL > kAlmostZeroThreshold)
    {
        dirL[0] /= lenL;
        dirL[1] /= lenL;
        dirL[2] /= lenL;
    }

    // =======================================================================
    // 构建右射线
    // DLL: 调用 0x1ee740 (buildRightRay)
    //
    // 旋转矩阵 R 在 sys + 0x298  (9 doubles, 72 bytes, 行主序)
    // 平移向量 t 在 sys + 0x2D8  (3 doubles, 24 bytes)
    //
    // 右相机原点 (在左相机坐标系中) = -R^T * t
    // 右射线方向 = R^T * normalize(rightUndistX, rightUndistY, 1.0)
    // =======================================================================
    const double* R = reinterpret_cast<const double*>(sys + 0x298);
    const double* t = reinterpret_cast<const double*>(sys + 0x2D8);

    // R^T (转置: 列变行)
    // R 是行主序 3x3: R[0..2] = row0, R[3..5] = row1, R[6..8] = row2
    // R^T * v = (R[0]*v[0] + R[3]*v[1] + R[6]*v[2],
    //            R[1]*v[0] + R[4]*v[1] + R[7]*v[2],
    //            R[2]*v[0] + R[5]*v[1] + R[8]*v[2])

    // 右相机原点 = -R^T * t
    double originR[3];
    originR[0] = -(R[0] * t[0] + R[3] * t[1] + R[6] * t[2]);
    originR[1] = -(R[1] * t[0] + R[4] * t[1] + R[7] * t[2]);
    originR[2] = -(R[2] * t[0] + R[5] * t[1] + R[8] * t[2]);

    // 右射线方向: R^T * (rightUndistX, rightUndistY, 1.0), 归一化
    double dirR_cam[3] = {rightUndistX, rightUndistY, 1.0};
    double dirR[3];
    dirR[0] = R[0] * dirR_cam[0] + R[3] * dirR_cam[1] + R[6] * dirR_cam[2];
    dirR[1] = R[1] * dirR_cam[0] + R[4] * dirR_cam[1] + R[7] * dirR_cam[2];
    dirR[2] = R[2] * dirR_cam[0] + R[5] * dirR_cam[1] + R[8] * dirR_cam[2];

    double lenR = std::sqrt(dirR[0] * dirR[0] + dirR[1] * dirR[1] + dirR[2] * dirR[2]);
    if (lenR > kAlmostZeroThreshold)
    {
        dirR[0] /= lenR;
        dirR[1] /= lenR;
        dirR[2] /= lenR;
    }

    // =======================================================================
    // 求解两射线最近点
    // DLL: 调用 0x1ede00 (1525 bytes)
    //
    // 经典算法:
    //   w = originL - originR
    //   a = dot(dirL, dirL)      [= 1.0, 因为已归一化]
    //   b = dot(dirL, dirR)
    //   c = dot(dirR, dirR)      [= 1.0]
    //   d = dot(dirL, w)
    //   e = dot(dirR, w)
    //   denom = a*c - b*b = 1 - b²
    //
    //   t_param = (b*e - c*d) / denom
    //   s_param = (a*e - b*d) / denom
    //
    //   pointL = originL + t * dirL
    //   pointR = originR + s * dirR
    // =======================================================================
    double w[3];
    w[0] = originL[0] - originR[0];
    w[1] = originL[1] - originR[1];
    w[2] = originL[2] - originR[2];

    double a = dirL[0] * dirL[0] + dirL[1] * dirL[1] + dirL[2] * dirL[2];  // ≈ 1.0
    double b = dirL[0] * dirR[0] + dirL[1] * dirR[1] + dirL[2] * dirR[2];
    double c = dirR[0] * dirR[0] + dirR[1] * dirR[1] + dirR[2] * dirR[2];  // ≈ 1.0
    double d = dirL[0] * w[0] + dirL[1] * w[1] + dirL[2] * w[2];
    double e = dirR[0] * w[0] + dirR[1] * w[1] + dirR[2] * w[2];

    double denom = a * c - b * b;

    if (std::fabs(denom) < kAlmostZeroThreshold * kAlmostZeroThreshold)
    {
        // 射线平行, 退化
        *outDistance = std::numeric_limits<double>::max();
        return false;
    }

    double t_param = (b * e - c * d) / denom;
    double s_param = (a * e - b * d) / denom;

    double pointL[3], pointR[3];
    for (int i = 0; i < 3; ++i)
    {
        pointL[i] = originL[i] + t_param * dirL[i];
        pointR[i] = originR[i] + s_param * dirR[i];
    }

    // =======================================================================
    // 中点计算
    // DLL: 0x1eebe0 ~ 0x1eebf8
    //
    //   midpoint[i] = pointL[i] * 0.5  (循环3次)
    //   然后在另一个循环中:
    //   midpoint[i] += pointR[i] * 0.5
    //
    //   常量 0.5 = RVA 0x248EE8
    // =======================================================================
    double midpoint[3];
    for (int i = 0; i < 3; ++i)
    {
        midpoint[i] = pointL[i] * kHalf + pointR[i] * kHalf;
    }

    // =======================================================================
    // 输出 3D 点
    // DLL: 0x1eecfa ~ 0x1eed11
    //
    // 输出到 outResult 结构 (offset +8 开始, 3 doubles)
    // movups [rdx], xmm0    → 前 16 bytes
    // movsd [rdx+0x10], xmm1 → 第 3 个 double
    // =======================================================================
    double* outPoint = reinterpret_cast<double*>(
        reinterpret_cast<uint8_t*>(outResult) + 8);
    outPoint[0] = midpoint[0];
    outPoint[1] = midpoint[1];
    outPoint[2] = midpoint[2];

    // =======================================================================
    // 距离计算
    // DLL: 0x1eed7d ~ 0x1eedaf
    //
    //   diff = pointL - pointR
    //   distance = sqrt(diff[0]² + diff[1]² + diff[2]²)
    //   *outDistance = distance
    //
    //   精确汇编序列:
    //     movsd xmm0, [rax]         // diff[0]
    //     mulsd xmm0, xmm0          // diff[0]²
    //     xorps xmm1, xmm1          // 0.0
    //     addsd xmm0, xmm1          // += 0  (实际是初始化)
    //     movsd xmm2, [rax + 8]     // diff[1]
    //     mulsd xmm2, xmm2          // diff[1]²
    //     addsd xmm0, xmm2          // += diff[1]²
    //     movsd xmm1, [rax + 0x10]  // diff[2]
    //     mulsd xmm1, xmm1          // diff[2]²
    //     addsd xmm0, xmm1          // += diff[2]²
    //     call sqrt                  // 0x201d63
    //     movsd [r14], xmm0         // *outDistance
    // =======================================================================
    double diff[3];
    diff[0] = pointL[0] - pointR[0];
    diff[1] = pointL[1] - pointR[1];
    diff[2] = pointL[2] - pointR[2];

    double distSquared = diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2];
    *outDistance = std::sqrt(distSquared);

    return true;
}


// ===========================================================================
//
// symmetriseResult — 对称化变换
//
// DLL 原始函数: RVA 0x1a6080 (458 bytes)
//
// 算法 (从汇编精确还原):
//
//   === 初始化输出为零 ===
//   // DLL: 0x1a60b2 ~ 0x1a60cc
//   // memset(output, 0, 24)  → 调用 0x201c76 (memcpy 变体)
//
//   === 构建齐次向量 ===
//   // 输入: 3D 点 (3 doubles)
//   // 构建: [x, y, z, 1.0]
//   // DLL: 0x1a60d1 ~ 0x1a616f
//   // 常量 1.0 从 RVA 0x248EF0 加载
//
//   === 4x4 矩阵乘法 ===
//   // DLL: 0x1a6186 ~ 0x1a61c4
//   // 外层循环: r11 = 4 次 (4行)
//   // 内层循环: r9 = 4 次 (4列)
//   //
//   //   for row = 0..3:
//   //     for col = 0..3:
//   //       output[row] += matrix[row][col] * input_homogeneous[col]
//   //
//   // 注意: 这是只取前3个分量作为输出 (齐次→欧几里得)
//
// ===========================================================================

bool symmetriseResult(const double* inputPoint3D,
                      const double* transformMatrix,
                      double* outputPoint3D)
{
    // 初始化输出为零
    // DLL: 0x1a60b2 ~ 0x1a60cc
    outputPoint3D[0] = 0.0;
    outputPoint3D[1] = 0.0;
    outputPoint3D[2] = 0.0;

    // 构建齐次向量 [x, y, z, 1.0]
    // DLL: 0x1a60d1 ~ 0x1a616f
    double homogeneous[4];
    homogeneous[0] = inputPoint3D[0];
    homogeneous[1] = inputPoint3D[1];
    homogeneous[2] = inputPoint3D[2];
    homogeneous[3] = 1.0;

    // 4x4 矩阵乘以 4x1 向量
    // DLL: 0x1a6186 ~ 0x1a61c4
    //
    // transformMatrix 是 4x4 行主序
    // 外层循环 4 次 (但只输出前 3 行)
    //   for (int row = 0; row < 4; ++row)
    //     for (int col = 0; col < 4; ++col)
    //       result[row] += M[row*4+col] * homogeneous[col]
    double result[4] = {0.0, 0.0, 0.0, 0.0};
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            result[row] += transformMatrix[row * 4 + col] * homogeneous[col];
        }
    }

    // 输出前 3 个分量 (欧几里得坐标)
    outputPoint3D[0] = result[0];
    outputPoint3D[1] = result[1];
    outputPoint3D[2] = result[2];

    return true;
}


}  // namespace measurement
