#!/usr/bin/env python3
"""
近红外双目相机 UDP 抓包数据解码器
Decoder for NIR stereo camera UDP packet capture (stylus tracking system)
协议分析摘要 / Protocol Analysis Summary
==========================================
- 传输层: UDP, 相机 (172.17.1.7:3509) → PC (172.17.1.56:50316)
- 每个 UDP 包含 24 字节传输头 + 图像帧分片数据
- 图像帧由多个 UDP 包重组而成，通过 frame_id 和 offset 字段重组
- 两个通道: channel 3 和 channel 4 (不同曝光/处理模式)
- 每个通道帧包含 80 字节元数据头 + 图像数据 + 可选跟踪数据
- 图像格式: 48 字节宽行, 包含两个 24 像素宽的子图像(左/右相机)
- 像素格式: 8-bit 灰度, 0x80(128)为背景, 0xFF(255)为IR LED饱和
Transport: UDP, Camera (172.17.1.7:3509) -> PC (172.17.1.56:50316)
Each UDP packet has a 24-byte transport header + frame fragment payload.
Frames are reassembled from multiple UDP packets using frame_id + offset.
Two channels: channel 3 and channel 4 (different exposure/processing modes).
Each channel frame: 80-byte metadata header + image data + optional tracking data.
Image format: 48-byte wide rows containing two 24-pixel sub-images (left/right camera).
Pixel format: 8-bit grayscale, 0x80 (128) = background, 0xFF (255) = IR LED saturation.
"""
import struct
import os
import sys
import json
from collections import defaultdict
try:
    from scapy.all import rdpcap, UDP, IP, Raw
except ImportError:
    print("错误: 需要安装 scapy 库 / Error: scapy library required")
    print("  pip install scapy")
    sys.exit(1)
try:
    from PIL import Image
except ImportError:
    print("错误: 需要安装 Pillow 库 / Error: Pillow library required")
    print("  pip install Pillow")
    sys.exit(1)
# ============================================================
# 常量定义 / Constants
# ============================================================
# UDP 传输包头大小
TRANSPORT_HEADER_SIZE = 24
# 帧数据中的元数据头大小
FRAME_METADATA_SIZE = 80
# 图像行宽(字节), 包含左右两个子图像
IMAGE_ROW_WIDTH = 48
# 每个子图像的宽度(像素)
SUB_IMAGE_WIDTH = 24
# 背景像素值
BACKGROUND_PIXEL = 0x80
# ============================================================
# 传输包头解析 / Transport Header Parsing
# ============================================================
def parse_transport_header(data):
    """
    解析 24 字节 UDP 传输包头
    偏移  大小  描述
    0     1     通道 ID (0x03=通道A, 0x04=通道B)
    1     1     协议标识 (固定 0x10)
    2     2     数据长度字段 (LE u16)
    4     4     序列号 (LE u32, 全局递增)
    8     4     保留 (全零)
    12    4     帧 ID (LE u32, 每帧递增)
    16    4     帧总大小 (LE u32, 该通道该帧的总字节数)
    20    4     帧内偏移 (LE u32, 该分片在帧中的偏移)
    """
    if len(data) < TRANSPORT_HEADER_SIZE:
        return None
    channel = data[0]
    protocol_id = data[1]
    data_len_field = struct.unpack_from('<H', data, 2)[0]
    sequence_num = struct.unpack_from('<I', data, 4)[0]
    reserved = struct.unpack_from('<I', data, 8)[0]
    frame_id = struct.unpack_from('<I', data, 12)[0]
    frame_size = struct.unpack_from('<I', data, 16)[0]
    frame_offset = struct.unpack_from('<I', data, 20)[0]
    return {
        'channel': channel,
        'protocol_id': protocol_id,
        'data_len_field': data_len_field,
        'sequence_num': sequence_num,
        'frame_id': frame_id,
        'frame_size': frame_size,
        'frame_offset': frame_offset,
        'payload': data[TRANSPORT_HEADER_SIZE:]
    }
