// Copyright Edge26. All Rights Reserved.
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include "Sim/InputFrame.h"
#include "Sim/MatchState.h"
#include "Math/Hash.h"
#include "AI/Formations.h"
#include "AI/Roles.h"
#include "AI/SpatialValueModel.h"
#include "AI/PlayerDecisions.h"
#include "AI/UnitCoordination.h"
#include "AI/TeamStrategy.h"
#include "AI/GoalkeeperAI.h"
#include "AI/Switching.h"
#include <cstring>

namespace edge26 {

SimWorld::SimWorld(uint64_t rngSeed) {
    // Required §4: zero-init the entire state including explicit padding.
    std::memset(&State, 0, sizeof(State));
    State.RngState = (rngSeed != 0) ? rngSeed : 0xDEADBEEFCAFEBABEull;

    // 0xFF = "no possession / no offside call" sentinel (cannot be 0 from memset).
    State.Match.PossessionTeam          = 0xFF;
    State.Match.PossessionPlayer        = 0xFF;
    State.Match.PendingOffsideCallTeam  = 0xFF;
    State.Match.HumanControlledIndex    = 0;    // will be reset by host
    State.Match.LastManualSwitchTick    = 0;    // M9: cooldown starts inactive

    // Initialize each player's TeamId / RoleId / slot world position based on
    // the 4-3-3 formation. Players 0..10 = home; players 11..21 = away.
    for (int i = 0; i < kSimPlayerCount; ++i) {
        FSimPlayerState& p = State.Players[i];
        int teamId         = (i < 11) ? 0 : 1;
        int slotIndex      = i % 11;
        const FFormationSlot& slot = kFormation_4_3_3[slotIndex];

        p.TeamId           = (uint8_t)teamId;
        p.RoleId           = (uint8_t)slot.Role;
        p.Position         = SlotWorldPosition(slotIndex, teamId);
        p.Velocity         = FixedVec3::Zero();
        p.AITargetPosition = p.Position;
        p.ControllerIndex  = kStationaryController;  // vestigial; will be replaced in M9
        p.IntendedPassTarget = 0xFF;
        p.CurrentIntent    = 0;  // EIntent::HoldPosition (defined in M3)
        p.Flags            = 0;
    }

    // Kickoff possession assignment. Without an initial carrier, EvaluateOnBall
    // never fires, ball never moves, no possession changes hands — game stalls
    // at kickoff. v0 grants the kickoff team's CDM (slot 4) initial possession
    // and parks the ball at their feet. Match flow takes over from there.
    constexpr int kKickoffCarrier = 4;   // home CDM (player 4) in default 4-3-3
    State.Match.PossessionTeam   = 0;
    State.Match.PossessionPlayer = (uint8_t)kKickoffCarrier;
    State.Ball.Position          = State.Players[kKickoffCarrier].Position;
}

extern void StepPlayer(FSimPlayerState& p, const FInputFrame& frame, int playerIdx, const FMatchState& match);
extern void StepBall(FSimBallState& b);
extern void MaybeApplyKick(FSimBallState& b, FSimPlayerState& p, const FInputFrame& frame,
                           FSimWorldState& state, int playerIdx);

static void ResolveOffsideCall(FSimWorldState& s)
{
    if (s.Match.PendingOffsideCallTeam == 0xFF) return;

    const uint8_t attackingTeam = s.Match.PendingOffsideCallTeam;
    const uint32_t startedTick  = s.Match.PendingOffsideCallTick;
    const uint32_t graceTicks   = 30;   // 0.6 s

    // Resolution trigger 1: grace expired.
    bool expired  = (s.TickNumber >= startedTick + graceTicks);
    // Resolution trigger 2: the attacking team controls the ball (receiver picked it up).
    // Guard: don't trigger on the same tick the flag was set (possession hasn't updated yet).
    bool received = (s.Match.PossessionTeam == attackingTeam) &&
                    (s.TickNumber > startedTick);

    if (expired || received) {
        // Award possession to the defending team.
        s.Match.PossessionTeam = (uint8_t)(1 - attackingTeam);
        // Find nearest defending-team outfielder to the ball.
        int     bestIdx = 0xFF;
        // Clamp deltas before squaring to prevent overflow (15000 cm per axis cap).
        constexpr int64_t kMaxDelta = (int64_t)15000 << 32;
        Fixed64 bestSq = Fixed64::FromInt(99999999);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const auto& p = s.Players[i];
            if (p.TeamId != (uint8_t)(1 - attackingTeam)) continue;
            if (p.RoleId == (uint8_t)ERole::GK) continue;
            Fixed64 dx = p.Position.X - s.Ball.Position.X;
            Fixed64 dy = p.Position.Y - s.Ball.Position.Y;
            if (dx.Raw >  kMaxDelta) dx.Raw =  kMaxDelta;
            if (dx.Raw < -kMaxDelta) dx.Raw = -kMaxDelta;
            if (dy.Raw >  kMaxDelta) dy.Raw =  kMaxDelta;
            if (dy.Raw < -kMaxDelta) dy.Raw = -kMaxDelta;
            Fixed64 dSq = dx * dx + dy * dy;
            if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
        }
        s.Match.PossessionPlayer = (uint8_t)bestIdx;
        // Stop the ball (no set-piece restart in v0; just give it to defender).
        s.Ball.Velocity        = FixedVec3::Zero();
        s.Ball.AngularVelocity = FixedVec3::Zero();
        if (bestIdx != 0xFF) {
            s.Ball.Position = s.Players[bestIdx].Position;
        }
        // Clear the flag.
        s.Match.PendingOffsideCallTeam = 0xFF;
        s.Match.PendingOffsideCallTick = 0;
    }
}

