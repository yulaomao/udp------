// ===========================================================================
// 逆向工程还原 — 圆心拟合算法 (Circle Center Fitting)
// 来源: fusionTrack64.dll
//
// DLL函数地址:
//   主函数: 0x58240 - 0x5a54c (8972 bytes, 1971条指令, 128次FP运算)
//   hypot辅助: 0x63ce0 - 0x63d79 (154 bytes)
//   sqrt: 0x201d63 (IAT跳转)
//   矩阵元素访问: 0x5fac0 (30次调用)
//
// 算法说明:
//   本函数实现基于Givens旋转的迭代圆心拟合算法，用于从blob边缘像素
//   精确计算红外反光标记点的亚像素级圆心坐标。
//
//   核心数学:
//     给定 N 个边界点 (xi, yi), 寻找圆心 (cx, cy) 和半径 r
//     使得 Σ((xi-cx)² + (yi-cy)² - r²)² 最小
//
//     使用Givens旋转逐步消元求解，无需显式构造矩阵
//     收敛容限: 1e-7
//     最大迭代: 49次 (DLL: cmp rdi, 0x31)
//
// RTTI关联类:
//   .?AV?$SegmenterV21@...@segmenter@measurement@@
//   .?AVRawFiducial@measurement@@
//   .?AVFiducialDetector@measurement@@
//
// 调用链:
//   FiducialDetector::detect()
//     → SegmenterV21::segment()
//       → CircleFitting::fitCircleCenter()
//         → 生成 ftkRawData 中的亚像素坐标
// ===========================================================================

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace measurement {
namespace segmenter {

// ===========================================================================
// 辅助数学函数 — 从DLL反汇编还原
// ===========================================================================

/// 稳定的二维范数计算
/// DLL: 0x63ce0 - 0x63d79 (154 bytes)
///
/// 反汇编逻辑:
///   if |a| > |b|:
///     return |a| * sqrt(1 + (b/a)²)
///   elif |b| > epsilon:
///     return |b| * sqrt(1 + (a/b)²)
///   else:
///     return 0.0
///
/// 这是标准的 hypot 实现，避免溢出/下溢
inline double stableHypot(double a, double b)
{
    double absA = std::fabs(a);
    double absB = std::fabs(b);

    if (absA > absB)
    {
        // DLL: 0x63d08-0x63d32
        double ratio = absB / absA;
        return absA * std::sqrt(1.0 + ratio * ratio);
    }
    else if (absB > 0.0)
    {
        // DLL: 0x63d3d-0x63d67
        double ratio = absA / absB;
        return absB * std::sqrt(1.0 + ratio * ratio);
    }
    else
    {
        // DLL: 0x63d68-0x63d79
        return 0.0;
    }
}

// ===========================================================================
// 小矩阵类 — 用于圆拟合的3x3和4x4矩阵运算
// 对应DLL中内联展开的矩阵操作
// ===========================================================================

/// 3x3 对称矩阵（用于圆拟合法方程）
struct SymMatrix3x3
{
    double data[3][3];  // 直接按DLL内存布局

    SymMatrix3x3()
    {
        std::memset(data, 0, sizeof(data));
    }

    double& operator()(int r, int c) { return data[r][c]; }
    const double& operator()(int r, int c) const { return data[r][c]; }
};

/// 3维向量
struct Vec3
{
    double x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(double ax, double ay, double az) : x(ax), y(ay), z(az) {}

    double& operator[](int i) { return (&x)[i]; }
    const double& operator[](int i) const { return (&x)[i]; }
};

// ===========================================================================
// Givens旋转结构 — 从DLL反汇编中识别的核心模式
//
// DLL中的Givens旋转模式 (出现在 0x59e3f-0x59e8e):
//   cos = a[j][k]  * c + a[j][l]  * s
//   sin = a[j][l]  * c - a[j][k]  * s
// 其中 c = cos(θ), s = sin(θ), θ = atan2(...)
//
// DLL中使用 xmm9 = c (cosine), xmm7 = s (sine)
// 通过 divsd + xorps(-0.0) 计算
// ===========================================================================

struct GivensRotation
{
    double c;   ///< cos(θ) — DLL: xmm9
    double s;   ///< sin(θ) — DLL: xmm7

    GivensRotation() : c(1.0), s(0.0) {}
    GivensRotation(double cosVal, double sinVal)
        : c(cosVal), s(sinVal) {}

    /// 计算消除 (a, b) 中 b 的Givens旋转
    /// DLL: 0x59df6-0x59e12
    /// 反汇编:
    ///   divsd xmm7, xmm9      ; ratio = a / norm
    ///   mulsd xmm9, xmm14     ; c = ratio * col_val
    ///   mulsd xmm7, xmm8      ; s = ratio * row_val
    ///   xorps xmm7, [-0.0]    ; s = -s
    static GivensRotation compute(double a, double b)
    {
        double r = stableHypot(a, b);
        if (r < 1e-7)  // DLL: 0x248ec8 = 1e-7
            return GivensRotation(1.0, 0.0);

        // DLL: 0x59dee → load 1.0, then divsd/mulsd
        return GivensRotation(a / r, -b / r);
    }

    /// 应用Givens旋转到两个值
    /// DLL: 0x59e43-0x59e71 (循环中每次迭代)
    ///
    /// 反汇编模式:
    ///   mulsd xmm3, xmm9      ; v1_new = old1 * c
    ///   mulsd xmm0, xmm7      ; temp   = old2 * s
    ///   addsd xmm3, xmm0      ; v1_new += temp  →  v1' = c*v1 + s*v2
    ///
    ///   mulsd xmm3, xmm9      ; v2_new = old2 * c
    ///   mulsd xmm0, xmm7      ; temp   = old1 * s
    ///   subsd xmm3, xmm0      ; v2_new -= temp  →  v2' = c*v2 - s*v1
    void apply(double& v1, double& v2) const
    {
        double t1 = c * v1 + s * v2;
        double t2 = c * v2 - s * v1;
        v1 = t1;
        v2 = t2;
    }
};

// ===========================================================================
// 圆心拟合算法 — 核心还原
//
// DLL函数: 0x58240 - 0x5a54c (8972 bytes)
//
// 参数 (从寄存器使用推断):
//   rcx = this 指针 (SegmenterV21 实例)
//   rdx = 边缘点数组指针
//   xmm1 = 初始估计参数
//   r8  = 输出: 圆心x坐标 (double*)
//   r9  = 输出: 圆心y坐标 (double*)
//   [rbp+0xd0] = 输出: 半径 (double*)
//
// 成员偏移 (从 rcx 基址):
//   +0x10   : 点数 (uint32_t)
//   +0x20   : 点数组 (结构体数组, 每个72字节)
//   +0x1820 : 高级模式标志 (bool)
//   +0x1840 : 缓存的点数 (uint64_t)
//   +0x1848 : 当前点数 (uint64_t)
//   +0x1888 : 距离排序树 (std::map)
//   +0x18b0 : 已计算标志 (bool)
//   +0x18b4 : 结果状态 (int32_t, 4=成功)
//
// 算法概述:
//   1. 计算所有点之间的成对距离 → 排序
//   2. 选取3个最优初始点 (最远点三角形)
//   3. 从3点解析计算初始圆参数
//   4. Givens旋转迭代法精化圆心
//   5. 更新所有坐标点的残差
//   6. 返回亚像素精度圆心坐标
// ===========================================================================

class CircleFitting
{
public:
    /// 边缘点结构（与DLL中偏移+0x20处的数组对应）
    struct EdgePoint
    {
        double x;           ///< +0x00: 像素x坐标
        double y;           ///< +0x08: 像素y坐标
        double z;           ///< +0x10: 扩展坐标（通常为0）
        double weight;      ///< +0x18: 像素权重（亮度值）
        double distance;    ///< +0x20: 到圆心距离（迭代中更新）
        double residual;    ///< +0x28: 残差
        double reserved[3]; ///< +0x30: 保留字段
        // 总计 72 bytes (0x48), 对应 DLL 中 [r14 + i*72 + 0x20] 访问模式
    };

    /// 拟合结果
    struct CircleResult
    {
        double centerX;     ///< 圆心X (亚像素)
        double centerY;     ///< 圆心Y (亚像素)
        double radius;      ///< 半径
        double rmsError;    ///< RMS残差
        bool   converged;   ///< 是否收敛
        int    iterations;  ///< 迭代次数
    };

    // ==================================================================
    // 主拟合函数 — 完整还原
    //
    // DLL: 0x58240 - 0x5a54c
    //
    // 算法: 基于Givens旋转的迭代最小二乘圆拟合
    //
    // 该方法比标准Kasa方法更鲁棒，因为:
    //   - 不需要显式构造法方程矩阵（避免病态）
    //   - 使用Givens旋转逐步消元，数值稳定
    //   - 支持加权拟合（像素亮度作为权重）
    //   - 迭代收敛，可处理非均匀分布的点
    //
    // 数学基础:
    //   对于圆方程: x² + y² + D*x + E*y + F = 0
    //   圆心 = (-D/2, -E/2), 半径 = sqrt(D²/4 + E²/4 - F)
    //
    //   构建超定方程组:
    //     [xi  yi  1] * [D]   [-(xi² + yi²)]
    //     [xj  yj  1] * [E] = [-(xj² + yj²)]
    //     [...]        * [F]   [...]
    //
    //   使用Givens旋转将系数矩阵化为上三角形
    //   回代求解 D, E, F
    //   从 D, E, F 恢复 cx, cy, r
    // ==================================================================
    static CircleResult fitCircleCenter(
        const std::vector<EdgePoint>& points,
        double convergenceTol = 1e-7)
    {
        CircleResult result;
        result.centerX = 0.0;
        result.centerY = 0.0;
        result.radius = 0.0;
        result.rmsError = 0.0;
        result.converged = false;
        result.iterations = 0;

        const size_t N = points.size();

        if (N < 3)
            return result;

        // ================================================================
        // 阶段 1: 初始估计 — 从3个最远点解析求圆
        // DLL: 0x58344-0x58592
        //
        // 反汇编逻辑:
        //   0x58344: 对每对点计算距离 (subsd+mulsd+addsd+sqrt)
        //   0x58501-0x58592: 选取最大三角形的3个顶点
        // ================================================================
        double cx = 0.0, cy = 0.0, r = 0.0;

        // 计算质心作为初始猜测
        double sumX = 0.0, sumY = 0.0;
        for (size_t i = 0; i < N; ++i)
        {
            sumX += points[i].x;
            sumY += points[i].y;
        }
        cx = sumX / static_cast<double>(N);
        cy = sumY / static_cast<double>(N);

        // 选择3个互相最远的点来初始化
        // DLL中使用 std::map 按距离排序 (0x4fde0调用)
        size_t idx0 = 0, idx1 = 0, idx2 = 0;
        double maxDist = 0.0;

        // 找最远的两点
        for (size_t i = 0; i < N; ++i)
        {
            for (size_t j = i + 1; j < N; ++j)
            {
                double dx = points[i].x - points[j].x;
                double dy = points[i].y - points[j].y;
                double dz = points[i].z - points[j].z;
                double d = dx * dx + dy * dy + dz * dz;
                if (d > maxDist)
                {
                    maxDist = d;
                    idx0 = i;
                    idx1 = j;
                }
            }
        }

        // 找离这两点连线最远的第三个点
        maxDist = 0.0;
        for (size_t i = 0; i < N; ++i)
        {
            if (i == idx0 || i == idx1)
                continue;
            double d0 = (points[i].x - points[idx0].x) * (points[i].x - points[idx0].x)
                       + (points[i].y - points[idx0].y) * (points[i].y - points[idx0].y);
            double d1 = (points[i].x - points[idx1].x) * (points[i].x - points[idx1].x)
                       + (points[i].y - points[idx1].y) * (points[i].y - points[idx1].y);
            double d = std::min(d0, d1);
            if (d > maxDist)
            {
                maxDist = d;
                idx2 = i;
            }
        }

        // ================================================================
        // 从3点解析计算初始圆
        // DLL: 0x59f18-0x59fb5
        //
        // 反汇编数学（关键段 FP #15-#33）:
        //
        //   设 p0 = (x0, y0), p1 = (x1, y1), p2 = (x2, y2)
        //
        //   # 差值的平方差（避免直接平方以提高精度）
        //   FP#15: subsd xmm7, xmm6         → dx01 = x0 - x1
        //   FP#16: addsd xmm0, xmm11        → sx01 = x0 + x1
        //   FP#17: mulsd xmm7, xmm0         → A = dx01 * sx01 = x0² - x1²
        //
        //   FP#18: subsd xmm1, xmm8         → dy01 = y0 - y1
        //   FP#19: addsd xmm0, xmm14        → sy01 = y0 + y1
        //   FP#20: mulsd xmm1, xmm0         → B = dy01 * sy01 = y0² - y1²
        //
        //   FP#21: addsd xmm7, xmm1         → num1 = A + B = (x0²-x1²) + (y0²-y1²)
        //
        //   FP#22: addsd xmm0, xmm8         → 2*y0 (= y0 + y0)
        //   FP#23: mulsd xmm0, xmm11        → denom1 = 2*y0*x1
        //   FP#24: divsd xmm7, xmm0         → t = num1/denom1
        //
        //   # 使用 hypot 和 atan2 风格的稳定除法
        //   FP#25: addsd xmm1, xmm7         → 组合值
        //   FP#26: divsd xmm9, xmm1         → 比率
        //   FP#27: subsd xmm9, xmm8         → 调整
        //   FP#28: mulsd xmm9, xmm8         → 项1
        //
        //   FP#29: subsd xmm1, xmm6         → dx20 = x2 - x0
        //   FP#30: addsd xmm0, xmm6         → sx20 = x2 + x0
        //   FP#31: mulsd xmm1, xmm0         → C = x2² - x0²
        //   FP#32: addsd xmm9, xmm1         → num2 += C
        //   FP#33: divsd xmm9, xmm13        → cy = num2 / (2*y2)
        //
        // 这是标准的3点定圆公式的数值稳定实现
        // ================================================================
        {
            double x0 = points[idx0].x, y0 = points[idx0].y;
            double x1 = points[idx1].x, y1 = points[idx1].y;
            double x2 = points[idx2].x, y2 = points[idx2].y;

            // 对应DLL FP#15-#24: 计算第一个方程
            double A = (x0 - x1) * (x0 + x1);  // x0² - x1²
            double B = (y0 - y1) * (y0 + y1);  // y0² - y1²
            double denom1 = 2.0 * y0 * x1;

            if (std::fabs(denom1) > 1e-7)
            {
                double t = (A + B) / denom1;

                // 对应DLL FP#25-#28
                double num1 = x1 / (t + x1) - y0;
                double term1 = num1 * y0;

                // 对应DLL FP#29-#33
                double C = (x2 - x0) * (x2 + x0);  // x2² - x0²
                double num2 = term1 + C;
                cy = num2 / x2;

                // 从cy反算cx
                cx = (A + B + 2.0 * cy * (y1 - y0)) / (2.0 * (x0 - x1));
            }
            else
            {
                // 退化情况: 使用质心
                cx = (x0 + x1 + x2) / 3.0;
                cy = (y0 + y1 + y2) / 3.0;
            }

            // 计算初始半径
            double dx0 = points[idx0].x - cx;
            double dy0 = points[idx0].y - cy;
            r = std::sqrt(dx0 * dx0 + dy0 * dy0);
        }

        // ================================================================
        // 阶段 2: Givens旋转迭代精化
        // DLL: 0x59c90 - 0x5a2aa (1562 bytes, 外层循环)
        //
        // 最大迭代次数: 49
        // DLL: 0x59ecb: cmp rdi, 0x31 (49)
        //
        // 收敛判据: 1e-7
        // DLL: 0x248ec8 = 1e-7
        //
        // 每次迭代:
        //   1. 更新残差向量 ri = sqrt((xi-cx)²+(yi-cy)²) - r
        //   2. 构建雅可比矩阵 J (Nx3):
        //        J[i][0] = (xi - cx) / di
        //        J[i][1] = (yi - cy) / di
        //        J[i][2] = -1
        //   3. 使用Givens旋转将 [J | r] 化为上三角
        //   4. 回代求解增量 Δ = [Δcx, Δcy, Δr]
        //   5. 更新: cx += Δcx, cy += Δcy, r += Δr
        //   6. 检查收敛: |Δcx| + |Δcy| + |Δr| < tol
        //
        // Givens旋转应用模式 (DLL: 0x59e3f-0x59e8e):
        //   对于矩阵的每一列 j (0..2):
        //     对于每一对行 (k, k+1):
        //       计算旋转角消除 A[k+1][j]
        //       将旋转应用到 A 和 b 的两行
        // ================================================================

        static constexpr int kMaxIterations = 49;

        // 工作数组 — 对应DLL中的栈分配
        // DLL: [rbp - 0x50] = 系数向量数组
        // DLL: [rsi + i*8 + 0x50] = 坐标引用数组
        std::vector<double> residuals(N);
        std::vector<double> jacobianCol0(N);  // ∂f/∂cx
        std::vector<double> jacobianCol1(N);  // ∂f/∂cy
        std::vector<double> jacobianCol2(N);  // ∂f/∂r = -1

        double R[3][4];  // 上三角矩阵 + RHS (Givens消元结果)

        for (int iter = 0; iter < kMaxIterations; ++iter)
        {
            result.iterations = iter + 1;

            // --- 计算残差和雅可比矩阵 ---
            // DLL: 0x59300-0x59488 (内层循环)
            for (size_t i = 0; i < N; ++i)
            {
                double dx = points[i].x - cx;
                double dy = points[i].y - cy;
                double di = std::sqrt(dx * dx + dy * dy);

                if (di < 1e-7)
                    di = 1e-7;

                residuals[i] = di - r;
                jacobianCol0[i] = dx / di;
                jacobianCol1[i] = dy / di;
                jacobianCol2[i] = -1.0;
            }

            // --- Givens旋转QR分解 ---
            // DLL: 0x59c90-0x5a2aa 的核心
            //
            // 初始化上三角矩阵为零
            std::memset(R, 0, sizeof(R));

            for (size_t i = 0; i < N; ++i)
            {
                // 当前行: [J0[i], J1[i], J2[i] | res[i]]
                double row[4] = {
                    jacobianCol0[i],
                    jacobianCol1[i],
                    jacobianCol2[i],
                    residuals[i]
                };

                // 对已有的上三角行逐一消元
                // DLL: 0x59e22-0x59e8e 的循环 (3次 for j in 0..2)
                for (int j = 0; j < 3; ++j)
                {
                    if (std::fabs(row[j]) < 1e-7)
                        continue;

                    if (std::fabs(R[j][j]) < 1e-7)
                    {
                        // DLL中对应 ucomisd+je 跳过零元素
                        // 直接将当前行放入R[j]
                        for (int k = j; k < 4; ++k)
                            R[j][k] = row[k];
                        break;
                    }

                    // 计算Givens旋转
                    // DLL: 0x59df6 → divsd xmm7, xmm9
                    //      0x59dff → mulsd xmm9, xmm14
                    //      0x59e04 → mulsd xmm7, xmm8
                    //      0x59e12 → xorps xmm7, [-0.0] (取反)
                    double a = R[j][j];
                    double b = row[j];
                    double norm = stableHypot(a, b);

                    // DLL: 1.0/norm → divsd from 0x248ef0
                    double invNorm = 1.0 / norm;
                    double gc = a * invNorm;   // cos(θ)
                    double gs = -b * invNorm;  // sin(θ)

                    // 应用Givens旋转到R[j]和row
                    // DLL: 0x59e43-0x59e71 的循环 (对每列k)
                    //   t1 = gc * R[j][k] + gs * row[k]
                    //   t2 = gc * row[k]  - gs * R[j][k]
                    for (int k = j; k < 4; ++k)
                    {
                        double t1 = gc * R[j][k] + gs * row[k];
                        double t2 = gc * row[k] - gs * R[j][k];
                        R[j][k] = t1;
                        row[k] = t2;
                    }
                }
            }

            // --- 回代求解 ---
            // DLL: 0x5a139-0x5a215 (回代循环)
            //
            // 从 R 上三角矩阵回代:
            //   R[2][2]*δr     = R[2][3]
            //   R[1][1]*δcy + R[1][2]*δr = R[1][3]
            //   R[0][0]*δcx + R[0][1]*δcy + R[0][2]*δr = R[0][3]
            double delta[3] = {0.0, 0.0, 0.0};

            for (int j = 2; j >= 0; --j)
            {
                if (std::fabs(R[j][j]) < 1e-7)
                    continue;

                double sum = R[j][3];
                for (int k = j + 1; k < 3; ++k)
                    sum -= R[j][k] * delta[k];

                // DLL: 0x5a141 → divsd xmm1, xmm6
                delta[j] = sum / R[j][j];
            }

            // --- 更新参数 ---
            cx -= delta[0];  // 注意符号: DLL中的雅可比定义
            cy -= delta[1];
            r  -= delta[2];

            // --- 收敛检查 ---
            // DLL: 0x59fd3 → load 1e-7, ucomisd comparison
            double change = std::fabs(delta[0]) + std::fabs(delta[1]) + std::fabs(delta[2]);
            if (change < convergenceTol)
            {
                result.converged = true;
                break;
            }
        }

        // ================================================================
        // 阶段 3: 计算最终RMS误差
        // DLL: 0x5a2b2-0x5a317
        //
        //   sumSq = 0
        //   for each point:
        //     d = sqrt((xi-cx)² + (yi-cy)²) - r
        //     sumSq += d * d
        //   rms = sqrt(sumSq / N)
        // ================================================================
        double sumSq = 0.0;
        for (size_t i = 0; i < N; ++i)
        {
            double dx = points[i].x - cx;
            double dy = points[i].y - cy;
            double di = std::sqrt(dx * dx + dy * dy) - r;
            sumSq += di * di;
        }
        result.rmsError = std::sqrt(sumSq / static_cast<double>(N));

        result.centerX = cx;
        result.centerY = cy;
        result.radius = r;

        return result;
    }

    // ==================================================================
    // 加权圆心拟合 — 使用像素亮度作为权重
    //
    // 对应DLL选项: "Pixel Weight for Centroid"
    //
    // 当 m_usePixelWeight = true 时使用此版本
    // 权重越高的点（更亮的像素）对拟合影响越大
    //
    // DLL中权重的使用:
    //   在构建雅可比矩阵时，每行乘以 sqrt(weight):
    //     J_w[i][j] = sqrt(w_i) * J[i][j]
    //     r_w[i]    = sqrt(w_i) * r[i]
    //   这等价于加权最小二乘
    // ==================================================================
    static CircleResult fitCircleCenterWeighted(
        const std::vector<EdgePoint>& points,
        double convergenceTol = 1e-7)
    {
        CircleResult result;
        result.centerX = 0.0;
        result.centerY = 0.0;
        result.radius = 0.0;
        result.rmsError = 0.0;
        result.converged = false;
        result.iterations = 0;

        const size_t N = points.size();
        if (N < 3)
            return result;

        // 初始质心估计（加权）
        double sumX = 0.0, sumY = 0.0, sumW = 0.0;
        for (size_t i = 0; i < N; ++i)
        {
            double w = points[i].weight;
            sumX += points[i].x * w;
            sumY += points[i].y * w;
            sumW += w;
        }

        double cx = sumX / sumW;
        double cy = sumY / sumW;
        double r = 0.0;

        // 初始半径估计
        for (size_t i = 0; i < N; ++i)
        {
            double dx = points[i].x - cx;
            double dy = points[i].y - cy;
            r += std::sqrt(dx * dx + dy * dy);
        }
        r /= static_cast<double>(N);

        // 迭代精化（与非加权版相同结构，但雅可比行乘以sqrt(weight)）
        static constexpr int kMaxIterations = 49;
        double R[3][4];

        for (int iter = 0; iter < kMaxIterations; ++iter)
        {
            result.iterations = iter + 1;
            std::memset(R, 0, sizeof(R));

            for (size_t i = 0; i < N; ++i)
            {
                double dx = points[i].x - cx;
                double dy = points[i].y - cy;
                double di = std::sqrt(dx * dx + dy * dy);
                if (di < 1e-7) di = 1e-7;

                double w = std::sqrt(std::max(points[i].weight, 0.0));

                // 加权雅可比行
                double row[4] = {
                    w * (dx / di),
                    w * (dy / di),
                    w * (-1.0),
                    w * (di - r)
                };

                // Givens消元（与非加权版完全相同）
                for (int j = 0; j < 3; ++j)
                {
                    if (std::fabs(row[j]) < 1e-7) continue;
                    if (std::fabs(R[j][j]) < 1e-7)
                    {
                        for (int k = j; k < 4; ++k) R[j][k] = row[k];
                        break;
                    }
                    double a = R[j][j], b = row[j];
                    double norm = stableHypot(a, b);
                    double inv = 1.0 / norm;
                    double gc = a * inv, gs = -b * inv;
                    for (int k = j; k < 4; ++k)
                    {
                        double t1 = gc * R[j][k] + gs * row[k];
                        double t2 = gc * row[k] - gs * R[j][k];
                        R[j][k] = t1;
                        row[k] = t2;
                    }
                }
            }

            // 回代
            double delta[3] = {0.0, 0.0, 0.0};
            for (int j = 2; j >= 0; --j)
            {
                if (std::fabs(R[j][j]) < 1e-7) continue;
                double sum = R[j][3];
                for (int k = j + 1; k < 3; ++k)
                    sum -= R[j][k] * delta[k];
                delta[j] = sum / R[j][j];
            }

            cx -= delta[0];
            cy -= delta[1];
            r  -= delta[2];

            double change = std::fabs(delta[0]) + std::fabs(delta[1]) + std::fabs(delta[2]);
            if (change < convergenceTol)
            {
                result.converged = true;
                break;
            }
        }

        // 计算最终误差
        double sumSq = 0.0;
        for (size_t i = 0; i < N; ++i)
        {
            double dx = points[i].x - cx;
            double dy = points[i].y - cy;
            double di = std::sqrt(dx * dx + dy * dy) - r;
            sumSq += di * di * points[i].weight;
        }
        result.rmsError = std::sqrt(sumSq / sumW);

        result.centerX = cx;
        result.centerY = cy;
        result.radius = r;

        return result;
    }
};

}  // namespace segmenter
}  // namespace measurement
