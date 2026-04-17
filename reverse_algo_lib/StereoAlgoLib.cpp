// ===========================================================================
// StereoAlgoLib.cpp — 独立逆向工程双目视觉算法库实现
//
// 所有算法直接从 verify_reverse_engineered.py 中验证通过的 Python 代码移植
// 包含以下已修复的关键算法:
//   Fix 1: 右相机射线变换 (R^T 而非 R)
//   Fix 2: 极线距离在无畸变理想像素空间计算
//   Fix 3: 去畸变固定 20 次迭代 + DLL 切向顺序 + 奇异性检查
// ===========================================================================

#include "StereoAlgoLib.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace stereo_algo {

// ===========================================================================
// 常量
// ===========================================================================

static constexpr double kAlmostZero = 1e-7;
static constexpr int kUndistortIterations = 20;  // DLL: cmp eax, 0x14
static constexpr double kPixelCenterOffset = 0.5;
static constexpr double kBgStep = 2.0;  // V3 8-bit 量化步长

// ===========================================================================
// 辅助函数
// ===========================================================================

static inline double dot3(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline double norm3(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline Vec3 normalize3(const Vec3& v) {
    double n = norm3(v);
    if (n < 1e-15) return v;
    return { v.x / n, v.y / n, v.z / n };
}

static inline Vec3 sub3(const Vec3& a, const Vec3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

static inline Vec3 add3(const Vec3& a, const Vec3& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

static inline Vec3 scale3(const Vec3& v, double s) {
    return { v.x * s, v.y * s, v.z * s };
}

/// 构建上三角内参矩阵的逆 (解析公式)
static Mat3 invertUpperTriangularK(const Mat3& K) {
    double fx = K.m[0][0], fy = K.m[1][1];
    double cx = K.m[0][2], cy = K.m[1][2];
    double s  = K.m[0][1];

    Mat3 inv;
    inv.m[0][0] = 1.0 / fx;
    inv.m[0][1] = -s / (fx * fy);
    inv.m[0][2] = (s * cy - fy * cx) / (fx * fy);
    inv.m[1][1] = 1.0 / fy;
    inv.m[1][2] = -cy / fy;
    inv.m[2][2] = 1.0;
    return inv;
}

// ===========================================================================
// StereoVision 实现
// ===========================================================================

StereoVision::StereoVision()
    : m_initialized(false)
{
}

StereoVision::~StereoVision() = default;

const char* StereoVision::version() {
    return "stereo_algo v2.0.0 (2026-04, based on verify_reverse_engineered.py)";
}

// ---------------------------------------------------------------------------
// Rodrigues 向量 → 旋转矩阵
// ---------------------------------------------------------------------------

Mat3 StereoVision::rodrigues(double rx, double ry, double rz) {
    double theta = std::sqrt(rx * rx + ry * ry + rz * rz);
    if (theta < 1e-10) {
        return Mat3::identity();
    }

    double kx = rx / theta, ky = ry / theta, kz = rz / theta;
    double c = std::cos(theta), s = std::sin(theta);
    double v = 1.0 - c;

    Mat3 R;
    R.m[0][0] = c + kx * kx * v;
    R.m[0][1] = kx * ky * v - kz * s;
    R.m[0][2] = kx * kz * v + ky * s;
    R.m[1][0] = ky * kx * v + kz * s;
    R.m[1][1] = c + ky * ky * v;
    R.m[1][2] = ky * kz * v - kx * s;
    R.m[2][0] = kz * kx * v - ky * s;
    R.m[2][1] = kz * ky * v + kx * s;
    R.m[2][2] = c + kz * kz * v;
    return R;
}

// ---------------------------------------------------------------------------
// 初始化
// ---------------------------------------------------------------------------

bool StereoVision::initialize(const StereoCalibration& cal) {
    m_cal = cal;

    // Rodrigues → 旋转矩阵
    m_R = rodrigues(cal.rotation[0], cal.rotation[1], cal.rotation[2]);
    m_Rt = m_R.transpose();
    m_t = { cal.translation[0], cal.translation[1], cal.translation[2] };

    // 内参矩阵 K_L
    m_KL = {};
    m_KL.m[0][0] = cal.leftCam.focalLength[0];
    m_KL.m[0][1] = cal.leftCam.skew * cal.leftCam.focalLength[0];
    m_KL.m[0][2] = cal.leftCam.opticalCentre[0];
    m_KL.m[1][1] = cal.leftCam.focalLength[1];
    m_KL.m[1][2] = cal.leftCam.opticalCentre[1];
    m_KL.m[2][2] = 1.0;

    // 内参矩阵 K_R
    m_KR = {};
    m_KR.m[0][0] = cal.rightCam.focalLength[0];
    m_KR.m[0][1] = cal.rightCam.skew * cal.rightCam.focalLength[0];
    m_KR.m[0][2] = cal.rightCam.opticalCentre[0];
    m_KR.m[1][1] = cal.rightCam.focalLength[1];
    m_KR.m[1][2] = cal.rightCam.opticalCentre[1];
    m_KR.m[2][2] = 1.0;

    // K 逆
    m_KL_inv = invertUpperTriangularK(m_KL);
    m_KR_inv = invertUpperTriangularK(m_KR);

    // 基础矩阵 F = K_R^{-T} * [t]_x * R * K_L^{-1}
    Mat3 tx_cross = {};
    tx_cross.m[0][1] = -m_t.z;
    tx_cross.m[0][2] =  m_t.y;
    tx_cross.m[1][0] =  m_t.z;
    tx_cross.m[1][2] = -m_t.x;
    tx_cross.m[2][0] = -m_t.y;
    tx_cross.m[2][1] =  m_t.x;

    Mat3 E = tx_cross.mul(m_R);  // 本质矩阵
    m_F = m_KR_inv.transpose().mul(E).mul(m_KL_inv);

    m_initialized = true;
    return true;
}

// ---------------------------------------------------------------------------
// 去畸变 — 迭代 Brown-Conrady 模型 (20 次, 与 DLL 完全一致)
//
// 直接从 verify_reverse_engineered.py 的 undistort_point() 移植
// ---------------------------------------------------------------------------

Vec2 StereoVision::undistortPoint(double px, double py,
                                   const CameraIntrinsics& cam) const {
    double fx = cam.focalLength[0];
    double fy = cam.focalLength[1];

    if (std::fabs(fx) < kAlmostZero || std::fabs(fy) < kAlmostZero) {
        return { px - cam.opticalCentre[0], py - cam.opticalCentre[1] };
    }

    double cx = cam.opticalCentre[0];
    double cy = cam.opticalCentre[1];
    double k1 = cam.distortion[0];
    double k2 = cam.distortion[1];
    double p1 = cam.distortion[2];
    double p2 = cam.distortion[3];
    double k3 = cam.distortion[4];

    // 初始归一化坐标
    double yn = (py - cy) / fy;
    double xn = (px - cx) / fx - cam.skew * yn;

    double x0 = xn;
    double y0 = yn;

    // 固定 20 次迭代 (DLL: cmp eax, 0x14)
    for (int iter = 0; iter < kUndistortIterations; ++iter) {
        double r2 = xn * xn + yn * yn;
        double r4 = r2 * r2;
        double r6 = r4 * r2;

        double radial = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;

        // 奇异性检查 (DLL: "The value of radial is almost zero.")
        if (std::fabs(radial) < kAlmostZero) {
            return { xn, yn };
        }

        double inv_radial = 1.0 / radial;

        // DLL 切向畸变顺序: p2*(r²+2x²) + 2*p1*xy
        double dx = p2 * (r2 + 2.0 * xn * xn) + 2.0 * p1 * xn * yn;
        double dy = p1 * (r2 + 2.0 * yn * yn) + 2.0 * p2 * xn * yn;

        xn = (x0 - dx) * inv_radial;
        yn = (y0 - dy) * inv_radial;
    }

    return { xn, yn };
}

// ---------------------------------------------------------------------------
// 加畸变 — 正向 Brown-Conrady 模型
//
// 直接从 verify_reverse_engineered.py 的 _distort_to_pixel() / project_point_to_pixel() 移植
// ---------------------------------------------------------------------------

Vec2 StereoVision::distortPoint(double xn, double yn,
                                 const CameraIntrinsics& cam) const {
    double k1 = cam.distortion[0];
    double k2 = cam.distortion[1];
    double p1 = cam.distortion[2];
    double p2 = cam.distortion[3];
    double k3 = cam.distortion[4];

    double r2 = xn * xn + yn * yn;
    double r4 = r2 * r2;
    double r6 = r4 * r2;

    double radial = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;
    double dx = 2.0 * p1 * xn * yn + p2 * (r2 + 2.0 * xn * xn);
    double dy = p1 * (r2 + 2.0 * yn * yn) + 2.0 * p2 * xn * yn;

    double xd = xn * radial + dx;
    double yd = yn * radial + dy;

    double px = cam.focalLength[0] * (xd + cam.skew * yd) + cam.opticalCentre[0];
    double py = cam.focalLength[1] * yd + cam.opticalCentre[1];

    return { px, py };
}

// ---------------------------------------------------------------------------
// 极线计算
//
// 从 verify_reverse_engineered.py 的 compute_epipolar_line() 移植
// SDK 在无畸变理想像素空间操作: line = F * (K_L * [norm_x, norm_y, 1])
// ---------------------------------------------------------------------------

Vec3 StereoVision::computeEpipolarLine(double leftNormX, double leftNormY) const {
    // 转为理想像素坐标: K_L * (x, y, 1)
    Vec3 plPixel = {
        m_KL.m[0][0] * leftNormX + m_KL.m[0][1] * leftNormY + m_KL.m[0][2],
        m_KL.m[1][1] * leftNormY + m_KL.m[1][2],
        1.0
    };

    // line = F * plPixel
    Vec3 line = m_F.mul(plPixel);

    double abs_a = std::fabs(line.x);
    double abs_b = std::fabs(line.y);
    if (abs_a < kAlmostZero && abs_b < kAlmostZero) {
        return { 0.0, 0.0, 0.0 };  // 退化
    }

    return line;
}

// ---------------------------------------------------------------------------
// 点到极线距离
//
// 从 verify_reverse_engineered.py 移植
// 使用无畸变理想像素坐标: K_R * (norm_x, norm_y, 1)
// 验证: mean diff 0.001 px, max 0.005 px
// ---------------------------------------------------------------------------

double StereoVision::pointToEpipolarDistance(
    double rightNormX, double rightNormY,
    const Vec3& line) const {
    // 理想像素坐标: K_R * (x, y, 1)
    double rpX = m_KR.m[0][0] * rightNormX + m_KR.m[0][1] * rightNormY + m_KR.m[0][2];
    double rpY = m_KR.m[1][1] * rightNormY + m_KR.m[1][2];

    double nrm = std::sqrt(line.x * line.x + line.y * line.y);
    if (nrm < 1e-12) return 0.0;

    return (line.x * rpX + line.y * rpY + line.z) / nrm;
}

// ---------------------------------------------------------------------------
// 两射线最近点 — 中点法三角化
//
// 从 verify_reverse_engineered.py 的 triangulate_midpoint() 移植
// DLL: RVA 0x1EE990
// ---------------------------------------------------------------------------

Vec3 StereoVision::closestPointOnRays(
    const Vec3& originL, const Vec3& dirL,
    const Vec3& originR, const Vec3& dirR,
    double& minDistance) const {

    Vec3 w = sub3(originL, originR);

    double a = dot3(dirL, dirL);
    double b = dot3(dirL, dirR);
    double c = dot3(dirR, dirR);
    double d = dot3(dirL, w);
    double e = dot3(dirR, w);

    double denom = a * c - b * b;
    if (std::fabs(denom) < 1e-15) {
        minDistance = std::numeric_limits<double>::max();
        return { 0.0, 0.0, 0.0 };
    }

    double s = (b * e - c * d) / denom;
    double t = (a * e - b * d) / denom;

    Vec3 pL = add3(originL, scale3(dirL, s));
    Vec3 pR = add3(originR, scale3(dirR, t));

    Vec3 mid = { (pL.x + pR.x) * 0.5, (pL.y + pR.y) * 0.5, (pL.z + pR.z) * 0.5 };
    minDistance = norm3(sub3(pL, pR));

    return mid;
}

// ---------------------------------------------------------------------------
// 完整三角化管线: undistort → rays → midpoint → epipolar error
//
// 从 verify_reverse_engineered.py 的 triangulate_point() 移植
// 包含关键修复:
//   Fix 1: 右相机原点 = R^T * (-t), 方向 = R^T * dir_cam_right
//   Fix 2: 极线距离在无畸变理想像素空间
// ---------------------------------------------------------------------------

TriangulationResult StereoVision::triangulatePoint(
    double leftPx, double leftPy,
    double rightPx, double rightPy) const {

    TriangulationResult result = {};
    if (!m_initialized) {
        result.success = false;
        return result;
    }

    // 去畸变
    Vec2 lnorm = undistortPoint(leftPx, leftPy, m_cal.leftCam);
    Vec2 rnorm = undistortPoint(rightPx, rightPy, m_cal.rightCam);

    // 左射线: 原点 = (0,0,0), 方向 = normalize(lx, ly, 1)
    Vec3 leftOrigin = { 0.0, 0.0, 0.0 };
    Vec3 leftDir = normalize3({ lnorm.x, lnorm.y, 1.0 });

    // 右射线 (Fix 1):
    //   右相机中心(左帧): origin_R = R^T * (-t)
    //   方向(左帧):       dir_R = R^T * (rx, ry, 1)
    Vec3 negT = { -m_t.x, -m_t.y, -m_t.z };
    Vec3 rightOrigin = m_Rt.mul(negT);
    Vec3 rightDirCam = { rnorm.x, rnorm.y, 1.0 };
    Vec3 rightDir = normalize3(m_Rt.mul(rightDirCam));

    // 三角化
    double triErr = 0.0;
    Vec3 pos3d = closestPointOnRays(leftOrigin, leftDir, rightOrigin, rightDir, triErr);

    // 极线误差 (Fix 2): 在无畸变理想像素空间
    Vec3 line = computeEpipolarLine(lnorm.x, lnorm.y);
    double epiErr = pointToEpipolarDistance(rnorm.x, rnorm.y, line);

    result.position = pos3d;
    result.epipolarError = epiErr;
    result.triangulationError = triErr;
    result.success = true;
    return result;
}

// ---------------------------------------------------------------------------
// 重投影: 3D → 左右 2D 像素
//
// 从 verify_reverse_engineered.py 的 reproject_3d_to_2d() 移植
// ---------------------------------------------------------------------------

ReprojectionResult StereoVision::reprojectTo2D(const Vec3& pos3d) const {
    ReprojectionResult result = {};
    if (!m_initialized) {
        result.success = false;
        return result;
    }

    // 左相机: 点已在左帧, 直接投影
    if (std::fabs(pos3d.z) < 1e-12) {
        result.success = false;
        return result;
    }
    double lxn = pos3d.x / pos3d.z;
    double lyn = pos3d.y / pos3d.z;
    result.leftPixel = distortPoint(lxn, lyn, m_cal.leftCam);

    // 右相机: 变换到右相机帧 p_right = R * p_left + t
    Vec3 pRight = add3(m_R.mul(pos3d), m_t);
    if (std::fabs(pRight.z) < 1e-12) {
        result.success = false;
        return result;
    }
    double rxn = pRight.x / pRight.z;
    double ryn = pRight.y / pRight.z;
    result.rightPixel = distortPoint(rxn, ryn, m_cal.rightCam);

    result.success = true;
    return result;
}

// ---------------------------------------------------------------------------
// Blob 检测 (种子扩展 + 背景减除加权质心)
//
// 从 verify_reverse_engineered.py 的 verify_circle_centroid() 中的分割逻辑移植
// 关键修复:
//   - 像素中心坐标约定: (x+0.5, y+0.5)
//   - 背景减除加权: weight = intensity - (minIntensity - 2)
// ---------------------------------------------------------------------------

uint32_t StereoVision::detectBlobs(
    const uint8_t* imageData, uint32_t width, uint32_t height,
    BlobDetection* outBlobs, uint32_t maxBlobs,
    uint32_t seedThreshold,
    uint32_t minArea, uint32_t maxArea,
    float minAspectRatio) const {

    if (!imageData || !outBlobs || maxBlobs == 0) return 0;

    struct Pixel { uint32_t x, y; uint8_t val; };

    std::vector<bool> visited(static_cast<size_t>(width) * height, false);
    uint32_t detCount = 0;

    for (uint32_t yy = 0; yy < height && detCount < maxBlobs; ++yy) {
        for (uint32_t xx = 0; xx < width && detCount < maxBlobs; ++xx) {
            uint32_t idx = yy * width + xx;
            if (visited[idx]) continue;

            uint8_t val = imageData[idx];
            if (val < seedThreshold) {
                visited[idx] = true;
                continue;
            }

            // 洪泛填充 (4-连通)
            std::vector<Pixel> blobPixels;
            std::vector<std::pair<uint32_t, uint32_t>> stack;
            stack.push_back({ xx, yy });
            visited[idx] = true;

            while (!stack.empty()) {
                auto [cx, cy] = stack.back();
                stack.pop_back();
                blobPixels.push_back({ cx, cy, imageData[cy * width + cx] });

                const int dx[] = { -1, 1, 0, 0 };
                const int dy[] = { 0, 0, -1, 1 };
                for (int d = 0; d < 4; ++d) {
                    int nx = static_cast<int>(cx) + dx[d];
                    int ny = static_cast<int>(cy) + dy[d];
                    if (nx >= 0 && nx < static_cast<int>(width) &&
                        ny >= 0 && ny < static_cast<int>(height)) {
                        uint32_t nidx = static_cast<uint32_t>(ny) * width + static_cast<uint32_t>(nx);
                        if (!visited[nidx] && imageData[nidx] >= seedThreshold) {
                            visited[nidx] = true;
                            stack.push_back({ static_cast<uint32_t>(nx),
                                              static_cast<uint32_t>(ny) });
                        }
                    }
                }
            }

            // 面积过滤
            uint32_t area = static_cast<uint32_t>(blobPixels.size());
            if (area < minArea || area > maxArea) continue;

            // 边界框和长宽比
            uint32_t bMinX = UINT32_MAX, bMaxX = 0, bMinY = UINT32_MAX, bMaxY = 0;
            for (const auto& p : blobPixels) {
                if (p.x < bMinX) bMinX = p.x;
                if (p.x > bMaxX) bMaxX = p.x;
                if (p.y < bMinY) bMinY = p.y;
                if (p.y > bMaxY) bMaxY = p.y;
            }
            uint16_t bw = static_cast<uint16_t>(bMaxX - bMinX + 1);
            uint16_t bh = static_cast<uint16_t>(bMaxY - bMinY + 1);
            float aspect = static_cast<float>(std::min(bw, bh)) /
                           static_cast<float>(std::max(bw, bh));
            if (aspect < minAspectRatio) continue;

            // 背景减除加权质心 (验证结果: 平均 0.000388 px)
            uint8_t minIntensity = 255;
            for (const auto& p : blobPixels) {
                if (p.val < minIntensity) minIntensity = p.val;
            }

            double bgLevel = static_cast<double>(minIntensity) - kBgStep;
            double sumX = 0.0, sumY = 0.0, sumW = 0.0;
            for (const auto& p : blobPixels) {
                double w = static_cast<double>(p.val) - bgLevel;
                if (w <= 0.0) w = 0.0;
                sumX += (p.x + kPixelCenterOffset) * w;
                sumY += (p.y + kPixelCenterOffset) * w;
                sumW += w;
            }

            if (sumW <= 0.0) continue;

            outBlobs[detCount].centerX = static_cast<float>(sumX / sumW);
            outBlobs[detCount].centerY = static_cast<float>(sumY / sumW);
            outBlobs[detCount].area = area;
            outBlobs[detCount].width = bw;
            outBlobs[detCount].height = bh;
            ++detCount;
        }
    }

    return detCount;
}

// ---------------------------------------------------------------------------
// Kabsch 刚体配准
//
// 从 verify_reverse_engineered.py 的 kabsch_registration() 移植
// 使用 Jacobi SVD 实现 (不依赖外部库)
// ---------------------------------------------------------------------------

// 内部: 3x3 SVD (基于 Jacobi 对称特征分解)
namespace {

struct SVD3x3 {
    Mat3 U, V;
    double S[3];
};

/// 对称 3x3 矩阵的 Jacobi 特征分解
/// 输入 A (对称), 输出 eigenvalues + eigenvectors
static void symmetricEigen3x3(const double A[3][3],
                                double eigenvalues[3],
                                double eigenvectors[3][3]) {
    // 初始化 eigenvectors 为单位矩阵
    double V[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
    double a[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            a[i][j] = A[i][j];

    for (int sweep = 0; sweep < 200; ++sweep) {
        double offDiag = std::fabs(a[0][1]) + std::fabs(a[0][2]) + std::fabs(a[1][2]);
        if (offDiag < 1e-15) break;

        // Sweep over all off-diagonal pairs
        int pairs[][2] = {{0,1}, {0,2}, {1,2}};
        for (auto& pr : pairs) {
            int p = pr[0], q = pr[1];
            if (std::fabs(a[p][q]) < 1e-15) continue;

            // Compute Jacobi rotation
            double theta = 0.5 * std::atan2(2.0 * a[p][q], a[p][p] - a[q][q]);
            double c = std::cos(theta);
            double s = std::sin(theta);

            // Apply rotation: A' = J^T * A * J
            // This needs to be done carefully for the symmetric case
            double a_new[3][3];
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    a_new[i][j] = a[i][j];

            // Update rows and columns p, q
            for (int i = 0; i < 3; ++i) {
                if (i == p || i == q) continue;
                a_new[i][p] = c * a[i][p] + s * a[i][q];
                a_new[p][i] = a_new[i][p];
                a_new[i][q] = -s * a[i][p] + c * a[i][q];
                a_new[q][i] = a_new[i][q];
            }

            // Update the 2x2 block
            a_new[p][p] = c * c * a[p][p] + 2.0 * c * s * a[p][q] + s * s * a[q][q];
            a_new[q][q] = s * s * a[p][p] - 2.0 * c * s * a[p][q] + c * c * a[q][q];
            a_new[p][q] = 0.0;
            a_new[q][p] = 0.0;

            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    a[i][j] = a_new[i][j];

            // Accumulate rotation in V
            for (int i = 0; i < 3; ++i) {
                double vp = V[i][p], vq = V[i][q];
                V[i][p] = c * vp + s * vq;
                V[i][q] = -s * vp + c * vq;
            }
        }
    }

    for (int i = 0; i < 3; ++i) {
        eigenvalues[i] = a[i][i];
        for (int j = 0; j < 3; ++j)
            eigenvectors[i][j] = V[i][j];
    }
}

static SVD3x3 svd3x3(const Mat3& M) {
    // Compute A = M^T * M (symmetric positive semi-definite)
    Mat3 AtA = M.transpose().mul(M);

    // Eigendecomposition: AtA = V * diag(σ²) * V^T
    double eigenvals[3];
    double eigvecs[3][3];
    symmetricEigen3x3(AtA.m, eigenvals, eigvecs);

    SVD3x3 result;
    // V from eigenvectors (columns)
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            result.V.m[i][j] = eigvecs[i][j];

    // Singular values = sqrt of eigenvalues
    for (int i = 0; i < 3; ++i) {
        result.S[i] = std::sqrt(std::max(0.0, eigenvals[i]));
    }

    // U = M * V * Σ^{-1}
    // For zero singular values or degenerate columns, complete the orthonormal basis
    Mat3 MV = M.mul(result.V);
    result.U = {};
    for (int j = 0; j < 3; ++j) {
        if (result.S[j] > 1e-12) {
            double inv = 1.0 / result.S[j];
            for (int i = 0; i < 3; ++i)
                result.U.m[i][j] = MV.m[i][j] * inv;
        }
    }

    // Check each U column for near-zero norm (can happen with rank-deficient M
    // even when S is not exactly zero due to floating-point)
    int zeroCol = -1;
    int validCols = 0;
    for (int j = 0; j < 3; ++j) {
        double colNorm = 0.0;
        for (int i = 0; i < 3; ++i)
            colNorm += result.U.m[i][j] * result.U.m[i][j];
        if (colNorm < 1e-12) {
            zeroCol = j;
        } else {
            ++validCols;
            // Normalize the column
            double inv = 1.0 / std::sqrt(colNorm);
            for (int i = 0; i < 3; ++i)
                result.U.m[i][j] *= inv;
        }
    }

    // If one column of U is degenerate, complete it as cross product of other two
    if (validCols == 2 && zeroCol >= 0) {
        int c1 = (zeroCol + 1) % 3;
        int c2 = (zeroCol + 2) % 3;
        result.U.m[0][zeroCol] = result.U.m[1][c1] * result.U.m[2][c2]
                               - result.U.m[2][c1] * result.U.m[1][c2];
        result.U.m[1][zeroCol] = result.U.m[2][c1] * result.U.m[0][c2]
                               - result.U.m[0][c1] * result.U.m[2][c2];
        result.U.m[2][zeroCol] = result.U.m[0][c1] * result.U.m[1][c2]
                               - result.U.m[1][c1] * result.U.m[0][c2];
    }

    // Similarly check V columns
    zeroCol = -1;
    validCols = 0;
    for (int j = 0; j < 3; ++j) {
        double colNorm = 0.0;
        for (int i = 0; i < 3; ++i)
            colNorm += result.V.m[i][j] * result.V.m[i][j];
        if (colNorm < 1e-12) {
            zeroCol = j;
        } else {
            ++validCols;
        }
    }
    if (validCols == 2 && zeroCol >= 0) {
        int c1 = (zeroCol + 1) % 3;
        int c2 = (zeroCol + 2) % 3;
        result.V.m[0][zeroCol] = result.V.m[1][c1] * result.V.m[2][c2]
                               - result.V.m[2][c1] * result.V.m[1][c2];
        result.V.m[1][zeroCol] = result.V.m[2][c1] * result.V.m[0][c2]
                               - result.V.m[0][c1] * result.V.m[2][c2];
        result.V.m[2][zeroCol] = result.V.m[0][c1] * result.V.m[1][c2]
                               - result.V.m[1][c1] * result.V.m[0][c2];
    }

    return result;
}

}  // anonymous namespace

RegistrationResult StereoVision::kabschRegistration(
    const Vec3* modelPts, const Vec3* measuredPts, uint32_t n) {

    RegistrationResult result = {};
    if (n < 3 || !modelPts || !measuredPts) {
        result.success = false;
        return result;
    }

    // 质心
    Vec3 centModel = {}, centMeas = {};
    for (uint32_t i = 0; i < n; ++i) {
        centModel.x += modelPts[i].x;
        centModel.y += modelPts[i].y;
        centModel.z += modelPts[i].z;
        centMeas.x += measuredPts[i].x;
        centMeas.y += measuredPts[i].y;
        centMeas.z += measuredPts[i].z;
    }
    double invN = 1.0 / n;
    centModel = scale3(centModel, invN);
    centMeas = scale3(centMeas, invN);

    // 交叉协方差矩阵 H = Σ (model_centered)^T * (measured_centered)
    Mat3 H = {};
    for (uint32_t i = 0; i < n; ++i) {
        Vec3 mc = sub3(modelPts[i], centModel);
        Vec3 me = sub3(measuredPts[i], centMeas);
        H.m[0][0] += mc.x * me.x;  H.m[0][1] += mc.x * me.y;  H.m[0][2] += mc.x * me.z;
        H.m[1][0] += mc.y * me.x;  H.m[1][1] += mc.y * me.y;  H.m[1][2] += mc.y * me.z;
        H.m[2][0] += mc.z * me.x;  H.m[2][1] += mc.z * me.y;  H.m[2][2] += mc.z * me.z;
    }

    // SVD(H) = U * Σ * V^T
    auto svdResult = svd3x3(H);

    // R = V * diag(1, 1, det(V*U^T)) * U^T
    Mat3 VUt = svdResult.V.mul(svdResult.U.transpose());
    double det = VUt.m[0][0] * (VUt.m[1][1] * VUt.m[2][2] - VUt.m[1][2] * VUt.m[2][1])
               - VUt.m[0][1] * (VUt.m[1][0] * VUt.m[2][2] - VUt.m[1][2] * VUt.m[2][0])
               + VUt.m[0][2] * (VUt.m[1][0] * VUt.m[2][1] - VUt.m[1][1] * VUt.m[2][0]);

    Mat3 signMat = Mat3::identity();
    if (det < 0) signMat.m[2][2] = -1.0;

    result.rotation = svdResult.V.mul(signMat).mul(svdResult.U.transpose());

    // t = centMeas - R * centModel
    Vec3 RcM = result.rotation.mul(centModel);
    result.translation = sub3(centMeas, RcM);

    // RMS 误差
    double sumSqErr = 0.0;
    for (uint32_t i = 0; i < n; ++i) {
        Vec3 transformed = add3(result.rotation.mul(modelPts[i]), result.translation);
        Vec3 diff = sub3(measuredPts[i], transformed);
        sumSqErr += dot3(diff, diff);
    }
    result.rmsError = std::sqrt(sumSqErr / n);
    result.success = true;
    return result;
}

}  // namespace stereo_algo
