// Copyright Edge26. All Rights Reserved.
// Per-player role identity. Drives Layer C decision weights (FRoleWeights —
// added in M3) and formation slot mapping (FFormationSlot — added in T1.3).
#pragma once

#include <cstdint>

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

}  // namespace edge26
