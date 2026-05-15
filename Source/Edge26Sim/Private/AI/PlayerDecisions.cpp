// Copyright Edge26. All Rights Reserved.
#include "AI/PlayerDecisions.h"
#include "AI/Formations.h"
#include "AI/Intents.h"
#include "AI/Roles.h"
#include "AI/SpatialValueModel.h"
#include "Sim/WorldState.h"
#include "Math/Atan2.h"

namespace edge26 {

int SlotIndexForPlayer(int playerIdx) { return playerIdx % 11; }

void UpdatePlayerAI(FSimPlayerState& p, const FSimWorldState& s, int playerIdx) {
    // Stub for now — fleshed out in T3.3 onwards.
    // Default: stand at slot, face ball.
    int slotIdx = SlotIndexForPlayer(playerIdx);
    p.AITargetPosition = SlotWorldPosition(slotIdx, p.TeamId);
    p.CurrentIntent = (uint8_t)EIntent::HoldPosition;
    p.FacingTarget = SimMath::Atan2(
        s.Ball.Position.Y - p.Position.Y,
        s.Ball.Position.X - p.Position.X);
}

}  // namespace edge26
