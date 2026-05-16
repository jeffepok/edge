// Copyright Edge26. All Rights Reserved.
#pragma once
#include <cstdint>

namespace edge26 {

struct FSimWorldState;
struct FTeamPlan;

// Total match length in seconds (90 minutes for v0). Hardcoded.
constexpr int32_t kMatchTotalSeconds = 90 * 60;

// Tick rate of the sim (per Phase 1 spec). Used to convert TickNumber → seconds.
constexpr int32_t kSimTickHz = 50;

int32_t MatchSecondsRemaining(const FSimWorldState& s);

// Entry point called once per team every 25 ticks (2 Hz).
void UpdateTeamStrategy(FTeamPlan& Plan, const FSimWorldState& s, int teamId);

// Convenience: runs both teams. Called from SimWorld::Step.
void UpdateAllTeamStrategy(FSimWorldState& s);

}  // namespace edge26
