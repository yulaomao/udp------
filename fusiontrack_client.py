#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fusionTrack 相机 Python 客户端
================================

基于 pcapng 抓包逆向分析和 SDK 逆向工程，实现从零连接 fusionTrack 相机的 Python 客户端。

功能:
  1. 设备发现 — 广播发现局域网中的 fusionTrack 设备
  2. 设备连接 — UDP 连接到设备，完成初始化序列
  3. 校准读取 — 从设备板载存储读取双目标定参数
  4. 图像采集 — 接收压缩图像流并解压缩
  5. 图像保存 — 保存左右目红外追踪图像

协议来源:
  - fusionTrack SDK x64/output/full_01.pcapng — 177条命令完整时序
  - analysis_pc_to_camera_messages.md — 协议格式定义
  - decode_compressed_images.py — V3 8-bit RLE 图像解压缩
  - reverse_engineered_src/device/UdpTransport.h — 通讯层架构

通讯协议:
  PC→Camera 命令格式 (24字节, 小端序):
    [2] command_type: 0x0001=READ, 0x0002=WRITE, 0x0005=FRAME_REQ, 0x0007=DISCONNECT
    [2] command_words: 包总大小/2
    [4] sequence: 全局递增序列号
    [4] reserved: 0
    [4] param1: 寄存器地址/帧令牌
    [4] param2: 数据大小/值
    [4] param3: 计数/标志

  Camera→PC 响应格式:
    0x1001 (READ响应):  [2]tag [2]words [4]cam_seq [4]pc_seq [N]data
    0x1002 (WRITE ACK): [2]tag [2]words=6 [4]cam_seq [4]pc_seq
    0x1003 (左图像):    [2]tag [2]words [4]seq [4]reserved [4]token [4]size [4]offset [N]data
    0x1004 (右图像):    同上
    0x1006 (状态):      [2]tag [2]words [4]seq [4]pc_seq [N]data
    0x1901 (特殊ACK):   [2]tag [2]words [4]cam_seq [4]pc_seq

用法:
    # 连接相机并采集图像
    python fusiontrack_client.py --camera-ip 172.17.1.7

    # 使用指定输出目录
    python fusiontrack_client.py --camera-ip 172.17.1.7 --output-dir captured_images

    # 调试模式
    python fusiontrack_client.py --camera-ip 172.17.1.7 --debug

    # 离线模式: 从已有 pcapng 提取图像 (不连接相机)
    python fusiontrack_client.py --offline "fusionTrack SDK x64/output/full_01.pcapng"

