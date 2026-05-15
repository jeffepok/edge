// Copyright Edge26. All Rights Reserved.
// Per-player role identity. Drives Layer C decision weights (FRoleWeights —
// added in M3) and formation slot mapping (FFormationSlot — added in T1.3).
#pragma once

#include <cstdint>
#include "Math/Fixed.h"

namespace edge26 {

enum class ERole : uint8_t {
    GK   = 0,   // Goalkeeper
    CB   = 1,   // Center Back
    FB_L = 2,   // Left Full Back
    FB_R = 3,   // Right Full Back
    CDM  = 4,   // Defensive Mid
    CM   = 5,   // Central Mid
    CAM  = 6,   // Attacking Mid
    W_L  = 7,   // Left Wing
    W_R  = 8,   // Right Wing
    ST   = 9,   // Striker
    Count = 10
};

struct FRoleWeights {
    // Off-ball multipliers (all in [0..2] roughly)
    Fixed32  MakeRunForward;
    Fixed32  HoldPosition;
    Fixed32  DropToReceive;
    Fixed32  ProvideWidth;
    Fixed32  Press;
    Fixed32  TrackRunner;
    Fixed32  HoldDefensiveLine;
    // On-ball multipliers
    Fixed32  PreferPass;
    Fixed32  PreferShoot;
    Fixed32  PreferDribble;
    Fixed32  PreferLongBall;
};

// Hardcoded role-weight table indexed by ERole. Defined in Roles.cpp.
extern const FRoleWeights kRoleWeightsTable[(int)ERole::Count];

}  // namespace edge26
