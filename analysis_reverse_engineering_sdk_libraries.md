# fusionTrack SDK 二进制库逆向工程分析报告

## 核心结论

**"3D坐标计算、工具识别、变换计算"这三项核心算法 100% 在 `fusionTrack64.dll` (2.7 MB) 中执行**，运行在 PC 端。`device64.dll` (913 KB) 仅负责 UDP 网络通讯、设备发现和原始图像接收。相机硬件端不执行任何高层算法。

---

## 1. DLL 架构总览

### 1.1 核心 DLL 功能分层

```
┌─────────────────────────────────────────────────────────────────┐
│                    应用程序 (ftk2_AcquisitionBasic64.exe 等)       │
├─────────────────────────────────────────────────────────────────┤
│  fusionTrack64.dll (2.7 MB) — 核心算法引擎                        │
│  ┌──────────────┬──────────────┬──────────────┬──────────────┐  │
│  │ 图像解压缩    │ 图像分割      │ 三角测量      │ 工具匹配      │  │
│  │ PictureComp- │ SegmenterV21 │ StereoCame-  │ MatchMarkers │  │
│  │ ressor       │              │ raSystem     │              │  │
│  └──────────────┴──────────────┴──────────────┴──────────────┘  │
│  ┌──────────────┬──────────────┬──────────────┬──────────────┐  │
│  │ 几何体管理    │ 标定数据管理   │ 温度补偿      │ 特征检测      │  │
│  │ Geometry-    │ StereoProvi- │ Temperature- │ Feature-     │  │
│  │ ReaderBase   │ derV0-V3     │ Compensation │ HandlingV1   │  │
│  └──────────────┴──────────────┴──────────────┴──────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│  device64.dll (913 KB) — 设备通讯层                               │
│  ┌──────────────┬──────────────┬──────────────┬──────────────┐  │
│  │ UDP 传输      │ 图像接收      │ 心跳/看门狗   │ 设备配置      │  │
│  │ UdpTransport │ ImageProces- │ Heartbeat/   │ NetworkCfg   │  │
│  │              │ sor          │ Watchdog     │ Reader       │  │
│  └──────────────┴──────────────┴──────────────┴──────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│  操作系统 (WSOCK32.dll / WS2_32.dll — UDP 套接字)                  │
├─────────────────────────────────────────────────────────────────┤
│  fusionTrack 双目相机硬件 (仅发送压缩的 ROI 稀疏图像)               │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 DLL 文件清单与功能

| DLL | 大小 | 功能 | 角色 |
|-----|------|------|------|
| **fusionTrack64.dll** | 2.7 MB | 核心算法引擎：图像分割、三角化、工具匹配 | **算法核心** |
| **device64.dll** | 913 KB | 设备通讯：UDP、图像接收、设备管理 | 通讯层 |
| calibrationClient64.dll | 123 KB | 标定客户端 | 辅助 |
| dump2cams64.dll | 92 KB | 帧数据转 XML 导出 | 辅助 |
| opencv_world450.dll | 59 MB | OpenCV 4.5.0（仅 demo 使用） | UI/可视化 |
| fileSignature64.dll | 271 KB | 文件签名验证 | 安全 |
| capi.dll | 70 KB | OpenSSL 引擎（认证） | 安全 |

---

## 2. fusionTrack64.dll 逆向分析

### 2.1 导出函数（62 个公开 API）

通过 PE 文件分析提取的完整导出函数列表：

```
=== 核心帧处理 ===
ftkGetLastFrame        → 获取最新帧（内部执行完整处理管线）
ftkReprocessFrame      → 重新处理帧数据（PC 端三角化+匹配）
ftkCreateFrame         → 分配帧缓冲区
ftkDeleteFrame         → 释放帧缓冲区
ftkSetFrameOptions     → 设置帧数据选项
ftkSetAdvancedFrameOption → 高级帧选项

=== 3D 几何计算 ===
ftkTriangulate         → 2D→3D 三角测量（左右目 2D 点→3D 坐标）
ftkReprojectPoint      → 3D→2D 重投影（3D 坐标→左右目像素）

