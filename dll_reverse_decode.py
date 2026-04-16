#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fusionTrack DLL 逆向工程辅助解码器
====================================

通过直接读取 fusionTrack64.dll 和 device64.dll 的二进制数据，
提取协议常量、数据结构布局和标定参数解析逻辑，
然后用这些信息精确解码 pcapng 抓包文件中的所有数据。

DLL 逆向工程发现的关键信息：
- 源码项目结构: soft.atr.meta.2cams.sdk
  - soft.atr.framework: 底层框架 (UDP通信, 线程, Factory模式)
  - soft.atr.2cams.ftk: 设备驱动 (devPacketReader, devImageProcessor, devUdpTransport)
  - soft.atr.2cams.sdk: SDK接口 (fusionTrack.cpp, ftkDevice.cpp, StereoProviderV3.cpp)
  - soft.atr.framework.math: 数学库 (Tensor, Matrix, ColVector, SVD)
- C++类层次 (RTTI): StereoProviderV0-V3, StereoInterpolatorV1,
  FeatureHandlingV1, SegmenterV21, GeometryReaderJsonV2
- 标定模型版本: StereoProviderV3 (最新), 支持温度补偿插值

数据结构 (来自 SDK 头文件 + DLL 逆向):

  ftkCameraParameters (40 bytes, packed):
    float FocalLength[2]     # 焦距 [fx, fy] (像素)
    float OpticalCentre[2]   # 光学中心 [cx, cy] (像素)
    float Distorsions[5]     # 畸变系数 [k1, k2, p1, p2, k3]
    float Skew               # 倾斜参数 (u/v轴夹角)

  ftkStereoParameters (104 bytes, packed):
    ftkCameraParameters LeftCamera   # 左目相机参数 (40 bytes)
    ftkCameraParameters RightCamera  # 右目相机参数 (40 bytes)
    float Translation[3]             # 右目在左目坐标系中的平移 [tx, ty, tz] (mm)
    float Rotation[3]                # 右目在左目坐标系中的旋转 [rx, ry, rz] (Rodrigues)

  ftkARCalibrationParameters (32 bytes, packed):
    double principalPointX    # 主点X (像素)
    double principalPointY    # 主点Y (像素)
    double focalLenghtX       # 焦距X (像素)
    double focalLenghtY       # 焦距Y (像素)

协议层次结构 (来自 devPacketReader.cpp/devUdpTransport.cpp 逆向):

  Layer 0 - PC 命令 (24 bytes):
    0x0001 = READ_REGISTER   读取设备寄存器/内存
    0x0002 = WRITE_REGISTER  写入设备寄存器
    0x0005 = HEARTBEAT       心跳/看门狗喂狗
    0x0007 = IMAGE_REQUEST   请求图像数据

  Layer 1 - 设备分片协议 (24 bytes header):
    stream_tag + packet_words + sequence + reserved + frame_token + frame_size + payload_offset

  数据流类型:
    0x1001 = 寄存器/配置响应流 (register read responses)
    0x1003 = 左目 ROI 稀疏图像数据
    0x1004 = 右目 ROI 稀疏图像数据
    0x1006 = 标定文件数据流

  Layer 2 - 内层帧 (80 bytes header):
    magic/timestamp + device_timestamp + frame_counter + sensor_flags + config fields

  Layer 3 - ROI 编码 (16 bytes records):
    稀疏行段压缩的红外反光点图像

标定数据传输流程 (来自 devFusionTrack.cpp / CalibrationGetter.cpp 逆向):
  1. SDK 通过 READ_REGISTER 读取 ONCHIP_ADDR 寄存器获取内存地址表
  2. SDK 通过连续 READ_REGISTER 命令从设备内存下载标定文件
  3. 标定文件格式: JSON (GeometryReaderJsonV2) 或 INI (GeometryReaderIniV0/V1) 或 BIN (GeometryReaderBinV1)
  4. 标定文件包含 StereoProvider 数据 (相机内外参、畸变模型、温度补偿表)
  5. ftkExtractFrameInfo(CalibrationParameters) 返回 ftkStereoParameters 结构

用法：
    python dll_reverse_decode.py [pcapng_file] [--dll-path PATH] [--extract-calib] [--json output.json]