static void UpdatePossession(FSimWorldState& s)
{
    // Ball out of pitch → clear possession (no restart in v0).
    if (Abs(s.Ball.Position.X).Raw > SimConst::PitchHalfLen.Raw ||
        Abs(s.Ball.Position.Y).Raw > SimConst::PitchHalfWid.Raw)
    {
        s.Match.PossessionTeam   = 0xFF;
        s.Match.PossessionPlayer = 0xFF;
        return;
    }

    // Nearest outfield player within pickup radius gains possession.
    const Fixed64 kPickupRadius = Fixed64::FromInt(80);   // 80 cm
    const Fixed64 kPickupRSq    = kPickupRadius * kPickupRadius;
    int   bestIdx = -1;
    Fixed64 bestSq = kPickupRSq;
    // Clamp deltas before squaring to prevent overflow (same pattern as EvaluateOffBall).
    constexpr int64_t kMaxDelta = (int64_t)15000 << 32;  // 15000 cm in Q32.32
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        Fixed64 dx = p.Position.X - s.Ball.Position.X;
        Fixed64 dy = p.Position.Y - s.Ball.Position.Y;
        if (dx.Raw >  kMaxDelta) dx.Raw =  kMaxDelta;
        if (dx.Raw < -kMaxDelta) dx.Raw = -kMaxDelta;
        if (dy.Raw >  kMaxDelta) dy.Raw =  kMaxDelta;
        if (dy.Raw < -kMaxDelta) dy.Raw = -kMaxDelta;
        Fixed64 dSq = dx * dx + dy * dy;
        if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
    }
    if (bestIdx >= 0) {
        s.Match.PossessionPlayer = (uint8_t)bestIdx;
        s.Match.PossessionTeam   = s.Players[bestIdx].TeamId;
    } else {
        // Ball is loose mid-flight — no one within pickup radius. Clear stale
        // possession so AI doesn't keep evaluating on-ball for the prior carrier.
        s.Match.PossessionTeam   = 0xFF;
        s.Match.PossessionPlayer = 0xFF;
    }
}

