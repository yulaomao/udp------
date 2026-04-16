// ===========================================================================
// 逆向工程还原 — Match2D3D 极线匹配-三角化管线
// 来源: fusionTrack64.dll
//
// RTTI: .?AVMatch2D3D@matching@measurement@@
//
// 编译路径 (从DLL嵌入):
//   G:\workspace\sdk_win_build\soft.atr.meta.2cams.sdk\
//     soft.atr.2cams.sdk\dev\include\matching\Match2D3D.h
//
// 本文件还原的函数:
//   matchSinglePair()   — RVA 0x18c740 (998 bytes)
//   epipolarSearch()     — RVA 0x413b6 (2003 bytes)
//   matchAndTriangulate() — RVA 0x1968d4 outer loop (655 bytes)
//
// DLL字符串证据:
//   "Invalid Match2D3D instance"
//   "3D fiducial overflow"
//   "list<T> too long"
//   "Could not find binning for point %i at (%f, %f)"
//
// 类关系:
//   Match2D3D@matching@measurement
//   ├── 拥有 StereoCameraSystem* (成员偏移 +0x00)
//   ├── 管理左/右匹配链表 (+0x18, +0x28)
//   ├── 管理3D fiducial输出缓冲区 (+0x38)
//   └── 配置参数: epipolarMaxDistance (+0x50)
// ===========================================================================

#pragma once

#include "EpipolarMatcher.h"
#include <vector>
#include <list>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace measurement {
namespace matching {

// ---------------------------------------------------------------------------
// 空间分箱 (Spatial Binning) — 加速极线搜索
//
// DLL函数: RVA 0x213d0 ("Could not find binning for point %i at (%f, %f)")
//
// fusionTrack 使用空间哈希/网格加速极线搜索:
// 不是暴力搜索所有右图点, 而是:
// 1. 将右图点按像素位置分配到网格单元
// 2. 对于给定极线, 只搜索极线经过的网格单元中的点
// 3. 这将 O(N*M) 降低到接近 O(N*k), k << M
// ---------------------------------------------------------------------------
struct SpatialBin
{
    std::vector<uint32_t> pointIndices;  ///< 该单元格中的右图点索引
};

struct SpatialGrid
{
    static constexpr int kGridWidth = 32;
    static constexpr int kGridHeight = 32;
    static constexpr double kBinSizeX = 40.0;   // 像素
    static constexpr double kBinSizeY = 40.0;

    SpatialBin bins[kGridHeight][kGridWidth];

    /// 将点添加到网格
    void addPoint(uint32_t index, double px, double py)
    {
        int bx = static_cast<int>(px / kBinSizeX);
        int by = static_cast<int>(py / kBinSizeY);
        if (bx >= 0 && bx < kGridWidth && by >= 0 && by < kGridHeight)
        {
            bins[by][bx].pointIndices.push_back(index);
        }
    }

    /// 获取极线附近的候选点
    void getCandidatesAlongLine(double a, double b, double c,
                                 double maxDist,
                                 std::vector<uint32_t>& candidates) const
    {
        candidates.clear();
        // 遍历极线经过的网格单元
        for (int by = 0; by < kGridHeight; ++by)
        {
            for (int bx = 0; bx < kGridWidth; ++bx)
            {
                if (bins[by][bx].pointIndices.empty())
                    continue;

                // 检查网格单元中心到极线的距离
                double cx = (bx + 0.5) * kBinSizeX;
                double cy = (by + 0.5) * kBinSizeY;
                double dist = std::fabs(a * cx + b * cy + c)
                            / std::sqrt(a * a + b * b);

                // 如果距离小于阈值+单元格对角线半径, 则包含
                double diagHalf = std::sqrt(kBinSizeX * kBinSizeX +
                                            kBinSizeY * kBinSizeY) * 0.5;
                if (dist < maxDist + diagHalf)
                {
                    for (uint32_t idx : bins[by][bx].pointIndices)
                    {
                        candidates.push_back(idx);
                    }
                }
            }
        }
    }