# ============================================================
# 帧元数据头解析 / Frame Metadata Header Parsing
# ============================================================
def parse_frame_metadata(data):
    """
    解析帧数据中前 80 字节的元数据头
    偏移  大小  描述
    0     4     时间戳低位 (LE u32, Unix timestamp seconds)
    4     4     时间戳高位填充 (全零)
    8     4     时间戳微秒/纳秒部分
    12    4     填充
    16    4     帧计数器 (LE u32)
    20    4     传感器偏移 (0x00=通道A, 0xC0=通道B)
    24    4     保留
    28    4     参数1 (0x80 = 128)
    32    4     传感器标识 (含 0x6D6D = "mm")
    36    4     参数2 (0x28 = 40)
    40    4     参数3 (0x82 = 130)
    44    4     参数4
    48    8     保留
    56    4     配置标志 (0x1FFE)
    60    4     保留
    64    4     通道数据标识
    68    12    保留 (全零)
    """
    if len(data) < FRAME_METADATA_SIZE:
        return None
    timestamp_sec = struct.unpack_from('<I', data, 0)[0]
    timestamp_usec = struct.unpack_from('<I', data, 8)[0]
    frame_counter = struct.unpack_from('<I', data, 16)[0]
    sensor_offset = struct.unpack_from('<I', data, 20)[0]
    param_1 = struct.unpack_from('<I', data, 28)[0]
    param_2 = struct.unpack_from('<I', data, 36)[0]
    param_3 = struct.unpack_from('<I', data, 40)[0]
    config_flags = struct.unpack_from('<I', data, 52)[0]
    channel_data_id = struct.unpack_from('<I', data, 64)[0]
    return {
        'timestamp_sec': timestamp_sec,
        'timestamp_usec': timestamp_usec,
        'frame_counter': frame_counter,
        'sensor_offset': sensor_offset,
        'param_1': param_1,
        'param_2': param_2,
        'param_3': param_3,
        'config_flags': config_flags,
        'channel_data_id': channel_data_id,
    }
# ============================================================
# 帧重组 / Frame Reassembly
# ============================================================
def extract_frames(pcap_file):
    """
    从 pcapng 文件中提取并重组所有帧
    返回: dict of (frame_id, channel) -> { 'frame_id', 'channel', 'frame_size', 'data' }
    """
    print(f"读取抓包文件: {pcap_file}")
    packets = rdpcap(pcap_file)
    print(f"总包数: {len(packets)}")
    # 统计
    stats = {
        'total_packets': len(packets),
        'udp_packets': 0,
        'camera_packets': 0,
        'pc_packets': 0,
        'broadcast_packets': 0,
    }
    # 帧分片收集
    frame_chunks = {}
    for pkt in packets:
        if not pkt.haslayer(UDP):
            continue
        stats['udp_packets'] += 1
        if not pkt.haslayer(Raw):
            continue
        ip_src = pkt[IP].src
        ip_dst = pkt[IP].dst
        if ip_src == "172.17.1.7":
            stats['camera_packets'] += 1
        elif "255" in ip_dst:
            stats['broadcast_packets'] += 1
            continue
        else:
            stats['pc_packets'] += 1
            continue
        raw_data = bytes(pkt[Raw].load)
        header = parse_transport_header(raw_data)
        if header is None:
            continue
        key = (header['frame_id'], header['channel'])
        if key not in frame_chunks:
            frame_chunks[key] = {
                'frame_id': header['frame_id'],
                'channel': header['channel'],
                'frame_size': header['frame_size'],
                'chunks': []
            }
        frame_chunks[key]['chunks'].append(
            (header['frame_offset'], header['payload'])
        )
    # 重组帧
    frames = {}
    for key, info in frame_chunks.items():
        chunks = sorted(info['chunks'], key=lambda x: x[0])
        assembled = bytearray(info['frame_size'])
        for offset, payload in chunks:
            end = min(offset + len(payload), info['frame_size'])
            assembled[offset:end] = payload[:end - offset]
        frames[key] = {
            'frame_id': info['frame_id'],
            'channel': info['channel'],
            'frame_size': info['frame_size'],
            'data': bytes(assembled),
        }
    print(f"\n=== 包统计 / Packet Statistics ===")
    for k, v in stats.items():
        print(f"  {k}: {v}")
    ch3_count = sum(1 for k in frames if k[1] == 3)
    ch4_count = sum(1 for k in frames if k[1] == 4)
    print(f"\n=== 帧统计 / Frame Statistics ===")
    print(f"  通道3帧数 / Channel 3 frames: {ch3_count}")
    print(f"  通道4帧数 / Channel 4 frames: {ch4_count}")
    print(f"  总帧数 / Total frames: {len(frames)}")
    return frames, stats
