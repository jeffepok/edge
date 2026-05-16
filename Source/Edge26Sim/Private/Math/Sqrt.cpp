// Copyright Edge26. All Rights Reserved.
#include "Math/Sqrt.h"

namespace edge26 { namespace SimMath {

Fixed64 Sqrt(Fixed64 x) {
    if (x.Raw <= 0) return Fixed64::FromRaw(0);

    // Initial guess: 2^(msb(x)/2). For Q32.32, that's roughly
    // raw = 1 << ((msb(x.Raw) + 32) / 2). Much closer to sqrt than x/2 for
    // large x; Newton-Raphson then converges in a handful of iterations
    // regardless of magnitude. Without this, 8 iters left a ~10x error on
    // distances >= 1000 cm (Phase 1 sqrt was only ever called on
    // small values; Phase 2 needs the full range up to PitchHalfLen).
    int msb = 0;
    for (int64_t v = x.Raw; v > 1; v >>= 1) ++msb;   // floor(log2(x.Raw))
    int shift = (msb + 32) / 2;                       // (msb + log2(2^32)) / 2
    if (shift < 0) shift = 0;
    if (shift > 62) shift = 62;
    Fixed64 g = Fixed64::FromRaw((int64_t)1 << shift);

    // Newton-Raphson: g = (g + x/g) / 2. 16 iterations is overkill given a
    // good seed but cheap (each iter is one 128/64 div + one add); ensures
    // convergence across the full Q32.32 input range.
    for (int i = 0; i < 16; ++i) {
        Fixed64 q = x / g;
        Fixed64 sum = g + q;
        g = Fixed64::FromRaw(sum.Raw >> 1);
    }
    return g;
}

} }  // namespace edge26::SimMath
