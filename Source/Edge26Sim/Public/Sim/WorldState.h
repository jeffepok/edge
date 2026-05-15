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
    FSimPlayerState Players[kSimPlayerCount];     // 176 B (2 * 88 B after T1.2 extension)
};
// FSimWorldState grows to 272 B after FSimPlayerState extended to 88 B in T1.2.
// Final world-state static_assert is deferred — size grows substantially in M2.
static_assert(sizeof(FSimWorldState)  == 272, "FSimWorldState must be 272 bytes");
static_assert(alignof(FSimWorldState) == 8,   "FSimWorldState must be 8-aligned");

}  // namespace edge26