=== 几何体/工具管理 ===
ftkSetGeometry         → 设置几何体（已弃用）
ftkSetRigidBody        → 设置刚体几何
ftkRegisterRigidBody   → 注册刚体
ftkRegisterRigidBodyFromFileName → 从文件注册刚体
ftkRegisterRigidBodyFromFileContents → 从内容注册刚体
ftkLoadRigidBodyFromFile → 加载刚体文件
ftkLoadRidigBodyInformation → 加载刚体信息
ftkSaveRigidBodyToFile → 保存刚体到文件
ftkClearGeometry       → 清除几何体
ftkClearRigidBody      → 清除刚体
ftkEnumerateGeometries → 枚举已注册几何体
ftkEnumerateRigidBodies → 枚举已注册刚体
ftkGetRigidBodyProperty → 获取刚体属性
ftkGeometryFileConversion → 几何体文件格式转换
loadInternalGeometry   → 加载内部几何体

=== 设备管理 ===
ftkInit / ftkInitExt   → 初始化 SDK
ftkClose               → 关闭 SDK
ftkEnumerateDevices    → 发现设备
ftkRemoveDevice        → 移除设备
ftkVersion             → SDK 版本
ftkStatusToString      → 状态码转字符串
ftkGetLastErrorString  → 最近错误信息

=== 选项/寄存器访问 ===
ftkGetInt32 / ftkSetInt32         → 整数选项读写
ftkGetFloat32 / ftkSetFloat32     → 浮点选项读写
ftkGetData / ftkSetData           → 二进制数据读写
ftkEnumerateOptions               → 枚举所有选项
ftkEnumerateRegisters             → 枚举寄存器
ftkGetAccessLevel / ftkSetAccessLevel → 访问级别

=== 其他 ===
ftkGetCameraHeader     → 获取相机头信息
ftkGetAccelerometerData → 获取加速度计数据
ftkGetRealTimeClock    → 获取实时时钟
ftkOpenDumpFile / ftkCloseDumpFile / ftkDumpFrame / ftkDumpInfo → 数据转储
ftkCreateEvent / ftkDeleteEvent → 事件管理
ftkMemRead / ftkMemWrite → 设备内存读写
ftkExtractFrameInfo / ftkGetFrameInfo → 帧信息提取
ftkGetTotalObjectNumber → 获取对象总数
```

### 2.2 从 device64.dll 导入的函数（27 个）

fusionTrack64.dll 调用 device64.dll 的以下函数：

```
devInit                → 初始化设备通讯层
devClose               → 关闭设备通讯
devEnumerateDevices    → 发现网络上的设备
devEnumerateOptions    → 枚举设备选项
devEnumerateRegisters  → 枚举设备寄存器

devGetInt32 / devGetInt32Check     → 读取设备整数选项
devSetInt32 / devSetInt32Check     → 写入设备整数选项
devGetFloat32 / devGetFloat32Check → 读取设备浮点选项
devSetFloat32 / devSetFloat32Check → 写入设备浮点选项
devGetData / devGetDataCheck       → 读取设备二进制数据
devSetData / devSetDataCheck       → 写入设备二进制数据

devLastImages          → 获取最新的原始压缩图像（核心数据入口）
devReleaseImages       → 释放图像缓冲区
devGetLastError        → 获取设备错误信息

devGetAcceleration     → 获取加速度数据
devGetRealTimeClock    → 获取设备时钟
devGetWirelessMarkerGeometry → 获取无线标记器几何体
devGetDeviceConfig     → 获取设备配置
devGetAccessLevel / devSetAccessLevel → 设备访问级别
devMemRead / devMemWrite → 设备内存读写
devRemoveDevice        → 移除设备

