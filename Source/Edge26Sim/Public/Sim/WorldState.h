// Copyright Edge26. All Rights Reserved.
#pragma once

#include <cstdint>
#include "Sim/BallState.h"
#include "Sim/PlayerState.h"
#include "AI/SpatialValueModel.h"

namespace edge26 {

constexpr int kSimPlayerCount = 22;   // 11 per team. Was 2 in Phase 1.

struct FSimWorldState {
    uint32_t            TickNumber;
    uint32_t            _pad0;
    uint64_t            RngState;
    FSimBallState       Ball;
    FSimPlayerState     Players[kSimPlayerCount];
    FSpatialValueModel  Spatial;        // M2 T2.1 — 70720 B; FMatchState added in T2.6
};
// Strict size assertion is deferred to M2 (when FMatchState + FSpatialValueModel
// are embedded). For now, validate alignment only.
static_assert(alignof(FSimWorldState) == 8, "FSimWorldState must be 8-aligned");

}  // namespace edge26
