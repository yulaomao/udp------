# fusionTrack 双目相机图像处理流水线深度分析报告

## 核心结论

**从图像采集到小球识别、三维重建、再到工具（Marker）识别，这一完整的处理流水线是在 PC 端（电脑端）通过 SDK 库执行的，而非在相机端执行。相机端仅负责采集近红外图像并进行初步的 ROI（感兴趣区域）稀疏图像压缩编码，然后通过 UDP 协议将左右目的原始检测数据发送给 PC。**

以下从七个独立维度进行严谨论证。

---

## 论证一：API 头文件 `ftkInterface.h` 的函数签名与文档注释

### 1.1 `ftkGetLastFrame` —— 一次调用完成全部处理

```c
ATR_EXPORT ftkError ftkGetLastFrame(ftkLibrary lib,
                                     uint64 sn,
                                     ftkFrameQuery* frameQueryInOut,
                                     uint32 timeoutMS);
```

该函数的返回错误码列表揭示了其**内部执行了哪些算法步骤**（摘自 `ftkInterface.h:2087-2118`）：

| 错误码 | 含义 | 说明 |
|--------|------|------|
| `FTK_ERR_SEG_OVERFLOW` | "overflow occurred during **image segmentation**" | SDK 内部执行了图像分割 |
| `FTK_ERR_ALGORITHMIC_WALLTIME` | "**processing time** exceed the set walltime for the current frame" | SDK 有算法处理耗时计时 |
| `FTK_ERR_IMG_DEC` | "a picture **cannot be decompressed**" | SDK 需要解压相机发来的压缩图像 |
| `FTK_ERR_IMG_FMT` | "gotten picture data are not compatible with the SDK" | SDK 接收并解读原始图像数据 |
| `FTK_ERR_INTERNAL` | "**triangulation** or the **marker matcher** class are not properly initialised" | SDK 内部有三角化模块和标记体匹配模块 |
| `FTK_ERR_IMPAIRING_CALIB` | "calibration stored in device prevents **triangulation**" | 三角化依赖标定参数 |

**关键论证**: 如果这些算法（图像分割、三角化、标记体匹配）是在相机端执行的，那么 `ftkGetLastFrame` 的错误码不可能包含 `FTK_ERR_SEG_OVERFLOW`、`FTK_ERR_ALGORITHMIC_WALLTIME` 等指示算法处理失败的错误。这些错误码只有在 SDK 库本身（即 PC 端的 DLL）执行这些算法时才有意义。

### 1.2 `ftkReprocessFrame` —— 明确在 PC 端重新处理帧数据

```c
ATR_EXPORT ftkError ftkReprocessFrame(ftkLibrary lib, uint64 sn, ftkFrameQuery* frameQueryInOut);
```

文档注释（`ftkInterface.h:2125-2163`）明确说明：

> "Frame reprocessing allows the user to reprocess only a part of the contained data, i.e. the markers, or 3D and markers. The current implementation does **not** support a built-in reprocessing of the pictures, meaning that **pixel reprocessing must be done by a user defined function**."

**关键论证**: 此函数允许用户修改 2D 原始数据后，在 PC 端重新执行三角化和标记体匹配。如果三角化和标记体匹配是在相机端执行的，那么在 PC 端根本不可能有"重处理"功能，因为 PC 端没有这些算法的实现。

### 1.3 `ftkTriangulate` 和 `ftkReprojectPoint` —— PC 端三角化 API

```c
ATR_EXPORT ftkError ftkTriangulate(ftkLibrary lib, uint64 sn,
                                    const ftk3DPoint* leftPixel,
                                    const ftk3DPoint* rightPixel,
                                    ftk3DPoint* outPoint);

ATR_EXPORT ftkError ftkReprojectPoint(ftkLibrary lib, uint64 sn,
                                       const ftk3DPoint* inPoint,
                                       ftk3DPoint* outLeftData,
                                       ftk3DPoint* outRightData);
```

