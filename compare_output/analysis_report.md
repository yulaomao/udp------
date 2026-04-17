# compare_output 数据分析报告

## 1. 分析概述

对 `stereo99_CompareAlgorithms.cpp` 捕获的 200 帧比较数据进行了全面分析，
结合 `libfusionTrack64.so` 反汇编逆向验证，发现并修复了以下算法差异。

## 2. 发现的问题

### 2.1 极线匹配阈值错误 (关键问题)

**现象:** 55/200 帧存在 fiducial 数量差异，SDK 总是产生更多匹配点，共缺失 99 个 fiducial。

**根因:** 通过反编译 `libfusionTrack64.so` 中的 `Match2D3D` 类静态常量区 (地址 `0x3216f0`)：
```
cstEpipolarDistDef = 5.0f   (SDK 默认值)
cstEpipolarDistMin = 0.1f
cstEpipolarDistMax = 10.0f
```

我们的实现默认使用 3.0 像素，而 SDK 默认使用 **5.0 像素**。

**修复:** 将 `StereoAlgoLib.h` 和 `StereoAlgoPipeline.cpp` 中的默认阈值从 3.0 改为 5.0。

### 2.2 错误的反向极线验证 (关键问题)

**现象:** 即使阈值匹配，仍有部分匹配点被错误过滤。

**根因:** 反编译 `_findRightEpipolarMatch` (地址 `0x28d0b0`, 1350 字节):
```assembly
; 仅调用 rightEpipolarLine + signedDist
call rightEpipolarLine   ; 0x28d0ff
call signedDist           ; 0x28d174
andpd (abs_mask)          ; 0x28d181 - 取绝对值
ucomisd epipolarMax       ; 0x28d18e - 与阈值比较
jb next_candidate         ; 0x28d192 - 超过则跳过
```

**关键发现:**
- SDK **不执行**反向极线验证 (无 `leftEpipolarLine` 调用)
- `symmetriseCoords` 默认值为 0 (地址 `0x3216d8`)，表示禁用坐标对称化
- `keepRejectedPoints` 默认值为 1 (地址 `0x3216e4`)，保留被 WorkingVolume 拒绝的点

**修复:** 从 `matchEpipolar()` 中移除反向极线验证，仅使用前向极线距离检查。

### 2.3 标定参数精度不一致 (次要问题)

**现象:** 157 个异常样本的 3D 位置差异 (最大 0.789mm)，全部集中在 ~91.6m 远距离标记。

**根因:** `initialize()` 函数中，Rodrigues 旋转和内参矩阵 K 使用了原始 `double` 精度的 `cal` 参数，
而去畸变使用了 `float32` 精度的 `m_cal` 参数。SDK 内部全部使用 `float32` 存储标定参数。

**修复:** 统一使用 `m_cal` (float32 精度) 计算 Rodrigues、K 矩阵和平移向量。

## 3. 修复后验证结果

```
==== Epipolar Matching (Match2D3D) ====
  SDK total pairs:     1527
  Exact match:         1527
  Match rate:          100.0%
  SDK only (missing):  0     ← 之前: ~99 缺失
  Ours only (extra):   0
```

## 4. 剩余差异分析

### 4.1 3D 位置差异 (不可消除)
- **最大差异:** 0.789mm (仅在 Z~91600mm 远距离点)
- **原因:** 深度敏感度放大效应: dZ/dpx ≈ Z²/(f·B) ≈ 35000 mm/px
- 0.00002 px 的去畸变数值差 → ~0.7mm 3D 差异
- 这是 double vs float 浮点精度的固有限制，SDK 也存在同样的不确定性

### 4.2 "Wrong right index" (303 对)
- 当同一左点有多个右候选时 (probability < 1.0)，迭代顺序可能不同
- 匹配对集合完全一致，仅遍历顺序不同
- **不影响功能正确性**

## 5. 逆向工程方法

### 5.1 使用的工具
- `readelf -Ws` - 提取符号表
- `objdump -d` - 反汇编关键函数
- `strings` - 提取字符串常量
- Python 脚本读取 ELF 二进制常量区

### 5.2 关键逆向发现

| 地址 | 符号 | 大小 | 发现 |
|------|------|------|------|
| `0x28d0b0` | `_findRightEpipolarMatch` | 1350B | 仅前向极线检查，无反向验证 |
| `0x28d600` | `Match2D3D::detect` | 444B | 遍历左点列表，逐个调用匹配 |
| `0x3216f0` | `cstEpipolarDistDef` | 4B | 值=5.0f |
| `0x3216d8` | `cstSymmetriseCoordsDef` | 4B | 值=0 (禁用) |
| `0x3216e4` | `cstKeepRejectedPointsDef` | 4B | 值=1 (启用) |

### 5.3 SDK Match2D3D 内部流程 (还原)

```
detect(timingGuardian, leftCount, rightCount):
  1. _copyAndInitRawFiducials() — 复制并初始化原始检测
  2. for each left_fiducial (未被标记为已匹配):
     3. _findRightEpipolarMatch(left_fid, right_list):
        a. rightEpipolarLine(line, left_cx, left_cy)
        b. for each right_fid (未被标记):
           i.  dist = |line.signedDist(right_normalized)|
           ii. if dist < epipolarMaxDistance:
               - triangulate(result, triError, lx, ly, rx, ry)
               - WorkingVolumeAnalyser::applyCut(fid3d, symmetrise)
               - if keepRejectedPoints || status==0:
                   push to fiducial list
        c. probability = 1.0 / num_candidates
  4. copy results to output
```
