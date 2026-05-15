// Copyright Edge26. All Rights Reserved.
#include "AI/GoalkeeperAI.h"
#include "AI/Roles.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"
#include "Sim/Constants.h"
#include "Math/Atan2.h"

namespace edge26 {

int FindGoalkeeper(const FSimWorldState& s, int teamId) {
    for (int i = 0; i < kSimPlayerCount; ++i) {
        if (s.Players[i].TeamId == (uint8_t)teamId
            && s.Players[i].RoleId == (uint8_t)ERole::GK) return i;
    }
    return -1;
}

void UpdateGoalkeeperAI(FSimPlayerState&, const FSimWorldState&, int) {
    // Fleshed out in T8.2.
}

void MaybeGoalkeeperSave(FSimBallState&, FSimWorldState&) {
    // Fleshed out in T8.3.
}

}  // namespace edge26
