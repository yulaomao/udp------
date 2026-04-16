// ===========================================================================
// 逆向工程还原 — PictureCompressor 图像压缩/解压缩
// 来源: fusionTrack64.dll (深度反汇编还原)
// 编译路径:
//   G:\workspace\sdk_win_build\soft.atr.meta.2cams.sdk\
//     soft.atr.framework\dev\include\compressor\pictureCompressor_v2_8bits.cpp
//     soft.atr.framework\dev\include\compressor\pictureCompressor_v3_8bits.cpp
//     soft.atr.framework\dev\include\compressor\pictureCompressor_v3_16bits.cpp
//
// RTTI (从 SegmenterBase 模板参数推断):
//   CompressedDataV2<uint8_t>
//   CompressedDataV3<uint8_t>
//   CompressedDataV3<uint16_t>
//
// DLL字符串证据:
//   "the compressed picture size do no match the ouput picture size."
//   "0 pixels skip found before end of picture. (line %i)"
//   "compression threshold is too low (%u)"
//   "padding area contains non-zero bytes."
//   "FTK_ERR_IMG_DEC" — 图像解压缩错误
//   "FTK_ERR_IMG_FMT" — 图像格式错误
//
// DLL 函数地址 (经 Capstone 反汇编验证):
//   compress V3 8-bit 入口:     RVA 0x001f15a0 (初始化, 382B)
//   compress V3 8-bit 内循环:   RVA 0x001f1746 (逐像素编码, 558B)
//   decompress V3 8-bit 入口:   RVA 0x001f1b80 (验证+参数检查, 336B)
//   decompress V3 8-bit 核心:   RVA 0x001f1cd0 (RLE解码循环, 440B)
//   decompress V3 8-bit 错误:   RVA 0x001f1e88 (错误处理, 405B)
//   decompress V2 8-bit 入口:   RVA 0x001f2840 (V2格式, 573B)
//
// 压缩格式 (V3 8-bit, 从反汇编还原):
//   稀疏 RLE 编码，以128像素为块单位处理每行:
//
//   字节编码:
//     0x00      : 填充/行尾标记 (在16字节边界有特殊含义)
//     0x01-0x7F : 跳过计数 (1-127个暗像素)
//     0x80      : 跳过128个暗像素 (完整暗块标记)
//     0x81-0xFF : 编码像素值, 原始值 = (byte - 0x80) * 2
//
//   数据流结构:
//     [行像素数据][0x00填充到16字节边界]
//     [0x00][u16_skip_count_LE][13个零字节]  ← 行间跳过记录 (16字节)
//     [下一组行像素数据] ...
//
//   编码公式:  encoded = (pixel >> 1) + 0x80   (DLL: shr al,1; add al,0x80)
//   解码公式:  pixel   = (encoded - 0x80) * 2   (有损: LSB丢失, 精度±1)
//
//   行宽度: 2048像素 (从字节级跟踪验证, 每行skip+pixel总数=2048)
// ===========================================================================

#pragma once

#include <cstdint>
#include <vector>
#include <cstring>

namespace measurement {
namespace compressor {

// ─── 压缩数据结构 (从 DLL 内存访问模式还原) ─────────────────

/// CompressedData 结构 (传递给 decompress 的参数)
/// 从 RVA 0x001f1b86 的内存访问推断:
///   [rdx+0x00] = data ptr,  [rdx+0x08] = size,
///   [rdx+0x10] = width,     [rdx+0x14] = height
template <typename PixelT>
struct CompressedDataV3
{
    const uint8_t* data;     ///< [+0x00] 压缩数据指针 (qword)
    uint64_t       size;     ///< [+0x08] 压缩数据大小 (必须是16的倍数)
    uint32_t       width;    ///< [+0x10] 图像宽度 (像素, 通常2048)
    uint32_t       height;   ///< [+0x14] 图像高度/通道数
};

template <typename PixelT>
struct CompressedDataV2
{
    const uint8_t* data;
    uint64_t       size;
    uint32_t       width;
    uint32_t       height;
};

// ─── 返回值枚举 ─────────────────────────────────────────────

enum DecompressResult : int {
    DECOMPRESS_OK            = 0,   ///< 成功
    DECOMPRESS_ERR_X_BOUNDS  = 1,   ///< x越界 "x has value %u, whilst picture width is %u"
    DECOMPRESS_ERR_CHANNELS  = 2,   ///< 通道异常
    DECOMPRESS_ERR_COUNT     = 4,   ///< 通道数不匹配 "wrong number of channels"
    DECOMPRESS_ERR_FORMAT    = 5,   ///< 格式错误 (大小非16倍数/尺寸不匹配)
};

// ─── PictureCompressor ──────────────────────────────────────

/// 图像压缩/解压缩器
///
/// 压缩格式 (V3 8-bit, 从 DLL 反汇编完整还原):
///
/// fusionTrack 使用稀疏 RLE 编码传输红外追踪图像:
/// - 场景中只有少量反光标记球是亮的，>90%的图像是暗背景
/// - 每行以128像素为块单位处理
/// - 暗像素用跳过计数表示，亮像素编码为 (pixel>>1)+0x80
/// - 行数据填充到16字节边界，行间空白用跳过记录表示
class PictureCompressor
{
public:
    PictureCompressor() = default;
    ~PictureCompressor() = default;

