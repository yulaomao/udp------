// ===========================================================================
// 逆向工程还原 — 来源: fusionTrack64.dll RTTI
// RTTI类名: .?AV?$Matrix@N$0R$0C@@math@measurement@@  (Matrix<double, ROWS, COLS>)
// 命名空间: math::measurement
//
// DLL中发现的矩阵实例化: 2x2, 3x3, 3x4...3x64, 4x3, 5x3...64x3
// 这表明核心算法使用最多64个fiducial点（最大64x3的雅可比矩阵）
// ===========================================================================

#pragma once

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <array>

namespace measurement {
namespace math {

/// 列向量模板 — RTTI: .?AV?$ColVector@N$0X@@math@measurement@@
/// 发现的实例: ColVector<double, 2>, ColVector<double, 3>, ColVector<double, 4>
template <typename T, unsigned int N>
class ColVector
{
public:
    ColVector()
    {
        std::memset(m_data, 0, sizeof(m_data));
    }

    explicit ColVector(const T* src)
    {
        std::memcpy(m_data, src, sizeof(m_data));
    }

    T& operator[](unsigned int i) { return m_data[i]; }
    const T& operator[](unsigned int i) const { return m_data[i]; }

    /// 向量点积
    T dot(const ColVector& other) const
    {
        T result = T(0);
        for (unsigned int i = 0u; i < N; ++i)
            result += m_data[i] * other.m_data[i];
        return result;
    }

    /// 向量范数
    T norm() const
    {
        return std::sqrt(dot(*this));
    }

    /// 向量归一化
    ColVector normalized() const
    {
        T n = norm();
        if (n < T(1e-15))
            throw std::runtime_error("Cannot normalize zero vector");
        ColVector result;
        for (unsigned int i = 0u; i < N; ++i)
            result.m_data[i] = m_data[i] / n;
        return result;
    }

    /// 标量乘法
    ColVector operator*(T s) const
    {
        ColVector result;
        for (unsigned int i = 0u; i < N; ++i)
            result.m_data[i] = m_data[i] * s;
        return result;
    }

    /// 向量加法
    ColVector operator+(const ColVector& o) const
    {
        ColVector result;
        for (unsigned int i = 0u; i < N; ++i)
            result.m_data[i] = m_data[i] + o.m_data[i];
        return result;
    }

    /// 向量减法
    ColVector operator-(const ColVector& o) const
    {
        ColVector result;
        for (unsigned int i = 0u; i < N; ++i)
            result.m_data[i] = m_data[i] - o.m_data[i];
        return result;
    }

    const T* data() const { return m_data; }
    T* data() { return m_data; }

    static constexpr unsigned int size() { return N; }

private:
    T m_data[N];
};

/// 3D向量叉积（仅用于 ColVector<T,3>）
template <typename T>
ColVector<T, 3> cross(const ColVector<T, 3>& a, const ColVector<T, 3>& b)
{
    ColVector<T, 3> result;
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
    return result;
}

using Vec2d = ColVector<double, 2>;
using Vec3d = ColVector<double, 3>;
using Vec4d = ColVector<double, 4>;

// ---------------------------------------------------------------------------
/// 矩阵模板 — RTTI: .?AV?$Matrix@N$0R$0C@@math@measurement@@
/// 发现的实例化范围: Matrix<double, 2, 2> 到 Matrix<double, 64, 3>
template <typename T, unsigned int ROWS, unsigned int COLS>
class Matrix
{
public:
    Matrix()
    {
        std::memset(m_data, 0, sizeof(m_data));
    }

    T& operator()(unsigned int r, unsigned int c)
    {
        return m_data[r * COLS + c];
    }

    const T& operator()(unsigned int r, unsigned int c) const
    {
        return m_data[r * COLS + c];
    }

    /// 矩阵乘法
    template <unsigned int COLS2>
    Matrix<T, ROWS, COLS2> operator*(const Matrix<T, COLS, COLS2>& rhs) const
    {
        Matrix<T, ROWS, COLS2> result;
        for (unsigned int i = 0u; i < ROWS; ++i)
            for (unsigned int j = 0u; j < COLS2; ++j)
            {
                T sum = T(0);
                for (unsigned int k = 0u; k < COLS; ++k)
                    sum += (*this)(i, k) * rhs(k, j);
                result(i, j) = sum;
            }
        return result;
    }

    /// 矩阵-向量乘法
    ColVector<T, ROWS> operator*(const ColVector<T, COLS>& v) const
    {
        ColVector<T, ROWS> result;
        for (unsigned int i = 0u; i < ROWS; ++i)
        {
            T sum = T(0);
            for (unsigned int j = 0u; j < COLS; ++j)
                sum += (*this)(i, j) * v[j];
            result[i] = sum;
        }
        return result;
    }

    /// 转置
    Matrix<T, COLS, ROWS> transpose() const
    {
        Matrix<T, COLS, ROWS> result;
        for (unsigned int i = 0u; i < ROWS; ++i)
            for (unsigned int j = 0u; j < COLS; ++j)
                result(j, i) = (*this)(i, j);
        return result;
    }

    /// 单位矩阵（仅方阵）
    static Matrix identity()
    {
        static_assert(ROWS == COLS, "Identity only for square matrices");
        Matrix result;
        for (unsigned int i = 0u; i < ROWS; ++i)
            result(i, i) = T(1);
        return result;
    }

    /// 3x3行列式
    T determinant3x3() const
    {
        static_assert(ROWS == 3 && COLS == 3, "Only for 3x3");
        return (*this)(0, 0) * ((*this)(1, 1) * (*this)(2, 2) - (*this)(1, 2) * (*this)(2, 1))
             - (*this)(0, 1) * ((*this)(1, 0) * (*this)(2, 2) - (*this)(1, 2) * (*this)(2, 0))
             + (*this)(0, 2) * ((*this)(1, 0) * (*this)(2, 1) - (*this)(1, 1) * (*this)(2, 0));
    }

    const T* data() const { return m_data; }
    T* data() { return m_data; }

    static constexpr unsigned int rows() { return ROWS; }
    static constexpr unsigned int cols() { return COLS; }

private:
    T m_data[ROWS * COLS];
};

using Mat2d = Matrix<double, 2, 2>;
using Mat3d = Matrix<double, 3, 3>;
using Mat4d = Matrix<double, 4, 4>;
using Mat3x4d = Matrix<double, 3, 4>;

// ---------------------------------------------------------------------------
/// SVD分解（用于Kabsch配准算法）
/// 从MatchMarkers类中的刚体配准推断需要3x3 SVD
///
/// 基于 Jacobi 旋转的3x3 SVD 分解
/// 输入: A = U * S * V^T
void svd3x3(const Mat3d& A, Mat3d& U, Vec3d& S, Mat3d& V);

/// Kabsch 算法：计算两组对应3D点之间的最优刚体变换
/// 从 DLL 字符串 "Cannot perform registration with" 推断
/// 最小化 sum_i || R * pi + t - qi ||^2
///
/// @param P 源点集（N x 3）
/// @param Q 目标点集（N x 3）
/// @param numPoints 点数（至少3个）
/// @param R [out] 3x3旋转矩阵
/// @param t [out] 3D平移向量
/// @return 配准均方根误差
double kabschRegistration(const Vec3d* P, const Vec3d* Q,
                          unsigned int numPoints,
                          Mat3d& R, Vec3d& t);

}  // namespace math
}  // namespace measurement
