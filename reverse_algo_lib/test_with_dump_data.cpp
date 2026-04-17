// ===========================================================================
// test_with_dump_data.cpp — 使用 dump_output 数据验证 StereoAlgoLib
//
// 读取 stereo99_DumpAllData 生成的 CSV 文件，调用逆向算法库
// 与 SDK 原始输出逐点对比，报告各项指标误差
//
// 用法:
//   ./test_with_dump_data <dump_output_dir>
//   例: ./test_with_dump_data ../../dump_output
// ===========================================================================

#include "StereoAlgoLib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace stereo_algo;

// ===========================================================================
// CSV 数据结构
// ===========================================================================

struct RawDet {
    int frameIdx;
    int detIdx;
    double cx, cy;
    int pixelCount;
    int width, height;
    int status;
};

struct Fid3D {
    int frameIdx;
    int fidIdx;
    double px, py, pz;
    int leftIdx, rightIdx;
    double epiErr, triErr;
    double probability;
    int status;
};

struct Reproj {
    int frameIdx;
    int fidIdx;
    double px, py, pz;
    double leftX, leftY;
    double rightX, rightY;
};

struct MarkerEntry {
    int frameIdx;
    int markerIdx;
    int geomId;
    int trackId;
    double regErr;
    double tx, ty, tz;
    double rot[3][3];
    int presenceMask;
    int fidCorresp[6];
    int status;
};

struct GeomPoint {
    int idx;
    double px, py, pz;
};

// ===========================================================================
// CSV 加载
// ===========================================================================

static vector<string> splitCSV(const string& line) {
    vector<string> parts;
    stringstream ss(line);
    string cell;
    while (getline(ss, cell, ',')) {
        parts.push_back(cell);
    }
    return parts;
}

static bool isNumericLine(const string& line) {
    if (line.empty() || line[0] == '#') return false;
    // Skip header lines
    try { stod(line.substr(0, line.find(','))); return true; }
    catch (...) { return false; }
}

static StereoCalibration loadCalibration(const string& dir) {
    StereoCalibration cal = {};
    ifstream f(dir + "/calibration.csv");
    if (!f.is_open()) {
        cerr << "ERROR: Cannot open calibration.csv" << endl;
        exit(1);
    }

    string line;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto parts = splitCSV(line);
        if (parts.empty()) continue;
        string key = parts[0];

        if (key == "left_focal_length" && parts.size() >= 3) {
            cal.leftCam.focalLength[0] = stod(parts[1]);
            cal.leftCam.focalLength[1] = stod(parts[2]);
        } else if (key == "left_optical_centre" && parts.size() >= 3) {
            cal.leftCam.opticalCentre[0] = stod(parts[1]);
            cal.leftCam.opticalCentre[1] = stod(parts[2]);
        } else if (key == "left_distortions" && parts.size() >= 6) {
            for (int i = 0; i < 5; ++i)
                cal.leftCam.distortion[i] = stod(parts[1 + i]);
        } else if (key == "left_skew" && parts.size() >= 2) {
            cal.leftCam.skew = stod(parts[1]);
        } else if (key == "right_focal_length" && parts.size() >= 3) {
            cal.rightCam.focalLength[0] = stod(parts[1]);
            cal.rightCam.focalLength[1] = stod(parts[2]);
        } else if (key == "right_optical_centre" && parts.size() >= 3) {
            cal.rightCam.opticalCentre[0] = stod(parts[1]);
            cal.rightCam.opticalCentre[1] = stod(parts[2]);
        } else if (key == "right_distortions" && parts.size() >= 6) {
            for (int i = 0; i < 5; ++i)
                cal.rightCam.distortion[i] = stod(parts[1 + i]);
        } else if (key == "right_skew" && parts.size() >= 2) {
            cal.rightCam.skew = stod(parts[1]);
        } else if (key == "translation" && parts.size() >= 4) {
            for (int i = 0; i < 3; ++i)
                cal.translation[i] = stod(parts[1 + i]);
        } else if (key == "rotation" && parts.size() >= 4) {
            for (int i = 0; i < 3; ++i)
                cal.rotation[i] = stod(parts[1 + i]);
        }
    }
    return cal;
}

