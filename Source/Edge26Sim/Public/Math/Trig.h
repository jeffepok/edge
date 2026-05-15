// Copyright Edge26. All Rights Reserved.
#pragma once

#include "Math/FixedAngle.h"
#include "Math/Fixed.h"

namespace edge26 { namespace SimMath {

// Sin/Cos return Fixed32 in [-1.0, 1.0]. LUT-based, linear-interpolated between entries.
Fixed32 Sin(FixedAngle a);
Fixed32 Cos(FixedAngle a);

} }  // namespace edge26::SimMath
