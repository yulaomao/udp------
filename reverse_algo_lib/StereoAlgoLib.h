// ===========================================================================
// StereoAlgoLib.h - 独立逆向工程双目视觉算法库
//
// 基于 verify_reverse_engineered.py 中验证通过的算法实现
// + libfusionTrack64.so 反汇编分析优化
// 所有算法已通过 dump_output/compare_output 数据验证与 SDK 输出匹配
//
// 独立命名空间 stereo_algo，避免与 SDK 或其他逆向代码冲突
//
// 关键逆向修复 (v2.1.0):
//   - 极线匹配: SDK 默认仅前向极线验证 (symmetriseCoords=0)
//   - 极线阈值: SDK 默认 5.0 px (cstEpipolarDistDef)
//   - 标定参数: 全部使用 float32 精度 (Rodrigues, K矩阵, t向量)
//
// 验证精度:
//   - 重投影:    mean 0.002/0.005 px (左/右)
//   - 三角化:    mean 0.024 mm (好品质匹配)
//   - 极线误差:  mean 0.001 px (相关系数 1.0)
//   - 去畸变:    精确匹配 (round-trip 0.0)
//   - 工具识别:  mean 0.0002 mm 平移, 0.0000014 旋转
//   - 圆心检测:  mean 0.000388 px, 99.4% < 0.01 px
//
// 用法:
//   stereo_algo::StereoVision sv;
//   sv.initialize(calibration);
//   sv.triangulatePoint(lx, ly, rx, ry, &pos3d, &epiErr, &triErr);
//   sv.reprojectTo2D(pos3d, &leftPx, &rightPx);
// ===========================================================================

#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace stereo_algo {

// ===========================================================================
// 基础数据结构 - 独立于 SDK 的轻量级类型
// ===========================================================================

/// 2D 向量
struct Vec2 {
    double x = 0.0, y = 0.0;
};

/// 3D 向量
struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

/// 3x3 矩阵 (行主序)
struct Mat3 {
    double m[3][3] = {};

    Mat3() = default;

    static Mat3 identity() {
        Mat3 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = 1.0;
        return r;
    }

    /// 矩阵-向量乘法
    Vec3 mul(const Vec3& v) const {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        };
    }

    /// 矩阵-矩阵乘法
    Mat3 mul(const Mat3& b) const {
        Mat3 r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    r.m[i][j] += m[i][k] * b.m[k][j];
        return r;
    }

    /// 转置
    Mat3 transpose() const {
        Mat3 r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                r.m[i][j] = m[j][i];
        return r;
    }
};

// ===========================================================================
// 标定参数 - 与 SDK 的 ftkCameraParameters / ftkStereoParameters 对应
// ===========================================================================

/// 单相机内参
struct CameraIntrinsics {
    double focalLength[2] = {};    // fx, fy
    double opticalCentre[2] = {};  // cx, cy
    double distortion[5] = {};     // k1, k2, p1, p2, k3
    double skew = 0.0;
};

/// 双目系统标定参数
struct StereoCalibration {
    CameraIntrinsics leftCam;
    CameraIntrinsics rightCam;
    double translation[3] = {};    // 右相机在左相机坐标系的平移
    double rotation[3] = {};       // Rodrigues 旋转向量
};

// ===========================================================================
// 三角化结果
// ===========================================================================

struct TriangulationResult {
    Vec3 position;              // 3D 坐标 (mm)
    double epipolarError;       // 极线误差 (pixels)
    double triangulationError;  // 三角化误差 (mm)
    bool success;
};

// ===========================================================================
// 重投影结果
// ===========================================================================

struct ReprojectionResult {
    Vec2 leftPixel;     // 左图投影 (pixels)
    Vec2 rightPixel;    // 右图投影 (pixels)
    bool success;
};

// ===========================================================================
// Blob 检测结果
// ===========================================================================

struct BlobDetection {
    float centerX;      // 质心 X (pixels)
    float centerY;      // 质心 Y (pixels)
    uint32_t area;      // 面积 (pixels)
    uint16_t width;     // 边界框宽度
    uint16_t height;    // 边界框高度
};

// ===========================================================================
// Kabsch 配准结果
// ===========================================================================

struct RegistrationResult {
    Mat3 rotation;
    Vec3 translation;
    double rmsError;
    bool success;
};

// ===========================================================================
// 极线匹配结果 - 单个匹配对
// ===========================================================================

struct EpipolarMatchResult {
    uint32_t leftIndex;         // 左图检测索引
    uint32_t rightIndex;        // 右图检测索引
    Vec3 position;              // 三角化 3D 坐标 (mm)
    double epipolarError;       // 极线误差 (pixels)
    double triangulationError;  // 三角化误差 (mm)
    double probability;         // 候选歧义度 (1/n)
};

// ===========================================================================
// 2D 检测点输入 - 用于极线匹配
// ===========================================================================

struct Detection2D {
    double centerX;     // 像素 X
    double centerY;     // 像素 Y
    uint32_t index;     // 原始检测索引
};

// ===========================================================================
// StereoVision - 核心算法引擎
// ===========================================================================

class StereoVision {
public:
    StereoVision();
    ~StereoVision();

    /// 从标定参数初始化
    bool initialize(const StereoCalibration& cal);

    /// 检查是否已初始化
    bool isInitialized() const { return m_initialized; }

    // ------------------------------------------------------------------
    // 核心算法
    // ------------------------------------------------------------------

    /// 单点三角化
    /// @param leftPx, leftPy   左图像素坐标
    /// @param rightPx, rightPy 右图像素坐标
    /// @return 三角化结果
    TriangulationResult triangulatePoint(
        double leftPx, double leftPy,
        double rightPx, double rightPy) const;

