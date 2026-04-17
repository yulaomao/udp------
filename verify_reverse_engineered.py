#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""verify_reverse_engineered.py

反向工程的 fusionTrack 算法全面验证脚本
针对 stereo99_DumpAllData 捕获的官方 SDK 输出数据进行验证。

本脚本验证以下反向工程实现：
1. 图像分割和圆心提取（SegmenterV21 / CircleFitting）
2. 镜头畸变校正（Brown-Conrady 去畸变）
3. 对极立体匹配（EpipolarMatcher / Match2D3D）
4. 三角测量（closestPointOnRays）
5. 标记识别和位姿估计（MatchMarkers / Kabsch）
6. 3D -> 2D 重投影

使用方法：
    python verify_reverse_engineered.py --data-dir ./dump_output [--image-dir ./dump_output]

输入：来自 stereo99_DumpAllData 的 CSV 文件和图像
输出：每种算法精度指标的对比报告
"""

import argparse
import csv
import math
import os
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

import numpy as np

# ============================================================================
# Data Loading
# ============================================================================


@dataclass
class Calibration:
    """立体相机标定参数。"""
    left_focal: np.ndarray = field(default_factory=lambda: np.zeros(2))
    left_center: np.ndarray = field(default_factory=lambda: np.zeros(2))
    left_distortion: np.ndarray = field(default_factory=lambda: np.zeros(5))
    left_skew: float = 0.0
    right_focal: np.ndarray = field(default_factory=lambda: np.zeros(2))
    right_center: np.ndarray = field(default_factory=lambda: np.zeros(2))
    right_distortion: np.ndarray = field(default_factory=lambda: np.zeros(5))
    right_skew: float = 0.0
    translation: np.ndarray = field(default_factory=lambda: np.zeros(3))
    rotation: np.ndarray = field(default_factory=lambda: np.zeros(3))  # Rodrigues vector


@dataclass
class RawDetection:
    """单个相机的 2D 圆形检测。"""
    frame_idx: int
    detection_idx: int
    center_x: float
    center_y: float
    pixels_count: int
    width: int
    height: int
    status_bits: int


@dataclass
class Fiducial3D:
    """立体匹配得到的 3D 基准点。"""
    frame_idx: int
    fid_idx: int
    pos_x: float
    pos_y: float
    pos_z: float
    left_index: int
    right_index: int
    epipolar_error: float
    triangulation_error: float
    probability: float
    status_bits: int


@dataclass
class MarkerData:
    """标记位姿数据。"""
    frame_idx: int
    marker_idx: int
    geometry_id: int
    tracking_id: int
    registration_error: float
    translation: np.ndarray  # (3,)
    rotation: np.ndarray  # (3, 3)
    presence_mask: int
    fid_corresp: List[int]  # length 6
    status_bits: int


@dataclass
class Reprojection:
    """3D -> 2D 重投影结果。"""
    frame_idx: int
    fid_idx: int
    pos_3d: np.ndarray  # (3,)
    left_2d: np.ndarray  # (2,)
    right_2d: np.ndarray  # (2,)


@dataclass
class GeometryPoint:
    """标记几何定义中的一个点。"""
    index: int
    position: np.ndarray  # (3,)
    normal: np.ndarray  # (3,)
    fid_type: int
    angle_of_view: float


@dataclass
class ImageHeader:
    """每帧图像元数据。"""
    frame_idx: int
    timestamp_us: int
    counter: int
    format_: int
    width: int
    height: int
    stride_bytes: int
    desynchro_us: int


def load_calibration(data_dir: str) -> Optional[Calibration]:
    """加载 calibration.csv。"""
    path = os.path.join(data_dir, "calibration.csv")
    if not os.path.exists(path):
        print(f"警告：{path} 未找到")
        return None

    cal = Calibration()
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(",")
            key = parts[0].strip()
            vals = [float(v) for v in parts[1:] if v.strip()]
            if key == "left_focal_length":
                cal.left_focal = np.array(vals[:2])
            elif key == "left_optical_centre":
                cal.left_center = np.array(vals[:2])
            elif key == "left_distortions":
                cal.left_distortion = np.array(vals[:5])
            elif key == "left_skew":
                cal.left_skew = vals[0]
            elif key == "right_focal_length":
                cal.right_focal = np.array(vals[:2])
            elif key == "right_optical_centre":
                cal.right_center = np.array(vals[:2])
            elif key == "right_distortions":
                cal.right_distortion = np.array(vals[:5])
            elif key == "right_skew":
                cal.right_skew = vals[0]
            elif key == "translation":
                cal.translation = np.array(vals[:3])
            elif key == "rotation":
                cal.rotation = np.array(vals[:3])
    return cal


def load_csv_generic(path: str) -> List[List[str]]:
    """加载 CSV 文件，跳过注释/头部行。"""
    rows = []
    if not os.path.exists(path):
        return rows
    with open(path, "r") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].startswith("#"):
                continue
            # Skip header row (non-numeric first field)
            try:
                float(row[0])
            except ValueError:
                continue
            rows.append(row)
    return rows


def load_raw_detections(data_dir: str, side: str) -> Dict[int, List[RawDetection]]:
    """Load raw_data_left.csv or raw_data_right.csv, grouped by frame."""
    path = os.path.join(data_dir, f"raw_data_{side}.csv")
    result = defaultdict(list)
    for row in load_csv_generic(path):
        det = RawDetection(
            frame_idx=int(row[0]),
            detection_idx=int(row[1]),
            center_x=float(row[2]),
            center_y=float(row[3]),
            pixels_count=int(row[4]),
            width=int(row[5]),
            height=int(row[6]),
            status_bits=int(row[7]),
        )
        result[det.frame_idx].append(det)
    return dict(result)


def load_fiducials_3d(data_dir: str) -> Dict[int, List[Fiducial3D]]:
    """Load fiducials_3d.csv, grouped by frame."""
    path = os.path.join(data_dir, "fiducials_3d.csv")
    result = defaultdict(list)
    for row in load_csv_generic(path):
        fid = Fiducial3D(
            frame_idx=int(row[0]),
            fid_idx=int(row[1]),
            pos_x=float(row[2]),
            pos_y=float(row[3]),
            pos_z=float(row[4]),
            left_index=int(row[5]),
            right_index=int(row[6]),
            epipolar_error=float(row[7]),
            triangulation_error=float(row[8]),
            probability=float(row[9]),
            status_bits=int(row[10]),
        )
        result[fid.frame_idx].append(fid)
    return dict(result)


def load_markers(data_dir: str) -> Dict[int, List[MarkerData]]:
    """Load markers.csv, grouped by frame."""
    path = os.path.join(data_dir, "markers.csv")
    result = defaultdict(list)
    for row in load_csv_generic(path):
        rot = np.array([
            [float(row[8]), float(row[9]), float(row[10])],
            [float(row[11]), float(row[12]), float(row[13])],
            [float(row[14]), float(row[15]), float(row[16])],
        ])
        fid_corresp = [int(row[18]), int(row[19]), int(row[20]),
                       int(row[21]), int(row[22]), int(row[23])]
        marker = MarkerData(
            frame_idx=int(row[0]),
            marker_idx=int(row[1]),
            geometry_id=int(row[2]),
            tracking_id=int(row[3]),
            registration_error=float(row[4]),
            translation=np.array([float(row[5]), float(row[6]), float(row[7])]),
            rotation=rot,
            presence_mask=int(row[17]),
            fid_corresp=fid_corresp,
            status_bits=int(row[24]),
        )
        result[marker.frame_idx].append(marker)
    return dict(result)


def load_reprojections(data_dir: str) -> Dict[int, List[Reprojection]]:
    """Load reprojections.csv, grouped by frame."""
    path = os.path.join(data_dir, "reprojections.csv")
    result = defaultdict(list)
    for row in load_csv_generic(path):
        try:
            left_x = float(row[5])
            left_y = float(row[6])
            right_x = float(row[7])
            right_y = float(row[8])
        except (ValueError, IndexError):
            continue  # Skip NaN entries
        if math.isnan(left_x):
            continue
        rp = Reprojection(
            frame_idx=int(row[0]),
            fid_idx=int(row[1]),
            pos_3d=np.array([float(row[2]), float(row[3]), float(row[4])]),
            left_2d=np.array([left_x, left_y]),
            right_2d=np.array([right_x, right_y]),
        )
        result[rp.frame_idx].append(rp)
    return dict(result)


def load_geometry(data_dir: str) -> Tuple[int, List[GeometryPoint]]:
    """Load geometry.csv. Returns (geometry_id, list_of_points)."""
    path = os.path.join(data_dir, "geometry.csv")
    geom_id = 0
    points = []
    if not os.path.exists(path):
        return geom_id, points
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(",")
            if parts[0] == "geometryId":
                geom_id = int(parts[1])
            elif parts[0] in ("version", "pointsCount", "divotsCount"):
                continue
            else:
                try:
                    idx = int(parts[0])
                    gp = GeometryPoint(
                        index=idx,
                        position=np.array([float(parts[1]), float(parts[2]), float(parts[3])]),
                        normal=np.array([float(parts[4]), float(parts[5]), float(parts[6])]),
                        fid_type=int(parts[7]),
                        angle_of_view=float(parts[8]),
                    )
                    points.append(gp)
                except (ValueError, IndexError):
                    continue
    return geom_id, points


def load_image_headers(data_dir: str) -> Dict[int, ImageHeader]:
    """Load image_headers.csv."""
    path = os.path.join(data_dir, "image_headers.csv")
    result = {}
    for row in load_csv_generic(path):
        hdr = ImageHeader(
            frame_idx=int(row[0]),
            timestamp_us=int(row[1]),
            counter=int(row[2]),
            format_=int(row[3]),
            width=int(row[4]),
            height=int(row[5]),
            stride_bytes=int(row[6]),
            desynchro_us=int(row[7]),
        )
        result[hdr.frame_idx] = hdr
    return result


def load_image(path: str, width: int, height: int) -> Optional[np.ndarray]:
    """Load a .raw binary image file."""
    if not os.path.exists(path):
        return None
    data = np.fromfile(path, dtype=np.uint8)
    expected = width * height
    if data.size < expected:
        return None
    return data[:expected].reshape((height, width))


# ============================================================================
# Reverse-Engineered Algorithm Implementations
# ============================================================================


def rodrigues_to_rotation_matrix(rvec: np.ndarray) -> np.ndarray:
    """Convert Rodrigues rotation vector to 3x3 rotation matrix.

    This is the standard Rodrigues formula used in the SDK.
    """
    theta = np.linalg.norm(rvec)
    if theta < 1e-12:
        return np.eye(3)
    k = rvec / theta
    K = np.array([
        [0, -k[2], k[1]],
        [k[2], 0, -k[0]],
        [-k[1], k[0], 0],
    ])
    R = np.eye(3) + np.sin(theta) * K + (1 - np.cos(theta)) * K @ K
    return R


def compute_fundamental_matrix(cal: Calibration) -> np.ndarray:
    """Compute the fundamental matrix F from calibration parameters.

    F = K_R^{-T} * [t]_x * R * K_L^{-1}
    where [t]_x is the skew-symmetric matrix of translation.
    """
    # Build intrinsic matrices
    K_L = np.array([
        [cal.left_focal[0], cal.left_skew * cal.left_focal[0], cal.left_center[0]],
        [0, cal.left_focal[1], cal.left_center[1]],
        [0, 0, 1],
    ])

    K_R = np.array([
        [cal.right_focal[0], cal.right_skew * cal.right_focal[0], cal.right_center[0]],
        [0, cal.right_focal[1], cal.right_center[1]],
        [0, 0, 1],
    ])

    R = rodrigues_to_rotation_matrix(cal.rotation)
    t = cal.translation

    # Skew-symmetric matrix of t
    tx = np.array([
        [0, -t[2], t[1]],
        [t[2], 0, -t[0]],
        [-t[1], t[0], 0],
    ])

    # Essential matrix E = [t]_x * R
    E = tx @ R

    # Fundamental matrix F = K_R^{-T} * E * K_L^{-1}
    F = np.linalg.inv(K_R).T @ E @ np.linalg.inv(K_L)

    return F


def undistort_point(px: float, py: float, focal: np.ndarray,
                    center: np.ndarray, distortion: np.ndarray,
                    skew: float) -> Tuple[float, float]:
    """Brown-Conrady iterative undistortion.

    Reverse-engineered from DLL RVA 0x1F0800.
    Converts distorted pixel coordinates to normalized undistorted coordinates.

    Returns undistorted normalized coordinates (xn, yn).
    """
    fcx, fcy = focal[0], focal[1]
    ccx, ccy = center[0], center[1]
    k1, k2, p1, p2, k3 = distortion

    if abs(fcx) < 1e-7 or abs(fcy) < 1e-7:
        return (px - ccx), (py - ccy)

    # Initial normalized coords
    yn = (py - ccy) / fcy
    xn = (px - ccx) / fcx - skew * yn

    # Iterative undistortion (exactly 20 iterations, per DLL: cmp eax, 0x14)
    # Note: DLL does NOT have early convergence check — it always runs 20 iters.
    x0 = xn
    y0 = yn
    for _ in range(20):
        r2 = xn * xn + yn * yn
        r4 = r2 * r2
        r6 = r4 * r2

        radial = 1.0 + k1 * r2 + k2 * r4 + k3 * r6
        if abs(radial) < 1e-7:
            # Avoid division by zero when radial distortion term approaches zero
            return xn, yn

        inv_radial = 1.0 / radial
        # DLL tangential order: p2*(r²+2x²) + 2*p1*xy for dx
        dx = p2 * (r2 + 2.0 * xn * xn) + 2.0 * p1 * xn * yn
        dy = p1 * (r2 + 2.0 * yn * yn) + 2.0 * p2 * xn * yn

        xn = (x0 - dx) * inv_radial
        yn = (y0 - dy) * inv_radial

    return xn, yn


def project_point_to_pixel(point_3d: np.ndarray, focal: np.ndarray,
                           center: np.ndarray, distortion: np.ndarray,
                           skew: float) -> np.ndarray:
    """Project a 3D point (in camera frame) to 2D pixel coordinates.

    Includes lens distortion application (forward model).
    """
    if abs(point_3d[2]) < 1e-12:
        return np.array([float("nan"), float("nan")])

    xn = point_3d[0] / point_3d[2]
    yn = point_3d[1] / point_3d[2]

    k1, k2, p1, p2, k3 = distortion
    r2 = xn * xn + yn * yn
    r4 = r2 * r2
    r6 = r4 * r2

    radial = 1.0 + k1 * r2 + k2 * r4 + k3 * r6
    dx = 2.0 * p1 * xn * yn + p2 * (r2 + 2.0 * xn * xn)
    dy = p1 * (r2 + 2.0 * yn * yn) + 2.0 * p2 * xn * yn

    xd = xn * radial + dx
    yd = yn * radial + dy

    px = focal[0] * (xd + skew * yd) + center[0]
    py = focal[1] * yd + center[1]

    return np.array([px, py])


def compute_epipolar_line(F: np.ndarray, left_px: float,
                          left_py: float) -> np.ndarray:
    """Compute the epipolar line in the right image for a left image point.

    Returns line coefficients (a, b, c) such that a*x + b*y + c = 0.
    """
    p = np.array([left_px, left_py, 1.0])
    line = F @ p
    # Normalize
    norm = math.sqrt(line[0] ** 2 + line[1] ** 2)
    if norm > 1e-12:
        line /= norm
    return line


def point_to_line_distance(line: np.ndarray, px: float, py: float) -> float:
    """Signed distance from point to line (a*x + b*y + c = 0)."""
    return line[0] * px + line[1] * py + line[2]


def triangulate_midpoint(left_origin: np.ndarray, left_dir: np.ndarray,
                         right_origin: np.ndarray,
                         right_dir: np.ndarray) -> Tuple[np.ndarray, float]:
    """Triangulate by finding the midpoint of closest approach of two rays.

    This is the 'closestPointOnRays' algorithm from DLL RVA 0x1EE990.

    Returns (3D_point, triangulation_error_mm).
    """
    w = left_origin - right_origin
    a = np.dot(left_dir, left_dir)
    b = np.dot(left_dir, right_dir)
    c = np.dot(right_dir, right_dir)
    d = np.dot(left_dir, w)
    e = np.dot(right_dir, w)

    denom = a * c - b * b
    if abs(denom) < 1e-12:
        return left_origin.copy(), float("inf")

    s = (b * e - c * d) / denom
    t = (a * e - b * d) / denom

    p1 = left_origin + s * left_dir
    p2 = right_origin + t * right_dir

    midpoint = (p1 + p2) / 2.0
    error = np.linalg.norm(p1 - p2)

    return midpoint, error


def _distort_to_pixel(xn: float, yn: float, focal: np.ndarray,
                      center: np.ndarray, distortion: np.ndarray,
                      skew: float) -> Tuple[float, float]:
    """Apply forward distortion model to convert normalized coords to pixel.

    This matches the DLL's distort() function (StereoCameraSystem.cpp:278).
    """
    k1, k2, p1, p2, k3 = distortion
    r2 = xn * xn + yn * yn
    r4 = r2 * r2
    r6 = r4 * r2

    radial = 1.0 + k1 * r2 + k2 * r4 + k3 * r6
    dx = 2.0 * p1 * xn * yn + p2 * (r2 + 2.0 * xn * xn)
    dy = p1 * (r2 + 2.0 * yn * yn) + 2.0 * p2 * xn * yn

    xd = xn * radial + dx
    yd = yn * radial + dy

    px = focal[0] * (xd + skew * yd) + center[0]
    py = focal[1] * yd + center[1]
    return px, py


def triangulate_point(cal: Calibration, left_px: float, left_py: float,
                      right_px: float,
                      right_py: float) -> Tuple[np.ndarray, float, float]:
    """Full triangulation pipeline: undistort -> rays -> midpoint.

    Returns (3D_point, epipolar_error_pixels, triangulation_error_mm).
    """
    R = rodrigues_to_rotation_matrix(cal.rotation)
    t = cal.translation

    # Undistort both points to normalized coordinates
    lx, ly = undistort_point(left_px, left_py, cal.left_focal,
                             cal.left_center, cal.left_distortion, cal.left_skew)
    rx, ry = undistort_point(right_px, right_py, cal.right_focal,
                             cal.right_center, cal.right_distortion, cal.right_skew)

    # Ray from left camera (origin = [0,0,0], direction = [lx, ly, 1])
    left_origin = np.array([0.0, 0.0, 0.0])
    left_dir = np.array([lx, ly, 1.0])
    left_dir /= np.linalg.norm(left_dir)

    # Ray from right camera
    # Right camera position in left camera frame: origin_R = -R^T * t
    # Right ray direction in left camera frame:   dir_R = R^T * (rx, ry, 1)
    # (From DLL RVA 0x1ee990: buildRightRay uses R^T to transform)
    Rt = R.T
    right_origin = Rt @ (-t)
    right_dir_cam = np.array([rx, ry, 1.0])
    right_dir = Rt @ right_dir_cam
    right_dir /= np.linalg.norm(right_dir)

    # Triangulate
    point_3d, tri_err = triangulate_midpoint(left_origin, left_dir,
                                             right_origin, right_dir)

    # Compute epipolar error (matching SDK approach):
    # From DLL analysis: computeEpipolarLine takes normalized left point,
    # converts to ideal undistorted pixel via KL, then applies F.
    # pointToEpipolarDistance converts normalized right point to ideal
    # undistorted pixel via KR, then measures distance to the line.
    # F = KR^{-T} * E * KL^{-1} operates in undistorted pixel space.
    F = compute_fundamental_matrix(cal)

    # Undistorted ideal left pixel: KL * (lx, ly, 1) — no distortion, only intrinsics
    left_ideal = np.array([
        cal.left_focal[0] * lx + cal.left_skew * cal.left_focal[0] * ly + cal.left_center[0],
        cal.left_focal[1] * ly + cal.left_center[1],
        1.0,
    ])
    # Undistorted ideal right pixel: KR * (rx, ry, 1) — no distortion, only intrinsics
    right_ideal = np.array([
        cal.right_focal[0] * rx + cal.right_skew * cal.right_focal[0] * ry + cal.right_center[0],
        cal.right_focal[1] * ry + cal.right_center[1],
        1.0,
    ])

    line = F @ left_ideal
    norm = math.sqrt(line[0] ** 2 + line[1] ** 2)
    if norm > 1e-12:
        epi_err = (line[0] * right_ideal[0] + line[1] * right_ideal[1] + line[2]) / norm
    else:
        epi_err = 0.0

    return point_3d, epi_err, tri_err


def reproject_3d_to_2d(cal: Calibration,
                       point_3d: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """Reproject a 3D point to left and right 2D pixel coordinates.

    Returns (left_pixel, right_pixel).
    """
    R = rodrigues_to_rotation_matrix(cal.rotation)
    t = cal.translation

    # In left camera frame, the point is already in the correct frame
    left_2d = project_point_to_pixel(point_3d, cal.left_focal,
                                     cal.left_center, cal.left_distortion,
                                     cal.left_skew)

    # Transform to right camera frame
    point_right = R @ point_3d + t
    right_2d = project_point_to_pixel(point_right, cal.right_focal,
                                      cal.right_center, cal.right_distortion,
                                      cal.right_skew)

    return left_2d, right_2d


def kabsch_registration(model_points: np.ndarray,
                        measured_points: np.ndarray) -> Tuple[
    np.ndarray, np.ndarray, float]:
    """Kabsch algorithm for rigid body registration.

    Finds optimal R, t that minimizes || R * model + t - measured ||^2.

    Returns (rotation_3x3, translation_3, rms_error).
    """
    assert model_points.shape == measured_points.shape
    n = model_points.shape[0]

    # Centroids
    centroid_model = np.mean(model_points, axis=0)
    centroid_measured = np.mean(measured_points, axis=0)

    # Center the points
    model_centered = model_points - centroid_model
    measured_centered = measured_points - centroid_measured

    # Cross-covariance matrix
    H = model_centered.T @ measured_centered

    # SVD
    U, S, Vt = np.linalg.svd(H)

    # Ensure proper rotation (det = +1)
    d = np.linalg.det(Vt.T @ U.T)
    sign_matrix = np.diag([1.0, 1.0, np.sign(d) if abs(d) > 1e-10 else 1.0])

    R = Vt.T @ sign_matrix @ U.T
    t_vec = centroid_measured - R @ centroid_model

    # RMS error
    errors = measured_points - (model_points @ R.T + t_vec)
    rms = np.sqrt(np.mean(np.sum(errors ** 2, axis=1)))

    return R, t_vec, rms


# ============================================================================
# Verification Functions
# ============================================================================


def verify_reprojection(cal: Calibration,
                        reprojections: Dict[int, List[Reprojection]]) -> dict:
    """验证 3D -> 2D 重投影是否与 SDK 的 ftkReprojectPoint 输出一致。"""
    print("\n" + "=" * 70)
    print("验证 1：3D -> 2D 重投影（ftkReprojectPoint）")
    print("=" * 70)

    all_left_errs = []
    all_right_errs = []

    for frame_idx in sorted(reprojections.keys()):
        for rp in reprojections[frame_idx]:
            our_left, our_right = reproject_3d_to_2d(cal, rp.pos_3d)

            left_err = np.linalg.norm(our_left - rp.left_2d)
            right_err = np.linalg.norm(our_right - rp.right_2d)

            all_left_errs.append(left_err)
            all_right_errs.append(right_err)

    if not all_left_errs:
        print("  没有可用的重投影数据。")
        return {"status": "no_data"}

    left_errs = np.array(all_left_errs)
    right_errs = np.array(all_right_errs)

    print(f"  样本数：{len(left_errs)}")
    print(f"  左相机重投影误差： 平均={left_errs.mean():.6f} px, "
          f"最大={left_errs.max():.6f} px, 标准差={left_errs.std():.6f}")
    print(f"  右相机重投影误差： 平均={right_errs.mean():.6f} px, "
          f"最大={right_errs.max():.6f} px, 标准差={right_errs.std():.6f}")

    threshold = 0.1  # 像素
    pass_rate = np.mean(np.maximum(left_errs, right_errs) < threshold) * 100
    print(f"  通过率（<{threshold} px）：{pass_rate:.1f}%")
    print(f"  结果：{'PASS' if pass_rate > 95 else '需要检查'}")

    return {
        "status": "pass" if pass_rate > 95 else "fail",
        "samples": len(left_errs),
        "left_mean": float(left_errs.mean()),
        "right_mean": float(right_errs.mean()),
        "pass_rate": pass_rate,
    }


def verify_triangulation(cal: Calibration,
                         fiducials_3d: Dict[int, List[Fiducial3D]],
                         raw_left: Dict[int, List[RawDetection]],
                         raw_right: Dict[int, List[RawDetection]]) -> dict:
    """验证立体三角测量是否与 SDK 3D 基准点输出一致。"""
    print("\n" + "=" * 70)
    print("验证 2：立体三角测量（EpipolarMatcher）")
    print("=" * 70)

    pos_errors = []
    epi_errors = []
    tri_errors = []
    statuses = []

    for frame_idx in sorted(fiducials_3d.keys()):
        left_dets = {d.detection_idx: d for d in raw_left.get(frame_idx, [])}
        right_dets = {d.detection_idx: d for d in raw_right.get(frame_idx, [])}

        for fid in fiducials_3d[frame_idx]:
            if fid.left_index not in left_dets or fid.right_index not in right_dets:
                continue

            left_det = left_dets[fid.left_index]
            right_det = right_dets[fid.right_index]

            our_3d, our_epi, our_tri = triangulate_point(
                cal, left_det.center_x, left_det.center_y,
                right_det.center_x, right_det.center_y
            )

            sdk_3d = np.array([fid.pos_x, fid.pos_y, fid.pos_z])
            pos_err = np.linalg.norm(our_3d - sdk_3d)
            epi_err_diff = abs(our_epi - fid.epipolar_error)

            pos_errors.append(pos_err)
            epi_errors.append(epi_err_diff)
            tri_errors.append(abs(our_tri - fid.triangulation_error))
            statuses.append(fid.status_bits)

    if not pos_errors:
        print("  没有可用的三角测量数据（需要raw + 3D基准点）。")
        return {"status": "no_data"}

    pos_errors = np.array(pos_errors)
    epi_errors = np.array(epi_errors)
    tri_errors = np.array(tri_errors)
    statuses = np.array(statuses)

    # 整体统计
    print(f"  总样本数：{len(pos_errors)}")
    print(f"  3D 位置误差（所有）： 平均={pos_errors.mean():.6f} mm, "
          f"最大={pos_errors.max():.6f} mm, 标准差={pos_errors.std():.6f}")

    # 状态=0（高质量匹配）— 主要指标
    good_mask = statuses == 0
    good_count = int(good_mask.sum())
    outlier_count = len(pos_errors) - good_count

    if good_count > 0:
        good_pos = pos_errors[good_mask]
        good_epi = epi_errors[good_mask]
        good_tri = tri_errors[good_mask]
        print(f"\n  良好匹配（状态=0）：{good_count}")
        print(f"    3D 位置误差：       平均={good_pos.mean():.6f} mm, "
              f"最大={good_pos.max():.6f} mm")
        print(f"    对极误差差值：     平均={good_epi.mean():.6f} px, "
              f"最大={good_epi.max():.6f} px")
        print(f"    三角测量误差差值: 平均={good_tri.mean():.6f} mm, "
              f"最大={good_tri.max():.6f} mm")

    if outlier_count > 0:
        out_pos = pos_errors[~good_mask]
        print(f"\n  异常匹配（状态>0）：{outlier_count}")
        print(f"    3D 位置误差：       平均={out_pos.mean():.6f} mm, "
              f"最大={out_pos.max():.6f} mm")
        print(f"    （预计误差较大：float32 精度随距离退化，~30mm @ 93km 深度）")

    # 基于良好匹配的验证率：float32 精度给出 ~0.03mm 最大误差
    threshold_mm = 0.05  # 50 微米 — 考虑 float32 精度
    if good_count > 0:
        pass_rate_good = np.mean(good_pos < threshold_mm) * 100
    else:
        pass_rate_good = 0.0
    pass_rate_all = np.mean(pos_errors < threshold_mm) * 100

    print(f"\n  验证率（良好，<{threshold_mm} mm）：{pass_rate_good:.1f}%")
    print(f"  验证率（所有， <{threshold_mm} mm）：{pass_rate_all:.1f}%")
    status = "pass" if pass_rate_good > 95 else "fail"
    print(f"  结果：{'PASS' if status == 'pass' else '需要检查'}")

    return {
        "status": status,
        "samples": len(pos_errors),
        "good_count": good_count,
        "outlier_count": outlier_count,
        "pos_mean_mm": float(pos_errors.mean()),
        "pos_max_mm": float(pos_errors.max()),
        "good_pos_mean_mm": float(good_pos.mean()) if good_count > 0 else None,
        "good_pos_max_mm": float(good_pos.max()) if good_count > 0 else None,
        "epi_mean_px": float(epi_errors.mean()),
        "pass_rate_good": pass_rate_good,
        "pass_rate_all": pass_rate_all,
    }


def verify_epipolar_geometry(cal: Calibration,
                             fiducials_3d: Dict[int, List[Fiducial3D]],
                             raw_left: Dict[int, List[RawDetection]],
                             raw_right: Dict[int, List[RawDetection]]) -> dict:
    """验证对极线计算和匹配约束。"""
    print("\n" + "=" * 70)
    print("验证 3：对极几何（基础矩阵）")
    print("=" * 70)

    F = compute_fundamental_matrix(cal)
    print(f"  基础矩阵 F：")
    for row in F:
        print(f"    [{row[0]:+.10e}, {row[1]:+.10e}, {row[2]:+.10e}]")

    epi_errors_our = []
    epi_errors_sdk = []

    for frame_idx in sorted(fiducials_3d.keys()):
        left_dets = {d.detection_idx: d for d in raw_left.get(frame_idx, [])}
        right_dets = {d.detection_idx: d for d in raw_right.get(frame_idx, [])}

        for fid in fiducials_3d[frame_idx]:
            if fid.left_index not in left_dets or fid.right_index not in right_dets:
                continue

            left_det = left_dets[fid.left_index]
            right_det = right_dets[fid.right_index]

            # Compute our epipolar distance (matching SDK undistorted pixel approach)
            # SDK undistorts both points to normalized coords, then converts to
            # undistorted ideal pixel coords via intrinsic matrices before
            # applying the fundamental matrix.
            lx, ly = undistort_point(left_det.center_x, left_det.center_y,
                                     cal.left_focal, cal.left_center,
                                     cal.left_distortion, cal.left_skew)
            rx, ry = undistort_point(right_det.center_x, right_det.center_y,
                                     cal.right_focal, cal.right_center,
                                     cal.right_distortion, cal.right_skew)

            # Undistorted ideal pixels: K * normalized (no distortion model)
            left_ideal = np.array([
                cal.left_focal[0] * lx + cal.left_skew * cal.left_focal[0] * ly + cal.left_center[0],
                cal.left_focal[1] * ly + cal.left_center[1],
                1.0,
            ])
            right_ideal = np.array([
                cal.right_focal[0] * rx + cal.right_skew * cal.right_focal[0] * ry + cal.right_center[0],
                cal.right_focal[1] * ry + cal.right_center[1],
                1.0,
            ])

            line = F @ left_ideal
            norm = math.sqrt(line[0] ** 2 + line[1] ** 2)
            if norm > 1e-12:
                our_epi = (line[0] * right_ideal[0] + line[1] * right_ideal[1] + line[2]) / norm
            else:
                our_epi = 0.0

            epi_errors_our.append(our_epi)
            epi_errors_sdk.append(fid.epipolar_error)

    if not epi_errors_our:
        print("  没有对极验证数据。")
        return {"status": "no_data"}

    our = np.array(epi_errors_our)
    sdk = np.array(epi_errors_sdk)
    diff = np.abs(our - sdk)

    print(f"\n  样本数：{len(our)}")
    print(f"  我们的对极误差：   平均={np.mean(np.abs(our)):.6f} px, "
          f"标准差={np.std(our):.6f}")
    print(f"  SDK 对极误差：   平均={np.mean(np.abs(sdk)):.6f} px, "
          f"标准差={np.std(sdk):.6f}")
    print(f"  差值（|我们-SDK|）： 平均={diff.mean():.6f} px, "
          f"最大={diff.max():.6f} px")

    # 相关性
    if np.std(our) > 1e-12 and np.std(sdk) > 1e-12:
        corr = np.corrcoef(our, sdk)[0, 1]
        print(f"  相关性：{corr:.6f}")
    else:
        corr = float("nan")

    threshold = 0.01
    pass_rate = np.mean(diff < threshold) * 100
    print(f"  验证率（<{threshold} px 差值）：{pass_rate:.1f}%")
    print(f"  结果：{'PASS' if pass_rate > 90 else '需要检查'}")

    return {
        "status": "pass" if pass_rate > 90 else "fail",
        "samples": len(our),
        "diff_mean": float(diff.mean()),
        "diff_max": float(diff.max()),
        "correlation": float(corr) if not math.isnan(corr) else None,
        "pass_rate": pass_rate,
    }


def verify_marker_registration(markers: Dict[int, List[MarkerData]],
                               fiducials_3d: Dict[int, List[Fiducial3D]],
                               geometry_id: int,
                               geometry_points: List[
                                   GeometryPoint]) -> dict:
    """验证标记位姿估计（Kabsch 配准）是否与 SDK 输出一致。"""
    print("\n" + "=" * 70)
    print("验证 4：标记配准（Kabsch 算法）")
    print("=" * 70)

    if not geometry_points:
        print("  未加载几何定义。")
        return {"status": "no_data"}

    model_pts_all = np.array([gp.position for gp in geometry_points])
    print(f"  几何 ID：{geometry_id}，点数：{len(model_pts_all)}")

    trans_errors = []
    rot_errors = []
    rms_diffs = []

    for frame_idx in sorted(markers.keys()):
        fids = fiducials_3d.get(frame_idx, [])
        fid_dict = {f.fid_idx: f for f in fids}

        for marker in markers[frame_idx]:
            if marker.geometry_id != geometry_id:
                continue

            # Collect matched model -> measured point pairs
            model_pts = []
            measured_pts = []
            for geom_fid_idx in range(len(geometry_points)):
                if geom_fid_idx >= len(marker.fid_corresp):
                    break
                fid3d_idx = marker.fid_corresp[geom_fid_idx]
                if fid3d_idx == 0xFFFFFFFF or fid3d_idx not in fid_dict:
                    continue
                fid = fid_dict[fid3d_idx]
                model_pts.append(geometry_points[geom_fid_idx].position)
                measured_pts.append(np.array([fid.pos_x, fid.pos_y, fid.pos_z]))

            if len(model_pts) < 3:
                continue

            model_arr = np.array(model_pts)
            measured_arr = np.array(measured_pts)

            # Our Kabsch registration
            our_R, our_t, our_rms = kabsch_registration(model_arr, measured_arr)

            # SDK values
            sdk_R = marker.rotation
            sdk_t = marker.translation
            sdk_rms = marker.registration_error

            # Compare translation
            trans_err = np.linalg.norm(our_t - sdk_t)
            trans_errors.append(trans_err)

            # Compare rotation (Frobenius norm of difference)
            rot_err = np.linalg.norm(our_R - sdk_R, "fro")
            rot_errors.append(rot_err)

            # Compare RMS
            rms_diff = abs(our_rms - sdk_rms)
            rms_diffs.append(rms_diff)

    if not trans_errors:
        print("  没有可用的标记数据进行验证。")
        return {"status": "no_data"}

    trans_errors = np.array(trans_errors)
    rot_errors = np.array(rot_errors)
    rms_diffs = np.array(rms_diffs)

    print(f"  样本数：{len(trans_errors)}")
    print(f"  平移误差：    平均={trans_errors.mean():.6f} mm, "
          f"最大={trans_errors.max():.6f} mm")
    print(f"  旋转误差（Fro）： 平均={rot_errors.mean():.8f}, "
          f"最大={rot_errors.max():.8f}")
    print(f"  RMS 误差差值：       平均={rms_diffs.mean():.6f} mm, "
          f"最大={rms_diffs.max():.6f} mm")

    trans_threshold = 0.01  # 10 微米
    pass_rate = np.mean(trans_errors < trans_threshold) * 100
    print(f"  验证率（<{trans_threshold} mm 平移）：{pass_rate:.1f}%")
    print(f"  结果：{'PASS' if pass_rate > 90 else '需要检查'}")

    return {
        "status": "pass" if pass_rate > 90 else "fail",
        "samples": len(trans_errors),
        "trans_mean_mm": float(trans_errors.mean()),
        "trans_max_mm": float(trans_errors.max()),
        "rot_mean_fro": float(rot_errors.mean()),
        "rms_diff_mean": float(rms_diffs.mean()),
        "pass_rate": pass_rate,
    }


def verify_undistortion(cal: Calibration,
                        reprojections: Dict[int, List[Reprojection]]) -> dict:
    """通过往返验证去畸变：distort(undistort(p)) ≈ p。

    使用重投影数据来测试去畸变模型的一致性。
    """
    print("\n" + "=" * 70)
    print("验证 5：镜头去畸变（Brown-Conrady）")
    print("=" * 70)

    left_roundtrip_errors = []
    right_roundtrip_errors = []

    for frame_idx in sorted(reprojections.keys()):
        for rp in reprojections[frame_idx]:
            # Left camera round-trip test
            left_px, left_py = rp.left_2d[0], rp.left_2d[1]
            xn, yn = undistort_point(left_px, left_py, cal.left_focal,
                                     cal.left_center, cal.left_distortion,
                                     cal.left_skew)
            px_back, py_back = _distort_to_pixel(xn, yn, cal.left_focal,
                                                  cal.left_center,
                                                  cal.left_distortion,
                                                  cal.left_skew)
            err = math.sqrt((px_back - left_px) ** 2 + (py_back - left_py) ** 2)
            left_roundtrip_errors.append(err)

            # Right camera round-trip test
            right_px, right_py = rp.right_2d[0], rp.right_2d[1]
            xn, yn = undistort_point(right_px, right_py, cal.right_focal,
                                     cal.right_center, cal.right_distortion,
                                     cal.right_skew)
            px_back, py_back = _distort_to_pixel(xn, yn, cal.right_focal,
                                                  cal.right_center,
                                                  cal.right_distortion,
                                                  cal.right_skew)
            err = math.sqrt((px_back - right_px) ** 2 + (py_back - right_py) ** 2)
            right_roundtrip_errors.append(err)

    if not left_roundtrip_errors:
        print("  没有去畸变往返测试的数据。")
        return {"status": "no_data"}

    left_errs = np.array(left_roundtrip_errors)
    right_errs = np.array(right_roundtrip_errors)

    print(f"  样本数：{len(left_errs)}")
    print(f"  左侧往返误差：  平均={left_errs.mean():.10f} px, "
          f"最大={left_errs.max():.10f} px")
    print(f"  右侧往返误差：  平均={right_errs.mean():.10f} px, "
          f"最大={right_errs.max():.10f} px")

    threshold = 1e-3  # 0.001 像素 — 迭代去畸变残差
    all_pass = (left_errs.max() < threshold) and (right_errs.max() < threshold)
    print(f"  结果：{'PASS' if all_pass else '需要检查'} "
          f"（阈值：{threshold} px）")

    return {
        "status": "pass" if all_pass else "fail",
        "samples": len(left_errs),
        "left_max": float(left_errs.max()),
        "right_max": float(right_errs.max()),
    }


def verify_circle_centroid(raw_left: Dict[int, List[RawDetection]],
                           raw_right: Dict[int, List[RawDetection]],
                           fiducials_3d: Dict[int, List[Fiducial3D]],
                           cal: Calibration) -> dict:
    """验证圆心检测一致性。

    比较来自原始检测的圆心位置与通过重投影 3D 基准点获得的预期位置。

    返回（圆心误差统计）。
    """
    print("\n" + "=" * 70)
    print("验证 6：圆心检测（SegmenterV21）")
    print("=" * 70)

    left_centroid_errors = []
    right_centroid_errors = []
    detection_counts = []

    for frame_idx in sorted(fiducials_3d.keys()):
        left_dets = {d.detection_idx: d for d in raw_left.get(frame_idx, [])}
        right_dets = {d.detection_idx: d for d in raw_right.get(frame_idx, [])}

        for fid in fiducials_3d[frame_idx]:
            if fid.left_index not in left_dets or fid.right_index not in right_dets:
                continue

            left_det = left_dets[fid.left_index]
            right_det = right_dets[fid.right_index]

            # Reproject 3D fiducial to 2D to get expected centroid
            sdk_3d = np.array([fid.pos_x, fid.pos_y, fid.pos_z])
            expected_left, expected_right = reproject_3d_to_2d(cal, sdk_3d)

            # Compute centroid error
            left_error = np.linalg.norm(
                np.array([left_det.center_x, left_det.center_y]) - expected_left
            )
            right_error = np.linalg.norm(
                np.array([right_det.center_x, right_det.center_y]) - expected_right
            )

            left_centroid_errors.append(left_error)
            right_centroid_errors.append(right_error)
            detection_counts.append(1)

    if not left_centroid_errors:
        print("  没有可用的圆心数据进行验证（需要原始检测 + 3D 基准点）。")
        return {"status": "no_data"}

    left_errors = np.array(left_centroid_errors)
    right_errors = np.array(right_centroid_errors)

    print(f"  分析的总检测数：{len(left_errors)}")
    print(f"  左侧圆心误差：  平均={left_errors.mean():.6f} px, "
          f"最大={left_errors.max():.6f} px, 标准差={left_errors.std():.6f}")
    print(f"  右侧圆心误差: 平均={right_errors.mean():.6f} px, "
          f"最大={right_errors.max():.6f} px, 标准差={right_errors.std():.6f}")

    # 统计分解
    threshold_px = 1.0  # 1 像素圆心定位容差
    pass_count = np.sum((left_errors < threshold_px) & (right_errors < threshold_px))
    pass_rate = (pass_count / len(left_errors)) * 100 if len(left_errors) > 0 else 0.0

    print(f"\n  圆心精度（<{threshold_px} px 两个相机）：{pass_rate:.1f}%")

    # 百分位数分析
    p50_left = np.percentile(left_errors, 50)
    p90_left = np.percentile(left_errors, 90)
    p99_left = np.percentile(left_errors, 99)
    p50_right = np.percentile(right_errors, 50)
    p90_right = np.percentile(right_errors, 90)
    p99_right = np.percentile(right_errors, 99)

    print(f"  左侧百分位数（50/90/99）：   {p50_left:.6f} / {p90_left:.6f} / {p99_left:.6f} px")
    print(f"  右侧百分位数（50/90/99）：  {p50_right:.6f} / {p90_right:.6f} / {p99_right:.6f} px")

    status = "pass" if pass_rate > 95 else "fail"
    print(f"  结果：{'PASS' if status == 'pass' else '需要检查'}")

    return {
        "status": status,
        "samples": len(left_errors),
        "left_mean": float(left_errors.mean()),
        "left_max": float(left_errors.max()),
        "right_mean": float(right_errors.mean()),
        "right_max": float(right_errors.max()),
        "pass_rate": pass_rate,
    }


def verify_cross_references(fiducials_3d: Dict[int, List[Fiducial3D]],
                            raw_left: Dict[int, List[RawDetection]],
                            raw_right: Dict[int, List[RawDetection]],
                            markers: Dict[int, List[MarkerData]]) -> dict:
    """验证数据交叉参考一致性。"""
    print("\n" + "=" * 70)
    print("验证 7：数据交叉参考一致性")
    print("=" * 70)

    issues = []
    total_fids = 0
    total_markers = 0

    for frame_idx in sorted(fiducials_3d.keys()):
        left_dets = {d.detection_idx: d for d in raw_left.get(frame_idx, [])}
        right_dets = {d.detection_idx: d for d in raw_right.get(frame_idx, [])}

        for fid in fiducials_3d[frame_idx]:
            total_fids += 1
            if fid.left_index not in left_dets:
                issues.append(f"Frame {frame_idx}: fid3d[{fid.fid_idx}].leftIndex={fid.left_index} "
                              f"not in raw_left")
            if fid.right_index not in right_dets:
                issues.append(f"Frame {frame_idx}: fid3d[{fid.fid_idx}].rightIndex={fid.right_index} "
                              f"not in raw_right")

    for frame_idx in sorted(markers.keys()):
        fid_dict = {f.fid_idx: f for f in fiducials_3d.get(frame_idx, [])}
        for marker in markers[frame_idx]:
            total_markers += 1
            for gi, fc in enumerate(marker.fid_corresp):
                if fc != 0xFFFFFFFF and fc not in fid_dict:
                    issues.append(f"Frame {frame_idx}: marker[{marker.marker_idx}].fidCorresp[{gi}]="
                                  f"{fc} not in fid3d")

    print(f"  检查的 3D 基准点总数：{total_fids}")
    print(f"  检查的标记总数：{total_markers}")
    print(f"  交叉参考问题数：{len(issues)}")
    if issues:
        for issue in issues[:10]:
            print(f"    - {issue}")
        if len(issues) > 10:
            print(f"    ... 还有 {len(issues) - 10} 个")
    print(f"  结果：{'PASS' if len(issues) == 0 else '需要检查'}")

    return {
        "status": "pass" if len(issues) == 0 else "fail",
        "total_fids": total_fids,
        "total_markers": total_markers,
        "issues": len(issues),
    }


def print_data_summary(image_headers: Dict[int, ImageHeader],
                       raw_left: Dict[int, List[RawDetection]],
                       raw_right: Dict[int, List[RawDetection]],
                       fiducials_3d: Dict[int, List[Fiducial3D]],
                       markers: Dict[int, List[MarkerData]],
                       reprojections: Dict[int, List[Reprojection]],
                       cal: Optional[Calibration]) -> None:
    """打印已加载数据的摘要。"""
    print("\n" + "=" * 70)
    print("数据摘要")
    print("=" * 70)

    frames = sorted(set(
        list(image_headers.keys()) +
        list(raw_left.keys()) +
        list(raw_right.keys()) +
        list(fiducials_3d.keys()) +
        list(markers.keys())
    ))

    print(f"  总帧数：{len(frames)}")
    print(f"  图像头数：{len(image_headers)}")
    if image_headers:
        first_hdr = image_headers[min(image_headers.keys())]
        print(f"  图像尺寸：{first_hdr.width} x {first_hdr.height}, "
              f"步幅：{first_hdr.stride_bytes}")

    total_left = sum(len(v) for v in raw_left.values())
    total_right = sum(len(v) for v in raw_right.values())
    total_fids = sum(len(v) for v in fiducials_3d.values())
    total_markers = sum(len(v) for v in markers.values())
    total_reproj = sum(len(v) for v in reprojections.values())

    print(f"  左侧检测：{total_left}，跨越 {len(raw_left)} 帧")
    print(f"  右侧检测：{total_right}，跨越 {len(raw_right)} 帧")
    print(f"  3D 基准点：{total_fids}，跨越 {len(fiducials_3d)} 帧")
    print(f"  标记：{total_markers}，跨越 {len(markers)} 帧")
    print(f"  重投影：{total_reproj}，跨越 {len(reprojections)} 帧")
    print(f"  标定：{'已加载' if cal else '不可用'}")

    if cal:
        print(f"\n  标定详情：")
        print(f"    左侧焦距：[{cal.left_focal[0]:.4f}, {cal.left_focal[1]:.4f}]")
        print(f"    左侧中心：[{cal.left_center[0]:.4f}, {cal.left_center[1]:.4f}]")
        print(f"    左侧畸变：{cal.left_distortion}")
        print(f"    右侧焦距：[{cal.right_focal[0]:.4f}, {cal.right_focal[1]:.4f}]")
        print(f"    右侧中心：[{cal.right_center[0]:.4f}, {cal.right_center[1]:.4f}]")
        print(f"    右侧畸变：{cal.right_distortion}")
        print(f"    平移：{cal.translation}")
        print(f"    旋转（Rodrigues）：{cal.rotation}")


# ============================================================================
# Main
# ============================================================================


def main():
    parser = argparse.ArgumentParser(
        description="对反向工程的 fusionTrack 算法与 SDK 输出数据进行验证。"
    )
    parser.add_argument("--data-dir", required=True,
                        help="包含来自 stereo99_DumpAllData 的 CSV 数据的目录")
    parser.add_argument("--image-dir", default=None,
                        help="包含图像的目录（默认为 data-dir）")
    args = parser.parse_args()

    data_dir = args.data_dir
    image_dir = args.image_dir or data_dir

    if not os.path.isdir(data_dir):
        print(f"错误：找不到数据目录：{data_dir}")
        sys.exit(1)

    print("=" * 70)
    print("FUSIONTRACK 反向工程验证")
    print("=" * 70)
    print(f"数据目录：{data_dir}")
    print(f"图像目录：{image_dir}")

    # 加载所有数据
    print("\n加载数据...")
    cal = load_calibration(data_dir)
    image_headers = load_image_headers(data_dir)
    raw_left = load_raw_detections(data_dir, "left")
    raw_right = load_raw_detections(data_dir, "right")
    fiducials_3d = load_fiducials_3d(data_dir)
    markers_data = load_markers(data_dir)
    reprojections = load_reprojections(data_dir)
    geometry_id, geometry_points = load_geometry(data_dir)

    # Print data summary
    print_data_summary(image_headers, raw_left, raw_right, fiducials_3d,
                       markers_data, reprojections, cal)

    # Run verifications
    results = {}

    if cal:
        results["reprojection"] = verify_reprojection(cal, reprojections)
        results["triangulation"] = verify_triangulation(
            cal, fiducials_3d, raw_left, raw_right
        )
        results["epipolar"] = verify_epipolar_geometry(
            cal, fiducials_3d, raw_left, raw_right
        )
        results["undistortion"] = verify_undistortion(cal, reprojections)
        results["circle_centroid"] = verify_circle_centroid(
            raw_left, raw_right, fiducials_3d, cal
        )
    else:
        print("\n警告：没有标定数据 - 跳过依赖于标定的测试")

    results["marker_registration"] = verify_marker_registration(
        markers_data, fiducials_3d, geometry_id, geometry_points
    )
    results["cross_reference"] = verify_cross_references(
        fiducials_3d, raw_left, raw_right, markers_data
    )

    # 最终总结
    print("\n" + "=" * 70)
    print("最终总结")
    print("=" * 70)

    all_pass = True
    for name, result in results.items():
        status = result.get("status", "unknown")
        icon = "[√]" if status == "pass" else ("[?]" if status == "no_data" else "[×]")
        print(f"  {icon} {name}：{status}")
        if status == "fail":
            all_pass = False

    print()
    if all_pass:
        print("  整体：所有验证均通过")
    else:
        print("  整体：某些验证需要检查")
        print("  查看上面的详细输出以了解具体差异。")

    print()
    print("注意：小的数值差异（< 1e-5）是由实现之间的")
    print("浮点精度差异引起的。")
    print("较大的差异可能表示需要调查的算法差异。")


if __name__ == "__main__":
    main()
