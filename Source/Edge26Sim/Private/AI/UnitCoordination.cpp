// Copyright Edge26. All Rights Reserved.
#include "AI/UnitCoordination.h"
#include "AI/Roles.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"

namespace edge26 {

void UpdateDefensiveUnit(FUnitState&, FSimWorldState&, int)  {}
void UpdateMidfieldUnit (FUnitState&, FSimWorldState&, int)  {}
void UpdateAttackUnit   (FUnitState&, FSimWorldState&, int)  {}

void UpdateAllUnits(FSimWorldState& s) {
    for (int team = 0; team < 2; ++team) {
        UpdateDefensiveUnit(s.Match.Units[team][0], s, team);
        UpdateMidfieldUnit (s.Match.Units[team][1], s, team);
        UpdateAttackUnit   (s.Match.Units[team][2], s, team);
    }
}

}  // namespace edge26
