// Copyright Edge26. All Rights Reserved.
#pragma once

#include <cstdint>
#include "Sim/BallState.h"
#include "Sim/PlayerState.h"
#include "Sim/MatchState.h"
#include "AI/SpatialValueModel.h"

namespace edge26 {

constexpr int kSimPlayerCount = 22;   // 11 per team. Was 2 in Phase 1.

struct FSimWorldState {
    uint32_t            TickNumber;        // offset 0,  size 4
    uint32_t            _pad0;             // offset 4,  size 4
    uint64_t            RngState;          // offset 8,  size 8
    FSimBallState       Ball;              // offset 16, size 80
    FSimPlayerState     Players[kSimPlayerCount];  // offset 96, size 22×88=1936
    FMatchState         Match;             // offset 2032, size 184  (M2 T2.6)
    FSpatialValueModel  Spatial;           // offset 2216, size 70720 (M2 T2.1)
};
static_assert(alignof(FSimWorldState) == 8, "FSimWorldState must be 8-aligned");
// Size: 4+4+8+80+22×88+184+70720 = 72936 B (T2.6+T2.7; spec estimated 72944, diff=8)
static_assert(sizeof(FSimWorldState) == 72936, "FSimWorldState size locked to 72936 B (M2)");

}  // namespace edge26
