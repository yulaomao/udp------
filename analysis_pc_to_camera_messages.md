# fusionTrack SDK 电脑端→相机端消息完整解析

## 1. 概述

**分析文件**: `fusionTrack SDK x64/output/full_01.pcapng`

**网络环境**:
- **电脑 IP**: `172.17.1.22`（端口 49278 / 49279）
- **相机 IP**: `172.17.1.7`（端口 3509，fusionTrack 默认端口）
- **广播地址**: `172.17.1.255` / `255.255.255.255`（端口 63630）

**数据包统计**:
| 方向 | 数量 |
|------|------|
| 总 UDP 包 | 30,600 |
| PC → 相机 | 177 包 |
| 相机 → PC | 30,380 包 |
| 广播包 | 34 包 |

**去重后唯一命令**: 156 条（按序列号 seq 去重，部分命令因 UDP 重传发送了多次）

---

## 2. 协议格式

### 2.1 PC→Camera 命令包格式

所有电脑发往相机的命令均为固定格式的小端序（little-endian）UDP 数据包：

```
偏移    大小(字节)  字段              说明
0       2           command_type      命令类型
2       2           command_words     载荷总长度 / 2（即以 word 为单位）
4       4           sequence          序列号（全局递增，用于匹配请求/响应）
8       4           reserved          保留字段（始终为 0）
--- 以下字段仅在 24 字节命令中存在 ---
12      4           param1            参数1（含义取决于命令类型）
16      4           param2            参数2
20      4           param3            参数3
```

### 2.2 命令类型定义

从通讯数据中观察到以下 4 种命令类型：

| 命令类型 | 载荷大小 | 名称 | 说明 |
|----------|----------|------|------|
| `0x0001` | 24 字节 | **READ_REGISTER** | 读取设备寄存器/选项值 |
| `0x0002` | 24 字节 | **WRITE_REGISTER** | 写入设备寄存器/选项值 |
| `0x0005` | 16 字节 | **FRAME_REQUEST** | 请求帧数据（ACK/流控） |
| `0x0007` | 12 字节 | **DISCONNECT** | 断开连接 |

### 2.3 Camera→PC 数据流格式（Vendor Header）

相机返回的数据使用分帧流协议，24 字节头部：

```
偏移    大小(字节)  字段
0       2           stream_tag        流标识（0x1001/0x1003/0x1004/0x1006）
2       2           packet_words      包大小/2
4       4           packet_sequence   包序号
8       4           reserved          保留
12      4           frame_token       帧令牌
16      4           frame_size        帧总大小
20      4           payload_offset    当前片段在帧中的偏移
```

---

## 3. 完整消息时序与 API 映射

以下是按序列号排列的完整消息清单。每条消息都标注了：
- **命令类型**和参数
- **设备寄存器/选项名称**（从 0x1001 配置流解码获得）
- **对应的 SDK API 调用**
- **在 stereo2_AcquisitionBasic.cpp 代码中的位置**

### 阶段一：设备发现（广播阶段）

在 PC→Camera 单播通讯之前，SDK 首先通过广播发现设备。

| 包号 | 方向 | 内容 | API 对应 |
|------|------|------|----------|
| #4410 | PC → 172.17.1.255:63630 | `"0:3\0"` (4字节) | `ftkEnumerateDevices()` → 设备发现广播 |
| #4411 | PC → 255.255.255.255:63630 | `"0:3\0"` (4字节) | 同上（双广播确保覆盖） |
| ...重复多组... | | | SDK 内部周期性广播直到发现设备 |

**总计**: 34 个广播包，分为 17 对（子网广播 + 全局广播），在整个会话期间持续发送。

**说明**: `ftkEnumerateDevices()` 内部向端口 63630 发送 `"0:3\0"` 广播包，其中 `"0:3"` 表示协议版本标识。相机收到广播后会响应自己的序列号和设备类型。对应代码 `stereo2_AcquisitionBasic.cpp` 第 382 行：
```cpp
DeviceData device( retrieveLastDevice( lib, true, false, !isNotFromConsole ) );
```

---

### 阶段二：设备连接与初始化（seq 1-10）