# ============================================================
# 图像解码 / Image Decoding
# ============================================================
def decode_frame_image(frame_data, max_image_rows=None):
    """
    从帧数据中解码图像
    帧数据结构:
    [0:80]   元数据头
    [80:80+48*H] 图像数据 (H行, 每行48字节)
    [80+48*H:]   跟踪/追踪结果数据
    每行48字节包含:
    [0:24]  左相机子图像 (24像素, 8-bit灰度)
    [24:48] 右相机子图像 (24像素, 8-bit灰度)
    返回: (metadata_dict, left_image_bytes, right_image_bytes, tracking_data, num_rows)
    """
    if len(frame_data) < FRAME_METADATA_SIZE:
        return None, None, None, None, 0
    metadata = parse_frame_metadata(frame_data)
    image_data = frame_data[FRAME_METADATA_SIZE:]
    # 确定图像行数: 扫描数据找到图像->跟踪数据的边界
    # 跟踪数据特征: 大量连续零字节, 小整数值结构
    total_rows = len(image_data) // IMAGE_ROW_WIDTH
    if max_image_rows is not None:
        image_rows = min(total_rows, max_image_rows)
    else:
        # 自动检测: 从末尾向前扫描, 找到图像/跟踪数据边界
        image_rows = total_rows
        for r in range(total_rows - 1, -1, -1):
            row_start = r * IMAGE_ROW_WIDTH
            row = image_data[row_start:row_start + IMAGE_ROW_WIDTH]
            zero_count = sum(1 for b in row if b == 0)
            # 跟踪数据行通常有 >30 个零字节且最大值通常 < 200
            # 图像行通常有较少零字节或较高像素值
            max_val = max(row) if row else 0
            mean_val = sum(row) / len(row) if row else 0
            # 判定为图像行的条件: 均值 > 50 且零字节占比 < 70%
            if mean_val > 50 and zero_count < 34:
                image_rows = r + 1
                break
    # 分离左右子图像
    left_pixels = bytearray()
    right_pixels = bytearray()
    for r in range(image_rows):
        row_start = r * IMAGE_ROW_WIDTH
        row = image_data[row_start:row_start + IMAGE_ROW_WIDTH]
        if len(row) < IMAGE_ROW_WIDTH:
            row = row + bytes(IMAGE_ROW_WIDTH - len(row))
        left_pixels.extend(row[:SUB_IMAGE_WIDTH])
        right_pixels.extend(row[SUB_IMAGE_WIDTH:])
    tracking_data = image_data[image_rows * IMAGE_ROW_WIDTH:]
    return metadata, bytes(left_pixels), bytes(right_pixels), tracking_data, image_rows
def parse_tracking_data(tracking_data):
    """
    解析跟踪结果数据 (尽力解析, 格式可能不完全准确)
    跟踪数据包含 u32 LE 值, 可能代表:
    - 检测到的 IR LED 坐标
    - 置信度/强度值
    - 其他传感器参数
    """
    if len(tracking_data) < 4:
        return None
    values = []
    for i in range(0, len(tracking_data) - 3, 4):
        v = struct.unpack_from('<I', tracking_data, i)[0]
        values.append(v)
    return {
        'raw_bytes': tracking_data.hex(),
        'u32_values': values,
        'length': len(tracking_data),
    }
# ============================================================
# 图像保存 / Image Saving
# ============================================================
def save_grayscale_image(pixel_data, width, height, filepath, scale=4):
    """保存灰度图像, 可选放大"""
    if len(pixel_data) < width * height:
        pixel_data = pixel_data + bytes(width * height - len(pixel_data))
    img = Image.frombytes('L', (width, height), pixel_data[:width * height])
    if scale > 1:
        img = img.resize((width * scale, height * scale), Image.NEAREST)
    img.save(filepath)
    return img
def save_combined_image(left_pixels, right_pixels, width, height, filepath, scale=4, gap=4):
    """保存左右子图像的组合视图"""
    combined_width = width * 2 + gap
    img = Image.new('L', (combined_width, height), 64)
    left_img = Image.frombytes('L', (width, height), left_pixels[:width * height])
    right_img = Image.frombytes('L', (width, height), right_pixels[:width * height])
    img.paste(left_img, (0, 0))
    img.paste(right_img, (width + gap, 0))
    if scale > 1:
        img = img.resize((combined_width * scale, height * scale), Image.NEAREST)
    img.save(filepath)
    return img
