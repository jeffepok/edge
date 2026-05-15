// Copyright Edge26. All Rights Reserved.
#pragma once

namespace edge26 {

struct FSimWorldState;
struct FUnitState;

// Entry points called once per (team, unit) every 5 ticks.
void UpdateDefensiveUnit(FUnitState& u, FSimWorldState& s, int teamId);
void UpdateMidfieldUnit (FUnitState& u, FSimWorldState& s, int teamId);
void UpdateAttackUnit   (FUnitState& u, FSimWorldState& s, int teamId);

// Convenience: runs all three for both teams. Cheap; called from SimWorld::Step.
void UpdateAllUnits(FSimWorldState& s);

}  // namespace edge26
