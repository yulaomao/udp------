// ===========================================================================
// 逆向工程还原 — MatchMarkers.cpp 实现
// 编译路径: ftkMatchMarker.cpp
// ===========================================================================

#include "MatchMarkers.h"
#include <cstring>
#include <limits>

namespace measurement {
namespace markerReco {

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------

MatchMarkers::MatchMarkers()
    : m_triangleMatchTolerance(0.01f)  // 1% 边长比容差
    , m_registrationMaxError(2.0f)     // 2mm 默认最大配准误差
    , m_obliquityEnabled(false)
{
}

MatchMarkers::~MatchMarkers() = default;

// ---------------------------------------------------------------------------
// 注册/移除几何体
// ---------------------------------------------------------------------------

bool MatchMarkers::registerGeometry(const ftkRigidBody& geometry)
{
    if (geometry.pointsCount < 3)
        return false;

    RegisteredGeometry reg;
    reg.rigidBody = geometry;

    // 存储模型点
    for (uint32_t i = 0; i < geometry.pointsCount; ++i)
    {
        math::Vec3d pt;
        pt[0] = geometry.fiducials[i].position.x;
        pt[1] = geometry.fiducials[i].position.y;
        pt[2] = geometry.fiducials[i].position.z;
        reg.modelPoints.push_back(pt);
    }

    // 预计算三角形不变量
    for (uint32_t i = 0; i < geometry.pointsCount; ++i)
    {
        for (uint32_t j = i + 1; j < geometry.pointsCount; ++j)
        {
            for (uint32_t k = j + 1; k < geometry.pointsCount; ++k)
            {
                TriangleInvariant tri;
                tri.idx[0] = i;
                tri.idx[1] = j;
                tri.idx[2] = k;

                math::Vec3d d01 = reg.modelPoints[j] - reg.modelPoints[i];
                math::Vec3d d02 = reg.modelPoints[k] - reg.modelPoints[i];
                math::Vec3d d12 = reg.modelPoints[k] - reg.modelPoints[j];

                tri.sides[0] = static_cast<float>(d01.norm());
                tri.sides[1] = static_cast<float>(d02.norm());
                tri.sides[2] = static_cast<float>(d12.norm());

                std::sort(tri.sides, tri.sides + 3);

                if (tri.sides[2] > 1e-6f)
                {
                    tri.sideRatios[0] = tri.sides[0] / tri.sides[2];
                    tri.sideRatios[1] = tri.sides[1] / tri.sides[2];
                }

                reg.triangles.push_back(tri);
            }
        }
    }

    m_geometries[geometry.geometryId] = std::move(reg);
    return true;
}

bool MatchMarkers::clearGeometry(uint32_t geometryId)
{
    return m_geometries.erase(geometryId) > 0;
}

// ---------------------------------------------------------------------------
// 从测量 fiducials 构建三角形
// ---------------------------------------------------------------------------

void MatchMarkers::_buildMeasuredTriangles(
    const ftk3DFiducial* fiducials,
    uint32_t count,
    std::vector<TriangleInvariant>& triangles)
{
    triangles.clear();

    for (uint32_t i = 0; i < count; ++i)
    {
        for (uint32_t j = i + 1; j < count; ++j)
        {
            for (uint32_t k = j + 1; k < count; ++k)
            {
                TriangleInvariant tri;
                tri.idx[0] = i;
                tri.idx[1] = j;
                tri.idx[2] = k;

                math::Vec3d pi, pj, pk;
                pi[0] = fiducials[i].positionMM.x;
                pi[1] = fiducials[i].positionMM.y;
                pi[2] = fiducials[i].positionMM.z;
                pj[0] = fiducials[j].positionMM.x;
                pj[1] = fiducials[j].positionMM.y;
                pj[2] = fiducials[j].positionMM.z;
                pk[0] = fiducials[k].positionMM.x;
                pk[1] = fiducials[k].positionMM.y;
                pk[2] = fiducials[k].positionMM.z;

                math::Vec3d d01 = pj - pi;
                math::Vec3d d02 = pk - pi;
                math::Vec3d d12 = pk - pj;

                tri.sides[0] = static_cast<float>(d01.norm());
                tri.sides[1] = static_cast<float>(d02.norm());
                tri.sides[2] = static_cast<float>(d12.norm());

                std::sort(tri.sides, tri.sides + 3);

                if (tri.sides[2] > 1e-6f)
                {
                    tri.sideRatios[0] = tri.sides[0] / tri.sides[2];
                    tri.sideRatios[1] = tri.sides[1] / tri.sides[2];
                }

                triangles.push_back(tri);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 三角形不变量匹配
// ---------------------------------------------------------------------------

bool MatchMarkers::_findCandidateMatches(
    const std::vector<TriangleInvariant>& measuredTriangles,
    const ftk3DFiducial* fiducials,
    uint32_t fiducialCount,
    std::vector<MatchCandidate>& candidates)
{
    candidates.clear();

    for (const auto& geomPair : m_geometries)
    {
        uint32_t geomId = geomPair.first;
        const RegisteredGeometry& geom = geomPair.second;

        // 对每个测量三角形，尝试匹配模型三角形
        // 使用投票机制: 多个匹配三角形投票确认身份
        std::map<uint32_t, uint32_t> fiducialVotes;  // fiducial index → 投票数

        for (const auto& measTri : measuredTriangles)
        {
            for (const auto& modelTri : geom.triangles)
            {
                // 比较三角形不变量（边长比）
                float diff0 = std::abs(measTri.sideRatios[0] - modelTri.sideRatios[0]);
                float diff1 = std::abs(measTri.sideRatios[1] - modelTri.sideRatios[1]);

                // 额外检查绝对边长的匹配（排除缩放）
                float scaleFactor = measTri.sides[2] / modelTri.sides[2];
                float scaleError = std::abs(scaleFactor - 1.0f);

                if (diff0 < m_triangleMatchTolerance &&
                    diff1 < m_triangleMatchTolerance &&
                    scaleError < 0.05f)  // 5% 尺度容差
                {
                    // 匹配成功 — 为涉及的 fiducials 投票
                    for (int m = 0; m < 3; ++m)
                    {
                        fiducialVotes[measTri.idx[m]]++;
                    }
                }
            }
        }

        // 检查是否有足够的投票确认匹配
        // 至少需要3个点匹配上
        std::vector<std::pair<uint32_t, uint32_t>> correspondences;
        uint32_t presenceMask = 0;

        // 暴力匹配: 对每个投票的测量点，找到最近的模型点
        for (const auto& vote : fiducialVotes)
        {
            if (vote.second < 2) continue;  // 至少被2个三角形支持

            uint32_t measIdx = vote.first;
            math::Vec3d measPt;
            measPt[0] = fiducials[measIdx].positionMM.x;
            measPt[1] = fiducials[measIdx].positionMM.y;
            measPt[2] = fiducials[measIdx].positionMM.z;

            // 这里简化处理: 在实际DLL中使用的是三角形顶点对应关系
            // 而不是最近点匹配
            correspondences.push_back({0, measIdx});
        }

        if (correspondences.size() >= 3)
        {
            MatchCandidate cand;
            cand.geometryId = geomId;
            cand.presenceMask = presenceMask;
            std::memset(cand.fiducialCorresp, 0xFF, sizeof(cand.fiducialCorresp));
            cand.pointCorrespondences = correspondences;

            candidates.push_back(cand);
        }
    }

    return !candidates.empty();
}

// ---------------------------------------------------------------------------
// Kabsch 刚体配准
// ---------------------------------------------------------------------------

float MatchMarkers::_computeRigidTransform(
    const RegisteredGeometry& geom,
    const MatchCandidate& candidate,
    const ftk3DFiducial* fiducials,
    float rotation[3][3],
    float translation[3])
{
    // DLL字符串: "Cannot perform registration with "
    // 这表明此函数有点数检查

    uint32_t numCorr = static_cast<uint32_t>(candidate.pointCorrespondences.size());
    if (numCorr < 3)
        return std::numeric_limits<float>::max();

    // 构建对应点集
    std::vector<math::Vec3d> modelPts(numCorr);
    std::vector<math::Vec3d> measPts(numCorr);

    for (uint32_t i = 0; i < numCorr; ++i)
    {
        uint32_t geomIdx = candidate.pointCorrespondences[i].first;
        uint32_t fidIdx = candidate.pointCorrespondences[i].second;

        modelPts[i] = geom.modelPoints[geomIdx];

        measPts[i][0] = fiducials[fidIdx].positionMM.x;
        measPts[i][1] = fiducials[fidIdx].positionMM.y;
        measPts[i][2] = fiducials[fidIdx].positionMM.z;
    }

    // Kabsch 算法
    // 步骤 1: 计算质心
    math::Vec3d centroidModel, centroidMeas;
    for (uint32_t i = 0; i < numCorr; ++i)
    {
        centroidModel = centroidModel + modelPts[i];
        centroidMeas = centroidMeas + measPts[i];
    }
    centroidModel = centroidModel * (1.0 / numCorr);
    centroidMeas = centroidMeas * (1.0 / numCorr);

    // 步骤 2: 去中心化
    for (uint32_t i = 0; i < numCorr; ++i)
    {
        modelPts[i] = modelPts[i] - centroidModel;
        measPts[i] = measPts[i] - centroidMeas;
    }

    // 步骤 3: 构建协方差矩阵 H = Σ model_i * meas_i^T
    math::Mat3d H;
    for (uint32_t i = 0; i < numCorr; ++i)
    {
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                H(r, c) += modelPts[i][r] * measPts[i][c];
    }

    // 步骤 4: SVD 分解 H = U * S * V^T
    // 使用 Jacobi 旋转的简化 3x3 SVD
    math::Mat3d U, V;
    math::Vec3d S;
    math::svd3x3(H, U, S, V);

    // 步骤 5: R = V * U^T
    math::Mat3d R = V * U.transpose();

    // 检查行列式（确保是旋转而非反射）
    double det = R.determinant3x3();
    if (det < 0)
    {
        // 翻转 V 的最后一列
        for (int r = 0; r < 3; ++r)
            V(r, 2) = -V(r, 2);
        R = V * U.transpose();
    }

    // 步骤 6: t = centroid_meas - R * centroid_model
    math::Vec3d t = centroidMeas - R * centroidModel;

    // 输出旋转矩阵
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            rotation[r][c] = static_cast<float>(R(r, c));

    // 输出平移向量
    for (int i = 0; i < 3; ++i)
        translation[i] = static_cast<float>(t[i]);

    // 计算配准误差
    // 还原原始点并计算误差
    for (uint32_t i = 0; i < numCorr; ++i)
    {
        modelPts[i] = modelPts[i] + centroidModel;
        measPts[i] = measPts[i] + centroidMeas;
    }

    double totalError = 0.0;
    for (uint32_t i = 0; i < numCorr; ++i)
    {
        math::Vec3d transformed = R * modelPts[i] + t;
        math::Vec3d diff = transformed - measPts[i];
        totalError += diff.dot(diff);
    }

    return static_cast<float>(std::sqrt(totalError / numCorr));
}

// ---------------------------------------------------------------------------
// 倾斜补偿
// ---------------------------------------------------------------------------

void MatchMarkers::_applyObliquityCompensation(
    const RegisteredGeometry& geom,
    const MatchCandidate& candidate,
    const ftk3DFiducial* fiducials,
    ftkMarker& marker)
{
    // DLL字符串: "partially obliquity corrected marker detected"
    //
    // 倾斜补偿校正 LED/反射球因观察角度引起的检测位置偏移
    // 当 fiducial 以倾斜角度被观察时，检测到的中心会偏移
    // 该补偿使用 fiducial 的法向量和观察方向计算校正量
    //
    // 此处为简化实现 — 实际 DLL 中的算法更复杂

    if (!m_obliquityEnabled)
        return;

    // 对每个匹配的 fiducial 计算倾斜校正
    for (const auto& corr : candidate.pointCorrespondences)
    {
        uint32_t geomIdx = corr.first;
        if (geomIdx >= geom.rigidBody.pointsCount)
            continue;

        const ftkFiducial& fid = geom.rigidBody.fiducials[geomIdx];

        // 检查是否有法向量信息
        math::Vec3d normal;
        normal[0] = fid.normalVector.x;
        normal[1] = fid.normalVector.y;
        normal[2] = fid.normalVector.z;

        if (normal.norm() < 1e-6)
            continue;  // 无法向量信息，跳过

        // 标记为已校正
        marker.status.Corrected = 1;
    }
}

// ---------------------------------------------------------------------------
// 主匹配入口
// ---------------------------------------------------------------------------

uint32_t MatchMarkers::matchMarkers(
    const ftk3DFiducial* fiducials,
    uint32_t fiducialCount,
    ftkMarker* markers,
    uint32_t maxMarkers)
{
    if (fiducialCount < 3 || m_geometries.empty())
        return 0;

    // 步骤 1: 构建测量三角形
    std::vector<TriangleInvariant> measuredTriangles;
    _buildMeasuredTriangles(fiducials, fiducialCount, measuredTriangles);

    // 步骤 2: 匹配候选
    std::vector<MatchCandidate> candidates;
    if (!_findCandidateMatches(measuredTriangles, fiducials, fiducialCount, candidates))
        return 0;

    // 步骤 3: 对每个候选计算刚体变换
    uint32_t markerCount = 0;

    for (const auto& candidate : candidates)
    {
        if (markerCount >= maxMarkers)
            break;

        auto it = m_geometries.find(candidate.geometryId);
        if (it == m_geometries.end())
            continue;

        ftkMarker& marker = markers[markerCount];
        std::memset(&marker, 0, sizeof(ftkMarker));

        marker.geometryId = candidate.geometryId;
        marker.geometryPresenceMask = candidate.presenceMask;
        std::memcpy(marker.fiducialCorresp, candidate.fiducialCorresp,
                    sizeof(marker.fiducialCorresp));

        // 计算刚体变换
        float error = _computeRigidTransform(
            it->second, candidate, fiducials,
            marker.rotation, marker.translationMM);

        marker.registrationErrorMM = error;

        // 配准误差检查
        if (error > m_registrationMaxError)
            continue;

        // 可选: 倾斜补偿
        _applyObliquityCompensation(it->second, candidate, fiducials, marker);

        ++markerCount;
    }

    return markerCount;
}

}  // namespace markerReco
}  // namespace measurement
