// Copyright Edge26. All Rights Reserved.
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include "Math/Hash.h"
#include <cstring>

namespace edge26 {

SimWorld::SimWorld(uint64_t rngSeed) {
    // Required §4: zero-init the entire state including explicit padding.
    std::memset(&State, 0, sizeof(State));
    State.RngState = (rngSeed != 0) ? rngSeed : 0xDEADBEEFCAFEBABEull;

    // Initialize player ControllerIndex fields.
    // Players 0-1 are human-controlled; 2-21 are stationary until AI is wired (Phase 2).
    constexpr int kHumanPlayers = 2;
    for (int i = 0; i < kHumanPlayers; ++i) {
        State.Players[i].ControllerIndex = (uint8_t)i;            // P1 → 0, P2 → 1
    }
    for (int i = kHumanPlayers; i < kSimPlayerCount; ++i) {
        State.Players[i].ControllerIndex = kStationaryController; // 0xFF = stationary
    }
}

extern void StepPlayer(FSimPlayerState& p, const FInputFrame& frame);
extern void StepBall(FSimBallState& b);
extern void MaybeApplyKick(FSimBallState& b, const FSimPlayerState& p, const FInputFrame& frame);

void SimWorld::Step(const FInputFrame& frame) {
    State.TickNumber = frame.TickNumber;

    // Player updates in ascending ControllerIndex order (deterministic).
    for (int i = 0; i < kSimPlayerCount; ++i) {
        StepPlayer(State.Players[i], frame);
    }
    // Kicks resolved in ascending player index for deterministic order.
    for (int i = 0; i < kSimPlayerCount; ++i) {
        MaybeApplyKick(State.Ball, State.Players[i], frame);
    }
    StepBall(State.Ball);
}

void SimWorld::Snapshot(FSimWorldState& out) const {
    out = State;  // POD copy
}

void SimWorld::Restore(const FSimWorldState& in) {
    State = in;
}

uint64_t SimWorld::HashState() const {
    return Hash::XXH64(&State, sizeof(State), 0);
}

}  // namespace edge26