"""

import struct
import json
import argparse
import sys
import os
from collections import defaultdict
from dataclasses import dataclass, asdict, field
from typing import Optional, List, Dict, Any, Tuple

import numpy as np

# =============================================================================
# Part 1: DLL 二进制读取器 - 从 DLL 中提取协议常量和数据结构
# =============================================================================

class DLLBinaryReader:
    """直接读取 DLL 文件提取逆向工程信息"""

    def __init__(self, dll_path: str):
        self.dll_path = dll_path
        self.data = b''
        self.sections = {}
        self.exports = {}
        self.strings_cache = None

        if os.path.exists(dll_path):
            with open(dll_path, 'rb') as f:
                self.data = f.read()
            self._parse_pe_sections()
            self._parse_exports()

    def _parse_pe_sections(self):
        """解析 PE 文件头和节区"""
        if len(self.data) < 64:
            return
        # DOS header -> PE offset
        if self.data[:2] != b'MZ':
            return
        pe_offset = struct.unpack_from('<I', self.data, 0x3C)[0]
        if pe_offset + 24 > len(self.data):
            return
        # PE signature
        if self.data[pe_offset:pe_offset + 4] != b'PE\x00\x00':
            return
        # COFF header
        num_sections = struct.unpack_from('<H', self.data, pe_offset + 6)[0]
        opt_header_size = struct.unpack_from('<H', self.data, pe_offset + 20)[0]
        sections_offset = pe_offset + 24 + opt_header_size

        # Optional header - get ImageBase
        magic = struct.unpack_from('<H', self.data, pe_offset + 24)[0]
        if magic == 0x20B:  # PE32+
            self.image_base = struct.unpack_from('<Q', self.data, pe_offset + 24 + 24)[0]
        else:
            self.image_base = struct.unpack_from('<I', self.data, pe_offset + 24 + 28)[0]

        # Parse sections
        for i in range(num_sections):
            off = sections_offset + i * 40
            name = self.data[off:off + 8].decode('utf-8', errors='replace').strip('\x00')
            virtual_size = struct.unpack_from('<I', self.data, off + 8)[0]
            virtual_addr = struct.unpack_from('<I', self.data, off + 12)[0]
            raw_size = struct.unpack_from('<I', self.data, off + 16)[0]
            raw_offset = struct.unpack_from('<I', self.data, off + 20)[0]
            self.sections[name] = {
                'virtual_addr': virtual_addr,
                'virtual_size': virtual_size,
                'raw_offset': raw_offset,
                'raw_size': raw_size,
            }

    def _parse_exports(self):
        """解析导出表"""
        try:
            import pefile
            pe = pefile.PE(self.dll_path)
            if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
                for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
                    if exp.name:
                        self.exports[exp.name.decode('utf-8', errors='replace')] = exp.address
            pe.close()
        except (ImportError, Exception):
            pass

    def extract_strings(self, min_length=6) -> List[Tuple[int, str]]:
        """从 .rdata 和 .data 节区提取字符串"""
        if self.strings_cache is not None:
            return self.strings_cache

        strings = []
        for sec_name in ('.rdata', '.data'):
            if sec_name not in self.sections:
                continue
            sec = self.sections[sec_name]
            data = self.data[sec['raw_offset']:sec['raw_offset'] + sec['raw_size']]
            current = b""
            start_offset = 0
            for i, b in enumerate(data):
                if 0x20 <= b < 0x7f:
                    if not current:
                        start_offset = i
                    current += bytes([b])
                else:
                    if len(current) >= min_length:
                        try:
                            s = current.decode('ascii')
                            rva = sec['virtual_addr'] + start_offset
                            strings.append((rva, s))
                        except UnicodeDecodeError:
                            pass
                    current = b""

        self.strings_cache = strings
        return strings

    def extract_rtti_classes(self) -> List[str]:
        """从 RTTI 信息提取C++类名"""
        classes = []
        for sec_name in ('.rdata', '.data'):
            if sec_name not in self.sections:
                continue
            sec = self.sections[sec_name]
            data = self.data[sec['raw_offset']:sec['raw_offset'] + sec['raw_size']]
            for prefix in (b'.?AV', b'.?AU'):
                pos = 0
                while pos < len(data):
                    idx = data.find(prefix, pos)
                    if idx == -1:
                        break
                    end = data.find(b'@@', idx)
                    if end != -1 and end - idx < 256:
                        name = data[idx:end + 2].decode('ascii', errors='replace')
                        classes.append(name)
                    pos = idx + 4
        return sorted(set(classes))

    def find_string_references(self, search_term: str) -> List[Tuple[int, str]]:
        """搜索包含特定文本的字符串"""
        all_strings = self.extract_strings(min_length=4)
        return [(rva, s) for rva, s in all_strings
                if search_term.lower() in s.lower()]

    def get_source_files(self) -> List[str]:
        """提取源文件路径"""
        all_strings = self.extract_strings(min_length=10)
        paths = set()
        for _, s in all_strings:
            if ('\\workspace\\' in s or '/workspace/' in s) and (
                s.endswith('.cpp') or s.endswith('.hpp') or s.endswith('.tpp') or s.endswith('.h')
            ):
                paths.add(s)
        return sorted(paths)


# =============================================================================
# Part 2: 协议常量 (从 DLL 逆向确认)
# =============================================================================

# 设备网络配置
CAMERA_IP = '172.17.1.7'
CAMERA_PORT = 3509

# PC -> Camera 命令类型 (从 devPacketReader.cpp 逆向)
CMD_READ_REGISTER = 0x0001      # 读取寄存器/内存
CMD_WRITE_REGISTER = 0x0002     # 写入寄存器
CMD_HEARTBEAT = 0x0005          # 心跳/看门狗
CMD_IMAGE_REQUEST = 0x0007      # 请求图像数据

# Camera -> PC 数据流标识 (从 devGenRtStream.cpp 逆向)
STREAM_CONFIG = 0x1001          # 配置/寄存器响应
STREAM_CALIB = 0x1002           # 标定数据 (JSON/INI/BIN)
STREAM_LEFT = 0x1003            # 左目 ROI 数据
STREAM_RIGHT = 0x1004           # 右目 ROI 数据
STREAM_FACTORY = 0x1006         # 工厂标定文件

# 传感器标志 (从内层帧头 byte[20])
SENSOR_LEFT = 0x00
SENSOR_RIGHT = 0xC0

# 协议尺寸常量 (从 devPacketReader.tpp 逆向)
VENDOR_HEADER_SIZE = 24         # UDP 分片头大小 (同 VendorHeader.STRUCT_SIZE)
INNER_HEADER_SIZE = 80          # 内层帧头大小
ROI_RECORD_SIZE = 16            # ROI 记录大小
PC_COMMAND_SIZE = 24            # PC 命令大小


# =============================================================================
# Part 3: 数据结构定义 (从 ftkInterface.h + DLL RTTI 确认)
# =============================================================================

@dataclass
class CameraParameters:
    """
    ftkCameraParameters (40 bytes packed)
    标定参考: Caltech Camera Calibration Toolbox (Bouguet模型)
    """
    focal_length: Tuple[float, float] = (0.0, 0.0)     # [fx, fy] 像素
    optical_centre: Tuple[float, float] = (0.0, 0.0)    # [cx, cy] 像素
    distortions: Tuple[float, ...] = (0.0, 0.0, 0.0, 0.0, 0.0)  # [k1,k2,p1,p2,k3]
    skew: float = 0.0                                    # 倾斜参数

    STRUCT_SIZE = 40
    STRUCT_FMT = '<2f2f5ff'  # 10 floats = 40 bytes

    @classmethod
    def from_bytes(cls, data: bytes, offset: int = 0) -> 'CameraParameters':
        values = struct.unpack_from(cls.STRUCT_FMT, data, offset)
        return cls(
            focal_length=(values[0], values[1]),
            optical_centre=(values[2], values[3]),
            distortions=tuple(values[4:9]),
            skew=values[9],
        )

    def to_opencv_format(self) -> Dict[str, Any]:
        """转换为 OpenCV 格式的相机矩阵和畸变系数"""
        fx, fy = self.focal_length
        cx, cy = self.optical_centre
        camera_matrix = np.array([
            [fx, self.skew, cx],
            [0.0, fy, cy],
            [0.0, 0.0, 1.0]
        ])
        dist_coeffs = np.array(self.distortions)
        return {
            'camera_matrix': camera_matrix.tolist(),
            'dist_coeffs': dist_coeffs.tolist(),
            'fx': fx, 'fy': fy,
            'cx': cx, 'cy': cy,
            'k1': self.distortions[0],
            'k2': self.distortions[1],
            'p1': self.distortions[2],
            'p2': self.distortions[3],
            'k3': self.distortions[4],
            'skew': self.skew,
        }


@dataclass
class StereoParameters:
    """
    ftkStereoParameters (104 bytes packed)
    双目相机系统标定参数
    """
    left_camera: CameraParameters = field(default_factory=CameraParameters)
    right_camera: CameraParameters = field(default_factory=CameraParameters)
    translation: Tuple[float, float, float] = (0.0, 0.0, 0.0)  # [tx,ty,tz] mm
    rotation: Tuple[float, float, float] = (0.0, 0.0, 0.0)     # Rodrigues [rx,ry,rz]

    STRUCT_SIZE = 104

    @classmethod
    def from_bytes(cls, data: bytes, offset: int = 0) -> 'StereoParameters':
        left = CameraParameters.from_bytes(data, offset)
        right = CameraParameters.from_bytes(data, offset + CameraParameters.STRUCT_SIZE)
        base = offset + 2 * CameraParameters.STRUCT_SIZE
        tx, ty, tz, rx, ry, rz = struct.unpack_from('<6f', data, base)
        return cls(
            left_camera=left,
            right_camera=right,
            translation=(tx, ty, tz),
            rotation=(rx, ry, rz),
        )

    def to_dict(self) -> Dict[str, Any]:
        """导出为完整字典"""
        return {
            'left_camera': self.left_camera.to_opencv_format(),
            'right_camera': self.right_camera.to_opencv_format(),
            'translation_mm': list(self.translation),
            'rotation_rodrigues': list(self.rotation),
            'baseline_mm': np.linalg.norm(self.translation).item(),
        }


@dataclass
class ARCalibrationParameters:
    """
    ftkARCalibrationParameters (32 bytes packed)
    增强现实标定参数 (高精度 double)
    """
    principal_point_x: float = 0.0
    principal_point_y: float = 0.0
    focal_length_x: float = 0.0
    focal_length_y: float = 0.0

    STRUCT_SIZE = 32
    STRUCT_FMT = '<4d'  # 4 doubles = 32 bytes

    @classmethod
    def from_bytes(cls, data: bytes, offset: int = 0) -> 'ARCalibrationParameters':
        ppx, ppy, flx, fly = struct.unpack_from(cls.STRUCT_FMT, data, offset)
        return cls(
            principal_point_x=ppx,
            principal_point_y=ppy,
            focal_length_x=flx,
            focal_length_y=fly,
        )


@dataclass
class VendorHeader:
    """24 字节 UDP 分片头 (从 devPacketReader.tpp 逆向确认)"""
    stream_tag: int = 0
    packet_words: int = 0
    packet_sequence: int = 0
    reserved: int = 0
    frame_token: int = 0
    frame_size: int = 0
    payload_offset: int = 0

    STRUCT_FMT = '<HHIIIII'
    STRUCT_SIZE = 24

    @classmethod
    def from_bytes(cls, data: bytes, offset: int = 0) -> Optional['VendorHeader']:
        if len(data) - offset < cls.STRUCT_SIZE:
            return None
        vals = struct.unpack_from(cls.STRUCT_FMT, data, offset)
        hdr = cls(*vals)
        if hdr.packet_words * 2 != len(data) - offset:
            return None
        if hdr.payload_offset > hdr.frame_size:
            return None
        return hdr


@dataclass
class PCCommand:
    """PC -> Camera 命令 (从 devPacketReader.cpp 逆向)"""
    command_type: int = 0
    command_words: int = 0
    sequence: int = 0
    reserved: int = 0
    param1: int = 0
    param2: int = 0
    param3: int = 0
    raw: bytes = b''

    KNOWN_COMMANDS = {
        0x0001: 'READ_REGISTER',
        0x0002: 'WRITE_REGISTER',
        0x0005: 'HEARTBEAT',
        0x0007: 'IMAGE_REQUEST',
    }

    @classmethod
    def from_bytes(cls, data: bytes) -> Optional['PCCommand']:
        if len(data) < 12:
            return None
        cmd_type, cmd_words, seq, reserved = struct.unpack_from('<HHII', data, 0)
        p1 = p2 = p3 = 0
        if len(data) >= 24:
            p1, p2, p3 = struct.unpack_from('<III', data, 12)
        return cls(cmd_type, cmd_words, seq, reserved, p1, p2, p3, data)

    @property
    def name(self) -> str:
        return self.KNOWN_COMMANDS.get(self.command_type, f'UNKNOWN_0x{self.command_type:04x}')

    def describe(self) -> str:
        if self.command_type == CMD_READ_REGISTER:
            return (f"READ_REG addr=0x{self.param1:08x} "
                    f"size={self.param2} count={self.param3}")
        elif self.command_type == CMD_WRITE_REGISTER:
            return (f"WRITE_REG addr=0x{self.param1:08x} "
                    f"value=0x{self.param2:08x} count={self.param3}")
        elif self.command_type == CMD_HEARTBEAT:
            return f"HEARTBEAT watchdog={self.param1}"
        elif self.command_type == CMD_IMAGE_REQUEST:
            return "IMAGE_REQUEST"
        else:
            return f"{self.name} raw={self.raw.hex()}"


@dataclass
class InnerFrameHeader:
    """
    80 字节内层帧头 (从 devImageProcessor.cpp 逆向)

    这个结构体是设备端填充的，包含图像采集时的传感器状态信息。
    """
    capture_timestamp: int = 0      # 偏移 0: 采集时间戳/magic (Unix时间)
    padding_4: int = 0              # 偏移 4: 填充
    device_timestamp_us: int = 0    # 偏移 8: 设备时间 (微秒)
    frame_counter: int = 0          # 偏移 16: 帧计数器
    sensor_flags: int = 0           # 偏移 20: 传感器标志 (0=左, 0xC0=右)
    reserved_21_27: bytes = b''     # 偏移 21-27: 保留
    field_28: int = 0               # 偏移 28: 配置字段
    field_32: int = 0               # 偏移 32: 配置字段
    field_36: int = 0               # 偏移 36: 配置字段
    field_40: int = 0               # 偏移 40: ROI尺寸相关
    field_44: int = 0               # 偏移 44: 配置字段
    reserved_48_55: bytes = b''     # 偏移 48-55: 保留
    field_56: int = 0               # 偏移 56: 配置字段
    reserved_60: int = 0            # 偏移 60: 保留
    field_64: int = 0               # 偏移 64: 配置字段
    reserved_68_79: bytes = b''     # 偏移 68-79: 保留

    @classmethod
    def from_bytes(cls, data: bytes, offset: int = 0) -> Optional['InnerFrameHeader']:
        if len(data) - offset < INNER_HEADER_SIZE:
            return None
        d = data[offset:]
        return cls(
            capture_timestamp=struct.unpack_from('<I', d, 0)[0],
            padding_4=struct.unpack_from('<I', d, 4)[0],
            device_timestamp_us=struct.unpack_from('<Q', d, 8)[0],
            frame_counter=struct.unpack_from('<I', d, 16)[0],
            sensor_flags=d[20],
            reserved_21_27=d[21:28],
            field_28=struct.unpack_from('<I', d, 28)[0],
            field_32=struct.unpack_from('<I', d, 32)[0],
            field_36=struct.unpack_from('<I', d, 36)[0],
            field_40=struct.unpack_from('<I', d, 40)[0],
            field_44=struct.unpack_from('<I', d, 44)[0],
            reserved_48_55=d[48:56],
            field_56=struct.unpack_from('<I', d, 56)[0],
            reserved_60=struct.unpack_from('<I', d, 60)[0],
            field_64=struct.unpack_from('<I', d, 64)[0],
            reserved_68_79=d[68:80],
        )

    @property
    def sensor_name(self) -> str:
        if self.sensor_flags == SENSOR_LEFT:
            return 'LEFT'
        elif self.sensor_flags == SENSOR_RIGHT:
            return 'RIGHT'
        return f'UNKNOWN(0x{self.sensor_flags:02x})'


# =============================================================================
# Part 4: pcapng 读取器
# =============================================================================

def read_pcapng_udp(filename: str) -> List[Dict]:
    """读取 pcapng 文件，提取 UDP 数据包"""
    results = []
    with open(filename, 'rb') as f:
        data = f.read()
    pos = 0
    pkt_idx = 0
    while pos < len(data) - 12:
        block_type = struct.unpack_from('<I', data, pos)[0]
        block_len = struct.unpack_from('<I', data, pos + 4)[0]
        if block_type == 6:  # Enhanced Packet Block
            if pos + 28 <= len(data):
                cap_len = struct.unpack_from('<I', data, pos + 20)[0]
                ts_high = struct.unpack_from('<I', data, pos + 12)[0]
                ts_low = struct.unpack_from('<I', data, pos + 16)[0]
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
                                    'idx': pkt_idx,
                                    'ts': (ts_high << 32) | ts_low,
                                    'src_ip': src_ip, 'dst_ip': dst_ip,
                                    'src_port': src_port, 'dst_port': dst_port,
                                    'payload': payload,
                                })
                pkt_idx += 1
        if block_len < 12:
            break
        pos += block_len
    return results


# =============================================================================
# Part 5: 帧重组器
# =============================================================================

def reassemble_frames(packets: List[Dict],
                      stream_filter: Optional[int] = None) -> Dict[Tuple[int, int], bytes]:
    """
    重组 UDP 分片为完整帧

    从 devPacketReader.tpp 逆向得知:
    - 同一帧的所有分片共享相同的 (stream_tag, frame_token)
    - payload_offset 指示分片在帧内的位置
    - frame_size 为帧总大小
    """
    fragments = defaultdict(lambda: {'size': 0, 'chunks': []})

    for pkt in packets:
        hdr = VendorHeader.from_bytes(pkt['payload'])
        if hdr is None:
            continue
        if stream_filter is not None and hdr.stream_tag != stream_filter:
            continue

        key = (hdr.stream_tag, hdr.frame_token)
        fragments[key]['size'] = hdr.frame_size
        chunk = pkt['payload'][VENDOR_HEADER_SIZE:]
        fragments[key]['chunks'].append((hdr.payload_offset, chunk))

    result = {}
    for key, info in fragments.items():
        fsize = info['size']
        if fsize <= 0 or fsize > 100_000_000:
            continue
        buf = bytearray(fsize)
        total = 0
        for offset, chunk in sorted(info['chunks']):
            end = min(offset + len(chunk), fsize)
            buf[offset:end] = chunk[:end - offset]
            total += end - offset
        if total >= fsize * 0.90:
            result[key] = bytes(buf)

    return result


# =============================================================================
# Part 6: 标定数据提取 (从 DLL 逆向确认的方法)
# =============================================================================

def extract_register_responses(packets: List[Dict]) -> List[Tuple[int, bytes]]:
    """
    从 pcapng 数据包中提取寄存器读取响应

    协议逆向发现:
    - 寄存器读取响应通过 0x1001 vendor-protocol 包返回
    - 格式: [2B stag=0x1001] [2B pkt_words] [4B global_seq] [4B cmd_ref_seq] [NB data]
    - cmd_ref_seq 与发送的 READ_REGISTER 命令的 sequence 号对应
    - data 部分是寄存器/内存内容

    ONCHIP 内存组织 (通过 WRITE reg3 选择 bank):
    - Bank 0 (count=0): 未使用/占位
    - Bank 1 (count=1): 寄存器描述表 (INI 格式选项定义)
    - Bank 2 (count=2): 标定文件 (StereoProviderV3 二进制格式)
    - Bank 3 (count=3): 设备信息 (INI 格式: Model, Lenses, SKU, firmware)
    """
    responses = []
    for pkt in packets:
        payload = pkt['payload']
        if len(payload) < 12:
            continue
        stag, pkt_words = struct.unpack_from('<HH', payload, 0)
        if stag == 0x1001 and pkt_words * 2 == len(payload):
            cmd_ref = struct.unpack_from('<I', payload, 8)[0]
            reg_data = payload[12:]
            responses.append((cmd_ref, reg_data))
    return responses


@dataclass
class OnChipCalibration:
    """
    设备 ONCHIP 内存中的标定数据 (StereoProviderV3 格式)

    二进制布局 (从 DLL 逆向 + pcapng 分析确认):

    Header (280 bytes):
      [0:16]   - Hash/UUID (16 bytes)
      [16:20]  - 设备序列号低32位
      [20:24]  - 设备序列号高32位 / 标志
      [24]     - 版本号
      [25]     - 算法版本
      [26:58]  - 标定日期字符串 (如 "2024-12-17_CMM")
      [58:280] - 保留/填充

    Each Temperature Record (296 bytes = 37 doubles):
      LEFT Camera (128 bytes = 16 doubles):
        [0:8]   double fx           - 焦距X (像素)
        [8:16]  double fy           - 焦距Y (像素)
        [16:24] double cx           - 主点X (像素)
        [24:32] double cy           - 主点Y (像素)
        [32:40] double k1           - 径向畸变1
        [40:48] double k2           - 径向畸变2
        [48:56] double p1           - 切向畸变1
        [56:64] double p2           - 切向畸变2
        [64:72] double k3           - 径向畸变3
        [72:80] double skew         - 倾斜参数
        [80:128] zeros (padding)

      RIGHT Camera + Extrinsics (168 bytes = 21 doubles):
        [0:8]   double fx           - 焦距X (像素)
        [8:16]  double fy           - 焦距Y (像素)
        [16:24] double cx           - 主点X (像素)
        [24:32] double cy           - 主点Y (像素)
        [32:40] double k1           - 径向畸变1
        [40:48] double k2           - 径向畸变2
        [48:56] double p1           - 切向畸变1
        [56:64] double p2           - 切向畸变2
        [64:72] double k3           - 径向畸变3
        [72:80] double skew         - 倾斜参数
        [80:88] double rx           - Rodrigues 旋转X
        [88:96] double ry           - Rodrigues 旋转Y
        [96:104] double rz          - Rodrigues 旋转Z
        [104:112] double tx         - 平移X (mm)
        [112:120] double ty         - 平移Y (mm)
        [120:128] double tz         - 平移Z (mm)
        [128:168] padding/reserved
    """
    calibration_date: str = ''
    serial_number: int = 0
    version: int = 0
    algo_version: int = 0
    temperature_records: List[Dict[str, Any]] = field(default_factory=list)
    device_info: Dict[str, str] = field(default_factory=dict)
    register_descriptions: str = ''

    HEADER_SIZE = 280
    RECORD_SIZE = 296  # 37 doubles
    LEFT_BLOCK_DOUBLES = 16  # 128 bytes
    RIGHT_BLOCK_DOUBLES = 21  # 168 bytes

    @classmethod
    def from_raw_data(cls, data: bytes) -> 'OnChipCalibration':
        """从原始二进制数据解析标定参数"""
        cal = cls()
        if len(data) < cls.HEADER_SIZE + cls.RECORD_SIZE:
            return cal

        # Parse header
        cal.serial_number = struct.unpack_from('<I', data, 16)[0]
        cal.version = data[24]
        cal.algo_version = data[25]
        date_bytes = data[26:58]
        cal.calibration_date = date_bytes.decode('ascii', errors='replace').split('\x00')[0]

        # Parse temperature records
        num_records = (len(data) - cls.HEADER_SIZE) // cls.RECORD_SIZE
        for i in range(num_records):
            off = cls.HEADER_SIZE + i * cls.RECORD_SIZE
            vals = struct.unpack_from(f'<{cls.LEFT_BLOCK_DOUBLES + cls.RIGHT_BLOCK_DOUBLES}d',
                                      data, off)

            # RIGHT extrinsics: indices 26-28 = rotation, 29-31 = translation
            rx, ry, rz = vals[26], vals[27], vals[28]
            tx, ty, tz = vals[29], vals[30], vals[31]

            record = {
                'index': i,
                'left_camera': {
                    'fx': vals[0], 'fy': vals[1],
                    'cx': vals[2], 'cy': vals[3],
                    'k1': vals[4], 'k2': vals[5],
                    'p1': vals[6], 'p2': vals[7],
                    'k3': vals[8], 'skew': vals[9],
                },
                'right_camera': {
                    'fx': vals[16], 'fy': vals[17],
                    'cx': vals[18], 'cy': vals[19],
                    'k1': vals[20], 'k2': vals[21],
                    'p1': vals[22], 'p2': vals[23],
                    'k3': vals[24], 'skew': vals[25],
                },
                'extrinsics': {
                    'rotation_rodrigues': [rx, ry, rz],
                    'translation_mm': [tx, ty, tz],
                    'baseline_mm': float(np.linalg.norm([tx, ty, tz])),
                },
            }
            cal.temperature_records.append(record)

        return cal

    def to_dict(self) -> Dict[str, Any]:
        """导出为完整字典"""
        result = {
            'calibration_date': self.calibration_date,
            'serial_number': self.serial_number,
            'version': self.version,
            'algo_version': self.algo_version,
            'format': 'StereoProviderV3 (temperature-compensated)',
            'num_temperature_points': len(self.temperature_records),
            'temperature_records': self.temperature_records,
        }
        if self.device_info:
            result['device_info'] = self.device_info
        if self.register_descriptions:
            result['register_descriptions_preview'] = self.register_descriptions[:500]
        return result

    def to_opencv_stereo(self, record_index: int = 0) -> Dict[str, Any]:
        """导出为 OpenCV 格式的双目标定参数"""
        if not self.temperature_records:
            return {}
        rec = self.temperature_records[record_index]
        lc = rec['left_camera']
        rc = rec['right_camera']
        ext = rec['extrinsics']

        return {
            'left_camera_matrix': [
                [lc['fx'], lc['skew'], lc['cx']],
                [0.0, lc['fy'], lc['cy']],
                [0.0, 0.0, 1.0],
            ],
            'left_dist_coeffs': [lc['k1'], lc['k2'], lc['p1'], lc['p2'], lc['k3']],
            'right_camera_matrix': [
                [rc['fx'], rc['skew'], rc['cx']],
                [0.0, rc['fy'], rc['cy']],
                [0.0, 0.0, 1.0],
            ],
            'right_dist_coeffs': [rc['k1'], rc['k2'], rc['p1'], rc['p2'], rc['k3']],
            'rotation_rodrigues': ext['rotation_rodrigues'],
            'translation_mm': ext['translation_mm'],
            'baseline_mm': ext['baseline_mm'],
        }


def extract_calibration_from_register_responses(
    cam_packets: List[Dict],
) -> Optional[OnChipCalibration]:
    """
    从相机发送的寄存器响应中提取完整的标定数据

    协议流程 (从 devFusionTrack.cpp / devMemoryReadHelper.tpp 逆向):
    1. PC WRITE reg3=0 count=2  → 选择 ONCHIP 内存 Bank 2 (标定数据)
    2. PC READ reg4             → 获取 bank 状态
    3. PC READ addr=0x3c0d0 size=16 → 读取 Bank Header (TOC)
       TOC: [4B bank_id] [4B total_size] [4B data_size] [4B checksum]
    4. PC READ addr=0x3c0e0 size=1440 → 第一页数据
    5. PC READ addr=0x3c680 size=1440 → 第二页数据
       ... (连续页读取)
    """
    responses = extract_register_responses(cam_packets)
    if not responses:
        return None

    # 识别 Bank 2 (标定) 的 TOC 和数据页
    # TOC 特征: cmd_ref 的响应包含 16 bytes, bank_id=2
    calib_toc = None
    calib_pages = []
    device_info_text = ''
    register_desc_text = ''

    # Bank 识别策略: 找到 bank_id=2 的 TOC, 然后收集后续数据页
    bank2_start_seq = None
    bank2_end_seq = None
    bank3_start_seq = None
    bank1_page_seqs = []

    for cmd_ref, data in responses:
        if len(data) == 16:
            # 可能是 TOC
            bank_id = struct.unpack_from('<I', data, 0)[0]
            if bank_id == 2:
                bank2_start_seq = cmd_ref
                total_size = struct.unpack_from('<I', data, 4)[0]
                data_size = struct.unpack_from('<I', data, 8)[0]
                calib_toc = {
                    'bank_id': bank_id,
                    'total_size': total_size,
                    'data_size': data_size,
                }
            elif bank_id == 3:
                bank3_start_seq = cmd_ref
            elif bank_id == 1:
                pass  # Bank 1 = register descriptions

    if bank2_start_seq is None:
        return None

    # 收集 Bank 2 的数据页 (TOC 之后、Bank 3 TOC 之前的大数据包)
    collecting = False
    for cmd_ref, data in responses:
        if cmd_ref == bank2_start_seq:
            collecting = True
            continue
        if bank3_start_seq is not None and cmd_ref >= bank3_start_seq:
            collecting = False
        if collecting and len(data) > 100:
            calib_pages.append((cmd_ref, data))

    # 也收集 Bank 3 (设备信息) 和 Bank 1 (寄存器描述)
    bank3_collecting = False
    for cmd_ref, data in responses:
        if bank3_start_seq is not None and cmd_ref == bank3_start_seq:
            bank3_collecting = True
            continue
        if bank3_collecting and len(data) > 10:
            text = data.decode('ascii', errors='replace')
            clean = ''.join(c if c.isprintable() or c in '\n\r\t' else '' for c in text)
            device_info_text += clean
            bank3_collecting = False

    # 收集寄存器描述 (Bank 1 数据页)
    for cmd_ref, data in responses:
        if len(data) > 100:
            text = data.decode('ascii', errors='replace')
            if '[SN_LOW]' in text or 'desc=' in text:
                clean = ''.join(c if c.isprintable() or c in '\n\r\t' else '' for c in text)
                register_desc_text += clean

    if not calib_pages:
        return None

    # 拼接标定数据
    calib_raw = bytearray()
    for cmd_ref, data in sorted(calib_pages):
        calib_raw.extend(data)

    # 解析标定参数
    cal = OnChipCalibration.from_raw_data(bytes(calib_raw))
    cal.register_descriptions = register_desc_text

    # 解析设备信息 INI
    if device_info_text:
        for line in device_info_text.split('\n'):
            line = line.strip()
            if '=' in line and not line.startswith('['):
                key, val = line.split('=', 1)
                cal.device_info[key.strip()] = val.strip()

    return cal


# =============================================================================
# Part 7: ROI 图像解码 (从 SegmenterV21 逆向确认)
# =============================================================================

@dataclass
class BlobInfo:
    """检测到的红外反光点 (blob) 信息"""
    center_x: float
    center_y: float
    area: int
    peak_value: int
    bbox_width: int
    bbox_height: int
    sensor: str  # 'LEFT' or 'RIGHT'


def decode_roi_frame(frame_data: bytes,
                     stream_tag: int) -> Tuple[Optional[InnerFrameHeader], List[BlobInfo]]:
    """
    解码一帧 ROI 数据

    根据 SegmenterV21 (segmenter::SegmenterV21<Image<uint8,1>>) 逆向:
    - 内层帧 = 80字节头 + N个16字节 ROI 记录
    - ROI 记录编码了稀疏行段压缩的红外检测图像
    - 0x80 为背景, > 0x80 为有效像素, < 0x80 可能是位置标记
    """
    header = InnerFrameHeader.from_bytes(frame_data)
    if header is None:
        return None, []

    body = frame_data[INNER_HEADER_SIZE:]
    record_count = len(body) // ROI_RECORD_SIZE

    # 简单的 blob 检测：通过阈值分析 ROI 记录
    blobs = []
    bright_pixels = []

    for i in range(record_count):
        record = body[i * ROI_RECORD_SIZE:(i + 1) * ROI_RECORD_SIZE]
        for j, val in enumerate(record):
            if val > 0x80 and val != 0xFF:
                bright_pixels.append((i, j, val))

    # 聚类亮像素为 blob
    if bright_pixels:
        # 简单连通分量分析
        visited = set()
        for idx, (row, col, val) in enumerate(bright_pixels):
            if idx in visited:
                continue
            # BFS 聚类
            cluster = [(row, col, val)]
            queue = [idx]
            visited.add(idx)
            while queue:
                cur = queue.pop(0)
                cr, cc, _ = bright_pixels[cur]
                for other_idx, (or_, oc, ov) in enumerate(bright_pixels):
                    if other_idx in visited:
                        continue
                    if abs(or_ - cr) <= 2 and abs(oc - cc) <= 2:
                        visited.add(other_idx)
                        queue.append(other_idx)
                        cluster.append((or_, oc, ov))

            if len(cluster) >= 3:
                rows = [r for r, c, v in cluster]
                cols = [c for r, c, v in cluster]
                vals = [v for r, c, v in cluster]
                blobs.append(BlobInfo(
                    center_x=sum(cols) / len(cols),
                    center_y=sum(rows) / len(rows),
                    area=len(cluster),
                    peak_value=max(vals),
                    bbox_width=max(cols) - min(cols) + 1,
                    bbox_height=max(rows) - min(rows) + 1,
                    sensor='LEFT' if stream_tag == STREAM_LEFT else 'RIGHT',
                ))

    return header, blobs


# =============================================================================
# Part 8: 主分析函数
# =============================================================================

def analyze_dll(dll_path: str) -> Dict[str, Any]:
    """分析 DLL 文件，提取逆向工程信息"""
    reader = DLLBinaryReader(dll_path)
    basename = os.path.basename(dll_path)

    info = {
        'file': basename,
        'exports': list(reader.exports.keys()) if reader.exports else [],
        'sections': reader.sections,
        'rtti_classes': [],
        'source_files': [],
        'calibration_strings': [],
        'protocol_strings': [],
    }

    # RTTI 类名
    info['rtti_classes'] = reader.extract_rtti_classes()

    # 源文件路径
    info['source_files'] = reader.get_source_files()

    # 标定相关字符串
    info['calibration_strings'] = [
        s for _, s in reader.find_string_references('calib')
        if not s.startswith('G:\\') and not s.startswith('g:\\')
    ][:50]

    # 协议相关字符串
    for keyword in ['packet', 'register', 'UDP', 'frame', 'image']:
        refs = reader.find_string_references(keyword)
        for _, s in refs[:10]:
            if s not in info['protocol_strings'] and not s.startswith('G:\\'):
                info['protocol_strings'].append(s)

    return info


def analyze_pcap(filename: str, dll_info: Optional[Dict] = None) -> Dict[str, Any]:
    """完整分析一个 pcapng 文件"""
    if not os.path.exists(filename):
        return {'error': f'File not found: {filename}'}

    packets = read_pcapng_udp(filename)

    cam_to_pc = [p for p in packets if p['src_ip'] == CAMERA_IP]
    pc_to_cam = [p for p in packets if p['dst_ip'] == CAMERA_IP]

    result = {
        'file': filename,
        'total_packets': len(packets),
        'camera_to_pc': len(cam_to_pc),
        'pc_to_camera': len(pc_to_cam),
    }

    # === 解析 PC 命令 ===
    commands = []
    for p in pc_to_cam:
        cmd = PCCommand.from_bytes(p['payload'])
        if cmd:
            commands.append({
                'sequence': cmd.sequence,
                'name': cmd.name,
                'description': cmd.describe(),
            })

    # 去重 (心跳包会重复发送)
    seen_seqs = set()
    unique_commands = []
    for c in commands:
        key = (c['sequence'], c['name'])
        if key not in seen_seqs:
            seen_seqs.add(key)
            unique_commands.append(c)
    result['commands'] = unique_commands

    # === 重组所有帧 ===
    all_frames = reassemble_frames(cam_to_pc)
    stream_summary = defaultdict(lambda: {'frame_count': 0, 'total_bytes': 0, 'frame_sizes': []})
    for (stag, ftok), data in all_frames.items():
        s = stream_summary[stag]
        s['frame_count'] += 1
        s['total_bytes'] += len(data)
        s['frame_sizes'].append(len(data))

    result['streams'] = {}
    for stag, s in sorted(stream_summary.items()):
        name = {
            STREAM_CONFIG: 'CONFIG/REGISTER',
            STREAM_CALIB: 'CALIBRATION',
            STREAM_LEFT: 'LEFT_ROI',
            STREAM_RIGHT: 'RIGHT_ROI',
            STREAM_FACTORY: 'FACTORY_FILE',
        }.get(stag, f'UNKNOWN')
        result['streams'][f'0x{stag:04x}'] = {
            'name': name,
            'frame_count': s['frame_count'],
            'total_bytes': s['total_bytes'],
            'avg_frame_size': round(s['total_bytes'] / max(1, s['frame_count']), 1),
            'min_frame_size': min(s['frame_sizes']),
            'max_frame_size': max(s['frame_sizes']),
        }

    # === 提取标定数据 (通过寄存器响应 - DLL 逆向确认的方法) ===
    onchip_cal = extract_calibration_from_register_responses(cam_to_pc)

    result['calibration'] = {
        'found': onchip_cal is not None and len(onchip_cal.temperature_records) > 0,
    }
    if onchip_cal and onchip_cal.temperature_records:
        result['calibration']['onchip_calibration'] = onchip_cal.to_dict()
        result['calibration']['opencv_stereo'] = onchip_cal.to_opencv_stereo(0)
        if onchip_cal.device_info:
            result['calibration']['device_info'] = onchip_cal.device_info

    # === 解码示例帧 ===
    sample_frames = {'left': [], 'right': []}
    for (stag, ftok), data in sorted(all_frames.items()):
        if stag not in (STREAM_LEFT, STREAM_RIGHT):
            continue
        side = 'left' if stag == STREAM_LEFT else 'right'
        if len(sample_frames[side]) >= 3:
            continue
        header, blobs = decode_roi_frame(data, stag)
        if header:
            frame_info = {
                'frame_token': ftok,
                'frame_size': len(data),
                'device_timestamp_us': header.device_timestamp_us,
                'frame_counter': header.frame_counter,
                'sensor': header.sensor_name,
                'roi_record_count': (len(data) - INNER_HEADER_SIZE) // ROI_RECORD_SIZE,
                'blob_count': len(blobs),
                'blobs': [
                    {
                        'center_x': round(b.center_x, 2),
                        'center_y': round(b.center_y, 2),
                        'area': b.area,
                        'peak_value': b.peak_value,
                    }
                    for b in blobs[:20]
                ],
            }
            sample_frames[side].append(frame_info)

    result['sample_frames'] = sample_frames

    return result


def main():
    parser = argparse.ArgumentParser(
        description='fusionTrack DLL 逆向工程辅助解码器',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python dll_reverse_decode.py                          # 使用默认路径
  python dll_reverse_decode.py stylus.pcapng            # 指定抓包文件
  python dll_reverse_decode.py --extract-calib          # 提取标定参数
  python dll_reverse_decode.py --dll-info               # 仅显示 DLL 逆向信息
  python dll_reverse_decode.py --json output.json       # 输出 JSON
        """)
    parser.add_argument('pcap', nargs='?',
                        default='fusionTrack SDK x64/output/full_01.pcapng',
                        help='pcapng 文件路径')
    parser.add_argument('--dll-path', default='fusionTrack SDK x64/bin',
                        help='DLL 文件目录')
    parser.add_argument('--extract-calib', action='store_true',
                        help='提取并输出标定参数')
    parser.add_argument('--dll-info', action='store_true',
                        help='仅显示 DLL 逆向工程信息')
    parser.add_argument('--json', help='输出 JSON 文件路径')
    parser.add_argument('--all-pcaps', action='store_true',
                        help='分析所有可用的 pcapng 文件')
    args = parser.parse_args()

    output = {}

    # === DLL 逆向分析 ===
    dll_dir = args.dll_path
    dll_infos = {}
    for dll_name in ('fusionTrack64.dll', 'device64.dll'):
        dll_path = os.path.join(dll_dir, dll_name)
        if os.path.exists(dll_path):
            dll_infos[dll_name] = analyze_dll(dll_path)

    output['dll_analysis'] = dll_infos

    if args.dll_info:
        print(json.dumps(output, indent=2, ensure_ascii=False, default=str))
        return

    # === 打印 DLL 逆向摘要 ===
    print("=" * 70)
    print("fusionTrack DLL 逆向工程辅助解码器")
    print("=" * 70)

    for dll_name, info in dll_infos.items():
        print(f"\n--- {dll_name} ---")
        print(f"  导出函数: {len(info['exports'])} 个")
        print(f"  RTTI 类: {len(info['rtti_classes'])} 个")
        print(f"  源文件: {len(info['source_files'])} 个")
        # 关键类
        key_classes = [c for c in info['rtti_classes']
                       if any(k in c for k in ['Stereo', 'Calib', 'Segment',
                                                'Marker', 'Feature', 'ftkDevice'])]
        if key_classes:
            print(f"  关键类:")
            for c in key_classes[:15]:
                # 简化 RTTI 名
                simple = c.replace('.?AV', '').replace('.?AU', '').replace('@@', '')
                print(f"    {simple}")

    # === pcapng 分析 ===
    pcap_files = []
    if args.all_pcaps:
        pcap_dir = 'fusionTrack SDK x64/output'
        if os.path.isdir(pcap_dir):
            for f in sorted(os.listdir(pcap_dir)):
                if f.endswith('.pcapng'):
                    pcap_files.append(os.path.join(pcap_dir, f))
    else:
        pcap_files = [args.pcap]

    output['pcap_analysis'] = {}
    for pcap_file in pcap_files:
        if not os.path.exists(pcap_file):
            print(f"\n⚠️  文件不存在: {pcap_file}")
            continue

        print(f"\n{'=' * 70}")
        print(f"分析: {pcap_file}")
        print(f"{'=' * 70}")

        pcap_result = analyze_pcap(pcap_file, dll_infos)
        output['pcap_analysis'][pcap_file] = pcap_result

        # 打印结果
        print(f"\n  UDP 包: {pcap_result['total_packets']} 总计")
        print(f"    相机→电脑: {pcap_result['camera_to_pc']}")
        print(f"    电脑→相机: {pcap_result['pc_to_camera']}")

        # 命令
        if pcap_result.get('commands'):
            print(f"\n  控制命令 ({len(pcap_result['commands'])} 条):")
            for cmd in pcap_result['commands'][:15]:
                print(f"    seq={cmd['sequence']:3d}  {cmd['description']}")
            if len(pcap_result['commands']) > 15:
                print(f"    ... (还有 {len(pcap_result['commands']) - 15} 条)")

        # 数据流
        print(f"\n  数据流:")
        for stag, info in pcap_result.get('streams', {}).items():
            print(f"    {stag} ({info['name']}): "
                  f"{info['frame_count']} 帧, "
                  f"大小 {info['min_frame_size']}-{info['max_frame_size']} bytes, "
                  f"总计 {info['total_bytes']:,} bytes")

        # 标定数据
        calib = pcap_result.get('calibration', {})
        if calib.get('found'):
            cal_data = calib.get('onchip_calibration', {})
            opencv = calib.get('opencv_stereo', {})
            dev = calib.get('device_info', {})

            print(f"\n  ✅ 标定数据成功从设备 ONCHIP 内存提取!")
            if cal_data.get('calibration_date'):
                print(f"    标定日期: {cal_data['calibration_date']}")
            if dev:
                print(f"    设备型号: {dev.get('Model', 'N/A')}")
                print(f"    镜头类型: {dev.get('Lenses', 'N/A')}")
                print(f"    SKU: {dev.get('SKU', 'N/A')}")
            print(f"    温度补偿点数: {cal_data.get('num_temperature_points', 0)}")
            print(f"    格式: {cal_data.get('format', 'N/A')}")

            if opencv:
                lm = opencv.get('left_camera_matrix', [[0]*3]*3)
                rm = opencv.get('right_camera_matrix', [[0]*3]*3)
                ld = opencv.get('left_dist_coeffs', [0]*5)
                rd = opencv.get('right_dist_coeffs', [0]*5)
                t = opencv.get('translation_mm', [0]*3)
                r = opencv.get('rotation_rodrigues', [0]*3)
                bl = opencv.get('baseline_mm', 0)

                print(f"\n    === 左目相机内参 ===")
                print(f"      焦距: fx={lm[0][0]:.4f}, fy={lm[1][1]:.4f} px")
                print(f"      主点: cx={lm[0][2]:.4f}, cy={lm[1][2]:.4f} px")
                print(f"      畸变: k1={ld[0]:.8f}, k2={ld[1]:.8f}")
                print(f"             p1={ld[2]:.8f}, p2={ld[3]:.8f}")
                print(f"             k3={ld[4]:.8f}")

                print(f"\n    === 右目相机内参 ===")
                print(f"      焦距: fx={rm[0][0]:.4f}, fy={rm[1][1]:.4f} px")
                print(f"      主点: cx={rm[0][2]:.4f}, cy={rm[1][2]:.4f} px")
                print(f"      畸变: k1={rd[0]:.8f}, k2={rd[1]:.8f}")
                print(f"             p1={rd[2]:.8f}, p2={rd[3]:.8f}")
                print(f"             k3={rd[4]:.8f}")

                print(f"\n    === 双目外参 (右目相对于左目) ===")
                print(f"      旋转(Rodrigues): [{r[0]:.10f}, {r[1]:.10f}, {r[2]:.10f}]")
                print(f"      平移(mm): [{t[0]:.4f}, {t[1]:.4f}, {t[2]:.4f}]")
                print(f"      基线长度: {bl:.4f} mm")
        else:
            print(f"\n  ⚠️  未能从抓包中提取到标定参数")
            print(f"      标定数据在设备初始化时通过 READ_REGISTER 命令传输,")
            print(f"      存储在设备 ONCHIP 内存 Bank 2 中")
            print(f"      请确保抓包覆盖了完整的初始化阶段")

        # 示例帧
        for side in ('left', 'right'):
            frames = pcap_result.get('sample_frames', {}).get(side, [])
            if frames:
                print(f"\n  {side.upper()} 目示例帧:")
                for f in frames[:2]:
                    print(f"    帧#{f['frame_counter']}: "
                          f"{f['roi_record_count']} ROI记录, "
                          f"{f['blob_count']} 个检测点, "
                          f"时间戳={f['device_timestamp_us']}μs")

    # === 输出 JSON ===
    if args.json:
        with open(args.json, 'w', encoding='utf-8') as f:
            json.dump(output, f, indent=2, ensure_ascii=False, default=str)
        print(f"\n📄 完整结果已保存到: {args.json}")

    # === 标定参数提取 ===
    if args.extract_calib:
        print(f"\n{'=' * 70}")
        print("标定参数提取")
        print(f"{'=' * 70}")
        print("""
根据 DLL 逆向工程分析:

1. ftkCameraParameters 结构 (40 bytes, packed):
   - FocalLength[2]:    float32 × 2  (焦距 fx, fy 像素)
   - OpticalCentre[2]:  float32 × 2  (主点 cx, cy 像素)
   - Distorsions[5]:    float32 × 5  (畸变 k1, k2, p1, p2, k3)
   - Skew:              float32      (倾斜参数)
   标定模型参考: Bouguet/Caltech Camera Calibration Toolbox

2. ftkStereoParameters 结构 (104 bytes, packed):
   - LeftCamera:         ftkCameraParameters  (左目)
   - RightCamera:        ftkCameraParameters  (右目)
   - Translation[3]:     float32 × 3  (右目在左目坐标系的平移 mm)
   - Rotation[3]:        float32 × 3  (右目在左目坐标系的旋转, Rodrigues 向量)

3. ftkARCalibrationParameters 结构 (32 bytes, packed):
   - principalPointX:    float64  (主点X)
   - principalPointY:    float64  (主点Y)
   - focalLenghtX:       float64  (焦距X)
   - focalLenghtY:       float64  (焦距Y)

标定数据传输流程 (从 DLL 逆向重建):
  a) ftkInit/ftkInitExt → 初始化库
  b) retrieveLastDevice → 设备发现 (广播)
  c) devFusionTrack::init → 读取寄存器描述 (READ_REG addr=0-4)
  d) devMemRead → 从 ONCHIP 内存下载标定文件 (READ_REG addr=0x3c0d0+)
  e) CalibrationGetter / GeometryReaderJsonV2 → 解析 JSON 标定文件
  f) StereoProviderV3 → 初始化立体视觉几何模型 (含温度补偿)
  g) StereoInterpolatorV1 → 温度插值器
  h) ftkExtractFrameInfo(CalibrationParameters) → 返回 ftkStereoParameters

SDK API 获取标定参数的方法:
  ftkFrameInfoData info;
  info.WantedInformation = CalibrationParameters;
  ftkExtractFrameInfo(&frame, &info);
  // info.Calibration 即为 ftkStereoParameters
""")

        # 尝试从所有已分析的 pcap 中找到标定数据
        found = False
        for pcap_file, result in output.get('pcap_analysis', {}).items():
            calib = result.get('calibration', {})
            if calib.get('found'):
                cal_data = calib.get('onchip_calibration', {})
                opencv = calib.get('opencv_stereo', {})
                print(f"\n从 {pcap_file} 提取到的标定参数:")
                print(json.dumps(opencv, indent=2))
                if cal_data.get('temperature_records'):
                    print(f"\n温度补偿标定表 ({cal_data['num_temperature_points']} 个温度点):")
                    print(json.dumps(cal_data['temperature_records'][:3], indent=2))
                    if len(cal_data['temperature_records']) > 3:
                        print(f"... (还有 {len(cal_data['temperature_records']) - 3} 个温度点)")
                found = True
                break

        if not found:
            print("⚠️  当前抓包中未找到标定参数数据。")
            print("    请确保抓包覆盖了完整的设备初始化阶段。")


if __name__ == '__main__':
    main()
