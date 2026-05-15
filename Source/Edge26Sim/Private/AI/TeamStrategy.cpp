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

// Helper: build a Fixed32 from a numerator/denominator pair without floats.
static constexpr Fixed32 F32FromFraction(int num, int denom) {
    return Fixed32::FromRaw((int32_t)((int64_t)num * Fixed32::One / denom));
}

void UpdateTeamStrategy(FTeamPlan& Plan, const FSimWorldState& s, int teamId) {
    const int    scoreDiff = (int)s.Match.Score[teamId] - (int)s.Match.Score[1 - teamId];
    const int32_t secsLeft = MatchSecondsRemaining(s);

    // Baseline 4-3-3 defaults.
    Plan.Mentality          = 0;
    Plan.LineHeightBias     = 0;
    Plan.PressIntensity     = 2;
    Plan.Tempo              = 2;
    Plan.BuildupStyle       = 1;        // mixed
    Plan.CounterAttackBias  = 1;
    Plan.PanicBias          = F32FromFraction(2, 10);    // 0.2
    Plan.HoldBias           = F32FromFraction(2, 10);    // 0.2
    Plan.MentalityShootBias = F32FromFraction(10, 10);   // 1.0

    // Trailing late: push everyone forward.
    if (scoreDiff < 0 && secsLeft < 30 * 60) {
        Plan.Mentality      = +2;
        Plan.LineHeightBias = +1;
        Plan.PressIntensity = 3;
        Plan.MentalityShootBias = F32FromFraction(15, 10);  // 1.5
    }
    // Leading late: drop deep, game-manage.
    else if (scoreDiff > 0 && secsLeft < 20 * 60) {
        Plan.Mentality      = -1;
        Plan.LineHeightBias = -1;
        Plan.PressIntensity = 1;
        Plan.Tempo          = 1;
        Plan.PanicBias      = F32FromFraction(5, 10);       // 0.5
        Plan.HoldBias       = F32FromFraction(7, 10);       // 0.7
    }
    // Drawn late: cautious push.
    else if (scoreDiff == 0 && secsLeft < 20 * 60) {
        Plan.Mentality = +1;
    }

    // Per-team personality (v0 hardcode; data-driven later).
    if (teamId == 0) {                  // home = possession
        Plan.BuildupStyle = 0;
        Plan.HoldBias     = F32FromFraction(3, 10);         // 0.3
    } else {                            // away = counter
        Plan.BuildupStyle = 2;
        Plan.CounterAttackBias = 3;
    }
}

void UpdateAllTeamStrategy(FSimWorldState& s) {
    for (int team = 0; team < 2; ++team) {
        UpdateTeamStrategy(s.Match.Plans[team], s, team);
    }
}

}  // namespace edge26
