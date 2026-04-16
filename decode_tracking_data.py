#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fusionTrack 双目相机 UDP 抓包解码器
====================================

解码 Atracsys fusionTrack 近红外双目相机与电脑之间的 UDP 通讯数据。

协议层次结构：
    Layer 1 - UDP 私有分片协议 (24 字节头)
    Layer 2 - 内层帧 (80 字节帧头 + ROI 稀疏图像数据体)
    Layer 3 - ROI 编码: 16 字节定长记录的稀疏行段压缩

两路数据流：
    0x1003 = 左目相机 ROI 检测数据
    0x1004 = 右目相机 ROI 检测数据

重要说明：
    相机通过网络传输的是原始的 ROI (感兴趣区域) 红外图像检测数据，
    即每帧中检测到的高亮红外反光点的稀疏灰度图。
    3D 位置和旋转矩阵是由 PC 端的 SDK 库 (ftkGetLastFrame) 根据
    左右目检测数据进行立体匹配、三角化测量和标记体匹配后计算得出的，
    并不在网络数据中直接传输。

    本解码器提取每帧的：
    - 帧元数据 (时间戳、帧号、传感器标识)
    - 左目/右目各自检测到的红外反光点 (blob) 的 2D 质心坐标、面积、峰值亮度
    - 大面积 blob 的追踪结果 (对应定位工具上的反光小球)

用法：
    python decode_tracking_data.py [pcapng_file] [--min-area N] [--threshold N] [--json output.json]

参数：
    pcapng_file    pcapng 抓包文件路径，默认读取 fusionTrack SDK x64/output/full_02.pcapng
    --min-area N   最小 blob 面积筛选阈值，默认 100 像素
    --threshold N  二值化阈值，默认 80
    --json F       输出 JSON 文件路径
    --last N       只处理最后 N 帧
    --all-blobs    输出所有 blob (不只是大面积的)
