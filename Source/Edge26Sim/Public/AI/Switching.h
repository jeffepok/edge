// Copyright Edge26. All Rights Reserved.
#pragma once
#include <cstdint>

namespace edge26 {

struct FSimWorldState;

// Returns the index of the player the human should currently control.
// Policy: if humanTeam has possession, return the carrier. Otherwise return
// the nearest non-GK teammate to the ball. Tie-break: lower index wins.
// Returns -1 only if humanTeam has no outfield players (never in v0).
int ChooseHumanControlled(const FSimWorldState& s, int humanTeam);

// Pick the next-nearest teammate to the ball for manual-switch cycling.
// Skips the currently-controlled player + the GK. Returns -1 if none found.
int NextSwitchTarget(const FSimWorldState& s, int humanTeam, int currentIdx);

}  // namespace edge26