**关键论证**: SDK 提供了独立的三角化和重投影函数，接受 2D 像素坐标输入，输出 3D 坐标。这是 PC 端执行三角化的直接证据——如果三角化是在相机端完成的，SDK 不需要也不应该暴露这样的接口。

---

## 论证二：`ftkFrameQuery` 数据结构的分层设计

`ftkFrameQuery` 结构体（`ftkInterface.h:1273-1397`）包含以下数据层次：

```
ftkFrameQuery
├── imageHeader          → 图像头 (时间戳、分辨率、像素格式)
├── imageLeftPixels      → 左目原始像素数据
├── imageRightPixels     → 右目原始像素数据
├── rawDataLeft[]        → 左目 2D blob 检测结果 (ftkRawData)
├── rawDataRight[]       → 右目 2D blob 检测结果 (ftkRawData)
├── threeDFiducials[]    → 三维基准点 (ftk3DFiducial) ← 三角化结果
├── markers[]            → 标记体/工具 (ftkMarker) ← 刚体匹配结果
└── events[]             → 事件数据
```

每一层都有独立的 `ftkQueryStatus` 状态字段和可选的启用/禁用开关。

### 关键证据：`ftkSetFrameOptions` 的参数

```c
ATR_EXPORT ftkError ftkSetFrameOptions(bool pixels,
                                        uint32 eventsCount,
                                        uint32 leftRawCount,
                                        uint32 rightRawCount,
                                        uint32 threeDCount,
                                        uint32 markersCount,
                                        ftkFrameQuery* frame);
```

**用户必须预先分配每一层的缓冲区大小**。如果某层缓冲区为 0，该层数据将不会被填充（状态为 `QS_WAR_SKIPPED`）。

**关键论证**: 如果 3D 坐标和 Marker 识别是在相机端完成的，则这些数据应该随着网络包一同传来。但事实上，用户可以选择只请求 rawData 而不请求 threeDFiducials 或 markers，或者只请求 markers 而不请求 rawData。这种灵活的按需填充机制只有在 SDK 本身执行处理时才有意义——SDK 可以根据用户分配的缓冲区来决定执行哪些算法步骤。

### `QS_REPROCESS` 状态码

```c
QS_REPROCESS = 10  // "This field is requested to be reprocessed."
```

`ftkQueryStatus` 中的 `QS_REPROCESS` 状态码用于标记某一层数据需要重新处理。`stereo34_ReprocessFrame.cpp` 中的代码：

```cpp
outFrame->threeDFiducialsStat = ftkQueryStatus::QS_REPROCESS;
outFrame->markersStat = ftkQueryStatus::QS_REPROCESS;
```

**关键论证**: 用户代码可以修改 rawData 后，将 threeDFiducials 和 markers 标记为需要重处理，然后调用 `ftkReprocessFrame`，SDK 会在 PC 端重新执行三角化和标记体匹配。这直接证明了处理管线在 PC 端执行。

---

## 论证三：网络通讯数据包分析（pcapng 抓包验证）

### 3.1 通讯流量特征

根据 `capture_summary.json` 的分析：

| 字段 | 值 |
|------|-----|
| 数据流方向 | **相机 → PC 单向** (172.17.1.7:3509 → 172.17.1.56:50316) |
| 总 UDP 包数 | 9452 |
| 相机→PC 包数 | 9445（占 99.9%） |
| PC→相机 包数 | **仅 1 个**（12 字节） |
| 传输持续时间 | 3.13 秒 |
| 总有效载荷 | 12,828,616 字节（约 12.2 MB） |

**关键论证**: 通讯几乎完全是**相机到 PC 的单向传输**。PC 仅发送了 1 个 12 字节的包（可能是初始握手/触发命令，前缀为 `0x07000600`）。如果处理结果是在相机端计算的，那么相机返回的数据应该包含 3D 坐标和旋转矩阵，但实际传输的数据流完全由左右目图像数据帧组成。

### 3.2 数据流标识与帧结构

根据 `decode_tracking_data.py` 的协议解析和 `capture_summary.json`：

