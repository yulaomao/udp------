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

    // 创建 Image 适配器
    // SegmenterV21 使用模板化的 Image 接口
    // 这里我们直接实现内联的分割逻辑，等价于 SegmenterV21::segment()

    using namespace measurement::segmenter;

    // ================================================================
    // 步骤 1: 种子扩展分割 (Seed Expansion)
    // DLL: 0x3CCE0-0x3DB1B
    // ================================================================
    const uint32_t threshold = m_segConfig.seedThreshold;
    const uint32_t minArea = m_segConfig.blobMinSurface;
    const uint32_t maxArea = m_segConfig.blobMaxSurface;
    const float minAspect = m_segConfig.blobMinAspectRatio;

    std::vector<bool> visited(width * height, false);
    uint32_t detCount = 0;

    for (uint32_t y = 0; y < height && detCount < maxDetections; ++y)
    {
        for (uint32_t x = 0; x < width && detCount < maxDetections; ++x)
        {
            uint32_t idx = y * width + x;
            if (visited[idx])
                continue;

            uint8_t val = imageData[idx];
            if (val < threshold)
            {
                visited[idx] = true;
                continue;
            }

            // 种子扩展: 洪泛填充 (4-连通)
            struct Pixel { uint32_t x, y; uint8_t val; };
            std::vector<Pixel> blobPixels;
            std::vector<std::pair<uint32_t, uint32_t>> stack;
            stack.push_back({x, y});
            visited[idx] = true;

            while (!stack.empty())
            {
                auto [cx, cy] = stack.back();
                stack.pop_back();
                blobPixels.push_back({cx, cy, imageData[cy * width + cx]});

                // 4-连通邻域
                const int dx[] = {-1, 1, 0, 0};
                const int dy[] = {0, 0, -1, 1};
                for (int d = 0; d < 4; ++d)
                {
                    int nx = static_cast<int>(cx) + dx[d];
                    int ny = static_cast<int>(cy) + dy[d];
                    if (nx >= 0 && nx < static_cast<int>(width) &&
                        ny >= 0 && ny < static_cast<int>(height))
                    {
                        uint32_t nidx = ny * width + nx;
                        if (!visited[nidx] && imageData[nidx] >= threshold)
                        {
                            visited[nidx] = true;
                            stack.push_back({static_cast<uint32_t>(nx),
                                             static_cast<uint32_t>(ny)});
                        }
                    }
                }
            }

            // 面积过滤
            uint32_t area = static_cast<uint32_t>(blobPixels.size());
            if (area < minArea || area > maxArea)
                continue;

            // 边界框和长宽比过滤
            uint32_t minX = UINT32_MAX, maxX = 0, minY = UINT32_MAX, maxY = 0;
            for (const auto& p : blobPixels)
            {
                if (p.x < minX) minX = p.x;
                if (p.x > maxX) maxX = p.x;
                if (p.y < minY) minY = p.y;
                if (p.y > maxY) maxY = p.y;
            }
            uint16_t bboxW = static_cast<uint16_t>(maxX - minX + 1);
            uint16_t bboxH = static_cast<uint16_t>(maxY - minY + 1);
            float aspect = static_cast<float>(std::min(bboxW, bboxH)) /
                           static_cast<float>(std::max(bboxW, bboxH));
            if (aspect < minAspect)
                continue;

            // ================================================================
            // 步骤 2: 背景减除加权质心
            // DLL 验证结果:
            //   - 使用像素中心坐标约定: (x+0.5, y+0.5)
            //   - 使用背景减除: weight = intensity - (minIntensity - kBgStep)
            //   - kBgStep = 2 (V3 8-bit 压缩的量化步长)
            //   - 验证精度: 平均 0.000388 px, 99.4% < 0.01 px
            // ================================================================
            uint8_t minIntensity = 255;
            for (const auto& p : blobPixels)
            {
                if (p.val < minIntensity) minIntensity = p.val;
            }

            static constexpr double kPixelCenterOffset = 0.5;
            static constexpr double kBgStep = 2.0;
            double bgLevel = static_cast<double>(minIntensity) - kBgStep;

            double sumX = 0.0, sumY = 0.0, sumW = 0.0;
            for (const auto& p : blobPixels)
            {
                double weight = static_cast<double>(p.val) - bgLevel;
                if (weight <= 0.0) weight = 0.0;

                sumX += (p.x + kPixelCenterOffset) * weight;
                sumY += (p.y + kPixelCenterOffset) * weight;
                sumW += weight;
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
