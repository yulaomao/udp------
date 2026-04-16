// ===========================================================================
// 逆向工程还原 — ColVector 独立头文件
// RTTI: .?AV?$ColVector@N$01@math@measurement@@  → ColVector<double,2>
// RTTI: .?AV?$ColVector@N$02@math@measurement@@  → ColVector<double,3>
// RTTI: .?AV?$ColVector@N$03@math@measurement@@  → ColVector<double,4>
//
// 该文件从 Matrix.h 中分离以保持模块化
// ===========================================================================

#pragma once

// ColVector 定义已包含在 Matrix.h 中
// 此文件为独立包含提供便利

#include "Matrix.h"

namespace measurement {
namespace math {

// 类型别名已在 Matrix.h 中定义:
//   using Vec2d = ColVector<double, 2>;
//   using Vec3d = ColVector<double, 3>;
//   using Vec4d = ColVector<double, 4>;

}  // namespace math
}  // namespace measurement
