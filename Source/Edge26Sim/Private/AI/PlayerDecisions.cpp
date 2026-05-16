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

// Returns a probability in [0..1] (as Fixed32) that a pass from `from` to `to`
// completes successfully. Cheap heuristic: 1 - (n_blockers / 4), clamped to [0,1].
// "Blocker" = opposing player within 1.5m of the straight-line segment.
static Fixed32 PassSuccessProbability(FixedVec3 from, FixedVec3 to,
                                      int passingTeam, const FSimWorldState& s)
{
    FixedVec3 seg = to - from;
    Fixed64   segLenSq = seg.X * seg.X + seg.Y * seg.Y;
    // Fix 1d: epsilon-clamp — skip blocker projection when segment is near-zero
    // (prevents __int128 divide by near-zero in the projLenSq calculation below).
    const Fixed64 kMinSegSq = kMinDistCm * kMinDistCm;
    if (segLenSq.Raw <= kMinSegSq.Raw) return Fixed32::FromRaw(Fixed32::One);   // near-zero length: trivially "succeeds"

    // 1.5m = 150 cm
    const Fixed64 kBlockerRadius = Fixed64::FromInt(150);
    const Fixed64 kBlockerRSq    = kBlockerRadius * kBlockerRadius;

    int blockers = 0;
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& opp = s.Players[i];
        if (opp.TeamId == passingTeam) continue;
        if (opp.RoleId == (uint8_t)ERole::GK) continue;

        // Project opp onto segment.
        FixedVec3 v = opp.Position - from;
        // t = dot(v, seg) / segLen^2, clamped to [0,1]
        Fixed64 dotvs = v.X * seg.X + v.Y * seg.Y;
        if (dotvs.Raw < 0)                continue;        // before `from`
        if (dotvs.Raw > segLenSq.Raw)     continue;        // past `to`
        // Perp distance squared = |v|^2 - (dot)^2 / segLen^2
        Fixed64 vLenSq = v.X * v.X + v.Y * v.Y;
        Fixed64 projLenSq = Fixed64::FromRaw(
            (int64_t)((__int128)dotvs.Raw * dotvs.Raw / segLenSq.Raw));  // SIM-LINT-OK: single __int128 use, same pattern as Mul64.h
        Fixed64 perpSq = Fixed64::FromRaw(vLenSq.Raw - projLenSq.Raw);
        if (perpSq.Raw < kBlockerRSq.Raw) ++blockers;
    }

    // success = max(0, 1 - blockers * 0.25)
    int32_t successRaw = Fixed32::One - blockers * (Fixed32::One / 4);
    if (successRaw < 0) successRaw = 0;
    return Fixed32::FromRaw(successRaw);
}