devCreateEvent / devDeleteEvent → 设备事件管理
```

**关键发现**: fusionTrack64.dll 通过 `devLastImages()` 从 device64.dll 获取原始压缩图像数据，然后在 PC 端执行所有后续处理。

### 2.3 C++ 类层次结构（RTTI 逆向）

从 fusionTrack64.dll 的 RTTI (Run-Time Type Information) 提取的 C++ 命名空间和类：

#### 2.3.1 图像分割模块 (`segmenter::measurement`)

```cpp
namespace segmenter {
namespace measurement {

// 分割器基类 — 模板化支持不同图像格式
template<ImageType, CompressedDataType, PixelType, IndexType, Version1, Version2>
class SegmenterBase;

// 具体实现 — V21 版本分割器
template<ImageType, CompressedDataType, PixelType, IndexType, Version>
class SegmenterV21;

// 行扫描线结构
template<PixelType, IndexType>
class Line;

// 实例化类型:
// SegmenterV21<Image<uint8,1>, CompressedDataV2<uint8>, uint8, uint32, v1>
//   — 8位灰度图 + V2压缩格式
// SegmenterV21<Image<uint8,1>, CompressedDataV3<uint8>, uint8, uint32, v2>
//   — 8位灰度图 + V3压缩格式
// SegmenterV21<Image<uint16,3>, CompressedDataV3<uint16>, uint16, uint64, v2>
//   — 16位RGB图 + V3压缩格式

}} // namespace
```

**逆向推断的分割算法核心逻辑**:
1. 接收 device64.dll 传来的压缩 ROI 数据
2. 使用 `PictureCompressor` 解压为灰度图像 (`Image<uint8,1>`)
3. `SegmenterV21` 执行行扫描线分割，提取 blob（亮点区域）
4. 对每个 blob 计算质心坐标 (`AdvancedCentroidGetSetter`)
5. 过滤条件: `BlobMinSurface`, `BlobMaxSurface`, `BlobMinAspectRatio`, `EdgeBlobs`
6. 输出: 左右目各自的 2D blob 列表 (`ftkRawData[]`)

#### 2.3.2 图像压缩/解压模块 (`compressor::measurement`)

```cpp
namespace compressor {
namespace measurement {

class PictureCompressorBase;

// V2 压缩格式 — 8位灰度
template<ImageType, CompressedDataV2<uint8>, uint8, Version>
class PictureCompressor;

// V3 压缩格式 — 8位灰度 (新版)
template<ImageType, CompressedDataV3<uint8>, uint8, Version>
class PictureCompressor;

// V3 压缩格式 — 16位 (高精度)
template<ImageType, CompressedDataV3<uint16>, uint16, Version>
class PictureCompressor;

}} // namespace
```

**关键**: 三个不同版本的压缩器对应不同固件版本的相机。相机端使用对应的压缩器编码 ROI 数据，PC 端 `PictureCompressor` 负责解压。

#### 2.3.3 立体视觉/三角测量模块 (`calibHandling::measurement`)

```cpp
namespace calibHandling {
namespace measurement {

// 标定数据提供者 — 工厂模式，支持多版本
class StereoProviderBase;       // 基类
class StereoProviderV0;         // 版本0
class StereoProviderV1;         // 版本1 
class StereoProviderV2;         // 版本2
class StereoProviderV3;         // 版本3 (最新)

// 立体插值器
class StereoInterpolatorBase;
class StereoInterpolatorV1;

}} // namespace

namespace measurement {

// 立体相机系统 — 执行实际三角化
class StereoCameraSystem;

// 基准点检测器
class FiducialDetector;

} // namespace
```

**逆向推断的三角化核心逻辑**:
1. `StereoProviderV3` 从设备内存读取标定参数（内参、外参、畸变）
2. `StereoInterpolatorV1` 基于温度对标定参数进行实时插值补偿
3. `StereoCameraSystem` 使用标定参数执行：
   - 极线匹配 (epipolar matching): 根据 `EpipolarMaxGetSetter` 阈值
   - 三角化 (triangulation): 从左右目匹配的 2D 点计算 3D 坐标
4. `FiducialDetector` 将三角化结果生成 `ftk3DFiducial` 数组

#### 2.3.4 工具识别/刚体匹配模块 (`markerReco::measurement`)

```cpp
namespace markerReco {
namespace measurement {

// 几何体扩展类 — 模板参数为支持的 fiducial 数量
class ftkGeometryExt;

// 特化几何体 — 支持 3~64 个 fiducial 的工具
template<int N>
class SpecialisedGeometryExt;
// N = 3,4,5,6,7,8,9,10,11,...,64

}} // namespace
```

**逆向发现**: SDK 支持从 3 个到 64 个 fiducial 点的工具几何体！RTTI 中包含了 `SpecialisedGeometryExt<3>` 到 `SpecialisedGeometryExt<64>` 的完整模板实例化。这是编译时多态——每种 fiducial 数量的匹配算法在编译时就已特化，以获得最优性能。

**逆向推断的工具匹配核心逻辑** (来自源路径 `ftkMatchMarker.cpp`):

```
类: MatchMarkers (ftkMatchMarker.cpp)