| 数据流 | stream_tag | 描述 | 每帧大小 |
|--------|-----------|------|---------|
| 左目 | 0x1003 | 左目 ROI 检测数据 | 5,344 ~ 5,440 字节 |
| 右目 | 0x1004 | 右目 ROI 检测数据 | 6,592 ~ 6,704 字节 |

每帧由 4-5 个 UDP 分片组成，通过 24 字节私有头部中的 `frame_token` 和 `payload_offset` 重组。

**关键论证**: 网络中只有两路数据流（0x1003 和 0x1004），分别对应左右目传感器。**不存在第三路包含 3D 坐标或 Marker 信息的数据流**。如果处理是在相机端完成的，必然需要额外的数据流来传输 3D fiducial 和 marker 结果。

### 3.3 帧内容解码验证

`decode_tracking_data.py` 中对帧体的解码揭示了帧的具体内容：

```python
# 每帧结构
# 80 字节帧头 (内层帧头)
#   - magic/timestamp
#   - device_timestamp_us (设备运行时间)
#   - frame_counter
#   - sensor_flags (0x00=左目, 0xC0=右目)
# 帧体: ROI 稀疏图像编码 (16字节定长记录)
#   - 背景填充 0x80
#   - X 起始位置 + 灰度像素值
#   - 行段压缩格式
```

帧体是 **稀疏灰度图像数据**（ROI 编码），包含了传感器检测到的高亮红外反光点的像素信息。这些是最底层的原始图像数据，**绝非处理后的结果数据**。

### 3.4 帧大小分析

- 左目帧大小: 平均 ~5,383 字节（标准差 17.5）
- 右目帧大小: 平均 ~6,625 字节（标准差 18.2）
- 帧大小的微小波动说明内容是变长的 ROI 编码数据，与每帧检测到的 blob 数量和大小有关

对比一下，如果传输的是处理结果：
- 一个 `ftkMarker` 结构体仅约 92 字节（status 4 + id 4 + geometryId 4 + presenceMask 4 + fiducialCorresp 24 + rotation 36 + translation 12 + registrationError 4 = 92 字节）
- 一个 `ftk3DFiducial` 结构体仅 40 字节
- 三个小球的工具识别结果总共不超过 300 字节

但实际每帧传输了约 5-7 KB 的数据，正好与稀疏 ROI 图像数据的大小一致。

---

## 论证四：SDK DLL 库文件分析

### 4.1 库文件清单

```
lib/
├── atracsys_stk.lib          (6,478 KB) - 静态库
├── device_d.lib              (4,222 KB) - 设备通讯库(debug)
├── device_d.dll              (6,637 KB) - 设备通讯库(debug)
├── device_d.pdb              (15,312 KB) - 调试符号
├── fusionTrack64_d.lib       (86 KB)    - fusionTrack 导入库(debug)
├── fusionTrack64_d.dll       (8,543 KB) - fusionTrack 主库(debug)
├── fusionTrack64_d.pdb       (19,616 KB) - 调试符号
└── zlib1.dll                 (111 KB)   - zlib 压缩库
```

**关键论证**:

1. **`fusionTrack64_d.dll` (8.5 MB)** — 这是一个体积非常庞大的 DLL 库。如果它只是简单地接收相机端已处理好的结果数据，绝不需要 8.5 MB 的代码量。这个大小表明它包含了完整的图像处理管线：图像分割（segmentation）、blob 检测、立体匹配（stereo matching）、三角化（triangulation）、刚体匹配（rigid body matching/marker detection）、以及温度补偿等算法。

2. **`zlib1.dll`** — zlib 压缩库的存在说明 SDK 需要在 PC 端**解压**相机发来的压缩图像数据。相机传输的是压缩后的稀疏 ROI 数据，SDK 在 PC 端进行解压。

3. **`device_d.dll` (6.6 MB)** — 设备通讯库，负责 UDP 通讯、帧重组、设备发现等底层工作。

4. **`data/segmentation250.xml` (327 KB)** — 这是一个巨大的图像分割配置文件，存储在 PC 端的 SDK 数据目录中。如果分割是在相机端执行的，这个文件不需要存在于 PC 端。

---