SDK 连接到相机后，首先执行一系列寄存器读取以获取设备基本信息和校准数据。

| seq | 命令 | 寄存器ID | 寄存器名称 | param2 | param3 | SDK API | 说明 |
|-----|------|----------|------------|--------|--------|---------|------|
| 1 | READ | 0 | SN_LOW (序列号低32位) | 4 | 0 | `ftkInitExt()` / `ftkEnumerateDevices()` | 读取设备序列号低位 |
| 2 | READ | 1 | SN_HIGH (序列号高32位) | 4 | 0 | 同上 | 读取设备序列号高位 |
| 3 | READ | 2 | ONCHIP_ADDR | 4 | 0 | 同上 | 读取芯片地址信息 |
| 4 | WRITE | 3 | CHUNK_GET | - | 1 | 同上 | 请求配置数据块 chunk=1 |
| 5 | READ | 4 | CHUNK_RDY | 4 | 0 | 同上 | 检查数据块是否就绪 |
| 6 | READ | 245968 (0x3C0D0) | [内存区域头部] | 16 | 1 | 同上 | 读取板载内存区域头信息(16字节) |
| 7 | READ | 245984 (0x3C0E0) | [内存区域数据] | 1440 | 1 | 同上 | 读取板载内存数据(1440字节) |
| 8 | READ | 247424 (0x3C680) | [内存区域数据] | 1440 | 1 | 同上 | 继续读取板载内存数据 |
| 9 | READ | 248864 (0x3CC20) | [内存区域数据] | 1440 | 1 | 同上 | 继续读取板载内存数据 |
| 10 | READ | 250304 (0x3D1C0) | [内存区域数据] | 1348 | 1 | 同上 | 读取最后一段内存数据(1348字节) |

**说明**:
- seq 1-2：读取设备序列号（64位，分高低32位读取），用于唯一标识设备。这些对应 `ftkEnumerateDevices()` 的设备枚举回调中获取的 `sn`。
- seq 3：读取 ONCHIP_ADDR，获取芯片内存布局信息。
- seq 4-5：CHUNK_GET/CHUNK_RDY 是分块传输协议。写入 CHUNK_GET=1 请求第1块配置数据，然后读取 CHUNK_RDY 确认就绪。相机随后通过 0x1001 流返回设备选项定义列表。
- seq 6-10：这些读取的是**高地址内存区域**（0x3C0D0-0x3D704），对应设备板载存储的**校准参数**。总共读取 16 + 1440×3 + 1348 = **5684 字节**的校准/配置数据。这些数据由 `ftkEnumerateDevices()` 内部自动读取，用于初始化SDK的三角测量引擎。

---

### 阶段三：帧请求与流控（seq 11-15）

| seq | 命令 | param1 (16进制) | 说明 | SDK API |
|-----|------|-----------------|------|---------|
| 11 | FRAME_REQUEST | 0x1FC4 | 帧流控确认 (ACK token=0x1FC4) | 内部流控 |
| 12 | FRAME_REQUEST | 0x1BC4 | 帧流控确认 | 同上 |
| 13 | FRAME_REQUEST | 0x17C4 | 帧流控确认 | 同上 |
| 14 | FRAME_REQUEST | 0x0FC4 | 帧流控确认 | 同上 |
| 15 | FRAME_REQUEST | 0x07C4 | 帧流控确认 | 同上 |

**说明**: `FRAME_REQUEST (0x0005)` 是 16 字节命令，其中 param1 (偏移 12-15) 是帧令牌 / 流控 ACK。SDK 库通过这些命令告诉相机"我已经处理完毕某帧数据，请继续发送"。这些消息在 `ftkInitExt()` → `ftkEnumerateDevices()` 期间自动发出，因为此时相机已开始发送 0x1003 / 0x1004 图像数据流。值 `0x1FC4`, `0x1BC4`, `0x17C4`, `0x0FC4`, `0x07C4` 为递减序列，对应连续5帧的确认令牌。

---

### 阶段四：PACKET_MAX_SIZE 设置与设备信息读取（seq 16-18）

