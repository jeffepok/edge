// Copyright Edge26. All Rights Reserved.
#include "AI/UnitCoordination.h"
#include "AI/Roles.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"
#include "Sim/Constants.h"
#include "Math/Sqrt.h"

namespace edge26 {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static Fixed64 SignForTeam(int teamId) {
    return (teamId == 0) ? Fixed64::FromInt(1) : Fixed64::FromInt(-1);
}

// ---------------------------------------------------------------------------
// Task T5.2 — UpdateDefensiveUnit
// ---------------------------------------------------------------------------

void UpdateDefensiveUnit(FUnitState& u, FSimWorldState& s, int teamId) {
    // Average X of CBs + FBs (X is up-pitch in our frame).
    Fixed64 sumX        = Fixed64::FromInt(0);
    int     countX      = 0;
    // Start at +99999999 in signed-forward space so any real defender is closer to own goal.
    Fixed64 minXForward = Fixed64::FromInt(99999999);
    int     rearDefIdx  = -1;

    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId != teamId) continue;
        if (UnitOf((ERole)p.RoleId) != 0) continue;
        if (p.RoleId == (uint8_t)ERole::GK) continue;
        sumX = sumX + p.Position.X;
        ++countX;
        // Find rearmost outfield defender (closest to own goal = lowest signed forward).
        Fixed64 forward = p.Position.X * SignForTeam(teamId);
        if (forward.Raw < minXForward.Raw) {
            minXForward = forward;
            rearDefIdx  = i;
        }
    }
    if (countX > 0) {
        u.LineY = Fixed64::FromRaw(sumX.Raw / countX);
        // LineHeightBias in [-1..+1] steps of 1, scaled to ±5m (500 cm) per step.
        // Use Fixed64::operator* for safe 128-bit intermediate product.
        const FTeamPlan& Plan = s.Match.Plans[teamId];
        Fixed64 bias = Fixed64::FromInt((int)Plan.LineHeightBias * 500);
        u.LineY = u.LineY + bias * SignForTeam(teamId);
    }

    // OffsideLineY = rearmost outfield defender's X OR ball X, whichever is closer to
    // their own goal (standard offside rule: second-last opponent pins the line).
    Fixed64 ballX = s.Ball.Position.X;
    if (rearDefIdx >= 0) {
        Fixed64 a = minXForward;                         // signed forward coord of rearmost def
        Fixed64 b = ballX * SignForTeam(teamId);         // signed forward coord of ball
        // Choose the one closer to own goal (lower signed-forward value).
        Fixed64 lineForward = (a.Raw < b.Raw) ? a : b;
        // Convert back to absolute X: multiply signed-forward by sign.
        // Use Fixed64::operator* to avoid raw overflow.
        s.Match.OffsideLineY[teamId] = lineForward * SignForTeam(teamId);
    } else {
        s.Match.OffsideLineY[teamId] = Fixed64::FromInt(0);
    }

    // Compactness = stddev of X positions (cheap proxy for defensive line spread).
    if (countX > 1) {
        Fixed64 meanX = u.LineY;
        Fixed64 sumSq = Fixed64::FromInt(0);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const auto& p = s.Players[i];
            if (p.TeamId != teamId) continue;
            if (UnitOf((ERole)p.RoleId) != 0) continue;
            if (p.RoleId == (uint8_t)ERole::GK) continue;
            Fixed64 d = p.Position.X - meanX;
            sumSq = sumSq + d * d;
        }
        Fixed64 var = Fixed64::FromRaw(sumSq.Raw / countX);
        Fixed64 std = SimMath::Sqrt(var);
        // Project to Fixed32 (compactness grows with stddev in cm / 1000).
        u.Compactness = Fixed32::FromRaw((int32_t)(std.Raw * Fixed32::One / Fixed64::FromInt(1000).Raw));
    }

    // Press nomination: only when opponent has the ball.
    if (s.Match.PossessionTeam != (uint8_t)teamId && s.Match.PossessionTeam != 0xFF) {
        u.PressTrigger = 1;
        // Pick nearest unit-member to ball.
        int     bestIdx = 0xFF;
        Fixed64 bestSq  = Fixed64::FromInt(99999999);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const auto& p = s.Players[i];
            if (p.TeamId != teamId) continue;
            if (UnitOf((ERole)p.RoleId) != 0) continue;
            if (p.RoleId == (uint8_t)ERole::GK) continue;
            Fixed64 dx = p.Position.X - s.Ball.Position.X;
            Fixed64 dy = p.Position.Y - s.Ball.Position.Y;
            // Clamp deltas to prevent overflow in the squared product.
            constexpr int64_t kMaxDelta = (int64_t)15000 << 32;
            if (dx.Raw >  kMaxDelta) dx.Raw =  kMaxDelta;
            if (dx.Raw < -kMaxDelta) dx.Raw = -kMaxDelta;
            if (dy.Raw >  kMaxDelta) dy.Raw =  kMaxDelta;
            if (dy.Raw < -kMaxDelta) dy.Raw = -kMaxDelta;
            Fixed64 dSq = dx * dx + dy * dy;
            if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
        }
        u.PressTargetIdx = (uint8_t)bestIdx;
    } else {
        u.PressTrigger   = 0;
        u.PressTargetIdx = 0xFF;
    }
    u.OverlapTriggerIdx = 0xFF;  // defense unit doesn't overlap
}

// ---------------------------------------------------------------------------
// Task T5.3 — UpdateMidfieldUnit
// ---------------------------------------------------------------------------