    /// 解压缩 V3 格式 8位图像
    /// @param compressed  压缩数据结构 (含数据指针、大小、宽高)
    /// @param output      输出像素缓冲区 (需预分配 height*stride 字节并清零)
    /// @param stride      输出图像行步长 (字节)
    /// @return DecompressResult 错误码
    int decompressV3_8bit(const CompressedDataV3<uint8_t>& compressed,
                          uint8_t* output,
                          int32_t stride);

    /// 解压缩 V2 格式 8位图像
    int decompressV2_8bit(const CompressedDataV2<uint8_t>& compressed,
                          uint8_t* output,
                          int32_t stride);

    /// 解压缩 V3 格式 16位图像
    int decompressV3_16bit(const CompressedDataV3<uint16_t>& compressed,
                           uint16_t* output,
                           int32_t stride);
};

// ===========================================================================
// V3 8-bit 解压缩实现 (从 DLL RVA 0x001f1b80-0x001f201d 完整还原)
// ===========================================================================

inline int PictureCompressor::decompressV3_8bit(
    const CompressedDataV3<uint8_t>& compressed,
    uint8_t* output,
    int32_t stride)
{
    // ═══ 入口校验 (RVA 0x001f1b80) ═══

    const uint8_t* data = compressed.data;
    const uint64_t size = compressed.size;
    const uint32_t width = compressed.width;
    const uint32_t height = compressed.height;

    // 空数据检查 → "empty compressed picture."
    if (!data || size == 0)
        return DECOMPRESS_ERR_FORMAT;

    // 大小必须是16的倍数 → "the compressed data size is not a multiple of 16"
    if (size & 0xF)
        return DECOMPRESS_ERR_FORMAT;

    // ═══ 清零输出缓冲区 ═══
    for (uint32_t y = 0; y < height; ++y)
        std::memset(output + y * stride, 0, width);

    // ═══ RLE 解码核心 (RVA 0x001f1cd0) ═══
    //
    // 寄存器映射:
    //   rbx → ptr (数据指针)
    //   r11 → data_start
    //   r15 → data_end
    //   r10d → width
    //   edi → x (行内位置)
    //   esi → line_count (行×块计数)
    //   r9d → channel_count (通道计数)
    //   ebp → groups_per_row = width >> 7

    const uint8_t* ptr = data;
    const uint8_t* data_end = data + size;
    const uint32_t groups_per_row = width >> 7;

    uint32_t x = 0;             // edi
    uint32_t line_count = 0;    // esi
    uint32_t channel_count = 0; // r9d
    uint32_t row = 0;           // 当前输出行 (高层追踪)

    while (ptr < data_end)
    {
        // 读取一个字节
        uint8_t byte = *ptr++;

        if (byte == 0x00)
        {
            // ═══ 零字节处理 (RVA 0x001f1d85) ═══

            // 检查 x 是否在行边界
            if (x == 0 || (x % width) != 0)
            {
                // x 不在行边界: 0x00 视为 NOP
                if (x <= width)
                    continue;
                return DECOMPRESS_ERR_X_BOUNDS;
            }

            // 检查16字节对齐: (ptr - data_start) % 16 == 1
            uint64_t offset = static_cast<uint64_t>(ptr - data);
            if ((offset & 0xF) != 1)
            {
                // 对齐不匹配: 0x00 视为 NOP
                if (x <= width)
                    continue;
                return DECOMPRESS_ERR_X_BOUNDS;
            }

            // ═══ 读取 u16 行跳过计数 (RVA 0x001f1daa) ═══
            if (ptr + 2 > data_end)
                break;
            uint16_t skip_count = ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8);
            ptr += 2;

            if (skip_count == 0)
            {
                // skip_count == 0 → 数据结束
                break;
            }

            // 重置 x
            x = 0;
            channel_count += skip_count;

            // 验证填充区 (13字节必须为零)
            // → "padding area contains non-zero bytes."
            for (int pad = 0; pad < 13 && ptr < data_end; ++pad)
            {
                if (*ptr != 0)
                    return DECOMPRESS_ERR_FORMAT;
                ++ptr;
            }

            // 更新行计数
            line_count += skip_count * groups_per_row;

            // 跳过空白行 (更新输出行指针)
            row += skip_count;
            continue;
        }

        // ═══ 非零字节处理 ═══

        uint32_t advance;
        if (byte > 0x80)
        {
            // ★ 像素值 (RVA 0x001f1d19): advance = 1
            // 解码: pixel = (byte - 0x80) * 2
            // 来自编码器 (RVA 0x001f17f3): shr al, 1; add al, 0x80
            uint8_t pixel = static_cast<uint8_t>((byte - 0x80) * 2);
            if (row < height && (x % width) < width)
            {
                output[row * stride + (x % width)] = pixel;
            }
            advance = 1;
        }
        else
        {
            // ★ 跳过计数 (RVA 0x001f1d20): advance = byte
            // 0x80 = 跳过128像素 (完整暗块)
            // 0x01-0x7F = 跳过1-127像素
            advance = byte;
        }

        // 累加 x 位置
        x += advance;

        // 检查128像素块边界对齐 (RVA 0x001f1d30)
        if ((x & 0x7F) == 0)
            line_count++;

        // 检查是否完成一整行 (x % width == 0)
        if (x > 0 && (x % width) == 0)
        {
            channel_count++;
            x = 0;
            row++;
        }

        // x 越界检查
        if (x > width)
            return DECOMPRESS_ERR_X_BOUNDS;
    }

