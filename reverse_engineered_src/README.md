# fusionTrack SDK 逆向工程核心代码

## 概述

本目录包含从 Atracsys fusionTrack SDK 的二进制库（fusionTrack64.dll、device64.dll）和头文件
逆向工程还原的核心算法代码。

## 逆向工程依据

1. **头文件**：ftkInterface.h（2937行）完整定义了所有公开数据结构和API函数签名
2. **DLL导出表**：fusionTrack64.dll 导出62个函数，device64.dll 导出49个函数
3. **RTTI类型信息**：从DLL中提取的503个C++类名，包含完整的命名空间层次
4. **嵌入源文件路径**：DLL中包含完整的源文件编译路径（G:\workspace\sdk_win_build\...）
5. **错误信息字符串**：DLL中数百条错误消息，揭示了内部算法逻辑和处理流程
6. **示例代码**：samples/ 目录中的完整示例程序
7. **advancedAPI**：高级C++ API的完整源代码

## 源文件结构

还原的源文件按照DLL中发现的命名空间组织：

```
reverse_engineered_src/
├── README.md                          # 本文件
├── fusionTrack.cpp                    # 主入口：ftkGetLastFrame 完整处理管线
├── ftkDevice.cpp                      # 设备管理：SDK初始化、设备枚举
├── StereoCameraSystem.h/.cpp          # 核心：双目三角化 2D→3D
├── math/
│   ├── Matrix.h                       # 矩阵模板库（2x2到64x64）
│   └── ColVector.h                    # 列向量模板
├── calibHandling/
│   ├── StereoProviderBase.h/.cpp      # 标定数据抽象基类
│   ├── StereoProviderV3.cpp           # V3标定读取（温度补偿）
│   └── StereoInterpolatorV1.cpp       # 温度插值标定参数
├── segmenter/
│   └── SegmenterV21.h/.cpp            # 图像分割：blob检测、质心计算
├── compressor/
│   └── PictureCompressor.h/.cpp       # 图像解压缩（V2/V3，8位/16位）
├── markerReco/
│   ├── MatchMarkers.h/.cpp            # 工具识别：3D点→已知几何体匹配
│   └── SpecialisedGeometryExt.h       # 模板化几何体（3-64个fiducials）
├── feature/
│   └── FeatureHandlingV1.h/.cpp       # 特征处理与温度补偿
├── geometryHandling/
│   └── GeometryReaderBase.h/.cpp      # 几何体文件读取（INI/二进制）
└── device/
    ├── UdpTransport.h/.cpp            # UDP通讯层
    ├── ImageProcessor.h/.cpp          # 原始图像接收
    └── Heartbeat.h/.cpp               # 心跳/看门狗
```

## 核心处理管线

```
相机硬件 → [UDP压缩图像] → device64.dll (网络接收)
                                  ↓
fusionTrack64.dll:
  1. PictureCompressor      → 图像解压缩
  2. SegmenterV21           → blob检测 + 亚像素质心
  3. StereoCameraSystem     → 双目三角化 (2D→3D)
  4. MatchMarkers           → 工具几何匹配 + 位姿估计
  5. TemperatureCompensation → 温度补偿校正
                                  ↓
              ftkMarker (rotation[3][3] + translationMM[3])
```

## 关键发现

- **所有核心算法在 fusionTrack64.dll 中执行**（PC端），相机硬件仅发送压缩图像
- 三角化使用经典针孔相机模型 + 径向/切向畸变校正
- 工具匹配基于三角形不变量 + Kabsch/SVD刚体配准
- 支持3-64个fiducial点的几何体（通过模板特化 SpecialisedGeometryExt）
- 标定系统有V0-V3四个版本，V3支持温度补偿

## 逆向精度说明

- **数据结构**：100%准确（来自官方头文件）
- **API函数签名**：100%准确（来自头文件和导出表）
- **类层次结构**：95%准确（来自RTTI信息）
- **算法逻辑**：基于标准计算机视觉算法 + DLL字符串反推的合理还原
- **具体实现细节**（如精确的数值优化参数）：为合理推测
