// ===========================================================================
// ReverseEngineeredPipeline.cpp — 逆向工程算法封装实现
// ===========================================================================

#include "ReverseEngineeredPipeline.h"

namespace reverse_engineered {

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------

ReverseEngineeredPipeline::ReverseEngineeredPipeline() = default;
ReverseEngineeredPipeline::~ReverseEngineeredPipeline() = default;

// ---------------------------------------------------------------------------
// 初始化
// ---------------------------------------------------------------------------

bool ReverseEngineeredPipeline::initialize(const ftkStereoParameters& params)
{
    return m_stereo.initialize(params);
}

bool ReverseEngineeredPipeline::isInitialized() const
{
    return m_stereo.isInitialized();
}

// ---------------------------------------------------------------------------
// 图像分割 + 质心/圆心检测
// ---------------------------------------------------------------------------

uint32_t ReverseEngineeredPipeline::detectBlobs(
    const uint8_t* imageData,
    uint32_t width,
    uint32_t height,
    ftkRawData* rawData,
    uint32_t maxDetections) const
{
    if (imageData == nullptr || rawData == nullptr || maxDetections == 0)
        return 0;

    using namespace measurement::segmenter;

    // ================================================================
    // 步骤 1: 种子扩展分割 (RLE Seed Expansion) — 优化版
    //
    // 基于 fusionTrack64.dll 反汇编 (DLL: 0x3CCE0-0x3DB1B) 的 RLE 扫描线
    // 种子扩展策略优化:
    //  - RLE 行程扫描: 水平扩展 run + 垂直播种
    //  - uint8_t 平坦 visited 替代 vector<bool>
    //  - 可复用缓冲区 (runs/seedStack) 避免每 blob 堆分配
    //  - 单遍统计: bbox / minIntensity 在行程展开中累积
    //  - 尽早放弃: 面积超 maxArea 立即终止
    // ================================================================
    const uint32_t threshold = m_segConfig.seedThreshold;
    const uint32_t minArea = m_segConfig.blobMinSurface;
    const uint32_t maxArea = m_segConfig.blobMaxSurface;
    const float minAspect = m_segConfig.blobMinAspectRatio;
    const uint8_t thresh = static_cast<uint8_t>(
        threshold <= 255 ? threshold : 255);

    const size_t totalPixels = static_cast<size_t>(width) * height;
    std::vector<uint8_t> visited(totalPixels, 0);

    struct Run { uint32_t row, colStart, colEnd; };
    struct Seed { uint32_t x, y; };
    std::vector<Run> runs;
    std::vector<Seed> seedStack;

    uint32_t detCount = 0;

    for (uint32_t y = 0; y < height && detCount < maxDetections; ++y)
    {
        const uint8_t* rowPtr = imageData + static_cast<size_t>(y) * width;
        uint8_t* visitedRow = visited.data() + static_cast<size_t>(y) * width;

        for (uint32_t x = 0; x < width && detCount < maxDetections; ++x)
        {
            if (visitedRow[x]) continue;

            if (rowPtr[x] < thresh)
            {
                visitedRow[x] = 1;
                continue;
            }

            // RLE Seed Expansion
            runs.clear();
            seedStack.clear();
            seedStack.push_back({x, y});
            visitedRow[x] = 1;

            uint32_t area = 0;
            uint32_t bMinX = x, bMaxX = x, bMinY = y, bMaxY = y;
            uint8_t  minIntensity = 255;
            bool     overflow = false;

            while (!seedStack.empty())
            {
                auto [sx, sy] = seedStack.back();
                seedStack.pop_back();

                const uint8_t* imgRow = imageData + static_cast<size_t>(sy) * width;
                uint8_t* visRow = visited.data() + static_cast<size_t>(sy) * width;

                // Horizontal expansion: left
                uint32_t left = sx;
                while (left > 0 && !visRow[left - 1] && imgRow[left - 1] >= thresh)
                {
                    --left;
                    visRow[left] = 1;
                }
                // Horizontal expansion: right
                uint32_t right = sx;
                while (right + 1 < width && !visRow[right + 1] && imgRow[right + 1] >= thresh)
                {
                    ++right;
                    visRow[right] = 1;
                }
                for (uint32_t c = left; c <= right; ++c)
                    visRow[c] = 1;

                runs.push_back({sy, left, right});

                uint32_t runLen = right - left + 1;
                area += runLen;
                if (area > maxArea) { overflow = true; break; }

                if (left  < bMinX) bMinX = left;
                if (right > bMaxX) bMaxX = right;
                if (sy    < bMinY) bMinY = sy;
                if (sy    > bMaxY) bMaxY = sy;

                for (uint32_t c = left; c <= right; ++c)
                {
                    uint8_t v = imgRow[c];
                    if (v < minIntensity) minIntensity = v;
                }

                // Vertical neighbor seeding
                for (int dy = -1; dy <= 1; dy += 2)
                {
                    int ny = static_cast<int>(sy) + dy;
                    if (ny < 0 || ny >= static_cast<int>(height)) continue;
                    const uint8_t* nImgRow = imageData + static_cast<size_t>(ny) * width;
                    uint8_t* nVisRow = visited.data() + static_cast<size_t>(ny) * width;
                    for (uint32_t c = left; c <= right; ++c)
                    {
                        if (!nVisRow[c] && nImgRow[c] >= thresh)
                        {
                            nVisRow[c] = 1;
                            seedStack.push_back({c, static_cast<uint32_t>(ny)});
                        }
                    }
                }
            }

            // Drain remaining seeds if overflow
            if (overflow)
            {
                while (!seedStack.empty())
                {
                    auto [sx, sy] = seedStack.back();
                    seedStack.pop_back();
                    const uint8_t* imgRow = imageData + static_cast<size_t>(sy) * width;
                    uint8_t* visRow = visited.data() + static_cast<size_t>(sy) * width;
                    uint32_t left = sx;
                    while (left > 0 && !visRow[left - 1] && imgRow[left - 1] >= thresh)
                    { --left; visRow[left] = 1; }
                    uint32_t right = sx;
                    while (right + 1 < width && !visRow[right + 1] && imgRow[right + 1] >= thresh)
                    { ++right; visRow[right] = 1; }
                    for (uint32_t c = left; c <= right; ++c) visRow[c] = 1;
                    for (int dy = -1; dy <= 1; dy += 2)
                    {
                        int ny = static_cast<int>(sy) + dy;
                        if (ny < 0 || ny >= static_cast<int>(height)) continue;
                        const uint8_t* nImgRow = imageData + static_cast<size_t>(ny) * width;
                        uint8_t* nVisRow = visited.data() + static_cast<size_t>(ny) * width;
                        for (uint32_t c = left; c <= right; ++c)
                        {
                            if (!nVisRow[c] && nImgRow[c] >= thresh)
                            {
                                nVisRow[c] = 1;
                                seedStack.push_back({c, static_cast<uint32_t>(ny)});
                            }
                        }
                    }
                }
                continue;
            }

            // 面积过滤
            if (area < minArea)
                continue;

            // 边界框和长宽比过滤
            uint16_t bboxW = static_cast<uint16_t>(bMaxX - bMinX + 1);
            uint16_t bboxH = static_cast<uint16_t>(bMaxY - bMinY + 1);
            float aspect = static_cast<float>(std::min(bboxW, bboxH)) /
                           static_cast<float>(std::max(bboxW, bboxH));
            if (aspect < minAspect)
                continue;

            // ================================================================
            // 步骤 2: 背景减除加权质心 (single pass over RLE runs)
            //
            // 数值结果不变:
            //   - 使用像素中心坐标约定: (x+0.5, y+0.5)
            //   - 使用背景减除: weight = intensity - (minIntensity - kBgStep)
            //   - kBgStep = 2 (V3 8-bit 压缩的量化步长)
            //   - 验证精度: 平均 0.000388 px, 99.4% < 0.01 px
            // ================================================================
            static constexpr double kPixelCenterOffset = 0.5;
            static constexpr double kBgStep = 2.0;
            double bgLevel = static_cast<double>(minIntensity) - kBgStep;

            double sumX = 0.0, sumY = 0.0, sumW = 0.0;
            for (const auto& run : runs)
            {
                const uint8_t* imgRow = imageData + static_cast<size_t>(run.row) * width;
                double rowCoord = run.row + kPixelCenterOffset;
                for (uint32_t c = run.colStart; c <= run.colEnd; ++c)
                {
                    double weight = static_cast<double>(imgRow[c]) - bgLevel;
                    if (weight <= 0.0) weight = 0.0;

                    sumX += (c + kPixelCenterOffset) * weight;
                    sumY += rowCoord * weight;
                    sumW += weight;
                }
            }

            if (sumW <= 0.0)
                continue;

            // 填充 ftkRawData 输出
            ftkRawData& rd = rawData[detCount];
            rd.centerXPixels = static_cast<float>(sumX / sumW);
            rd.centerYPixels = static_cast<float>(sumY / sumW);
            rd.pixelsCount = area;
            rd.width = bboxW;
            rd.height = bboxH;
            rd.status = 0;  // 正常状态

            ++detCount;
        }
    }

    return detCount;
}

// ---------------------------------------------------------------------------
// 单点三角化
// ---------------------------------------------------------------------------

bool ReverseEngineeredPipeline::triangulate(
    const ftk3DPoint& leftPixel,
    const ftk3DPoint& rightPixel,
    ftk3DPoint* outPoint,
    float* outEpipolarError,
    float* outTriangulationError) const
{
    if (!m_stereo.isInitialized() || outPoint == nullptr)
        return false;

    float epiErr = 0.0f, triErr = 0.0f;
    bool ok = m_stereo.triangulate(leftPixel, rightPixel, *outPoint, epiErr, triErr);

    if (outEpipolarError)
        *outEpipolarError = epiErr;
    if (outTriangulationError)
        *outTriangulationError = triErr;

    return ok;
}

// ---------------------------------------------------------------------------
// 重投影
// ---------------------------------------------------------------------------

bool ReverseEngineeredPipeline::reproject(
    const ftk3DPoint& point3D,
    ftk3DPoint* outLeft,
    ftk3DPoint* outRight) const
{
    if (!m_stereo.isInitialized() || outLeft == nullptr || outRight == nullptr)
        return false;

    return m_stereo.reproject(point3D, *outLeft, *outRight);
}

// ---------------------------------------------------------------------------
// 批量极线匹配+三角化
// ---------------------------------------------------------------------------

uint32_t ReverseEngineeredPipeline::matchAndTriangulate(
    const ftkRawData* leftRawData,
    uint32_t leftCount,
    const ftkRawData* rightRawData,
    uint32_t rightCount,
    ftk3DFiducial* fiducials,
    uint32_t maxFiducials) const
{
    if (!m_stereo.isInitialized())
        return 0u;

    return m_stereo.matchAndTriangulate(
        leftRawData, leftCount, rightRawData, rightCount, fiducials, maxFiducials);
}

// ---------------------------------------------------------------------------
// 工具识别
// ---------------------------------------------------------------------------

bool ReverseEngineeredPipeline::registerGeometry(const ftkRigidBody& geometry)
{
    return m_matchMarkers.registerGeometry(geometry);
}

bool ReverseEngineeredPipeline::clearGeometry(uint32_t geometryId)
{
    return m_matchMarkers.clearGeometry(geometryId);
}

uint32_t ReverseEngineeredPipeline::matchMarkers(
    const ftk3DFiducial* fiducials,
    uint32_t fiducialCount,
    ftkMarker* markers,
    uint32_t maxMarkers)
{
    return m_matchMarkers.matchMarkers(fiducials, fiducialCount, markers, maxMarkers);
}

// ---------------------------------------------------------------------------
// 配置
// ---------------------------------------------------------------------------

void ReverseEngineeredPipeline::setEpipolarMaxDistance(float pixels)
{
    m_stereo.setEpipolarMaxDistance(pixels);
}

float ReverseEngineeredPipeline::getEpipolarMaxDistance() const
{
    return m_stereo.getEpipolarMaxDistance();
}

void ReverseEngineeredPipeline::setSegmenterConfig(const SegmenterConfig& config)
{
    m_segConfig = config;
}

ReverseEngineeredPipeline::SegmenterConfig
ReverseEngineeredPipeline::getSegmenterConfig() const
{
    return m_segConfig;
}

std::string ReverseEngineeredPipeline::version()
{
    return "reverse-engineered-pipeline v1.1.0 (2026-04, centroid fix)";
}

}  // namespace reverse_engineered
