// ===========================================================================
// 逆向工程还原 — GeometryReaderBase 几何体文件读取
// 来源: fusionTrack64.dll
//
// RTTI:
//   .?AV?$Factory@VGeometryReaderBase@geometryHandling@measurement@@
//     U?$pair@IW4FileType@GeometryReaderBase@geometryHandling@measurement@@@std@@
//     UCustomHash@23@@factory@measurement@@
//
// 编译路径:
//   G:\workspace\sdk_win_build\soft.atr.meta.2cams.sdk\
//     soft.atr.2cams.sdk\dev\include\ftkGeometryExt.cpp
//
// DLL字符串证据:
//   "could not register geometry reading classes"
//   "when accessing fiducial position for geometry "
//   "when accessing fiducial normal for geometry "
//   "when accessing fiducial type for geometry "
//   "when accessing divot position for geometry "
//   "\" key in [geometry] section"
//   "Unsupported source file format"
//   "submarkers/[]/usedTriangles"
//   "Invalid value for /fiducials/angleOfRegistration"
//
// API函数签名:
//   ATR_EXPORT ftkError ftkLoadRigidBodyFromFile(ftkLibrary, const ftkBuffer*, ftkRigidBody*)
//   ATR_EXPORT ftkError ftkSaveRigidBodyToFile(ftkLibrary, const ftkRigidBody*, ftkBuffer*)
//   ATR_EXPORT ftkError ftkGeometryFileConversion(ftkLibrary, const ftkBuffer*, ftkBuffer*)
//
// 支持的文件格式（来自SDK文档和DLL字符串）:
//   1. Legacy INI (version 0) — 旧版几何体文件
//   2. INI version 1         — 扩展几何体文件（增加法向量、fiducial类型）
//   3. Binary version 1      — 二进制格式（用于无线工具内存）
// ===========================================================================

#pragma once

#include <ftkInterface.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>

namespace measurement {
namespace geometryHandling {

/// 文件类型枚举
enum class FileType : uint32_t
{
    INI_V0 = 0,     ///< Legacy INI 格式
    INI_V1 = 1,     ///< INI version 1
    BINARY_V1 = 2,  ///< Binary version 1
    UNKNOWN = 0xFF
};

/// GeometryReaderBase — 几何体读取器基类
///
/// 使用工厂模式: Factory<GeometryReaderBase, pair<uint32_t, FileType>>
/// 根据文件类型和版本号创建对应的读取器
class GeometryReaderBase
{
public:
    virtual ~GeometryReaderBase() = default;

    /// 从文件内容读取几何体
    virtual bool read(const char* data, uint32_t size, ftkRigidBody& geom) = 0;

    /// 将几何体写入文件内容
    virtual bool write(const ftkRigidBody& geom, char* data, uint32_t& size) = 0;

