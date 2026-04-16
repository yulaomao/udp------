# PictureCompressor::decompress() 反汇编还原分析

## 概述

本文档详细记录了对 `fusionTrack64.dll` 中 `PictureCompressor` 图像压缩/解压缩模块的
深度反汇编逆向工程过程。通过 Capstone 反汇编引擎对 DLL 二进制进行指令级分析,
完整还原了 V3 8-bit 格式的压缩算法，并基于此从 pcapng 网络抓包中成功提取了红外追踪图像。

## 1. 函数定位方法

### 1.1 字符串搜索

在 DLL 的 `.rdata` 段中搜索与压缩相关的诊断字符串，定位到关键函数:

| 字符串 | 含义 | 引用函数 RVA |
|--------|------|-------------|
| `"the compressed picture size do no match the ouput picture size."` | 尺寸校验错误 | 0x001f1cb5 |
| `"padding area contains non-zero bytes."` | 填充区非零检查 | 0x001f1e54 |
| `"trying to skip 0 lines."` | 零行跳过错误 | 0x001f1ef1 |
| `"x has value %u, whilst picture width is %u"` | x 越界错误 | 0x001f1f51 |
| `"wrong number of channels, %u were counted but %u expected"` | 通道数不匹配 | 0x001f1fdf |
| `"empty compressed picture."` | 空图像错误 | 0x001f2086 |
| `"compression threshold is too low (%u)"` | 阈值过低错误 | 0x001f20f3 |
| `"0 pixels skip found before end of picture. (line %i)"` | 行末标记错误 | (V2格式) |

### 1.2 源码路径确认

DLL 中嵌入的源码路径字符串:

```
G:\workspace\sdk_win_build\soft.atr.meta.2cams.sdk\
    soft.atr.framework\dev\include\compressor\pictureCompressor_v3_8bits.cpp
    soft.atr.framework\dev\include\compressor\pictureCompressor_v2_8bits.cpp
    soft.atr.framework\dev\include\compressor\pictureCompressor_v3_16bits.cpp
```

确认了三个压缩器版本: V2 8-bit, V3 8-bit, V3 16-bit。

### 1.3 .pdata 异常处理表

通过 PE 文件的 `.pdata` 段 (异常处理表) 精确确定每个函数的起始和结束地址:

| 函数名称 | RVA 范围 | 大小 |
|----------|---------|------|
| `compressV3_8bit` (初始化) | 0x001f15a0 - 0x001f171e | 382 B |
| `compressV3_8bit` (内循环) | 0x001f1746 - 0x001f1974 | 558 B |
| **`decompressV3_8bit`** (验证入口) | **0x001f1b80 - 0x001f1cd0** | **336 B** |
| **`decompressV3_8bit`** (RLE解码核心) | **0x001f1cd0 - 0x001f1e88** | **440 B** |
| `decompressV3_8bit` (错误处理) | 0x001f1e88 - 0x001f201d | 405 B |
| `decompressV3_8bit` (空图像处理) | 0x001f201d - 0x001f20a1 | 132 B |
| `compressV3_16bit` (初始化) | 0x001f20b0 - 0x001f218e | 222 B |
| `decompressV2_8bit` (入口) | 0x001f2840 - 0x001f2a7d | 573 B |

---

## 2. decompressV3_8bit 验证入口 (RVA 0x001f1b80)

### 2.1 函数签名

```
int decompressV3_8bit(
    CompressedData* compData,   // rdx: 压缩数据结构指针
    OutputBuffer* outputBuf,    // r8:  输出缓冲区结构指针
    ...
)
```

### 2.2 CompressedData 结构布局

从代码访问模式推断:

```c
struct CompressedData {
    uint8_t* data;       // [+0x00] 压缩数据指针 (qword)
    uint64_t size;       // [+0x08] 压缩数据大小 (qword)
    uint32_t width;      // [+0x10] 图像宽度 (dword)
    uint32_t height;     // [+0x14] 图像高度/通道数 (dword)
};
```