核心方法:
  _gatherTrianglesForFourPiMarkers(geometryID, triangleID)
  → 构建 fiducial 点之间的三角形拓扑

算法流程:
  1. 从 FiducialDetector 获取当前帧的 3D fiducial 列表
  2. 对每个已注册的 geometry (SpecialisedGeometryExt<N>):
     a. 在 3D fiducial 中搜索匹配的三角形组合
     b. 使用距离容差 (MatchingToleranceGetSetter) 进行软匹配
     c. 允许缺失点 (MissingPointsMaxGetSetter)
     d. 执行刚体配准 (registration):
        - 计算最优旋转矩阵 R 和平移向量 t
        - 约束条件: RegistrationMaxGetSetter (最大配准误差)
     e. 斜率补偿 (ObliquityCompensationEnableGetSetter)
     f. 4π 标记器特殊处理 (FourPiAngleHandlingGetSetter)
  3. 输出: ftkMarker[] (包含 rotation[3][3] + translationMM[3])
  
超时控制:
  MarkerRecoWalltimeGetSetter → 最大处理时间限制
```

#### 2.3.5 数学库 (`math::measurement`)

```cpp
namespace math {
namespace measurement {

template<typename T, int Rows>
class ColVector;    // 列向量 (2D, 3D, 4D)

template<typename T, int Rows>
class RowVector;    // 行向量

template<typename T, int Rows, int Cols>
class Matrix;       // 矩阵 (最大到 64×2, 3×64 等)

template<typename T, int D1, int D2>
class Tensor;       // 张量 (最大到 64×64)

template<typename T, int Rows, int Cols>
class TensorComputingCache;   // 计算缓存

}} // namespace
```

**关键**: Matrix 模板实例化范围为 `Matrix<double, 3, N>` 其中 N 从 3 到 64——对应 3D 坐标矩阵（3行×N列，每列一个 fiducial 的 XYZ 坐标）。

#### 2.3.6 特征处理模块 (`feature::measurement`)

```cpp
namespace feature {
namespace measurement {

class FeatureHandlingBase;
class FeatureHandlingV1;
class FeatureVersionAnalyser;

}} // namespace
```

### 2.4 完整处理管线（逆向重建）

```
fusionTrack64.dll::ftkGetLastFrame() 内部执行流程:
═══════════════════════════════════════════════════

Step 1: 获取原始图像
  ┌──────────────────────────────────────┐
  │ device64.dll::devLastImages()         │
  │   → 从 UDP 缓冲区取出最新的压缩帧     │
  │   → 返回左/右目 CompressedDataV2/V3   │
  └────────────────┬─────────────────────┘
                   ▼
Step 2: 图像解压缩
  ┌──────────────────────────────────────┐
  │ PictureCompressor::decompress()       │
  │   → 将 ROI 稀疏编码解压为完整灰度图   │
  │   → 支持 V2(8bit) / V3(8/16bit)     │
  │   → 错误: FTK_ERR_IMG_DEC           │
  └────────────────┬─────────────────────┘
                   ▼
Step 3: 图像分割 (Segmentation)
  ┌──────────────────────────────────────┐
  │ SegmenterV21::segment()               │
  │   → 行扫描线检测亮点 blob             │
  │   → 过滤: MinSurface, MaxSurface     │
  │   → 计算质心: AdvancedCentroid        │
  │   → 像素权重: PixelWeight             │
  │   → 错误: FTK_ERR_SEG_OVERFLOW       │
  │   → 输出: rawDataLeft[], rawDataRight[]│
  └────────────────┬─────────────────────┘
                   ▼
