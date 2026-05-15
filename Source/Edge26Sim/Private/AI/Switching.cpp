// Copyright Edge26. All Rights Reserved.
#include "AI/Switching.h"
#include "AI/Roles.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"
#include "Math/FixedVec.h"

namespace edge26 {

// Clamp deltas before squaring to prevent int64 overflow (same pattern as
// ResolveOffsideCall). 15000 cm per axis is the saturation cap.
static constexpr int64_t kMaxDelta = (int64_t)15000 << 32;

static Fixed64 ClampedDistSq(FixedVec3 a, FixedVec3 b)
{
    Fixed64 dx = a.X - b.X;
    Fixed64 dy = a.Y - b.Y;
    if (dx.Raw >  kMaxDelta) dx.Raw =  kMaxDelta;
    if (dx.Raw < -kMaxDelta) dx.Raw = -kMaxDelta;
    if (dy.Raw >  kMaxDelta) dy.Raw =  kMaxDelta;
    if (dy.Raw < -kMaxDelta) dy.Raw = -kMaxDelta;
    return dx * dx + dy * dy;
}

int ChooseHumanControlled(const FSimWorldState& s, int humanTeam)
{
    // 1. Possession on our team → carrier.
    if (s.Match.PossessionTeam == (uint8_t)humanTeam
        && s.Match.PossessionPlayer != 0xFF)
    {
        return (int)s.Match.PossessionPlayer;
    }

    // 2. Nearest non-GK teammate to ball.
    int   bestIdx = -1;
    Fixed64 bestSq = Fixed64::FromInt(99999999);
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId != (uint8_t)humanTeam) continue;
        if (p.RoleId == (uint8_t)ERole::GK) continue;
        Fixed64 dSq = ClampedDistSq(p.Position, s.Ball.Position);
        if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
    }
    return bestIdx;
}

int NextSwitchTarget(const FSimWorldState& s, int humanTeam, int currentIdx)
{
    int   bestIdx = -1;
    Fixed64 bestSq = Fixed64::FromInt(99999999);
    for (int i = 0; i < kSimPlayerCount; ++i) {
        if (i == currentIdx) continue;
        const auto& p = s.Players[i];
        if (p.TeamId != (uint8_t)humanTeam) continue;
        if (p.RoleId == (uint8_t)ERole::GK) continue;
        Fixed64 dSq = ClampedDistSq(p.Position, s.Ball.Position);
        if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
    }
    return bestIdx;
}

}  // namespace edge26
