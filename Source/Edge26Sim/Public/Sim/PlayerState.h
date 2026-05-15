// Copyright Edge26. All Rights Reserved.
#pragma once

#include <cstdint>
#include "Math/FixedVec.h"
#include "Math/FixedAngle.h"

namespace edge26 {

namespace PlayerFlag {
    constexpr uint8_t Grounded  = 1 << 0;
    constexpr uint8_t Sprinting = 1 << 1;
}

constexpr uint8_t kStationaryController = 0xFF;

struct FSimPlayerState {
    FixedVec3   Position;        // 24 B
    FixedVec3   Velocity;        // 24 B
    FixedAngle  Heading;         // 4 B
    FixedAngle  FacingTarget;    // 4 B
    uint8_t     ControllerIndex; // 1 B; 0=P1, 1=P2, 0xFF=stationary
    uint8_t     Flags;           // 1 B
    uint8_t     _pad[6];         // explicit trailing padding to 64 B (8-aligned)
};
static_assert(sizeof(FSimPlayerState) == 64, "FSimPlayerState must be 64 bytes");

}  // namespace edge26