Step 4: 温度补偿
  ┌──────────────────────────────────────┐
  │ TemperatureCompensation               │
  │   → 读取设备温度传感器数据             │
  │   → StereoInterpolatorV1 插值标定参数 │
  │   → 错误: FTK_ERR_COMP_ALGO          │
  │   → 警告: FTK_WAR_TEMP_LOW/HIGH      │
  └────────────────┬─────────────────────┘
                   ▼
Step 5: 立体匹配 + 三角化
  ┌──────────────────────────────────────┐
  │ StereoCameraSystem::triangulate()     │
  │   → 极线约束匹配 (EpipolarMax)       │
  │   → 三角测量: 2D+2D → 3D             │
  │   → 标定数据: StereoProviderV0~V3    │
  │   → 错误: FTK_ERR_IMPAIRING_CALIB    │
  │   → 输出: threeDFiducials[]           │
  └────────────────┬─────────────────────┘
                   ▼
Step 6: 工具识别 (Marker Recognition)
  ┌──────────────────────────────────────┐
  │ MatchMarkers::match()                 │
  │   → 遍历所有已注册 geometry            │
  │   → 三角形拓扑匹配                    │
  │   → 距离容差匹配: MatchingTolerance   │
  │   → 允许缺失点: MissingPointsMax      │
  │   → 刚体配准 (SVD): rotation + trans  │
  │   → 配准误差阈值: RegistrationMax     │
  │   → 斜率补偿: ObliquityCompensation   │
  │   → 超时控制: MarkerRecoWalltime      │
  │   → 错误: FTK_ERR_ALGORITHMIC_WALLTIME│
  │   → 输出: markers[]                   │
  └──────────────────────────────────────┘
```

---

## 3. device64.dll 逆向分析

### 3.1 导出函数（48 个）

```
=== 设备管理 ===
devInit                → 初始化设备通讯 (UDP)
devClose               → 关闭设备
devEnumerateDevices    → 广播发现设备
devRemoveDevice        → 移除设备

=== 图像数据 ===
devLastImages          → 获取最新原始压缩图像 (核心!)
devReleaseImages       → 释放图像缓冲区

=== 选项/寄存器 ===
devEnumerateOptions    → 枚举设备选项
devEnumerateRegisters  → 枚举设备寄存器
devGetInt32/devSetInt32         → 整数参数读写
devGetFloat32/devSetFloat32     → 浮点参数读写
devGetData/devSetData           → 二进制数据读写
devEnumerateDeviceConfig       → 枚举设备配置
devGetDeviceConfig/devSetDeviceConfig → 设备配置读写

=== 传感器数据 ===
devGetAcceleration     → 加速度计
devGetRealTimeClock    → 实时时钟

=== 无线标记器 ===
devGetWirelessMarkerGeometry → 获取无线标记器几何

=== 访问控制 ===
devGetAccessLevel/devSetAccessLevel → 访问权限
devMemRead/devMemWrite → 设备内存直接读写

=== 事件 ===
devCreateEvent/devDeleteEvent → 事件管理
devGetLastError        → 错误信息
```

### 3.2 C++ 类层次结构 (RTTI)

```cpp
// 核心处理类
class ImageProcessor;           // 图像数据处理（接收+解码）
class AccelerometerProcessor;   // 加速度计处理
class TemperatureProcessor;     // 温度处理
class ShockProcessor;           // 冲击检测
class WatchdogProcessor;        // 看门狗
class Heartbeat;                // 心跳维持

// 网络通讯
class EthernetAdapters;         // 网卡管理
class InternetProtocolV4Address; // IPv4地址
class SocketAddress;            // Socket地址

// UDP 传输
class UdpTransport;             // UDP收发 (devUdpTransport.cpp)
class UdpClient;                // UDP客户端 (winUdpClient.cpp)
class UdpCom;                   // UDP通讯 (winUdpCom.cpp)

// 数据缓冲
template<typename, typename, typename>
class ConcurrentBuffer;         // 并发图像缓冲区
class TripleBuffering<PictureData>; // 三重缓冲（左右目图像）
class DynBuffer;                // 动态缓冲区
class ImgDataArray;             // 图像数据数组

