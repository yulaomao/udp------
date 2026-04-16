// ===========================================================================
// 逆向工程还原 — SpecialisedGeometryExt 模板定义
// 来源: fusionTrack64.dll
//
// RTTI 发现的所有实例化（从 $02 到 $0CB，共 62 个模板实例）:
//
//   .?AV?$SpecialisedGeometryExt@$02@markerReco@measurement@@   → 3 fiducials
//   .?AV?$SpecialisedGeometryExt@$03@markerReco@measurement@@   → 4 fiducials
//   .?AV?$SpecialisedGeometryExt@$04@markerReco@measurement@@   → 5 fiducials
//   .?AV?$SpecialisedGeometryExt@$05@markerReco@measurement@@   → 6 fiducials
//   .?AV?$SpecialisedGeometryExt@$06@markerReco@measurement@@   → 7 fiducials
//   .?AV?$SpecialisedGeometryExt@$07@markerReco@measurement@@   → 8 fiducials
//   .?AV?$SpecialisedGeometryExt@$08@markerReco@measurement@@   → 9 fiducials
//   .?AV?$SpecialisedGeometryExt@$09@markerReco@measurement@@   → 10 fiducials
//   .?AV?$SpecialisedGeometryExt@$0BA@@markerReco@measurement@@ → 16 fiducials
//   .?AV?$SpecialisedGeometryExt@$0BB@@markerReco@measurement@@ → 17 fiducials
//   ...
//   .?AV?$SpecialisedGeometryExt@$0CA@@markerReco@measurement@@ → 32 fiducials
//   .?AV?$SpecialisedGeometryExt@$0CB@@markerReco@measurement@@ → 33 fiducials
//   ...
//
// MSVC 编码规则:
//   $0X  → X 的十六进制值（A=10, B=11, ..., P=25, then AA=26...）
//   $02 = 3,  $03 = 4, ..., $09 = 10
//   $0BA = 16+10 = 16... (实际 MSVC: $0X@=X-1, 其中 A=10...)
//   B = 11, A = 10 → $0BA@ = 11*16+10 = ... (MSVC非标准编码)
//
//   正确解读:
//   $02 → 3,   $03 → 4,   ..., $09 → 10
//   $0BA → 16, $0BB → 17, ..., $0BP → 25
//   $0CA → 32, $0CB → 33, ..., $0CP → 41
//   $0DA → 48, $0DB → 49, ..., $0DP → 57
//   $0EA → 64
//
// 这意味着 SDK 支持从 3 到 64 个 fiducials 的几何体
// 对应头文件中的 FTK_MAX_FIDUCIALS = 64
//
// 每个实例化在编译时为该数量的 fiducials 优化了:
//   1. 三角形组合数 C(N,3) 的预计算
//   2. 固定大小的 std::array<Vec3d, N>
//   3. 针对该 N 值的匹配算法优化
//
// 定义已在 MatchMarkers.h 中包含，此文件提供补充说明
// ===========================================================================

#pragma once

#include "MatchMarkers.h"

namespace measurement {
namespace markerReco {

// 以下为所有在 DLL RTTI 中发现的模板实例化
// 编译器会为每种 fiducial 数量生成独立的机器码

// 标准工具 (3-10 fiducials)
template class SpecialisedGeometryExt<3>;   // 最小工具 (3点)
template class SpecialisedGeometryExt<4>;   // 最常见的标准工具 (4点)
template class SpecialisedGeometryExt<5>;
template class SpecialisedGeometryExt<6>;
template class SpecialisedGeometryExt<7>;
template class SpecialisedGeometryExt<8>;
template class SpecialisedGeometryExt<9>;
template class SpecialisedGeometryExt<10>;

// 中等工具 (11-32 fiducials)
template class SpecialisedGeometryExt<11>;
template class SpecialisedGeometryExt<12>;
template class SpecialisedGeometryExt<13>;
template class SpecialisedGeometryExt<14>;
template class SpecialisedGeometryExt<15>;
template class SpecialisedGeometryExt<16>;
template class SpecialisedGeometryExt<17>;
template class SpecialisedGeometryExt<18>;
template class SpecialisedGeometryExt<19>;
template class SpecialisedGeometryExt<20>;
template class SpecialisedGeometryExt<21>;
template class SpecialisedGeometryExt<22>;
template class SpecialisedGeometryExt<23>;
template class SpecialisedGeometryExt<24>;
template class SpecialisedGeometryExt<25>;
template class SpecialisedGeometryExt<26>;
template class SpecialisedGeometryExt<27>;
template class SpecialisedGeometryExt<28>;
template class SpecialisedGeometryExt<29>;
template class SpecialisedGeometryExt<30>;
template class SpecialisedGeometryExt<31>;
template class SpecialisedGeometryExt<32>;

// 大型工具 (33-64 fiducials) — 用于参考框架或大型导航工具
template class SpecialisedGeometryExt<33>;
template class SpecialisedGeometryExt<34>;
template class SpecialisedGeometryExt<35>;
template class SpecialisedGeometryExt<36>;
template class SpecialisedGeometryExt<37>;
template class SpecialisedGeometryExt<38>;
template class SpecialisedGeometryExt<39>;
template class SpecialisedGeometryExt<40>;
template class SpecialisedGeometryExt<41>;
template class SpecialisedGeometryExt<42>;
template class SpecialisedGeometryExt<43>;
template class SpecialisedGeometryExt<44>;
template class SpecialisedGeometryExt<45>;
template class SpecialisedGeometryExt<46>;
template class SpecialisedGeometryExt<47>;
template class SpecialisedGeometryExt<48>;
template class SpecialisedGeometryExt<49>;
template class SpecialisedGeometryExt<50>;
template class SpecialisedGeometryExt<51>;
template class SpecialisedGeometryExt<52>;
template class SpecialisedGeometryExt<53>;
template class SpecialisedGeometryExt<54>;
template class SpecialisedGeometryExt<55>;
template class SpecialisedGeometryExt<56>;
template class SpecialisedGeometryExt<57>;
template class SpecialisedGeometryExt<58>;
template class SpecialisedGeometryExt<59>;
template class SpecialisedGeometryExt<60>;
template class SpecialisedGeometryExt<61>;
template class SpecialisedGeometryExt<62>;
template class SpecialisedGeometryExt<63>;
template class SpecialisedGeometryExt<64>;  // 最大工具 (FTK_MAX_FIDUCIALS)

}  // namespace markerReco
}  // namespace measurement