# ============================================================
# 主解码流程 / Main Decode Pipeline
# ============================================================
def decode_pcap(pcap_file, output_dir="output", max_frames=None, save_every_n=1):
    """
    完整解码流程
    参数:
        pcap_file: pcapng 文件路径
        output_dir: 输出目录
        max_frames: 最大解码帧数 (None=全部)
        save_every_n: 每隔 N 帧保存一张图 (1=全部保存)
    """
    os.makedirs(output_dir, exist_ok=True)
    os.makedirs(os.path.join(output_dir, "ch3_combined"), exist_ok=True)
    os.makedirs(os.path.join(output_dir, "ch3_left"), exist_ok=True)
    os.makedirs(os.path.join(output_dir, "ch3_right"), exist_ok=True)
    os.makedirs(os.path.join(output_dir, "ch4_combined"), exist_ok=True)
    os.makedirs(os.path.join(output_dir, "ch4_left"), exist_ok=True)
    os.makedirs(os.path.join(output_dir, "ch4_right"), exist_ok=True)
    os.makedirs(os.path.join(output_dir, "full_row"), exist_ok=True)
    # 提取帧
    frames, stats = extract_frames(pcap_file)
    # 按 frame_id 排序
    sorted_keys = sorted(frames.keys())
    # 收集解码结果
    results = {
        'packet_stats': stats,
        'protocol': {
            'transport': 'UDP',
            'camera_ip': '172.17.1.7',
            'camera_port': 3509,
            'pc_ip': '172.17.1.56',
            'pc_port': 50316,
            'transport_header_size': TRANSPORT_HEADER_SIZE,
            'frame_metadata_size': FRAME_METADATA_SIZE,
            'image_row_width': IMAGE_ROW_WIDTH,
            'sub_image_width': SUB_IMAGE_WIDTH,
        },
        'frames': [],
    }
    # 帧大小统计
    ch3_sizes = []
    ch4_sizes = []
    frame_count = 0
    saved_count = 0
    print(f"\n=== 开始解码 / Decoding ===")
    for key in sorted_keys:
        frame = frames[key]
        frame_id = frame['frame_id']
        channel = frame['channel']
        if max_frames is not None and frame_count >= max_frames * 2:
            break
        # 解码图像
        metadata, left_px, right_px, tracking, num_rows = decode_frame_image(frame['data'])
        if metadata is None:
            continue
        ch_name = f"ch{channel}"
        if channel == 3:
            ch3_sizes.append(frame['frame_size'])
        else:
            ch4_sizes.append(frame['frame_size'])
        # 解析跟踪数据
        tracking_info = parse_tracking_data(tracking) if tracking else None
        # 计算亮斑质心 (centroid of bright spots)
        left_centroid = _compute_centroid(left_px, SUB_IMAGE_WIDTH, num_rows)
        right_centroid = _compute_centroid(right_px, SUB_IMAGE_WIDTH, num_rows)
        frame_info = {
            'frame_id': f"0x{frame_id:08X}",
            'channel': channel,
            'frame_size': frame['frame_size'],
            'image_rows': num_rows,
            'image_width_per_eye': SUB_IMAGE_WIDTH,
            'timestamp_sec': metadata['timestamp_sec'],
            'frame_counter': metadata['frame_counter'],
            'left_centroid': left_centroid,
            'right_centroid': right_centroid,
            'tracking_data_size': len(tracking) if tracking else 0,
        }
        results['frames'].append(frame_info)
        # 保存图像
        if frame_count % (save_every_n * 2) < 2:
            idx = saved_count
            saved_count += 1
            if left_px and right_px and num_rows > 0:
                # 保存组合图 (左+右并排)
                save_combined_image(
                    left_px, right_px,
                    SUB_IMAGE_WIDTH, num_rows,
                    os.path.join(output_dir, f"{ch_name}_combined",
                                 f"frame_{idx:04d}_{frame_id:08x}.png"),
                    scale=8, gap=2
                )
                # 保存单独的左/右子图像
                save_grayscale_image(
                    left_px, SUB_IMAGE_WIDTH, num_rows,
                    os.path.join(output_dir, f"{ch_name}_left",
                                 f"frame_{idx:04d}_{frame_id:08x}.png"),
                    scale=8
                )
                save_grayscale_image(
                    right_px, SUB_IMAGE_WIDTH, num_rows,
                    os.path.join(output_dir, f"{ch_name}_right",
                                 f"frame_{idx:04d}_{frame_id:08x}.png"),
                    scale=8
                )
                # 保存完整48像素宽行视图
                full_row_data = frame['data'][FRAME_METADATA_SIZE:
                                              FRAME_METADATA_SIZE + num_rows * IMAGE_ROW_WIDTH]
                save_grayscale_image(
                    full_row_data, IMAGE_ROW_WIDTH, num_rows,
                    os.path.join(output_dir, "full_row",
                                 f"{ch_name}_frame_{idx:04d}_{frame_id:08x}.png"),
                    scale=8
                )
        frame_count += 1
        if frame_count % 200 == 0:
            print(f"  已处理 {frame_count} 帧...")
    # 统计信息
    print(f"\n=== 解码完成 / Decode Complete ===")
    print(f"  总帧数: {frame_count}")
    print(f"  保存图像数: {saved_count}")
    if ch3_sizes:
        print(f"\n  通道3 帧大小范围: {min(ch3_sizes)} - {max(ch3_sizes)} 字节")
    if ch4_sizes:
        print(f"  通道4 帧大小范围: {min(ch4_sizes)} - {max(ch4_sizes)} 字节")
    # 保存解析结果 JSON
    results_path = os.path.join(output_dir, "decode_results.json")
    with open(results_path, 'w', encoding='utf-8') as f:
        json.dump(results, f, indent=2, ensure_ascii=False, default=str)
    print(f"\n  解析结果已保存到: {results_path}")
    return results
