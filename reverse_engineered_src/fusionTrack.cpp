// ===========================================================================
// 逆向工程还原 — fusionTrack.cpp 主处理管线
// 来源: fusionTrack64.dll
// 编译路径:
//   G:\workspace\sdk_win_build\soft.atr.meta.2cams.sdk\
//     soft.atr.2cams.sdk\dev\include\fusionTrack.cpp
//
// 这是 ftkGetLastFrame 的核心实现 — 完整的帧处理管线
//
// DLL字符串证据:
//   "cannot allocate left compressed pixels"
//   "cannot allocate right compressed pixels"
//   "cannot allocate left pixels"
//   "cannot allocate right pixels"
//   "cannot allocate 3D fiducials"
//   "cannot allocate markers"
//   "cannot allocate sixtyFourFiducialsMarkers"
//   "cannot allocate events"
//   "cannot allocate image header"
//   "cannot allocate left raw data"
//   "marker buffer size will overflow, please reduce the number"
//   "sixtyFourFiducialsMarkers buffer size will overflow"
//   "mismatch in the size of embedded processing structs between device and host"
//
// API函数签名:
//   ATR_EXPORT ftkFrameQuery* ftkCreateFrame()
//   ATR_EXPORT void ftkDeleteFrame(ftkFrameQuery* frame)
//   ATR_EXPORT ftkError ftkSetFrameOptions(bool, bool, uint32, uint32, uint32, uint32, ftkFrameQuery*)
//   ATR_EXPORT ftkError ftkGetLastFrame(ftkLibrary, uint64, ftkFrameQuery*, uint32)
//
// 处理管线（从 DLL 字符串顺序和头文件注释推断）:
//
//   ┌─────────────────────────────────────────────────────────┐
//   │                   ftkGetLastFrame                        │
//   │                                                         │
//   │  1. 检查参数有效性                                       │
//   │  2. 从 ImageProcessor 获取最新压缩图像帧                  │
//   │  3. 解压缩左右图像 (PictureCompressor)                   │
//   │  4. 分割左右图像 (SegmenterV21)                          │
//   │     → 左: ftkRawData[] + 右: ftkRawData[]               │
//   │  5. 获取当前温度                                         │
//   │  6. 温度插值标定参数 (StereoInterpolatorV1)              │
//   │  7. 更新 StereoCameraSystem 参数                        │
//   │  8. 极线匹配 + 三角化 (StereoCameraSystem)               │
//   │     → ftk3DFiducial[]                                   │
//   │  9. 工具匹配 + 位姿估计 (MatchMarkers)                   │
//   │     → ftkMarker[]                                       │
//   │  10. 填充 ftkFrameQuery 结构体                           │
//   │  11. 处理事件 (温度、加速度、同步等)                      │
//   │                                                         │
//   └─────────────────────────────────────────────────────────┘
//
// ===========================================================================

#include "ftkDevice.cpp"  // ftkDevice, ftkLibraryData 定义
#include <cstring>
#include <cmath>