依赖: Pillow (图像保存), scapy (仅离线模式)
"""

from __future__ import annotations

import argparse
import logging
import os
import select
import socket
import struct
import sys
import threading
import time
from collections import defaultdict
from pathlib import Path
from typing import Optional

try:
    from PIL import Image, ImageOps
except ImportError:
    Image = None
    ImageOps = None

# ═══════════════════════════════════════════════════════════════════════════════
# 常量定义 — 来自 pcapng 逆向分析
# ═══════════════════════════════════════════════════════════════════════════════

# 网络参数
CAMERA_PORT = 3509                  # fusionTrack 默认通讯端口
DISCOVERY_PORT = 63630              # 设备发现广播端口
DISCOVERY_MSG = b"0:3\0"            # 发现广播内容 (协议版本标识)
DEFAULT_PACKET_MAX_SIZE = 1452      # 标准以太网 MTU - IP/UDP 头
JUMBO_PACKET_SIZE = 8972            # Jumbo frame 大小 (9000 - IP/UDP 头)

# 命令类型
CMD_READ_REGISTER = 0x0001          # 读取设备寄存器/选项
CMD_WRITE_REGISTER = 0x0002         # 写入设备寄存器/选项
CMD_FRAME_REQUEST = 0x0005          # 帧流控 ACK (16字节)
CMD_DISCONNECT = 0x0007             # 断开连接 (12字节)

# 响应流标识
RESP_READ_DATA = 0x1001             # 读取响应 (含数据)
RESP_WRITE_ACK = 0x1002             # 写入确认
RESP_LEFT_IMAGE = 0x1003            # 左相机图像流
RESP_RIGHT_IMAGE = 0x1004           # 右相机图像流
RESP_STATUS = 0x1006                # 状态/事件通知
RESP_SPECIAL_ACK = 0x1901           # 特殊确认

# 设备寄存器 ID (从 pcapng 分析和 analysis_pc_to_camera_messages.md)
REG_SN_LOW = 0                      # 序列号低 32 位 (RO)
REG_SN_HIGH = 1                     # 序列号高 32 位 (RO)
REG_ONCHIP_ADDR = 2                 # 芯片地址信息 (RO)
REG_CHUNK_GET = 3                   # 请求配置数据块 (WO)
REG_CHUNK_RDY = 4                   # 配置块就绪状态 (RO)
REG_PACKET_MAX_SIZE = 5             # 最大数据包大小 (RW)
REG_US_EXP_TARGET = 9               # 曝光目标 μs (RW)
REG_US_ALT_EXP_TARGET = 10          # 备用曝光目标 (RW)
REG_US_STR_TARGET = 14              # 频闪目标 μs (RW)
REG_US_STR = 15                     # 频闪周期 (RO)
REG_US_TRI_START = 16               # 三角测量开始 (RW)
REG_US_TRI_TARGET = 17              # 三角测量目标 (RW)
REG_STROBE_MODE = 21                # 频闪模式 (RW)
REG_ACQUISITION = 46                # 采集控制 (WO) **关键**
REG_CAM0_THRESHOLD = 47             # 左相机检测阈值 (RW)
REG_CAM1_THRESHOLD = 48             # 右相机检测阈值 (RW)
REG_ACC_RANGE = 54                  # 加速度计量程 (RO)
REG_ELEC_VERSION = 62               # 电子版本 (RO)
REG_DEVICE_VERSION = 63             # 设备版本 (RO)
REG_DEVICE_ACCESS = 64              # 访问权限 (RO)
REG_UART_TSTP_DATA_RATE = 74        # UART 时间戳数据率 (RW)
REG_EIO_ENABLE = 75                 # EIO 使能 (RW)
REG_EXP_TIMESTAMP_ENABLE = 76       # 曝光时间戳使能 (RW)
REG_CONT_TIMESTAMP_DELAY = 77       # 连续时间戳延迟 (RW)
REG_EIO_1_RX_TX_CONTROL = 78        # EIO 端口1控制 (RW)
REG_EIO_2_RX_TX_CONTROL = 79        # EIO 端口2控制 (RW)
REG_ETHERNET_SPEED = 82             # 以太网速度 (RW)
REG_ACC_STATUS = 83                 # 加速度计状态 (RO)
REG_PICTURE_SZ_CUTOFF = 84          # 图像大小截止 (RW)
REG_S_FREQ_PCK_SHOCK = 86           # 震动检测频率 (RW)
REG_USER_LED_RED = 89               # LED 红 (RW)
REG_USER_LED_GREEN = 90             # LED 绿 (RW)
REG_USER_LED_BLUE = 91              # LED 蓝 (RW)
REG_USER_LED_FREQUENCY = 92         # LED 频率 (RW)
REG_USER_LED_ON = 93                # LED 开关 (RW)
REG_LASER0_ENABLE = 109             # 激光器0 (RW)
REG_LASER1_ENABLE = 110             # 激光器1 (RW)
REG_PAYLOAD_DATA_MASK = 117         # 数据载荷掩码 (RW)
REG_LS_WM_SELECTED_API = 121        # 无线标记 API (RW)
REG_LS_WM_SESSION_ID = 122          # 会话 ID (RW)
REG_LS_WM_AUTODISC_US = 123         # 自动发现间隔 (RW)
REG_LS_WM_NVRAM_RDY = 127           # NVRAM 就绪 (RO)
REG_LS_WM_AUTOSTATUS_US = 130       # 自动状态间隔 (RW)
REG_LS_WM_FIRE_LEDS = 134           # LED 触发 (WO)
REG_LS_WM_MASK = 135                # 标记掩码 (RW)
REG_LS_WM_IR_LEDS_MASK = 136        # IR LED 掩码 (RW)
REG_PLAIN_NUMBER_LOW = 144          # 认证明文低位 (RW)
REG_BUZZER_PERIOD = 148             # 蜂鸣器周期 (RW)
REG_BUZZER_DURATION = 149           # 蜂鸣器持续 (RW)
REG_PTP_CONTROL = 151               # PTP 控制 (RW)
REG_PTP_DOM_NBR = 152               # PTP 域号 (RW)
REG_PTP_ANNOUNCE_TIMINGS = 153      # PTP 公告时序 (RW)
REG_PTP_SYNC_TIMINGS = 154          # PTP 同步时序 (RW)

# 校准数据内存地址 (从 pcapng seq 6-10)
CALIB_MEM_HEADER = 0x3C0D0          # 校准数据头
CALIB_MEM_START = 0x3C0E0           # 校准数据起始
CALIB_CHUNK_SIZE = 1440             # 每段读取大小

# 图像参数
IMAGE_WIDTH = 2048                  # 传感器宽度
IMAGE_HEIGHT = 1088                 # 传感器高度
INNER_HEADER_BYTES = 80             # 内层帧头部大小
ROI_START_OFFSET = 65               # ROI 起始行在帧头中的偏移

# 超时和重试
CMD_TIMEOUT_MS = 2000               # 命令超时 (ms)
CMD_RETRIES = 3                     # 命令重试次数
HEARTBEAT_INTERVAL_MS = 400         # 心跳间隔 (ms)
DISCOVERY_TIMEOUT_S = 5.0           # 发现超时 (s)

logger = logging.getLogger("fusiontrack")


# ═══════════════════════════════════════════════════════════════════════════════
# 命令构建器 — 构造 PC→Camera UDP 数据包
# ═══════════════════════════════════════════════════════════════════════════════

def build_read_cmd(seq: int, reg_addr: int, data_size: int = 4, count: int = 0) -> bytes:
    """
    构造 READ_REGISTER 命令 (24字节).

    格式: [type=0x0001][words=12][seq][reserved=0][addr][size][count]
    """
    return struct.pack("<HHIIIII",
                       CMD_READ_REGISTER,       # command_type
                       12,                       # command_words (24/2)
                       seq,                      # sequence
                       0,                        # reserved
                       reg_addr,                 # param1: register address
                       data_size,                # param2: data size in bytes
                       count)                    # param3: count/flags


def build_write_cmd(seq: int, reg_addr: int, value: int) -> bytes:
    """
    构造 WRITE_REGISTER 命令 (24字节).

    格式: [type=0x0002][words=12][seq][reserved=0][addr][0][value]
    """
    return struct.pack("<HHIIIII",
                       CMD_WRITE_REGISTER,       # command_type
                       12,                        # command_words (24/2)
                       seq,                       # sequence
                       0,                         # reserved
                       reg_addr,                  # param1: register address
                       0,                         # param2: always 0 for write
                       value)                     # param3: value to write


def build_frame_request(seq: int, token: int) -> bytes:
    """
    构造 FRAME_REQUEST 命令 (16字节).

    格式: [type=0x0005][words=8][seq][reserved=0][token]
    """
    return struct.pack("<HHIII",
                       CMD_FRAME_REQUEST,        # command_type
                       8,                         # command_words (16/2)
                       seq,                       # sequence
                       0,                         # reserved
                       token)                     # param1: frame token


def build_disconnect(seq: int) -> bytes:
    """
    构造 DISCONNECT 命令 (12字节).

    格式: [type=0x0007][words=6][seq][reserved=0]
    """
    return struct.pack("<HHII",
                       CMD_DISCONNECT,           # command_type
                       6,                         # command_words (12/2)
                       seq,                       # sequence
                       0)                         # reserved


# ═══════════════════════════════════════════════════════════════════════════════
# 响应解析器 — 解析 Camera→PC 数据包
# ═══════════════════════════════════════════════════════════════════════════════

def parse_response(data: bytes) -> Optional[dict]:
    """解析相机响应数据包."""
    if len(data) < 12:
        return None

    tag = struct.unpack_from("<H", data, 0)[0]
    words = struct.unpack_from("<H", data, 2)[0]

    # 检查 words 是否与包大小匹配
    expected_size = words * 2
    if expected_size != len(data):
        # 可能是截断的包，仍然尝试解析
        pass

    if tag == RESP_READ_DATA:
        # READ 响应: tag, words, cam_seq, pc_seq, data...
        if len(data) < 12:
            return None
        cam_seq = struct.unpack_from("<I", data, 4)[0]
        pc_seq = struct.unpack_from("<I", data, 8)[0]
        payload = data[12:]
        return {"type": "read", "tag": tag, "cam_seq": cam_seq,
                "pc_seq": pc_seq, "data": payload}

    elif tag == RESP_WRITE_ACK:
        # WRITE ACK: tag, words=6, cam_seq, pc_seq
        if len(data) < 12:
            return None
        cam_seq = struct.unpack_from("<I", data, 4)[0]
        pc_seq = struct.unpack_from("<I", data, 8)[0]
        return {"type": "write_ack", "tag": tag, "cam_seq": cam_seq,
                "pc_seq": pc_seq}

    elif tag in (RESP_LEFT_IMAGE, RESP_RIGHT_IMAGE):
        # 图像流: tag, words, pkt_seq, reserved, token, frame_size, offset, data...
        if len(data) < 24:
            return None
        pkt_seq = struct.unpack_from("<I", data, 4)[0]
        reserved = struct.unpack_from("<I", data, 8)[0]
        frame_token = struct.unpack_from("<I", data, 12)[0]
        frame_size = struct.unpack_from("<I", data, 16)[0]
        payload_offset = struct.unpack_from("<I", data, 20)[0]
        payload = data[24:]
        side = "left" if tag == RESP_LEFT_IMAGE else "right"
        return {"type": "image", "tag": tag, "side": side,
                "pkt_seq": pkt_seq, "token": frame_token,
                "frame_size": frame_size, "offset": payload_offset,
                "payload": payload}

    elif tag == RESP_STATUS:
        # 状态/事件
        cam_seq = struct.unpack_from("<I", data, 4)[0] if len(data) >= 8 else 0
        return {"type": "status", "tag": tag, "cam_seq": cam_seq,
                "data": data[8:]}

    elif tag == RESP_SPECIAL_ACK:
        # 特殊确认 (0x1901)
        cam_seq = struct.unpack_from("<I", data, 4)[0] if len(data) >= 8 else 0
        pc_seq = struct.unpack_from("<I", data, 8)[0] if len(data) >= 12 else 0
        return {"type": "special_ack", "tag": tag, "cam_seq": cam_seq,
                "pc_seq": pc_seq}

    else:
        return {"type": "unknown", "tag": tag, "data": data}


# ═══════════════════════════════════════════════════════════════════════════════
# V3 8-bit 图像解压缩 (从 DLL 逆向还原)
# ═══════════════════════════════════════════════════════════════════════════════

def decompress_v3_8bit(body: bytes, width: int = IMAGE_WIDTH) -> list[dict[int, int]]:
    """
    解压缩 V3 8-bit 压缩图像数据.

    还原自 fusionTrack64.dll (RVA 0x001f1cd0).

    算法:
      对每个字节 b:
        b == 0x00: 填充字节, 忽略
        0x01 <= b <= 0x7F: 跳过 b 个暗像素
        b == 0x80: 跳过 128 个暗像素
        0x81 <= b <= 0xFF: 像素值 = (b - 0x80) * 2
      当 x >= width: 行结束, 跳过填充到 16 字节边界
      16 字节边界处检测跳过记录: [0x00][skip_lo][skip_hi][13×0x00]
    """
    all_rows: list[dict[int, int]] = []
    current_row: dict[int, int] = {}
    x = 0
    i = 0

    while i < len(body):
        # 在 16 字节边界检测行间跳过记录
        if i % 16 == 0 and i + 16 <= len(body):
            block = body[i: i + 16]
            if block[0] == 0x00 and all(b == 0 for b in block[3:16]):
                skip_count = block[1] | (block[2] << 8)
                if current_row:
                    all_rows.append(current_row)
                    current_row = {}
                    x = 0
                for _ in range(skip_count):
                    all_rows.append({})
                i += 16
                continue

        b = body[i]
        i += 1

        if b == 0x00:
            continue
        elif b == 0x80:
            x += 128
        elif b < 0x80:
            x += b
        else:
            pixel = (b - 0x80) * 2
            current_row[x] = pixel
            x += 1

        if x >= width:
            all_rows.append(current_row)
            current_row = {}
            x = 0
            while i < len(body) and i % 16 != 0:
                i += 1

    if current_row:
        all_rows.append(current_row)

    return all_rows


def parse_inner_header(data: bytes) -> dict:
    """解析 80 字节内层帧头."""
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


def rows_to_image(
    rows: list[dict[int, int]],
    width: int = IMAGE_WIDTH,
    height: int = IMAGE_HEIGHT,
    roi_start_row: int = 0,
) -> Optional["Image.Image"]:
    """将稀疏行数据转换为 PIL 图像."""
    if Image is None:
        logger.warning("Pillow 未安装, 无法生成图像")
        return None
    if not rows:
        return None

    img = Image.new("L", (width, height), 0)
    pixels = img.load()

    for row_idx, row in enumerate(rows):
        y = roi_start_row + row_idx
        if y >= height:
            break
        for x_pos, val in row.items():
            if 0 <= x_pos < width:
                pixels[x_pos, y] = min(255, val)

    return img


# ═══════════════════════════════════════════════════════════════════════════════
# 帧重组器 — 从 UDP 分片重组完整图像帧
# ═══════════════════════════════════════════════════════════════════════════════

class FrameAssembler:
    """
    图像帧重组器.

    fusionTrack 相机将一帧图像分割为多个 UDP 包发送:
    - 每个包有 24 字节 vendor header (tag, seq, token, frame_size, offset)
    - 根据 offset 将各片段放入正确位置
    - 当所有片段收齐 (总字节数 == frame_size) 时帧完成

    左右图以相同的 frame_token 关联为一帧对.
    """

    def __init__(self, max_pending: int = 10):
        self._pending: dict[tuple[str, int], dict] = {}  # (side, token) -> info
        self._max_pending = max_pending
        self._completed: list[dict] = []
        self._lock = threading.Lock()
        self._total_left = 0
        self._total_right = 0

    def add_fragment(self, parsed: dict) -> Optional[dict]:
        """
        添加图像片段.

        返回: 完成的帧数据 dict 或 None
        """
        side = parsed["side"]
        token = parsed["token"]
        frame_size = parsed["frame_size"]
        offset = parsed["offset"]
        payload = parsed["payload"]

        key = (side, token)

        with self._lock:
            if key not in self._pending:
                self._pending[key] = {
                    "token": token,
                    "side": side,
                    "frame_size": frame_size,
                    "buffer": bytearray(frame_size),
                    "received": 0,
                    "timestamp": time.time(),
                }

            info = self._pending[key]

            # 写入片段
            end = offset + len(payload)
            if end <= frame_size:
                info["buffer"][offset:end] = payload
                info["received"] += len(payload)

            # 检查是否完成
            if info["received"] >= info["frame_size"]:
                completed = {
                    "token": token,
                    "side": side,
                    "data": bytes(info["buffer"]),
                    "size": info["frame_size"],
                }
                del self._pending[key]
                if side == "left":
                    self._total_left += 1
                else:
                    self._total_right += 1
                return completed

            # 清理过旧的 pending 帧
            if len(self._pending) > self._max_pending * 2:
                now = time.time()
                stale = [k for k, v in self._pending.items()
                         if now - v["timestamp"] > 5.0]
                for k in stale:
                    del self._pending[k]

        return None

    @property
    def stats(self) -> str:
        return (f"完成帧: L={self._total_left} R={self._total_right}, "
                f"等待中: {len(self._pending)}")


# ═══════════════════════════════════════════════════════════════════════════════
# fusionTrack 客户端
# ═══════════════════════════════════════════════════════════════════════════════

class FusionTrackClient:
    """
    fusionTrack 相机客户端.

    实现完整的设备连接、初始化和图像采集流程.
    """

    def __init__(self, camera_ip: str = "172.17.1.7",
                 camera_port: int = CAMERA_PORT,
                 local_ip: str = "",
                 packet_size: int = DEFAULT_PACKET_MAX_SIZE,
                 debug: bool = False):
        self.camera_ip = camera_ip
        self.camera_port = camera_port
        self.local_ip = local_ip
        self.packet_size = packet_size
        self.debug = debug

        self._sock: Optional[socket.socket] = None
        self._seq = 0                       # 全局序列号计数器
        self._serial_number: int = 0        # 设备序列号
        self._device_version: int = 0       # 设备版本
        self._elec_version: int = 0         # 电子版本
        self._connected = False
        self._acquiring = False

        # 心跳
        self._heartbeat_thread: Optional[threading.Thread] = None
        self._heartbeat_stop = threading.Event()
        self._last_frame_token: int = 0

        # 图像接收
        self._assembler = FrameAssembler()
        self._receiver_thread: Optional[threading.Thread] = None
        self._receiver_stop = threading.Event()
        self._frame_callback = None

        # 响应缓冲
        self._response_lock = threading.Lock()
        self._pending_responses: dict[int, dict] = {}  # pc_seq -> response
        self._response_event = threading.Event()

        # 校准数据
        self.calibration_chunks: dict[int, bytes] = {}

        # 统计
        self._cmd_count = 0
        self._img_pkt_count = 0

    # ─── 序列号管理 ──────────────────────────────────────────────────────

    def _next_seq(self) -> int:
        self._seq += 1
        return self._seq

    # ─── Socket 管理 ─────────────────────────────────────────────────────

    def _create_socket(self) -> socket.socket:
        """创建 UDP socket."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # 设置大接收缓冲区 (图像数据量大)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 256 * 1024)
        # 绑定到本地地址
        if self.local_ip:
            sock.bind((self.local_ip, 0))
        else:
            sock.bind(("", 0))
        sock.settimeout(CMD_TIMEOUT_MS / 1000.0)
        logger.info(f"Socket 绑定到 {sock.getsockname()}")
        return sock

    # ─── 低级通讯 ────────────────────────────────────────────────────────

    def _send(self, data: bytes):
        """发送数据到相机."""
        if self._sock is None:
            raise ConnectionError("Socket 未初始化")
        self._sock.sendto(data, (self.camera_ip, self.camera_port))
        self._cmd_count += 1
        if self.debug:
            logger.debug(f"TX [{len(data)}B]: {data.hex()}")

    def _recv(self, timeout: float = 2.0) -> Optional[bytes]:
        """接收一个数据包."""
        if self._sock is None:
            return None
        try:
            self._sock.settimeout(timeout)
            data, addr = self._sock.recvfrom(65535)
            if self.debug:
                tag = struct.unpack_from("<H", data, 0)[0] if len(data) >= 2 else 0
                logger.debug(f"RX [{len(data)}B] tag=0x{tag:04x} from {addr}")
            return data
        except socket.timeout:
            return None

    def _send_and_wait(self, cmd: bytes, expected_seq: int,
                       timeout: float = 2.0, retries: int = CMD_RETRIES) -> Optional[dict]:
        """
        发送命令并等待对应序列号的响应.

        从接收缓冲中过滤出匹配 pc_seq 的响应,
        图像数据包被路由到帧重组器.
        """
        for attempt in range(retries):
            self._send(cmd)
            deadline = time.time() + timeout

            while time.time() < deadline:
                data = self._recv(timeout=min(0.5, deadline - time.time()))
                if data is None:
                    continue

                parsed = parse_response(data)
                if parsed is None:
                    continue

                # 图像数据 → 路由到帧重组器
                if parsed["type"] == "image":
                    self._img_pkt_count += 1
                    self._last_frame_token = parsed["token"]
                    completed = self._assembler.add_fragment(parsed)
                    if completed and self._frame_callback:
                        self._frame_callback(completed)
                    continue

                # 状态/事件 → 记录
                if parsed["type"] in ("status", "unknown"):
                    logger.debug(f"状态包: tag=0x{parsed['tag']:04x}")
                    continue

                # 特殊 ACK → 记录
                if parsed["type"] == "special_ack":
                    logger.debug(f"特殊ACK: pc_seq={parsed.get('pc_seq', '?')}")
                    # 如果匹配我们等待的 seq，视为成功
                    if parsed.get("pc_seq") == expected_seq:
                        return parsed
                    continue

                # READ/WRITE 响应 → 检查序列号
                resp_seq = parsed.get("pc_seq", -1)
                if resp_seq == expected_seq:
                    return parsed

            logger.warning(f"命令 seq={expected_seq} 超时 (尝试 {attempt + 1}/{retries})")

        logger.error(f"命令 seq={expected_seq} 重试 {retries} 次后失败")
        return None

    # ─── 高级命令接口 ────────────────────────────────────────────────────

    def read_register(self, reg_addr: int, data_size: int = 4,
                      count: int = 0) -> Optional[bytes]:
        """
        读取设备寄存器.

        返回: 寄存器数据字节, 或 None
        """
        seq = self._next_seq()
        cmd = build_read_cmd(seq, reg_addr, data_size, count)
        resp = self._send_and_wait(cmd, seq)
        if resp and resp["type"] == "read":
            return resp["data"]
        return None

    def read_register_u32(self, reg_addr: int) -> Optional[int]:
        """读取 32 位无符号寄存器值."""
        data = self.read_register(reg_addr, 4, 0)
        if data and len(data) >= 4:
            return struct.unpack_from("<I", data, 0)[0]
        return None

    def write_register(self, reg_addr: int, value: int) -> bool:
        """
        写入设备寄存器.

        返回: 是否成功
        """
        seq = self._next_seq()
        cmd = build_write_cmd(seq, reg_addr, value)
        resp = self._send_and_wait(cmd, seq)
        return resp is not None

    def send_frame_request(self, token: int) -> bool:
        """发送帧流控 ACK."""
        seq = self._next_seq()
        cmd = build_frame_request(seq, token)
        # FRAME_REQUEST 不需要等待响应
        self._send(cmd)
        return True

    # ─── 设备发现 ────────────────────────────────────────────────────────

    @staticmethod
    def discover(timeout: float = DISCOVERY_TIMEOUT_S,
                 local_ip: str = "") -> list[str]:
        """
        通过广播发现局域网中的 fusionTrack 设备.

        发送 "0:3\\0" 到端口 63630, 等待设备响应.

        返回: 发现的设备 IP 列表
        """
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.settimeout(1.0)

        if local_ip:
            sock.bind((local_ip, 0))
        else:
            sock.bind(("", 0))

        devices = set()
        deadline = time.time() + timeout

        while time.time() < deadline:
            # 发送子网广播和全局广播
            try:
                sock.sendto(DISCOVERY_MSG, ("255.255.255.255", DISCOVERY_PORT))
                # 如果知道子网，也发送子网广播
                if local_ip:
                    parts = local_ip.split(".")
                    subnet_broadcast = f"{parts[0]}.{parts[1]}.{parts[2]}.255"
                    sock.sendto(DISCOVERY_MSG, (subnet_broadcast, DISCOVERY_PORT))
            except OSError as e:
                logger.warning(f"广播发送失败: {e}")

            # 接收响应
            try:
                data, addr = sock.recvfrom(4096)
                if data and addr[0] not in devices:
                    devices.add(addr[0])
                    logger.info(f"发现设备: {addr[0]}:{addr[1]} 响应={data!r}")
            except socket.timeout:
                pass

        sock.close()
        return list(devices)

    # ─── 连接与初始化 ────────────────────────────────────────────────────

    def connect(self) -> bool:
        """
        连接到相机并执行完整初始化序列.

        按照 pcapng 中捕获的命令时序执行:
          阶段 1: 读取设备序列号和芯片地址
          阶段 2: 请求并读取校准数据块 1-3
          阶段 3: 设置 PACKET_MAX_SIZE
          阶段 4: 读取设备版本
          阶段 5: 启动采集
          阶段 6: 配置各项参数
        """
        logger.info(f"连接到 {self.camera_ip}:{self.camera_port} ...")
        self._sock = self._create_socket()

        # ── 阶段 1: 读取设备序列号 ──
        logger.info("阶段 1: 读取设备序列号...")
        sn_low = self.read_register_u32(REG_SN_LOW)
        if sn_low is None:
            logger.error("无法读取设备序列号低位")
            return False

        sn_high = self.read_register_u32(REG_SN_HIGH)
        if sn_high is None:
            logger.error("无法读取设备序列号高位")
            return False

        self._serial_number = (sn_high << 32) | sn_low
        logger.info(f"设备序列号: {self._serial_number} "
                     f"(0x{self._serial_number:016X})")

        # 读取芯片地址
        onchip = self.read_register_u32(REG_ONCHIP_ADDR)
        logger.info(f"芯片地址: 0x{onchip:08X}" if onchip else "芯片地址读取失败")

        # ── 阶段 2: 读取校准数据 (3个chunk) ──
        logger.info("阶段 2: 读取校准数据...")
        for chunk_id in range(1, 4):
            calib_data = self._read_calibration_chunk(chunk_id)
            if calib_data:
                self.calibration_chunks[chunk_id] = calib_data
                logger.info(f"  校准块 {chunk_id}: {len(calib_data)} 字节")
            else:
                logger.warning(f"  校准块 {chunk_id}: 读取失败")

        # ── 阶段 3: 处理初始帧 (发送 FRAME_REQUEST ACK) ──
        logger.info("阶段 3: 处理初始帧请求...")
        # 发送几个 FRAME_REQUEST 来 ACK 设备可能已在发送的帧
        if self._last_frame_token > 0:
            for i in range(5):
                token = self._last_frame_token - i * 1024
                if token > 0:
                    self.send_frame_request(token)

        # ── 阶段 4: 设置 PACKET_MAX_SIZE ──
        logger.info(f"阶段 4: 设置 PACKET_MAX_SIZE = {self.packet_size}...")
        if not self.write_register(REG_PACKET_MAX_SIZE, self.packet_size):
            logger.warning("PACKET_MAX_SIZE 设置失败")

        # ── 阶段 5: 读取设备版本 ──
        logger.info("阶段 5: 读取设备版本...")
        self._device_version = self.read_register_u32(REG_DEVICE_VERSION) or 0
        self._elec_version = self.read_register_u32(REG_ELEC_VERSION) or 0
        logger.info(f"  设备版本: {self._device_version}, "
                     f"电子版本: {self._elec_version}")

        # ── 阶段 6: 枚举选项和配置 ──
        logger.info("阶段 6: 配置设备参数...")
        self._configure_device()

        self._connected = True
        logger.info("✓ 设备连接和初始化完成!")
        return True

    def _read_calibration_chunk(self, chunk_id: int) -> Optional[bytes]:
        """
        读取一个校准数据块.

        流程: WRITE CHUNK_GET=chunk_id → READ CHUNK_RDY → READ memory blocks
        """
        # 请求校准块
        if not self.write_register(REG_CHUNK_GET, chunk_id):
            return None

        # 等待就绪
        rdy = self.read_register_u32(REG_CHUNK_RDY)
        if rdy is None:
            return None

        # 读取头信息 (16字节)
        header = self.read_register(CALIB_MEM_HEADER, 16, 1)
        if header is None or len(header) < 12:
            return None

        # 从头信息中提取总大小 (从 pcapng 推断头格式)
        # header 格式: [4]chunk_id [4]total_size [4]checksum ...
        # 但实际可能不是这样，让我们从 pcapng 推断
        # Chunk 1: header(16) + 1440*3 + 1348 = 5684 bytes data
        # Chunk 2: header(16) + 1440*4 + 440 = 6216 bytes data
        # Chunk 3: header(16) + 124 bytes data
        #
        # 根据 header 中的信息确定读取大小
        # header bytes 4-7 可能是 total_size, bytes 8-11 可能是另一个 size
        total_size_a = struct.unpack_from("<I", header, 4)[0] if len(header) >= 8 else 0
        total_size_b = struct.unpack_from("<I", header, 8)[0] if len(header) >= 12 else 0

        # 从 pcapng 我们知道每个 chunk 读取的确切大小
        # 我们使用与原始 SDK 相同的读取模式
        chunk_sizes = {
            1: [1440, 1440, 1440, 1348],       # seq 7-10: 总共 5668 字节
            2: [1440, 1440, 1440, 1440, 440],   # seq 22-26: 总共 6200 字节
            3: [124],                             # seq 30: 总共 124 字节
        }

        if chunk_id not in chunk_sizes:
            return None

        # 按段读取数据
        all_data = bytearray(header)  # 包含头
        addr = CALIB_MEM_START

        for block_size in chunk_sizes[chunk_id]:
            data = self.read_register(addr, block_size, 1)
            if data is None:
                logger.warning(f"校准块 {chunk_id} 读取失败 @ 0x{addr:X}")
                return bytes(all_data)  # 返回已读取的部分
            all_data.extend(data)
            addr += block_size

        return bytes(all_data)

    def _configure_device(self):
        """
        执行设备配置 — 对应 pcapng 中的阶段七至阶段十.

        这些是可选配置，即使部分失败也不影响基本图像采集.
        """
        # 读取当前选项值
        self.read_register_u32(REG_CAM0_THRESHOLD)     # 左相机阈值
        self.read_register_u32(REG_CAM1_THRESHOLD)     # 右相机阈值
        self.read_register_u32(REG_ACC_RANGE)           # 加速度计量程

        # ★ 关键: 启动采集 (WRITE ACQUISITION = 1)
        logger.info("  → 启动采集模式 (ACQUISITION = 1)...")
        if not self.write_register(REG_ACQUISITION, 1):
            logger.error("  ✗ 启动采集失败!")
            return

        # 读取更多选项
        self.read_register_u32(REG_DEVICE_ACCESS)
        self.read_register_u32(REG_BUZZER_PERIOD)
        self.read_register_u32(REG_PLAIN_NUMBER_LOW)
        self.read_register_u32(REG_US_ALT_EXP_TARGET)
        self.read_register_u32(REG_STROBE_MODE)
        self.read_register_u32(REG_LS_WM_NVRAM_RDY)
        self.read_register_u32(REG_ACC_STATUS)

        # 设置以太网速度
        self.write_register(REG_ETHERNET_SPEED, 1)

        # PTP 配置
        self.read_register_u32(REG_PTP_CONTROL)
        self.write_register(REG_PTP_CONTROL, 0)         # 关闭 PTP

        # 数据掩码
        self.read_register_u32(REG_PAYLOAD_DATA_MASK)
        self.write_register(REG_PAYLOAD_DATA_MASK, 7)    # 基本数据掩码

        # EIO 配置
        self.write_register(REG_EIO_ENABLE, 1)
        self.write_register(REG_EXP_TIMESTAMP_ENABLE, 1)
        self.write_register(REG_PAYLOAD_DATA_MASK, 0x107)  # 扩展掩码

        # 图像参数
        self.write_register(REG_CAM0_THRESHOLD, 200)     # 左相机阈值 = 200
        self.write_register(REG_CAM1_THRESHOLD, 200)     # 右相机阈值 = 200
        self.write_register(REG_US_EXP_TARGET, 130)      # 曝光 = 130μs
        self.write_register(REG_US_STR_TARGET, 130)      # 频闪 = 130μs
        self.write_register(REG_STROBE_MODE, 0)          # 正常模式
        self.write_register(REG_US_ALT_EXP_TARGET, 130)  # 备用曝光

        # 高级参数
        self.write_register(REG_PICTURE_SZ_CUTOFF, 2228288)  # 图像大小截止
        self.write_register(REG_S_FREQ_PCK_SHOCK, 0)
        self.write_register(REG_UART_TSTP_DATA_RATE, 25)
        self.write_register(REG_US_TRI_START, 0)
        self.write_register(REG_US_TRI_TARGET, 0)
        self.write_register(REG_CONT_TIMESTAMP_DELAY, 0)
        self.write_register(REG_LASER0_ENABLE, 0)        # 关闭激光
        self.write_register(REG_LASER1_ENABLE, 0)

        # 蜂鸣器
        self.write_register(REG_BUZZER_PERIOD, 488)
        self.write_register(REG_BUZZER_DURATION, 5)

        # LED 控制
        self.write_register(REG_USER_LED_RED, 0)
        self.write_register(REG_USER_LED_GREEN, 0)
        self.write_register(REG_USER_LED_BLUE, 0)
        self.write_register(REG_USER_LED_FREQUENCY, 0)
        self.write_register(REG_USER_LED_ON, 0)

        logger.info("  ✓ 设备配置完成")

    # ─── 图像采集 ────────────────────────────────────────────────────────

    def start_acquisition(self, callback=None):
        """
        启动图像接收和心跳.

        callback: 每完成一帧时调用, 参数为 dict:
            {"token": int, "side": "left"|"right", "data": bytes, "size": int}
        """
        if not self._connected:
            raise RuntimeError("设备未连接")

        self._frame_callback = callback

        # 启动接收线程
        self._receiver_stop.clear()
        self._receiver_thread = threading.Thread(
            target=self._receiver_loop, daemon=True, name="receiver")
        self._receiver_thread.start()

        # 启动心跳线程
        self._heartbeat_stop.clear()
        self._heartbeat_thread = threading.Thread(
            target=self._heartbeat_loop, daemon=True, name="heartbeat")
        self._heartbeat_thread.start()

        self._acquiring = True
        logger.info("✓ 图像采集已启动")

    def stop_acquisition(self):
        """停止图像接收和心跳."""
        self._acquiring = False
        self._receiver_stop.set()
        self._heartbeat_stop.set()

        if self._receiver_thread:
            self._receiver_thread.join(timeout=3.0)
        if self._heartbeat_thread:
            self._heartbeat_thread.join(timeout=3.0)

        logger.info("图像采集已停止")

    def _receiver_loop(self):
        """后台接收线程 — 接收所有 UDP 数据包."""
        logger.debug("接收线程启动")
        while not self._receiver_stop.is_set():
            try:
                if self._sock is None:
                    break

                # 使用 select 实现超时
                ready = select.select([self._sock], [], [], 0.1)
                if not ready[0]:
                    continue

                data, addr = self._sock.recvfrom(65535)
                if not data:
                    continue

                parsed = parse_response(data)
                if parsed is None:
                    continue

                if parsed["type"] == "image":
                    self._img_pkt_count += 1
                    self._last_frame_token = parsed["token"]
                    completed = self._assembler.add_fragment(parsed)
                    if completed and self._frame_callback:
                        try:
                            self._frame_callback(completed)
                        except Exception as e:
                            logger.error(f"帧回调错误: {e}")

                elif parsed["type"] in ("read", "write_ack", "special_ack"):
                    # 命令响应 — 存入缓冲供 _send_and_wait 使用
                    pc_seq = parsed.get("pc_seq", 0)
                    with self._response_lock:
                        self._pending_responses[pc_seq] = parsed

            except OSError:
                if not self._receiver_stop.is_set():
                    logger.warning("接收线程 socket 错误")
                break
            except Exception as e:
                logger.error(f"接收线程异常: {e}")

        logger.debug("接收线程退出")

    def _heartbeat_loop(self):
        """
        心跳线程 — 定期发送 FRAME_REQUEST 保持连接.

        fusionTrack 设备有 WATCHDOG_TIMEOUT, 如果一段时间没收到
        SDK 的心跳 (FRAME_REQUEST), 设备会停止发送数据.
        """
        logger.debug("心跳线程启动")
        while not self._heartbeat_stop.wait(HEARTBEAT_INTERVAL_MS / 1000.0):
            if self._last_frame_token > 0:
                try:
                    seq = self._next_seq()
                    cmd = build_frame_request(seq, self._last_frame_token)
                    self._send(cmd)
                except Exception as e:
                    logger.warning(f"心跳发送失败: {e}")

        logger.debug("心跳线程退出")

    # ─── 断开连接 ────────────────────────────────────────────────────────

    def disconnect(self):
        """断开与相机的连接."""
        if self._acquiring:
            self.stop_acquisition()

        if self._sock and self._connected:
            try:
                seq = self._next_seq()
                cmd = build_disconnect(seq)
                self._send(cmd)
                logger.info("已发送 DISCONNECT 命令")
            except Exception as e:
                logger.warning(f"发送 DISCONNECT 失败: {e}")

        if self._sock:
            self._sock.close()
            self._sock = None

        self._connected = False
        logger.info("已断开连接")

    # ─── 状态信息 ────────────────────────────────────────────────────────

    @property
    def serial_number(self) -> int:
        return self._serial_number

    @property
    def is_connected(self) -> bool:
        return self._connected

    @property
    def is_acquiring(self) -> bool:
        return self._acquiring

    def get_stats(self) -> str:
        return (f"序列号: {self._serial_number}, "
                f"命令数: {self._cmd_count}, "
                f"图像包数: {self._img_pkt_count}, "
                f"{self._assembler.stats}")


