// ===========================================================================
// StereoAlgoPipeline.cpp — SDK 兼容管线封装实现
//
// 将 stereo_algo::StereoVision 的纯算法接口适配为 SDK 类型
// (ftkStereoParameters, ftk3DPoint, ftk3DFiducial, ftkMarker 等)
// ===========================================================================

#include "StereoAlgoPipeline.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>

namespace stereo_algo {

// ===========================================================================
// 构造/析构
// ===========================================================================

StereoAlgoPipeline::StereoAlgoPipeline()
    : m_epipolarMaxDist(5.0f)  // SDK 默认值: cstEpipolarDistDef = 5.0f
{
}

StereoAlgoPipeline::~StereoAlgoPipeline() = default;

std::string StereoAlgoPipeline::version()
{
    return std::string("StereoAlgoPipeline v2.0.0 (based on ") +
           StereoVision::version() + ")";
}

// ===========================================================================
// 初始化
// ===========================================================================

bool StereoAlgoPipeline::initialize(const ftkStereoParameters& params)
{
    StereoCalibration cal = {};

    // 左相机
    cal.leftCam.focalLength[0] = params.LeftCamera.FocalLength[0];
    cal.leftCam.focalLength[1] = params.LeftCamera.FocalLength[1];
    cal.leftCam.opticalCentre[0] = params.LeftCamera.OpticalCentre[0];
    cal.leftCam.opticalCentre[1] = params.LeftCamera.OpticalCentre[1];
    cal.leftCam.distortion[0] = params.LeftCamera.Distorsions[0];
    cal.leftCam.distortion[1] = params.LeftCamera.Distorsions[1];
    cal.leftCam.distortion[2] = params.LeftCamera.Distorsions[2];
    cal.leftCam.distortion[3] = params.LeftCamera.Distorsions[3];
    cal.leftCam.distortion[4] = params.LeftCamera.Distorsions[4];
    cal.leftCam.skew = params.LeftCamera.Skew;

    // 右相机
    cal.rightCam.focalLength[0] = params.RightCamera.FocalLength[0];
    cal.rightCam.focalLength[1] = params.RightCamera.FocalLength[1];
    cal.rightCam.opticalCentre[0] = params.RightCamera.OpticalCentre[0];
    cal.rightCam.opticalCentre[1] = params.RightCamera.OpticalCentre[1];
    cal.rightCam.distortion[0] = params.RightCamera.Distorsions[0];
    cal.rightCam.distortion[1] = params.RightCamera.Distorsions[1];
    cal.rightCam.distortion[2] = params.RightCamera.Distorsions[2];
    cal.rightCam.distortion[3] = params.RightCamera.Distorsions[3];
    cal.rightCam.distortion[4] = params.RightCamera.Distorsions[4];
    cal.rightCam.skew = params.RightCamera.Skew;

    // 外参
    cal.translation[0] = params.Translation[0];
    cal.translation[1] = params.Translation[1];
    cal.translation[2] = params.Translation[2];
    cal.rotation[0] = params.Rotation[0];
    cal.rotation[1] = params.Rotation[1];
    cal.rotation[2] = params.Rotation[2];

    m_cal = cal;
    return m_sv.initialize(cal);
}

bool StereoAlgoPipeline::isInitialized() const
{
    return m_sv.isInitialized();
}

// ===========================================================================
// 三角化
// ===========================================================================

bool StereoAlgoPipeline::triangulate(
    const ftk3DPoint& leftPixel,
    const ftk3DPoint& rightPixel,
    ftk3DPoint* outPoint,
    float* outEpipolarError,
    float* outTriangulationError) const
{
    if (!m_sv.isInitialized() || !outPoint) return false;

    auto res = m_sv.triangulatePoint(
        leftPixel.x, leftPixel.y,
        rightPixel.x, rightPixel.y);

    if (!res.success) return false;

    outPoint->x = static_cast<floatXX>(res.position.x);
    outPoint->y = static_cast<floatXX>(res.position.y);
    outPoint->z = static_cast<floatXX>(res.position.z);

    if (outEpipolarError)
        *outEpipolarError = static_cast<float>(res.epipolarError);
    if (outTriangulationError)
        *outTriangulationError = static_cast<float>(res.triangulationError);

    return true;
}

// ===========================================================================
// 重投影
// ===========================================================================

bool StereoAlgoPipeline::reproject(
    const ftk3DPoint& point3D,
    ftk3DPoint* outLeft,
    ftk3DPoint* outRight) const
{
    if (!m_sv.isInitialized() || !outLeft || !outRight) return false;

    Vec3 pos = { point3D.x, point3D.y, point3D.z };
    auto res = m_sv.reprojectTo2D(pos);

    if (!res.success) return false;

    outLeft->x = static_cast<floatXX>(res.leftPixel.x);
    outLeft->y = static_cast<floatXX>(res.leftPixel.y);
    outLeft->z = 0.0f;

    outRight->x = static_cast<floatXX>(res.rightPixel.x);
    outRight->y = static_cast<floatXX>(res.rightPixel.y);
    outRight->z = 0.0f;

    return true;
}

// ===========================================================================
// 批量极线匹配+三角化
//
// 实现与 SDK 一致的极线匹配流程 (还原 DLL Match2D3D):
//   1. 对每个左图点, 计算极线, 找所有右候选 (距离 < epipolarMaxDist)
//   2. 双向验证: 反向极线距离也要在阈值内
//   3. 所有通过验证的候选都产生 fiducial (允许一对多, probability=1/n)
// ===========================================================================

uint32_t StereoAlgoPipeline::matchAndTriangulate(
    const ftkRawData* leftRawData,
    uint32_t leftCount,
    const ftkRawData* rightRawData,
    uint32_t rightCount,
    ftk3DFiducial* fiducials,
    uint32_t maxFiducials) const
{
    if (!m_sv.isInitialized() || !leftRawData || !rightRawData ||
        !fiducials || maxFiducials == 0)
        return 0;

    // 转换为 Detection2D 格式
    std::vector<Detection2D> leftDets(leftCount);
    std::vector<Detection2D> rightDets(rightCount);

    for (uint32_t i = 0; i < leftCount; ++i) {
        leftDets[i].centerX = leftRawData[i].centerXPixels;
        leftDets[i].centerY = leftRawData[i].centerYPixels;
        leftDets[i].index = i;
    }
    for (uint32_t i = 0; i < rightCount; ++i) {
        rightDets[i].centerX = rightRawData[i].centerXPixels;
        rightDets[i].centerY = rightRawData[i].centerYPixels;
        rightDets[i].index = i;
    }

    // 使用核心极线匹配算法
    std::vector<EpipolarMatchResult> results(maxFiducials);
    uint32_t matchCount = m_sv.matchEpipolar(
        leftDets.data(), leftCount,
        rightDets.data(), rightCount,
        results.data(), maxFiducials,
        m_epipolarMaxDist);

    // 转换为 ftk3DFiducial 格式
    for (uint32_t i = 0; i < matchCount; ++i) {
        ftk3DFiducial& fid = fiducials[i];
        std::memset(&fid, 0, sizeof(fid));
        fid.leftIndex = results[i].leftIndex;
        fid.rightIndex = results[i].rightIndex;
        fid.positionMM.x = static_cast<floatXX>(results[i].position.x);
        fid.positionMM.y = static_cast<floatXX>(results[i].position.y);
        fid.positionMM.z = static_cast<floatXX>(results[i].position.z);
        fid.epipolarErrorPixels = static_cast<floatXX>(results[i].epipolarError);
        fid.triangulationErrorMM = static_cast<floatXX>(results[i].triangulationError);
        fid.probability = static_cast<float>(results[i].probability);
    }

    return matchCount;
}

// ===========================================================================
// 几何体注册
// ===========================================================================

bool StereoAlgoPipeline::registerGeometry(const ftkRigidBody& geometry)
{
    RegisteredGeometry rg;
    rg.geometryId = geometry.geometryId;
    rg.pointsCount = geometry.pointsCount;

    for (uint32_t i = 0; i < geometry.pointsCount && i < 64; ++i) {
        rg.points[i] = { geometry.fiducials[i].position.x,
                          geometry.fiducials[i].position.y,
                          geometry.fiducials[i].position.z };
    }

    m_geometries[geometry.geometryId] = rg;
    return true;
}

bool StereoAlgoPipeline::clearGeometry(uint32_t geometryId)
{
    return m_geometries.erase(geometryId) > 0;
}

// ===========================================================================
// 工具匹配 — 三角形不变量 + Kabsch 配准
//
// 简化实现:
//   1. 对每个注册几何体, 尝试在 fiducial 点中找到匹配
//   2. 使用三角形边长比作为不变量
//   3. 找到 >= 3 个匹配后, 用 Kabsch 计算刚体变换
// ===========================================================================

uint32_t StereoAlgoPipeline::matchMarkers(
    const ftk3DFiducial* fiducials,
    uint32_t fiducialCount,
    ftkMarker* markers,
    uint32_t maxMarkers)
{
    if (!fiducials || !markers || maxMarkers == 0 || fiducialCount < 3)
        return 0;

    uint32_t markerCount = 0;

    for (auto& [geomId, geom] : m_geometries) {
        if (markerCount >= maxMarkers) break;
        if (geom.pointsCount < 3 || fiducialCount < geom.pointsCount) continue;

        // 计算几何体模型中所有三角形的边长
        uint32_t n = geom.pointsCount;
        struct Triangle {
            uint32_t i, j, k;
            double sides[3];  // 排序后的边长
        };

        std::vector<Triangle> modelTriangles;
        for (uint32_t i = 0; i < n; ++i) {
            for (uint32_t j = i + 1; j < n; ++j) {
                for (uint32_t k = j + 1; k < n; ++k) {
                    double d01 = std::sqrt(
                        std::pow(geom.points[i].x - geom.points[j].x, 2) +
                        std::pow(geom.points[i].y - geom.points[j].y, 2) +
                        std::pow(geom.points[i].z - geom.points[j].z, 2));
                    double d02 = std::sqrt(
                        std::pow(geom.points[i].x - geom.points[k].x, 2) +
                        std::pow(geom.points[i].y - geom.points[k].y, 2) +
                        std::pow(geom.points[i].z - geom.points[k].z, 2));
                    double d12 = std::sqrt(
                        std::pow(geom.points[j].x - geom.points[k].x, 2) +
                        std::pow(geom.points[j].y - geom.points[k].y, 2) +
                        std::pow(geom.points[j].z - geom.points[k].z, 2));

                    Triangle tri;
                    tri.i = i; tri.j = j; tri.k = k;
                    tri.sides[0] = d01; tri.sides[1] = d02; tri.sides[2] = d12;
                    std::sort(tri.sides, tri.sides + 3);
                    modelTriangles.push_back(tri);
                }
            }
        }

        // 对每个 fiducial 三角形, 尝试匹配
        struct MatchCandidate {
            uint32_t modelIdx[3];       // 几何体点索引
            uint32_t fidIdx[3];         // fiducial 索引
            double sideError;           // 边长匹配误差
        };

        std::vector<MatchCandidate> matchCands;
        double sideThreshold = 2.0;  // mm

        for (uint32_t fi = 0; fi < fiducialCount; ++fi) {
            for (uint32_t fj = fi + 1; fj < fiducialCount; ++fj) {
                for (uint32_t fk = fj + 1; fk < fiducialCount; ++fk) {
                    Vec3 fp[3] = {
                        { fiducials[fi].positionMM.x, fiducials[fi].positionMM.y, fiducials[fi].positionMM.z },
                        { fiducials[fj].positionMM.x, fiducials[fj].positionMM.y, fiducials[fj].positionMM.z },
                        { fiducials[fk].positionMM.x, fiducials[fk].positionMM.y, fiducials[fk].positionMM.z },
                    };

                    double fd[3];
                    fd[0] = std::sqrt(std::pow(fp[0].x-fp[1].x,2)+std::pow(fp[0].y-fp[1].y,2)+std::pow(fp[0].z-fp[1].z,2));
                    fd[1] = std::sqrt(std::pow(fp[0].x-fp[2].x,2)+std::pow(fp[0].y-fp[2].y,2)+std::pow(fp[0].z-fp[2].z,2));
                    fd[2] = std::sqrt(std::pow(fp[1].x-fp[2].x,2)+std::pow(fp[1].y-fp[2].y,2)+std::pow(fp[1].z-fp[2].z,2));
                    std::sort(fd, fd + 3);

                    for (const auto& mt : modelTriangles) {
                        double err = std::fabs(fd[0] - mt.sides[0]) +
                                     std::fabs(fd[1] - mt.sides[1]) +
                                     std::fabs(fd[2] - mt.sides[2]);
                        if (err < sideThreshold) {
                            // 确定对应关系 (找使误差最小的排列)
                            uint32_t fidIdxArr[3] = { fi, fj, fk };
                            uint32_t modelIdxArr[3] = { mt.i, mt.j, mt.k };

                            // 暴力尝试 6 种排列 (3! = 6)
                            uint32_t bestPerm[3] = { 0, 1, 2 };
                            double bestPermErr = std::numeric_limits<double>::max();

                            uint32_t perms[6][3] = {
                                {0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}
                            };
                            for (auto& p : perms) {
                                double permErr = 0.0;
                                for (int d = 0; d < 3; ++d) {
                                    double dx = fp[p[d]].x - geom.points[modelIdxArr[d]].x;
                                    double dy = fp[p[d]].y - geom.points[modelIdxArr[d]].y;
                                    double dz = fp[p[d]].z - geom.points[modelIdxArr[d]].z;
                                    // Can't use Euclidean distance for assignment since
                                    // points are in different frames; use side lengths
                                }

                                // Use side length comparison for this permutation
                                double d01_f = std::sqrt(
                                    std::pow(fp[p[0]].x-fp[p[1]].x,2)+
                                    std::pow(fp[p[0]].y-fp[p[1]].y,2)+
                                    std::pow(fp[p[0]].z-fp[p[1]].z,2));
                                double d02_f = std::sqrt(
                                    std::pow(fp[p[0]].x-fp[p[2]].x,2)+
                                    std::pow(fp[p[0]].y-fp[p[2]].y,2)+
                                    std::pow(fp[p[0]].z-fp[p[2]].z,2));
                                double d12_f = std::sqrt(
                                    std::pow(fp[p[1]].x-fp[p[2]].x,2)+
                                    std::pow(fp[p[1]].y-fp[p[2]].y,2)+
                                    std::pow(fp[p[1]].z-fp[p[2]].z,2));

                                double d01_m = std::sqrt(
                                    std::pow(geom.points[modelIdxArr[0]].x-geom.points[modelIdxArr[1]].x,2)+
                                    std::pow(geom.points[modelIdxArr[0]].y-geom.points[modelIdxArr[1]].y,2)+
                                    std::pow(geom.points[modelIdxArr[0]].z-geom.points[modelIdxArr[1]].z,2));
                                double d02_m = std::sqrt(
                                    std::pow(geom.points[modelIdxArr[0]].x-geom.points[modelIdxArr[2]].x,2)+
                                    std::pow(geom.points[modelIdxArr[0]].y-geom.points[modelIdxArr[2]].y,2)+
                                    std::pow(geom.points[modelIdxArr[0]].z-geom.points[modelIdxArr[2]].z,2));
                                double d12_m = std::sqrt(
                                    std::pow(geom.points[modelIdxArr[1]].x-geom.points[modelIdxArr[2]].x,2)+
                                    std::pow(geom.points[modelIdxArr[1]].y-geom.points[modelIdxArr[2]].y,2)+
                                    std::pow(geom.points[modelIdxArr[1]].z-geom.points[modelIdxArr[2]].z,2));

                                permErr = std::fabs(d01_f - d01_m) +
                                          std::fabs(d02_f - d02_m) +
                                          std::fabs(d12_f - d12_m);

                                if (permErr < bestPermErr) {
                                    bestPermErr = permErr;
                                    bestPerm[0] = p[0];
                                    bestPerm[1] = p[1];
                                    bestPerm[2] = p[2];
                                }
                            }

                            MatchCandidate mc;
                            for (int d = 0; d < 3; ++d) {
                                mc.modelIdx[d] = modelIdxArr[d];
                                mc.fidIdx[d] = fidIdxArr[bestPerm[d]];
                            }
                            mc.sideError = bestPermErr;
                            matchCands.push_back(mc);
                        }
                    }
                }
            }
        }

        if (matchCands.empty()) continue;

        // 选择误差最小的候选
        std::sort(matchCands.begin(), matchCands.end(),
                  [](const MatchCandidate& a, const MatchCandidate& b) {
                      return a.sideError < b.sideError;
                  });

        const auto& best = matchCands[0];

        // 用 Kabsch 注册
        Vec3 modelPts[3], measPts[3];
        for (int d = 0; d < 3; ++d) {
            modelPts[d] = geom.points[best.modelIdx[d]];
            measPts[d] = { fiducials[best.fidIdx[d]].positionMM.x,
                           fiducials[best.fidIdx[d]].positionMM.y,
                           fiducials[best.fidIdx[d]].positionMM.z };
        }

        auto reg = StereoVision::kabschRegistration(modelPts, measPts, 3);
        if (!reg.success) continue;

        // 填充 ftkMarker
        ftkMarker& mk = markers[markerCount];
        std::memset(&mk, 0, sizeof(mk));
        mk.geometryId = geomId;
        mk.id = markerCount;
        mk.registrationErrorMM = static_cast<floatXX>(reg.rmsError);

        mk.translationMM[0] = static_cast<floatXX>(reg.translation.x);
        mk.translationMM[1] = static_cast<floatXX>(reg.translation.y);
        mk.translationMM[2] = static_cast<floatXX>(reg.translation.z);

        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                mk.rotation[i][j] = static_cast<floatXX>(reg.rotation.m[i][j]);

        // Presence mask and fiducial correspondence
        mk.geometryPresenceMask = 0;
        for (uint32_t i = 0; i < FTK_MAX_FIDUCIALS; ++i)
            mk.fiducialCorresp[i] = INVALID_ID;

        for (int d = 0; d < 3; ++d) {
            mk.geometryPresenceMask |= (1u << best.modelIdx[d]);
            mk.fiducialCorresp[best.modelIdx[d]] = best.fidIdx[d];
        }

        ++markerCount;
    }

    return markerCount;
}

// ===========================================================================
// 配置
// ===========================================================================

void StereoAlgoPipeline::setEpipolarMaxDistance(float pixels)
{
    m_epipolarMaxDist = pixels;
}

float StereoAlgoPipeline::getEpipolarMaxDistance() const
{
    return m_epipolarMaxDist;
}

}  // namespace stereo_algo