static void ApplySwitching(FSimWorldState& s, const FInputFrame& frame, int humanTeam)
{
    // Manual switch — button edge (Buttons[0] bit 4): advance to next-nearest, set cooldown.
    if (frame.Buttons[0] & InputButton::Switch) {
        int nxt = NextSwitchTarget(s, humanTeam, s.Match.HumanControlledIndex);
        if (nxt >= 0) {
            s.Match.HumanControlledIndex = (uint8_t)nxt;
            s.Match.LastManualSwitchTick = s.TickNumber;
        }
        return;     // skip auto-switch this tick
    }
    // Auto-switch suppressed during 25-tick cooldown after a manual switch.
    const uint32_t kCooldownTicks = 25;
    if (s.TickNumber < s.Match.LastManualSwitchTick + kCooldownTicks) return;

    int target = ChooseHumanControlled(s, humanTeam);
    if (target >= 0) s.Match.HumanControlledIndex = (uint8_t)target;
}

void SimWorld::Step(const FInputFrame& frame) {
    State.TickNumber = frame.TickNumber;

    // Update all 5 spatial value fields before any player/ball logic (spec §5).
    UpdateSpatialFields(State);

    // Layer A: team strategy at 2 Hz (every 25 ticks). Must run BEFORE
    // Layer B so unit coordination can read updated plans this same tick.
    if (State.TickNumber % 25 == 0) {
        UpdateAllTeamStrategy(State);
    }

    // Layer B: unit coordination at 10 Hz (every 5 ticks). Must run BEFORE
    // Layer C so players can read updated nominations this same tick.
    if (State.TickNumber % 5 == 0) {
        UpdateAllUnits(State);
    }

    // Layer C: per-player intent evaluation. Runs for EVERYONE including the
    // human (M12 fix: was skipping the human, which meant when ChooseHumanControlled
    // auto-switched to the carrier, Layer C never wrote on-ball intent +
    // PendingButtons for them — the carrier sat there with stale Press intent
    // and never fired a kick). The human's actual movement still comes from
    // frame.Move[0] in StepPlayer's human path; Layer C's AITargetPosition is
    // a "suggestion" that gets ignored for the human pawn. PendingButtons IS
    // OR'd with frame.Buttons[0] for humans (see ResolveButtonsForPlayer) so
    // the AI fires kicks autonomously when the user isn't pressing a button.
    // GKs get a dedicated AI path (UpdateGoalkeeperAI) instead of the generic
    // Layer C intent loop (UpdatePlayerAI).
    for (int i = 0; i < kSimPlayerCount; ++i) {
        FSimPlayerState& p = State.Players[i];
        if (p.RoleId == (uint8_t)ERole::GK) {
            UpdateGoalkeeperAI(p, State, i);
        } else {
            UpdatePlayerAI(p, State, i);
        }
    }

    // Player updates in ascending index order (deterministic).
    for (int i = 0; i < kSimPlayerCount; ++i) {
        StepPlayer(State.Players[i], frame, i, State.Match);
    }
    // Kicks resolved in ascending player index for deterministic order.
    for (int i = 0; i < kSimPlayerCount; ++i) {
        MaybeApplyKick(State.Ball, State.Players[i], frame, State, i);
    }
    MaybeGoalkeeperSave(State.Ball, State);  // M8 T8.4 — between kicks and offside resolve
    ResolveOffsideCall(State);               // M7 T7.2 — before possession update
    UpdatePossession(State);                 // M4 T4.4
    StepBall(State.Ball);

    // Auto/manual switching for the human team.
    // HumanTeam in v0 is always 0 (home). Future: read from match config.
    ApplySwitching(State, frame, /*humanTeam*/ 0);
}

void SimWorld::Snapshot(FSimWorldState& out) const {
    out = State;  // POD copy
}

void SimWorld::Restore(const FSimWorldState& in) {
    State = in;
}

uint64_t SimWorld::HashState() const {
    return Hash::XXH64(&State, sizeof(State), 0);
}

}  // namespace edge26