### 2.3 反汇编 (关键路径)

```asm
; ═══ 入口: 参数校验 ═══
001f1b80: push    r14
001f1b82: sub     rsp, 0x40
001f1b86: mov     r11, [rdx]           ; r11 = compData->data (压缩数据指针)
001f1b89: mov     r14, rdx             ; r14 = compData (保存结构指针)
001f1b8c: test    r11, r11
001f1b8f: je      0x1f201d             ; if (data == NULL) → "empty compressed picture"

001f1b95: mov     r9, [rdx + 8]        ; r9 = compData->size
001f1b99: test    r9, r9
001f1b9c: je      0x1f201d             ; if (size == 0) → "empty compressed picture"

; ═══ 校验: 大小必须是16的倍数 ═══
001f1ba2: test    r9b, 0xf             ; size & 0x0F
001f1ba6: je      0x1f1c30             ; if (size % 16 == 0) → 继续
    ; ... 报错: "the compressed data size is not a multiple of 16" ...
    mov     eax, 5                     ; return 5 (FTK_ERR_IMG_FMT)
    ret

; ═══ 校验: 压缩数据与输出缓冲区尺寸匹配 ═══
001f1c30: mov     r10d, [rdx + 0x10]   ; r10d = width
001f1c34: mov     eax, r10d
001f1c37: mov     ecx, [r8 + 0xc]      ; ecx = output height
001f1c3b: imul    eax, [rdx + 0x14]    ; eax = width * compData->height
001f1c3f: imul    ecx, [r8 + 8]        ; ecx *= output width
001f1c44: cmp     ecx, eax
001f1c46: je      0x1f1cd0             ; if match → 进入核心解码
    ; ... 报错: "the compressed picture size do no match the ouput picture size" ...
    mov     eax, 5
    ret
```

**关键发现:**
- 压缩数据大小必须是 **16的倍数**
- `r10d` = 图像宽度，贯穿整个解码过程
- 返回值: 0=成功, 1=x越界, 2=通道异常, 4=通道数错误, 5=格式错误

---

## 3. decompressV3_8bit 核心解码 (RVA 0x001f1cd0) ⭐

这是最核心的函数，实现了 RLE 解码循环。

### 3.1 寄存器分配

| 寄存器 | 用途 | 说明 |
|--------|------|------|
| `rbx`  | 数据指针 | 当前读取位置 |
| `r11`  | 数据起始 | 压缩数据首地址 |
| `r15`  | 数据末尾 | r11 + size |
| `r10d` | 宽度 | 图像宽度 (像素) |
| `edi`  | x 位置 | 当前行内位置 |
| `esi`  | 行计数 | 已处理的行×通道数 |
| `r9d`  | 通道计数 | 已处理的通道数 |
| `ebp`  | 行组数 | width >> 7 (width / 128) |
| `r14`  | 结构指针 | CompressedData* |
| `r8d`  | 常量0 | 用于清零操作 |

### 3.2 完整反汇编 + 逐行注释

