// Copyright Edge26. All Rights Reserved.
#include "AI/GoalkeeperAI.h"
#include "AI/Roles.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"
#include "Sim/Constants.h"
#include "Math/Atan2.h"
#include "Math/Sqrt.h"

namespace edge26 {

int FindGoalkeeper(const FSimWorldState& s, int teamId) {
    for (int i = 0; i < kSimPlayerCount; ++i) {
        if (s.Players[i].TeamId == (uint8_t)teamId
            && s.Players[i].RoleId == (uint8_t)ERole::GK) return i;
    }
    return -1;
}

static Fixed64 SignForTeam(int teamId) {
    return (teamId == 0) ? Fixed64::FromInt(1) : Fixed64::FromInt(-1);
}

// Returns true if any opponent of `gkTeam` is within `r` cm of the ball.
static bool OpponentNearBall(const FSimWorldState& s, int gkTeam, Fixed64 r) {
    Fixed64 rSq = r * r;
    // Clamp deltas before squaring to prevent overflow (same pattern as other sim code).
    constexpr int64_t kMaxDelta = (int64_t)15000 << 32;  // 150m cap per axis
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId == (uint8_t)gkTeam) continue;
        Fixed64 dx = p.Position.X - s.Ball.Position.X;
        Fixed64 dy = p.Position.Y - s.Ball.Position.Y;
        if (dx.Raw >  kMaxDelta) dx.Raw =  kMaxDelta;
        if (dx.Raw < -kMaxDelta) dx.Raw = -kMaxDelta;
        if (dy.Raw >  kMaxDelta) dy.Raw =  kMaxDelta;
        if (dy.Raw < -kMaxDelta) dy.Raw = -kMaxDelta;
        Fixed64 dSq = dx * dx + dy * dy;
        if (dSq.Raw < rSq.Raw) return true;
    }
    return false;
}

void UpdateGoalkeeperAI(FSimPlayerState& gk, const FSimWorldState& s, int gkIdx)
{
    gk.PendingButtons = 0;
    gk.CurrentIntent = 0;  // EIntent::HoldPosition

    const int    teamId  = gk.TeamId;
    const Fixed64 sign   = SignForTeam(teamId);
    const Fixed64 goalX  = -SimConst::PitchHalfLen * sign;  // team's own goal X
    const Fixed64 stanceX = goalX + sign * SimConst::kGKStanceOffset;

    // Tier 1 — Goal-line stance, leaning toward ball laterally.
    Fixed64 leanY = s.Ball.Position.Y;
    if (leanY.Raw >  SimConst::kGoalHalfWidth.Raw) leanY = SimConst::kGoalHalfWidth;
    if (leanY.Raw < -SimConst::kGoalHalfWidth.Raw) leanY = Fixed64::FromRaw(0) - SimConst::kGoalHalfWidth;
    FixedVec3 target { stanceX, leanY, Fixed64::FromInt(0) };

    // Tier 2 — Sweeper: ball in own box AND no opponent within 3m of ball.
    Fixed64 ballSidedness = (s.Ball.Position.X - goalX) * sign;
    bool ballInBox = (ballSidedness.Raw > 0) && (ballSidedness.Raw < SimConst::kBoxDepth.Raw);
    if (ballInBox && !OpponentNearBall(s, teamId, Fixed64::FromInt(300))) {
        target = s.Ball.Position;
    }

    // Tier 3 — Save target: ball moving toward our goal (X velocity toward goal).
    Fixed64 ballVxToGoal = s.Ball.Velocity.X * (-sign);
    bool incoming = (ballVxToGoal.Raw > Fixed64::FromInt(200).Raw);   // > 2 m/s toward goal
    if (incoming) {
        // Predict ball Y when it reaches goal line.
        Fixed64 dx = goalX - s.Ball.Position.X;
        // dt = dx / vx; clamp to a tick budget so we don't extrapolate forever.
        if (s.Ball.Velocity.X.Raw != 0) {
            // dt in Q32.32: (dx.Raw << 32) / vx.Raw gives Q32.32 time in cm/(cm/s) = seconds.
            Fixed64 dt = Fixed64::FromRaw(((__int128)dx.Raw << 32) / s.Ball.Velocity.X.Raw);  // SIM-LINT-OK: same __int128 pattern as Fixed64::operator/
            // Clamp dt to 1 second (50 ticks) to avoid huge extrapolation.
            const Fixed64 kMaxDt = Fixed64::FromInt(1);
            if (dt.Raw < -kMaxDt.Raw) dt.Raw = -kMaxDt.Raw;
            if (dt.Raw >  kMaxDt.Raw) dt.Raw =  kMaxDt.Raw;
            Fixed64 predY = s.Ball.Position.Y + Fixed64::FromRaw(
                ((__int128)s.Ball.Velocity.Y.Raw * dt.Raw) >> 32);  // SIM-LINT-OK: same __int128 pattern
            if (predY.Raw >  SimConst::kGoalHalfWidth.Raw) predY = SimConst::kGoalHalfWidth;
            if (predY.Raw < -SimConst::kGoalHalfWidth.Raw) predY = Fixed64::FromRaw(0) - SimConst::kGoalHalfWidth;
            target = FixedVec3{ stanceX, predY, Fixed64::FromInt(0) };
        }
    }

    gk.AITargetPosition = target;
    gk.FacingTarget = SimMath::Atan2(
        s.Ball.Position.Y - gk.Position.Y,
        s.Ball.Position.X - gk.Position.X);
    (void)gkIdx;
}

