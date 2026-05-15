// Copyright Edge26. All Rights Reserved.
#include "AI/PlayerDecisions.h"
#include "AI/Formations.h"
#include "AI/Intents.h"
#include "AI/Roles.h"
#include "AI/SpatialValueModel.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"
#include "Sim/Constants.h"
#include "Math/Atan2.h"
#include "Math/Sqrt.h"

namespace edge26 {

int SlotIndexForPlayer(int playerIdx) { return playerIdx % 11; }

// ---- Helpers ----------------------------------------------------------------

static Fixed64 SignForTeam(int teamId) {
    return (teamId == 0) ? Fixed64::FromInt(1) : Fixed64::FromInt(-1);
}

// Returns true if cell (its center) is past the opponents' offside line.
static bool CellIsOffsidePub(int cellIdx, int attackingTeam, const FMatchState& m) {
    Fixed64 cellX = CellCenter(cellIdx).X;   // pitch X is up-pitch
    Fixed64 lineY = m.OffsideLineY[1 - attackingTeam];
    return (attackingTeam == 0) ? (cellX.Raw > lineY.Raw) : (cellX.Raw < lineY.Raw);
}

// Iterate cells and find the one with maximum value among those passing `filter`.
template <typename FilterFn>
static int ArgMaxCellFiltered(const FSpatialValueModel& sm, int teamId,
                              ESpatialField field, FilterFn filter)
{
    int bestIdx = 0;
    Fixed32 bestVal = Fixed32::FromRaw(INT32_MIN);
    for (int c = 0; c < kPitchCells; ++c) {
        if (!filter(c)) continue;
        Fixed32 v = sm.Cells[teamId][(int)field][c];
        if (v.Raw > bestVal.Raw) { bestVal = v; bestIdx = c; }
    }
    return bestIdx;
}

// Distance penalty: prefer nearby targets (cheaper to reach in finite stamina).
static Fixed32 DistancePenalty(FixedVec3 target, FixedVec3 from) {
    Fixed64 dx = target.X - from.X;
    Fixed64 dy = target.Y - from.Y;
    // Clamp each axis delta to avoid overflow in squared product.
    // Max pitch dimension is 10500 cm; clamp conservatively to 15000 cm.
    constexpr int64_t kMaxDelta = (int64_t)15000 << 32;  // 15000 in Q32.32
    if (dx.Raw >  kMaxDelta) dx.Raw =  kMaxDelta;
    if (dx.Raw < -kMaxDelta) dx.Raw = -kMaxDelta;
    if (dy.Raw >  kMaxDelta) dy.Raw =  kMaxDelta;
    if (dy.Raw < -kMaxDelta) dy.Raw = -kMaxDelta;
    Fixed64 distSq = dx * dx + dy * dy;
    Fixed64 dist   = SimMath::Sqrt(distSq);
    // Penalty grows linearly to ~Fixed32::One at 30m (3000 cm).
    int64_t penaltyRaw = (dist.Raw * (int64_t)Fixed32::One) / Fixed64::FromInt(3000).Raw;
    if (penaltyRaw > Fixed32::One) penaltyRaw = Fixed32::One;
    return Fixed32::FromRaw((int32_t)penaltyRaw);
}

static Fixed32 BiasFromMentality(int8_t mentality, int8_t sign) {
    // Mentality is -2..+2; sign is +1 (attacking bias) or -1 (defensive bias).
    // Return a multiplier in roughly [0.5..1.5].
    int adjusted = mentality * sign;
    return Fixed32::FromRaw((int32_t)(Fixed32::One + adjusted * (Fixed32::One / 4)));
}

// ---- The main off-ball evaluator -------------------------------------------

static void EvaluateOffBall(FSimPlayerState& p, const FSimWorldState& s,
                            const FRoleWeights& W, int playerIdx,
                            EIntent& bestIntent, FixedVec3& bestTarget, Fixed32& bestScore)
{
    const FTeamPlan& Plan = s.Match.Plans[p.TeamId];
    const bool ownTeamHasBall = (s.Match.PossessionTeam == p.TeamId);

    // 1. HoldPosition — anchor to slot.
    {
        int slot = SlotIndexForPlayer(playerIdx);
        FixedVec3 target = SlotWorldPosition(slot, p.TeamId);
        Fixed32 score = W.HoldPosition * BiasFromMentality(Plan.Mentality, 0);
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::HoldPosition; bestTarget = target; bestScore = score;
        }
    }

