// Copyright Edge26. All Rights Reserved.
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include "Math/Hash.h"
#include "AI/Formations.h"
#include "AI/Roles.h"
#include "AI/SpatialValueModel.h"
#include <cstring>

namespace edge26 {

SimWorld::SimWorld(uint64_t rngSeed) {
    // Required §4: zero-init the entire state including explicit padding.
    std::memset(&State, 0, sizeof(State));
    State.RngState = (rngSeed != 0) ? rngSeed : 0xDEADBEEFCAFEBABEull;

    // 0xFF = "no possession / no offside call" sentinel (cannot be 0 from memset).
    State.Match.PossessionTeam          = 0xFF;
    State.Match.PossessionPlayer        = 0xFF;
    State.Match.PendingOffsideCallTeam  = 0xFF;

    // Initialize each player's TeamId / RoleId / slot world position based on
    // the 4-3-3 formation. Players 0..10 = home; players 11..21 = away.
    for (int i = 0; i < kSimPlayerCount; ++i) {
        FSimPlayerState& p = State.Players[i];
        int teamId         = (i < 11) ? 0 : 1;
        int slotIndex      = i % 11;
        const FFormationSlot& slot = kFormation_4_3_3[slotIndex];

        p.TeamId           = (uint8_t)teamId;
        p.RoleId           = (uint8_t)slot.Role;
        p.Position         = SlotWorldPosition(slotIndex, teamId);
        p.Velocity         = FixedVec3::Zero();
        p.AITargetPosition = p.Position;
        p.ControllerIndex  = kStationaryController;  // vestigial; will be replaced in M9
        p.IntendedPassTarget = 0xFF;
        p.CurrentIntent    = 0;  // EIntent::HoldPosition (defined in M3)
        p.Flags            = 0;
    }
}

extern void StepPlayer(FSimPlayerState& p, const FInputFrame& frame);
extern void StepBall(FSimBallState& b);
extern void MaybeApplyKick(FSimBallState& b, const FSimPlayerState& p, const FInputFrame& frame);

void SimWorld::Step(const FInputFrame& frame) {
    State.TickNumber = frame.TickNumber;

    // Update all 5 spatial value fields before any player/ball logic (spec §5).
    UpdateSpatialFields(State);

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