    /// 3D 点重投影到左右相机
    /// @param pos3d 3D 坐标 (mm)
    /// @return 重投影结果
    ReprojectionResult reprojectTo2D(const Vec3& pos3d) const;

    /// 极线匹配+三角化 - 还原 DLL Match2D3D 完整管线
    ///
    /// 通过 libfusionTrack64.so 反汇编还原 (_findRightEpipolarMatch):
    ///   1. 阈值筛选: |前向极线距离| < epipolarMaxDistance (仅前向)
    ///   2. 全候选输出: 所有通过验证的候选对均保留 (允许一对多)
    ///      probability = 1/n (n = 同一左图点的候选数)
    ///
    /// SDK 默认参数 (从二进制常量读取):
    ///   cstEpipolarDistDef = 5.0, symmetriseCoords = 0 (无反向验证)
    ///
    /// @param leftDets   左图 2D 检测点
    /// @param leftCount  左图检测数
    /// @param rightDets  右图 2D 检测点
    /// @param rightCount 右图检测数
    /// @param outResults 输出匹配结果
    /// @param maxResults 最大输出数量
    /// @param epipolarMaxDistance 极线最大距离 (pixels, SDK 默认 5.0)
    /// @return 实际匹配对数
    uint32_t matchEpipolar(
        const Detection2D* leftDets, uint32_t leftCount,
        const Detection2D* rightDets, uint32_t rightCount,
        EpipolarMatchResult* outResults, uint32_t maxResults,
        double epipolarMaxDistance = 5.0) const;

    /// 计算反向极线 (F^T * right_ideal_pixel) - 用于右->左交叉验证
    Vec3 computeReverseEpipolarLine(double rightNormX, double rightNormY) const;

    /// 点到左图极线距离 (反向) - 使用 K_L 变换
    double pointToReverseEpipolarDistance(double leftNormX, double leftNormY,
                                          const Vec3& epipolarLine) const;

    // ------------------------------------------------------------------
    // 低级算法 - 对外暴露以便测试和对比
    // ------------------------------------------------------------------

    /// 去畸变: 像素坐标 -> 归一化坐标 (迭代 Brown-Conrady, 20次)
    Vec2 undistortPoint(double px, double py,
                        const CameraIntrinsics& cam) const;

    /// 加畸变: 归一化坐标 -> 像素坐标 (正向 Brown-Conrady)
    Vec2 distortPoint(double xn, double yn,
                      const CameraIntrinsics& cam) const;

    /// 计算极线 (F * left_ideal_pixel)
    Vec3 computeEpipolarLine(double leftNormX, double leftNormY) const;

    /// 点到极线距离
    double pointToEpipolarDistance(double rightNormX, double rightNormY,
                                   const Vec3& epipolarLine) const;

    /// 两射线最近点中点法三角化
    /// @param[out] minDistance 两射线最短距离 (三角化误差)
    Vec3 closestPointOnRays(
        const Vec3& originL, const Vec3& dirL,
        const Vec3& originR, const Vec3& dirR,
        double& minDistance) const;

    // ------------------------------------------------------------------
    // 图像处理
    // ------------------------------------------------------------------

    /// 从灰度图像检测 blob (种子扩展+背景减除加权质心)
    /// @param imageData  灰度图像 (uint8, row-major)
    /// @param width, height 图像尺寸
    /// @param outBlobs   输出检测结果
    /// @param maxBlobs   最大输出数量
    /// @param seedThreshold  种子阈值 (默认 10)
    /// @param minArea, maxArea 面积过滤
    /// @param minAspectRatio  长宽比过滤
    /// @return 实际检测到的 blob 数量
    uint32_t detectBlobs(
        const uint8_t* imageData, uint32_t width, uint32_t height,
        BlobDetection* outBlobs, uint32_t maxBlobs,
        uint32_t seedThreshold = 10,
        uint32_t minArea = 4, uint32_t maxArea = 10000,
        float minAspectRatio = 0.3f) const;

    // ------------------------------------------------------------------
    // 工具识别 (Kabsch 配准)
    // ------------------------------------------------------------------

    /// Kabsch 刚体配准算法
    /// @param modelPts   模型点坐标 (N x 3)
    /// @param measuredPts 测量点坐标 (N x 3)
    /// @param numPoints  点数 (>= 3)
    /// @return 配准结果 (旋转 + 平移 + RMS误差)
    static RegistrationResult kabschRegistration(
        const Vec3* modelPts, const Vec3* measuredPts, uint32_t numPoints);

    // ------------------------------------------------------------------
    // 辅助
    // ------------------------------------------------------------------

    /// Rodrigues 向量 -> 旋转矩阵
    static Mat3 rodrigues(double rx, double ry, double rz);

    /// 获取标定参数
    const StereoCalibration& getCalibration() const { return m_cal; }

    /// 获取旋转矩阵
    const Mat3& getRotation() const { return m_R; }

    /// 获取基础矩阵
    const Mat3& getFundamentalMatrix() const { return m_F; }

    /// 版本信息
    static const char* version();

private:
    bool m_initialized;
    StereoCalibration m_cal;

    // 预计算的矩阵
    Mat3 m_R;       // 旋转矩阵 (从 Rodrigues)
    Mat3 m_Rt;      // R 的转置
    Vec3 m_t;       // 平移向量
    Mat3 m_KL;      // 左相机内参矩阵
    Mat3 m_KR;      // 右相机内参矩阵
    Mat3 m_KL_inv;  // 左相机内参矩阵逆
    Mat3 m_KR_inv;  // 右相机内参矩阵逆
    Mat3 m_F;       // 基础矩阵
};

}  // namespace stereo_algo
