// ===========================================================================
// 逆向工程还原 — SegmenterV21 图像分割
// 来源: fusionTrack64.dll
// RTTI:
//   .?AV?$SegmenterBase@V?$Image@E$00@measurement@@U?$CompressedDataV2@E@compressor@2@EI$00$01@segmenter@measurement@@
//   .?AV?$SegmenterV21@V?$Image@E$00@measurement@@U?$CompressedDataV2@E@compressor@2@EI$00@segmenter@measurement@@
//   .?AV?$SegmenterV21@V?$Image@E$00@measurement@@U?$CompressedDataV3@E@compressor@2@EI$01@segmenter@measurement@@
//   .?AV?$SegmenterV21@V?$Image@G$02@measurement@@U?$CompressedDataV3@G@compressor@2@G_K$01@segmenter@measurement@@
//
// DLL字符串证据:
//   "Blob Maximum Surface"
//   "Blob Minimum Surface"
//   "Blob Minimum Aspect Ratio"
//   "Pixel Weight for Centroid"
//   "Advanced centroid detection"
//   "Enables processing of rejected raw data (edge blobs)."
//   "Seed Expansion Tolerance" — .?AVSeedExpansionToleranceGetSetter@@
//   "FTK_ERR_SEG_OVERFLOW" — 分割溢出错误
//
// DLL函数地址:
//   配置注册函数:   0x325A0 - 0x3438B (7659 bytes)
//   分割器初始化:   0x3CCE0 - 0x3DB1B (3643 bytes)
//   距离排序/选点:  0x44850 - 0x45255 (2565 bytes)
//   高级质心检测:   0x459F0 - 0x461D1 (2017 bytes)
//   圆心拟合主函数: 0x58240 - 0x5A54C (8972 bytes, 115 FP ops)
//
// 图像类型:
//   Image<uint8_t, 1>  → 8位灰度图
//   Image<uint16_t, 3> → 16位灰度图（GRAY16）
//
// 分割算法: 种子扩展 (seed expansion) blob检测 + 加权质心计算 + 圆心拟合
//
// 详细文档: analysis_disassembly_image_segmentation_circle_fitting.md
// 圆心拟合: CircleFitting.h
// ===========================================================================

#pragma once

#include <ftkInterface.h>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>
#include <map>
#include <algorithm>

namespace measurement {
namespace segmenter {

/// 图像模板 — RTTI: .?AV?$Image@E$00@measurement@@
/// E = uint8_t (char), G = uint16_t
template <typename PixelT, int Channels>
class Image
{
public:
    Image() : m_width(0), m_height(0), m_stride(0), m_data(nullptr) {}
    Image(uint16_t w, uint16_t h, int32_t stride, PixelT* data)
        : m_width(w), m_height(h), m_stride(stride), m_data(data) {}

    uint16_t width() const { return m_width; }
    uint16_t height() const { return m_height; }
    int32_t stride() const { return m_stride; }

    PixelT pixel(int x, int y) const
    {
        const uint8_t* row = reinterpret_cast<const uint8_t*>(m_data) + y * m_stride;
        return reinterpret_cast<const PixelT*>(row)[x];
    }

private:
    uint16_t m_width;
    uint16_t m_height;
    int32_t  m_stride;
    PixelT*  m_data;
};

/// Blob检测结果（内部表示，比 ftkRawData 更详细）
struct BlobResult
{
    float centerX;       ///< 亚像素质心 X
    float centerY;       ///< 亚像素质心 Y
    uint32_t area;       ///< 像素面积
    uint16_t bboxWidth;  ///< 边界框宽度
    uint16_t bboxHeight; ///< 边界框高度
    float peakIntensity; ///< 峰值亮度
    bool touchesEdge[4]; ///< [right, bottom, left, top]
};

/// 扫描线段 — RTTI: .?AV?$Line@EI@segmenter@measurement@@
/// 用于高效的行程编码 blob 处理
template <typename PixelT, typename IndexT>
struct Line
{
    IndexT   row;      ///< 所在行
    IndexT   colStart; ///< 起始列
    IndexT   colEnd;   ///< 结束列
    uint32_t blobId;   ///< 所属blob ID
};

/// 分割器基类 — SegmenterBase
///
/// 模板参数:
///   ImageT       = Image<PixelT, Channels>
///   CompressedT  = CompressedDataV2<PixelT> 或 CompressedDataV3<PixelT>
///   PixelT       = uint8_t 或 uint16_t
///   IndexT       = uint32_t 或 uint64_t
///   Version1     = 压缩版本（V2=0, V3=1）
///   Version2     = 图像通道数
template <typename ImageT, typename PixelT, typename IndexT>
class SegmenterBase
{
public:
    virtual ~SegmenterBase() = default;

