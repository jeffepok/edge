// Copyright Edge26. All Rights Reserved.
#pragma once

#include <cstdint>
#include "Math/FixedVec.h"

namespace edge26 {

namespace BallFlag {
    constexpr uint8_t Grounded = 1 << 0;
}

struct FSimBallState {
    FixedVec3 Position;         // 24 B world-space, cm
    FixedVec3 Velocity;         // 24 B cm/s
    FixedVec3 AngularVelocity;  // 24 B rad/s, unused v0
    uint8_t   Flags;            // 1 B
    uint8_t   _pad[7];          // explicit padding to 80 B
};
static_assert(sizeof(FSimBallState) == 80, "FSimBallState must be 80 bytes");

}  // namespace edge26
