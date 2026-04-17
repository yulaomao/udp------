/**
 * SDK 原生函数调用测试 — 尝试通过 C++ 调用 processPictures
 *
 * 编译:
 *   cd fusionTrack_SDK-v4.10.1-linux64
 *   g++ -std=c++11 -I include -L lib \
 *       -o test_sdk ../test_sdk_centroid.cpp \
 *       -lfusionTrack64 -Wl,-rpath,'$ORIGIN/lib'
 *
 * 运行:
 *   LD_LIBRARY_PATH=lib:$LD_LIBRARY_PATH ./test_sdk
 *
 * 说明:
 *   本程序演示如何通过 SDK 公开 API 获取 ftkRawData (2D 圆心),
 *   但由于没有物理设备连接, processPictures 需要有效的 ftkDevice 指针,
 *   因此实际只能测试到 SDK 初始化阶段。
 *   如果有设备连接, 可以通过 ftkGetLastFrame 获取完整的 rawDataLeft/rawDataRight,
 *   其中 centerXPixels/centerYPixels 就是 SDK 原版圆心提取结果。
 */

#include <ftkInterface.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

int main()
{
    printf("=== fusionTrack SDK 原生库测试 ===\n\n");

    // 1. 初始化 SDK
    ftkBuffer buffer{};
    ftkLibrary lib = ftkInitExt(nullptr, &buffer);

    if (lib == nullptr)
    {
        printf("[1] ftkInitExt: 失败\n");
        printf("    消息: %s\n", buffer.data);
        printf("    (预期: 无物理设备时失败)\n");
        return 1;
    }

    printf("[1] ftkInitExt: 成功\n");

    // 2. 枚举设备
    struct DeviceInfo
    {
        uint64 sn;
        ftkDeviceType type;
    };

    DeviceInfo devInfo{};

    auto enumerator = [](uint64 sn, void* user, ftkDeviceType type) {
        auto* info = reinterpret_cast<DeviceInfo*>(user);
        info->sn = sn;
        info->type = type;
    };

    ftkError err = ftkEnumerateDevices(lib, enumerator, &devInfo);
    printf("[2] ftkEnumerateDevices: %s (err=%d)\n",
           err == ftkError::FTK_OK ? "成功" : "失败", static_cast<int>(err));

    if (devInfo.sn == 0)
    {
        printf("    未检测到设备 (SN=0)\n");
        printf("    说明: 没有物理追踪设备, 无法调用 ftkGetLastFrame\n\n");
    }
    else
    {
        printf("    设备 SN: 0x%016llx, Type: %d\n",
               (unsigned long long)devInfo.sn, static_cast<int>(devInfo.type));
    }

    // 3. 创建 Frame
    ftkFrameQuery* frame = ftkCreateFrame();
    if (frame == nullptr)
    {
        printf("[3] ftkCreateFrame: 失败\n");
        ftkClose(&lib);
        return 1;
    }
    printf("[3] ftkCreateFrame: 成功\n");

    // 4. 设置 Frame 选项
    err = ftkSetFrameOptions(true, 5u, 128u, 128u, 256u, 16u, frame);
    printf("[4] ftkSetFrameOptions: %s (err=%d)\n",
           err == ftkError::FTK_OK ? "成功" : "失败", static_cast<int>(err));

    if (err == ftkError::FTK_OK)
    {
        printf("    rawDataLeftVersionSize: Ver=%u, Size=%u\n",
               frame->rawDataLeftVersionSize.Version,
               frame->rawDataLeftVersionSize.ReservedSize);
        printf("    sizeof(ftkRawData) = %zu\n", sizeof(ftkRawData));
        printf("    sizeof(ftk3DFiducial) = %zu\n", sizeof(ftk3DFiducial));
        printf("    sizeof(ftkFrameQuery) = %zu\n", sizeof(ftkFrameQuery));
    }

    // 5. 尝试 ftkGetLastFrame (预期失败 — 无设备)
    if (devInfo.sn != 0)
    {
        err = ftkGetLastFrame(lib, devInfo.sn, frame, 1000u);
        printf("[5] ftkGetLastFrame: %s (err=%d)\n",
               err == ftkError::FTK_OK ? "成功" : "失败", static_cast<int>(err));

        if (err == ftkError::FTK_OK)
        {
            printf("    rawDataLeftCount: %u\n", frame->rawDataLeftCount);
            printf("    rawDataRightCount: %u\n", frame->rawDataRightCount);

            // 打印 SDK 原版圆心结果
            printf("\n    === SDK 原版 Left rawData (圆心) ===\n");
            for (uint32 i = 0; i < frame->rawDataLeftCount; ++i)
            {
                printf("    [%u] center=(%.6f, %.6f) area=%u size=%ux%u\n",
                       i,
                       frame->rawDataLeft[i].centerXPixels,
                       frame->rawDataLeft[i].centerYPixels,
                       frame->rawDataLeft[i].pixelsCount,
                       frame->rawDataLeft[i].width,
                       frame->rawDataLeft[i].height);
            }

            printf("\n    === SDK 原版 Right rawData (圆心) ===\n");
            for (uint32 i = 0; i < frame->rawDataRightCount; ++i)
            {
                printf("    [%u] center=(%.6f, %.6f) area=%u size=%ux%u\n",
                       i,
                       frame->rawDataRight[i].centerXPixels,
                       frame->rawDataRight[i].centerYPixels,
                       frame->rawDataRight[i].pixelsCount,
                       frame->rawDataRight[i].width,
                       frame->rawDataRight[i].height);
            }
        }
        else
        {
            char errBuf[1024];
            ftkGetLastErrorString(lib, 1024, errBuf);
            printf("    错误信息: %s\n", errBuf);
        }
    }
    else
    {
        printf("[5] ftkGetLastFrame: 跳过 (无设备)\n");
    }

    // 6. 查看 processPictures 符号
    void* handle = dlopen("libfusionTrack64.so", RTLD_LAZY);
    if (handle)
    {
        // 使用 mangled name 查找 processPictures
        void* procPicSym = dlsym(handle,
            "_Z15processPicturesP9ftkDeviceRN11measurement14TimingGuardianERK14ftkPixelFormatRK11ProcessTypejPvjSA_P13ftkFrameQueryRb");
        printf("[6] processPictures 符号: %s\n",
               procPicSym ? "找到" : "未找到");

        if (procPicSym)
        {
            printf("    地址: %p\n", procPicSym);
            printf("    说明: processPictures 需要 ftkDevice* 和 TimingGuardian& 等内部类型,\n");
            printf("    无法在没有完整 SDK 内部状态的情况下直接调用\n");
        }
        dlclose(handle);
    }

    // 7. 结构体布局验证
    printf("\n[7] 结构体布局验证:\n");
    printf("    ftkRawData 大小: %zu bytes\n", sizeof(ftkRawData));
    printf("    ftkRawData.centerXPixels 偏移: %zu\n",
           offsetof(ftkRawData, centerXPixels));
    printf("    ftkRawData.centerYPixels 偏移: %zu\n",
           offsetof(ftkRawData, centerYPixels));
    printf("    ftkRawData.pixelsCount 偏移: %zu\n",
           offsetof(ftkRawData, pixelsCount));
    printf("    ftkRawData.width 偏移: %zu\n",
           offsetof(ftkRawData, width));
    printf("    ftkRawData.height 偏移: %zu\n",
           offsetof(ftkRawData, height));

    // 清理
    ftkDeleteFrame(frame);
    ftkClose(&lib);

    printf("\n=== 测试完成 ===\n");
    return 0;
}
