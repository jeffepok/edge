// Copyright Edge26. All Rights Reserved.
#pragma once

namespace edge26 {

struct FSimWorldState;
struct FSimPlayerState;

// Layer C entry point. Called per AI player per tick. Writes p.CurrentIntent,
// p.FacingTarget, p.AITargetPosition. Does NOT move the player — that's
// StepPlayer's job (it reads FacingTarget and integrates velocity toward it).
void UpdatePlayerAI(FSimPlayerState& p, const FSimWorldState& s, int playerIdx);

// Helper: which formation slot does this player occupy? Used to anchor HoldPosition.
int  SlotIndexForPlayer(int playerIdx);    // 0..10 (mod 11)

}  // namespace edge26