"""

import struct
import json
import argparse
import sys
import os
from collections import defaultdict, Counter

import numpy as np
from scipy import ndimage

# =============================================================================
# 设备网络常量
# =============================================================================

CAMERA_IP = '172.17.1.7'        # fusionTrack 相机默认 IP 地址
CAMERA_PORT = 3509              # fusionTrack 相机默认 UDP 端口
STREAM_TAG_LEFT = 0x1003        # 左目相机数据流标识
STREAM_TAG_RIGHT = 0x1004       # 右目相机数据流标识
SENSOR_FLAG_LEFT = 0x00         # 左目传感器标志 (内层帧头 byte[20])
SENSOR_FLAG_RIGHT = 0xC0        # 右目传感器标志 (内层帧头 byte[20])
INNER_HEADER_SIZE = 80          # 内层帧头部大小 (字节)
ROI_RECORD_SIZE = 16            # ROI 记录固定大小 (字节)
ROI_BACKGROUND = 0x80           # ROI 背景填充值
ROI_PADDING = 0x00              # ROI 尾部填充值


# =============================================================================
# Layer 1: UDP 分片协议 - 24 字节私有头部
# =============================================================================

def parse_udp_vendor_header(payload):
    """
    解析 24 字节 UDP 私有分片头部。

    头部格式 (小端序):
        Offset  Size  Field
        0       2     stream_tag        流标识 (0x1003=左目, 0x1004=右目)
        2       2     packet_words      载荷长度/2 (packet_words * 2 = UDP载荷字节数)
        4       4     packet_sequence   全局递增序列号
        8       4     reserved          保留字段 (恒为0)
        12      4     frame_token       帧标识符 (同一帧的分片共享此值)
        16      4     frame_size        帧总字节数
        20      4     payload_offset    当前分片在帧内的偏移量

    返回: dict 或 None
    """
    if len(payload) < 24:
        return None

    stag, pkt_words, pkt_seq, reserved, frame_token, frame_size, pay_offset = \
        struct.unpack_from('<HHIIIII', payload, 0)

    # 校验: packet_words * 2 应等于载荷总长度
    if pkt_words * 2 != len(payload):
        return None

    return {
        'stream_tag': stag,
        'packet_words': pkt_words,
        'packet_sequence': pkt_seq,
        'reserved': reserved,
        'frame_token': frame_token,
        'frame_size': frame_size,
        'payload_offset': pay_offset,
        'chunk_data': payload[24:]
    }


# =============================================================================
# Layer 1.5: 帧重组
# =============================================================================

def reassemble_frames(packets):
    """
    将 UDP 分片重组为完整帧。

    参数:
        packets: list of dict, 每个元素包含 'payload' 字段

    返回:
        dict: {(stream_tag, frame_token): bytes}
    """
    fragments = defaultdict(lambda: {'size': 0, 'chunks': []})

    for pkt in packets:
        hdr = parse_udp_vendor_header(pkt['payload'])
        if hdr is None:
            continue

        key = (hdr['stream_tag'], hdr['frame_token'])
        fragments[key]['size'] = hdr['frame_size']
        fragments[key]['chunks'].append((hdr['payload_offset'], hdr['chunk_data']))

    assembled = {}
    for key, info in fragments.items():
        buf = bytearray(info['size'])
        total_written = 0
        for offset, data in sorted(info['chunks']):
            end = offset + len(data)
            if end <= info['size']:
                buf[offset:end] = data
                total_written += len(data)
        # 允许少量容差
        if total_written >= info['size'] * 0.95:
            assembled[key] = bytes(buf)

    return assembled


# =============================================================================
# Layer 2: 内层帧头部 - 80 字节
# =============================================================================

def parse_inner_frame_header(frame_data):
    """
    解析 80 字节内层帧头部。

    已确认的字段映射:
        Offset  Size  Field
        0       4     magic / capture_unix_timestamp
                      值为 0x66441819 = 1715738649
                      对应 Unix 时间 2024-05-15T01:04:09Z (抓包时刻)
        4       4     padding (恒为 0)
        8       8     device_timestamp_us
                      设备运行时间 (微秒)，每帧递增约 2985μs (≈335Hz 帧率)
        16      4     frame_counter
                      帧计数器，逐帧 +1
        20      1     sensor_flags
                      0x00 = 左目传感器 (stream 0x1003)
                      0xC0 = 右目传感器 (stream 0x1004)
        21      7     reserved (恒为 0)
        28      4     field_28 (值为 128, 含义待定, 可能与传感器配置相关)
        32      4     field_32 (值为 0x6d6d8000, 含义待定)
        36      4     field_36 (值为 40, 含义待定)
        40      4     field_40 (值为 130, 可能与 ROI 画布尺寸相关)
        44      4     field_44 (值为 0x00828000, 含义待定)
        48      8     reserved (恒为 0)
        56      4     field_56 (值为 0x1ffe0000, 含义待定)
        60      4     reserved (恒为 0)
        64      4     field_64 (值为 0x00017f00 = 98048, 含义待定)
        68      12    reserved (恒为 0)

    返回: dict
    """
    if len(frame_data) < 80:
        return None

    magic = struct.unpack_from('<I', frame_data, 0)[0]
    padding_4 = struct.unpack_from('<I', frame_data, 4)[0]
    device_timestamp_us = struct.unpack_from('<Q', frame_data, 8)[0]
    frame_counter = struct.unpack_from('<I', frame_data, 16)[0]
    sensor_flags = frame_data[20]

    # 识别传感器
    if sensor_flags == 0x00:
        sensor = 'LEFT'
    elif sensor_flags == 0xC0:
        sensor = 'RIGHT'
    else:
        sensor = 'UNKNOWN(0x%02x)' % sensor_flags

    # 读取剩余已知字段
    field_28 = struct.unpack_from('<I', frame_data, 28)[0]
    field_32 = struct.unpack_from('<I', frame_data, 32)[0]
    field_36 = struct.unpack_from('<I', frame_data, 36)[0]
    field_40 = struct.unpack_from('<I', frame_data, 40)[0]
    field_44 = struct.unpack_from('<I', frame_data, 44)[0]
    field_56 = struct.unpack_from('<I', frame_data, 56)[0]
    field_64 = struct.unpack_from('<I', frame_data, 64)[0]

    return {
        'magic': magic,
        'device_timestamp_us': device_timestamp_us,
        'device_timestamp_s': device_timestamp_us / 1e6,
        'frame_counter': frame_counter,
        'sensor_flags': sensor_flags,
        'sensor': sensor,
        'field_28': field_28,
        'field_32': field_32,
        'field_36': field_36,
        'field_40': field_40,
        'field_44': field_44,
        'field_56': field_56,
        'field_64': field_64,
    }


# =============================================================================
# Layer 3: ROI 稀疏图像编码
# =============================================================================

def decode_roi_body(body):
    """
    解码 ROI 编码体为行段列表。

    编码规则 (每条记录 16 字节):
        - 0x80 字节为行头部/尾部填充 (背景值)
        - 0x00 字节为记录尾部填充
        - 如果去除填充后首字节 < 0x80，该字节为新行段的 X 起始位置
          后续字节为该段的灰度像素值
        - 如果首字节 >= 0x80，这些字节是当前行段的续接像素数据
        - 全填充记录 (全 0x80/0x00) 表示行边界

    参数:
        body: bytes, 帧体数据 (帧数据去除前 80 字节头)

    返回:
        list of list of dict: 每行包含多个段,
            每段格式: {'x': int, 'pixels': list[int]}
            x = -1 表示续接段
    """
    records = [body[i:i+16] for i in range(0, len(body), 16)]
    rows = []
    current_row = []

    for rec in records:
        # 去除首部 0x80 填充和尾部 0x00 填充
        ds, de = 0, 16
        while ds < 16 and rec[ds] == 0x80:
            ds += 1
        while de > ds and rec[de - 1] == 0x00:
            de -= 1

        if ds >= de:
            # 全填充记录 = 行边界
            if current_row:
                rows.append(current_row)
                current_row = []
            continue

        meaningful = rec[ds:de]
        if meaningful[0] < 0x80:
            # 新段: 首字节是 X 起始位置
            current_row.append({'x': meaningful[0], 'pixels': list(meaningful[1:])})
        else:
            # 续接像素
            current_row.append({'x': -1, 'pixels': list(meaningful)})

    if current_row:
        rows.append(current_row)

    return rows


def build_canvas(rows, max_w=300, max_h=300):
    """
    将行段列表渲染为灰度图画布。

    返回: numpy 2D array (uint8)
    """
    canvas = np.zeros((max_h, max_w), dtype=np.uint8)
    y = 0
    for row_segs in rows:
        x = 0
        for seg in row_segs:
            if seg['x'] >= 0:
                x = seg['x']
            for px in seg['pixels']:
                if 0 <= x < max_w and 0 <= y < max_h:
                    canvas[y, x] = px
                x += 1
        y += 1
    return canvas[:y, :]


def extract_blobs(canvas, threshold=80, min_area=10):
    """
    从灰度画布中提取连通域 (blob)。

    对应物理含义: 每个 blob 是一个红外反光点,
    对应定位工具上的反光小球在相机传感器上的像。

    参数:
        canvas: numpy 2D array
        threshold: 二值化阈值
        min_area: 最小面积筛选

    返回:
        list of dict: 每个 blob 包含:
            - cx, cy: 亮度加权质心 (像素坐标)
            - area: 面积 (像素数)
            - peak: 峰值亮度
            - bbox: (x_min, y_min, x_max, y_max)
    """
    binary = (canvas > threshold).astype(np.uint8)
    labeled, num_features = ndimage.label(binary)
    blobs = []

    for i in range(1, num_features + 1):
        blob_mask = (labeled == i)
        ys, xs = np.where(blob_mask)
        if len(xs) < min_area:
            continue

        weights = canvas[blob_mask].astype(float)
        cx = float(np.average(xs, weights=weights))
        cy = float(np.average(ys, weights=weights))
        area = int(len(xs))
        peak = int(canvas[blob_mask].max())
        bbox = (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))

        blobs.append({
            'cx': cx,
            'cy': cy,
            'area': area,
            'peak': peak,
            'bbox': bbox,
        })

    # 按面积降序排列
    blobs.sort(key=lambda b: -b['area'])
    return blobs


# =============================================================================
# pcapng 文件读取
# =============================================================================

def read_pcapng_udp(filename):
    """
    读取 pcapng 文件，提取 UDP 数据包。

    返回: list of dict，每个包含 src_ip, dst_ip, src_port, dst_port, payload
    """
    results = []
    with open(filename, 'rb') as f:
        data = f.read()

    pos = 0
    while pos < len(data) - 12:
        block_type = struct.unpack_from('<I', data, pos)[0]
        block_len = struct.unpack_from('<I', data, pos + 4)[0]

        if block_type == 6:  # Enhanced Packet Block
            if pos + 28 <= len(data):
                cap_len = struct.unpack_from('<I', data, pos + 20)[0]
                pkt_data = data[pos + 28:pos + 28 + cap_len]

                if len(pkt_data) >= 42:
                    eth_type = struct.unpack_from('>H', pkt_data, 12)[0]
                    if eth_type == 0x0800:  # IPv4
                        ip_hdr_len = (pkt_data[14] & 0x0F) * 4
                        ip_proto = pkt_data[23]
                        src_ip = '.'.join(str(b) for b in pkt_data[26:30])
                        dst_ip = '.'.join(str(b) for b in pkt_data[30:34])

                        if ip_proto == 17:  # UDP
                            udp_start = 14 + ip_hdr_len
                            if udp_start + 8 <= len(pkt_data):
                                src_port = struct.unpack_from('>H', pkt_data, udp_start)[0]
                                dst_port = struct.unpack_from('>H', pkt_data, udp_start + 2)[0]
                                payload = pkt_data[udp_start + 8:]
                                results.append({
                                    'src_ip': src_ip,
                                    'dst_ip': dst_ip,
                                    'src_port': src_port,
                                    'dst_port': dst_port,
                                    'payload': payload,
                                })

        if block_len < 12:
            break
        pos += block_len

    return results


# =============================================================================
# 主解码流程
# =============================================================================

def decode_pcapng(filename, threshold=80, min_area=100, last_n=None, all_blobs=False):
    """
    完整解码 pcapng 文件中的 fusionTrack 数据。

    返回:
        dict: 包含 summary 和 frames 列表
    """
    print("读取 %s ..." % filename)
    packets = read_pcapng_udp(filename)
    print("  UDP 包总数: %d" % len(packets))

    # 筛选相机->电脑的数据包
    cam_packets = [p for p in packets
                   if p['src_ip'] == CAMERA_IP and p['src_port'] == CAMERA_PORT]
    print("  相机数据包: %d" % len(cam_packets))

    # 也记录控制包 (电脑->相机)
    ctrl_packets = [p for p in packets
                    if p['dst_ip'] == CAMERA_IP and p['dst_port'] == CAMERA_PORT]
    print("  控制命令包: %d" % len(ctrl_packets))

    # 重组帧
    print("重组帧...")
    assembled = reassemble_frames(cam_packets)
    print("  重组成功: %d 帧" % len(assembled))

    # 按流分组
    stream_frames = defaultdict(list)
    for (stag, ftok), data in assembled.items():
        stream_frames[stag].append((ftok, data))
    for stag in stream_frames:
        stream_frames[stag].sort()

    print("  数据流:")
    for stag in sorted(stream_frames):
        flist = stream_frames[stag]
        sizes = sorted(set(len(d) for _, d in flist))
        print("    0x%04x: %d 帧, 尺寸范围 %d-%d" % (stag, len(flist), sizes[0], sizes[-1]))

    # 找出两路流共有的 frame_token
    all_tags = sorted(stream_frames.keys())
    if STREAM_TAG_LEFT in stream_frames and STREAM_TAG_RIGHT in stream_frames:
        left_tokens = {ft: d for ft, d in stream_frames[STREAM_TAG_LEFT]}
        right_tokens = {ft: d for ft, d in stream_frames[STREAM_TAG_RIGHT]}
        common_tokens = sorted(set(left_tokens.keys()) & set(right_tokens.keys()))
    else:
        left_tokens = {}
        right_tokens = {}
        common_tokens = []
        print("  警告: 未找到 0x1003/0x1004 双流!")

    print("  左右目匹配帧: %d" % len(common_tokens))

    if last_n is not None and last_n < len(common_tokens):
        common_tokens = common_tokens[-last_n:]
        print("  (只处理最后 %d 帧)" % last_n)

    # 解码每帧
    print("解码帧数据 (threshold=%d, min_area=%d)..." % (threshold, min_area))
    decoded_frames = []
    blob_area_for_filter = 10 if all_blobs else min_area

    for idx, ftok in enumerate(common_tokens):
        frame_result = {
            'frame_token': ftok,
            'left': None,
            'right': None,
        }

        for sensor_name, sensor_data in [('left', left_tokens.get(ftok)),
                                         ('right', right_tokens.get(ftok))]:
            if sensor_data is None:
                continue

            # 解析内层帧头
            header = parse_inner_frame_header(sensor_data)

            # 解码 ROI 体
            body = sensor_data[80:]
            rows = decode_roi_body(body)
            canvas = build_canvas(rows)
            blobs = extract_blobs(canvas, threshold=threshold, min_area=blob_area_for_filter)

            # 大面积 blob (反光小球候选)
            significant_blobs = [b for b in blobs if b['area'] >= min_area]

            frame_result[sensor_name] = {
                'header': header,
                'frame_size': len(sensor_data),
                'body_size': len(body),
                'roi_rows': len(rows),
                'canvas_shape': list(canvas.shape),
                'total_blobs': len(blobs),
                'significant_blobs': len(significant_blobs),
                'blobs': [{
                    'cx': round(b['cx'], 3),
                    'cy': round(b['cy'], 3),
                    'area': b['area'],
                    'peak': b['peak'],
                    'bbox': list(b['bbox']),
                } for b in (blobs if all_blobs else significant_blobs)],
            }

        decoded_frames.append(frame_result)

        if (idx + 1) % 200 == 0:
            print("  已处理 %d/%d 帧..." % (idx + 1, len(common_tokens)))

    print("  解码完成: %d 帧" % len(decoded_frames))

    # 统计分析
    left_blob_counts = [f['left']['significant_blobs'] for f in decoded_frames if f['left']]
    right_blob_counts = [f['right']['significant_blobs'] for f in decoded_frames if f['right']]

    # 帧率估算
    if len(decoded_frames) >= 2:
        first_ts = decoded_frames[0]['left']['header']['device_timestamp_us'] if decoded_frames[0]['left'] else 0
        last_ts = decoded_frames[-1]['left']['header']['device_timestamp_us'] if decoded_frames[-1]['left'] else 0
        if last_ts > first_ts:
            duration_s = (last_ts - first_ts) / 1e6
            fps = (len(decoded_frames) - 1) / duration_s
        else:
            duration_s = 0
            fps = 0
    else:
        duration_s = 0
        fps = 0

    summary = {
        'source_file': os.path.basename(filename),
        'total_udp_packets': len(packets),
        'camera_packets': len(cam_packets),
        'control_packets': len(ctrl_packets),
        'total_frames_assembled': len(assembled),
        'matched_stereo_frames': len(common_tokens),
        'decoded_frames': len(decoded_frames),
        'duration_seconds': round(duration_s, 3),
        'estimated_fps': round(fps, 1),
        'threshold': threshold,
        'min_area': min_area,
        'left_blob_count_distribution': dict(Counter(left_blob_counts)),
        'right_blob_count_distribution': dict(Counter(right_blob_counts)),
    }

    return {
        'summary': summary,
        'frames': decoded_frames,
    }


def print_frame_detail(frame, idx=None):
    """打印单帧详细信息"""
    prefix = "帧 %d" % idx if idx is not None else "帧"
    token = frame['frame_token']

    left = frame['left']
    right = frame['right']

    if left:
        hdr = left['header']
        print("\n%s [token=%d, counter=%d, device_time=%.6fs]:" % (
            prefix, token, hdr['frame_counter'], hdr['device_timestamp_s']))
    else:
        print("\n%s [token=%d]:" % (prefix, token))

    for sensor_name, sensor_label in [('left', '左目'), ('right', '右目')]:
        sensor = frame[sensor_name]
        if sensor is None:
            print("  %s: 无数据" % sensor_label)
            continue

        hdr = sensor['header']
        print("  %s (0x%02x): 帧大小=%d, ROI行数=%d, 画布=%dx%d" % (
            sensor_label, hdr['sensor_flags'],
            sensor['frame_size'], sensor['roi_rows'],
            sensor['canvas_shape'][1], sensor['canvas_shape'][0]))
        print("    检测到 %d 个 blob (显著: %d)" % (
            sensor['total_blobs'], sensor['significant_blobs']))

        for j, blob in enumerate(sensor['blobs']):
            print("    blob %d: 质心=(%.3f, %.3f) 面积=%d 峰值=%d 包围盒=%s" % (
                j, blob['cx'], blob['cy'], blob['area'], blob['peak'], blob['bbox']))


def print_summary(result):
    """打印解码摘要"""
    s = result['summary']
    print("\n" + "=" * 70)
    print("fusionTrack UDP 数据解码摘要")
    print("=" * 70)
    print("源文件:           %s" % s['source_file'])
    print("UDP 包总数:       %d" % s['total_udp_packets'])
    print("相机数据包:       %d" % s['camera_packets'])
    print("控制命令包:       %d" % s['control_packets'])
    print("重组帧总数:       %d" % s['total_frames_assembled'])
    print("双目匹配帧数:     %d" % s['matched_stereo_frames'])
    print("解码帧数:         %d" % s['decoded_frames'])
    print("持续时间:         %.3f 秒" % s['duration_seconds'])
    print("估计帧率:         %.1f Hz" % s['estimated_fps'])
    print("二值化阈值:       %d" % s['threshold'])
    print("最小面积:         %d 像素" % s['min_area'])
    print("\n左目 blob 数量分布: %s" % s['left_blob_count_distribution'])
    print("右目 blob 数量分布: %s" % s['right_blob_count_distribution'])

    print("\n" + "-" * 70)
    print("协议结构概览:")
    print("-" * 70)
    print("""
  ┌─────────────────────────────────────────────┐
  │ UDP 数据包                                   │
  │ ┌─────────────────────────────────────────┐ │
  │ │ Layer 1: 24 字节私有分片头               │ │
  │ │   stream_tag (0x1003=左/0x1004=右)      │ │
  │ │   packet_words, sequence                │ │
  │ │   frame_token, frame_size, offset       │ │
  │ ├─────────────────────────────────────────┤ │
  │ │ 分片数据 (重组后为完整帧)               │ │
  │ │ ┌─────────────────────────────────────┐ │ │
  │ │ │ Layer 2: 80 字节内层帧头             │ │ │
  │ │ │   magic (unix时间戳)                 │ │ │
  │ │ │   device_timestamp_us               │ │ │
  │ │ │   frame_counter                     │ │ │
  │ │ │   sensor_flags (0x00=左/0xC0=右)    │ │ │
  │ │ ├─────────────────────────────────────┤ │ │
  │ │ │ Layer 3: ROI 稀疏图像数据体         │ │ │
  │ │ │   16字节定长记录 × N                │ │ │
  │ │ │   编码: 行段压缩稀疏灰度图          │ │ │
  │ │ │   内容: 红外反光点检测结果           │ │ │
  │ │ │   每个亮斑 → 一个 blob → 反光小球   │ │ │
  │ │ └─────────────────────────────────────┘ │ │
  │ └─────────────────────────────────────────┘ │
  └─────────────────────────────────────────────┘

  SDK 后处理 (不在网络数据中):
    左目 blob + 右目 blob
      → 立体匹配 → 三角测量 → 3D 坐标 (ftk3DFiducial)
      → 几何匹配 → 位置 + 旋转 (ftkMarker)
