// Copyright Edge26. All Rights Reserved.
#pragma once
#include <cstdint>

namespace edge26 {

struct FSimWorldState;
struct FSimPlayerState;
struct FSimBallState;

// Layer-C-equivalent for the GK. Sets the GK's AITargetPosition + FacingTarget.
// Called from SimWorld::Step's player loop instead of UpdatePlayerAI for
// the two GKs.
void UpdateGoalkeeperAI(FSimPlayerState& gk, const FSimWorldState& s, int gkIdx);

// Runs between MaybeApplyKick and ball physics. If a ball heading toward goal
// is within either GK's reach, intercept it.
void MaybeGoalkeeperSave(FSimBallState& b, FSimWorldState& s);

// Helper used by host (player-switch policy etc.).
int  FindGoalkeeper(const FSimWorldState& s, int teamId);

}  // namespace edge26