## 论证五：示例代码的执行流程分析

### 5.1 `stereo2_AcquisitionBasic.cpp` —— 基础采集示例

该示例是题目中提到的抓包所对应的 demo。其核心流程为：

```
1. ftkInitExt()        → 初始化 SDK 库
2. ftkEnumerateDevices() → 发现设备（通过 UDP 广播）
3. ftkSetRigidBody()   → 注册几何体到 SDK（注意：是发给 SDK 而非相机）
4. ftkCreateFrame()    → 在 PC 端分配帧缓冲区
5. ftkSetFrameOptions() → 设置需要哪些数据层
6. while(...) {
     ftkGetLastFrame()  → SDK 内部完成: 接收图像→分割→匹配→三角化→Marker匹配
     // 直接访问 frame->markers[m].translationMM 和 frame->markers[m].rotation
   }
7. ftkDeleteFrame()    → 释放缓冲区
8. ftkClose()          → 关闭 SDK
```

**关键论证**: 从代码可见，`ftkGetLastFrame` 返回后，用户直接从 `frame->markers[]` 中读取 3D 位置和旋转矩阵。用户并没有调用任何独立的"三角化"或"匹配"函数。这意味着 `ftkGetLastFrame` 内部封装了从接收原始图像到输出最终 Marker 位置和旋转的**完整处理流水线**。

### 5.2 `stereo3_AcquisitionAdvanced.cpp` —— 数据层级追踪

此示例展示了完整的数据层级关系：

```cpp
// 从 Marker 追溯到 3D Fiducial，再追溯到 2D RawData
for (size_t n = 0; n < FTK_MAX_FIDUCIALS; n++) {
    uint32 fidId = frame->markers[m].fiducialCorresp[n];
    if (fidId != INVALID_ID) {
        ftk3DFiducial& fid = frame->threeDFiducials[fidId];
        // 3D 位置
        fid.positionMM.x, fid.positionMM.y, fid.positionMM.z
        // 追溯到左目 2D 检测
        ftkRawData& left = frame->rawDataLeft[fid.leftIndex];
        // 追溯到右目 2D 检测
        ftkRawData& right = frame->rawDataRight[fid.rightIndex];
    }
}
```

**关键论证**: `ftk3DFiducial` 的 `leftIndex` 和 `rightIndex` 分别指向左右目 rawData 中的对应检测点。这种索引关联关系是在 PC 端立体匹配过程中建立的——SDK 将左目 blob 和右目 blob 进行配对（利用极线约束），计算出 3D 位置，并记录下对应的左右目索引。

### 5.3 `stereo34_ReprocessFrame.cpp` —— 重处理示例

此示例是最关键的证据之一。它展示了：

```cpp
// 用户自定义的原始数据修改函数
ReprocessItemFunc oneCentiMetreDisplacement = [](ftkLibrary lib, ...) {
    // 修改 3D 位置 (加 10mm)
    pt.positionMM.x += 10.f;
    // 用 ftkReprojectPoint 在 PC 端计算新的 2D 投影
    ftkReprojectPoint(lib, sn, &pt.positionMM, &leftPt, &rightPt);
    // 更新 rawData 中的 2D 坐标
    out->rawDataLeft[pt.leftIndex].centerXPixels = leftPt.x;
    out->rawDataRight[pt.rightIndex].centerXPixels = rightPt.x;
    // 清除 3D 和 Marker 结果，要求重新计算
    out->threeDFiducialsCount = 0u;
    out->markersCount = 0u;
};

// 在 PC 端重新执行三角化和 Marker 匹配
ftkReprocessFrame(lib, sn, frame);
```

**关键论证**: 用户可以在 PC 端修改 2D 原始数据，然后要求 SDK **在 PC 端重新执行三角化和标记体匹配**。这不仅证明了这些算法运行在 PC 端，还证明了 SDK 库中包含了完整的算法实现。

---

## 论证六：`ftkOptions.h` 中的组件分类

`ftkOptions.h` 定义了设备选项所属的功能模块：