// 配置管理
class Config;                   // 配置
class NetworkCfgReader;         // 网络配置读取
class SchedulerCfgReader;       // 调度器配置
class SyncCfgReader;            // 同步配置
class EventDispatcher;          // 事件分发

// 设备信息
struct devDeviceInfo;           // 设备信息
struct devLibraryImp;           // 库实现
struct devOptionsInfo;          // 选项信息
```

### 3.3 device64.dll 导入依赖

```
WSOCK32.dll: select, bind, WSACleanup, socket, recvfrom, htonl, ntohs,
             shutdown, htons, closesocket, WSAStartup, setsockopt,
             sendto, WSAGetLastError
WS2_32.dll:  inet_pton, WSAIoctl, WSASocketW, inet_ntop
```

**确认**: device64.dll 直接使用 Windows Socket API 进行 UDP 通讯，这是与相机通讯的最底层代码。

### 3.4 源代码目录结构（从 PDB 路径逆向）

```
G:\workspace\sdk_win_build\soft.atr.meta.2cams.sdk\
├── soft.atr.2cams.sdk\dev\                          ← fusionTrack64.dll 源代码
│   ├── include\
│   │   ├── fusionTrack.cpp                           ← 主入口（ftkInit, ftkGetLastFrame等）
│   │   ├── ftkDevice.cpp                             ← 设备管理
│   │   ├── ftkMatchMarker.cpp                        ← 🔑 工具匹配核心算法
│   │   ├── ftkGeometryExt.cpp                        ← 几何体扩展
│   │   ├── ftkWirelessManager.cpp                    ← 无线标记管理
│   │   ├── ftkWirelessMarkerSupport.cpp              ← 无线标记支持
│   │   ├── ftkEnvironmentCfgReader.cpp               ← 环境配置
│   │   ├── geometry\
│   │   │   └── StereoProviderV3.cpp                  ← 🔑 标定数据管理V3
│   │   └── options\
│   │       └── LoadEnvironmentSetter.cpp             ← 环境加载
│   └── src\
│       └── ftkHelpers.cpp                            ← 辅助函数
│
├── soft.atr.2cams.ftk\dev\                          ← device64.dll 源代码
│   └── include\
│       ├── devFusionTrack.cpp                        ← 🔑 设备主逻辑
│       ├── devImageProcessor.cpp                     ← 🔑 图像接收处理
│       ├── devPacketReader.cpp                       ← 🔑 数据包解析
│       ├── devGenRtStream.cpp                        ← 实时数据流
│       ├── devUdpTransport.cpp                       ← UDP传输
│       ├── devHeartbeat.cpp                          ← 心跳
│       ├── devAccelerometerProcessor.cpp             ← 加速度计
│       ├── devTemperatureProcessor.cpp               ← 温度
│       ├── devShockProcessor.cpp                     ← 冲击
│       ├── devWatchdogProcessor.cpp                  ← 看门狗
│       ├── devEventDispatcher.cpp                    ← 事件分发
│       ├── devHelpers.cpp                            ← 辅助
│       ├── devMemoryReadHelper.cpp                   ← 内存读取
│       ├── devNetworkConfigs.cpp                     ← 网络配置
│       ├── devNetworkCfgReader.cpp                   ← 网络配置V1
│       ├── devNetworkCfgReaderV2.cpp                 ← 网络配置V2
│       ├── devSchedulerCfgReader.cpp                 ← 调度器
│       ├── devSyncCfgReader.cpp                      ← 同步
│       ├── devJsonVersionAnalyser.cpp                ← JSON版本
│       ├── devDeviceInfoExt.cpp                      ← 设备信息
│       └── options\
│           ├── CalibrationGetter.cpp                 ← 标定获取
│           ├── FrequencyGetter.cpp                   ← 频率
│           ├── WirelessApiVersionGetSetter.cpp       ← 无线API
│           └── ... (30+ option getters/setters)
│
├── soft.atr.2cams.devInterface\dev\                 ← 设备接口层
│   └── include\
│       ├── devGenRtStream.cpp
│       └── devHelpers.cpp
│
└── soft.atr.framework\dev\                          ← 公共框架
    └── include\
        ├── compressor\
        │   ├── pictureCompressor_v2_8bits.cpp        ← 🔑 V2压缩器
        │   ├── pictureCompressor_v3_8bits.cpp        ← 🔑 V3压缩器(8bit)
        │   └── pictureCompressor_v3_16bits.cpp       ← 🔑 V3压缩器(16bit)
        ├── network\
        │   ├── EthernetAdapters.cpp
        │   └── InternetProtocolV4Address.cpp
        ├── _windows\
        │   ├── winUdpClient.cpp                      ← Windows UDP客户端
        │   ├── winUdpCom.cpp                         ← Windows UDP通讯
        │   ├── winSemaphore.cpp                      ← 信号量
        │   └── winSystem.cpp                         ← 系统函数
        ├── factory/Factory.hpp                       ← 工厂模式
        ├── SocketAddress.cpp                         ← Socket地址
        └── thread.cpp                                ← 线程管理