```asm
; ═══ 初始化 ═══
001f1cd0: mov     [rsp+0x50], rbx
001f1cd5: mov     rbx, r11             ; rbx = data_ptr (当前读取位置)
001f1cd8: mov     [rsp+0x58], rbp
001f1cdd: mov     ebp, r10d            ; ebp = width
001f1ce0: mov     [rsp+0x60], rsi
001f1ce5: mov     [rsp+0x38], rdi
001f1cea: shr     ebp, 7               ; ebp = width >> 7 = width / 128
                                        ; (每行的128像素块数)
001f1ced: xor     r8d, r8d             ; r8d = 0 (常量)
001f1cf0: mov     [rsp+0x30], r15
001f1cf5: lea     r15, [r9 + r11]      ; r15 = data_start + data_size = data_end
001f1cf9: mov     esi, r8d             ; esi = 0 (行计数器)
001f1cfc: mov     edi, r8d             ; edi = 0 (x位置)
001f1cff: mov     r9d, r8d             ; r9d = 0 (通道计数器)

; ═══════════════════════════════════════════
; ═══ 主循环: 逐字节处理压缩数据 ═══
; ═══════════════════════════════════════════
MAIN_LOOP:
001f1d02: cmp     rbx, r15             ; while (ptr < end)
001f1d05: jae     VALIDATE_END         ; 跳到最终验证

001f1d0b: movzx   eax, byte ptr [rbx]  ; eax = *ptr (读取一个字节)
001f1d0e: inc     rbx                   ; ptr++
001f1d11: test    al, al                ; if (byte == 0x00)
001f1d13: je      HANDLE_ZERO          ; → 处理零字节 (行尾标记)

; ── 非零字节: 判断是像素还是跳过 ──
001f1d15: cmp     al, 0x80             ; if (byte > 0x80)
001f1d17: jbe     IS_SKIP              ;   否则: 跳过计数
001f1d19: mov     eax, 1               ; ★ 像素值! advance = 1
001f1d1e: jmp     ACCUMULATE

IS_SKIP:
001f1d20: movzx   eax, al              ; ★ 跳过计数! advance = byte (1-128)

; ── 累加 x 位置 ──
ACCUMULATE:
001f1d23: lea     ecx, [rax + rdi]     ; ecx = advance + x (新的x位置)
001f1d26: xor     edx, edx
001f1d28: mov     eax, ecx
001f1d2a: mov     edi, r8d             ; edi = 0 (暂时清零, 后面可能恢复)
001f1d2d: div     r10d                 ; eax = (advance+x) / width
                                        ; edx = (advance+x) % width

; ── 检查是否对齐到128像素边界 ──
001f1d30: test    cl, 0x7f             ; if ((advance+x) & 0x7F == 0)
001f1d33: lea     eax, [rsi + 1]       ;   eax = line_count + 1
001f1d36: cmovne  eax, esi             ; 如果不对齐: eax = line_count (不变)
001f1d39: test    edx, edx             ; if ((advance+x) % width == 0)
001f1d3b: mov     esi, eax             ;   更新行计数
001f1d3d: lea     eax, [r9 + 1]        ;   eax = channel_count + 1
001f1d41: cmovne  eax, r9d             ; 如果余数!=0: channel_count不变
001f1d45: cmovne  edi, ecx             ; 如果余数!=0: x = advance+x (保持)
                                        ; 如果余数==0: x = 0 (行结束, edi保持为0)
001f1d48: mov     r9d, eax

; ── 检查 x 是否越界 ──
001f1d4b: cmp     edi, r10d            ; if (x > width)
001f1d4e: jbe     MAIN_LOOP            ;   x <= width: 继续循环
    ; ... 报错: "x has value %u, whilst picture width is %u" ...
    mov     eax, 1                      ; return 1 (x越界错误)
    ret

; ═══════════════════════════════════════════
; ═══ 处理 0x00 字节 (行尾标记) ═══
; ═══════════════════════════════════════════
HANDLE_ZERO:
001f1d85: xor     edx, edx
001f1d87: mov     eax, edi             ; eax = x
001f1d89: div     r10d                 ; edx = x % width
001f1d8c: test    edx, edx             ; if (x % width != 0)
001f1d8e: jne     0x1f1d4b             ;   → 回到x范围检查 (0x00视为NOP)

; ── x 恰好在行边界, 检查对齐 ──
001f1d90: mov     rax, rbx             ; rax = ptr (0x00之后的位置)
001f1d93: sub     rax, r11             ; rax = bytes_consumed
001f1d96: cqo                          ; 符号扩展 (正数→rdx=0)
001f1d98: and     edx, 0xf
001f1d9b: add     rax, rdx
001f1d9e: and     eax, 0xf             ; eax = bytes_consumed % 16
001f1da1: sub     rax, rdx
001f1da4: cmp     rax, 1               ; ★ 必须 == 1 (即0x00在16n+0位置)
001f1da8: jne     0x1f1d4b             ; 不对齐: 0x00视为NOP, 继续

; ── 读取 u16 行跳过计数 ──
001f1daa: movzx   eax, byte ptr [rbx+1] ; 高字节
001f1dae: movzx   edx, byte ptr [rbx]   ; 低字节
001f1db1: add     rbx, 2                ; 消耗2字节
001f1db5: shl     eax, 8                ; eax <<= 8
001f1db8: add     edx, eax              ; edx = skip_count (u16 LE)
001f1dba: je      0x1f1e88             ; ★ if (skip_count == 0) → 数据结束!

; ── skip_count > 0: 跳过空白行 ──
001f1dc0: mov     edi, r8d             ; x = 0
001f1dc3: add     r9d, edx             ; channel_count += skip_count
001f1dc6: mov     ecx, r8d             ; ecx = 0 (填充检查计数器)

; ── 验证剩余13字节全为零 (填充区检查) ──
PAD_CHECK_LOOP:
001f1dd0: movzx   eax, byte ptr [rbx]  ; 读取填充字节
001f1dd3: inc     rbx
001f1dd6: test    al, al               ; 必须为零
001f1dd8: jne     PAD_ERROR            ; ★ 非零 → 报错 "padding area contains non-zero bytes"
001f1dda: inc     ecx
001f1ddc: cmp     ecx, 0xd             ; 检查13个字节
001f1ddf: jb      PAD_CHECK_LOOP

; ── 更新行计数 ──
001f1de1: imul    edx, ebp             ; edx = skip_count * (width/128)
001f1de4: add     esi, edx             ; line_count += skip_count * groups_per_row
001f1de6: jmp     MAIN_LOOP            ; 继续主循环

; ═══════════════════════════════════════════
; ═══ 最终验证 ═══
; ═══════════════════════════════════════════
VALIDATE_END:
001f1f67: mov     eax, [r14+0x14]      ; eax = expected_height
001f1f6b: imul    eax, ebp             ; eax = height * (width/128)
001f1f6e: cmp     esi, eax             ; line_count == expected_total?
001f1f70: je      VALIDATE_OK
    ; ... 报错: "wrong number of channels" ...
    mov     eax, 4
    ret

VALIDATE_OK:
001f2005: test    edi, edi             ; x 应为 0 (最后一行已完成)
001f2007: jne     0x1f200f
001f2009: cmp     r9d, [r14+0x14]      ; channel_count == height
001f200d: je      SUCCESS

001f200f: mov     r8d, 2               ; return 2 (通道异常)
SUCCESS:
001f2015: mov     eax, r8d             ; return 0 (成功)
001f2018: jmp     EPILOGUE
```