```c
TYPED_ENUM(uint8, ftkComponent) {
    FTK_DEVICE = 0,            // 设备通用选项
    FTK_DETECTOR = 1,          // 检测阶段选项 (PC 端)
    FTK_MATCH2D3D = 2,         // 2D-3D 匹配选项 (PC 端)
    FTK_DEVICE_DETECTOR = 3,   // 设备端检测选项
    FTK_DEVICE_MATCH2D3D = 4,  // 设备端 2D-3D 匹配选项
    FTK_DEVICE_WIRELESS = 5,   // 设备端无线选项
    FTK_DEVICE_DEVICE = 6,     // 设备端设备选项
    FTK_LIBRARY = 100          // 库选项 (PC 端)
};
```

**关键论证**:

1. `FTK_DETECTOR` (=1) 和 `FTK_MATCH2D3D` (=2) 是 **PC 端**（offboard）的检测和匹配选项。
2. `FTK_DEVICE_DETECTOR` (=3) 和 `FTK_DEVICE_MATCH2D3D` (=4) 是 **设备端**（onboard）的对应选项。
3. 这两套选项的存在说明系统支持**双模式**——默认在 PC 端处理，设备端处理是可选的。

常量 `FTK_OFFBOARD_TO_ONBOARD_OPTION_IDS_DIFFERENCE = 7000` 定义了 PC 端选项 ID 和设备端对应选项 ID 之间的偏移量，进一步证实了双模式架构。

### spryTrack 的特殊处理

`stereo3_AcquisitionAdvanced.cpp` 中的代码：

```cpp
// 仅对 spryTrack 设备类型
if (DEV_SPRYTRACK_180 == device.Type || DEV_SPRYTRACK_300 == device.Type) {
    ftkSetInt32(lib, sn, options["Enable embedded processing"], 1);
    ftkSetInt32(lib, sn, options["Enable images sending"], 0);
}
```

**关键论证**:
- **"Enable embedded processing"** 选项说明设备端处理是需要**显式启用**的，默认是关闭的。对于 fusionTrack（而非 spryTrack），这个选项甚至可能不存在。
- **"Enable images sending"** 可以设为 0（禁用图像传输），这只在设备端处理（embedded processing）启用后才有意义。
- 对于 fusionTrack 设备（题目中使用的设备类型），代码中**没有启用 embedded processing 的逻辑**，说明 fusionTrack 始终在 PC 端处理。

---

## 论证七：网络数据中不包含 3D 坐标或旋转矩阵的直接验证

### 7.1 数据包内容搜索

根据问题描述，输出的位置和旋转矩阵为：

```
位置: -301.218, 66.043, 1777.359
旋转矩阵:
  0.714, 0.014, 0.700
 -0.700, 0.031, 0.714
 -0.012, -0.999, 0.032
```

将这些 float 值转换为 IEEE 754 单精度浮点数的十六进制表示：

| 值 | 十六进制 (IEEE 754) |
|----|---------------------|
| -301.218 | C396 9DF4 |
| 66.043 | 4284 160F |
| 1777.359 | 44DE 12DF |
| 0.714 | 3F36 C8B4 |
| -0.700 | BF33 3333 |
| -0.999 | BF7F BE77 |

如果这些值是从相机端传输的，它们的二进制表示必然出现在网络数据包中。但根据 `decode_tracking_data.py` 对帧内容的完整解码，帧体中的数据是 ROI 稀疏图像编码（灰度像素值范围 0x00-0xFF，行段格式），**不包含任何浮点数据**。

### 7.2 帧头部分析

内层帧头（80 字节）包含的字段仅有：
- 时间戳（magic/unix timestamp, device_timestamp_us：设备自启动以来的运行时间，单位微秒）
- 帧计数器
- 传感器标志（左/右目标识）
- 与传感器配置相关的固定参数

**帧头中没有任何字段承载 3D 坐标或旋转矩阵。**

### 7.3 逻辑验证

每帧的帧大小（5-7 KB）随帧波动，但波动幅度仅约 ±100 字节（标准差 ~18 字节）。这种波动特征与 ROI 编码数据一致——不同帧中 blob 的大小和数量略有变化，导致编码长度有微小差异。