# ═══════════════════════════════════════════════════════════════════════════════
# 离线模式 — 从 pcapng 提取图像
# ═══════════════════════════════════════════════════════════════════════════════

def offline_extract(pcap_path: str, output_dir: str = "captured_images",
                    max_frames: int = 5):
    """
    从 pcapng 文件提取并解压缩图像.

    这是 decode_compressed_images.py 的集成版本.
    """
    try:
        from scapy.all import IP, Raw, UDP, PcapNgReader
    except ImportError:
        print("离线模式需要安装 scapy: pip install scapy")
        return

    if Image is None:
        print("需要安装 Pillow: pip install Pillow")
        return

    pcap = Path(pcap_path)
    if not pcap.exists():
        print(f"文件不存在: {pcap}")
        return

    outdir = Path(output_dir)
    outdir.mkdir(parents=True, exist_ok=True)

    print(f"从 {pcap} 提取图像...")
    print(f"输出到 {outdir}/")

    # 重组帧
    fragment_map: dict[tuple[int, int], dict] = defaultdict(
        lambda: {"chunks": [], "frame_size": 0}
    )

    with PcapNgReader(str(pcap)) as reader:
        for packet in reader:
            if UDP not in packet or Raw not in packet:
                continue
            if IP not in packet:
                continue

            payload = bytes(packet[Raw].load)
            if len(payload) < 24:
                continue

            stream_tag = struct.unpack_from("<H", payload, 0)[0]
            if stream_tag not in (RESP_LEFT_IMAGE, RESP_RIGHT_IMAGE):
                continue

            pkt_words = struct.unpack_from("<H", payload, 2)[0]
            if pkt_words * 2 != len(payload):
                continue

            frame_token = struct.unpack_from("<I", payload, 12)[0]
            frame_size = struct.unpack_from("<I", payload, 16)[0]
            payload_offset = struct.unpack_from("<I", payload, 20)[0]

            if payload_offset > frame_size:
                continue

            key = (stream_tag, frame_token)
            fragment_map[key]["frame_size"] = frame_size
            fragment_map[key]["chunks"].append((payload_offset, payload[24:]))

    # 重组并解压缩
    complete_frames: dict[int, list] = defaultdict(list)
    for (tag, token), info in sorted(fragment_map.items()):
        chunks = sorted(info["chunks"], key=lambda c: c[0])
        frame_size = info["frame_size"]
        buf = bytearray(frame_size)
        expected = 0
        ok = True
        for offset, chunk in chunks:
            if offset != expected:
                ok = False
            end = offset + len(chunk)
            if end > frame_size:
                ok = False
                break
            buf[offset:end] = chunk
            expected = end
        if expected != frame_size:
            ok = False
        if ok:
            complete_frames[tag].append({"token": token, "data": bytes(buf)})

    stream_names = {RESP_LEFT_IMAGE: "left", RESP_RIGHT_IMAGE: "right"}
    saved = 0

    for tag, name in stream_names.items():
        if tag not in complete_frames:
            print(f"未找到 {name} 相机数据流")
            continue

        frames = sorted(complete_frames[tag], key=lambda f: f["token"])
        count = min(max_frames, len(frames))
        print(f"\n{name.upper()} 相机: {len(frames)} 帧, 解码 {count}")

        for idx, frame in enumerate(frames[:count]):
            data = frame["data"]
            header = parse_inner_header(data)
            body = data[INNER_HEADER_BYTES:]
            roi_start = header.get("roi_start_row", 0)

            rows = decompress_v3_8bit(body)
            total_px = sum(len(r) for r in rows)
            active = sum(1 for r in rows if r)

            img = rows_to_image(rows, roi_start_row=roi_start)
            if img is None:
                print(f"  帧 {idx}: 无有效像素")
                continue

            # 保存完整图像
            full_path = outdir / f"{name}_frame_{idx:03d}_full.png"
            img.save(full_path)

            # 保存裁剪增强图像
            bbox = img.getbbox()
            if bbox:
                margin = 10
                crop_box = (
                    max(0, bbox[0] - margin),
                    max(0, bbox[1] - margin),
                    min(IMAGE_WIDTH, bbox[2] + margin),
                    min(IMAGE_HEIGHT, bbox[3] + margin),
                )
                cropped = img.crop(crop_box)
                enhanced = ImageOps.autocontrast(cropped)
                crop_path = outdir / f"{name}_frame_{idx:03d}.png"
                enhanced.save(crop_path)

            saved += 1
            print(f"  帧 {idx}: token={frame['token']:>8d} | "
                  f"ROI row {roi_start}+{len(rows)} ({active} 有效) | "
                  f"{total_px} 像素 → {full_path.name}")

    print(f"\n✓ 共保存 {saved} 张图像到 {outdir}/")