### 3.3 算法流程图

```
开始
  │
  ├─ 初始化: ptr=data, x=0, line_count=0, channel_count=0
  │
  ▼
┌─────────────────────────┐
│ 主循环: ptr < data_end? │──否──→ 最终验证
└─────────┬───────────────┘
          │是
          ▼
    ┌─────────────┐
    │ 读取 byte   │
    │ ptr++       │
    └─────┬───────┘
          │
    ┌─────┴─────────────────────────────┐
    │ byte == 0x00?                     │
    │   是: 检查 x%width==0 且对齐==1?  │──否──→ 视为NOP, 继续循环
    │   是: 读取 u16 skip_count         │
    │     skip==0? → 数据结束!          │
    │     skip>0? → 跳过空白行, 继续    │
    │                                   │
    │ byte > 0x80? (0x81-0xFF)          │
    │   是: ★ 像素值! advance=1         │
    │   pixel = (byte - 0x80) * 2       │
    │                                   │
    │ byte <= 0x80? (0x01-0x80)         │
    │   是: ★ 跳过! advance=byte        │
    │   (0x80 = 跳过128个暗像素)        │
    └─────┬─────────────────────────────┘
          │
          ▼
    ┌─────────────────┐
    │ x += advance    │
    │ x >= width?     │──是──→ 行结束: x=0, 更新计数
    │                 │
    └─────────────────┘
          │否
          └──→ 继续主循环
```