```

---

## 4. 关键算法参数（从 SDK 选项逆向）

从 fusionTrack64.dll 字符串提取的所有可调算法参数：

### 4.1 图像分割参数

| 参数名 | 类 | 说明 |
|--------|-----|------|
| Blob Minimum Surface | `BlobMinSurfaceGetSetter` | blob 最小面积（像素） |
| Blob Maximum Surface | `BlobMaxSurfaceGetSetter` | blob 最大面积 |
| Blob Minimum Aspect Ratio | — | blob 最小长宽比 |
| Advanced centroid detection | `AdvancedCentroidGetSetter` | 高级质心检测算法 |
| Pixel Weight for Centroid | `PixelWeightGetSetter` | 质心计算像素权重 |
| Enables processing of rejected raw data (edge blobs) | `EdgeBlobsGetSetter` | 边缘 blob 处理 |
| Seed expansion tolerance | `SeedExpansionToleranceGetSetter` | 种子扩展容差 |

### 4.2 三角化参数

| 参数名 | 类 | 说明 |
|--------|-----|------|
| Epipolar Maximum Distance | `EpipolarMaxGetSetter` | 极线最大距离 |
| Temperature Compensation | `TemperatureCompensationGetSetter` | 温度补偿开关 |
| Reference Temperature | `ReferenceTemperatureGetter` | 参考温度 |
| Symmetrise Coords | `SymmetriseCoordsGetSetter` | 坐标对称化 |
| Tracking Range | `TrackingRangeGetSetter` | 追踪范围 |
| Working Volume Hard Cuts | `EnableWorkingVolumeHardCuts` | 工作体积硬截断 |

### 4.3 工具匹配参数

| 参数名 | 类 | 说明 |
|--------|-----|------|
| New marker reco algorithm | `NewMarkerRecoGetSetter` | 新/旧匹配算法切换 |
| Matching tolerance (old algo) | `MatchingToleranceGetSetter` | 匹配容差（旧算法） |
| Distance matching tolerance (new algo) | `DistanceToleranceGetSetter` | 距离匹配容差（新算法） |
| Maximum missing points | `MissingPointsMaxGetSetter` | 最大允许缺失点数 |
| Registration Mean Error | `RegistrationMaxGetSetter` | 最大配准均方误差 |
| Obliquity compensation enable | `ObliquityCompensationEnableGetSetter` | 斜率补偿 |
| Four pi angle handling | `FourPiAngleHandlingGetSetter` | 4π标记处理 |
| Marker reconstruction walltime | `MarkerRecoWalltimeGetSetter` | 最大处理时间 |
| Tracking Time Span | `TrackingTimeSpanGetSetter` | 追踪时间跨度 |
| Calibration Export | `CalibrationExportGetSetter` | 标定导出 |
| Allow All Calibration | `AllowAllCalibrationGetSetter` | 允许所有标定 |

---

## 5. 嵌入式处理证据 — fusionTrack64.dll 中的关键字符串

### 5.1 "embedded processing" 结构体大小不匹配警告

```
"%s:%d: mismatch in the size of embedded processing structs 
between device and host. Size of elements on host: %d. 
# of elements in stream: %d. Size in bytes in stream: %d. (%s)"
```

**逆向分析**: 这个错误信息证明 fusionTrack64.dll 在处理 **spryTrack 设备的嵌入式处理结果** 时，需要检查设备端和 PC 端的数据结构大小是否一致。这意味着：

1. 对于 **spryTrack** 设备：相机端可以执行嵌入式处理（segmentation + triangulation + marker matching），将 `embedded processing structs`（处理结果结构体）通过 USB 发回 PC
2. 对于 **fusionTrack** 设备：此代码路径不会执行，因为 fusionTrack 不支持嵌入式处理

### 5.2 spryTrack 16位图像支持

```
class StkGet16BitsImages;   // "Stk" = spryTrack
```

此选项类名以 "Stk" 前缀开头，专用于 spryTrack 设备。

### 5.3 几何体只发送到 spryTrack

文档中的 `\if STK` 条件注释以及代码中 geometry 上传逻辑证实，几何体数据只在 spryTrack 设备上发送到相机端，fusionTrack 设备的几何体始终只存储在 PC 端。

---

## 6. 三个核心算法的具体执行位置

| 算法 | 执行位置 | 具体 DLL | 核心类 | 源文件 |
|------|---------|---------|--------|--------|
| **3D 坐标计算** (三角测量) | PC 端 | fusionTrack64.dll | `StereoCameraSystem`, `StereoProviderV3`, `StereoInterpolatorV1` | fusionTrack.cpp, StereoProviderV3.cpp |
| **工具识别** (刚体匹配) | PC 端 | fusionTrack64.dll | `MatchMarkers`, `SpecialisedGeometryExt<N>`, `ftkGeometryExt` | ftkMatchMarker.cpp, ftkGeometryExt.cpp |
| **变换计算** (旋转+平移) | PC 端 | fusionTrack64.dll | `MatchMarkers` (内部 SVD 配准), `Matrix<N,M>`, `Tensor<N,M>` | ftkMatchMarker.cpp |

### 6.1 数据流向

```
双目相机硬件
    │
    │ UDP (压缩的 ROI 稀疏图像, ~12 KB/帧)
    ▼
