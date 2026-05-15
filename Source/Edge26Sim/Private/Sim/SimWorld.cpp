// Copyright Edge26. All Rights Reserved.
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include <cstring>

namespace edge26 {

SimWorld::SimWorld(uint64_t rngSeed) {
    // Required §4: zero-init the entire state including explicit padding.
    std::memset(&State, 0, sizeof(State));
    State.RngState = (rngSeed != 0) ? rngSeed : 0xDEADBEEFCAFEBABEull;

    // Initialize player ControllerIndex fields. ControllerIndex 0xFF = stationary.
    for (int i = 0; i < kSimPlayerCount; ++i) {
        State.Players[i].ControllerIndex = (uint8_t)i;  // P1 → 0, P2 → 1
    }
}

extern void StepPlayer(FSimPlayerState& p, const FInputFrame& frame);

void SimWorld::Step(const FInputFrame& frame) {
    State.TickNumber = frame.TickNumber;

    // Player updates in ascending ControllerIndex order (deterministic).
    for (int i = 0; i < kSimPlayerCount; ++i) {
        StepPlayer(State.Players[i], frame);
    }
}

}  // namespace edge26