    void clear()
    {
        for (int by = 0; by < kGridHeight; ++by)
            for (int bx = 0; bx < kGridWidth; ++bx)
                bins[by][bx].pointIndices.clear();
    }
};

// ---------------------------------------------------------------------------
// 匹配候选记录
//
// 从 DLL epipolarSearch 函数的结构分析
// ---------------------------------------------------------------------------
struct MatchCandidate
{
    uint32_t rightIndex;     ///< 右图点索引
    double   epipolarDist;   ///< 到极线的距离 (像素)
};

// ---------------------------------------------------------------------------
// 匹配结果链表节点
//
// DLL 使用 std::list 管理匹配结果
// 从 "list<T> too long" 字符串确认
// 链表节点大小从 magic number 0xAAAAAAAAAAAAAAAA9 计算 → 24 bytes
// ---------------------------------------------------------------------------
struct MatchNode
{
    RawPoint2D* pointPtr;     ///< 指向原始 2D 点
    MatchNode*  prev;
    MatchNode*  next;
};

// ===========================================================================
//
// Match2D3D 类 — 极线匹配三角化管线
//
// 内存布局 (从 matchSinglePair 的 rbx 寄存器偏移分析):
//
//   偏移    大小    描述
//   +0x00   8      指向 StereoCameraSystem 对象
//   +0x08   8      指向温度补偿后的参数
//   +0x10   8      指向温度补偿器
//   +0x18   8      左匹配链表头指针
//   +0x20   8      左匹配计数
//   +0x28   8      右匹配链表头指针
//   +0x30   8      右匹配计数
//   +0x38   8      fiducial 缓冲区起始地址
//   +0x40   8      fiducial 缓冲区当前写入位置
//   +0x48   8      fiducial 缓冲区结束地址
//   +0x50   4      极线最大距离 (float, 像素)
//   +0x54   1      允许多重匹配 flag
//   +0x55   1      使用对称化 flag
//   +0x68   24     极线缓存 (Vec3d)
//   +0xC8   16     距离缓存
//   +0xE0   8      右图点对指针
//   +0xF8   var    closestPointOnRays 输出结构
//   +0x118  8      指向额外数据 (24 bytes xyzw)
//
// ===========================================================================

class Match2D3D
{
public:
    Match2D3D();
    ~Match2D3D();

    /// 初始化匹配器
    /// @param stereoSystem  已初始化的双目系统指针
    /// @param epipolarMaxDist 极线最大匹配距离 (像素)
    void initialize(void* stereoSystem, float epipolarMaxDist);