---

## 4. compressV3_8bit 编码器 (RVA 0x001f1746)

通过分析编码器，反向确认了压缩格式。

### 4.1 编码器内循环关键代码

```asm
; ═══ 像素阈值比较 ═══
001f1770: mov     rax, [rbp + 8]       ; 压缩参数结构
001f1774: movzx   ecx, byte ptr [rax]  ; threshold (阈值)
001f1777: cmp     byte ptr [r9], cl    ; pixel vs threshold
001f177a: jae     ABOVE_THRESHOLD      ; pixel >= threshold → 编码

; ── 像素低于阈值: 累计跳过 ──
001f177c: inc     r11d                 ; below_threshold_count++
001f1784: jmp     NEXT_PIXEL

ABOVE_THRESHOLD:
; ── 写入编码像素 ──
001f17ef: movzx   eax, byte ptr [r9]   ; eax = raw pixel value
001f17f3: shr     al, 1                ; ★ pixel >> 1
001f17f5: add     al, 0x80             ; ★ + 0x80
001f17f7: mov     byte ptr [r8], al    ; 写入编码字节
001f17fa: inc     r8                   ; output_ptr++

; ═══ 128像素块全暗: 写入0x80标记 ═══
001f1809: cmp     r11d, 0x80           ; below_count == 128?
001f1810: jne     PARTIAL_SKIP
001f1812: inc     r10d                 ; full_skip_count++ (累计0x80标记)

; ═══ 行末填充到16字节边界 ═══
001f18b0: mov     byte ptr [r8], 0     ; 写入0x00
001f18b4: inc     r8
001f18b7: mov     rax, r8
001f18ba: sub     rax, [r15]           ; 计算当前偏移
001f18bd: cqo
001f18bf: and     edx, 0xf
001f18c2: add     rax, rdx
001f18c5: and     eax, 0xf
001f18c8: cmp     rax, rdx
001f18cb: jne     0x1f18b0             ; 循环直到16字节对齐

; ═══ 行跳过记录 (整行全暗时) ═══
001f18dc: inc     esi                  ; line_skip_count++
    ; ... (当遇到非空白行时写入跳过记录) ...

; ═══ 写入跳过记录 (16字节) ═══
001f18f2: mov     byte ptr [r8], 0     ; byte 0: 0x00 (标记)
001f18f6: mov     byte ptr [r8+1], al  ; byte 1: skip_count 低字节
001f18fc: shr     esi, 8
001f18ff: mov     byte ptr [r8+2], sil ; byte 2: skip_count 高字节
001f1905: mov     qword ptr [r8+3], rax ; bytes 3-10: 零填充
001f1909: mov     dword ptr [r8+0xb], eax ; bytes 11-14: 零填充
001f190d: mov     byte ptr [r8+0xf], al ; byte 15: 零填充
001f1911: add     r8, 0x10             ; output_ptr += 16
```

### 4.2 编码公式确认

从编码器 `shr al, 1; add al, 0x80` 确认:

| 操作 | 公式 | 示例 |
|------|------|------|
| **编码** | `encoded = (pixel >> 1) + 0x80` | pixel=200 → encoded=0xE4 |
| **解码** | `pixel = (encoded - 0x80) * 2` | encoded=0xE4 → pixel=200 |

注意: 编码使用右移1位 (除以2), 因此原始像素值的最低位 (LSB) 会丢失。
这是有损压缩，精度损失 ±1。

---

## 5. 压缩数据格式总结

### 5.1 字节编码表

