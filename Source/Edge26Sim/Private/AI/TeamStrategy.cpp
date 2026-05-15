// Copyright Edge26. All Rights Reserved.
#include "AI/TeamStrategy.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"

namespace edge26 {

int32_t MatchSecondsRemaining(const FSimWorldState& s) {
    int32_t elapsed = (int32_t)(s.TickNumber / (uint32_t)kSimTickHz);
    int32_t remaining = kMatchTotalSeconds - elapsed;
    if (remaining < 0) remaining = 0;
    return remaining;
}

void UpdateTeamStrategy(FTeamPlan& Plan, const FSimWorldState&, int) {
    // Stub: defaults will be set in T6.2.
    (void)Plan;
}

void UpdateAllTeamStrategy(FSimWorldState& s) {
    for (int team = 0; team < 2; ++team) {
        UpdateTeamStrategy(s.Match.Plans[team], s, team);
    }
}

}  // namespace edge26