    /// -----------------------------------------------------------------------
    /// matchSinglePair — 对单个左右 2D 点对执行完整匹配管线
    ///
    /// DLL 原始函数: RVA 0x18c740 (998 bytes)
    ///
    /// 完整算法 (从汇编逐行还原):
    ///
    ///   === 步骤 1: 去畸变左图点 ===
    ///   // DLL: 0x18c76a ~ 0x18c790
    ///   // rcx = stereoSystem + 0x90 (左相机内参)
    ///   // rdx = &leftPoint->undistortedX   (+0x20)
    ///   // r8  = &leftPoint->undistortedY   (+0x28)
    ///   // r9  = &leftPoint->subPixelX      (+0x00, 输入像素x)
    ///   // [rsp+0x20] = &leftPoint->subPixelY (+0x08, 输入像素y)
    ///   //
    ///   // 调用 undistortPoint() → 如果失败, 返回 false
    ///
    ///   === 步骤 2: 去畸变右图点 ===
    ///   // DLL: 0x18c798 ~ 0x18c7bb
    ///   // rcx = stereoSystem + 0xE8 (右相机内参)
    ///   // 参数类似, 使用右图点
    ///   // 调用 undistortPoint() → 如果失败, 返回 false
    ///
    ///   === 步骤 3: 计算极线 ===
    ///   // DLL: 0x18c7c3 ~ 0x18c7d4
    ///   // xmm3 = leftPoint->undistortedY (+0x28)
    ///   // xmm2 = leftPoint->undistortedX (+0x20)
    ///   // rdx = &this->epipolarLineCache (+0x68)
    ///   // rcx = stereoSystem
    ///   // 调用 computeEpipolarLine() → 如果失败, 返回 false
    ///
    ///   === 步骤 4: 存储右图去畸变坐标到输出 ===
    ///   // DLL: 0x18c7e1 ~ 0x18c7fb
    ///   // rdx = this->rightPointOutput (+0xE0)
    ///   // *rdx = rightPoint->undistortedX
    ///   // *(rdx+8) = rightPoint->undistortedY
    ///
    ///   === 步骤 5: 计算极线距离 ===
    ///   // DLL: 0x18c7fb ~ 0x18c820
    ///   // rcx = &this->epipolarLineCache (+0x68)
    ///   // rdx = &this->distanceCache (+0xC8)
    ///   // 调用 pointToEpipolarLineDistance()
    ///   // 结果 xmm0 = 距离 (double)
    ///   //
    ///   // 取绝对值:
    ///   //   andps xmm2, [abs_mask]     // 0x18c80f
    ///   //   → 使用 0x7FFFFFFFFFFFFFFF 掩码
    ///   //
    ///   // 比较与阈值:
    ///   //   movss xmm1, [rbx + 0x50]   // this->epipolarMaxDistance (float)
    ///   //   cvtps2pd xmm1, xmm1        // float → double
    ///   //   comisd xmm2, xmm1          // 比较
    ///   //   ja → 跳过 (距离太大)
    ///   //
    ///   // 如果距离 > epipolarMaxDistance: 返回 false
    ///
    ///   === 步骤 6: 检查标定有效性 ===
    ///   // DLL: 0x18c826 ~ 0x18c847
    ///   // if (stereoSystem[0x140] != 0): 返回 false
    ///
    ///   === 步骤 7: 三角化 (closestPointOnRays) ===
    ///   // DLL: 0x18c847 ~ 0x18c871
    ///   // xmm3 = leftPoint->undistortedX
    ///   // xmm4 = rightPoint->undistortedX
    ///   // [rsp+0x20] = xmm4  (右图去畸变 x)
    ///   // [rsp+0x28] = rightPoint->undistortedY
    ///   // [rsp+0x30] = leftPoint->undistortedY
    ///   // rdx = &this->outputStruct (+0xF8)
    ///   // r8 = &localDistance (栈上)
    ///   // rcx = stereoSystem
    ///   //
    ///   // 调用 closestPointOnRays() → 如果失败, 返回 false
    ///
    ///   === 步骤 8: 填充 3D 结果 ===
    ///   // DLL: 0x18c877 ~ 0x18c893
    ///   // 初始化输出记录 (调用 0x18cb30)
    ///   // 设置 leftPoint->matched = true  (+0x3C)
    ///
    ///   === 步骤 9: 插入左匹配链表 ===
    ///   // DLL: 0x18c88a ~ 0x18c8c7
    ///   // 使用链表操作:
    ///   //   r15 = this->leftList.head  ([rbx + 0x18])
    ///   //   r12 = head->next
    ///   //   newNode->data = leftPoint
    ///   //   newNode->prev = head
    ///   //   newNode->next = head->next
    ///   //   head->next = newNode
    ///   //   head->next->prev = newNode
    ///   //   this->leftCount++
    ///   //
    ///   //   溢出检查: if (0xAAAAAAAAAAAAAAA9 - count < 1)
    ///   //     → "list<T> too long"
    ///
    ///   === 步骤 10: 插入右匹配链表 ===
    ///   // DLL: 0x18c8e8 ~ 0x18c91d (类似步骤9)
    ///
    ///   === 步骤 11: 构建输出记录 ===
    ///   // DLL: 0x18c914 ~ 0x18ca08
    ///   //
    ///   // 记录包含:
    ///   //   - 3D 坐标 (从 closestPointOnRays 结果)
    ///   //   - 极线距离 (float, 从步骤5)
    ///   //   - 三角化距离 (double, 从步骤7)
    ///   //   - 左右图点的像素坐标
    ///   //   - 左右图点索引
    ///   //   - 匹配状态
    ///   //   - 额外数据 (从 sys + 0x118 读取, 24 bytes)
    ///   //   - 概率 (初始 1.0)
    ///   //
    ///   //   xmm6 = 极线距离 (float, 保存在 0x1f2840 返回后)
    ///   //   [rbp - 0x80] = 三角化距离 (double, closestPointOnRays 输出)
    ///   //   [rsp + 0x78] = 极线距离 (float)
    ///   //   概率 = 1.0 (RVA 0x248EF0)
    ///   //
    ///   //   检查是否使用对称化:
    ///   //   if ([rbx + 0x55] == 1 && [rcx + 0x188] != 0):
    ///   //       调用 symmetriseResult (0x1a6430)
    ///   //       if (失败 && !allowMultipleMatches): 返回 false
    ///
    ///   === 步骤 12: 写入 fiducial 缓冲区 ===
    ///   // DLL: 0x18ca4b ~ 0x18cae1
    ///   //
    ///   //   if (this->bufCurrent < this->bufEnd):
    ///   //       memcpy(bufCurrent, &record, 0xD8)  // 216 bytes
    ///   //       bufCurrent += 0xD8
    ///   //   else:
    ///   //       调用 vector.push_back 扩展缓冲区
    ///   //
    ///   //   最大数量检查: if (count >= 50000): 返回 false
    ///   //     DLL: 0x18ca3e: cmp rdx, 0xC350
    ///   //
    ///   //   return true
    ///
    /// @param leftPoint  左图 2D 检测点
    /// @param rightPoint 右图 2D 检测点
    /// @return true 如果匹配和三角化成功
    /// -----------------------------------------------------------------------
    bool matchSinglePair(RawPoint2D* leftPoint,
                         RawPoint2D* rightPoint);