# ═══════════════════════════════════════════════════════════════════════════════
# 实时采集模式 — 连接相机采集图像
# ═══════════════════════════════════════════════════════════════════════════════

def live_acquisition(camera_ip: str, output_dir: str = "captured_images",
                     max_frames: int = 10, packet_size: int = DEFAULT_PACKET_MAX_SIZE,
                     local_ip: str = "", debug: bool = False):
    """
    连接相机进行实时图像采集.
    """
    if Image is None:
        print("需要安装 Pillow: pip install Pillow")
        return

    outdir = Path(output_dir)
    outdir.mkdir(parents=True, exist_ok=True)

    # 计数器
    frame_counts = {"left": 0, "right": 0}
    done_event = threading.Event()

    def on_frame(frame: dict):
        """帧完成回调."""
        side = frame["side"]
        data = frame["data"]
        token = frame["token"]

        if frame_counts[side] >= max_frames:
            if all(c >= max_frames for c in frame_counts.values()):
                done_event.set()
            return

        # 解压缩图像
        header = parse_inner_header(data)
        body = data[INNER_HEADER_BYTES:]
        roi_start = header.get("roi_start_row", 0)

        rows = decompress_v3_8bit(body)
        total_px = sum(len(r) for r in rows)
        active = sum(1 for r in rows if r)

        if total_px == 0:
            return

        img = rows_to_image(rows, roi_start_row=roi_start)
        if img is None:
            return

        idx = frame_counts[side]
        frame_counts[side] += 1

        # 保存完整图像
        full_path = outdir / f"{side}_frame_{idx:03d}_full.png"
        img.save(full_path)

        # 保存裁剪增强图像
        bbox = img.getbbox()
        if bbox:
            margin = 10
            crop_box = (
                max(0, bbox[0] - margin),
                max(0, bbox[1] - margin),
                min(IMAGE_WIDTH, bbox[2] + margin),
                min(IMAGE_HEIGHT, bbox[3] + margin),
            )
            cropped = img.crop(crop_box)
            enhanced = ImageOps.autocontrast(cropped)
            crop_path = outdir / f"{side}_frame_{idx:03d}.png"
            enhanced.save(crop_path)

        print(f"  [{side:5s}] 帧 {idx}: token={token:>8d} | "
              f"ROI row {roi_start}+{len(rows)} ({active} 有效) | "
              f"{total_px} 像素 → {full_path.name}")

        if all(c >= max_frames for c in frame_counts.values()):
            done_event.set()

    # 创建客户端
    client = FusionTrackClient(
        camera_ip=camera_ip,
        packet_size=packet_size,
        local_ip=local_ip,
        debug=debug,
    )

    try:
        # 连接
        if not client.connect():
            print("连接失败!")
            return

        print(f"\n设备信息: {client.get_stats()}")
        print(f"\n开始采集图像 (每个相机最多 {max_frames} 帧)...\n")

        # 启动采集
        client.start_acquisition(callback=on_frame)

        # 等待采集完成或超时
        done_event.wait(timeout=60)

        if done_event.is_set():
            print(f"\n✓ 采集完成!")
        else:
            print(f"\n⚠ 采集超时 (60秒)")

        print(f"统计: {client.get_stats()}")

    except KeyboardInterrupt:
        print("\n用户中断")
    finally:
        client.disconnect()

    total = sum(frame_counts.values())
    print(f"\n✓ 共保存 {total} 张图像到 {outdir}/")