| seq | 命令 | 寄存器ID | 寄存器名称 | 值 | SDK API | 说明 |
|-----|------|----------|------------|------|---------|------|
| 16 | WRITE | 5 | PACKET_MAX_SIZE | 1452 | `ftkInitExt()` | 设置最大数据包大小为 1452 字节 |
| 17 | READ | 63 | DEVICE_VERSION | - | `ftkEnumerateDevices()` | 读取设备版本号 |
| 18 | READ | 62 | ELEC_VERSION | - | 同上 | 读取电子板版本号 |

**说明**: 
- seq 16：写入 PACKET_MAX_SIZE = 1452，这是标准以太网 MTU (1500) 减去 IP/UDP 头部后的最大 UDP 载荷大小。
- seq 17-18：读取设备固件版本和电子版本，用于 SDK 内部兼容性检查。

---

### 阶段五：第二轮配置数据读取（seq 19-26）

| seq | 命令 | 寄存器ID | 寄存器名称 | 值 | 说明 |
|-----|------|----------|------------|------|------|
| 19 | WRITE | 3 | CHUNK_GET | 2 | 请求第2块配置数据 |
| 20 | READ | 4 | CHUNK_RDY | - | 检查就绪状态 |
| 21 | READ | 245968 | [内存头] | 16 | 读取头信息 |
| 22 | READ | 245984 | [内存数据] | 1440 | 读取第2块校准数据 |
| 23 | READ | 247424 | [内存数据] | 1440 | 继续 |
| 24 | READ | 248864 | [内存数据] | 1440 | 继续 |
| 25 | READ | 250304 | [内存数据] | 1440 | 继续 |
| 26 | READ | 251744 (0x3D760) | [内存数据] | 440 | 读取最后段(440字节) |

**说明**: 第2块配置数据，共 16 + 1440×4 + 440 = **6216 字节**。这是另一组校准参数块，同样由 SDK 在初始化阶段自动读取。

---

### 阶段六：第三轮配置数据读取（seq 27-30）

| seq | 命令 | 寄存器ID | 寄存器名称 | 值 | 说明 |
|-----|------|----------|------------|------|------|
| 27 | WRITE | 3 | CHUNK_GET | 3 | 请求第3块配置数据 |
| 28 | READ | 4 | CHUNK_RDY | - | 检查就绪状态 |
| 29 | READ | 245968 | [内存头] | 16 | 读取头信息 |
| 30 | READ | 245984 | [内存数据] | 124 | 仅 124 字节（较短的数据块） |

**说明**: 第3块配置数据较小，仅 16 + 124 = **140 字节**。可能是设备特定的少量配置参数。

---

### 阶段七：ftkEnumerateOptions 枚举设备选项（seq 31-44）

此阶段对应 `stereo2_AcquisitionBasic.cpp` 第 387 行：
```cpp
ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &options ) );
```

| seq | 命令 | 寄存器ID | 寄存器名称 | param3 (值) | 说明 |
|-----|------|----------|------------|-------------|------|
| 31 | READ | 47 | CAM0_THRESHOLD | - | 读取左相机检测阈值 |
| 32 | READ | 48 | CAM1_THRESHOLD | - | 读取右相机检测阈值 |
| 33 | READ | 54 | ACC_RANGE | - | 读取加速度计量程 |
| 34 | WRITE | 46 | ACQUISITION | 1 | **启动采集模式**（关键命令！） |
| 35 | READ | 64 | DEVICE_ACCESS | - | 读取设备访问权限 |
| 36 | READ | 148 | BUZZER_PERIOD | - | 读取蜂鸣器周期 |
| 37 | READ | 144 | PLAIN_NUMBER_LOW | - | 读取认证明文低位 |
| 38 | READ | 10 | US_ALT_EXP_TARGET | - | 读取备用曝光目标值 |
| 39 | READ | 21 | STROBE_MODE | - | 读取频闪模式 |
| 40 | READ | 127 | LS_WM_NVRAM_RDY | - | 读取无线标记NVRAM就绪 |
| 41 | READ | 148 | BUZZER_PERIOD | - | 读取蜂鸣器周期（重复） |
| 42 | READ | 83 | ACC_STATUS | - | 读取加速度计状态 |
| 43 | WRITE | 82 | ETHERNET_SPEED | 1 | 设置以太网速度 |
| 44 | READ | 151 | PTP_CONTROL | - | 读取PTP控制状态 |

