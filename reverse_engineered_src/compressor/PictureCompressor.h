// ===========================================================================
// 逆向工程还原 — PictureCompressor 图像解压缩
// 来源: fusionTrack64.dll
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
// 压缩格式分析（来自 pcapng 捕获的 UDP 数据包分析）:
//   fusionTrack 相机发送的不是完整图像，而是稀疏的 ROI（感兴趣区域）
//   压缩格式为行程编码 (RLE) 的稀疏图像
//   只包含亮于阈值的像素区域，大量黑色背景被跳过
// ===========================================================================

#pragma once

#include <cstdint>
#include <vector>
#include <cstring>

namespace measurement {
namespace compressor {

/// 压缩数据 V2 格式（8位图像）
/// 从 pcapng 分析和 DLL 字符串推断
template <typename PixelT>
struct CompressedDataV2
{
    uint32_t compressedSize;  ///< 压缩数据总大小
    uint32_t originalWidth;   ///< 原始图像宽度
    uint32_t originalHeight;  ///< 原始图像高度
    uint32_t imageCounter;    ///< 图像计数器
    uint8_t  cameraIndex;     ///< 相机索引 (0=左, 1=右)
    uint8_t  imageType;       ///< 图像类型标识

    const uint8_t* compressedPayload; ///< 压缩载荷指针
    uint32_t payloadSize;             ///< 载荷大小
};

/// 压缩数据 V3 格式（支持8位和16位）
template <typename PixelT>
struct CompressedDataV3
{
    uint32_t compressedSize;
    uint32_t originalWidth;
    uint32_t originalHeight;
    uint32_t imageCounter;
    uint8_t  cameraIndex;
    uint8_t  imageType;
    uint8_t  imageFormat;     ///< V3新增: 像素格式
    uint8_t  reserved;

    const uint8_t* compressedPayload;
    uint32_t payloadSize;
};

/// 图像解压缩器
///
/// 压缩格式（从 pcapng 捕获分析推断）:
///
/// fusionTrack 使用 ROI（感兴趣区域）稀疏压缩:
/// 1. 图像被分割为行
/// 2. 每行只传输亮于阈值的像素区域
/// 3. 格式: [skip_count][pixel_count][pixel_data...]
///    - skip_count: 跳过的黑色像素数（uint16）
///    - pixel_count: 有效像素数（uint16）
///    - pixel_data: 原始像素值
/// 4. 每行末尾特殊标记表示行结束
///
/// 这种稀疏编码非常适合红外追踪场景:
/// 场景中只有少量反射标记球是亮的，90%以上的图像是黑色背景
class PictureCompressor
{
public:
    PictureCompressor() = default;
    ~PictureCompressor() = default;

    /// 解压缩 V2 格式 8位图像
    /// @param compressed 压缩数据
    /// @param output     输出像素缓冲区（需预分配 width*stride 字节）
    /// @param stride     输出图像行步长
    /// @return true 如果成功
    bool decompressV2_8bit(const CompressedDataV2<uint8_t>& compressed,
                           uint8_t* output,
                           int32_t stride);

    /// 解压缩 V3 格式 8位图像
    bool decompressV3_8bit(const CompressedDataV3<uint8_t>& compressed,
                           uint8_t* output,
                           int32_t stride);