# ═══════════════════════════════════════════════════════════════════════════════
# 协议验证模式 — 用 pcapng 数据验证命令构建
# ═══════════════════════════════════════════════════════════════════════════════

def verify_protocol():
    """
    验证命令构建器生成的数据与 pcapng 中实际捕获的命令一致.
    """
    print("=" * 60)
    print("协议验证 — 对比构建命令与 pcapng 实际数据")
    print("=" * 60)

    # 从 pcapng 提取的实际命令 (hex)
    expected_commands = {
        # seq: (description, expected_hex)
        1:  ("READ SN_LOW",
             "01000c000100000000000000000000000400000000000000"),
        2:  ("READ SN_HIGH",
             "01000c000200000000000000010000000400000000000000"),
        3:  ("READ ONCHIP_ADDR",
             "01000c000300000000000000020000000400000000000000"),
        4:  ("WRITE CHUNK_GET=1",
             "02000c000400000000000000030000000000000001000000"),
        5:  ("READ CHUNK_RDY",
             "01000c000500000000000000040000000400000000000000"),
        6:  ("READ CALIB_HEADER",
             "01000c000600000000000000d0c003001000000001000000"),
        7:  ("READ CALIB_DATA_1440",
             "01000c000700000000000000e0c00300a005000001000000"),
        11: ("FRAME_REQUEST token=0x1FC4",
             "050008000b00000000000000c41f0000"),
        16: ("WRITE PACKET_MAX_SIZE=1452",
             "02000c0010000000000000000500000000000000ac050000"),
        34: ("WRITE ACQUISITION=1",
             "02000c0022000000000000002e0000000000000001000000"),
    }

    # 生成并对比
    all_pass = True
    for seq, (desc, expected_hex) in sorted(expected_commands.items()):
        expected = bytes.fromhex(expected_hex)

        # 构建命令
        if seq == 1:
            built = build_read_cmd(1, REG_SN_LOW, 4, 0)
        elif seq == 2:
            built = build_read_cmd(2, REG_SN_HIGH, 4, 0)
        elif seq == 3:
            built = build_read_cmd(3, REG_ONCHIP_ADDR, 4, 0)
        elif seq == 4:
            built = build_write_cmd(4, REG_CHUNK_GET, 1)
        elif seq == 5:
            built = build_read_cmd(5, REG_CHUNK_RDY, 4, 0)
        elif seq == 6:
            built = build_read_cmd(6, CALIB_MEM_HEADER, 16, 1)
        elif seq == 7:
            built = build_read_cmd(7, CALIB_MEM_START, 1440, 1)
        elif seq == 11:
            built = build_frame_request(11, 0x1FC4)
        elif seq == 16:
            built = build_write_cmd(16, REG_PACKET_MAX_SIZE, 1452)
        elif seq == 34:
            built = build_write_cmd(34, REG_ACQUISITION, 1)
        else:
            continue

        match = built == expected
        status = "✓" if match else "✗"
        all_pass = all_pass and match

        print(f"  {status} seq={seq:3d}: {desc}")
        if not match:
            print(f"    期望: {expected.hex()}")
            print(f"    生成: {built.hex()}")

    print()
    if all_pass:
        print("✓ 所有命令验证通过! 协议实现正确.")
    else:
        print("✗ 部分命令验证失败!")

    return all_pass