static map<int, vector<RawDet>> loadRawData(const string& dir, const string& side) {
    map<int, vector<RawDet>> result;
    ifstream f(dir + "/raw_data_" + side + ".csv");
    string line;
    while (getline(f, line)) {
        if (!isNumericLine(line)) continue;
        auto p = splitCSV(line);
        if (p.size() < 8) continue;
        RawDet d;
        d.frameIdx = stoi(p[0]);
        d.detIdx = stoi(p[1]);
        d.cx = stod(p[2]);
        d.cy = stod(p[3]);
        d.pixelCount = stoi(p[4]);
        d.width = stoi(p[5]);
        d.height = stoi(p[6]);
        d.status = stoi(p[7]);
        result[d.frameIdx].push_back(d);
    }
    return result;
}

static map<int, vector<Fid3D>> loadFiducials(const string& dir) {
    map<int, vector<Fid3D>> result;
    ifstream f(dir + "/fiducials_3d.csv");
    string line;
    while (getline(f, line)) {
        if (!isNumericLine(line)) continue;
        auto p = splitCSV(line);
        if (p.size() < 11) continue;
        Fid3D d;
        d.frameIdx = stoi(p[0]);
        d.fidIdx = stoi(p[1]);
        d.px = stod(p[2]);
        d.py = stod(p[3]);
        d.pz = stod(p[4]);
        d.leftIdx = stoi(p[5]);
        d.rightIdx = stoi(p[6]);
        d.epiErr = stod(p[7]);
        d.triErr = stod(p[8]);
        d.probability = stod(p[9]);
        d.status = stoi(p[10]);
        result[d.frameIdx].push_back(d);
    }
    return result;
}

static map<int, vector<Reproj>> loadReprojections(const string& dir) {
    map<int, vector<Reproj>> result;
    ifstream f(dir + "/reprojections.csv");
    string line;
    while (getline(f, line)) {
        if (!isNumericLine(line)) continue;
        auto p = splitCSV(line);
        if (p.size() < 9) continue;
        Reproj d;
        d.frameIdx = stoi(p[0]);
        d.fidIdx = stoi(p[1]);
        d.px = stod(p[2]);
        d.py = stod(p[3]);
        d.pz = stod(p[4]);
        try {
            d.leftX = stod(p[5]);
            d.leftY = stod(p[6]);
            d.rightX = stod(p[7]);
            d.rightY = stod(p[8]);
        } catch (...) { continue; }
        if (isnan(d.leftX)) continue;
        result[d.frameIdx].push_back(d);
    }
    return result;
}

static pair<int, vector<GeomPoint>> loadGeometry(const string& dir) {
    int geomId = 0;
    vector<GeomPoint> pts;
    ifstream f(dir + "/geometry.csv");
    string line;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto p = splitCSV(line);
        if (p.size() < 2) continue;
        if (p[0] == "geometryId") { geomId = stoi(p[1]); continue; }
        if (p[0] == "version" || p[0] == "pointsCount" || p[0] == "divotsCount") continue;
        try {
            GeomPoint gp;
            gp.idx = stoi(p[0]);
            gp.px = stod(p[1]);
            gp.py = stod(p[2]);
            gp.pz = stod(p[3]);
            pts.push_back(gp);
        } catch (...) { continue; }
    }
    return { geomId, pts };
}

static map<int, vector<MarkerEntry>> loadMarkers(const string& dir) {
    map<int, vector<MarkerEntry>> result;
    ifstream f(dir + "/markers.csv");
    string line;
    while (getline(f, line)) {
        if (!isNumericLine(line)) continue;
        auto p = splitCSV(line);
        if (p.size() < 25) continue;
        MarkerEntry m;
        m.frameIdx = stoi(p[0]);
        m.markerIdx = stoi(p[1]);
        m.geomId = stoi(p[2]);
        m.trackId = stoi(p[3]);
        m.regErr = stod(p[4]);
        m.tx = stod(p[5]); m.ty = stod(p[6]); m.tz = stod(p[7]);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                m.rot[i][j] = stod(p[8 + i * 3 + j]);
        m.presenceMask = stoi(p[17]);
        for (int i = 0; i < 6; ++i) {
            // fidCorresp values can be 4294967295 (UINT32_MAX = INVALID_ID)
            unsigned long val = stoul(p[18 + i]);
            m.fidCorresp[i] = static_cast<int>(val > 0x7FFFFFFF ? -1 : static_cast<int>(val));
        }
        m.status = stoi(p[24]);
        result[m.frameIdx].push_back(m);
    }
    return result;
}