如果帧中包含了 3D 处理结果，帧大小应该是相对固定的（因为结构体大小固定），不会表现出这种与图像内容相关的波动特征。

---

## 综合结论

| 维度 | 证据 | 结论 |
|------|------|------|
| **API 函数签名** | `ftkGetLastFrame` 返回 SEG_OVERFLOW、ALGORITHMIC_WALLTIME 等算法处理错误 | 图像分割、三角化、标记体匹配在 SDK（PC端）执行 |
| **重处理 API** | `ftkReprocessFrame` 可在 PC 端重新执行三角化和标记体匹配 | 这些算法实现在 PC 端 DLL 中 |
| **三角化 API** | `ftkTriangulate` 直接在 PC 端从 2D 计算 3D | 三角化算法在 PC 端 |
| **数据结构** | `ftkFrameQuery` 按层分配缓冲区，各层独立可选 | PC 端按需执行各处理步骤 |
| **网络数据** | 仅两路流（左右目 ROI 图像），无 3D 数据流 | 相机仅传输原始图像数据 |
| **帧内容** | 帧体为 ROI 稀疏灰度编码，不含浮点数据 | 网络中未传输 3D 坐标或旋转矩阵 |
| **库文件** | SDK DLL 8.5 MB，含 zlib 解压，含 segmentation 配置 | PC 端具备完整处理能力 |
| **选项系统** | `FTK_DETECTOR` vs `FTK_DEVICE_DETECTOR` 双模式 | 默认 PC 端处理，设备端处理需显式启用 |
| **spryTrack 代码** | "Enable embedded processing" 仅对 spryTrack 启用 | fusionTrack 始终在 PC 端处理 |
| **数据流向** | 相机→PC 9445 包，PC→相机 仅 1 包 | 完全单向传输，无结果回传 |

### 最终裁定

**fusionTrack 双目相机的完整数据处理流水线如下：**

```
┌──────────────────────────────────┐      UDP 网络传输      ┌──────────────────────────────────────────────┐
│         相机端 (fusionTrack)       │  ──────────────────→  │              PC 端 (SDK DLL)                   │
│                                    │   左/右目 ROI 数据    │                                                │
│  1. 近红外图像采集                  │                       │  1. 接收并重组 UDP 分片                        │
│  2. 初级 ROI 检测 (硬件/FPGA)      │                       │  2. 解压/解码 ROI 图像数据                     │
│  3. ROI 稀疏编码压缩               │                       │  3. 图像分割 (Segmentation) → ftkRawData      │
│  4. UDP 分片发送                   │                       │  4. 立体匹配 + 三角化 → ftk3DFiducial          │
│                                    │                       │  5. 刚体匹配 (Marker Detection) → ftkMarker   │
│                                    │                       │  6. 输出位置 + 旋转矩阵                        │
└──────────────────────────────────┘                        └──────────────────────────────────────────────┘
```

从图像到小球识别、到三维重建、到工具识别，这些步骤均在 **PC 端通过 fusionTrack SDK 库（fusionTrack64_d.dll / device_d.dll）** 执行，而非在相机端执行。相机端仅负责近红外图像的采集和初级 ROI 检测数据的传输。

---

## 附录：spryTrack 的特殊情况

需要指出的是，对于 **spryTrack** 系列设备（USB 接口，非以太网），存在 "embedded processing"（设备端嵌入式处理）选项，可以将部分处理下放到设备端。但这是 spryTrack 特有的功能，且需要显式启用。对于 **fusionTrack** 系列设备（以太网接口，本仓库的分析对象），所有处理均在 PC 端执行。

SDK 选项系统通过 `FTK_OFFBOARD_TO_ONBOARD_OPTION_IDS_DIFFERENCE = 7000` 这个偏移常量来区分 PC 端选项和设备端选项，但在 fusionTrack 的正常工作模式下，仅使用 PC 端选项（`FTK_DETECTOR` 和 `FTK_MATCH2D3D`）。
