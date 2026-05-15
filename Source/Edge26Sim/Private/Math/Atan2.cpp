// Copyright Edge26. All Rights Reserved.
// CORDIC atan2 with 20 fixed iterations. Operates on the raw Q32.32 components.
#include "Math/Atan2.h"

namespace edge26 { namespace SimMath {

// atanTable[i] = atan(2^-i) in Q16.16, for i = 0..19.
static constexpr int32_t kAtanTable[20] = {
    51472, 30386, 16055, 8150, 4091, 2047, 1024, 512, 256, 128,
    64, 32, 16, 8, 4, 2, 1, 0, 0, 0
};

FixedAngle Atan2(Fixed64 y, Fixed64 x) {
    if (x.Raw == 0 && y.Raw == 0) return FixedAngle::Zero();

    int32_t rotation = 0;  // accumulated angle in Q16.16

    int32_t xs = (x.Raw < 0) ? -1 : 1;
    int32_t ys = (y.Raw < 0) ? -1 : 1;
    int64_t xa = (x.Raw < 0) ? -x.Raw : x.Raw;
    int64_t ya = (y.Raw < 0) ? -y.Raw : y.Raw;

    int64_t cx = xa;
    int64_t cy = ya;
    for (int i = 0; i < 20; ++i) {
        int64_t dx, dy;
        if (cy > 0) {
            dx = cx + (cy >> i);
            dy = cy - (cx >> i);
            rotation += kAtanTable[i];
        } else {
            dx = cx - (cy >> i);
            dy = cy + (cx >> i);
            rotation -= kAtanTable[i];
        }
        cx = dx;
        cy = dy;
    }

    int32_t result = rotation;
    if (xs < 0 && ys >= 0) result =  FixedAngle::PiRaw() - result;       // Q2
    else if (xs < 0 && ys < 0) result = -FixedAngle::PiRaw() + result;   // Q3
    else if (xs >= 0 && ys < 0) result = -result;                        // Q4

    return FixedAngle::FromRaw(result);
}

} }  // namespace edge26::SimMath