**关键说明**:
- **seq 34 是最关键的命令之一**：WRITE ACQUISITION = 1。写入寄存器 46 (ACQUISITION) 值为 1，**启动相机采集模式**。在此之前，相机不会开始拍摄和发送图像数据。
- 其他 READ 命令是 `ftkEnumerateOptions()` 内部逻辑，读取各选项的当前值以构建选项映射表。

---

### 阶段八：设备详细配置（seq 45-82）

此阶段是 SDK 在设备检测后自动执行的详细配置，涵盖 PTP 时钟同步、图像采集参数、LED 控制、无线标记器等。

#### PTP 时钟同步配置（seq 45-46, 83-102）

| seq | 命令 | 寄存器ID | 名称 | 值 | 说明 |
|-----|------|----------|------|------|------|
| 45 | WRITE | 151 | PTP_CONTROL | 0 | 关闭PTP |
| 46 | READ | 117 | PAYLOAD_DATA_MASK | - | 读取数据载荷掩码 |
| 47 | WRITE | 117 | PAYLOAD_DATA_MASK | 7 | 设置数据掩码=7（bit0+bit1+bit2） |
| 83 | READ | 151 | PTP_CONTROL | - | 再次读取PTP状态 |
| 84 | WRITE | 151 | PTP_CONTROL | 1 | **启用PTP** |
| 85-86 | R/W | 117 | PAYLOAD_DATA_MASK | 1287 | 配置载荷掩码 |
| 87-88 | R/W | 151 | PTP_CONTROL | 1 | 确认PTP启用 |
| 89-92 | R/W | 153 | PTP_ANNOUNCE_TIMINGS | 1281 | PTP公告时序 |
| 93-94 | R/W | 154 | PTP_SYNC_TIMINGS | 0 | PTP同步时序 |
| 95-96 | R/W | 152 | PTP_DOM_NBR | 0 | PTP域号 |
| 97-98 | R/W | 153 | PTP_ANNOUNCE_TIMINGS | 1281 | 确认设置 |
| 99-102 | R/W | 151 | PTP_CONTROL | 1→0 | PTP控制最终配置 |

**说明**: PTP (Precision Time Protocol) 配置确保相机与电脑之间精确的时间同步。PAYLOAD_DATA_MASK 控制相机返回的数据类型（图像数据、分割数据等）。

#### EIO (外部I/O) 配置（seq 49-57, 125-155）

| seq | 命令 | 寄存器ID | 名称 | 值 | 说明 |
|-----|------|----------|------|------|------|
| 49 | WRITE | 75 | EIO_ENABLE | 1 | 启用外部I/O |
| 50 | WRITE | 76 | EXP_TIMESTAMP_ENABLE | 1 | 启用曝光时间戳 |
| 51 | WRITE | 117 | PAYLOAD_DATA_MASK | 263 | 配置数据掩码(0x107) |
| 54 | WRITE | 78 | EIO_1_RX_TX_CONTROL | 16 | 配置EIO端口1 |
| 57 | WRITE | 79 | EIO_2_RX_TX_CONTROL | 17 | 配置EIO端口2 |

**说明**: EIO 端口用于外部触发和同步信号。启用 EXP_TIMESTAMP_ENABLE 使每帧携带精确的曝光时间戳。

#### 图像采集参数配置（seq 59-65）

| seq | 命令 | 寄存器ID | 名称 | 值 | 说明 |
|-----|------|----------|------|------|------|
| 59 | WRITE | 47 | CAM0_THRESHOLD | 200 | 左相机检测阈值=200 |
| 60 | WRITE | 48 | CAM1_THRESHOLD | 200 | 右相机检测阈值=200 |
| 61 | READ | 15 | US_STR | - | 读取频闪周期 |
| 62 | WRITE | 9 | US_EXP_TARGET | 130 | 曝光目标=130μs |
| 63 | WRITE | 14 | US_STR_TARGET | 130 | 频闪目标=130μs |
| 64 | WRITE | 21 | STROBE_MODE | 0 | 频闪模式=0(正常) |
| 65 | WRITE | 10 | US_ALT_EXP_TARGET | 130 | 备用曝光目标=130μs |

