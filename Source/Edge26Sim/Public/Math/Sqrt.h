// Copyright Edge26. All Rights Reserved.
#pragma once

#include "Math/Fixed.h"

namespace edge26 {

// Minimum distance used at every unit-vector division site. Any computed distance
// below this threshold is treated as "effectively zero" and the division is skipped
// (fallback: use heading or zero velocity). 1 cm prevents huge/NaN-equivalent unit
// vectors that arise from near-zero denominators when players cluster at the ball.
constexpr Fixed64 kMinDistCm = Fixed64::FromInt(1);  // 1 cm in Q32.32

} // namespace edge26

namespace edge26 { namespace SimMath {

// Newton-Raphson sqrt with FIXED iteration count. Returns 0 for non-positive input.
Fixed64 Sqrt(Fixed64 x);

} }  // namespace edge26::SimMath