// Pick the best teammate to pass to (or -1 if no candidates exist).
// Score = PassReception[teammate's cell] * PassSuccessProb * forward bonus.
static int BestPassReceiverIdx(const FSimPlayerState& carrier,
                               const FSimWorldState& s,
                               int passingTeam,
                               Fixed32& outBestScore)
{
    int bestIdx = -1;
    outBestScore = Fixed32::FromRaw(INT32_MIN);
    for (int i = 0; i < kSimPlayerCount; ++i) {
        if (i == s.Match.PossessionPlayer) continue;  // can't pass to self
        const auto& mate = s.Players[i];
        if (mate.TeamId != passingTeam) continue;
        if (mate.RoleId == (uint8_t)ERole::GK) continue;
        if (mate.Position == carrier.Position) continue;

        int cellIdx = CellIndex(mate.Position);
        Fixed32 prVal = s.Spatial.Cells[passingTeam][(int)ESpatialField::PassReception][cellIdx];
        Fixed32 succ  = PassSuccessProbability(carrier.Position, mate.Position, passingTeam, s);

        // Forward bonus: heavily prefer up-pitch teammates (M12 tune — without
        // this, carriers favor "safe" lateral/back passes and the ball never
        // reaches the opp goal area). Bonus = 3 × (forward_delta / PitchHalfLen),
        // so a full down-pitch pass is 4× a sideways pass instead of 2×.
        Fixed64 forwardDelta = (mate.Position.X - carrier.Position.X) * SignForTeam(passingTeam);
        Fixed32 forwardBonus = (forwardDelta.Raw > 0)
            ? Fixed32::FromRaw((int32_t)((forwardDelta.Raw * (int64_t)(Fixed32::One * 3))
                                         / SimConst::PitchHalfLen.Raw))
            : Fixed32::FromRaw(0);
        Fixed32 oneF32 = Fixed32::FromRaw(Fixed32::One);
        Fixed32 score = prVal * succ * (oneF32 + forwardBonus);

        if (score.Raw > outBestScore.Raw) {
            outBestScore = score;
            bestIdx = i;
        }
    }
    return bestIdx;
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
        // Overlap nomination: 2× boost for the nominated FB (Layer B).
        const FUnitState& attackUnit = s.Match.Units[p.TeamId][2];
        if ((p.RoleId == (uint8_t)ERole::FB_L || p.RoleId == (uint8_t)ERole::FB_R)
            && attackUnit.OverlapTriggerIdx == (uint8_t)playerIdx)
        {
            score = score * Fixed32::FromInt(2);
        }
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

    // 5. Press / Recover — fire when opp has the ball OR ball is loose, AND
    // I'm the nominated presser for my unit (Layer B). Loose-ball recovery
    // is critical: without it the game stalls on every miscued pass because
    // no one chases the rolling ball.
    if (!ownTeamHasBall && p.RoleId != (uint8_t)ERole::GK)
    {
        int myUnit = UnitOf((ERole)p.RoleId);
        const FUnitState& unit = s.Match.Units[p.TeamId][myUnit];
        bool iAmNominated = (unit.PressTrigger != 0)
                          && (unit.PressTargetIdx == (uint8_t)playerIdx);

        if (iAmNominated) {
            Fixed32 score = W.Press
                * Fixed32::FromRaw(Fixed32::One * (1 + (int)Plan.PressIntensity) / 2)
                * Fixed32::FromInt(3);                          // 3× boost for nominee
            if (score.Raw > bestScore.Raw) {
                bestIntent = EIntent::Press; bestTarget = s.Ball.Position; bestScore = score;
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
        // Units[].LineY actually stores the up-pitch X coordinate (legacy naming
        // from spec §4); on our X-is-up-pitch convention, that's the X axis.
        FixedVec3 target { s.Match.Units[p.TeamId][0].LineY,
                           p.Position.Y,
                           Fixed64::FromInt(0) };
        Fixed32 score = W.HoldDefensiveLine;
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::HoldDefensiveLine; bestTarget = target; bestScore = score;
        }
    }
}

static void EvaluateOnBall(FSimPlayerState& p, const FSimWorldState& s,
                           const FRoleWeights& W, int /*playerIdx*/,
                           EIntent& bestIntent, FixedVec3& bestTarget, Fixed32& bestScore)
{
    const FTeamPlan& Plan = s.Match.Plans[p.TeamId];
    int carrierCell = CellIndex(p.Position);

    // 1. Pass
    {
        Fixed32 bestPassScore = Fixed32::FromRaw(INT32_MIN);
        int receiverIdx = BestPassReceiverIdx(p, s, p.TeamId, bestPassScore);
        if (receiverIdx >= 0) {
            Fixed32 score = bestPassScore * W.PreferPass;
            if (score.Raw > bestScore.Raw) {
                bestIntent = EIntent::Pass;
                bestTarget = s.Players[receiverIdx].Position;
                bestScore  = score;
                p.IntendedPassTarget = (uint8_t)receiverIdx;
            }
        }
    }

    // 2. Shoot — gated by "in opponent's third".
    {
        Fixed64 thirdLine = SimConst::PitchHalfLen * Fixed64::FromInt(1) / Fixed64::FromInt(3);
        Fixed64 inOppThird = (p.Position.X * SignForTeam(p.TeamId)) - thirdLine;
        if (inOppThird.Raw > 0) {
            Fixed32 threat = s.Spatial.Cells[p.TeamId][(int)ESpatialField::Threat][carrierCell];
            Fixed32 score  = threat * W.PreferShoot * Plan.MentalityShootBias;
            // Target = opponent goal center.
            FixedVec3 goalCenter {
                SimConst::PitchHalfLen * SignForTeam(p.TeamId),
                Fixed64::FromInt(0),
                Fixed64::FromInt(0)
            };
            if (score.Raw > bestScore.Raw) {
                bestIntent = EIntent::Shoot;
                bestTarget = goalCenter;
                bestScore  = score;
            }
        }
    }

    // 3. Dribble — pick the best-Space adjacent cell minus opp proximity penalty.
    {
        // 4-neighborhood in cell space.
        int bestCellIdx = -1;
        Fixed32 bestDribbleScore = Fixed32::FromRaw(INT32_MIN);
        int cx = carrierCell % kPitchCellsX;
        int cy = carrierCell / kPitchCellsX;
        const int dxs[4] = { +1, -1,  0,  0 };
        const int dys[4] = {  0,  0, +1, -1 };
        for (int n = 0; n < 4; ++n) {
            int nx = cx + dxs[n];
            int ny = cy + dys[n];
            if (nx < 0 || nx >= kPitchCellsX || ny < 0 || ny >= kPitchCellsY) continue;
            int neighborCell = ny * kPitchCellsX + nx;
            Fixed32 space = s.Spatial.Cells[p.TeamId][(int)ESpatialField::Space][neighborCell];

            FixedVec3 cellPos = CellCenter(neighborCell);
            // Nearest opponent penalty.
            Fixed64 nearestSq = Fixed64::FromInt(99999999);
            for (int i = 0; i < kSimPlayerCount; ++i) {
                const auto& o = s.Players[i];
                if (o.TeamId == p.TeamId) continue;
                Fixed64 dSq = (o.Position.X - cellPos.X) * (o.Position.X - cellPos.X)
                            + (o.Position.Y - cellPos.Y) * (o.Position.Y - cellPos.Y);
                if (dSq.Raw < nearestSq.Raw) nearestSq = dSq;
            }
            Fixed64 nearestDist = SimMath::Sqrt(nearestSq);
            // Penalty: max(0, 5m - nearestDist) / 5m
            Fixed64 fiveM = Fixed64::FromInt(500);
            int32_t penaltyRaw = 0;
            if (nearestDist.Raw < fiveM.Raw) {
                penaltyRaw = (int32_t)(((fiveM.Raw - nearestDist.Raw) * (int64_t)Fixed32::One)
                                        / fiveM.Raw);
            }
            Fixed32 score = space * W.PreferDribble;
            score = Fixed32::FromRaw(score.Raw - penaltyRaw);
            if (score.Raw > bestDribbleScore.Raw) {
                bestDribbleScore = score;
                bestCellIdx = neighborCell;
            }
        }
        if (bestCellIdx >= 0 && bestDribbleScore.Raw > bestScore.Raw) {
            bestIntent = EIntent::Dribble;
            bestTarget = CellCenter(bestCellIdx);
            bestScore  = bestDribbleScore;
        }
    }

    // 4. Hold — flat fallback weighted by Plan.HoldBias.
    {
        Fixed32 score = Plan.HoldBias;
        // Multiply by small constant (0.5) so Hold is the "do nothing" default.
        score = Fixed32::FromRaw(score.Raw / 2);
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::Hold;
            bestTarget = p.Position;
            bestScore  = score;
        }
    }

    // 5. Clear — defender-only panic ball.
    if (p.RoleId == (uint8_t)ERole::CB || p.RoleId == (uint8_t)ERole::FB_L
        || p.RoleId == (uint8_t)ERole::FB_R)
    {
        // Target: 30m up-pitch, on carrier's lateral side.
        Fixed64 thirtyM = Fixed64::FromInt(3000);
        FixedVec3 target {
            p.Position.X + thirtyM * SignForTeam(p.TeamId),
            p.Position.Y,
            Fixed64::FromInt(0)
        };
        // Estimate self-cell Threat (higher threat near own goal => panic ball more valuable).
        Fixed32 ownThreat = s.Spatial.Cells[1 - p.TeamId][(int)ESpatialField::Threat][carrierCell];
        Fixed32 score = W.PreferLongBall * ownThreat * Plan.PanicBias;
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::Clear;
            bestTarget = target;
            bestScore  = score;
        }
    }
}