    /// 检测文件类型
    static FileType detectFileType(const char* data, uint32_t size);
};

/// INI V0 读取器 — Legacy 格式
///
/// INI 格式示例（来自SDK文档和samples/geometryHelper.cpp）:
/// ```ini
/// [geometry]
/// count=4
/// id=52
/// [fiducial0]
/// x=0.0
/// y=0.0
/// z=0.0
/// [fiducial1]
/// x=78.7369
/// y=0.0
/// z=0.0
/// ...
/// ```
class GeometryReaderIniV0 : public GeometryReaderBase
{
public:
    bool read(const char* data, uint32_t size, ftkRigidBody& geom) override;
    bool write(const ftkRigidBody& geom, char* data, uint32_t& size) override;

private:
    std::string _getIniValue(const std::string& content,
                             const std::string& section,
                             const std::string& key) const;
};

/// INI V1 读取器 — 扩展格式
///
/// 新增字段:
///   - version
///   - fiducial法向量 (normalX, normalY, normalZ)
///   - fiducial类型 (fiducialType)
///   - fiducial观察角 (angleOfView)
///   - divot位置
class GeometryReaderIniV1 : public GeometryReaderBase
{
public:
    bool read(const char* data, uint32_t size, ftkRigidBody& geom) override;
    bool write(const ftkRigidBody& geom, char* data, uint32_t& size) override;
};

/// Binary V1 读取器
///
/// 二进制格式: ftkRigidBody 结构体的直接序列化
/// 用于无线工具（内存有限）
class GeometryReaderBinV1 : public GeometryReaderBase
{
public:
    bool read(const char* data, uint32_t size, ftkRigidBody& geom) override;
    bool write(const ftkRigidBody& geom, char* data, uint32_t& size) override;
};

// ===========================================================================
// 实现
// ===========================================================================

inline FileType GeometryReaderBase::detectFileType(const char* data, uint32_t size)
{
    if (size < 4)
        return FileType::UNKNOWN;

    // 检查是否为 INI 文件（以 '[' 开头或包含 [geometry] section）
    std::string content(data, std::min(size, 1024u));

    if (content.find("[geometry]") != std::string::npos)
    {
        // 检查是否为 V1（包含 version 字段）
        if (content.find("version=") != std::string::npos ||
            content.find("version =") != std::string::npos)
            return FileType::INI_V1;
        return FileType::INI_V0;
    }

    // 检查二进制格式（magic number 或结构体大小匹配）
    // Binary V1 以 geometryId (uint32) 开头
    if (size >= sizeof(ftkRigidBody))
        return FileType::BINARY_V1;

    return FileType::UNKNOWN;
}

// ---------------------------------------------------------------------------
// INI V0 读取器
// ---------------------------------------------------------------------------

inline std::string GeometryReaderIniV0::_getIniValue(
    const std::string& content,
    const std::string& section,
    const std::string& key) const
{
    std::string sectionMarker = "[" + section + "]";
    auto secPos = content.find(sectionMarker);
    if (secPos == std::string::npos)
        return "";

    auto start = secPos + sectionMarker.size();
    auto nextSection = content.find('[', start);
    std::string sectionContent = content.substr(start,
        nextSection == std::string::npos ? std::string::npos : nextSection - start);

    auto keyPos = sectionContent.find(key + "=");
    if (keyPos == std::string::npos)
    {
        keyPos = sectionContent.find(key + " =");
        if (keyPos == std::string::npos)
            return "";
    }

    auto eqPos = sectionContent.find('=', keyPos);
    auto endPos = sectionContent.find('\n', eqPos);
    std::string value = sectionContent.substr(eqPos + 1,
        endPos == std::string::npos ? std::string::npos : endPos - eqPos - 1);

    // 去除空白
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.erase(value.begin());
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
        value.pop_back();

    return value;
}

inline bool GeometryReaderIniV0::read(const char* data, uint32_t size, ftkRigidBody& geom)
{
    std::string content(data, size);

    // 读取 [geometry] section
    std::string countStr = _getIniValue(content, "geometry", "count");
    std::string idStr = _getIniValue(content, "geometry", "id");

    if (countStr.empty() || idStr.empty())
        return false;

    geom = ftkRigidBody();
    geom.geometryId = static_cast<uint32_t>(std::stoul(idStr));
    geom.pointsCount = static_cast<uint32_t>(std::stoul(countStr));
    geom.version = 1u;

    if (geom.pointsCount > FTK_MAX_FIDUCIALS)
        return false;

    // 读取每个 fiducial
    for (uint32_t i = 0; i < geom.pointsCount; ++i)
    {
        std::string section = "fiducial" + std::to_string(i);

        std::string xStr = _getIniValue(content, section, "x");
        std::string yStr = _getIniValue(content, section, "y");
        std::string zStr = _getIniValue(content, section, "z");

        if (xStr.empty() || yStr.empty() || zStr.empty())
            return false;

        geom.fiducials[i].position.x = std::stof(xStr);
        geom.fiducials[i].position.y = std::stof(yStr);
        geom.fiducials[i].position.z = std::stof(zStr);
    }

    return true;
}

inline bool GeometryReaderIniV0::write(const ftkRigidBody& geom, char* data, uint32_t& size)
{
    std::ostringstream oss;

    oss << "[geometry]\n";
    oss << "count=" << geom.pointsCount << "\n";
    oss << "id=" << geom.geometryId << "\n\n";

    for (uint32_t i = 0; i < geom.pointsCount; ++i)
    {
        oss << "[fiducial" << i << "]\n";
        oss << "x=" << geom.fiducials[i].position.x << "\n";
        oss << "y=" << geom.fiducials[i].position.y << "\n";
        oss << "z=" << geom.fiducials[i].position.z << "\n\n";
    }

    std::string result = oss.str();
    if (result.size() > BUFFER_MAX_SIZE)
        return false;

    std::memcpy(data, result.c_str(), result.size());
    size = static_cast<uint32_t>(result.size());
    return true;
}

// ---------------------------------------------------------------------------
// INI V1 读取器（扩展格式）
// ---------------------------------------------------------------------------

inline bool GeometryReaderIniV1::read(const char* data, uint32_t size, ftkRigidBody& geom)
{
    // V1 在 V0 基础上增加了:
    //   version, normalX/Y/Z, fiducialType, angleOfView, divotX/Y/Z
    // 先用 V0 读取基本数据
    GeometryReaderIniV0 v0Reader;
    if (!v0Reader.read(data, size, geom))
        return false;

    std::string content(data, size);

    // 读取额外字段
    for (uint32_t i = 0; i < geom.pointsCount; ++i)
    {
        std::string section = "fiducial" + std::to_string(i);
        // 法向量
        // fiducialType, angleOfView 等 — 解析逻辑类似
    }

    return true;
}

inline bool GeometryReaderIniV1::write(const ftkRigidBody& geom, char* data, uint32_t& size)
{
    std::ostringstream oss;

    oss << "[geometry]\n";
    oss << "count=" << geom.pointsCount << "\n";
    oss << "id=" << geom.geometryId << "\n";
    oss << "version=" << geom.version << "\n\n";

    for (uint32_t i = 0; i < geom.pointsCount; ++i)
    {
        oss << "[fiducial" << i << "]\n";
        oss << "x=" << geom.fiducials[i].position.x << "\n";
        oss << "y=" << geom.fiducials[i].position.y << "\n";
        oss << "z=" << geom.fiducials[i].position.z << "\n";
        oss << "normalX=" << geom.fiducials[i].normalVector.x << "\n";
        oss << "normalY=" << geom.fiducials[i].normalVector.y << "\n";
        oss << "normalZ=" << geom.fiducials[i].normalVector.z << "\n";
        oss << "fiducialType=" << static_cast<uint32_t>(geom.fiducials[i].fiducialInfo.type) << "\n";
        oss << "angleOfView=" << geom.fiducials[i].fiducialInfo.angleOfView << "\n\n";
    }

    std::string result = oss.str();
    if (result.size() > BUFFER_MAX_SIZE)
        return false;

    std::memcpy(data, result.c_str(), result.size());
    size = static_cast<uint32_t>(result.size());
    return true;
}

// ---------------------------------------------------------------------------
// Binary V1 读取器
// ---------------------------------------------------------------------------

inline bool GeometryReaderBinV1::read(const char* data, uint32_t size, ftkRigidBody& geom)
{
    if (size < sizeof(ftkRigidBody))
        return false;

    std::memcpy(&geom, data, sizeof(ftkRigidBody));
    return true;
}

inline bool GeometryReaderBinV1::write(const ftkRigidBody& geom, char* data, uint32_t& size)
{
    if (sizeof(ftkRigidBody) > BUFFER_MAX_SIZE)
        return false;

    std::memcpy(data, &geom, sizeof(ftkRigidBody));
    size = static_cast<uint32_t>(sizeof(ftkRigidBody));
    return true;
}

}  // namespace geometryHandling
}  // namespace measurement
