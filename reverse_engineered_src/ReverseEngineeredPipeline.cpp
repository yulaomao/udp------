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

std::string ReverseEngineeredPipeline::version()
{
    return "reverse-engineered-pipeline v1.0.0 (2026-04)";
}

}  // namespace reverse_engineered
