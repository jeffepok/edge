// Copyright Edge26. All Rights Reserved.
// Angle wrapped in Fixed32, always normalized to [-π, π).
#pragma once

#include "Math/Fixed.h"

namespace edge26 {

struct FixedAngle {
    Fixed32 Raw;

    // π in Q16.16 = round(π * 65536) = 205887
    static constexpr int32_t PiRaw()    { return 205887; }
    static constexpr int32_t TwoPiRaw() { return 2 * PiRaw(); }

    static constexpr FixedAngle Zero() { return FixedAngle{Fixed32::FromRaw(0)}; }

    // Construct from raw fixed (any value); normalizes to [-π, π).
    static FixedAngle FromRaw(int32_t r) {
        int32_t twoPi = TwoPiRaw();
        int32_t pi    = PiRaw();
        r = r % twoPi;
        if (r >=  pi) r -= twoPi;
        if (r <  -pi) r += twoPi;
        return FixedAngle{Fixed32::FromRaw(r)};
    }

    FixedAngle operator+(FixedAngle o) const { return FromRaw(Raw.Raw + o.Raw.Raw); }
    FixedAngle operator-(FixedAngle o) const { return FromRaw(Raw.Raw - o.Raw.Raw); }
    FixedAngle operator-()              const { return FromRaw(-Raw.Raw); }
    bool       operator==(FixedAngle o) const { return Raw.Raw == o.Raw.Raw; }
};

}  // namespace edge26
