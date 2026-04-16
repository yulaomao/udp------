// ===========================================================================
// 逆向工程还原 — device64.dll 通讯层
// 来源: device64.dll (49个导出函数)
//
// RTTI:
//   .?AVUdpCom@measurement@@           — UDP通讯类
//   .?AVUdpSocketClient@measurement@@  — UDP Socket客户端
//   .?AVSocketLibrary@measurement@@    — Socket库封装
//   .?AVHeartbeat@@                    — 心跳/看门狗
//   .?AVImageProcessor@@               — 图像接收处理
//
// DLL字符串证据:
//   "UDP connection failed"
//   "No UDP Jumbo frame capabilities"
//   "Setting UDP packet size to %u"
//   "Testing UDP packet size of %u"
//   "cannot initialise socket"
//   "connecting the socket"
//   "could not create socket (%d)"
//   "could not close socket"
//   "error creating socket"
//   "cannot set socket timeouts"
//   "uninitialised socket"
//   "subnet mask is not defined for address of class %c"
//   "valid IPv4 private address %s is of unsupported class %s"
//   "cannot set up HeartBeat instance to keep 0x%llx device up"
//   "cannot terminate Heartbeat instance"
//   "cannot activate data sending"
//   "cannot disable data sending for device %llx"
//   "WATCHDOG_TIMEOUT"
//   "semImageReady->wait failed"
//   "Corruption detected for %s picture. State = %i (expected %i)..."
//   "Incompatible u4ImageType: 0x%X, 0x%X"
//   "Incompatible u8ImageFormat: 0x%X, 0x%X"
//
// 导出函数 (device64.dll):
//   devInit                    — 初始化设备通讯
//   devClose                   — 关闭通讯
//   devEnumerateDevices        — 枚举设备
//   devLastImages              — 获取最新图像
//   devReleaseImages           — 释放图像缓冲
//   devGetInt32 / devSetInt32  — 读写整数寄存器
//   devGetFloat32 / devSetFloat32
//   devGetData / devSetData    — 读写二进制数据
//   devMemRead / devMemWrite   — 直接内存读写
//   devGetAcceleration         — 读取加速度计
//   devGetRealTimeClock        — 读取RTC
//   devGetWirelessMarkerGeometry — 获取无线工具几何体
//   devCreateEvent / devDeleteEvent — 事件管理
//   devEnumerateOptions        — 枚举选项
//   devEnumerateRegisters      — 枚举寄存器
//   devGetDeviceConfig / devSetDeviceConfig — 设备配置
//   devGetAccessLevel / devSetAccessLevel   — 访问级别
//   devRemoveDevice            — 移除设备
//   devGetLastError            — 获取最后错误
//   devErrorExt 类方法         — 错误信息管理
//
// 通讯架构（推断）:
//   fusionTrack 设备使用 UDP 协议通讯
//   默认端口: 3509（从 ftkInitExt JSON 配置推断）
//   默认地址: 172.17.10.7（从 ftkInitExt 文档推断）
//
//   数据流:
//   1. SDK通过UDP发送命令/查询
//   2. 设备返回压缩图像数据（via UDP Jumbo frames）
//   3. Heartbeat 线程定期发送心跳防止 watchdog 超时
//   4. ImageProcessor 接收并缓存图像对（左右同步）
// ===========================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

namespace measurement {

/// UDP Socket 客户端 — .?AVUdpSocketClient@measurement@@
class UdpSocketClient
{
public:
    UdpSocketClient();
    ~UdpSocketClient();

    /// 初始化 socket
    /// DLL字符串: "cannot initialise socket", "could not create socket"
    bool initialize(const std::string& address, uint16_t port);

    /// 关闭 socket
    /// DLL字符串: "could not close socket"
    void close();

    /// 发送数据
    bool send(const uint8_t* data, uint32_t size);

    /// 接收数据
    /// @param buffer    接收缓冲区
    /// @param maxSize   缓冲区大小
    /// @param timeoutMs 超时时间(ms)
    /// @return 接收到的字节数, -1 表示错误
    int32_t receive(uint8_t* buffer, uint32_t maxSize, uint32_t timeoutMs);

    /// 测试 Jumbo frame 支持
    /// DLL字符串: "Testing UDP packet size of %u", "No UDP Jumbo frame capabilities"
    bool testJumboFrameSupport(uint32_t& maxPacketSize);

