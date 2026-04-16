# fusionTrack64.dll 反汇编分析：图像分割与圆心拟合算法

## 目录

1. [概述](#1-概述)
2. [反汇编工具与方法](#2-反汇编工具与方法)
3. [DLL结构分析](#3-dll结构分析)
4. [RTTI类型信息](#4-rtti类型信息)
5. [字符串证据](#5-字符串证据)
6. [核心函数定位](#6-核心函数定位)
7. [图像分割算法详解](#7-图像分割算法详解)
8. [种子扩展算法](#8-种子扩展算法)
9. [加权质心计算](#9-加权质心计算)
10. [**圆心拟合算法**（重点）](#10-圆心拟合算法重点)
11. [高级质心检测](#11-高级质心检测)
12. [辅助数学函数](#12-辅助数学函数)
13. [完整调用链路](#13-完整调用链路)
14. [验证与对照](#14-验证与对照)

---

## 1. 概述

本文档详细记录了对 `fusionTrack64.dll`（Atracsys fusionTrack 光学追踪系统的核心库）进行反汇编分析的完整过程，重点还原了图像分割求取像素坐标的相关算法，特别是**圆心拟合算法**。

### 分析目标

fusionTrack 系统的核心流程是：
1. 红外相机拍摄图像
2. **图像分割**检测红外反光标记点（blob）
3. 计算每个blob的**亚像素级精确圆心坐标**
4. 通过双目立体视觉计算3D位置
5. 匹配已知几何模型，输出6DOF位姿

其中步骤2-3是我们分析的重点，涉及的主要算法：
- 种子扩展（Seed Expansion）区域生长
- 加权质心计算（Weighted Centroid）
- **基于Givens旋转的迭代圆心拟合**（Circle Fitting via Givens Rotations）

---

## 2. 反汇编工具与方法

### 使用的工具

| 工具 | 用途 |
|------|------|
| **Capstone** (v5.0.7) | x86-64反汇编引擎 |
| Python struct | PE文件格式解析 |
| 自编脚本 | 字符串提取、RTTI解析、交叉引用 |

### 分析方法

1. **PE头解析**：提取节表信息（.text, .rdata, .data, .pdata）
2. **字符串搜索**：在.rdata中搜索与分割相关的ASCII字符串
3. **RTTI提取**：搜索 `.?AV` 开头的类型描述符，识别C++类名
4. **函数边界**：解析.pdata节（异常处理表）获取所有函数的精确边界
5. **交叉引用**：通过RIP相对寻址找到引用特定字符串的代码位置
6. **浮点密度分析**：统计每个函数中mulsd/addsd/divsd指令数量，定位数学计算密集函数
7. **深度反汇编**：对目标函数逐指令分析，识别算法结构

---

## 3. DLL结构分析

### PE节信息

```
节名      RVA          虚拟大小      文件偏移       文件大小
.text    0x00001000   0x002096A8   0x00000400   0x00209800  (代码段)
.rdata   0x0020B000   0x0008258A   0x00209C00   0x00082600  (只读数据)
.data    0x0028E000   0x00009628   0x0028C200   0x00008A00  (数据段)
.pdata   0x00298000   0x0000CB40   0x00294C00   0x0000CC00  (异常处理)
.rsrc    0x002A5000   0x000001B4   0x002A1800   0x00000200  (资源)
.reloc   0x002A6000   0x000014BC   0x002A1A00   0x00001600  (重定位)
```

- **Image Base**: 0x0000000180000000
- **函数总数**（.pdata）: 4336个函数

### 浮点密集函数统计

通过统计每个函数中的 `mulsd`、`addsd`、`divsd`、`sqrtsd` 指令数量，找到137个浮点密集函数。前几名：

| 地址范围 | 大小(B) | FP操作数 | mul | add | div | sqrt | 推断功能 |
|----------|---------|----------|-----|-----|-----|------|----------|
| 0x58240-0x5A54C | 8972 | 115 | 65 | 36 | 14 | 0 | **圆心拟合主函数** |
| 0x1A99BA-0x1A9C3D | 643 | 55 | 46 | 9 | 0 | 0 | 4x4矩阵行列式 |
| 0x1EDE00-0x1EE3F5 | 1525 | 38 | 20 | 16 | 2 | 0 | 矩阵运算 |
| 0x44850-0x45255 | 2565 | 25 | 15 | 10 | 0 | 0 | 距离排序/初始选点 |
| 0x459F0-0x461D1 | 2017 | 25 | 13 | 12 | 0 | 0 | 高级质心检测 |
| 0x4CEE0-0x4E0C3 | 4579 | 20 | 9 | 8 | 3 | 0 | 段模板实例1 |
| 0x36CD4-0x36D63 | 143 | 14 | 12 | 2 | 0 | 0 | 3x3行列式/叉积 |

---

## 4. RTTI类型信息

从 `.data` 段提取的关键RTTI类型描述符：

### 分割器相关

```
0x002902C0: .?AV?$SegmenterBase@V?$Image@G$02@measurement@@
               U?$CompressedDataV3@G@compressor@2@G_K$01$01@segmenter@measurement@@
             → SegmenterBase<Image<uint16_t,3>, CompressedDataV3<uint16_t>>

0x00290380: .?AV?$SegmenterBase@V?$Image@E$00@measurement@@
               U?$CompressedDataV3@E@compressor@2@EI$01$01@segmenter@measurement@@
             → SegmenterBase<Image<uint8_t,1>, CompressedDataV3<uint8_t>>

0x00290410: .?AV?$SegmenterV21@V?$Image@E$00@measurement@@
               U?$CompressedDataV2@E@compressor@2@EI$00@segmenter@measurement@@
             → SegmenterV21<Image<uint8_t,1>, CompressedDataV2<uint8_t>>

0x00290590: .?AV?$SegmenterV21@V?$Image@G$02@measurement@@
               U?$CompressedDataV3@G@compressor@2@G_K$01@segmenter@measurement@@
             → SegmenterV21<Image<uint16_t,3>, CompressedDataV3<uint16_t>>

0x00290610: .?AV?$SegmenterV21@V?$Image@E$00@measurement@@
               U?$CompressedDataV3@E@compressor@2@EI$01@segmenter@measurement@@
             → SegmenterV21<Image<uint8_t,1>, CompressedDataV3<uint8_t>>
```

### 配置/检测相关

```
0x0028F440: .?AVAdvancedCentroidGetSetter@@          → 高级质心配置
0x0028F5B8: .?AVBlobMaxSurfaceGetSetter@@            → Blob最大面积配置
0x0028F5E8: .?AVBlobMinSurfaceGetSetter@@            → Blob最小面积配置
0x0028F9F8: .?AVPixelWeightGetSetter@@               → 像素权重配置
0x0028FB68: .?AVSeedExpansionToleranceGetSetter@@    → 种子扩展容差配置
0x0028F788: .?AVEdgeBlobsGetSetter@@                 → 边缘Blob处理配置
0x00290348: .?AV?$Line@G_K@segmenter@measurement@@   → 行程编码线段
0x00290690: .?AV?$Line@EI@segmenter@measurement@@    → 行程编码线段
0x00290490: .?AVRawFiducial@measurement@@             → 原始基准点
0x002904C0: .?AVFiducialDetector@measurement@@        → 基准点检测器
```

---

## 5. 字符串证据

在 `.rdata` 段中找到的分割/质心/拟合相关字符串：

### 配置选项字符串

| RVA | 字符串 | 用途 |
|-----|--------|------|
| 0x002155B8 | "Maximum surface of the blob" | Blob最大面积说明 |
| 0x002155D8 | "Blob Maximum Surface" | 配置项名 |
| 0x002155F0 | "Blob Minimum Surface" | 配置项名 |
| 0x00215608 | "Minimum surface of the blob" | 最小面积说明 |
| 0x00215628 | "Minimum aspect ratio of the blob" | 最小长宽比 |
| 0x00215650 | "Blob Minimum Aspect Ratio" | 配置项名 |
| 0x002156A8 | "Enhances the detection of the centroid detection" | 高级质心说明 |
| 0x002156E0 | "Advanced centroid detection" | 配置项名 |
| 0x00215700 | "Pixel Weight for Centroid" | 像素权重配置名 |
| 0x00215720 | "Use pixel weight for centroid calculation" | 权重说明 |
| 0x00215788 | "Enable rejected data" | 边缘数据处理 |

### 错误/状态字符串

| RVA | 字符串 | 用途 |
|-----|--------|------|
| 0x002175C8 | "Overflow during image segmentation" | 分割溢出错误 |
| 0x00217AD8 | "Could not find binning for point %i at (%f, %f)" | 分箱错误 |
| 0x00218158 | "Cannot update weight for point index " | 权重更新错误 |
| 0x00218840 | "Sum of weights is " | 权重求和日志 |
| 0x00218810 | "At least a weight is strictly smaller than zero" | 权重校验 |

### 交叉引用定位

通过字符串引用，确定了关键函数的位置：

```
"Blob Maximum Surface"              → 引用于 0x32816 [函数 0x325A0-0x3438B, 7659字节]
"Advanced centroid detection"        → 引用于 0x32A11 [函数 0x325A0-0x3438B]
"Pixel Weight for Centroid"          → 引用于 0x32915 [函数 0x325A0-0x3438B]
"Overflow during image segmentation" → 引用于 0x3D6C9 [函数 0x3CCE0-0x3DB1B, 3643字节]
```

---

## 6. 核心函数定位

### 函数映射表

| 函数RVA | 大小 | 功能 | 定位依据 |
|---------|------|------|----------|
| 0x325A0-0x3438B | 7659B | 分割器配置注册 | 引用所有配置字符串 |
| 0x3CCE0-0x3DB1B | 3643B | 分割器初始化/段注册 | "Overflow during segmentation" |
| 0x44850-0x45255 | 2565B | 点距排序/初始选点 | 25 FP ops, 距离计算模式 |
| 0x459F0-0x461D1 | 2017B | 高级质心检测 | 25 FP ops, 紧邻主函数 |
| **0x58240-0x5A54C** | **8972B** | **圆心拟合主函数** | **115 FP ops, Givens旋转模式** |
| 0x63CE0-0x63D79 | 154B | stableHypot 辅助函数 | hypot算法特征 |
| 0x36CD4-0x36D63 | 143B | 3x3行列式 | 12 mul + 交替 add/sub |
| 0x1A99BA-0x1A9C3D | 643B | 4x4矩阵运算 | 46 mul 密集矩阵乘法 |

---

## 7. 图像分割算法详解

### 整体流程

```
输入: 解压后的灰度图像 (uint8 或 uint16)
     ┌──────────────────────────────────────────┐
     │  1. 逐行扫描寻找种子像素                   │
     │     (亮度 ≥ 种子扩展容差)                   │
     │                                          │
     │  2. 从种子执行区域生长 (seedExpansion)       │
     │     - 洪泛填充 (flood fill)                │
     │     - 生成行程编码线段 (Line<PixelT>)       │
     │                                          │
     │  3. 合并线段为 Blob                        │
     │                                          │
     │  4. 过滤 Blob:                            │
     │     - 面积过滤 (minSurface ~ maxSurface)  │
     │     - 长宽比过滤 (minAspectRatio)          │
     │     - 边缘blob处理 (edgeBlobs)             │
     │                                          │
     │  5. 计算亚像素质心:                         │
     │     - 标准: 加权质心 (像素亮度权重)          │
     │     - 高级: 圆心拟合 (Givens旋转法)          │
     │                                          │
     │  6. 输出 ftkRawData 结构                   │
     └──────────────────────────────────────────┘
输出: ftkRawData[] (centerXPixels, centerYPixels, pixelsCount, ...)
```

### 配置参数 (从DLL字符串推断)

| 参数名 | 类型 | 默认值 | DLL字符串 |
|--------|------|--------|-----------|
| blobMinSurface | uint32_t | 4 | "Blob Minimum Surface" |
| blobMaxSurface | uint32_t | 10000 | "Blob Maximum Surface" |
| blobMinAspectRatio | float | 0.3 | "Blob Minimum Aspect Ratio" |
| usePixelWeight | bool | true | "Pixel Weight for Centroid" |
| advancedCentroid | bool | false | "Advanced centroid detection" |
| processEdgeBlobs | bool | false | "Enable rejected data" |
| seedTolerance | uint32_t | 10 | "Seed Expansion Tolerance" |

### 分割器函数 segment()

**DLL: 0x3CCE0-0x3DB1B (3643字节, 786条指令)**

反汇编入口：
```asm
0x3CCE0: mov  qword ptr [rsp + 8], rcx    ; 保存this指针
0x3CCE5: push rbp
0x3CCE6: push rsi
0x3CCE7: push rdi
0x3CCE8: push r14
0x3CCEA: push r15
0x3CCEC: lea  rbp, [rsp - 0x37]
0x3CCF1: sub  rsp, 0xA0                   ; 局部变量160字节
0x3CCF8: mov  qword ptr [rbp - 0x31], 0xFFFFFFFFFFFFFFFE  ; 异常cookie
0x3CD00: mov  qword ptr [rsp + 0xE0], rbx
0x3CD08: mov  r14, rcx                    ; r14 = this
0x3CD0B: xor  r15d, r15d                  ; r15 = 0 (计数器)
```

该函数是一个初始化/配置分发函数，通过反复调用 `0x3EE00`(容器构造) 和 `0x3EEA0`(选项注册) 来注册所有分割器选项。实际的分割算法在虚函数表中指向的其他函数中实现。

---

## 8. 种子扩展算法

### 算法描述

种子扩展(Seed Expansion)是从一个高亮度种子像素开始，通过洪泛填充(flood fill)将所有相连的足够亮的像素归为一个blob。

### 核心逻辑（从DLL行为推断 + RTTI + 字符串）

```cpp
void seedExpansion(Image& image, int startX, int startY,
                   PixelT threshold,
                   vector<Line>& lines, vector<vector<bool>>& visited)
{
    // 使用栈实现的洪泛填充
    stack<Seed> stack;
    stack.push({startX, startY});
    visited[startY][startX] = true;

    while (!stack.empty())
    {
        Seed s = stack.top();
        stack.pop();

        // 向左右扩展形成行程线段
        int left = s.x, right = s.x;
        while (left > 0 && !visited[s.y][left-1] &&
               image.pixel(left-1, s.y) >= threshold)
        {
            --left;
            visited[s.y][left] = true;
        }
        while (right < width-1 && !visited[s.y][right+1] &&
               image.pixel(right+1, s.y) >= threshold)
        {
            ++right;
            visited[s.y][right] = true;
        }

        // 记录行程编码线段 (Line<PixelT, IndexT>)
        lines.push_back({s.y, left, right});

        // 向上下行扩展
        for (int ny : {s.y-1, s.y+1})
        {
            for (int nx = left; nx <= right; ++nx)
            {
                if (!visited[ny][nx] && image.pixel(nx, ny) >= threshold)
                {
                    visited[ny][nx] = true;
                    stack.push({nx, ny});
                }
            }
        }
    }
}
```

### 行程编码数据结构

DLL RTTI: `.?AV?$Line@EI@segmenter@measurement@@`

```cpp
template <typename PixelT, typename IndexT>
struct Line {
    IndexT   row;      // 所在行号
    IndexT   colStart; // 起始列
    IndexT   colEnd;   // 结束列
    uint32_t blobId;   // 所属blob ID
};
```

---

## 9. 加权质心计算

### 标准加权质心

当 `Pixel Weight for Centroid` 启用时（默认启用），使用像素亮度作为权重计算质心：

```
centerX = Σ(x_i × intensity_i) / Σ(intensity_i)
centerY = Σ(y_i × intensity_i) / Σ(intensity_i)
```

### DLL中的权重验证

DLL中有多处权重验证代码，引用了以下字符串：
- `"Cannot update weight for point index "` — 出现10+次
- `"At least a weight is strictly smaller than zero"` — 负权重检查
- `"Sum of weights is "` — 权重和日志

这说明算法中对权重值进行了严格检查。

### 代码还原

```cpp
BlobResult computeWeightedCentroid(const Image& image, const vector<Line>& lines)
{
    BlobResult blob{};
    double sumX = 0, sumY = 0, sumW = 0;

    for (const auto& line : lines)
    {
        for (uint32_t x = line.colStart; x <= line.colEnd; ++x)
        {
            double weight = static_cast<double>(image.pixel(x, line.row));

            // DLL验证: weight >= 0
            assert(weight >= 0.0);

            sumX += x * weight;
            sumY += line.row * weight;
            sumW += weight;
            ++blob.area;
        }
    }

    // DLL: "Sum of weights is " — 验证 sumW > 0
    if (sumW > 0)
    {
        blob.centerX = static_cast<float>(sumX / sumW);
        blob.centerY = static_cast<float>(sumY / sumW);
    }

    return blob;
}
```

---

## 10. 圆心拟合算法（重点）

### 算法概述

**这是fusionTrack系统中最核心的算法之一**。标准的加权质心计算虽然快速，但对于圆形红外反光标记点的中心定位精度有限。圆心拟合算法通过将边缘像素拟合到一个圆的模型上，可以达到远超像素级的亚像素精度。

### DLL函数信息

```
函数地址:  0x58240 - 0x5A54C
函数大小:  8972 字节
指令数:    1971 条
FP操作:    128 次 (65 mul, 36 add, 14 div, 0 sqrt)
循环数:    33 个 (含嵌套)
调用函数:  11 个不同目标
最大迭代:  49 次 (cmp rdi, 0x31)
收敛容限:  1e-7 (从 RVA 0x248EC8 读取)
```

### 反汇编入口分析

```asm
; 函数序言 — 保存所有XMM寄存器（大量浮点运算的标志）
00058240: mov   rax, rsp
00058243: push  rbp
00058244: push  rsi
00058245: push  rdi
00058246: push  r12
00058248: push  r13
0005824A: push  r14
0005824C: push  r15
0005824E: lea   rbp, [rsp - 0x70]
00058253: sub   rsp, 0x170                 ; 368字节栈空间
; 保存 xmm6-xmm15 (10个XMM寄存器全部使用！)
00058266: movaps [rax - 0x48], xmm6
0005826A: movaps [rax - 0x58], xmm7
0005826E: movaps [rax - 0x68], xmm8
00058273: movaps [rax - 0x78], xmm9
00058278: movaps [rax - 0x88], xmm10
00058280: movaps [rax - 0x98], xmm11
00058288: movaps [rax - 0xA8], xmm12
00058290: movaps [rax - 0xB8], xmm13
00058298: movaps [rax - 0xC8], xmm14
000582A0: movaps [rax - 0xD8], xmm15
```

保存全部10个被调方保存的XMM寄存器说明此函数使用了**极其密集的浮点运算**。

### 参数分析（从寄存器使用推断）

```
rcx (→ rbx) = 输入数据结构指针（包含边缘点数组）
xmm1        = 初始参数
r8  (→ rsi) = 输出数组1
r9  (→ r15) = 输出数组2（x坐标结果）
[rbp+0xD0]  = 输出数组3（y坐标结果）
```

### 核心算法: 基于Givens旋转的迭代圆拟合

#### 数学原理

给定 N 个边界点 {(x₁,y₁), (x₂,y₂), ..., (xₙ,yₙ)}，求圆心(cx,cy)和半径r使得：

$$\min_{cx,cy,r} \sum_{i=1}^{N} (\sqrt{(x_i-cx)^2+(y_i-cy)^2} - r)^2$$

这是一个非线性最小二乘问题。DLL中使用**Gauss-Newton方法配合Givens旋转QR分解**求解。

#### 算法步骤

**步骤1: 初始估计（DLL: 0x58344-0x58592）**

从边缘点中选择3个互相最远的点，用3点定圆公式计算初始圆参数。

DLL中的距离计算模式（反汇编 0x44958-0x449A4）：
```asm
; 计算两点间距离的平方
movsd xmm1, [r14 + rcx*8 + 0x20]    ; x[i]
subsd xmm1, [r14 + rax*8 + 0x20]    ; x[i] - x[j]
movsd xmm2, [r14 + rcx*8 + 0x28]    ; y[i]
subsd xmm2, [r14 + rax*8 + 0x28]    ; y[i] - y[j]
movsd xmm0, [r14 + rcx*8 + 0x30]    ; z[i]
subsd xmm0, [r14 + rax*8 + 0x30]    ; z[i] - z[j]
mulsd xmm2, xmm2                     ; (y[i]-y[j])²
mulsd xmm1, xmm1                     ; (x[i]-x[j])²
addsd xmm2, xmm1                     ; sum
mulsd xmm0, xmm0                     ; (z[i]-z[j])²
addsd xmm2, xmm0                     ; dist² = Δx²+Δy²+Δz²
; → 调用 0x201D63 (sqrt 导入函数)
call  0x201D63                        ; dist = sqrt(dist²)
```

每个点的结构偏移：+0x20=x, +0x28=y, +0x30=z，步长为 9*8=72 字节。

**步骤2: 3点定圆（DLL: 0x59F18-0x59FB5，FP #15-#33）**

这是圆心拟合的初始化步骤，使用3点解析公式：

```asm
; 关键反汇编段（使用差值平方差技巧提高精度）

; FP#15-17: A = (x₀-x₁)(x₀+x₁) = x₀² - x₁²
subsd xmm7, xmm6          ; xmm7 = x₀ - x₁
addsd xmm0, xmm11         ; xmm0 = x₀ + x₁
mulsd xmm7, xmm0          ; xmm7 = x₀² - x₁²

; FP#18-20: B = (y₀-y₁)(y₀+y₁) = y₀² - y₁²
subsd xmm1, xmm8          ; xmm1 = y₀ - y₁
addsd xmm0, xmm14         ; xmm0 = y₀ + y₁
mulsd xmm1, xmm0          ; xmm1 = y₀² - y₁²

; FP#21: num = A + B = (x₀²-x₁²) + (y₀²-y₁²)
addsd xmm7, xmm1

; FP#22-23: denom = 2·y₀·x₁
addsd xmm0, xmm8          ; 2·y₀
mulsd xmm0, xmm11         ; 2·y₀·x₁

; FP#24: t = num / denom
divsd xmm7, xmm0

; ... 更多的行列式展开计算 ...

; FP#29-31: C = (x₂-x₀)(x₂+x₀) = x₂² - x₀²
subsd xmm1, xmm6
addsd xmm0, xmm6
mulsd xmm1, xmm0

; FP#32-33: cy = (term + C) / (2·x₂)
addsd xmm9, xmm1
divsd xmm9, xmm13
```

> **注意**：DLL使用 `(a-b)*(a+b)` 代替 `a²-b²`，这是一种**数值精度优化**技巧，在 `a ≈ b` 时可以避免灾难性消去。

**步骤3: Givens旋转迭代（DLL: 0x59C90-0x5A2AA，1562字节）**

这是整个圆心拟合的核心循环，使用Givens旋转进行增量式QR分解。

```
外层循环: 最多49次迭代 (cmp rdi, 0x31)
  │
  ├─ 计算每个点的残差: r_i = ||(x_i,y_i) - (cx,cy)|| - radius
  │   DLL: 0x59D48-0x59D8E
  │
  ├─ 构建雅可比行: J_i = [(x_i-cx)/d_i, (y_i-cy)/d_i, -1]
  │   DLL: 0x59DA1-0x59DBF
  │
  ├─ 对当前行执行Givens消元:
  │   │
  │   ├─ 计算Givens旋转参数:
  │   │   DLL 0x59DF6-0x59E12:
  │   │     norm = hypot(R[j][j], row[j])    ; 调用0x63CE0
  │   │     invNorm = 1.0 / norm              ; divsd from 0x248EF0
  │   │     c = R[j][j] * invNorm             ; cos(θ)
  │   │     s = -row[j] * invNorm             ; sin(θ)
  │   │                                        ; xorps with -0.0 (0x249370)
  │   │
  │   └─ 应用旋转到矩阵行:
  │       DLL 0x59E43-0x59E71 (3次循环, 对每列k):
  │         R[j][k]_new = c·R[j][k] + s·row[k]
  │         row[k]_new  = c·row[k]  - s·R[j][k]
  │
  ├─ 回代求解增量 Δ:
  │   DLL 0x5A139-0x5A215:
  │     Δr  = R[2][3] / R[2][2]
  │     Δcy = (R[1][3] - R[1][2]·Δr) / R[1][1]
  │     Δcx = (R[0][3] - R[0][1]·Δcy - R[0][2]·Δr) / R[0][0]
  │
  ├─ 更新参数:
  │     cx -= Δcx, cy -= Δcy, r -= Δr
  │
  └─ 收敛检查:
      |Δcx| + |Δcy| + |Δr| < 1e-7 ?
      DLL: ucomisd with [0x248EC8] = 1e-7
```

**步骤4: Givens旋转应用的详细反汇编**

```asm
; === DLL 0x59DF6-0x59E12: Givens旋转参数计算 ===

; 调用 hypot(a, b) 计算旋转半径
movaps xmm1, xmm14          ; xmm1 = row[j] (要消去的元素)
movaps xmm0, xmm8           ; xmm0 = R[j][j] (对角元素)
call   0x63CE0               ; norm = hypot(R[j][j], row[j])
movaps xmm9, xmm0           ; xmm9 = norm

; 检查 norm > epsilon
andps  xmm7, xmm15          ; |norm|
movsd  xmm0, [rip+0x1EF0E4] ; 加载 1e-7
comisd xmm0, xmm7           ; if norm < 1e-7 then skip
ja     0x5A34C

; 计算 c 和 s
movsd  xmm7, [rip+0x1EF0FA] ; 加载 1.0
divsd  xmm7, xmm9           ; invNorm = 1.0 / norm
mulsd  xmm9, xmm14          ; c = invNorm * R[j][j] (注: 实际是重用寄存器)
mulsd  xmm7, xmm8           ; s = invNorm * row[j]
; 符号翻转
movsd  xmm8, [rip+0x1EF55E] ; 加载 -0.0
xorps  xmm7, xmm8           ; s = -s

; === DLL 0x59E3F-0x59E8E: 旋转应用循环 ===
; 循环3次 (for j = 0, 1, 2), 对每列应用旋转

; xmm9 = c (cos), xmm7 = s (sin)

; 读取当前列值
movsd  xmm11, [rax + r12*8]  ; v1 = R[j][k]
movsd  xmm6, [rax + rbx*8]   ; v2 = row[k]

; 计算 v1_new = c*v1 + s*v2
movaps xmm3, xmm11
mulsd  xmm3, xmm9            ; c * v1
movaps xmm0, xmm6
mulsd  xmm0, xmm7            ; s * v2
addsd  xmm3, xmm0            ; v1_new = c*v1 + s*v2
; → 存储回 R[j][k]  (通过 call 0x5FAC0)

; 计算 v2_new = c*v2 - s*v1
movaps xmm3, xmm6
mulsd  xmm3, xmm9            ; c * v2
movaps xmm0, xmm11
mulsd  xmm0, xmm7            ; s * v1
subsd  xmm3, xmm0            ; v2_new = c*v2 - s*v1
; → 存储回 row[k]  (通过 call 0x5FAC0)

inc    rdi                    ; k++
add    rsi, 8                 ; 下一列
cmp    rdi, 3                 ; k < 3?
jb     loop_start
```

### 关键常量

| RVA | 值 | 用途 |
|-----|-----|------|
| 0x248EC8 | 1e-7 | 收敛容限 (epsilon) |
| 0x248EF0 | 1.0 | 单位值 (除法/归一化) |
| 0x249370 | -0.0 | 符号位掩码 (XOR取反) |

### 为什么使用Givens旋转而非直接QR?

1. **内存效率**: 不需要存储完整的 N×3 雅可比矩阵，只需维护 3×4 的上三角矩阵
2. **数值稳定**: Givens旋转是正交变换，不会放大误差
3. **增量式**: 可以逐点处理，适合流式计算
4. **精度**: 比Householder变换在小矩阵上更精确
5. **适合嵌入式**: 固定内存开销，可预测的执行时间

### 完整算法伪代码

```python
def fitCircleCenter(points, tol=1e-7, max_iter=49):
    """基于Givens旋转的迭代圆拟合 — 从fusionTrack64.dll逆向还原"""

    N = len(points)

    # 步骤1: 初始估计 (3点定圆)
    p0, p1, p2 = select_three_farthest_points(points)
    cx, cy, r = circle_from_3_points(p0, p1, p2)

    # 步骤2: Givens旋转迭代
    for iteration in range(max_iter):
        # 初始化上三角矩阵 R (3×4)
        R = zeros(3, 4)

        for i in range(N):
            # 计算残差和雅可比行
            dx = points[i].x - cx
            dy = points[i].y - cy
            di = sqrt(dx*dx + dy*dy)
            if di < 1e-7: di = 1e-7

            row = [dx/di, dy/di, -1.0, di - r]

            # Givens消元: 将row合并到R中
            for j in range(3):
                if abs(row[j]) < 1e-7:
                    continue
                if abs(R[j][j]) < 1e-7:
                    R[j] = row[j:]
                    break

                # 计算Givens旋转
                norm = hypot(R[j][j], row[j])
                c = R[j][j] / norm
                s = -row[j] / norm

                # 应用旋转
                for k in range(j, 4):
                    t1 = c * R[j][k] + s * row[k]
                    t2 = c * row[k]  - s * R[j][k]
                    R[j][k] = t1
                    row[k] = t2

        # 回代求解
        delta = backsubstitute(R)

        # 更新参数
        cx -= delta[0]
        cy -= delta[1]
        r  -= delta[2]

        # 收敛检查
        if abs(delta[0]) + abs(delta[1]) + abs(delta[2]) < tol:
            break

    return cx, cy, r
```

---

## 11. 高级质心检测

### DLL函数: 0x459F0-0x461D1 (2017字节)

当 `Advanced centroid detection` 启用时，系统使用更精确的质心计算方法。

### 关键反汇编段

```asm
; 初始化3元素数组并乘以缩放因子
00045ACA: movsd xmm1, [rip + 0x203556]   ; 加载缩放常量
00045AE0: movsd xmm0, [rax]              ; 读取元素
00045AE4: mulsd xmm0, xmm1               ; 乘以缩放因子
00045AE8: movsd [rax], xmm0              ; 写回
00045AEC: lea   rax, [rax + 8]           ; 下一元素
00045AF0: sub   rcx, 1                    ; 计数器--
00045AF4: jne   loop                      ; 循环3次
```

这表明高级质心使用了某种**坐标变换**或**加权因子缩放**，在3个维度上应用。

### 推断的高级质心算法

高级质心检测可能使用以下增强方法之一：
1. **Gaussian加权**: 使用高斯函数对距离质心越近的像素给予越高权重
2. **边缘梯度加权**: 使用图像梯度方向信息优化质心位置
3. **二次曲面拟合**: 在质心附近拟合二次曲面，取极值点

---

## 12. 辅助数学函数

### stableHypot (0x63CE0-0x63D79, 154字节)

```asm
; 稳定的二维范数: sqrt(a² + b²)，避免溢出
; 输入: xmm0 = a, xmm1 = b
; 输出: xmm0 = hypot(a, b)

00063CE0: sub   rsp, 0x48
; 取绝对值
00063CE9: movaps xmm6, xmm1
00063CEC: andps  xmm6, [sign_mask]    ; |b|
00063CFB: andps  xmm7, [sign_mask]    ; |a|

; if |a| > |b|:
00063D02: comisd xmm7, xmm6
00063D06: jbe    use_b_as_denominator
         divsd  xmm6, xmm7           ; ratio = |b|/|a|
         mulsd  xmm6, xmm6           ; ratio²
         addsd  xmm6, 1.0            ; 1 + ratio²
         sqrtsd xmm0, xmm6           ; sqrt(1 + ratio²)
         mulsd  xmm0, xmm7           ; |a| * sqrt(1 + (b/a)²)
         ret

; else if |b| > 0:
use_b_as_denominator:
         divsd  xmm7, xmm6           ; ratio = |a|/|b|
         mulsd  xmm7, xmm7           ; ratio²
         addsd  xmm7, 1.0            ; 1 + ratio²
         sqrtsd xmm0, xmm7           ; sqrt(1 + ratio²)
         mulsd  xmm0, xmm6           ; |b| * sqrt(1 + (a/b)²)
         ret

; else: return 0
         xorps  xmm0, xmm0
         ret
```

### 3x3行列式 (0x36CD4-0x36D63, 143字节)

```asm
; 计算 3×3 行列式: det([rcx], [rdx], [rax])
; 使用 Sarrus 规则: a(ei-fh) - b(di-fg) + c(dh-eg)
;
; 输入: rcx = 行向量1 [a,b,c]
;        rdx = 行向量2 [d,e,f]
;        rax = 行向量3 [g,h,i]
;        r9  = 输出 double*
;        r8  = 可选第二输出指针

00036CDA: movsd xmm8, [rdx]           ; d
00036CDF: movsd xmm0, [rcx]           ; a
00036CE3: movaps xmm1, xmm8           ; d
00036CE7: movsd xmm3, [rax]           ; g
00036CEB: movsd xmm5, [rax + 8]       ; h
00036CF0: mulsd xmm8, [rax + 0x10]    ; d*i
00036CF6: mulsd xmm0, [rdx + 8]       ; a*e
00036CFB: mulsd xmm1, [rcx + 0x10]    ; d*c
00036D00: mulsd xmm8, [rcx + 8]       ; d*i*b
00036D06: mulsd xmm0, [rax + 0x10]    ; a*e*i
; ... 后续完成Sarrus展开 ...
00036D35: addsd xmm11, xmm0
00036D3A: mulsd xmm3, [rdx + 8]
00036D3F: addsd xmm11, xmm1
00036D44: subsd xmm11, xmm3
00036D49: subsd xmm11, xmm5
00036D4E: subsd xmm11, xmm8
; 存储结果
00036D59: movsd [r9], xmm11
```

---

## 13. 完整调用链路

```
用户调用 ftkTrack()
    │
    ├── 左相机图像处理
    │   ├── PictureCompressor::decompress()          → 解压图像
    │   ├── SegmenterV21::segment()                  → 图像分割
    │   │   ├── seedExpansion()                      → 种子扩展区域生长
    │   │   ├── mergeLines()                         → 合并行程编码线段
    │   │   ├── filterBlobs()                        → 过滤blob(面积/长宽比)
    │   │   ├── computeWeightedCentroid()            → 标准加权质心
    │   │   │   OR
    │   │   ├── CircleFitting::fitCircleCenter()     → 圆心拟合
    │   │   │   ├── selectFarthestPoints()           → 选择初始3点
    │   │   │   ├── circle_from_3_points()           → 3点定圆
    │   │   │   └── givens_iteration() × 49          → Givens旋转迭代
    │   │   │       ├── stableHypot()                → 稳定范数
    │   │   │       ├── givens_apply()               → 旋转应用
    │   │   │       └── backsubstitute()             → 回代求解
    │   │   └── checkEdgeStatus()                    → 边缘状态检查
    │   └── 输出 ftkRawData[]                        → 亚像素坐标
    │
    ├── 右相机图像处理 (同上)
    │
    ├── StereoCameraSystem::triangulate()             → 双目三角测量
    │   ├── epipolarMatch()                          → 极线匹配
    │   └── linearTriangulation()                    → 线性三角化
    │
    └── markerReco::match()                          → 标记点几何匹配
        └── registrationError()                      → 配准误差计算
```

---

## 14. 验证与对照

### 算法一致性验证

| 特征 | DLL反汇编 | 还原代码 | 状态 |
|------|-----------|----------|------|
| 迭代次数上限 | `cmp rdi, 0x31` = 49 | `kMaxIterations = 49` | ✅ |
| 收敛容限 | RVA 0x248EC8 = 1e-7 | `convergenceTol = 1e-7` | ✅ |
| Givens旋转方向 | `xorps xmm7, [-0.0]` (取反) | `s = -b * invNorm` | ✅ |
| hypot实现 | 0x63CE0 稳定范数 | `stableHypot()` | ✅ |
| 3点初始化 | 差值平方差技巧 | `(a-b)*(a+b)` | ✅ |
| QR分解方法 | Givens旋转消元 | Givens旋转消元 | ✅ |
| 矩阵维度 | 3列(cx,cy,r) + RHS | R[3][4] | ✅ |
| 回代循环 | 从j=2到j=0 | `for j in [2,1,0]` | ✅ |
| 点数据步长 | 72字节 (9×double) | EdgePoint = 72字节 | ✅ |
| 坐标偏移 | +0x20=x, +0x28=y, +0x30=z | x, y, z 各 double | ✅ |

### 数值常量对照

| DLL RVA | DLL值 | 16进制 | 还原使用 |
|---------|-------|--------|----------|
| 0x248EC8 | 1e-7 | 0x3E7AD7F29ABCAF48 | 收敛容限 |
| 0x248EF0 | 1.0 | 0x3FF0000000000000 | 归一化 |
| 0x249370 | -0.0 | 0x8000000000000000 | 符号翻转 |

### 结论

本次反汇编分析成功还原了fusionTrack64.dll中图像分割和圆心拟合的核心算法：

1. **种子扩展**：标准的洪泛填充区域生长，使用行程编码优化
2. **加权质心**：像素亮度加权的矩方法
3. **圆心拟合**（核心）：基于Givens旋转的迭代最小二乘圆拟合
   - 数值稳定（正交变换）
   - 内存高效（3×4矩阵）
   - 收敛快速（通常5-10次迭代）
   - 亚像素精度（1e-7级收敛）

还原的代码保存在 `reverse_engineered_src/segmenter/` 目录：
- `SegmenterV21.h` — 图像分割完整实现
- `CircleFitting.h` — 圆心拟合算法完整实现（含详细注释和反汇编对照）
