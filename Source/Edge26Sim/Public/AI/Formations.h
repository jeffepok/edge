// Copyright Edge26. All Rights Reserved.
// Hardcoded 4-3-3 formation. v0 ships one formation per team (data-driven
// presets are a later phase). Slot positions are NORMALIZED ([-1, 1] in X
// and Y); world positions are derived per team in Formations.cpp.
#pragma once

#include <cstdint>
#include "AI/Roles.h"
#include "Math/Fixed.h"
#include "Math/FixedVec.h"

namespace edge26 {

struct FFormationSlot {
    ERole    Role;
    Fixed64  NormalizedX;   // -1 = own goal, +1 = opponent goal
    Fixed64  NormalizedY;   // -1 = left, +1 = right
};

// 11 slots. GK first, then defenders, mids, attackers.
extern const FFormationSlot kFormation_4_3_3[11];

// Compute the world position for a slot, given the team (0=home, 1=away).
// Home defends -Y end; away defends +Y end (so NormalizedX flips for away).
FixedVec3 SlotWorldPosition(int slotIndex, int teamId);

}  // namespace edge26