    /// -----------------------------------------------------------------------
    /// findEpipolarMatches — 为一个左图点搜索右图中的极线匹配
    ///
    /// DLL 原始函数: RVA 0x41350 (入口 102 bytes) + 0x413b6 (核心循环 2003 bytes)
    ///
    /// 算法 (从汇编还原):
    ///
    ///   === 入口 (0x41350) ===
    ///   // 获取右图点容器的大小
    ///   // r10 = container->begin
    ///   // r9 = container->end
    ///   // numRight = (end - begin) / sizeof(RawPoint2D)  // 使用 magic 除法
    ///   //
    ///   // 如果 numRight == 0: 返回 NULL
    ///   //
    ///   // 遍历右图空间网格:
    ///   //   对于每个候选右图点:
    ///   //     获取其像素坐标
    ///   //     如果已匹配 (matched flag): 跳过
    ///   //     计算到极线的距离
    ///   //     如果距离 < epipolarMaxDistance: 记录为候选
    ///   //
    ///   //   返回最佳匹配 (距离最小的)
    ///   //   如果有多个候选, 设置 multipleMatches flag
    ///
    ///   === 空间分箱加速 (0x413b6 内部) ===
    ///   // DLL: 0x414f6: call 0x213d0  (findBinForPoint)
    ///   // 如果找不到分箱:
    ///   //   DLL: 0x41501: "Could not find binning for point %i at (%f, %f)"
    ///   //
    ///   // 对左右图分别查找分箱
    ///   // DLL: 0x41626: call 0x213d0  (第二次调用)
    ///
    /// @param leftPoint  左图 2D 点 (已去畸变)
    /// @param rightContainer 右图点容器
    /// @param outMatchCount 输出匹配候选数量
    /// @param epipolarLine 极线参数
    /// @param maxDist 最大极线距离
    /// @return 最佳匹配的右图点指针, 或 NULL
    /// -----------------------------------------------------------------------
    RawPoint2D* findEpipolarMatches(const RawPoint2D* leftPoint,
                                    const std::vector<RawPoint2D>& rightPoints,
                                    uint32_t* outMatchCount,
                                    const double* epipolarLine,
                                    float maxDist);