static bool BallMovingTowardGoal(const FSimBallState& b, int gkTeam) {
    // Home GK (team 0) defends -X end: ball moving toward home goal = ball.Velocity.X < 0.
    // Away GK (team 1) defends +X end: ball moving toward away goal = ball.Velocity.X > 0.
    // sign = +1 for team 0, -1 for team 1.
    Fixed64 sign = (gkTeam == 0) ? Fixed64::FromInt(1) : Fixed64::FromInt(-1);
    return (b.Velocity.X * (-sign)).Raw > 0;
}

void MaybeGoalkeeperSave(FSimBallState& b, FSimWorldState& s)
{
    for (int t = 0; t < 2; ++t) {
        int gkIdx = FindGoalkeeper(s, t);
        if (gkIdx < 0) continue;
        const FSimPlayerState& gk = s.Players[gkIdx];

        // Clamp deltas before squaring to prevent overflow for out-of-pitch positions.
        constexpr int64_t kMaxDelta = (int64_t)15000 << 32;  // 150m cap per axis
        Fixed64 dx = b.Position.X - gk.Position.X;
        Fixed64 dy = b.Position.Y - gk.Position.Y;
        if (dx.Raw >  kMaxDelta) dx.Raw =  kMaxDelta;
        if (dx.Raw < -kMaxDelta) dx.Raw = -kMaxDelta;
        if (dy.Raw >  kMaxDelta) dy.Raw =  kMaxDelta;
        if (dy.Raw < -kMaxDelta) dy.Raw = -kMaxDelta;
        Fixed64 dist = SimMath::Sqrt(dx * dx + dy * dy);
        if (dist.Raw > SimConst::kGKReachRadius.Raw) continue;
        if (!BallMovingTowardGoal(b, t)) continue;

        // Save: zero ball velocity, GK gains possession.
        b.Velocity        = FixedVec3::Zero();
        b.AngularVelocity = FixedVec3::Zero();
        b.Position        = gk.Position + FixedVec3{
            Fixed64::FromInt(20), Fixed64::FromInt(0), Fixed64::FromInt(50)
        };
        s.Match.PossessionTeam   = (uint8_t)t;
        s.Match.PossessionPlayer = (uint8_t)gkIdx;
        return;     // first save in linear order wins (deterministic).
    }
}

}  // namespace edge26
