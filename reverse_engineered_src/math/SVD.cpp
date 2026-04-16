// ===========================================================================
// 逆向工程还原 — SVD 分解 + Kabsch 配准算法实现
// 来源: fusionTrack64.dll
//
// Kabsch 算法被 MatchMarkers 使用来计算刚体变换
// DLL字符串: "Cannot perform registration with "
// ===========================================================================

#include "Matrix.h"
#include <cmath>
#include <algorithm>

namespace measurement {
namespace math {

// ---------------------------------------------------------------------------
// 3x3 SVD — Jacobi 旋转法
//
// 输入: A (3x3矩阵)
// 输出: U, S, V 使得 A ≈ U * diag(S) * V^T
//
// 使用 One-sided Jacobi SVD 算法
// 对于3x3矩阵，精度足够且效率高
// ---------------------------------------------------------------------------

static void _jacobiRotation(Mat3d& A, Mat3d& V, int p, int q)
{
    // 计算 2x2 子矩阵的 SVD 旋转角
    // 列 p 和 q 的 Gram 矩阵:
    // [a  b]   [Ap·Ap   Ap·Aq]
    // [b  c] = [Aq·Ap   Aq·Aq]

    double a = 0, b = 0, c = 0;
    for (int i = 0; i < 3; ++i)
    {
        a += A(i, p) * A(i, p);
        b += A(i, p) * A(i, q);
        c += A(i, q) * A(i, q);
    }

    // 如果 off-diagonal 元素足够小，跳过
    if (std::abs(b) < 1e-15 * std::sqrt(a * c))
        return;

    // 计算旋转角
    double tau = (c - a) / (2.0 * b);
    double t;
    if (tau >= 0)
        t = 1.0 / (tau + std::sqrt(1.0 + tau * tau));
    else
        t = -1.0 / (-tau + std::sqrt(1.0 + tau * tau));

    double cosTheta = 1.0 / std::sqrt(1.0 + t * t);
    double sinTheta = t * cosTheta;

    // 应用旋转到 A 的列: A' = A * G(p, q, theta)
    for (int i = 0; i < 3; ++i)
    {
        double ap = A(i, p);
        double aq = A(i, q);
        A(i, p) = cosTheta * ap - sinTheta * aq;
        A(i, q) = sinTheta * ap + cosTheta * aq;
    }

    // 累积到 V
    for (int i = 0; i < 3; ++i)
    {
        double vp = V(i, p);
        double vq = V(i, q);
        V(i, p) = cosTheta * vp - sinTheta * vq;
        V(i, q) = sinTheta * vp + cosTheta * vq;
    }
}

void svd3x3(const Mat3d& A, Mat3d& U, Vec3d& S, Mat3d& V)
{
    // 复制 A → B (工作矩阵)
    Mat3d B = A;
    V = Mat3d::identity();

    // Jacobi 迭代（One-sided Jacobi SVD）
    // 对 3x3 矩阵，通常 5-10 次迭代即可收敛
    for (int iter = 0; iter < 30; ++iter)
    {
        // 对所有 (p, q) 对执行 Jacobi 旋转
        _jacobiRotation(B, V, 0, 1);
        _jacobiRotation(B, V, 0, 2);
        _jacobiRotation(B, V, 1, 2);

        // 检查收敛: off-diagonal 元素足够小
        double offDiag = 0;
        for (int j = 0; j < 3; ++j)
        {
            double colNorm = 0;
            for (int i = 0; i < 3; ++i)
                colNorm += B(i, j) * B(i, j);
            for (int k = j + 1; k < 3; ++k)
            {
                double dot = 0;
                for (int i = 0; i < 3; ++i)
                    dot += B(i, j) * B(i, k);
                offDiag += dot * dot;
            }
        }

        if (offDiag < 1e-30)
            break;
    }

    // 提取奇异值和 U
    // B = U * diag(S)  →  U[:,j] = B[:,j] / S[j]
    for (int j = 0; j < 3; ++j)
    {
        double sigma = 0;
        for (int i = 0; i < 3; ++i)
            sigma += B(i, j) * B(i, j);
        sigma = std::sqrt(sigma);

        S[j] = sigma;

        if (sigma > 1e-15)
        {
            for (int i = 0; i < 3; ++i)
                U(i, j) = B(i, j) / sigma;
        }
        else
        {
            for (int i = 0; i < 3; ++i)
                U(i, j) = (i == j) ? 1.0 : 0.0;
        }
    }

    // 确保 U 和 V 是正交矩阵
    // 对 U 进行 Gram-Schmidt 正交化
    for (int j = 0; j < 3; ++j)
    {
        for (int k = 0; k < j; ++k)
        {
            double dot = 0;
            for (int i = 0; i < 3; ++i)
                dot += U(i, j) * U(i, k);
            for (int i = 0; i < 3; ++i)
                U(i, j) -= dot * U(i, k);
        }
        double norm = 0;
        for (int i = 0; i < 3; ++i)
            norm += U(i, j) * U(i, j);
        norm = std::sqrt(norm);
        if (norm > 1e-15)
        {
            for (int i = 0; i < 3; ++i)
                U(i, j) /= norm;
        }
    }
}

// ---------------------------------------------------------------------------
// Kabsch 配准算法
// ---------------------------------------------------------------------------

double kabschRegistration(
    const Vec3d* P, const Vec3d* Q,
    unsigned int numPoints,
    Mat3d& R, Vec3d& t)
{
    if (numPoints < 3)
        return -1.0;

    // 步骤 1: 计算质心
    Vec3d centroidP, centroidQ;
    for (unsigned int i = 0; i < numPoints; ++i)
    {
        centroidP = centroidP + P[i];
        centroidQ = centroidQ + Q[i];
    }
    double invN = 1.0 / numPoints;
    centroidP = centroidP * invN;
    centroidQ = centroidQ * invN;

    // 步骤 2: 去中心化并构建协方差矩阵
    Mat3d H;
    for (unsigned int i = 0; i < numPoints; ++i)
    {
        Vec3d p = P[i] - centroidP;
        Vec3d q = Q[i] - centroidQ;

        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                H(r, c) += p[r] * q[c];
    }

    // 步骤 3: SVD 分解
    Mat3d U, V;
    Vec3d S;
    svd3x3(H, U, S, V);

    // 步骤 4: 最优旋转 R = V * U^T
    R = V * U.transpose();

    // 检查行列式 — 确保是旋转而非反射
    double det = R.determinant3x3();
    if (det < 0)
    {
        // 翻转 V 的最后一列
        for (int i = 0; i < 3; ++i)
            V(i, 2) = -V(i, 2);
        R = V * U.transpose();
    }

    // 步骤 5: 最优平移 t = centroid_Q - R * centroid_P
    t = centroidQ - R * centroidP;

    // 步骤 6: 计算 RMSE
    double totalError = 0.0;
    for (unsigned int i = 0; i < numPoints; ++i)
    {
        Vec3d transformed = R * P[i] + t;
        Vec3d diff = transformed - Q[i];
        totalError += diff.dot(diff);
    }

    return std::sqrt(totalError / numPoints);
}

}  // namespace math
}  // namespace measurement