""")


# =============================================================================
# 入口
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='fusionTrack 双目相机 UDP 抓包解码器',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('pcapng', nargs='?',
                        default='fusionTrack SDK x64/output/full_02.pcapng',
                        help='pcapng 文件路径')
    parser.add_argument('--threshold', type=int, default=80,
                        help='二值化阈值 (默认 80)')
    parser.add_argument('--min-area', type=int, default=100,
                        help='最小 blob 面积 (默认 100)')
    parser.add_argument('--json', type=str, default=None,
                        help='输出 JSON 文件路径')
    parser.add_argument('--last', type=int, default=None,
                        help='只处理最后 N 帧')
    parser.add_argument('--all-blobs', action='store_true',
                        help='输出所有 blob (不只是大面积的)')

    args = parser.parse_args()

    if not os.path.exists(args.pcapng):
        print("错误: 文件不存在: %s" % args.pcapng)
        sys.exit(1)

    result = decode_pcapng(
        args.pcapng,
        threshold=args.threshold,
        min_area=args.min_area,
        last_n=args.last,
        all_blobs=args.all_blobs,
    )

    # 打印摘要
    print_summary(result)

    # 打印最后几帧的详细信息
    frames = result['frames']
    n_detail = min(5, len(frames))
    if n_detail > 0:
        print("\n" + "=" * 70)
        print("最后 %d 帧详细信息:" % n_detail)
        print("=" * 70)
        for i in range(len(frames) - n_detail, len(frames)):
            print_frame_detail(frames[i], idx=i)

    # 输出 JSON
    if args.json:
        # 将 numpy/bytes 类型转为 JSON 可序列化类型
        def json_safe(obj):
            if isinstance(obj, (np.integer,)):
                return int(obj)
            if isinstance(obj, (np.floating,)):
                return float(obj)
            if isinstance(obj, np.ndarray):
                return obj.tolist()
            if isinstance(obj, bytes):
                return obj.hex()
            return obj

        with open(args.json, 'w', encoding='utf-8') as f:
            json.dump(result, f, default=json_safe, indent=2, ensure_ascii=False)
        print("\nJSON 输出已保存至: %s" % args.json)


if __name__ == '__main__':
    main()