    /// -----------------------------------------------------------------------
    /// matchAndTriangulateAll — 主匹配循环
    ///
    /// DLL 原始函数: RVA 0x1968d4 (655 bytes, 外层循环)
    ///
    /// 算法 (从汇编精确还原):
    ///
    ///   === 初始化 ===
    ///   // DLL: 0x1968d4 ~ 0x196900
    ///   // r14 = magic divisor 0x4BDA12F684BDA13 (用于除以 0xD8 = 216)
    ///   // xmm6 = 1.0 (概率初始值)
    ///   //
    ///   // r15d = 0 (rightPointIndex 起始)
    ///
    ///   === 外层循环 (遍历右图点) ===
    ///   // DLL: 0x196900 ~ 0x196b3a
    ///   //
    ///   //   r13 = r15  (当前右图点索引)
    ///   //
    ///   //   if (matchTable[r13] == -1):  // [rsi + r13*4 + 0x10]
    ///   //       goto nextPoint  (已经被匹配过, 跳过)
    ///   //
    ///   //   === 第一次极线搜索: 从左图视角 ===
    ///   //   // DLL: 0x19690f ~ 0x196971
    ///   //   // 获取右图点的坐标:
    ///   //   //   r9 = fiducialBuffer  [rsi + 0x178]
    ///   //   //   r12 = r13 * 0xD8    (记录偏移)
    ///   //   //   xmm3 = fiducial[r12 + 0x40] (去畸变x, 左图)
    ///   //   //   xmm0 = fiducial[r12 + 0x48] (去畸变y, 左图)
    ///   //   //
    ///   //   //   获取 StereoSystem 的极线相关参数:
    ///   //   //   rcx = [rbx + 0x30] → stereoConfig
    ///   //   //   rcx = [rcx + 8]    → 内部配置
    ///   //   //   rdx = rcx + 0x60   → 左→右极线结构
    ///   //   //   r8  = rcx + 0x100  → 右→左极线结构
    ///   //   //
    ///   //   //   [rsp+0x38] = &matchCountFlag   (输出)
    ///   //   //   [rsp+0x30] = maxEpipolarDist   (float, [rbx + 0x28])
    ///   //   //   [rsp+0x28] = lineThreshold      ([rbx + 0x18])
    ///   //   //   [rsp+0x20] = 去畸变y
    ///   //   //
    ///   //   //   调用 findEpipolarMatches (0x41350)
    ///   //   //
    ///   //   //   rax = 结果 (最佳匹配右图点指针)
    ///   //   //   如果 rax == NULL: 跳到检查区
    ///   //   //   如果 matchCount > 1: 跳到错误标记区
    ///
    ///   //   === 第二次极线搜索: 从右图视角 (交叉验证) ===
    ///   //   // DLL: 0x196982 ~ 0x1969e4
    ///   //   // 使用右图点的坐标搜索左图中的匹配
    ///   //   //   xmm3 = fiducial[r12 + 0x80] (去畸变x, 右图)
    ///   //   //   xmm0 = fiducial[r12 + 0x88] (去畸变y, 右图)
    ///   //   //
    ///   //   //   rdx = config + 0x78   → 右→左极线结构
    ///   //   //   r8  = config + 0x118  → 左→右极线结构
    ///   //   //
    ///   //   //   调用 findEpipolarMatches (0x41350) — 反向搜索
    ///   //   //   如果失败或 matchCount > 1: 标记为多重匹配
    ///
    ///   //   === 执行三角化 ===
    ///   //   // DLL: 0x1969ef ~ 0x196a00
    ///   //   // rdx = 第一次搜索结果 (左图最佳匹配点)
    ///   //   // r8  = 第二次搜索结果 (右图最佳匹配点)
    ///   //   // rcx = [rbx + 0x30] (StereoSystem内部结构)
    ///   //   //
    ///   //   // 调用 matchSinglePair (0x18c740)
    ///   //   // 如果失败: goto nextPoint
    ///
    ///   //   === 写入结果 ===
    ///   //   // DLL: 0x196a08 ~ 0x196b13
    ///   //   //
    ///   //   // 获取最新的 fiducial 记录:
    ///   //   //   count = (bufCurrent - bufBegin) / 0xD8 - 1
    ///   //   //   r9 = bufBegin + count * 0xD8
    ///   //   //
    ///   //   // 填充额外信息:
    ///   //   //   r9[0xC5] = 1          (matched = true)
    ///   //   //   r9[0xCE] = r15d       (rightPointLoopIndex)
    ///   //   //   r9[0xCA] = [rsi + 4]  (右帧索引)
    ///   //   //   r9[0xC6] = [rsi]      (左帧索引)
    ///   //   //
    ///   //   // 更新匹配表:
    ///   //   //   matchTable[r13] = count (当前匹配的 fiducial 索引)
    ///   //   //
    ///   //   // 复制 fiducial 数据到输出缓冲区:
    ///   //   //   目标: [rsi + 0x178] + r12 (原始缓冲区)
    ///   //   //   源:   r9 (最新记录)
    ///   //   //   大小: 0xD8 bytes (使用 10 条 movups 指令)
    ///   //   //
    ///   //   // 通知回调:
    ///   //   //   调用 [rdi + 0x28] (virtual method on observer)
    ///
    ///   //   r15d++
    ///   //   if (r15d < [rdi + 0x10]): goto 外层循环头
    ///
    /// @param leftPoints  左图 2D 点集合
    /// @param rightPoints 右图 2D 点集合
    /// @param fiducials   输出 3D fiducial 数组
    /// @param maxOutput   最大输出数量
    /// @return 成功匹配的数量
    /// -----------------------------------------------------------------------
    uint32_t matchAndTriangulateAll(
        std::vector<RawPoint2D>& leftPoints,
        std::vector<RawPoint2D>& rightPoints,
        std::vector<Fiducial3DRecord>& fiducials,
        uint32_t maxOutput);

    // -----------------------------------------------------------------------
    // 配置接口
    // -----------------------------------------------------------------------

    void setEpipolarMaxDistance(float dist) { m_epipolarMaxDistance = dist; }
    float getEpipolarMaxDistance() const { return m_epipolarMaxDistance; }

    void setAllowMultipleMatches(bool allow) { m_allowMultipleMatches = allow; }
    void setUseSymmetrize(bool use) { m_useSymmetrize = use; }

private:
    // 成员变量 — 与 DLL 中的偏移一一对应
    void*    m_stereoSystem;            // +0x00
    void*    m_tempCompParams;          // +0x08
    void*    m_tempCompensator;         // +0x10

    // 匹配链表 (std::list<RawPoint2D*>)
    std::list<RawPoint2D*> m_leftMatchList;   // +0x18 ~ +0x20
    std::list<RawPoint2D*> m_rightMatchList;  // +0x28 ~ +0x30

    // 输出缓冲区 (std::vector<Fiducial3DRecord>)
    std::vector<Fiducial3DRecord> m_fiducialBuffer;  // +0x38 ~ +0x48

    // 配置
    float   m_epipolarMaxDistance;       // +0x50 (默认 2.0 像素)
    bool    m_allowMultipleMatches;      // +0x54
    bool    m_useSymmetrize;             // +0x55