**关键说明**: 这些是**相机端图像采集的核心参数**：
- **CAM0/CAM1_THRESHOLD = 200**：设置左右相机的像素亮度检测阈值。高于此阈值的像素被视为可能的小球反光点。
- **US_EXP_TARGET = 130**：设置曝光时间目标为 130 微秒。
- 这些参数决定了相机如何拍摄图像，但**不涉及图像处理算法**。

#### 设备功能设置（seq 66-82）

| seq | 命令 | 寄存器ID | 名称 | 值 | 说明 |
|-----|------|----------|------|------|------|
| 66 | READ | 21 | STROBE_MODE | - | 验证频闪模式 |
| 67 | WRITE | 84 | PICTURE_SZ_CUTOFF | 2228288 | 图像大小截止值 |
| 68 | WRITE | 86 | S_FREQ_PCK_SHOCK | 0 | 震动检测频率 |
| 69 | WRITE | 74 | UART_TSTP_DATA_RATE | 25 | UART时间戳数据率 |
| 70 | WRITE | 16 | US_TRI_START | 0 | 三角测量开始时间 |
| 71 | WRITE | 17 | US_TRI_TARGET | 0 | 三角测量目标时间 |
| 72 | READ | 17 | US_TRI_TARGET | - | 验证设置 |
| 73 | WRITE | 77 | CONT_TIMESTAMP_DELAY | 0 | 连续时间戳延迟 |
| 74 | WRITE | 109 | LASER0_ENABLE | 0 | **关闭激光器0** |
| 75 | WRITE | 110 | LASER1_ENABLE | 0 | **关闭激光器1** |
| 76 | WRITE | 148 | BUZZER_PERIOD | 488 | 蜂鸣器周期=488 |
| 77 | WRITE | 149 | BUZZER_DURATION | 5 | 蜂鸣器持续时间=5 |
| 78 | WRITE | 89 | USER_LED_RED | 0 | LED红色=0 |
| 79 | WRITE | 90 | USER_LED_GREEN | 0 | LED绿色=0 |
| 80 | WRITE | 91 | USER_LED_BLUE | 0 | LED蓝色=0 |
| 81 | WRITE | 92 | USER_LED_FREQUENCY | 0 | LED频率=0 |
| 82 | WRITE | 93 | USER_LED_ON | 0 | LED关闭 |

**说明**: 
- **PICTURE_SZ_CUTOFF = 2228288 (0x220040)**：当压缩图像超过此大小时不发送。
- **US_TRI_START/TARGET = 0**：三角测量时序参数（这是相机端的**硬件触发时序**，不是算法）。
- **LASER0/1_ENABLE = 0**：关闭结构光激光器（被动标记跟踪不需要激光）。

---

### 阶段九：无线标记器配置（seq 103-123）

| seq | 命令 | 寄存器ID | 名称 | 值 | 说明 |
|-----|------|----------|------|------|------|
| 103 | READ | 117 | PAYLOAD_DATA_MASK | - | 读取当前数据掩码 |
| 104 | WRITE | 117 | PAYLOAD_DATA_MASK | 263 | 设置掩码=0x107 |
| 105 | READ | 121 | LS_WM_SELECTED_API | - | 读取无线标记API版本 |
| 106 | WRITE | 121 | LS_WM_SELECTED_API | 33 | 选择API版本=33 |
| 107 | READ | 117 | PAYLOAD_DATA_MASK | - | |
| 108 | WRITE | 136 | LS_WM_IR_LEDS_MASK | 15 | 红外LED掩码=0x0F(所有LED) |
| 109 | WRITE | 135 | LS_WM_MASK | 0 | 无线标记掩码 |
| 110 | WRITE | 134 | LS_WM_FIRE_LEDS | 0 | LED触发 |
| 111 | WRITE | 123 | LS_WM_AUTODISC_US | 0 | 自动发现间隔=0 |
| 112 | WRITE | 130 | LS_WM_AUTOSTATUS_US | 10000 | 自动状态报告=10000μs |
| 113-118 | R/W | 117,121 | PAYLOAD_DATA_MASK, LS_WM_SELECTED_API | 263,1 | 交替配置 |
| 119-120 | R/W | 121 | LS_WM_SELECTED_API | 1 | 最终设置 |
| 121 | WRITE | 122 | LS_WM_SESSION_ID | 0 | 会话ID=0 |
| 122-123 | R/W | 121 | LS_WM_SELECTED_API | 0 | 重置API选择 |

