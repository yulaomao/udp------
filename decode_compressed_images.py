#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fusionTrack PictureCompressor V3 8-bit 图像解压缩 & pcapng 图像提取
=====================================================================

通过对 fusionTrack64.dll 的深度反汇编逆向工程，完整还原了
PictureCompressor::decompress() 的 V3 8-bit 压缩算法，并用 Python
实现了等价的解压缩逻辑，从 pcapng 网络抓包中提取并保存红外追踪图像。

DLL 逆向定位:
  - decompressV3_8bit 入口:     RVA 0x001f1b80 (验证+参数检查)
  - decompressV3_8bit 核心:     RVA 0x001f1cd0 (RLE解码循环, 440字节)
  - compressV3_8bit 入口:       RVA 0x001f15a0 (压缩初始化)
  - compressV3_8bit 内循环:     RVA 0x001f1746 (逐像素编码, 558字节)

传感器参数 (通过 Capstone 反汇编 + 数据验证确定):
  ─────────────────────────────────────────────
  传感器全分辨率:  2048 × 1088 像素
  ROI (兴趣区域):  设备只传输包含有效数据的行范围
  ROI 起始行:      内层帧头 offset 65-66 (u16 LE)
  验证:            ROI_start + channel_count == 1088 (所有帧均成立)
  ─────────────────────────────────────────────

压缩格式 (V3 8-bit):
  ─────────────────────────────────────────────
  字节编码:
    0x81-0xFF : 像素值, 原始值 = (byte - 0x80) * 2, x前进1
    0x01-0x7F : 跳过计数, x前进此值 (暗像素区域)
    0x80      : 跳过128像素 (完整暗块标记)
    0x00      : 填充/行尾标记 (像素处理中忽略)

  行结构:
    每行恰好 width 个像素 (跳过+数据合计)
    行数据填充到16字节边界
    行间跳过记录: [0x00][skip_lo][skip_hi][13个零字节] (16字节)
    skip_count = 连续空白行数

  编码器流程 (来自 RVA 0x001f1746):
    1. 逐行处理, 每行以128像素块为单位
    2. 像素 < 阈值: 累计跳过计数
    3. 像素 >= 阈值: 编码为 (pixel >> 1) + 0x80
    4. 128像素全暗: 写入0x80标记
    5. 行末填充0x00到16字节边界
    6. 连续空白行: 累计后写入跳过记录
  ─────────────────────────────────────────────

  输出图像尺寸:
    所有帧统一输出为 2048 × 1088 的完整传感器图像，
    ROI 数据根据帧头中的 ROI 起始行放置在正确位置。
    这确保了左右目图像尺寸一致，可直接用于:
    - 圆心拟合 (circle center fitting)
    - 基线匹配 (baseline matching)
    - 立体匹配 (stereo matching)

用法:
    python decode_compressed_images.py [pcapng_file] [--output-dir DIR] [--max-frames N]
    python decode_compressed_images.py fusionTrack\\ SDK\\ x64/output/full_03.pcapng