    /// 解压缩 V3 格式 16位图像
    bool decompressV3_16bit(const CompressedDataV3<uint16_t>& compressed,
                            uint16_t* output,
                            int32_t stride);

private:
    /// 解压缩核心 — 行程编码稀疏图像解码
    ///
    /// 从 DLL 字符串推断的格式:
    ///   "0 pixels skip found before end of picture" → skip_count 为0表示行结束
    ///   "the compressed picture size do no match the ouput picture size" → 尺寸校验
    ///   "padding area contains non-zero bytes" → 跳过区域应为零
    ///
    /// @param compData   压缩载荷
    /// @param compSize   压缩数据大小
    /// @param output     输出缓冲区
    /// @param width      图像宽度
    /// @param height     图像高度
    /// @param stride     行步长
    /// @param bytesPerPixel 每像素字节数
    /// @return true 如果解压成功
    template <typename PixelT>
    bool decompressRLE(const uint8_t* compData,
                       uint32_t compSize,
                       PixelT* output,
                       uint32_t width,
                       uint32_t height,
                       int32_t stride,
                       uint32_t bytesPerPixel);
};

// ===========================================================================
// PictureCompressor 实现
// ===========================================================================

inline bool PictureCompressor::decompressV2_8bit(
    const CompressedDataV2<uint8_t>& compressed,
    uint8_t* output,
    int32_t stride)
{
    // 清零输出缓冲区
    for (uint32_t y = 0; y < compressed.originalHeight; ++y)
        std::memset(output + y * stride, 0, compressed.originalWidth);

    return decompressRLE<uint8_t>(
        compressed.compressedPayload,
        compressed.payloadSize,
        output,
        compressed.originalWidth,
        compressed.originalHeight,
        stride, 1);
}

inline bool PictureCompressor::decompressV3_8bit(
    const CompressedDataV3<uint8_t>& compressed,
    uint8_t* output,
    int32_t stride)
{
    for (uint32_t y = 0; y < compressed.originalHeight; ++y)
        std::memset(output + y * stride, 0, compressed.originalWidth);

    return decompressRLE<uint8_t>(
        compressed.compressedPayload,
        compressed.payloadSize,
        output,
        compressed.originalWidth,
        compressed.originalHeight,
        stride, 1);
}

inline bool PictureCompressor::decompressV3_16bit(
    const CompressedDataV3<uint16_t>& compressed,
    uint16_t* output,
    int32_t stride)
{
    for (uint32_t y = 0; y < compressed.originalHeight; ++y)
        std::memset(reinterpret_cast<uint8_t*>(output) + y * stride, 0,
                    compressed.originalWidth * 2);

    return decompressRLE<uint16_t>(
        compressed.compressedPayload,
        compressed.payloadSize,
        output,
        compressed.originalWidth,
        compressed.originalHeight,
        stride, 2);
}

template <typename PixelT>
bool PictureCompressor::decompressRLE(
    const uint8_t* compData,
    uint32_t compSize,
    PixelT* output,
    uint32_t width,
    uint32_t height,
    int32_t stride,
    uint32_t bytesPerPixel)
{
    uint32_t pos = 0;     // 压缩数据读取位置
    uint32_t row = 0;     // 当前行
    uint32_t col = 0;     // 当前列

    while (pos < compSize && row < height)
    {
        // 读取跳过数量（uint16 小端）
        if (pos + 2 > compSize) return false;
        uint16_t skipCount = compData[pos] | (compData[pos + 1] << 8);
        pos += 2;

        if (skipCount == 0)
        {
            // DLL字符串: "0 pixels skip found before end of picture"
            // skip_count = 0 表示当前行结束，进入下一行
            ++row;
            col = 0;
            continue;
        }

        // 跳过黑色像素
        col += skipCount;
        if (col >= width)
        {
            // 跨行跳过
            row += col / width;
            col = col % width;
            if (row >= height) break;
        }

        // 读取有效像素数量（uint16 小端）
        if (pos + 2 > compSize) return false;
        uint16_t pixelCount = compData[pos] | (compData[pos + 1] << 8);
        pos += 2;

        // 读取像素数据
        uint32_t dataBytes = pixelCount * bytesPerPixel;
        if (pos + dataBytes > compSize) return false;

        for (uint16_t i = 0; i < pixelCount; ++i)
        {
            if (row < height && col < width)
            {
                uint8_t* rowPtr = reinterpret_cast<uint8_t*>(output) + row * stride;
                PixelT* pixelPtr = reinterpret_cast<PixelT*>(rowPtr) + col;

                if (bytesPerPixel == 1)
                    *pixelPtr = static_cast<PixelT>(compData[pos]);
                else
                    *pixelPtr = static_cast<PixelT>(compData[pos] | (compData[pos + 1] << 8));
            }

            pos += bytesPerPixel;
            ++col;

            if (col >= width)
            {
                col = 0;
                ++row;
                if (row >= height) break;
            }
        }
    }

    return true;
}

}  // namespace compressor
}  // namespace measurement
