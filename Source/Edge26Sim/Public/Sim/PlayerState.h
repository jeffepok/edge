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
    FixedVec3   Position;            // 24 B (offset 0)
    FixedVec3   Velocity;            // 24 B (offset 24)
    FixedAngle  Heading;             //  4 B (offset 48)
    FixedAngle  FacingTarget;        //  4 B (offset 52)
    uint8_t     ControllerIndex;     //  1 B (offset 56) — vestigial; see Match.HumanControlledIndex (M1.5)
    uint8_t     Flags;               //  1 B (offset 57) — Grounded, Sprinting
    uint8_t     TeamId;              //  1 B (offset 58) — 0 = home, 1 = away
    uint8_t     RoleId;              //  1 B (offset 59) — ERole
    uint8_t     CurrentIntent;       //  1 B (offset 60) — EIntent (written by Layer C; 0 until M3)
    uint8_t     IntendedPassTarget;  //  1 B (offset 61) — player idx (0xFF if none)
    uint8_t     _pad[2];             //  2 B (offset 62-63) — explicit alignment
    FixedVec3   AITargetPosition;    // 24 B (offset 64-87) — where AI wants the player to go
};
static_assert(sizeof(FSimPlayerState) == 88, "FSimPlayerState must be 88 bytes");

}  // namespace edge26