    // 2. MakeRunForward — ahead of ball, not offside.
    if (ownTeamHasBall && p.RoleId != (uint8_t)ERole::GK) {
        int cell = ArgMaxCellFiltered(s.Spatial, p.TeamId, ESpatialField::PassReception,
            [&](int c) {
                // Cell must be ahead of ball for the attacking team.
                Fixed64 cellX = CellCenter(c).X;
                Fixed64 signedDelta = (cellX - s.Ball.Position.X) * SignForTeam(p.TeamId);
                if (signedDelta.Raw <= 0) return false;
                // Not past offside line.
                if (CellIsOffsidePub(c, p.TeamId, s.Match)) return false;
                return true;
            });
        FixedVec3 target = CellCenter(cell);
        Fixed32 score = s.Spatial.Cells[p.TeamId][(int)ESpatialField::PassReception][cell]
                      * W.MakeRunForward
                      * BiasFromMentality(Plan.Mentality, (int8_t)+1);
        // Subtract small distance penalty
        Fixed32 penalty = DistancePenalty(target, p.Position);
        if (score.Raw > penalty.Raw) score = Fixed32::FromRaw(score.Raw - penalty.Raw);
        else score = Fixed32::FromRaw(0);
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::MakeRunForward; bestTarget = target; bestScore = score;
        }
    }

    // 3. DropToReceive — only when own team has possession.
    if (ownTeamHasBall && p.RoleId != (uint8_t)ERole::GK) {
        // Pick a high-Space cell behind the ball.
        int cell = ArgMaxCellFiltered(s.Spatial, p.TeamId, ESpatialField::Space,
            [&](int c) {
                Fixed64 cellX = CellCenter(c).X;
                Fixed64 signedDelta = (cellX - s.Ball.Position.X) * SignForTeam(p.TeamId);
                return signedDelta.Raw < 0;   // behind ball
            });
        FixedVec3 target = CellCenter(cell);
        Fixed32 score = s.Spatial.Cells[p.TeamId][(int)ESpatialField::Space][cell] * W.DropToReceive;
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::DropToReceive; bestTarget = target; bestScore = score;
        }
    }

    // 4. ProvideWidth — touchline at ball's pitch-X.
    if (ownTeamHasBall && p.RoleId != (uint8_t)ERole::GK) {
        // Pick sideline (max |Y|) at ball's X.
        Fixed64 sideY = (p.Position.Y.Raw < 0) ? -SimConst::PitchHalfWid : SimConst::PitchHalfWid;
        FixedVec3 target { s.Ball.Position.X,
                           sideY * Fixed64::FromRaw((Fixed64::One * 9) / 10),
                           Fixed64::FromInt(0) };
        Fixed32 score = W.ProvideWidth;  // simple flat score; cell-lookup at sideline omitted
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::ProvideWidth; bestTarget = target; bestScore = score;
        }
    }

    // 5. Press — only when opposing team has the ball AND this player is closest to ball.
    if (!ownTeamHasBall && s.Match.PossessionTeam != 0xFF
        && p.RoleId != (uint8_t)ERole::GK)
    {
        // For v0 (pre Layer-B nomination): each player computes "am I nearest to ball on my team?".
        FixedVec3 ballPos = s.Ball.Position;
        Fixed64 dx0 = p.Position.X - ballPos.X;
        Fixed64 dy0 = p.Position.Y - ballPos.Y;
        // Clamp deltas before squaring to prevent overflow.
        constexpr int64_t kMaxDelta = (int64_t)15000 << 32;
        if (dx0.Raw >  kMaxDelta) dx0.Raw =  kMaxDelta;
        if (dx0.Raw < -kMaxDelta) dx0.Raw = -kMaxDelta;
        if (dy0.Raw >  kMaxDelta) dy0.Raw =  kMaxDelta;
        if (dy0.Raw < -kMaxDelta) dy0.Raw = -kMaxDelta;
        Fixed64 ourDistSq = dx0 * dx0 + dy0 * dy0;
        bool iAmNearest = true;
        for (int i = 0; i < kSimPlayerCount; ++i) {
            if (i == playerIdx) continue;
            const FSimPlayerState& other = s.Players[i];
            if (other.TeamId != p.TeamId) continue;
            Fixed64 odx = other.Position.X - ballPos.X;
            Fixed64 ody = other.Position.Y - ballPos.Y;
            if (odx.Raw >  kMaxDelta) odx.Raw =  kMaxDelta;
            if (odx.Raw < -kMaxDelta) odx.Raw = -kMaxDelta;
            if (ody.Raw >  kMaxDelta) ody.Raw =  kMaxDelta;
            if (ody.Raw < -kMaxDelta) ody.Raw = -kMaxDelta;
            Fixed64 otherDistSq = odx * odx + ody * ody;
            if (otherDistSq.Raw < ourDistSq.Raw) { iAmNearest = false; break; }
        }
        if (iAmNearest) {
            Fixed32 score = W.Press
                * Fixed32::FromRaw(Fixed32::One * (1 + (int)Plan.PressIntensity) / 2);
            if (score.Raw > bestScore.Raw) {
                bestIntent = EIntent::Press; bestTarget = ballPos; bestScore = score;
            }
        }
    }

    // 6. TrackRunner — defending; find nearest opposing forward.
    if (!ownTeamHasBall && s.Match.PossessionTeam != 0xFF) {
        int nearestOpp = -1;
        Fixed64 nearestSq = Fixed64::FromInt(99999999);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const FSimPlayerState& opp = s.Players[i];
            if (opp.TeamId == p.TeamId) continue;
            if (opp.RoleId == (uint8_t)ERole::GK) continue;
            Fixed64 ddx = opp.Position.X - p.Position.X;
            Fixed64 ddy = opp.Position.Y - p.Position.Y;
            // Clamp deltas before squaring.
            constexpr int64_t kMaxDelta = (int64_t)15000 << 32;
            if (ddx.Raw >  kMaxDelta) ddx.Raw =  kMaxDelta;
            if (ddx.Raw < -kMaxDelta) ddx.Raw = -kMaxDelta;
            if (ddy.Raw >  kMaxDelta) ddy.Raw =  kMaxDelta;
            if (ddy.Raw < -kMaxDelta) ddy.Raw = -kMaxDelta;
            Fixed64 dSq = ddx * ddx + ddy * ddy;
            if (dSq.Raw < nearestSq.Raw) { nearestSq = dSq; nearestOpp = i; }
        }
        if (nearestOpp >= 0) {
            Fixed32 score = W.TrackRunner;
            if (score.Raw > bestScore.Raw) {
                bestIntent = EIntent::TrackRunner;
                bestTarget = s.Players[nearestOpp].Position;
                bestScore = score;
            }
        }
    }

    // 7. HoldDefensiveLine — defenders only. Layer B fills LineY (default 0 pre-M5).
    if (p.RoleId == (uint8_t)ERole::CB || p.RoleId == (uint8_t)ERole::FB_L
        || p.RoleId == (uint8_t)ERole::FB_R)
    {
        FixedVec3 target { p.Position.X,
                           s.Match.Units[p.TeamId][0].LineY,   // [0] = Defense unit
                           Fixed64::FromInt(0) };
        Fixed32 score = W.HoldDefensiveLine;
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::HoldDefensiveLine; bestTarget = target; bestScore = score;
        }
    }
}

void UpdatePlayerAI(FSimPlayerState& p, const FSimWorldState& s, int playerIdx) {
    const FRoleWeights& W = kRoleWeightsTable[p.RoleId];

    EIntent   bestIntent = EIntent::HoldPosition;
    FixedVec3 bestTarget = SlotWorldPosition(SlotIndexForPlayer(playerIdx), p.TeamId);
    Fixed32   bestScore  = Fixed32::FromRaw(INT32_MIN);

    EvaluateOffBall(p, s, W, playerIdx, bestIntent, bestTarget, bestScore);
    // On-ball evaluation comes in M4.

    p.CurrentIntent = (uint8_t)bestIntent;
    p.AITargetPosition = bestTarget;
    p.FacingTarget = SimMath::Atan2(
        bestTarget.Y - p.Position.Y,
        bestTarget.X - p.Position.X);
}

}  // namespace edge26