    /// 对单幅图像执行分割
    /// @param image    输入图像（已解压缩）
    /// @param results  输出blob结果
    /// @param maxBlobs 最大blob数量
    /// @return 检测到的blob数量; -1 表示溢出
    virtual int32_t segment(const ImageT& image,
                            ftkRawData* results,
                            uint32_t maxBlobs) = 0;

    // 配置参数 — 来自 DLL 选项字符串
    void setBlobMinSurface(uint32_t pixels) { m_blobMinSurface = pixels; }
    void setBlobMaxSurface(uint32_t pixels) { m_blobMaxSurface = pixels; }
    void setBlobMinAspectRatio(float ratio) { m_blobMinAspectRatio = ratio; }
    void setPixelWeightEnabled(bool enabled) { m_usePixelWeight = enabled; }
    void setAdvancedCentroid(bool enabled) { m_advancedCentroid = enabled; }
    void setEdgeBlobsProcessing(bool enabled) { m_processEdgeBlobs = enabled; }
    void setSeedExpansionTolerance(uint32_t tolerance) { m_seedTolerance = tolerance; }

protected:
    uint32_t m_blobMinSurface = 4;       ///< 最小blob面积
    uint32_t m_blobMaxSurface = 10000;   ///< 最大blob面积
    float m_blobMinAspectRatio = 0.3f;   ///< 最小长宽比
    bool m_usePixelWeight = true;        ///< 使用像素权重计算质心
    bool m_advancedCentroid = false;     ///< 高级质心检测
    bool m_processEdgeBlobs = false;     ///< 处理边缘blob
    uint32_t m_seedTolerance = 10;       ///< 种子扩展容差
};

/// SegmenterV21 — 版本21分割器（当前主要算法）
///
/// 算法流程（从DLL字符串推断）:
/// 1. 扫描图像寻找亮于阈值的种子像素
/// 2. 从种子开始执行区域生长（seed expansion）
/// 3. 合并连接的行程编码线段为blob
/// 4. 过滤blob（面积、长宽比）
/// 5. 计算亚像素加权质心
/// 6. 标记边缘触碰状态
///
/// RTTI 显示此类适用于 8位/16位图像，V2/V3 压缩格式
template <typename ImageT, typename PixelT, typename IndexT>
class SegmenterV21 : public SegmenterBase<ImageT, PixelT, IndexT>
{
public:
    SegmenterV21() = default;
    ~SegmenterV21() override = default;

    int32_t segment(const ImageT& image,
                    ftkRawData* results,
                    uint32_t maxBlobs) override;

private:
    /// 种子扩展: 从一个亮像素开始，扩展到所有相连的足够亮的像素
    /// DLL选项: "Seed Expansion Tolerance"
    void seedExpansion(const ImageT& image,
                       int startX, int startY,
                       PixelT threshold,
                       std::vector<Line<PixelT, IndexT>>& lines,
                       std::vector<std::vector<bool>>& visited);

    /// 从行程编码线段计算加权质心
    /// DLL选项: "Pixel Weight for Centroid", "Advanced centroid detection"
    ///
    /// 标准质心: center = Σ(pos * intensity) / Σ(intensity)
    /// 高级质心: 使用圆心拟合 (见 CircleFitting.h)
    BlobResult computeCentroid(const ImageT& image,
                               const std::vector<Line<PixelT, IndexT>>& lines);

