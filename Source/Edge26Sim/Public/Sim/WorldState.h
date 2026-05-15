// Copyright Edge26. All Rights Reserved.
#pragma once

#include <cstdint>
#include "Sim/BallState.h"
#include "Sim/PlayerState.h"

namespace edge26 {

constexpr int kSimPlayerCount = 2;  // v0 hardcoded; becomes MAX_PLAYERS=22 later

struct FSimWorldState {
    uint32_t        TickNumber;                   // 4 B
    uint32_t        _pad0;                        // explicit pad before 8-aligned RngState
    uint64_t        RngState;                     // 8 B
    FSimBallState   Ball;                         // 80 B
    FSimPlayerState Players[kSimPlayerCount];     // 128 B
};
static_assert(sizeof(FSimWorldState)  == 224, "FSimWorldState must be 224 bytes");
static_assert(alignof(FSimWorldState) == 8,   "FSimWorldState must be 8-aligned");

}  // namespace edge26