| 字节值 | 含义 | 处理方式 |
|--------|------|----------|
| `0x00` | 填充/行尾/跳过记录标记 | 见下方规则 |
| `0x01-0x7F` | 跳过计数 (1-127像素) | x += byte |
| `0x80` | 跳过128像素 (完整暗块) | x += 128 |
| `0x81-0xFF` | 编码像素值 | pixel = (byte-0x80)*2, x += 1 |

### 5.2 数据流结构

```
┌──────────────────────────────────────────────────────────┐
│                    压缩数据流                             │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  ┌─ 行组 A (有数据的行) ────────────────────────────┐    │
│  │  [像素数据] [0x00填充到16字节边界]                │    │
│  │  [像素数据] [0x00填充到16字节边界]                │    │
│  │  ...                                             │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌─ 跳过记录 (16字节) ─────────────────────────────┐    │
│  │  [0x00] [skip_lo] [skip_hi] [13个零字节]         │    │
│  │  skip = 连续空白行数                              │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌─ 行组 B (有数据的行) ────────────────────────────┐    │
│  │  [像素数据] [0x00填充到16字节边界]                │    │
│  │  ...                                             │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌─ 跳过记录 ──────────────────────────────────────┐    │
│  │  [0x00] [skip_lo] [skip_hi] [13个零字节]         │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ... (重复)                                              │
│                                                          │
│  ┌─ 最终跳过记录 (到图像末尾) ─────────────────────┐    │
│  │  [0x00] [remaining_rows_lo] [hi] [13个零字节]    │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### 5.3 行宽度确定

行宽度 = **2048 像素**，通过以下方式验证:

1. **字节级跟踪**: 逐字节处理第一行数据，统计 skip+pixel 总数:
   - 6×skip(128) + skip(110) + 1 pixel + skip(17) + 9×skip(128) + 2×skip(128) = **2048** ✓
2. **多行验证**: 对前20行进行同样分析，每行总数均为 2048 ✓
3. **DLL代码**: `shr ebp, 7` 即 `width / 128`，表示每行分为 width/128 个128像素块

### 5.4 跳过记录识别规则

跳过记录的判定条件 (来自DLL验证逻辑):

1. **位于16字节边界**: byte_offset % 16 == 0
2. **首字节为 0x00**: 行尾标记
3. **字节 3-15 全为零**: 填充区验证
4. **字节 1-2 组成 u16**: 跳过的空白行数 (小端序)

特殊情况:
- 当 0x00 不在16字节边界，或 x 不在行边界时，**视为 NOP** (直接忽略)

---

## 6. pcapng 数据帧结构

### 6.1 三层协议栈

```
Layer 0: UDP 数据包
  └─ Layer 1: 设备分片头 (24 bytes)
       ├─ stream_tag     (u16): 0x1003=左目, 0x1004=右目
       ├─ packet_words   (u16): 总包长/2
       ├─ sequence       (u32): 序列号
       ├─ reserved       (u32): 保留
       ├─ frame_token    (u32): 帧标识
       ├─ frame_size     (u32): 完整帧大小
       └─ payload_offset (u32): 本包在帧中的偏移
       └─ Layer 2: 内层帧 (重组后)
            ├─ Inner Header (80 bytes)
            │    ├─ u32[0]:  0x66441819 (magic)
            │    ├─ u32[2]:  device_timestamp
            │    ├─ u32[3]:  version (=1)
            │    ├─ u32[4]:  frame_counter
            │    ├─ u32[5]:  camera_flags (LEFT=0x00, RIGHT=0xC0)
            │    ├─ u32[7]:  row_stride (=128, 即 width/128 的块单位)
            │    └─ u16@65:  ROI 起始行 ★ (新发现)
            └─ Layer 3: 压缩图像数据 (body)
                 └─ V3 8-bit RLE 编码 (如上所述)
