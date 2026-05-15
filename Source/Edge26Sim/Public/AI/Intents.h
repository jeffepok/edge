// Copyright Edge26. All Rights Reserved.
// All possible decisions a player can make in a tick.
#pragma once

#include <cstdint>

namespace edge26 {

enum class EIntent : uint8_t {
    // Off-ball
    HoldPosition      = 0,
    MakeRunForward    = 1,
    DropToReceive     = 2,
    ProvideWidth      = 3,
    Press             = 4,
    TrackRunner       = 5,
    HoldDefensiveLine = 6,
    // On-ball (filled in M4)
    Pass              = 7,
    Shoot             = 8,
    Dribble           = 9,
    Hold              = 10,
    Clear             = 11,
    Count             = 12
};

}  // namespace edge26