依赖: scapy, Pillow
"""

from __future__ import annotations

import argparse
import struct
import sys
from collections import defaultdict
from pathlib import Path

from PIL import Image, ImageOps
from scapy.all import IP, IPv6, Raw, UDP, PcapNgReader


# ─── 常量 ──────────────────────────────────────────────────────────────────

INNER_HEADER_BYTES = 80          # 内层帧头部大小
IMAGE_WIDTH = 2048               # 传感器宽度 (从字节级跟踪验证)
IMAGE_HEIGHT = 1088              # 传感器高度 (通过 Capstone 反汇编 + ROI 验证确认)
ROI_START_OFFSET = 65            # ROI起始行字段在内层帧头中的偏移 (u16 LE)
STREAM_TAG_LEFT = 0x1003         # 左目 ROI 流标记
STREAM_TAG_RIGHT = 0x1004        # 右目 ROI 流标记


# ─── UDP分片重组 ───────────────────────────────────────────────────────────

def parse_vendor_header(payload: bytes):
    """解析24字节设备分片头."""
    if len(payload) < 24:
        return None
    stream_tag, packet_words, seq, reserved, frame_token, frame_size, payload_offset = \
        struct.unpack("<HHIIIII", payload[:24])
    if packet_words * 2 != len(payload):
        return None
    if payload_offset > frame_size:
        return None
    if (len(payload) - 24) + payload_offset > frame_size:
        return None
    return stream_tag, frame_token, frame_size, payload_offset


def get_ip_layer(packet):
    """获取IP层."""
    if IP in packet:
        return packet[IP]
    if IPv6 in packet:
        return packet[IPv6]
    return None


def reassemble_frames(pcap_path: str | Path) -> dict[int, list[dict]]:
    """
    从 pcapng 文件重组完整帧.

    返回: {stream_tag: [{token, size, data}, ...]}
    """
    fragment_map: dict[tuple[int, int], dict] = defaultdict(
        lambda: {"chunks": [], "frame_size": 0}
    )

    with PcapNgReader(str(pcap_path)) as reader:
        for packet in reader:
            if UDP not in packet or Raw not in packet:
                continue
            ip = get_ip_layer(packet)
            if ip is None:
                continue

            payload = bytes(packet[Raw].load)
            parsed = parse_vendor_header(payload)
            if parsed is None:
                continue

            stream_tag, frame_token, frame_size, payload_offset = parsed
            key = (stream_tag, frame_token)
            fragment_map[key]["frame_size"] = frame_size
            fragment_map[key]["chunks"].append((payload_offset, payload[24:]))

    complete_frames: dict[int, list[dict]] = defaultdict(list)

    for (stream_tag, frame_token), info in sorted(fragment_map.items()):
        chunks = sorted(info["chunks"], key=lambda c: c[0])
        frame_size = info["frame_size"]
        buf = bytearray(frame_size)

        expected = 0
        complete = True
        for offset, chunk in chunks:
            if offset != expected:
                complete = False
            end = offset + len(chunk)
            if end > frame_size:
                complete = False
                break
            buf[offset:end] = chunk
            expected = end

        if expected != frame_size:
            complete = False

        if complete:
            complete_frames[stream_tag].append(
                {"token": frame_token, "size": frame_size, "data": bytes(buf)}
            )

    return dict(complete_frames)


# ─── V3 8-bit 解压缩 (从DLL逆向还原) ──────────────────────────────────────

def decompress_v3_8bit(body: bytes, width: int = IMAGE_WIDTH) -> list[dict[int, int]]:
    """
    解压缩 V3 8-bit 压缩图像数据.

    还原自 fusionTrack64.dll:
      - 验证函数:  RVA 0x001f1b80  (CompressedData 结构校验)
      - 解码核心:  RVA 0x001f1cd0  (RLE解码主循环)
      - 编码器:    RVA 0x001f1746  (参考编码逻辑反推格式)

    算法:
    ──────────────────────────────────────────────────────
    1. 按字节顺序读取压缩数据流
    2. 对每个字节 b:
       - b == 0x00: 行末填充, 忽略
       - 0x01 <= b <= 0x7F: 跳过 b 个暗像素, x += b
       - b == 0x80: 跳过128个暗像素 (完整暗块), x += 128
       - 0x81 <= b <= 0xFF: 像素值, raw = (b - 0x80) * 2, x += 1
    3. 当 x 累计达到 width: 一行结束, x 归零, 跳过填充到下一个16字节边界
    4. 在16字节边界处检测跳过记录:
       [0x00][skip_lo][skip_hi][13个零字节]
       skip_count > 0: 插入该数量的空白行
    ──────────────────────────────────────────────────────

    参数:
        body:  去掉80字节内层帧头后的压缩数据
        width: 图像宽度 (默认 2048, 从DLL逆向确认)

    返回:
        行列表, 每行是 {x_position: pixel_value} 字典
        空行用空字典 {} 表示
    """
    all_rows: list[dict[int, int]] = []
    current_row: dict[int, int] = {}
    x = 0
    i = 0

    while i < len(body):
        # ── 在16字节边界检测行间跳过记录 ──
        if i % 16 == 0 and i + 16 <= len(body):
            block = body[i : i + 16]
            if block[0] == 0x00 and all(b == 0 for b in block[3:16]):
                skip_count = block[1] | (block[2] << 8)
                # 保存当前行 (如果有未保存的数据)
                if current_row:
                    all_rows.append(current_row)
                    current_row = {}
                    x = 0
                # 插入空白行
                for _ in range(skip_count):
                    all_rows.append({})
                i += 16
                continue

        b = body[i]
        i += 1

        if b == 0x00:
            # 填充字节, 忽略
            continue
        elif b == 0x80:
            # 跳过128像素 (完整暗块标记)
            # DLL编码器: 当连续128像素全低于阈值时写入此标记
            x += 128
        elif b < 0x80:
            # 跳过计数: 1-127个暗像素
            x += b
        else:
            # 像素值: 0x81-0xFF
            # DLL编码器 (RVA 0x001f17f3): shr al, 1; add al, 0x80
            # 解码: pixel = (byte - 0x80) * 2
            pixel = (b - 0x80) * 2
            current_row[x] = pixel
            x += 1

        # ── 检查是否完成一整行 ──
        if x >= width:
            all_rows.append(current_row)
            current_row = {}
            x = 0
            # 跳过填充零到下一个16字节边界
            while i < len(body) and i % 16 != 0:
                i += 1

    # 保存最后一行
    if current_row:
        all_rows.append(current_row)

    return all_rows


# ─── 图像渲染 ──────────────────────────────────────────────────────────────

def rows_to_image(
    rows: list[dict[int, int]],
    width: int = IMAGE_WIDTH,
    height: int = IMAGE_HEIGHT,
    roi_start_row: int = 0,
) -> tuple[Image.Image | None, Image.Image | None]:
    """
    将稀疏行数据转换为固定尺寸的 PIL 图像.

    所有帧输出为统一的 width × height 传感器图像:
    - ROI 数据从 roi_start_row 开始放置
    - 非 ROI 区域填充为黑色 (0)
    - 确保左右目图像尺寸一致，可用于立体匹配

    参数:
        rows:          解压缩后的行数据列表
        width:         传感器完整宽度 (默认 2048)
        height:        传感器完整高度 (默认 1088)
        roi_start_row: ROI 在传感器中的起始行 (从帧头 offset 65-66 读取)

    返回: (full_image, cropped_image)
        full_image:    固定 width × height 的完整传感器图像
        cropped_image: 内容区域裁剪+边距的图像 (用于快速预览)
    """
    if not rows:
        return None, None

    # 创建固定尺寸的完整传感器图像
    img = Image.new("L", (width, height), 0)
    pixels = img.load()

    # 将 ROI 数据放置在正确的传感器位置
    min_x, max_x = width, 0
    min_y, max_y = height, 0

    for row_idx, row in enumerate(rows):
        y = roi_start_row + row_idx
        if y >= height:
            break
        for x_pos, val in row.items():
            if 0 <= x_pos < width:
                pixels[x_pos, y] = min(255, val)
                if row:
                    min_x = min(min_x, x_pos)
                    max_x = max(max_x, x_pos)
        if row:
            min_y = min(min_y, y)
            max_y = max(max_y, y)

    if max_x < min_x:
        return None, None

    # 裁剪到内容区域 (带边距)
    margin = 10
    crop_x1 = max(0, min_x - margin)
    crop_y1 = max(0, min_y - margin)
    crop_x2 = min(width, max_x + margin + 1)
    crop_y2 = min(height, max_y + margin + 1)
    cropped = img.crop((crop_x1, crop_y1, crop_x2, crop_y2))

    return img, cropped


def parse_inner_header(data: bytes) -> dict:
    """
    解析80字节内层帧头.

    帧头布局 (通过 Capstone 反汇编 fusionTrack64.dll 确定):
      offset  0-3:   magic (0x66441819)
      offset  4-7:   保留 (0)
      offset  8-11:  device_timestamp
      offset 12-15:  version (=1)
      offset 16-19:  frame_counter
      offset 20-23:  camera_flags (LEFT=0x00, RIGHT=0xC0)
      offset 24-27:  保留 (0)
      offset 28-31:  row_stride (=128, 即 width/128 的单位)
      offset 64:     保留 (0)
      offset 65-66:  ROI 起始行 (u16 LE)
                     DLL中: [rsi + 0x45] 存储 width, [rsi + 0x47] 存储 height
                     调用者从 [rax + 0x15] 和 [rax + 0x17] 读取
                     验证: roi_start + roi_height == 1088 (全传感器高度)
    """
    if len(data) < INNER_HEADER_BYTES:
        return {}
    u32 = struct.unpack("<20I", data[:INNER_HEADER_BYTES])
    roi_start = struct.unpack_from("<H", data, ROI_START_OFFSET)[0]
    return {
        "magic": u32[0],
        "device_timestamp": u32[2],
        "version": u32[3],
        "frame_counter": u32[4],
        "camera_flags": u32[5],
        "row_stride": u32[7],
        "roi_start_row": roi_start,
    }


# ─── 主程序 ────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="fusionTrack PictureCompressor V3 8-bit 图像解码器"
    )
    parser.add_argument(
        "pcapng",
        nargs="?",
        default="fusionTrack SDK x64/output/full_03.pcapng",
        help="pcapng 捕获文件路径",
    )
    parser.add_argument(
        "--output-dir",
        default="decoded_images",
        help="输出目录 (默认: decoded_images)",
    )
    parser.add_argument(
        "--max-frames",
        type=int,
        default=5,
        help="每个相机最多解码帧数 (默认: 5)",
    )
    parser.add_argument(
        "--width",
        type=int,
        default=IMAGE_WIDTH,
        help=f"传感器完整宽度 (默认: {IMAGE_WIDTH})",
    )
    parser.add_argument(
        "--height",
        type=int,
        default=IMAGE_HEIGHT,
        help=f"传感器完整高度 (默认: {IMAGE_HEIGHT})",
    )
    parser.add_argument(
        "--full",
        action="store_true",
        help="同时保存完整尺寸图像 (2048×1088)",
    )
    args = parser.parse_args()

    pcap_path = Path(args.pcapng)
    if not pcap_path.exists():
        print(f"错误: 文件不存在: {pcap_path}", file=sys.stderr)
        sys.exit(1)

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"解析 pcapng: {pcap_path}")
    print(f"输出目录:    {output_dir}")
    print(f"传感器尺寸:  {args.width} × {args.height}")
    print()

    # 重组帧
    print("正在重组 UDP 分片帧...")
    all_frames = reassemble_frames(pcap_path)

    stream_names = {STREAM_TAG_LEFT: "left", STREAM_TAG_RIGHT: "right"}
    total_saved = 0

    for tag, name in stream_names.items():
        if tag not in all_frames:
            print(f"\n⚠ 未找到 {name} 相机数据流 (0x{tag:04x})")
            continue

        frames = sorted(all_frames[tag], key=lambda f: f["token"])
        decode_count = min(args.max_frames, len(frames))

        print(f"\n{'='*60}")
        print(f"  {name.upper()} 相机 (0x{tag:04x})")
        print(f"  总帧数: {len(frames)}, 解码: {decode_count}")
        print(f"{'='*60}")

        for idx, frame in enumerate(frames[:decode_count]):
            data = frame["data"]
            header = parse_inner_header(data)
            body = data[INNER_HEADER_BYTES:]

            # 提取 ROI 起始行
            roi_start = header.get("roi_start_row", 0)

            # 解压缩
            rows = decompress_v3_8bit(body, width=args.width)

            # 统计
            total_pixels = sum(len(r) for r in rows)
            active_rows = sum(1 for r in rows if r)
            roi_height = len(rows)

            # 渲染为固定尺寸图像 (width × height)
            img_full, img_crop = rows_to_image(
                rows,
                width=args.width,
                height=args.height,
                roi_start_row=roi_start,
            )

            if img_crop is None:
                print(f"  帧 {idx}: token={frame['token']} - 无有效像素")
                continue

            # 保存裁剪+增强图像
            enhanced = ImageOps.autocontrast(img_crop)
            crop_path = output_dir / f"{name}_frame_{idx:03d}.png"
            enhanced.save(crop_path)
            total_saved += 1

            # 保存完整传感器图像 (固定尺寸)
            if args.full and img_full:
                full_path = output_dir / f"{name}_frame_{idx:03d}_full.png"
                img_full.save(full_path)

            print(
                f"  帧 {idx}: token={frame['token']:>8d} | "
                f"ROI: row {roi_start:>4d}+{roi_height:>4d} ({active_rows:>3d} 有效) | "
                f"{total_pixels:>5d} 像素 | "
                f"输出: {args.width}×{args.height} | "
                f"裁剪: {img_crop.size[0]}×{img_crop.size[1]} → {crop_path.name}"
            )

    print(f"\n✓ 完成! 共保存 {total_saved} 张图像到 {output_dir}/")


if __name__ == "__main__":
    main()
