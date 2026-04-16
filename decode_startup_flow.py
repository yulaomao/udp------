#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fusionTrack 启动流程解码器
===========================

解码双目相机初始化阶段 (full_01.pcapng) 中电脑 -> 相机的控制命令，
以及相机的配置响应数据 (0x1001 流)。

对应 SDK 调用流程:
    ftkInitExt()        → 库初始化
    retrieveLastDevice() → 设备发现
    ftkEnumerateOptions() → 枚举选项
    ftkSetRigidBody()    → 加载几何体
    ftkSetFrameOptions() → 配置帧选项
    ftkGetLastFrame()    → 持续获取帧数据

用法:
    python decode_startup_flow.py [pcapng_file]
"""

import struct
import sys
import os
from collections import defaultdict

# =============================================================================
# 设备网络常量
# =============================================================================

CAMERA_IP = '172.17.1.7'        # fusionTrack 相机默认 IP 地址
CAMERA_PORT = 3509              # fusionTrack 相机默认 UDP 端口


def read_pcapng_udp(filename):
    """读取 pcapng 文件，提取 UDP 数据包（保留时间顺序）"""
    results = []
    with open(filename, 'rb') as f:
        data = f.read()
    pos = 0
    pkt_idx = 0
    while pos < len(data) - 12:
        block_type = struct.unpack_from('<I', data, pos)[0]
        block_len = struct.unpack_from('<I', data, pos + 4)[0]
        if block_type == 6:
            if pos + 28 <= len(data):
                cap_len = struct.unpack_from('<I', data, pos + 20)[0]
                ts_high = struct.unpack_from('<I', data, pos + 12)[0]
                ts_low = struct.unpack_from('<I', data, pos + 16)[0]
                pkt_data = data[pos + 28:pos + 28 + cap_len]
                if len(pkt_data) >= 42:
                    eth_type = struct.unpack_from('>H', pkt_data, 12)[0]
                    if eth_type == 0x0800:
                        ip_hdr_len = (pkt_data[14] & 0x0F) * 4
                        ip_proto = pkt_data[23]
                        src_ip = '.'.join(str(b) for b in pkt_data[26:30])
                        dst_ip = '.'.join(str(b) for b in pkt_data[30:34])
                        if ip_proto == 17:
                            udp_start = 14 + ip_hdr_len
                            if udp_start + 8 <= len(pkt_data):
                                src_port = struct.unpack_from('>H', pkt_data, udp_start)[0]
                                dst_port = struct.unpack_from('>H', pkt_data, udp_start + 2)[0]
                                payload = pkt_data[udp_start + 8:]
                                results.append({
                                    'idx': pkt_idx,
                                    'ts_high': ts_high,
                                    'ts_low': ts_low,
                                    'src_ip': src_ip,
                                    'dst_ip': dst_ip,
                                    'src_port': src_port,
                                    'dst_port': dst_port,
                                    'payload': payload,
                                })
                pkt_idx += 1
        if block_len < 12:
            break
        pos += block_len
    return results


def decode_pc_command(payload):
    """
    解码电脑 -> 相机的控制命令 (24 字节)

    命令格式:
        Offset  Size  Field
        0       2     command_type      命令类型
        2       2     command_words     载荷长度/2
        4       4     sequence          序列号
        8       4     reserved          保留
        12      4     param1            参数1 (含义取决于命令类型)
        16      4     param2            参数2
        20      4     param3            参数3
    """
    if len(payload) < 12:
        return None

    cmd_type = struct.unpack_from('<H', payload, 0)[0]
    cmd_words = struct.unpack_from('<H', payload, 2)[0]
    sequence = struct.unpack_from('<I', payload, 4)[0]
    reserved = struct.unpack_from('<I', payload, 8)[0]

    result = {
        'command_type': cmd_type,
        'command_words': cmd_words,
        'sequence': sequence,
        'reserved': reserved,
        'raw_hex': payload.hex(),
    }

    if len(payload) >= 24:
        result['param1'] = struct.unpack_from('<I', payload, 12)[0]
        result['param2'] = struct.unpack_from('<I', payload, 16)[0]
        result['param3'] = struct.unpack_from('<I', payload, 20)[0]

    return result


def decode_config_stream(assembled_data):
    """
    解码 0x1001 配置流内容

    配置数据为 ASCII 文本格式的设备选项定义，
    格式类似 INI 文件:
        [OPTION_NAME]
        desc=@
        addr=N
        rw=M
    """
    # 0x1001 流的数据经常跨越多个包，且头部格式不同
    # 直接提取可打印 ASCII 内容
    text_parts = []
    for data in assembled_data:
        text = ""
        for b in data:
            if 32 <= b < 127 or b in (10, 13):
                text += chr(b)
        if text.strip():
            text_parts.append(text)
    return "\n".join(text_parts)


def main():
    filename = sys.argv[1] if len(sys.argv) > 1 else 'fusionTrack SDK x64/output/full_01.pcapng'

    if not os.path.exists(filename):
        print("错误: 文件不存在: %s" % filename)
        sys.exit(1)

    print("=" * 70)
    print("fusionTrack 启动流程解码")
    print("=" * 70)
    print("文件: %s\n" % filename)

    packets = read_pcapng_udp(filename)
    print("UDP 包总数: %d" % len(packets))

    # 分类数据包
    pc_to_cam = []
    cam_to_pc = []
    broadcast = []
    other = []

    for p in packets:
        if p['dst_ip'] == CAMERA_IP and p['dst_port'] == CAMERA_PORT:
            pc_to_cam.append(p)
        elif p['src_ip'] == CAMERA_IP and p['src_port'] == CAMERA_PORT:
            cam_to_pc.append(p)
        elif p['dst_ip'] in ('255.255.255.255', '172.17.1.255'):
            broadcast.append(p)
        else:
            other.append(p)

    print("电脑 -> 相机:     %d 包" % len(pc_to_cam))
    print("相机 -> 电脑:     %d 包" % len(cam_to_pc))
    print("广播包:           %d 包" % len(broadcast))

    # ===== 1. 广播数据包 (设备发现) =====
    print("\n" + "-" * 70)
    print("1. 广播数据包 (设备发现)")
    print("-" * 70)
    for p in broadcast[:5]:
        print("  [包#%d] %s:%d -> %s:%d len=%d data=%s" % (
            p['idx'], p['src_ip'], p['src_port'],
            p['dst_ip'], p['dst_port'], len(p['payload']),
            p['payload'].hex()))

    # ===== 2. 电脑 -> 相机命令 =====
    print("\n" + "-" * 70)
    print("2. 电脑 -> 相机控制命令")
    print("-" * 70)

    # Decode command sequence
    cmd_types = {}
    for i, p in enumerate(pc_to_cam):
        cmd = decode_pc_command(p['payload'])
        if cmd is None:
            continue

        ct = cmd['command_type']
        if ct not in cmd_types:
            cmd_types[ct] = 0
        cmd_types[ct] += 1

        if i < 30 or (len(pc_to_cam) - i) < 5:
            params = ""
            if 'param1' in cmd:
                params = " param1=0x%08x param2=0x%08x param3=0x%08x" % (
                    cmd['param1'], cmd['param2'], cmd['param3'])
            print("  [包#%d] cmd_type=0x%04x seq=%d%s" % (
                p['idx'], ct, cmd['sequence'], params))
            print("    raw: %s" % cmd['raw_hex'])

    print("\n  命令类型统计:")
    for ct, count in sorted(cmd_types.items()):
        print("    0x%04x: %d 次" % (ct, count))

    # ===== 3. 相机 -> 电脑响应 =====
    print("\n" + "-" * 70)
    print("3. 相机 -> 电脑数据流")
    print("-" * 70)

    # Analyze by destination port and stream tag
    cam_flows = defaultdict(lambda: {'count': 0, 'stream_tags': defaultdict(int)})

    for p in cam_to_pc:
        key = (p['dst_ip'], p['dst_port'])
        cam_flows[key]['count'] += 1
        if len(p['payload']) >= 2:
            stag = struct.unpack_from('<H', p['payload'], 0)[0]
            cam_flows[key]['stream_tags'][stag] += 1

    for key, info in sorted(cam_flows.items(), key=lambda x: -x[1]['count']):
        print("  -> %s:%d: %d 包" % (key[0], key[1], info['count']))
        for stag, cnt in sorted(info['stream_tags'].items()):
            print("    stream 0x%04x: %d 包" % (stag, cnt))

    # ===== 4. 0x1001 配置流解码 =====
    print("\n" + "-" * 70)
    print("4. 配置流 0x1001 (设备选项定义)")
    print("-" * 70)

    config_data = []
    for p in cam_to_pc:
        if len(p['payload']) >= 24:
            stag = struct.unpack_from('<H', p['payload'], 0)[0]
            if stag == 0x1001:
                # Extract body (after header)
                body = p['payload'][24:]
                config_data.append(body)

    if config_data:
        config_text = decode_config_stream(config_data)
        # Parse options
        options = []
        current_option = {}
        for line in config_text.split('\n'):
            line = line.strip()
            if line.startswith('[') and line.endswith(']'):
                if current_option:
                    options.append(current_option)
                current_option = {'name': line[1:-1]}
            elif '=' in line:
                key, val = line.split('=', 1)
                current_option[key] = val
        if current_option:
            options.append(current_option)

        print("  共 %d 个设备选项:" % len(options))
        for opt in options[:30]:  # Show first 30
            name = opt.get('name', '???')
            rw = opt.get('rw', '?')
            addr = opt.get('addr', '?')
            rw_label = {'1': 'RO', '3': 'RW'}.get(rw, 'RW=%s' % rw)
            print("    [addr=%3s] %-40s %s" % (addr, name, rw_label))

        if len(options) > 30:
            print("    ... 还有 %d 个选项" % (len(options) - 30))

    # ===== 5. 0x1006 流 =====
    print("\n" + "-" * 70)
    print("5. 其他数据流 (0x1006)")
    print("-" * 70)

    for p in cam_to_pc:
        if len(p['payload']) >= 24:
            stag = struct.unpack_from('<H', p['payload'], 0)[0]
            if stag == 0x1006:
                body = p['payload'][24:]
                print("  [包#%d] 0x1006 body: %s" % (p['idx'], body.hex()))
                if len(body) >= 4:
                    val = struct.unpack_from('<I', body, 0)[0]
                    print("    first uint32: %d" % val)

    # ===== 6. 时间线 =====
    print("\n" + "-" * 70)
    print("6. 启动流程时间线 (前 200 个包的交互)")
    print("-" * 70)

    # Find first 200 relevant packets in order
    relevant = []
    for p in packets[:500]:
        if p['dst_ip'] == CAMERA_IP and p['dst_port'] == CAMERA_PORT:
            relevant.append(('PC->CAM', p))
        elif p['src_ip'] == CAMERA_IP and p['src_port'] == CAMERA_PORT:
            relevant.append(('CAM->PC', p))
        elif p['dst_ip'] in ('255.255.255.255', '172.17.1.255'):
            relevant.append(('BCAST', p))

    prev_dir = None
    group_count = 0
    for direction, p in relevant[:100]:
        if direction != prev_dir:
            if prev_dir and group_count > 3:
                print("    ... 共 %d 包" % group_count)
            prev_dir = direction
            group_count = 0

        group_count += 1
        if group_count <= 3:
            payload = p['payload']
            extra = ""
            if direction == 'PC->CAM' and len(payload) >= 12:
                cmd = decode_pc_command(payload)
                if cmd:
                    extra = " cmd_type=0x%04x seq=%d" % (cmd['command_type'], cmd['sequence'])
            elif direction == 'CAM->PC' and len(payload) >= 24:
                stag, _, seq = struct.unpack_from('<HHI', payload, 0)
                extra = " stream=0x%04x seq=%d" % (stag, seq)
            elif direction == 'BCAST':
                extra = " data=%s" % payload.hex()

            print("  [包#%5d] %-8s %s:%d -> %s:%d len=%d%s" % (
                p['idx'], direction,
                p['src_ip'], p['src_port'],
                p['dst_ip'], p['dst_port'],
                len(payload), extra))

    if group_count > 3:
        print("    ... 共 %d 包" % group_count)


if __name__ == '__main__':
    main()
