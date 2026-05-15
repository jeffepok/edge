// Copyright Edge26. All Rights Reserved.
#pragma once

#include "Math/Fixed.h"

namespace edge26 { namespace SimMath {

// Newton-Raphson sqrt with FIXED iteration count. Returns 0 for non-positive input.
Fixed64 Sqrt(Fixed64 x);

} }  // namespace edge26::SimMath
