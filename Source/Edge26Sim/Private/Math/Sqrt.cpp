// Copyright Edge26. All Rights Reserved.
#include "Math/Sqrt.h"

namespace edge26 { namespace SimMath {

Fixed64 Sqrt(Fixed64 x) {
    if (x.Raw <= 0) return Fixed64::FromRaw(0);

    // Initial guess: half of x, but at least 1 (Q32.32 raw 1<<32).
    Fixed64 g = (x.Raw > Fixed64::One)
        ? Fixed64::FromRaw(x.Raw >> 1)
        : Fixed64::FromRaw(Fixed64::One);

    // Newton-Raphson: g = (g + x/g) / 2. 8 iterations.
    for (int i = 0; i < 8; ++i) {
        Fixed64 q = x / g;
        Fixed64 sum = g + q;
        g = Fixed64::FromRaw(sum.Raw >> 1);
    }
    return g;
}

} }  // namespace edge26::SimMath