# ═══════════════════════════════════════════════════════════════════════════════
# 主入口
# ═══════════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description="fusionTrack 相机 Python 客户端",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 验证协议实现
  python fusiontrack_client.py --verify

  # 从 pcapng 提取图像 (离线, 不需要相机)
  python fusiontrack_client.py --offline "fusionTrack SDK x64/output/full_01.pcapng"

  # 发现局域网中的设备
  python fusiontrack_client.py --discover

  # 连接相机采集图像
  python fusiontrack_client.py --camera-ip 172.17.1.7

  # 使用 Jumbo frame
  python fusiontrack_client.py --camera-ip 172.17.1.7 --jumbo
""")

    # 模式选择
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--verify", action="store_true",
                      help="验证协议实现 (不连接相机)")
    mode.add_argument("--offline", type=str, metavar="PCAPNG",
                      help="离线模式: 从 pcapng 提取图像")
    mode.add_argument("--discover", action="store_true",
                      help="发现局域网中的 fusionTrack 设备")
    mode.add_argument("--camera-ip", type=str,
                      help="相机 IP 地址 (实时采集模式)")

    # 参数
    parser.add_argument("--output-dir", type=str, default="captured_images",
                        help="图像输出目录 (默认: captured_images)")
    parser.add_argument("--max-frames", type=int, default=10,
                        help="每个相机最多采集帧数 (默认: 10)")
    parser.add_argument("--local-ip", type=str, default="",
                        help="本地 IP 地址 (默认: 自动)")
    parser.add_argument("--jumbo", action="store_true",
                        help="使用 Jumbo frame (9000 MTU)")
    parser.add_argument("--debug", action="store_true",
                        help="启用调试输出")

    args = parser.parse_args()

    # 配置日志
    level = logging.DEBUG if args.debug else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
        datefmt="%H:%M:%S",
    )

    packet_size = JUMBO_PACKET_SIZE if args.jumbo else DEFAULT_PACKET_MAX_SIZE

    # 执行
    if args.verify:
        verify_protocol()

    elif args.offline:
        offline_extract(args.offline, args.output_dir, args.max_frames)

    elif args.discover:
        print("发现 fusionTrack 设备...")
        devices = FusionTrackClient.discover(local_ip=args.local_ip)
        if devices:
            print(f"发现 {len(devices)} 个设备:")
            for ip in devices:
                print(f"  - {ip}")
        else:
            print("未发现设备. 请确认:")
            print("  1. 相机已开机并连接到同一网络")
            print("  2. 防火墙允许 UDP 端口 63630")
            print("  3. PC 与相机在同一子网 (通常 172.17.x.x)")

    elif args.camera_ip:
        live_acquisition(
            camera_ip=args.camera_ip,
            output_dir=args.output_dir,
            max_frames=args.max_frames,
            packet_size=packet_size,
            local_ip=args.local_ip,
            debug=args.debug,
        )

    else:
        parser.print_help()
        print("\n提示: 使用 --verify 验证协议, --offline 离线提取, "
              "或 --camera-ip 实时采集")


if __name__ == "__main__":
    main()