**说明**: 这些命令配置无线标记器（Wireless Marker）子系统。fusionTrack 支持有源无线标记工具，这些命令设置 IR LED 控制、自动发现和状态报告。

---

### 阶段十：ftkSetRigidBody 几何体设置阶段（seq 124-155）

此阶段对应 `stereo2_AcquisitionBasic.cpp` 第 441 行：
```cpp
if ( ftkError::FTK_OK != ftkSetRigidBody( lib, sn, &geom ) )
```

| seq | 命令 | 寄存器ID | 名称 | 值 | 说明 |
|-----|------|----------|------|------|------|
| 124 | READ | 117 | PAYLOAD_DATA_MASK | - | 读取数据掩码 |
| 125 | WRITE | 75 | EIO_ENABLE | 1 | 启用EIO |
| 126 | WRITE | 76 | EXP_TIMESTAMP_ENABLE | 1 | 启用时间戳 |
| 127 | WRITE | 117 | PAYLOAD_DATA_MASK | 263 | 数据掩码=0x107 |
| 128-151 | R/W | 75,78,79 | EIO_ENABLE, EIO_1/2_RX_TX_CONTROL | 多值 | 配置EIO通道 |
| 152 | READ | 117 | PAYLOAD_DATA_MASK | - | |
| 153 | WRITE | 75 | EIO_ENABLE | 0 | 关闭EIO |
| 154 | WRITE | 76 | EXP_TIMESTAMP_ENABLE | 0 | 关闭时间戳 |
| 155 | WRITE | 117 | PAYLOAD_DATA_MASK | 7 | 恢复数据掩码=7 |

**关键说明**:
- `ftkSetRigidBody()` 并**没有**将几何体数据发送给相机！
- 仔细观察：这些命令全部是 EIO 端口和数据掩码的配置/恢复。
- **几何体数据（geometry072.ini 中的小球坐标）仅存储在 SDK 库的本地内存中**。
- SDK 在 PC 端使用这些几何体坐标与三维重建的散点进行匹配，整个匹配算法在 PC 端执行。
- 这里的 EIO 配置可能与 `ftkSetRigidBody()` 内部的某些验证流程有关。

---

### 阶段十一：断开连接（seq 156）

| seq | 命令 | 大小 | 说明 | API |
|-----|------|------|------|-----|
| 156 | DISCONNECT (0x0007) | 12字节 | 断开与相机的连接 | `ftkClose(&lib)` |

对应 `stereo2_AcquisitionBasic.cpp` 第 480 行：
```cpp
if ( ftkError::FTK_OK != ftkClose( &lib ) )
```

---

### 持续帧请求（散布在整个会话中）

在 seq 11-15 之后，SDK 库内部持续发送 `FRAME_REQUEST (0x0005)` 来确认已处理的帧。观察到的帧请求分布：

- **seq 11**: 在包 #19494, #19847, #20269, #20706, #21162 重复发送（5次，相同 seq），间隔约 400-500 包 → 说明 SDK 使用 **UDP 可靠传输机制**，重复发送直到收到确认。
- **seq 12**: 在包 #21518-#21526 重复发送（5次），间隔很小 → 说明在等待相机 ACK。

---

## 4. 命令类型统计

| 命令类型 | 出现次数(去重) | 占比 | 说明 |
|----------|----------------|------|------|
| 0x0001 READ_REGISTER | 57 | 36.5% | 读取设备寄存器 |
| 0x0002 WRITE_REGISTER | 93 | 59.6% | 写入设备寄存器 |
| 0x0005 FRAME_REQUEST | 5 | 3.2% | 帧流控确认 |
| 0x0007 DISCONNECT | 1 | 0.6% | 断开连接 |
| **合计** | **156** | | |

---

## 5. 相机返回数据流统计