def _compute_centroid(pixel_data, width, height, threshold=150):
    """计算亮斑质心坐标"""
    sum_x, sum_y, sum_w = 0.0, 0.0, 0.0
    for r in range(height):
        for c in range(width):
            idx = r * width + c
            if idx < len(pixel_data):
                v = pixel_data[idx]
                if v > threshold:
                    sum_x += c * v
                    sum_y += r * v
                    sum_w += v
    if sum_w > 0:
        return {'x': round(sum_x / sum_w, 2), 'y': round(sum_y / sum_w, 2),
                'brightness': round(sum_w)}
    return None
# ============================================================
# PC -> Camera 命令解析 / PC to Camera Command Parsing
# ============================================================
def decode_pc_commands(pcap_file):
    """解析 PC 发送给相机的命令"""
    packets = rdpcap(pcap_file)
    commands = []
    for pkt in packets:
        if not pkt.haslayer(UDP) or not pkt.haslayer(Raw):
            continue
        if pkt[IP].src != "172.17.1.56":
            continue
        data = bytes(pkt[Raw].load)
        dst = pkt[IP].dst
        if "255" in dst:
            # 广播包 (设备发现)
            commands.append({
                'type': 'broadcast_discovery',
                'dst': dst,
                'data_hex': data.hex(),
                'data_ascii': data.decode('ascii', errors='replace'),
            })
        else:
            # 点对点命令
            commands.append({
                'type': 'camera_command',
                'dst': dst,
                'data_hex': data.hex(),
                'data_len': len(data),
            })
    return commands
# ============================================================
# 入口 / Entry Point
# ============================================================
def main():
    pcap_file = "stylus.pcapng"
    if len(sys.argv) > 1:
        pcap_file = sys.argv[1]
    if not os.path.exists(pcap_file):
        print(f"错误: 文件不存在 / Error: File not found: {pcap_file}")
        sys.exit(1)
    output_dir = "output"
    if len(sys.argv) > 2:
        output_dir = sys.argv[2]
    print("=" * 70)
    print("近红外双目相机 UDP 数据解码器")
    print("NIR Stereo Camera UDP Data Decoder")
    print("=" * 70)
    # 解码 PC 命令
    print("\n--- PC → Camera 命令 / Commands ---")
    commands = decode_pc_commands(pcap_file)
    for cmd in commands:
        print(f"  [{cmd['type']}] dst={cmd.get('dst','?')} data={cmd['data_hex']}")
    # 主解码
    print("\n--- 图像帧解码 / Image Frame Decoding ---")
    results = decode_pcap(pcap_file, output_dir=output_dir, save_every_n=10)
    # 输出样本帧信息
    print("\n--- 样本帧信息 / Sample Frame Info ---")
    for f in results['frames'][:10]:
        lc = f['left_centroid']
        rc = f['right_centroid']
        lc_str = f"({lc['x']:.1f}, {lc['y']:.1f})" if lc else "none"
        rc_str = f"({rc['x']:.1f}, {rc['y']:.1f})" if rc else "none"
        print(f"  Frame {f['frame_id']} ch{f['channel']}: "
              f"{f['image_width_per_eye']}x{f['image_rows']} "
              f"| L_centroid={lc_str} R_centroid={rc_str} "
              f"| tracking={f['tracking_data_size']}B")
    print("\n" + "=" * 70)
    print(f"输出目录 / Output directory: {output_dir}/")
    print("  ch3_combined/  - 通道3左右组合图")
    print("  ch3_left/      - 通道3左相机子图像")
    print("  ch3_right/     - 通道3右相机子图像")
    print("  ch4_combined/  - 通道4左右组合图")
    print("  ch4_left/      - 通道4左相机子图像")
    print("  ch4_right/     - 通道4右相机子图像")
    print("  full_row/      - 完整48像素行视图")
    print("  decode_results.json - 完整解析结果")
    print("=" * 70)
if __name__ == '__main__':
    main()