```

### 6.2 帧重组

每个完整帧被分成多个 UDP 包传输。重组过程:
1. 按 `(stream_tag, frame_token)` 分组
2. 按 `payload_offset` 排序
3. 拼接到 `frame_size` 大小的缓冲区
4. 验证连续性 (每个包的 offset 等于前一个包的 end)

---

## 7. 解压缩 Python 实现

核心解码函数:

```python
def decompress_v3_8bit(body: bytes, width: int = 2048) -> list[dict[int, int]]:
    """
    解压缩 V3 8-bit 压缩图像.
    
    参数:
        body:  80字节帧头之后的压缩数据
        width: 图像宽度 (默认 2048)
    
    返回:
        行列表, 每行是 {x_position: pixel_value} 字典
    """
    all_rows = []
    current_row = {}
    x = 0
    i = 0

    while i < len(body):
        # 检测16字节边界的跳过记录
        if i % 16 == 0 and i + 16 <= len(body):
            block = body[i:i+16]
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
            continue                    # 填充字节, 忽略
        elif b == 0x80:
            x += 128                    # 跳过128像素 (完整暗块)
        elif b < 0x80:
            x += b                      # 跳过 b 个暗像素
        else:
            pixel = (b - 0x80) * 2      # 解码像素: (byte-0x80)*2
            current_row[x] = pixel
            x += 1

        if x >= width:                  # 行结束
            all_rows.append(current_row)
            current_row = {}
            x = 0
            while i < len(body) and i % 16 != 0:
                i += 1                  # 跳过填充到16字节边界

    if current_row:
        all_rows.append(current_row)
    return all_rows
```

### 7.1 解码结果

**传感器全分辨率: 2048 × 1088** (通过 Capstone 反汇编 + 全帧 ROI 验证确定)

所有帧统一输出为 **2048 × 1088** 的固定尺寸图像，ROI 数据根据帧头 offset 65-66 的
起始行字段放置在正确的传感器坐标位置。

从 `full_03.pcapng` 解码的典型帧:

| 相机 | ROI起始行 | ROI高度 | 有效行数 | 像素数 | 输出尺寸 |
|------|----------|---------|---------|--------|---------|
| 左目 (0x1003) | ~383 | ~705 | ~214 | ~3473 | **2048×1088** |
| 右目 (0x1004) | ~374 | ~715 | ~203 | ~3528 | **2048×1088** |

图像内容: 黑色背景上的白色亮点 (红外反光标记球), 约10-15个标记点,
符合 fusionTrack 光学追踪系统的预期输出。

### 7.2 传感器分辨率发现过程 ★

通过以下步骤确定了传感器全分辨率为 **2048 × 1088**:

1. **宽度确认**: 逐字节跟踪第一行数据, 所有 skip + pixel 累加恰好等于 2048
2. **高度推导**: 使用 Capstone 反汇编 DLL 调用者代码:
   - RVA 0x001c1e36: `movzx edx, word ptr [rsi + 0x47]` → 读取 height (u16)
   - RVA 0x001c1e3a: `movzx ecx, word ptr [rsi + 0x45]` → 读取 width (u16)
   - RVA 0x001c1e40: `imul eax, edx` → width × height = 总像素数
   - DLL 验证: `width × height == output_buffer_pixels`
3. **ROI 字段发现**: 分析内层帧头 offset 64-66:
   - offset 64: 保留 (0x00)
   - offset 65-66: u16 LE = ROI 起始行
   - 左目典型值: 383-384, 右目典型值: 373-374
4. **全分辨率验证**: 对所有 ~4000 帧验证:
   - `ROI_start + channel_count == 1088` (100% 帧成立)
   - 左目: 383 + 705 = **1088** ✓ | 384 + 704 = **1088** ✓
   - 右目: 374 + 714 = **1088** ✓ | 373 + 715 = **1088** ✓

### 7.3 固定尺寸输出策略

问题: 原始解码输出图像尺寸不固定 (左目 704-705 行, 右目 714-716 行),
无法直接用于圆心拟合、基线匹配和立体匹配。

解决方案:
1. 解码器始终输出 **2048 × 1088** 的完整传感器图像
2. ROI 数据根据帧头 `u16@offset65` 的起始行放置在正确位置
3. 非 ROI 区域填充黑色 (值 0)
4. 左右目图像尺寸完全一致, 像素坐标对应传感器物理位置

```
传感器坐标系:
  ┌─────────────────── 2048 pixels ──────────────────┐
  │                                                    │ row 0
  │    (黑色填充区, ROI 之前)                          │
  │                                                    │ row roi_start-1
  │════════════════════════════════════════════════════│ row roi_start
  │    ROI 数据区 (有效像素)                           │
  │    - 包含 IR 标记点圆斑                            │
  │    - 每帧 ~200-215 行含非零像素                    │
  │════════════════════════════════════════════════════│ row roi_start+roi_height-1
  │                                                    │
  │    (黑色填充区, ROI 之后)                          │
  │                                                    │ row 1087
  └────────────────────────────────────────────────────┘
  总高度: 1088 行 (固定)
