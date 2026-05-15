// Copyright Edge26. All Rights Reserved.
#pragma once

#include "Math/Fixed.h"
#include "Math/FixedAngle.h"

namespace edge26 { namespace SimMath {

// CORDIC-based atan2. FIXED 20 iterations.
FixedAngle Atan2(Fixed64 y, Fixed64 x);

} }  // namespace edge26::SimMath