device64.dll::ImageProcessor
    │ devLastImages()
    ▼
fusionTrack64.dll
    │
    ├─ PictureCompressor → 解压缩
    ├─ SegmenterV21     → 图像分割 (2D blob 列表)
    ├─ StereoCameraSystem → 立体匹配+三角化 (3D fiducial 列表)
    └─ MatchMarkers     → 工具识别+姿态计算 (marker 旋转+平移)
    │
    ▼
应用程序通过 ftkGetLastFrame() 读取:
    frame->rawDataLeft/Right[]     ← 2D 检测结果
    frame->threeDFiducials[]       ← 3D 坐标
    frame->markers[]               ← 工具姿态 (rotation + translation)
```

---

## 7. 结论与建议

### 7.1 关于 fusionTrack 设备

fusionTrack（以太网接口）设备的所有三项核心算法**完全在 PC 端的 fusionTrack64.dll 中执行**。相机硬件仅负责：
- 近红外图像采集
- ROI 区域压缩编码
- UDP 传输

**没有任何 API 或隐藏选项可以让 fusionTrack 设备在相机端执行这些算法。**

### 7.2 关于 spryTrack 设备

spryTrack（USB 接口）设备**确实支持嵌入式处理模式**，可以在相机端执行：
- 图像分割
- 三角化
- 工具匹配
- 姿态计算

使用方法（从 SDK 示例代码）：
```cpp
ftkSetInt32(lib, sn, options["Enable embedded processing"], 1);
ftkSetInt32(lib, sn, options["Enable images sending"], 0);
```

### 7.3 如果需要在 fusionTrack 上实现类似功能

由于 fusionTrack 硬件不支持嵌入式处理，唯一的替代方案是：
1. **更换为 spryTrack 设备** — 原生支持
2. **在网络中间节点处理** — 在相机和终端 PC 之间部署边缘计算节点，运行 fusionTrack64.dll 的处理管线，只将结果转发
3. **UDP 协议逆向** — 完全理解相机 UDP 协议后，自行实现接收+处理（但这等同于重写 fusionTrack64.dll）