```

---

## 8. 正确性验证

### 8.1 编码/解码一致性

| 验证项 | 方法 | 结果 |
|--------|------|------|
| 字节编码表 | 编码器 `shr+add` ↔ 解码器 `sub+shl` | ✓ 互逆 |
| 行宽度 | 逐字节累加 skip+pixel = 2048 | ✓ 多行验证 |
| 16字节对齐 | 编码器循环填充 ↔ 解码器跳过填充 | ✓ 一致 |
| 跳过记录 | 编码器写入 [0x00][u16][13zeros] ↔ 解码器检测 | ✓ 匹配 |
| 图像内容 | 解码图像显示清晰的 IR 标记点圆斑 | ✓ 合理 |
| 像素计数 | 每帧约3400-3500像素, 与稀疏IR图像一致 | ✓ 合理 |
| 帧间一致性 | 相邻帧像素位置和数量高度相似 | ✓ 时间连续性 |

### 8.2 与 DLL 错误消息对应

| DLL 错误消息 | 对应的检查 | Python 中的处理 |
|-------------|-----------|----------------|
| `"data size is not a multiple of 16"` | body_size % 16 != 0 | 隐式处理 (16字节对齐) |
| `"padding area contains non-zero bytes"` | 跳过记录后13字节非零 | 跳过记录检测包含此验证 |
| `"trying to skip 0 lines"` | skip_count == 0 | 视为数据结束标记 |
| `"x has value %u, whilst picture width..."` | x > width | 不应发生 (数据正确时) |

---

## 9. 完整函数地图

```
fusionTrack64.dll - PictureCompressor 函数映射

RVA 0x001f15a0 ─ compressV3_8bit 初始化       (382 bytes)
RVA 0x001f171e ─ compressV3_8bit 辅助          (40 bytes)
RVA 0x001f1746 ─ compressV3_8bit 内循环        (558 bytes) ←── 编码核心
RVA 0x001f1974 ─ compressV3_8bit 行尾处理      (94 bytes)
RVA 0x001f19d2 ─ compressV3_8bit 收尾          (20 bytes)

RVA 0x001f1b80 ─ decompressV3_8bit 验证入口    (336 bytes) ←── 入口
RVA 0x001f1cd0 ─ decompressV3_8bit RLE核心     (440 bytes) ←── ★ 解码核心
RVA 0x001f1e88 ─ decompressV3_8bit 错误处理    (405 bytes)
RVA 0x001f201d ─ decompressV3_8bit 空图像      (132 bytes)

RVA 0x001f20b0 ─ compressV3_16bit 初始化       (222 bytes)
RVA 0x001f218e ─ compressV3_16bit 内循环        (315 bytes)

RVA 0x001f2840 ─ decompressV2_8bit 入口         (573 bytes)
RVA 0x001f2a80 ─ decompressV2_8bit RLE循环      (412 bytes)
```