namespace measurement {

// ===========================================================================
// ftkCreateFrame / ftkDeleteFrame
// ===========================================================================

/// ftkCreateFrame — 分配帧查询结构体
///
/// 内部分配所有子数组（markers, fiducials, rawData 等）
/// 初始大小由后续的 ftkSetFrameOptions 设置
ftkFrameQuery* ftkCreateFrameImpl()
{
    ftkFrameQuery* frame = new (std::nothrow) ftkFrameQuery();
    if (!frame)
        return nullptr;

    std::memset(frame, 0, sizeof(ftkFrameQuery));
    return frame;
}

/// ftkDeleteFrame — 释放帧查询结构体
void ftkDeleteFrameImpl(ftkFrameQuery* frame)
{
    if (frame)
    {
        // 释放内部分配的数组
        delete frame;
    }
}

/// ftkSetFrameOptions — 设置帧查询选项
///
/// @param retrieveLeftPixels   是否获取左图像素
/// @param retrieveRightPixels  是否获取右图像素
/// @param maxLeftRawData       最大左图 raw data 数量
/// @param maxRightRawData      最大右图 raw data 数量
/// @param max3DFiducials       最大3D fiducial数量
/// @param maxMarkers           最大marker数量
/// @param frame                帧查询结构体
ftkError ftkSetFrameOptionsImpl(
    bool retrieveLeftPixels,
    bool retrieveRightPixels,
    uint32_t maxLeftRawData,
    uint32_t maxRightRawData,
    uint32_t max3DFiducials,
    uint32_t maxMarkers,
    ftkFrameQuery* frame)
{
    if (!frame)
        return ftkError::FTK_ERR_INV_PTR;

    // 设置标志
    frame->retrieveLeftPixels = retrieveLeftPixels;
    frame->retrieveRightPixels = retrieveRightPixels;

    // 设置预留大小（用于后续溢出检查）
    frame->rawDataLeftVersionSize = maxLeftRawData;
    frame->rawDataRightVersionSize = maxRightRawData;
    frame->threeDFiducialsVersionSize = max3DFiducials;
    frame->markersVersionSize = maxMarkers;

    return ftkError::FTK_OK;
}

// ===========================================================================
// ftkGetLastFrame — 核心处理管线
// ===========================================================================

/// ftkGetLastFrame — 获取最新帧
///
/// 这是 fusionTrack SDK 最核心的函数
/// 执行完整的图像处理管线：从原始图像到工具位姿
///
/// @param lib      初始化的库句柄
/// @param sn       设备序列号
/// @param frame    预分配的帧查询结构体
/// @param timeoutMs 等待超时(ms)
/// @return ftkError
ftkError ftkGetLastFrameImpl(
    ftkLibrary lib,
    uint64_t sn,
    ftkFrameQuery* frame,
    uint32_t timeoutMs)
{
    // ------------------------------------------------------------------
    // 步骤 0: 参数检查
    // ------------------------------------------------------------------
    if (!lib || !frame)
        return ftkError::FTK_ERR_INV_PTR;

    ftkLibraryData* libData = reinterpret_cast<ftkLibraryData*>(lib);
    std::lock_guard<std::mutex> lock(libData->mutex);

    auto devIt = libData->devices.find(sn);
    if (devIt == libData->devices.end())
        return ftkError::FTK_ERR_INV_SN;

    ftkDevice& dev = *devIt->second;

    // ------------------------------------------------------------------
    // 步骤 1: 从 ImageProcessor 获取最新压缩图像帧
    // ------------------------------------------------------------------
    ImageProcessor::CompressedFramePair compressedFrame;
    if (!dev.imageProcessor.getLastFrame(compressedFrame, timeoutMs))
    {
        return ftkError::FTK_WAR_NO_FRAME;
    }

    frame->imageHeader.timestampUS = compressedFrame.timestampUS;
    frame->imageHeader.imageCounter = compressedFrame.imageCounter;
    frame->imageHeader.imageWidth = compressedFrame.width;
    frame->imageHeader.imageHeight = compressedFrame.height;

    // ------------------------------------------------------------------
    // 步骤 2: 解压缩左右图像
    // ------------------------------------------------------------------
    // 只在需要像素数据或需要分割时解压缩

    uint16_t imgW = compressedFrame.width;
    uint16_t imgH = compressedFrame.height;
    int32_t stride = static_cast<int32_t>(imgW);

    // 左图解压缩
    std::vector<uint8_t> leftPixels(imgW * imgH, 0);
    compressor::CompressedDataV3<uint8_t> leftComp;
    leftComp.originalWidth = imgW;
    leftComp.originalHeight = imgH;
    leftComp.compressedPayload = compressedFrame.leftData.data();
    leftComp.payloadSize = static_cast<uint32_t>(compressedFrame.leftData.size());

    if (!dev.compressor.decompressV3_8bit(leftComp, leftPixels.data(), stride))
    {
        libData->lastError = "cannot decompress left image";
        return ftkError::FTK_ERR_IMG_DEC;
    }

    // 右图解压缩
    std::vector<uint8_t> rightPixels(imgW * imgH, 0);
    compressor::CompressedDataV3<uint8_t> rightComp;
    rightComp.originalWidth = imgW;
    rightComp.originalHeight = imgH;
    rightComp.compressedPayload = compressedFrame.rightData.data();
    rightComp.payloadSize = static_cast<uint32_t>(compressedFrame.rightData.size());

    if (!dev.compressor.decompressV3_8bit(rightComp, rightPixels.data(), stride))
    {
        libData->lastError = "cannot decompress right image";
        return ftkError::FTK_ERR_IMG_DEC;
    }

    // 存储像素数据（如果请求）
    if (frame->retrieveLeftPixels && frame->imageLeftPixels)
    {
        std::memcpy(frame->imageLeftPixels, leftPixels.data(), leftPixels.size());
        frame->imageLeftPixelsStat = ftkQueryStatus::QS_OK;
    }
    if (frame->retrieveRightPixels && frame->imageRightPixels)
    {
        std::memcpy(frame->imageRightPixels, rightPixels.data(), rightPixels.size());
        frame->imageRightPixelsStat = ftkQueryStatus::QS_OK;
    }

    // ------------------------------------------------------------------
    // 步骤 3: 图像分割 — 检测 blob (反射标记球)
    // ------------------------------------------------------------------
    segmenter::Image8 leftImg(imgW, imgH, stride, leftPixels.data());
    segmenter::Image8 rightImg(imgW, imgH, stride, rightPixels.data());

    // 左图分割
    uint32_t maxRawData = frame->rawDataLeftVersionSize;
    std::vector<ftkRawData> leftRaw(maxRawData);
    int32_t leftBlobCount = dev.segmenterLeft->segment(leftImg, leftRaw.data(), maxRawData);

    if (leftBlobCount < 0)
    {
        frame->rawDataLeftStat = ftkQueryStatus::QS_ERR_OVERFLOW;
        leftBlobCount = static_cast<int32_t>(maxRawData);
    }
    else
    {
        frame->rawDataLeftStat = ftkQueryStatus::QS_OK;
    }
    frame->rawDataLeftCount = static_cast<uint32_t>(leftBlobCount);

    // 右图分割
    maxRawData = frame->rawDataRightVersionSize;
    std::vector<ftkRawData> rightRaw(maxRawData);
    int32_t rightBlobCount = dev.segmenterRight->segment(rightImg, rightRaw.data(), maxRawData);

    if (rightBlobCount < 0)
    {
        frame->rawDataRightStat = ftkQueryStatus::QS_ERR_OVERFLOW;
        rightBlobCount = static_cast<int32_t>(maxRawData);
    }
    else
    {
        frame->rawDataRightStat = ftkQueryStatus::QS_OK;
    }
    frame->rawDataRightCount = static_cast<uint32_t>(rightBlobCount);

    // 复制 raw data 到帧
    if (frame->rawDataLeft)
        std::memcpy(frame->rawDataLeft, leftRaw.data(),
                    frame->rawDataLeftCount * sizeof(ftkRawData));
    if (frame->rawDataRight)
        std::memcpy(frame->rawDataRight, rightRaw.data(),
                    frame->rawDataRightCount * sizeof(ftkRawData));

    // ------------------------------------------------------------------
    // 步骤 4: 温度补偿 — 更新标定参数
    // ------------------------------------------------------------------
    // DLL字符串: "Temperature compensation mode"
    ftkStereoParameters currentCalib;
    if (dev.stereoInterpolator &&
        dev.stereoInterpolator->getInterpolatedParameters(dev.currentTemperature, currentCalib))
    {
        dev.stereoCamera.initialize(currentCalib);
    }

    // ------------------------------------------------------------------
    // 步骤 5: 极线匹配 + 三角化 — 2D点对 → 3D坐标
    // ------------------------------------------------------------------
    uint32_t maxFiducials = frame->threeDFiducialsVersionSize;
    std::vector<ftk3DFiducial> fiducials(maxFiducials);

    uint32_t fiducialCount = dev.stereoCamera.matchAndTriangulate(
        leftRaw.data(), frame->rawDataLeftCount,
        rightRaw.data(), frame->rawDataRightCount,
        fiducials.data(), maxFiducials);

    frame->threeDFiducialsCount = fiducialCount;

    if (fiducialCount > maxFiducials)
    {
        frame->threeDFiducialsStat = ftkQueryStatus::QS_ERR_OVERFLOW;
        fiducialCount = maxFiducials;
    }
    else
    {
        frame->threeDFiducialsStat = ftkQueryStatus::QS_OK;
    }

    // 复制 fiducials 到帧
    if (frame->threeDFiducials)
        std::memcpy(frame->threeDFiducials, fiducials.data(),
                    fiducialCount * sizeof(ftk3DFiducial));

    // ------------------------------------------------------------------
    // 步骤 6: 工具匹配 + 位姿估计 — 3D点 → 旋转矩阵 + 平移向量
    // ------------------------------------------------------------------
    uint32_t maxMarkers = frame->markersVersionSize;
    std::vector<ftkMarker> markers(maxMarkers);

    uint32_t markerCount = dev.matchMarkers.matchMarkers(
        fiducials.data(), fiducialCount,
        markers.data(), maxMarkers);

    frame->markersCount = markerCount;

    if (markerCount > maxMarkers)
    {
        frame->markersStat = ftkQueryStatus::QS_ERR_OVERFLOW;
        markerCount = maxMarkers;
    }
    else
    {
        frame->markersStat = ftkQueryStatus::QS_OK;
    }

    // 复制 markers 到帧
    if (frame->markers)
        std::memcpy(frame->markers, markers.data(),
                    markerCount * sizeof(ftkMarker));

    // ------------------------------------------------------------------
    // 步骤 7: 释放图像缓冲
    // ------------------------------------------------------------------
    dev.imageProcessor.releaseFrame();

    return ftkError::FTK_OK;
}

// ===========================================================================
// 其他导出函数实现
// ===========================================================================

/// ftkSetGeometry — 注册几何体（旧版 API）
ftkError ftkSetGeometryImpl(ftkLibrary lib, uint64_t sn, ftkGeometry* geometryIn)
{
    if (!lib || !geometryIn)
        return ftkError::FTK_ERR_INV_PTR;

    ftkLibraryData* libData = reinterpret_cast<ftkLibraryData*>(lib);
    std::lock_guard<std::mutex> lock(libData->mutex);

    auto devIt = libData->devices.find(sn);
    if (devIt == libData->devices.end())
        return ftkError::FTK_ERR_INV_SN;

    if (geometryIn->pointsCount < 3 || geometryIn->pointsCount > FTK_MAX_FIDUCIALS)
        return ftkError::FTK_ERR_GEOM_PTS;

    // 转换旧版 ftkGeometry → ftkRigidBody
    ftkRigidBody rb{};
    rb.geometryId = geometryIn->geometryId;
    rb.pointsCount = geometryIn->pointsCount;
    rb.version = 1u;

    for (uint32_t i = 0; i < rb.pointsCount; ++i)
    {
        rb.fiducials[i].position = geometryIn->positions[i];
    }

    return devIt->second->matchMarkers.registerGeometry(rb)
        ? ftkError::FTK_OK : ftkError::FTK_ERR_INTERNAL;
}

/// ftkSetRigidBody — 注册几何体（新版 API）
ftkError ftkSetRigidBodyImpl(ftkLibrary lib, uint64_t sn, ftkRigidBody* geometryIn)
{
    if (!lib || !geometryIn)
        return ftkError::FTK_ERR_INV_PTR;

    ftkLibraryData* libData = reinterpret_cast<ftkLibraryData*>(lib);
    std::lock_guard<std::mutex> lock(libData->mutex);

    auto devIt = libData->devices.find(sn);
    if (devIt == libData->devices.end())
        return ftkError::FTK_ERR_INV_SN;

    if (geometryIn->pointsCount < 3 || geometryIn->pointsCount > FTK_MAX_FIDUCIALS)
        return ftkError::FTK_ERR_GEOM_PTS;

    return devIt->second->matchMarkers.registerGeometry(*geometryIn)
        ? ftkError::FTK_OK : ftkError::FTK_ERR_INTERNAL;
}

/// ftkClearGeometry / ftkClearRigidBody — 移除几何体
ftkError ftkClearGeometryImpl(ftkLibrary lib, uint64_t sn, uint32_t geometryId)
{
    if (!lib)
        return ftkError::FTK_ERR_INV_PTR;

    ftkLibraryData* libData = reinterpret_cast<ftkLibraryData*>(lib);
    std::lock_guard<std::mutex> lock(libData->mutex);

    auto devIt = libData->devices.find(sn);
    if (devIt == libData->devices.end())
        return ftkError::FTK_ERR_INV_SN;

    return devIt->second->matchMarkers.clearGeometry(geometryId)
        ? ftkError::FTK_OK : ftkError::FTK_WAR_GEOM_ID;
}

/// ftkTriangulate — 单点三角化
ftkError ftkTriangulateImpl(ftkLibrary lib, uint64_t sn,
                            const ftk3DPoint* leftPixel,
                            const ftk3DPoint* rightPixel,
                            ftk3DPoint* outPoint)
{
    if (!lib || !leftPixel || !rightPixel || !outPoint)
        return ftkError::FTK_ERR_INV_PTR;

    ftkLibraryData* libData = reinterpret_cast<ftkLibraryData*>(lib);
    std::lock_guard<std::mutex> lock(libData->mutex);

    auto devIt = libData->devices.find(sn);
    if (devIt == libData->devices.end())
        return ftkError::FTK_ERR_INV_SN;

    float epi = 0, tri = 0;
    if (!devIt->second->stereoCamera.triangulate(*leftPixel, *rightPixel, *outPoint, epi, tri))
        return ftkError::FTK_ERR_INTERNAL;

    return ftkError::FTK_OK;
}

/// ftkReprojectPoint — 3D点重投影
ftkError ftkReprojectPointImpl(ftkLibrary lib, uint64_t sn,
                               const ftk3DPoint* inPoint,
                               ftk3DPoint* outLeftData,
                               ftk3DPoint* outRightData)
{
    if (!lib || !inPoint || !outLeftData || !outRightData)
        return ftkError::FTK_ERR_INV_PTR;

    ftkLibraryData* libData = reinterpret_cast<ftkLibraryData*>(lib);
    std::lock_guard<std::mutex> lock(libData->mutex);

    auto devIt = libData->devices.find(sn);
    if (devIt == libData->devices.end())
        return ftkError::FTK_ERR_INV_SN;

    if (!devIt->second->stereoCamera.reproject(*inPoint, *outLeftData, *outRightData))
        return ftkError::FTK_ERR_INTERNAL;

    return ftkError::FTK_OK;
}

/// ftkGetInt32 — 读取整数选项
ftkError ftkGetInt32Impl(ftkLibrary lib, uint64_t sn, uint32_t optID,
                         int32_t* out, ftkOptionGetter what)
{
    if (!lib || !out)
        return ftkError::FTK_ERR_INV_PTR;

    ftkLibraryData* libData = reinterpret_cast<ftkLibraryData*>(lib);
    std::lock_guard<std::mutex> lock(libData->mutex);

    auto devIt = libData->devices.find(sn);
    if (devIt == libData->devices.end())
        return ftkError::FTK_ERR_INV_SN;

    auto optIt = devIt->second->intOptions.find(optID);
    if (optIt == devIt->second->intOptions.end())
        return ftkError::FTK_ERR_INV_OPT_PAR;

    *out = optIt->second;
    return ftkError::FTK_OK;
}

/// ftkSetInt32 — 设置整数选项
ftkError ftkSetInt32Impl(ftkLibrary lib, uint64_t sn, uint32_t optID, int32_t val)
{
    if (!lib)
        return ftkError::FTK_ERR_INV_PTR;

    ftkLibraryData* libData = reinterpret_cast<ftkLibraryData*>(lib);
    std::lock_guard<std::mutex> lock(libData->mutex);

    auto devIt = libData->devices.find(sn);
    if (devIt == libData->devices.end())
        return ftkError::FTK_ERR_INV_SN;

    auto optIt = devIt->second->intOptions.find(optID);
    if (optIt == devIt->second->intOptions.end())
        return ftkError::FTK_ERR_INV_OPT;

    // 实际DLL会验证范围
    devIt->second->intOptions[optID] = val;
    return ftkError::FTK_OK;
}

/// ftkLoadRigidBodyFromFile — 从文件加载几何体
ftkError ftkLoadRigidBodyFromFileImpl(ftkLibrary lib,
                                       const ftkBuffer* fileContent,
                                       ftkRigidBody* geom)
{
    if (!lib || !fileContent || !geom)
        return ftkError::FTK_ERR_INV_PTR;

    if (fileContent->size == 0)
        return ftkError::FTK_ERR_INV_INI_FILE;

    geometryHandling::FileType ftype =
        geometryHandling::GeometryReaderBase::detectFileType(fileContent->data, fileContent->size);

    std::unique_ptr<geometryHandling::GeometryReaderBase> reader;

    switch (ftype)
    {
    case geometryHandling::FileType::INI_V0:
        reader = std::make_unique<geometryHandling::GeometryReaderIniV0>();
        break;
    case geometryHandling::FileType::INI_V1:
        reader = std::make_unique<geometryHandling::GeometryReaderIniV1>();
        break;
    case geometryHandling::FileType::BINARY_V1:
        reader = std::make_unique<geometryHandling::GeometryReaderBinV1>();
        break;
    default:
        return ftkError::FTK_ERR_VERSION;
    }

    if (!reader->read(fileContent->data, fileContent->size, *geom))
        return ftkError::FTK_ERR_INV_INI_FILE;

    return ftkError::FTK_OK;
}

}  // namespace measurement