void UpdatePlayerAI(FSimPlayerState& p, const FSimWorldState& s, int playerIdx) {
    // Reset per-tick synthetic buttons (carrier may set them below).
    p.PendingButtons = 0;

    const FRoleWeights& W   = kRoleWeightsTable[p.RoleId];
    const bool onBall       = (s.Match.PossessionPlayer == (uint8_t)playerIdx);

    EIntent   bestIntent = EIntent::HoldPosition;
    FixedVec3 bestTarget = SlotWorldPosition(SlotIndexForPlayer(playerIdx), p.TeamId);
    Fixed32   bestScore  = Fixed32::FromRaw(INT32_MIN);

    if (onBall) {
        EvaluateOnBall (p, s, W, playerIdx, bestIntent, bestTarget, bestScore);
    } else {
        EvaluateOffBall(p, s, W, playerIdx, bestIntent, bestTarget, bestScore);
    }

    p.CurrentIntent    = (uint8_t)bestIntent;
    p.AITargetPosition = bestTarget;
    p.FacingTarget     = SimMath::Atan2(
        bestTarget.Y - p.Position.Y,
        bestTarget.X - p.Position.X);

    // On-ball intents that fire a kick raise a synthetic button bit. The bit
    // layout matches FInputFrame.Buttons (Sprint=1, Pass=2, Shoot=4, Chip=8).
    switch (bestIntent) {
        case EIntent::Pass:  p.PendingButtons |= (1 << 1); break;   // Pass
        case EIntent::Shoot: p.PendingButtons |= (1 << 2); break;   // Shoot
        case EIntent::Clear: p.PendingButtons |= (1 << 3); break;   // Chip (reuse chip impulse)
        default: break;
    }
}

}  // namespace edge26