// ===========================================================================
// 统计辅助
// ===========================================================================

struct Stats {
    double mean, maxVal, stddev;
    size_t count;
};

static Stats computeStats(const vector<double>& v) {
    Stats s = {};
    s.count = v.size();
    if (v.empty()) return s;
    double sum = accumulate(v.begin(), v.end(), 0.0);
    s.mean = sum / v.size();
    s.maxVal = *max_element(v.begin(), v.end());
    double sqSum = 0.0;
    for (auto x : v) sqSum += (x - s.mean) * (x - s.mean);
    s.stddev = sqrt(sqSum / v.size());
    return s;
}

// ===========================================================================
// main
// ===========================================================================

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <dump_output_dir>" << endl;
        return 1;
    }

    string dataDir = argv[1];
    cout << "=== StereoAlgoLib Validation Test ===" << endl;
    cout << "Library version: " << StereoVision::version() << endl;
    cout << "Data directory:  " << dataDir << endl;
    cout << endl;

    // 加载数据
    StereoCalibration cal = loadCalibration(dataDir);
    auto rawLeft = loadRawData(dataDir, "left");
    auto rawRight = loadRawData(dataDir, "right");
    auto fiducials = loadFiducials(dataDir);
    auto reprojections = loadReprojections(dataDir);
    auto [geomId, geomPts] = loadGeometry(dataDir);
    auto markers = loadMarkers(dataDir);

    cout << "Loaded data:" << endl;
    cout << "  Left raw frames:   " << rawLeft.size() << endl;
    cout << "  Right raw frames:  " << rawRight.size() << endl;
    cout << "  Fiducial frames:   " << fiducials.size() << endl;
    cout << "  Reproj frames:     " << reprojections.size() << endl;
    cout << "  Geometry ID:       " << geomId << " (" << geomPts.size() << " points)" << endl;
    cout << "  Marker frames:     " << markers.size() << endl;
    cout << endl;

    // 初始化算法
    StereoVision sv;
    if (!sv.initialize(cal)) {
        cerr << "ERROR: Failed to initialize StereoVision" << endl;
        return 1;
    }
    cout << "StereoVision initialized OK" << endl << endl;

    bool allPass = true;

    // ===================================================================
    // TEST 1: 重投影 (3D → 2D vs SDK reprojections.csv)
    // ===================================================================
    cout << "==== TEST 1: Reprojection (3D → 2D) ====" << endl;
    {
        vector<double> leftErrs, rightErrs;

        for (auto& [frameIdx, rps] : reprojections) {
            for (auto& rp : rps) {
                Vec3 pos = { rp.px, rp.py, rp.pz };
                auto res = sv.reprojectTo2D(pos);
                if (!res.success) continue;

                double le = sqrt(pow(res.leftPixel.x - rp.leftX, 2) +
                                 pow(res.leftPixel.y - rp.leftY, 2));
                double re = sqrt(pow(res.rightPixel.x - rp.rightX, 2) +
                                 pow(res.rightPixel.y - rp.rightY, 2));
                leftErrs.push_back(le);
                rightErrs.push_back(re);
            }
        }

        auto ls = computeStats(leftErrs);
        auto rs = computeStats(rightErrs);
        cout << "  Samples:   " << ls.count << endl;
        cout << "  Left err:  mean=" << fixed << setprecision(6) << ls.mean
             << " max=" << ls.maxVal << " px" << endl;
        cout << "  Right err: mean=" << rs.mean
             << " max=" << rs.maxVal << " px" << endl;

        double passThreshold = 0.1;  // pixels
        int passCount = 0;
        for (size_t i = 0; i < leftErrs.size(); ++i)
            if (max(leftErrs[i], rightErrs[i]) < passThreshold) ++passCount;
        double passRate = ls.count > 0 ? 100.0 * passCount / ls.count : 0;
        cout << "  Pass rate (<" << passThreshold << " px): " << setprecision(1) << passRate << "%" << endl;

        bool pass = passRate > 95.0;
        cout << "  Result: " << (pass ? "PASS" : "FAIL") << endl << endl;
        if (!pass) allPass = false;
    }

    // ===================================================================
    // TEST 2: 三角化 (2D left + 2D right → 3D)
    // ===================================================================
    cout << "==== TEST 2: Triangulation (2D → 3D) ====" << endl;
    {
        vector<double> posErrs, epiErrs, triErrs;
        vector<double> goodPosErrs, goodEpiErrs, goodTriErrs;

        for (auto& [frameIdx, fids] : fiducials) {
            auto itL = rawLeft.find(frameIdx);
            auto itR = rawRight.find(frameIdx);
            if (itL == rawLeft.end() || itR == rawRight.end()) continue;

            for (auto& fid : fids) {
                if (fid.leftIdx >= static_cast<int>(itL->second.size()) ||
                    fid.rightIdx >= static_cast<int>(itR->second.size()))
                    continue;

                auto& ld = itL->second[fid.leftIdx];
                auto& rd = itR->second[fid.rightIdx];

                auto res = sv.triangulatePoint(ld.cx, ld.cy, rd.cx, rd.cy);
                if (!res.success) continue;

                double posDiff = sqrt(
                    pow(res.position.x - fid.px, 2) +
                    pow(res.position.y - fid.py, 2) +
                    pow(res.position.z - fid.pz, 2));

                double epiDiff = fabs(res.epipolarError - fid.epiErr);
                double triDiff = fabs(res.triangulationError - fid.triErr);

                posErrs.push_back(posDiff);
                epiErrs.push_back(epiDiff);
                triErrs.push_back(triDiff);

                // 好品质匹配: probability=1.0, 低极线误差, 合理深度
                bool isGood = (fid.probability > 0.9 &&
                               fabs(fid.epiErr) < 1.0 &&
                               fid.status == 0);
                if (isGood) {
                    goodPosErrs.push_back(posDiff);
                    goodEpiErrs.push_back(epiDiff);
                    goodTriErrs.push_back(triDiff);
                }
            }
        }

        auto ps = computeStats(posErrs);
        auto es = computeStats(epiErrs);
        auto ts = computeStats(triErrs);
        cout << "  All samples:   " << ps.count << endl;
        cout << "  3D pos diff:   mean=" << fixed << setprecision(6) << ps.mean
             << " max=" << ps.maxVal << " mm" << endl;
        cout << "  Epi err diff:  mean=" << es.mean
             << " max=" << es.maxVal << " px" << endl;
        cout << "  Tri err diff:  mean=" << ts.mean
             << " max=" << ts.maxVal << " mm" << endl;

        auto gps = computeStats(goodPosErrs);
        auto ges = computeStats(goodEpiErrs);
        auto gts = computeStats(goodTriErrs);
        cout << "  Good quality (prob>0.9, epi<1px, status=0): " << gps.count << " samples" << endl;
        cout << "    3D pos diff: mean=" << gps.mean
             << " max=" << gps.maxVal << " mm" << endl;
        cout << "    Epi diff:    mean=" << ges.mean
             << " max=" << ges.maxVal << " px" << endl;
        cout << "    Tri diff:    mean=" << gts.mean
             << " max=" << gts.maxVal << " mm" << endl;

        bool pass = gps.mean < 0.5 && ges.mean < 0.1;
        cout << "  Result: " << (pass ? "PASS" : "FAIL") << endl << endl;
        if (!pass) allPass = false;
    }

    // ===================================================================
    // TEST 3: Kabsch 配准 (使用 marker 数据验证)
    // ===================================================================
    cout << "==== TEST 3: Kabsch Registration (Marker Pose) ====" << endl;
    {
        vector<double> transErrs, rotErrs;

        if (geomPts.size() >= 3 && !markers.empty()) {
            for (auto& [frameIdx, mks] : markers) {
                auto itFid = fiducials.find(frameIdx);
                if (itFid == fiducials.end()) continue;

                for (auto& mk : mks) {
                    if (mk.geomId != geomId) continue;

                    // 收集匹配的模型点和测量点
                    vector<Vec3> modelVec, measVec;
                    for (int fi = 0; fi < static_cast<int>(geomPts.size()); ++fi) {
                        uint32_t fidIdx = static_cast<uint32_t>(mk.fidCorresp[fi]);
                        if (fidIdx == 0xFFFFFFFF) continue;

                        // 找到对应的 fiducial 3D 位置
                        bool found = false;
                        for (auto& fid : itFid->second) {
                            if (fid.fidIdx == static_cast<int>(fidIdx)) {
                                modelVec.push_back({ geomPts[fi].px, geomPts[fi].py, geomPts[fi].pz });
                                measVec.push_back({ fid.px, fid.py, fid.pz });
                                found = true;
                                break;
                            }
                        }
                    }

                    if (modelVec.size() < 3) continue;

                    auto reg = StereoVision::kabschRegistration(
                        modelVec.data(), measVec.data(), static_cast<uint32_t>(modelVec.size()));
                    if (!reg.success) continue;

                    // 比较平移
                    double tDiff = sqrt(
                        pow(reg.translation.x - mk.tx, 2) +
                        pow(reg.translation.y - mk.ty, 2) +
                        pow(reg.translation.z - mk.tz, 2));
                    transErrs.push_back(tDiff);

                    // 比较旋转: angle = acos((trace(R_ours * R_sdk^T) - 1) / 2)
                    Mat3 Rsdk;
                    for (int i = 0; i < 3; ++i)
                        for (int j = 0; j < 3; ++j)
                            Rsdk.m[i][j] = mk.rot[i][j];
                    Mat3 Rdiff = reg.rotation.mul(Rsdk.transpose());
                    double trace = Rdiff.m[0][0] + Rdiff.m[1][1] + Rdiff.m[2][2];
                    double cosA = (trace - 1.0) / 2.0;
                    cosA = max(-1.0, min(1.0, cosA));
                    double angleDeg = acos(cosA) * 180.0 / 3.14159265358979323846;
                    rotErrs.push_back(angleDeg);
                }
            }
        }

        if (!transErrs.empty()) {
            auto ts = computeStats(transErrs);
            auto rs = computeStats(rotErrs);
            cout << "  Samples:     " << ts.count << endl;
            cout << "  Trans diff:  mean=" << fixed << setprecision(6) << ts.mean
                 << " max=" << ts.maxVal << " mm" << endl;
            cout << "  Rot diff:    mean=" << rs.mean
                 << " max=" << rs.maxVal << " deg" << endl;

            bool pass = ts.mean < 1.0 && rs.mean < 0.5;
            cout << "  Result: " << (pass ? "PASS" : "FAIL") << endl;
            if (!pass) allPass = false;
        } else {
            cout << "  No marker data available for Kabsch test." << endl;
        }
        cout << endl;
    }

    // ===================================================================
    // TEST 4: 去畸变 round-trip (undistort → distort 应回到原始像素)
    // ===================================================================
    cout << "==== TEST 4: Undistort/Distort Round-trip ====" << endl;
    {
        vector<double> roundTripErrs;

        for (auto& [frameIdx, dets] : rawLeft) {
            for (auto& det : dets) {
                Vec2 norm = sv.undistortPoint(det.cx, det.cy, cal.leftCam);
                Vec2 px = sv.distortPoint(norm.x, norm.y, cal.leftCam);
                double err = sqrt(pow(px.x - det.cx, 2) + pow(px.y - det.cy, 2));
                roundTripErrs.push_back(err);
            }
        }
        for (auto& [frameIdx, dets] : rawRight) {
            for (auto& det : dets) {
                Vec2 norm = sv.undistortPoint(det.cx, det.cy, cal.rightCam);
                Vec2 px = sv.distortPoint(norm.x, norm.y, cal.rightCam);
                double err = sqrt(pow(px.x - det.cx, 2) + pow(px.y - det.cy, 2));
                roundTripErrs.push_back(err);
            }
        }

        auto s = computeStats(roundTripErrs);
        cout << "  Samples:    " << s.count << endl;
        cout << "  Round-trip: mean=" << fixed << setprecision(9) << s.mean
             << " max=" << s.maxVal << " px" << endl;

        bool pass = s.maxVal < 0.001;
        cout << "  Result: " << (pass ? "PASS" : "FAIL") << endl << endl;
        if (!pass) allPass = false;
    }

    // ===================================================================
    // Summary
    // ===================================================================
    cout << "========================================" << endl;
    cout << "Overall: " << (allPass ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << endl;
    cout << "========================================" << endl;

    return allPass ? 0 : 1;
}