void UpdateMidfieldUnit(FUnitState& u, FSimWorldState& s, int teamId) {
    Fixed64 sumX  = Fixed64::FromInt(0);
    int     count = 0;
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId != teamId) continue;
        if (UnitOf((ERole)p.RoleId) != 1) continue;
        sumX = sumX + p.Position.X;
        ++count;
    }
    if (count > 0) u.LineY = Fixed64::FromRaw(sumX.Raw / count);

    // Press: only when opp has the ball AND it's in the central channel
    // (|ball.Y| < 1/3 pitch width).
    const Fixed64 centralChannel = SimConst::PitchHalfWid / Fixed64::FromInt(3);
    bool oppHasBall = (s.Match.PossessionTeam != (uint8_t)teamId
                        && s.Match.PossessionTeam != 0xFF);
    if (oppHasBall && Abs(s.Ball.Position.Y).Raw < centralChannel.Raw) {
        u.PressTrigger = 1;
        int     bestIdx = 0xFF;
        Fixed64 bestSq  = Fixed64::FromInt(99999999);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const auto& p = s.Players[i];
            if (p.TeamId != teamId) continue;
            if (UnitOf((ERole)p.RoleId) != 1) continue;
            Fixed64 dx = p.Position.X - s.Ball.Position.X;
            Fixed64 dy = p.Position.Y - s.Ball.Position.Y;
            constexpr int64_t kMaxDelta = (int64_t)15000 << 32;
            if (dx.Raw >  kMaxDelta) dx.Raw =  kMaxDelta;
            if (dx.Raw < -kMaxDelta) dx.Raw = -kMaxDelta;
            if (dy.Raw >  kMaxDelta) dy.Raw =  kMaxDelta;
            if (dy.Raw < -kMaxDelta) dy.Raw = -kMaxDelta;
            Fixed64 dSq = dx * dx + dy * dy;
            if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
        }
        u.PressTargetIdx = (uint8_t)bestIdx;
    } else {
        u.PressTrigger   = 0;
        u.PressTargetIdx = 0xFF;
    }

    // Compactness — same proxy as defense.
    if (count > 1) {
        Fixed64 meanX = u.LineY;
        Fixed64 sumSq = Fixed64::FromInt(0);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const auto& p = s.Players[i];
            if (p.TeamId != teamId) continue;
            if (UnitOf((ERole)p.RoleId) != 1) continue;
            Fixed64 d = p.Position.X - meanX;
            sumSq = sumSq + d * d;
        }
        Fixed64 var = Fixed64::FromRaw(sumSq.Raw / count);
        Fixed64 std = SimMath::Sqrt(var);
        u.Compactness = Fixed32::FromRaw((int32_t)(std.Raw * Fixed32::One / Fixed64::FromInt(1000).Raw));
    }
    u.OverlapTriggerIdx = 0xFF;
}

// ---------------------------------------------------------------------------
// Task T5.4 — UpdateAttackUnit
// ---------------------------------------------------------------------------

void UpdateAttackUnit(FUnitState& u, FSimWorldState& s, int teamId) {
    // LineY = highest player X (further forward = larger signed-forward coordinate).
    Fixed64 bestForward = Fixed64::FromInt(-99999999);
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId != teamId) continue;
        if (UnitOf((ERole)p.RoleId) != 2) continue;
        Fixed64 forward = p.Position.X * SignForTeam(teamId);
        if (forward.Raw > bestForward.Raw) bestForward = forward;
    }
    // Convert signed-forward back to absolute X via safe operator*.
    u.LineY = bestForward * SignForTeam(teamId);

    // Overlap nomination: if carrier is on a flank AND a same-side FB exists,
    // nominate that FB for the overlap.
    u.OverlapTriggerIdx = 0xFF;
    if (s.Match.PossessionTeam == (uint8_t)teamId
        && s.Match.PossessionPlayer != 0xFF)
    {
        const auto& carrier  = s.Players[s.Match.PossessionPlayer];
        // "Flank" = |carrier.Y| > 2/3 pitch half-width.
        Fixed64 flankCut = SimConst::PitchHalfWid * Fixed64::FromInt(2) / Fixed64::FromInt(3);
        if (Abs(carrier.Position.Y).Raw > flankCut.Raw) {
            uint8_t wantedFBRole = (carrier.Position.Y.Raw > 0)
                ? (uint8_t)ERole::FB_R
                : (uint8_t)ERole::FB_L;
            for (int i = 0; i < kSimPlayerCount; ++i) {
                const auto& p = s.Players[i];
                if (p.TeamId != teamId) continue;
                if (p.RoleId != wantedFBRole) continue;
                u.OverlapTriggerIdx = (uint8_t)i;
                break;
            }
        }
    }

    // Press nomination: attack unit only presses when opp has ball in opp's
    // own third (high press). For v0 keep this off — Defense + Midfield handle it.
    u.PressTrigger   = 0;
    u.PressTargetIdx = 0xFF;
    u.Compactness    = Fixed32::FromRaw(0);
}

// ---------------------------------------------------------------------------
// UpdateAllUnits
// ---------------------------------------------------------------------------

void UpdateAllUnits(FSimWorldState& s) {
    for (int team = 0; team < 2; ++team) {
        UpdateDefensiveUnit(s.Match.Units[team][0], s, team);
        UpdateMidfieldUnit (s.Match.Units[team][1], s, team);
        UpdateAttackUnit   (s.Match.Units[team][2], s, team);
    }
}

}  // namespace edge26