    // ═══ 最终验证 (RVA 0x001f1f67) ═══
    // line_count 应等于 height * groups_per_row
    uint32_t expected = height * groups_per_row;
    if (line_count != expected)
        return DECOMPRESS_ERR_COUNT;

    if (x != 0)
        return DECOMPRESS_ERR_CHANNELS;

    if (channel_count != height)
        return DECOMPRESS_ERR_CHANNELS;

    return DECOMPRESS_OK;
}

// ===========================================================================
// V2 8-bit 解压缩 (旧格式, 简化版)
// ===========================================================================

inline int PictureCompressor::decompressV2_8bit(
    const CompressedDataV2<uint8_t>& compressed,
    uint8_t* output,
    int32_t stride)
{
    const uint8_t* data = compressed.data;
    const uint64_t size = compressed.size;
    const uint32_t width = compressed.width;
    const uint32_t height = compressed.height;

    if (!data || size == 0)
        return DECOMPRESS_ERR_FORMAT;

    for (uint32_t y = 0; y < height; ++y)
        std::memset(output + y * stride, 0, width);

    // V2 格式使用 u16 skip/pixel 对
    // → "0 pixels skip found before end of picture. (line %i)"
    uint32_t pos = 0;
    uint32_t row = 0;
    uint32_t col = 0;

    while (pos < size && row < height)
    {
        if (pos + 2 > size) return DECOMPRESS_ERR_FORMAT;
        uint16_t skipCount = data[pos] | (data[pos + 1] << 8);
        pos += 2;

        if (skipCount == 0)
        {
            ++row;
            col = 0;
            continue;
        }

        col += skipCount;
        if (col >= width)
        {
            row += col / width;
            col = col % width;
            if (row >= height) break;
        }

        if (pos + 2 > size) return DECOMPRESS_ERR_FORMAT;
        uint16_t pixelCount = data[pos] | (data[pos + 1] << 8);
        pos += 2;

        for (uint16_t i = 0; i < pixelCount && pos < size; ++i)
        {
            if (row < height && col < width)
                output[row * stride + col] = data[pos];
            pos++;
            col++;
            if (col >= width) { col = 0; ++row; }
        }
    }

    return DECOMPRESS_OK;
}

// ===========================================================================
// V3 16-bit 解压缩 (类似V3 8-bit, 但每像素2字节)
// ===========================================================================

inline int PictureCompressor::decompressV3_16bit(
    const CompressedDataV3<uint16_t>& compressed,
    uint16_t* output,
    int32_t stride)
{
    const uint8_t* data = compressed.data;
    const uint64_t size = compressed.size;
    const uint32_t width = compressed.width;
    const uint32_t height = compressed.height;

    if (!data || size == 0)
        return DECOMPRESS_ERR_FORMAT;
    if (size & 0xF)
        return DECOMPRESS_ERR_FORMAT;

    for (uint32_t y = 0; y < height; ++y)
        std::memset(reinterpret_cast<uint8_t*>(output) + y * stride, 0, width * 2);

    // V3 16-bit 使用 u16 编码: encoded = (pixel >> 1) + 0x8000
    // 跳过标记使用 u16 值 <= 0x8000
    const uint8_t* ptr = data;
    const uint8_t* data_end = data + size;
    uint32_t x = 0;
    uint32_t row = 0;

    while (ptr + 1 < data_end)
    {
        uint16_t word = ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8);
        ptr += 2;

        if (word == 0x0000)
        {
            // 行尾/跳过记录处理
            if (ptr + 14 <= data_end)
            {
                uint16_t skip = ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8);
                ptr += 14; // skip + 12 pad bytes
                if (skip == 0)
                    break;
                row += skip;
                x = 0;
            }
            continue;
        }
        else if (word <= 0x8000)
        {
            x += word;
        }
        else
        {
            uint16_t pixel = static_cast<uint16_t>((word - 0x8000) * 2);
            if (row < height && x < width)
            {
                uint8_t* rowPtr = reinterpret_cast<uint8_t*>(output) + row * stride;
                reinterpret_cast<uint16_t*>(rowPtr)[x] = pixel;
            }
            x++;
        }

        if (x >= width)
        {
            x = 0;
            row++;
        }
    }

    return DECOMPRESS_OK;
}

}  // namespace compressor
}  // namespace measurement