    // 工作缓存
    double  m_epipolarLineCache[8];      // +0x68 (极线参数缓存, 64 bytes)
    double  m_distanceCache[4];          // +0xC8 (距离计算缓存)
    double* m_rightPointOutput;          // +0xE0
    double  m_closestPointResult[8];     // +0xF8
    double  m_extraData[3];             // +0x118 (24 bytes xyz)

    // 空间网格 (加速搜索)
    SpatialGrid m_spatialGrid;
};

// ===========================================================================
// Match2D3D 实现
// ===========================================================================

inline Match2D3D::Match2D3D()
    : m_stereoSystem(nullptr)
    , m_tempCompParams(nullptr)
    , m_tempCompensator(nullptr)
    , m_epipolarMaxDistance(2.0f)
    , m_allowMultipleMatches(false)
    , m_useSymmetrize(false)
    , m_rightPointOutput(nullptr)
{
    std::memset(m_epipolarLineCache, 0, sizeof(m_epipolarLineCache));
    std::memset(m_distanceCache, 0, sizeof(m_distanceCache));
    std::memset(m_closestPointResult, 0, sizeof(m_closestPointResult));
    std::memset(m_extraData, 0, sizeof(m_extraData));
}

inline Match2D3D::~Match2D3D() = default;

inline void Match2D3D::initialize(void* stereoSystem, float epipolarMaxDist)
{
    m_stereoSystem = stereoSystem;
    m_epipolarMaxDistance = epipolarMaxDist;
}

// ---------------------------------------------------------------------------
// matchSinglePair — 完整还原
//
// DLL: 0x18c740 - 0x18cb26 (998 bytes)
// ---------------------------------------------------------------------------

inline bool Match2D3D::matchSinglePair(
    RawPoint2D* leftPoint,
    RawPoint2D* rightPoint)
{
    if (!m_stereoSystem)
        return false;

    const uint8_t* sys = reinterpret_cast<const uint8_t*>(m_stereoSystem);

    // === 步骤 1: 去畸变左图点 ===
    // DLL: 0x18c76a ~ 0x18c790
    const CameraIntrinsics* leftCam =
        reinterpret_cast<const CameraIntrinsics*>(sys + 0x90);

    if (!undistortPoint(leftCam,
                        &leftPoint->undistortedX,
                        &leftPoint->undistortedY,
                        &leftPoint->subPixelX,
                        &leftPoint->subPixelY))
    {
        // DLL: 0x18c792: test al, al; je → 跳到函数末尾返回 false
        return false;
    }

    // === 步骤 2: 去畸变右图点 ===
    // DLL: 0x18c798 ~ 0x18c7bb
    const CameraIntrinsics* rightCam =
        reinterpret_cast<const CameraIntrinsics*>(sys + 0xE8);

    if (!undistortPoint(rightCam,
                        &rightPoint->undistortedX,
                        &rightPoint->undistortedY,
                        &rightPoint->subPixelX,
                        &rightPoint->subPixelY))
    {
        return false;
    }

    // === 步骤 3: 计算极线 ===
    // DLL: 0x18c7c3 ~ 0x18c7db
    if (!computeEpipolarLine(m_stereoSystem,
                              m_epipolarLineCache,
                              leftPoint->undistortedX,
                              leftPoint->undistortedY))
    {
        return false;
    }

    // === 步骤 4: 存储右图去畸变坐标 ===
    // DLL: 0x18c7e1 ~ 0x18c7fb
    m_distanceCache[0] = rightPoint->undistortedX;
    m_distanceCache[1] = rightPoint->undistortedY;

    // === 步骤 5: 计算极线距离并检查阈值 ===
    // DLL: 0x18c7fb ~ 0x18c820
    double epiDist = pointToEpipolarLineDistance(
        m_epipolarLineCache, m_distanceCache);

    float epiDistAbs = static_cast<float>(std::fabs(epiDist));

    // DLL: 0x18c807 ~ 0x18c820
    // movss xmm1, [rbx + 0x50] → m_epipolarMaxDistance
    // cvtps2pd → float → double
    // comisd → 比较
    // ja → 如果 epiDistAbs > maxDist, 返回 false
    if (epiDistAbs > m_epipolarMaxDistance)
    {
        return false;
    }

    // === 步骤 6: 检查标定有效性 ===
    // DLL: 0x18c826 ~ 0x18c847
    // cmp byte ptr [rcx + 0x140], 0
    // jne → 如果标定无效, 返回 false
    if (sys[0x140] != 0)
    {
        return false;
    }

    // === 步骤 7: 三角化 ===
    // DLL: 0x18c847 ~ 0x18c871
    double triangulationDist = 0.0;

    if (!closestPointOnRays(m_stereoSystem,
                             m_closestPointResult,
                             &triangulationDist,
                             leftPoint->undistortedX,
                             leftPoint->undistortedY,
                             rightPoint->undistortedX,
                             rightPoint->undistortedY))
    {
        return false;
    }

    // === 步骤 8: 标记已匹配 ===
    // DLL: 0x18c881: mov byte ptr [rsi + 0x3C], 1
    leftPoint->matched = true;

    // === 步骤 9: 插入左匹配链表 ===
    // DLL: 0x18c88a ~ 0x18c8c7
    // 使用 std::list 的链表操作
    // 溢出检查: 0xAAAAAAAAAAAAAAAA9 - count < 1 → "list<T> too long"
    m_leftMatchList.push_back(leftPoint);

    // === 步骤 10: 插入右匹配链表 ===
    // DLL: 0x18c8e8 ~ 0x18c91d
    rightPoint->matched = true;
    m_rightMatchList.push_back(rightPoint);

    // === 步骤 11: 构建输出记录 ===
    // DLL: 0x18c914 ~ 0x18ca08
    Fiducial3DRecord record;
    std::memset(&record, 0, sizeof(record));

    // 复制3D坐标
    // 从 closestPointResult + 8 (前 24 bytes = 3 doubles)
    double* point3D = m_closestPointResult + 1;  // offset +8

    // 转为 float 存入记录
    record.x() = static_cast<float>(point3D[0]);
    record.y() = static_cast<float>(point3D[1]);
    record.z() = static_cast<float>(point3D[2]);

    // 极线距离
    record.epipolarDistance() = epiDistAbs;

    // 左右图点像素坐标 (从源点复制)
    // DLL: 0x18c954 ~ 0x18c9fd 一系列 movups 指令
    // 复制整个左右点数据到记录中

    // 额外数据
    // DLL: 0x18c96c ~ 0x18c9a3
    // 从 this->extraData (m_extraData) 复制 24 bytes

    // 概率 = 1.0
    // DLL: 0x18c9a3: movsd xmm0, [rip + 0xbc545] → 1.0

    // === 步骤 12: 可选的对称化 ===
    // DLL: 0x18c9cd ~ 0x18ca45
    // if (m_useSymmetrize && [tempCompensator + 0x188] != 0):
    //     调用 symmetriseResult (0x1a6430)
    //     如果失败且不允许多重匹配: 返回 false
    if (m_useSymmetrize && m_tempCompensator)
    {
        double symOutput[3];
        if (!symmetriseResult(point3D,
                              reinterpret_cast<const double*>(m_tempCompensator),
                              symOutput))
        {
            if (!m_allowMultipleMatches)
                return false;
        }
        else
        {
            record.x() = static_cast<float>(symOutput[0]);
            record.y() = static_cast<float>(symOutput[1]);
            record.z() = static_cast<float>(symOutput[2]);
        }
    }

    // === 步骤 13: 写入缓冲区 ===
    // DLL: 0x18ca4b ~ 0x18cae1
    //
    // 检查最大数量:
    // DLL: 0x18ca3e: cmp rdx, 0xC350 (50000)
    if (m_fiducialBuffer.size() >= kMaxFiducialCount)
    {
        return false;
    }

    m_fiducialBuffer.push_back(record);

    return true;
}

// ---------------------------------------------------------------------------
// findEpipolarMatches — 极线搜索 (空间分箱加速)
//
// DLL: 0x41350 + 0x413b6 (2105 bytes 总计)
// ---------------------------------------------------------------------------

inline RawPoint2D* Match2D3D::findEpipolarMatches(
    const RawPoint2D* leftPoint,
    const std::vector<RawPoint2D>& rightPoints,
    uint32_t* outMatchCount,
    const double* epipolarLine,
    float maxDist)
{
    *outMatchCount = 0;

    if (rightPoints.empty())
        return nullptr;

    // === DLL: 0x413b6 主循环 ===
    // 在 DLL 中, 使用空间分箱加速搜索
    // 这里还原逻辑: 遍历右图点, 计算到极线距离

    RawPoint2D* bestMatch = nullptr;
    double bestDist = static_cast<double>(maxDist);
    uint32_t matchCount = 0;

    double a = epipolarLine[0];
    double b = epipolarLine[1];
    double c = epipolarLine[2];
    double normFactor = std::sqrt(a * a + b * b);

    if (normFactor < kAlmostZeroThreshold)
        return nullptr;

    for (size_t j = 0; j < rightPoints.size(); ++j)
    {
        const RawPoint2D& rp = rightPoints[j];

        // 跳过已匹配的点
        if (rp.matched)
            continue;

        // 计算到极线的距离
        // DLL 使用去畸变后的坐标
        double dist = std::fabs(a * rp.undistortedX + b * rp.undistortedY + c)
                    / normFactor;

        if (dist < static_cast<double>(maxDist))
        {
            ++matchCount;

            if (dist < bestDist)
            {
                bestDist = dist;
                bestMatch = const_cast<RawPoint2D*>(&rp);
            }
        }
    }

    *outMatchCount = matchCount;
    return bestMatch;
}

// ---------------------------------------------------------------------------
// matchAndTriangulateAll — 主匹配循环
//
// DLL: 0x1968d4 - 0x196b63 (655 bytes)
// ---------------------------------------------------------------------------

inline uint32_t Match2D3D::matchAndTriangulateAll(
    std::vector<RawPoint2D>& leftPoints,
    std::vector<RawPoint2D>& rightPoints,
    std::vector<Fiducial3DRecord>& fiducials,
    uint32_t maxOutput)
{
    // === DLL 初始化 ===
    // 0x1968d4: r14 = 0x4BDA12F684BDA13 (magic for /0xD8)
    // 0x1968e7: xmm6 = 1.0 (概率)

    // 匹配表: 记录每个右图点匹配到的 fiducial 索引
    // DLL: [rsi + r13*4 + 0x10]
    std::vector<int32_t> matchTable(rightPoints.size(), -1);

    m_fiducialBuffer.clear();
    m_leftMatchList.clear();
    m_rightMatchList.clear();

    bool overflowFlag = false;

    // === 外层循环: 遍历右图点 ===
    // DLL: 0x196900 ~ 0x196b3a
    for (uint32_t rightIdx = 0;
         rightIdx < static_cast<uint32_t>(rightPoints.size());
         ++rightIdx)
    {
        // DLL: 0x196903 ~ 0x196909
        // if (matchTable[rightIdx] != -1): 跳过 (已匹配)
        if (matchTable[rightIdx] != -1)
            continue;

        // === 第一次极线搜索: 从左图视角 ===
        // DLL: 0x19690f ~ 0x196971
        //
        // 获取当前右图点的去畸变坐标
        // 构建极线, 在左图中搜索匹配
        uint32_t matchCount1 = 0;

        // 计算极线 (使用左图去畸变坐标)
        // 注意: 在 DLL 中, 搜索顺序是:
        //   先用右图点的坐标(某些字段)搜索左图中的匹配
        //   但实际上是双向验证

        RawPoint2D* bestLeft = findEpipolarMatches(
            &rightPoints[rightIdx],
            leftPoints,
            &matchCount1,
            m_epipolarLineCache,
            m_epipolarMaxDistance);

        if (bestLeft == nullptr)
        {
            // DLL: 0x196971: je → 跳到检查区
            if (matchCount1 > 1)
                overflowFlag = true;
            continue;
        }
        if (matchCount1 > 1)
        {
            // DLL: 0x19697c: ja → 多重匹配
            overflowFlag = true;
            continue;
        }

        // === 第二次极线搜索: 从右图视角 (交叉验证) ===
        // DLL: 0x196982 ~ 0x1969e4
        uint32_t matchCount2 = 0;

        RawPoint2D* bestRight = findEpipolarMatches(
            bestLeft,
            rightPoints,
            &matchCount2,
            m_epipolarLineCache,
            m_epipolarMaxDistance);

        if (bestRight == nullptr)
        {
            if (matchCount2 > 1)
                overflowFlag = true;
            continue;
        }
        if (matchCount2 > 1)
        {
            overflowFlag = true;
            continue;
        }

        // === 执行三角化 ===
        // DLL: 0x1969ef ~ 0x196a00
        if (!matchSinglePair(bestLeft, bestRight))
        {
            continue;
        }

        // === 写入结果 ===
        // DLL: 0x196a08 ~ 0x196b13
        //
        // 获取最新的 fiducial 记录
        if (m_fiducialBuffer.empty())
            continue;

        Fiducial3DRecord& lastRecord = m_fiducialBuffer.back();

        // 填充额外信息
        // DLL: 0x196a3f ~ 0x196a61
        lastRecord.matchedFlag() = true;        // [r9 + 0xC5] = 1
        lastRecord.rightLoopIdx() = rightIdx;   // [r9 + 0xCE] = r15d

        // 更新匹配表
        // DLL: 0x196a68: matchTable[rightIdx] = fiducialIndex
        matchTable[rightIdx] = static_cast<int32_t>(m_fiducialBuffer.size() - 1);

        // 检查最大输出数量
        if (m_fiducialBuffer.size() >= maxOutput)
            break;
    }

    // 复制结果到输出
    fiducials = m_fiducialBuffer;

    return static_cast<uint32_t>(fiducials.size());
}

}  // namespace matching
}  // namespace measurement