| 流标识 | 包数 | 说明 |
|--------|------|------|
| 0x1001 | 13 | 配置数据流（设备选项定义，INI格式文本） |
| 0x1003 | 15,343 | **左相机压缩图像数据流** |
| 0x1004 | 14,886 | **右相机压缩图像数据流** |
| 0x1006 | 1 | 状态/事件通知流 |

**关键观察**: 相机仅返回 **压缩的图像原始数据** (0x1003/0x1004) 和 **配置信息** (0x1001)。没有任何流包含已处理的检测结果、3D 坐标或工具姿态数据。

---

## 6. 关键结论

### 6.1 PC 端发送给相机的消息 100% 是控制命令

从 156 条唯一命令的完整分析可以确认：
1. **没有任何一条命令包含图像处理算法参数**（如检测阈值是硬件级的像素亮度阈值，不是算法参数）
2. **没有发送几何体数据到相机**（`ftkSetRigidBody` 仅触发了 EIO 配置，几何体存储在 PC 本地）
3. **没有"请求识别结果"的命令**（`FRAME_REQUEST` 仅是流控 ACK，不是"请求处理"）
4. **所有命令可分为**：设备发现、寄存器读写、流控确认、断开连接

### 6.2 相机仅发送原始图像数据

相机的 30,380 个响应包中：
- 99.96% 是 0x1003/0x1004 **原始压缩图像数据**
- 0.04% 是 0x1001 **配置文本**
- 没有任何流包含已处理的跟踪结果

### 6.3 确认了"图像处理在 PC 端执行"

本次解析从通讯数据层面再次确认了 `analysis_fusionTrack_processing_pipeline.md` 中的结论：
- 相机端只负责：图像采集、压缩、传输
- PC 端 SDK 库负责：图像解压、blob 检测、2D→3D 三角测量、几何体匹配、姿态计算

---

## 7. 完整消息速查表

下表列出所有涉及的设备寄存器选项 ID 及其含义：