    bool isConnected() const { return m_connected; }

private:
    int m_socket;
    bool m_connected;
    std::string m_address;
    uint16_t m_port;
    uint32_t m_maxPacketSize;
};

/// UDP 通讯类 — .?AVUdpCom@measurement@@
///
/// 封装了与 fusionTrack 设备的完整 UDP 通讯协议
class UdpCom
{
public:
    UdpCom();
    ~UdpCom();

    /// 连接到设备
    bool connect(const std::string& address, uint16_t port);

    /// 断开连接
    void disconnect();

    /// 读取寄存器（对应 devGetInt32 等）
    bool readRegister(uint32_t address, uint8_t* data, uint32_t size);

    /// 写入寄存器（对应 devSetInt32 等）
    bool writeRegister(uint32_t address, const uint8_t* data, uint32_t size);

    /// 读取内存块（对应 devMemRead）
    bool memoryRead(uint32_t address, uint8_t* data, uint32_t size);

    /// 写入内存块（对应 devMemWrite）
    bool memoryWrite(uint32_t address, const uint8_t* data, uint32_t size);

    /// 激活图像流
    /// DLL字符串: "cannot activate data sending"
    bool activateDataStream();

    /// 停止图像流
    /// DLL字符串: "cannot disable data sending for device %llx"
    bool deactivateDataStream();

private:
    UdpSocketClient m_commandSocket;   ///< 命令通道
    UdpSocketClient m_dataSocket;      ///< 数据通道（图像）
    std::mutex m_mutex;
};

/// Heartbeat — 心跳/看门狗 — .?AVHeartbeat@@
///
/// DLL字符串:
///   "cannot set up HeartBeat instance to keep 0x%llx device up"
///   "cannot terminate Heartbeat instance"
///   "WATCHDOG_TIMEOUT" — 寄存器名
///
/// fusionTrack 设备有 watchdog 定时器
/// 如果一段时间没有收到 SDK 的心跳，设备会停止发送数据
/// 该类启动后台线程定期发送心跳包
class Heartbeat
{
public:
    Heartbeat();
    ~Heartbeat();

    /// 启动心跳线程
    /// @param com          UDP通讯对象
    /// @param serialNumber 设备序列号
    /// @param intervalMs   心跳间隔(ms)
    bool start(UdpCom* com, uint64_t serialNumber, uint32_t intervalMs = 500);

    /// 停止心跳线程
    void stop();

    bool isRunning() const { return m_running; }

private:
    void _heartbeatLoop();

    UdpCom* m_com;
    uint64_t m_serialNumber;
    uint32_t m_intervalMs;
    std::atomic<bool> m_running;
    std::thread m_thread;
};

/// ImageProcessor — 图像接收与缓存 — .?AVImageProcessor@@
///
/// DLL字符串:
///   "semImageReady->wait failed"
///   "Corruption detected for %s picture"
///   "Incompatible u4ImageType"
///   "Incompatible u8ImageFormat"
///
/// 功能:
/// 1. 接收设备发送的压缩图像 UDP 数据包
/// 2. 重组分片的图像帧
/// 3. 验证左右图像同步（检查计数器匹配）
/// 4. 缓存最新的完整帧供 SDK 获取
class ImageProcessor
{
public:
    /// 压缩图像帧（左右一对）
    struct CompressedFramePair
    {
        std::vector<uint8_t> leftData;     ///< 左图压缩数据
        std::vector<uint8_t> rightData;    ///< 右图压缩数据
        uint32_t imageCounter;             ///< 帧计数器
        uint64_t timestampUS;              ///< 时间戳(微秒)
        uint16_t width;                    ///< 图像宽度
        uint16_t height;                   ///< 图像高度
        uint8_t  imageFormat;              ///< 像素格式
        bool     isComplete;               ///< 左右图是否都收到
    };

    ImageProcessor();
    ~ImageProcessor();

    /// 启动图像接收
    bool start(UdpCom* com);

    /// 停止图像接收
    void stop();

    /// 获取最新帧（对应 devLastImages）
    /// @param frame   输出帧数据
    /// @param timeoutMs 等待超时(ms)
    /// @return true 如果成功获取
    bool getLastFrame(CompressedFramePair& frame, uint32_t timeoutMs);

    /// 释放帧缓冲（对应 devReleaseImages）
    void releaseFrame();

private:
    void _receiveLoop();

    UdpCom* m_com;
    std::atomic<bool> m_running;
    std::thread m_receiveThread;
    std::mutex m_frameMutex;

    CompressedFramePair m_currentFrame;
    CompressedFramePair m_pendingLeft;
    CompressedFramePair m_pendingRight;
    bool m_frameReady;
};

}  // namespace measurement