    /// 高级质心检测 — 圆心拟合
    /// DLL: 0x459F0-0x461D1 (2017 bytes) + 0x58240-0x5A54C (8972 bytes)
    ///
    /// 当 m_advancedCentroid = true 时使用此方法:
    /// 1. 提取blob边缘像素
    /// 2. 使用Givens旋转迭代法拟合圆
    /// 3. 圆心坐标作为亚像素中心
    ///
    /// 此方法精度远高于标准加权质心(可达1e-7级收敛)，
    /// 但计算量更大(最多49次迭代)
    BlobResult computeAdvancedCentroid(const ImageT& image,
                                       const std::vector<Line<PixelT, IndexT>>& lines);

    /// 提取blob的边缘像素（用于圆心拟合的输入）
    /// 边缘像素 = 在blob内但有至少一个4-连通邻居不在blob内的像素
    void extractEdgePixels(const ImageT& image,
                           const std::vector<Line<PixelT, IndexT>>& lines,
                           std::vector<double>& edgeX,
                           std::vector<double>& edgeY,
                           std::vector<double>& edgeWeights);

    /// 检查blob是否触碰图像边缘
    /// 对应 ftkStatus 中的 RightEdge, BottomEdge, LeftEdge, TopEdge 位
    void checkEdgeStatus(const BlobResult& blob,
                         uint16_t imageWidth,
                         uint16_t imageHeight,
                         ftkStatus& status);
};

// ===========================================================================
// SegmenterV21 实现
// ===========================================================================

template <typename ImageT, typename PixelT, typename IndexT>
int32_t SegmenterV21<ImageT, PixelT, IndexT>::segment(
    const ImageT& image,
    ftkRawData* results,
    uint32_t maxBlobs)
{
    uint16_t w = image.width();
    uint16_t h = image.height();

    // 访问标记矩阵
    std::vector<std::vector<bool>> visited(h, std::vector<bool>(w, false));

    // 自动阈值: 使用种子扩展容差作为阈值
    // 实际DLL可能使用更复杂的自适应阈值策略
    PixelT threshold = static_cast<PixelT>(this->m_seedTolerance);

    uint32_t blobCount = 0;

    // 扫描图像寻找种子像素
    for (uint16_t y = 0; y < h; ++y)
    {
        for (uint16_t x = 0; x < w; ++x)
        {
            if (visited[y][x])
                continue;

            PixelT val = image.pixel(x, y);
            if (val < threshold)
            {
                visited[y][x] = true;
                continue;
            }

            // 找到种子 — 执行区域生长
            std::vector<Line<PixelT, IndexT>> lines;
            seedExpansion(image, x, y, threshold, lines, visited);

            // 计算blob属性
            BlobResult blob;
            if (this->m_advancedCentroid)
            {
                // 高级质心: 使用圆心拟合算法
                // DLL: 当 "Advanced centroid detection" 启用时调用
                // 0x459F0-0x461D1 → 0x58240-0x5A54C
                blob = computeAdvancedCentroid(image, lines);
            }
            else
            {
                // 标准质心: 加权质心计算
                blob = computeCentroid(image, lines);
            }

            // 面积过滤
            if (blob.area < this->m_blobMinSurface ||
                blob.area > this->m_blobMaxSurface)
                continue;

            // 长宽比过滤
            if (blob.bboxWidth > 0 && blob.bboxHeight > 0)
            {
                float aspect = static_cast<float>(std::min(blob.bboxWidth, blob.bboxHeight))
                             / static_cast<float>(std::max(blob.bboxWidth, blob.bboxHeight));
                if (aspect < this->m_blobMinAspectRatio)
                    continue;
            }

            // 边缘blob过滤
            bool isEdgeBlob = blob.touchesEdge[0] || blob.touchesEdge[1] ||
                              blob.touchesEdge[2] || blob.touchesEdge[3];
            if (isEdgeBlob && !this->m_processEdgeBlobs)
                continue;

            // 输出检查
            if (blobCount >= maxBlobs)
                return -1;  // 溢出 → FTK_ERR_SEG_OVERFLOW

            // 填充 ftkRawData
            ftkRawData& rd = results[blobCount];
            rd.centerXPixels = blob.centerX;
            rd.centerYPixels = blob.centerY;
            rd.pixelsCount = blob.area;
            rd.width = blob.bboxWidth;
            rd.height = blob.bboxHeight;

            // 设置状态位
            checkEdgeStatus(blob, w, h, rd.status);

            ++blobCount;
        }
    }

    return static_cast<int32_t>(blobCount);
}

template <typename ImageT, typename PixelT, typename IndexT>
void SegmenterV21<ImageT, PixelT, IndexT>::seedExpansion(
    const ImageT& image,
    int startX, int startY,
    PixelT threshold,
    std::vector<Line<PixelT, IndexT>>& lines,
    std::vector<std::vector<bool>>& visited)
{
    // 种子扩展 — 使用栈实现的洪泛填充
    // DLL选项: "Seed Expansion Tolerance" 控制生长条件
    struct Seed { int x, y; };
    std::vector<Seed> stack;
    stack.push_back({startX, startY});
    visited[startY][startX] = true;

    int w = image.width();
    int h = image.height();

    while (!stack.empty())
    {
        Seed s = stack.back();
        stack.pop_back();

        // 从当前点向左右扩展形成行程
        int left = s.x, right = s.x;

        while (left > 0 && !visited[s.y][left - 1] && image.pixel(left - 1, s.y) >= threshold)
        {
            --left;
            visited[s.y][left] = true;
        }

        while (right < w - 1 && !visited[s.y][right + 1] && image.pixel(right + 1, s.y) >= threshold)
        {
            ++right;
            visited[s.y][right] = true;
        }

        // 记录行程
        Line<PixelT, IndexT> line;
        line.row = static_cast<IndexT>(s.y);
        line.colStart = static_cast<IndexT>(left);
        line.colEnd = static_cast<IndexT>(right);
        lines.push_back(line);

        // 向上下行扩展种子
        for (int ny : {s.y - 1, s.y + 1})
        {
            if (ny < 0 || ny >= h) continue;
            for (int nx = left; nx <= right; ++nx)
            {
                if (!visited[ny][nx] && image.pixel(nx, ny) >= threshold)
                {
                    visited[ny][nx] = true;
                    stack.push_back({nx, ny});
                }
            }
        }
    }
}

template <typename ImageT, typename PixelT, typename IndexT>
BlobResult SegmenterV21<ImageT, PixelT, IndexT>::computeCentroid(
    const ImageT& image,
    const std::vector<Line<PixelT, IndexT>>& lines)
{
    BlobResult blob{};
    double sumX = 0, sumY = 0, sumW = 0;
    uint32_t minX = UINT32_MAX, maxX = 0, minY = UINT32_MAX, maxY = 0;

    for (const auto& line : lines)
    {
        uint32_t y = line.row;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;

        for (uint32_t x = line.colStart; x <= line.colEnd; ++x)
        {
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;

            double weight = 1.0;
            if (this->m_usePixelWeight)
            {
                // 加权质心: 使用像素亮度作为权重
                weight = static_cast<double>(image.pixel(x, y));
            }

            sumX += x * weight;
            sumY += y * weight;
            sumW += weight;
            ++blob.area;
        }
    }

    if (sumW > 0)
    {
        blob.centerX = static_cast<float>(sumX / sumW);
        blob.centerY = static_cast<float>(sumY / sumW);
    }

    blob.bboxWidth = static_cast<uint16_t>(maxX - minX + 1);
    blob.bboxHeight = static_cast<uint16_t>(maxY - minY + 1);

    // 边缘触碰检查
    blob.touchesEdge[2] = (minX == 0);                           // left
    blob.touchesEdge[0] = (maxX >= image.width() - 1u);          // right
    blob.touchesEdge[3] = (minY == 0);                           // top
    blob.touchesEdge[1] = (maxY >= image.height() - 1u);         // bottom

    return blob;
}

template <typename ImageT, typename PixelT, typename IndexT>
void SegmenterV21<ImageT, PixelT, IndexT>::checkEdgeStatus(

// ===========================================================================
// 高级质心检测 — 圆心拟合
// DLL: 0x459F0-0x461D1 (预处理) + 0x58240-0x5A54C (核心拟合)
// ===========================================================================

template <typename ImageT, typename PixelT, typename IndexT>
void SegmenterV21<ImageT, PixelT, IndexT>::extractEdgePixels(
    const ImageT& image,
    const std::vector<Line<PixelT, IndexT>>& lines,
    std::vector<double>& edgeX,
    std::vector<double>& edgeY,
    std::vector<double>& edgeWeights)
{
    // 构建像素集合用于快速查找
    // DLL中使用行程编码直接判断
    std::map<uint32_t, std::vector<std::pair<uint32_t, uint32_t>>> rowSpans;
    for (const auto& line : lines)
    {
        rowSpans[line.row].push_back({line.colStart, line.colEnd});
    }

    auto isInBlob = [&](uint32_t x, uint32_t y) -> bool {
        auto it = rowSpans.find(y);
        if (it == rowSpans.end()) return false;
        for (const auto& span : it->second)
        {
            if (x >= span.first && x <= span.second) return true;
        }
        return false;
    };

    // 提取边缘像素: 在blob内但有邻居不在blob内
    for (const auto& line : lines)
    {
        uint32_t y = line.row;
        for (uint32_t x = line.colStart; x <= line.colEnd; ++x)
        {
            bool isBorder = false;
            // 4-连通邻域检查
            if (x == line.colStart || x == line.colEnd)
                isBorder = true;
            else if (!isInBlob(x, y - 1) || !isInBlob(x, y + 1))
                isBorder = true;

            if (isBorder)
            {
                edgeX.push_back(static_cast<double>(x));
                edgeY.push_back(static_cast<double>(y));

                double weight = 1.0;
                if (this->m_usePixelWeight)
                {
                    weight = static_cast<double>(image.pixel(x, y));
                }
                edgeWeights.push_back(weight);
            }
        }
    }
}

template <typename ImageT, typename PixelT, typename IndexT>
BlobResult SegmenterV21<ImageT, PixelT, IndexT>::computeAdvancedCentroid(
    const ImageT& image,
    const std::vector<Line<PixelT, IndexT>>& lines)
{
    // 首先计算标准质心作为后备
    BlobResult blob = computeCentroid(image, lines);

    // 如果点数太少，直接返回标准质心
    if (blob.area < 10)
        return blob;

    // 提取边缘像素
    std::vector<double> edgeX, edgeY, edgeWeights;
    extractEdgePixels(image, lines, edgeX, edgeY, edgeWeights);

    if (edgeX.size() < 3)
        return blob;

    // === 核心: Givens旋转迭代圆拟合 ===
    // DLL: 0x58240-0x5A54C
    // 详细算法见 CircleFitting.h

    const size_t N = edgeX.size();
    const double kEpsilon = 1e-7;         // DLL: RVA 0x248EC8
    const int kMaxIter = 49;              // DLL: cmp rdi, 0x31

    // 初始估计: 使用加权质心作为初始圆心
    double cx = blob.centerX;
    double cy = blob.centerY;
    double r = 0.0;

    // 初始半径 = 平均到圆心距离
    for (size_t i = 0; i < N; ++i)
    {
        double dx = edgeX[i] - cx;
        double dy = edgeY[i] - cy;
        r += std::sqrt(dx * dx + dy * dy);
    }
    r /= static_cast<double>(N);

    // Givens旋转QR迭代
    // DLL: 外层循环 0x59C90-0x5A2AA
    double R[3][4];

    for (int iter = 0; iter < kMaxIter; ++iter)
    {
        std::memset(R, 0, sizeof(R));

        for (size_t i = 0; i < N; ++i)
        {
            double dx = edgeX[i] - cx;
            double dy = edgeY[i] - cy;
            double di = std::sqrt(dx * dx + dy * dy);
            if (di < kEpsilon) di = kEpsilon;

            double w = this->m_usePixelWeight ? std::sqrt(edgeWeights[i]) : 1.0;

            // 加权雅可比行: [w*∂f/∂cx, w*∂f/∂cy, w*∂f/∂r, w*residual]
            double row[4] = {
                w * (dx / di),      // ∂f/∂cx
                w * (dy / di),      // ∂f/∂cy
                w * (-1.0),         // ∂f/∂r
                w * (di - r)        // 残差
            };

            // Givens消元: 将row合并到R的上三角中
            // DLL: 0x59E22-0x59E8E
            for (int j = 0; j < 3; ++j)
            {
                if (std::fabs(row[j]) < kEpsilon) continue;

                if (std::fabs(R[j][j]) < kEpsilon)
                {
                    for (int k = j; k < 4; ++k) R[j][k] = row[k];
                    break;
                }

                // 计算Givens旋转
                // DLL: 0x59DF6-0x59E12
                double a = R[j][j], b = row[j];
                double absA = std::fabs(a), absB = std::fabs(b);
                double norm;
                if (absA > absB)
                    norm = absA * std::sqrt(1.0 + (b / a) * (b / a));
                else
                    norm = absB * std::sqrt(1.0 + (a / b) * (a / b));

                double invNorm = 1.0 / norm;   // DLL: divsd from 0x248EF0
                double gc = a * invNorm;        // cos(θ)
                double gs = -b * invNorm;       // sin(θ), DLL: xorps with -0.0

                // 应用旋转到R[j]和row
                // DLL: 0x59E43-0x59E71
                for (int k = j; k < 4; ++k)
                {
                    double t1 = gc * R[j][k] + gs * row[k];
                    double t2 = gc * row[k] - gs * R[j][k];
                    R[j][k] = t1;
                    row[k] = t2;
                }
            }
        }

        // 回代求解增量
        // DLL: 0x5A139-0x5A215
        double delta[3] = {0.0, 0.0, 0.0};
        for (int j = 2; j >= 0; --j)
        {
            if (std::fabs(R[j][j]) < kEpsilon) continue;
            double sum = R[j][3];
            for (int k = j + 1; k < 3; ++k)
                sum -= R[j][k] * delta[k];
            delta[j] = sum / R[j][j];
        }

        // 更新参数
        cx -= delta[0];
        cy -= delta[1];
        r -= delta[2];

        // 收敛检查
        // DLL: ucomisd with 0x248EC8 = 1e-7
        double change = std::fabs(delta[0]) + std::fabs(delta[1]) + std::fabs(delta[2]);
        if (change < kEpsilon)
            break;
    }

    // 更新blob结果为拟合的圆心
    blob.centerX = static_cast<float>(cx);
    blob.centerY = static_cast<float>(cy);

    return blob;
}

// ===========================================================================
// checkEdgeStatus 实现
// ===========================================================================

template <typename ImageT, typename PixelT, typename IndexT>
void SegmenterV21<ImageT, PixelT, IndexT>::checkEdgeStatus(
    const BlobResult& blob,
    uint16_t imageWidth,
    uint16_t imageHeight,
    ftkStatus& status)
{
    // 对应 ftkStatus 位域:
    //   RightEdge : 1  → bit 0
    //   BottomEdge : 1 → bit 1
    //   LeftEdge : 1   → bit 2
    //   TopEdge : 1    → bit 3
    status = static_cast<uint32_t>(0);

    if (blob.touchesEdge[0]) status.RightEdge = 1;
    if (blob.touchesEdge[1]) status.BottomEdge = 1;
    if (blob.touchesEdge[2]) status.LeftEdge = 1;
    if (blob.touchesEdge[3]) status.TopEdge = 1;
}

// 显式实例化 — 从RTTI信息推断的实际使用类型
using Image8 = Image<uint8_t, 1>;
using Image16 = Image<uint16_t, 3>;
using Segmenter8 = SegmenterV21<Image8, uint8_t, uint32_t>;
using Segmenter16 = SegmenterV21<Image16, uint16_t, uint64_t>;

}  // namespace segmenter
}  // namespace measurement