| ID | 十六进制 | 名称 | 类型 | 本次用途 |
|----|----------|------|------|----------|
| 0 | 0x0000 | SN_LOW | RO | 读取序列号低位 |
| 1 | 0x0001 | SN_HIGH | RO | 读取序列号高位 |
| 2 | 0x0002 | ONCHIP_ADDR | RO | 芯片地址 |
| 3 | 0x0003 | CHUNK_GET | WO | 请求配置块 |
| 4 | 0x0004 | CHUNK_RDY | RO | 配置块就绪 |
| 5 | 0x0005 | PACKET_MAX_SIZE | RW | 最大包大小 |
| 9 | 0x0009 | US_EXP_TARGET | RW | 曝光目标(μs) |
| 10 | 0x000A | US_ALT_EXP_TARGET | RW | 备用曝光(μs) |
| 14 | 0x000E | US_STR_TARGET | RW | 频闪目标(μs) |
| 15 | 0x000F | US_STR | RO | 频闪周期 |
| 16 | 0x0010 | US_TRI_START | RW | 三角测量开始 |
| 17 | 0x0011 | US_TRI_TARGET | RW | 三角测量目标 |
| 21 | 0x0015 | STROBE_MODE | RW | 频闪模式 |
| 46 | 0x002E | ACQUISITION | WO | **采集控制** |
| 47 | 0x002F | CAM0_THRESHOLD | RW | 左相机阈值 |
| 48 | 0x0030 | CAM1_THRESHOLD | RW | 右相机阈值 |
| 54 | 0x0036 | ACC_RANGE | RO | 加速度计量程 |
| 62 | 0x003E | ELEC_VERSION | RO | 电子版本 |
| 63 | 0x003F | DEVICE_VERSION | RO | 设备版本 |
| 64 | 0x0040 | DEVICE_ACCESS | RO | 访问权限 |
| 74 | 0x004A | UART_TSTP_DATA_RATE | RW | UART数据率 |
| 75 | 0x004B | EIO_ENABLE | RW | EIO使能 |
| 76 | 0x004C | EXP_TIMESTAMP_ENABLE | RW | 时间戳使能 |
| 77 | 0x004D | CONT_TIMESTAMP_DELAY | RW | 时间戳延迟 |
| 78 | 0x004E | EIO_1_RX_TX_CONTROL | RW | EIO端口1控制 |
| 79 | 0x004F | EIO_2_RX_TX_CONTROL | RW | EIO端口2控制 |
| 82 | 0x0052 | ETHERNET_SPEED | RW | 以太网速度 |
| 83 | 0x0053 | ACC_STATUS | RO | 加速度计状态 |
| 84 | 0x0054 | PICTURE_SZ_CUTOFF | RW | 图像大小截止 |
| 86 | 0x0056 | S_FREQ_PCK_SHOCK | RW | 震动频率 |
| 89 | 0x0059 | USER_LED_RED | RW | LED红 |
| 90 | 0x005A | USER_LED_GREEN | RW | LED绿 |
| 91 | 0x005B | USER_LED_BLUE | RW | LED蓝 |
| 92 | 0x005C | USER_LED_FREQUENCY | RW | LED频率 |
| 93 | 0x005D | USER_LED_ON | RW | LED开关 |
| 109 | 0x006D | LASER0_ENABLE | RW | 激光器0 |
| 110 | 0x006E | LASER1_ENABLE | RW | 激光器1 |
| 117 | 0x0075 | PAYLOAD_DATA_MASK | RW | 数据掩码 |
| 121 | 0x0079 | LS_WM_SELECTED_API | RW | 无线标记API |
| 122 | 0x007A | LS_WM_SESSION_ID | RW | 会话ID |
| 123 | 0x007B | LS_WM_AUTODISC_US | RW | 自动发现间隔 |
| 127 | 0x007F | LS_WM_NVRAM_RDY | RO | NVRAM就绪 |
| 130 | 0x0082 | LS_WM_AUTOSTATUS_US | RW | 自动状态间隔 |
| 134 | 0x0086 | LS_WM_FIRE_LEDS | WO | LED触发 |
| 135 | 0x0087 | LS_WM_MASK | RW | 标记掩码 |
| 136 | 0x0088 | LS_WM_IR_LEDS_MASK | RW | IR LED掩码 |
| 144 | 0x0090 | PLAIN_NUMBER_LOW | RW | 认证明文低位 |
| 148 | 0x0094 | BUZZER_PERIOD | RW | 蜂鸣器周期 |
| 149 | 0x0095 | BUZZER_DURATION | RW | 蜂鸣器持续 |
| 151 | 0x0097 | PTP_CONTROL | RW | PTP控制 |
| 152 | 0x0098 | PTP_DOM_NBR | RW | PTP域号 |
| 153 | 0x0099 | PTP_ANNOUNCE_TIMINGS | RW | PTP公告时序 |
| 154 | 0x009A | PTP_SYNC_TIMINGS | RW | PTP同步时序 |

---

## 8. 与 stereo2_AcquisitionBasic.cpp 代码的精确对应

| 代码行 | SDK API 调用 | 对应 PC→相机消息 |
|--------|-------------|-----------------|
| 372 | `ftkInitExt(cfgFile, &buffer)` | 库初始化（本地操作，无网络消息） |
| 382 | `retrieveLastDevice(lib,...)` → `ftkEnumerateDevices()` | 广播包 `"0:3\0"` + seq 1-10 (设备发现+配置读取) |
| 387 | `ftkEnumerateOptions(lib, sn, ...)` | seq 19-44 (枚举选项+读取值) |
| 441 | `ftkSetRigidBody(lib, sn, &geom)` | seq 124-155 (EIO配置，**几何体不发送**) |
| 178 | `ftkSetFrameOptions(false, false, 16, 16, 0, 16, frame)` | 纯本地操作，无网络消息 |
| 196 | `ftkGetLastFrame(lib, sn, frame, 100)` | `FRAME_REQUEST (0x0005)` 帧流控确认 |
| 480 | `ftkClose(&lib)` | seq 156: `DISCONNECT (0x0007)` |

**注意**: `ftkSetFrameOptions()` 和 `ftkCreateFrame()` 是**纯本地操作**，不会产生任何网络流量。它们只是在 PC 端分配内存缓冲区用于接收和处理帧数据。
