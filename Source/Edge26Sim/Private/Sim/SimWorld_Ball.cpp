// Copyright Edge26. All Rights Reserved.
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include "Sim/WorldState.h"
#include "Math/Trig.h"
#include "Math/Sqrt.h"

namespace edge26 {

// Returns the effective button bitfield for a player in a given tick.
// Human-controlled players use the FInputFrame; AI players use PendingButtons.
static uint16_t ResolveButtonsForPlayer(const FInputFrame& frame,
                                        const FSimPlayerState& p,
                                        const FMatchState& m,
                                        int playerIdx)
{
    // Human input is always in slot 0 (single local player in v0).
    // M12 fix: OR with PendingButtons so the AI fires kicks autonomously
    // for the human's pawn when the user isn't pressing buttons (otherwise
    // the human-controlled carrier just stands there during PIE observation).
    // User input still wins because OR — any bit set in frame.Buttons[0]
    // also fires its kick path.
    if ((uint8_t)playerIdx == m.HumanControlledIndex) {
        return (uint16_t)(frame.Buttons[0] | p.PendingButtons);
    }
    return (uint16_t)p.PendingButtons;
}

void StepBall(FSimBallState& b) {
    bool wasGrounded = (b.Flags & BallFlag::Grounded) != 0;

    // Gravity only when airborne — once grounded, the normal force cancels it.
    if (!wasGrounded) {
        b.Velocity.Z = b.Velocity.Z - SimConst::Gravity * SimConst::DT;
    }

    // Linear drag (multiplicative; treated as (1 - drag) per tick).
    Fixed64 retain = Fixed64::FromRaw(Fixed64::One) - SimConst::LinearDragPerTick;
    b.Velocity = {
        b.Velocity.X * retain,
        b.Velocity.Y * retain,
        b.Velocity.Z * retain,
    };

    // Integrate.
    b.Position = b.Position + b.Velocity * SimConst::DT;

    // Floor collision.
    if (b.Position.Z.Raw <= SimConst::BallRadius.Raw) {
        b.Position.Z = SimConst::BallRadius;
        if (b.Velocity.Z.Raw < 0) {
            Fixed64 absVz{-b.Velocity.Z.Raw};
            Fixed64 postBounceVz = absVz * SimConst::Restitution;
            // Settle when the post-bounce velocity can't lift the ball above the
            // floor for even one tick (i.e., gravity wins immediately). This
            // is the physically-correct criterion; SettleThreshold is a coarse
            // safety net for very-slow direct contacts.
            Fixed64 gravityKick = SimConst::Gravity * SimConst::DT;
            bool settle = (postBounceVz.Raw <= gravityKick.Raw) ||
                          (absVz.Raw < SimConst::SettleThreshold.Raw);
            if (settle) {
                b.Velocity.Z = Fixed64::FromRaw(0);
                b.Velocity.X = b.Velocity.X * SimConst::GroundFrictionXY;
                b.Velocity.Y = b.Velocity.Y * SimConst::GroundFrictionXY;
                b.Flags |= BallFlag::Grounded;
            } else {
                b.Velocity.Z = postBounceVz;
                b.Velocity.X = b.Velocity.X * SimConst::GroundFrictionXY;
                b.Velocity.Y = b.Velocity.Y * SimConst::GroundFrictionXY;
                b.Flags &= ~BallFlag::Grounded;
            }
        } else {
            // Vz >= 0 at/below floor → grounded (sitting still or being lifted).
            b.Flags |= BallFlag::Grounded;
        }
    } else {
        b.Flags &= ~BallFlag::Grounded;
    }

    // Touchline / byline bounds. v0 doesn't model throws or corners — when
    // the ball reaches a touchline, stop it. UpdatePossession sees the ball
    // out-of-bounds and clears possession; a future Set-Pieces milestone
    // will replace this with a proper restart.
    if (b.Position.X.Raw >  SimConst::PitchHalfLen.Raw) {
        b.Position.X = SimConst::PitchHalfLen;
        b.Velocity.X = Fixed64::FromRaw(0);
    } else if (b.Position.X.Raw < -SimConst::PitchHalfLen.Raw) {
        b.Position.X = Fixed64::FromRaw(-SimConst::PitchHalfLen.Raw);
        b.Velocity.X = Fixed64::FromRaw(0);
    }
    if (b.Position.Y.Raw >  SimConst::PitchHalfWid.Raw) {
        b.Position.Y = SimConst::PitchHalfWid;
        b.Velocity.Y = Fixed64::FromRaw(0);
    } else if (b.Position.Y.Raw < -SimConst::PitchHalfWid.Raw) {
        b.Position.Y = Fixed64::FromRaw(-SimConst::PitchHalfWid.Raw);
        b.Velocity.Y = Fixed64::FromRaw(0);
    }
}

void MaybeApplyKick(FSimBallState& b, FSimPlayerState& p, const FInputFrame& frame,
                    FSimWorldState& state, int playerIdx)
{
    // Phase 2: every player (human and AI alike) can fire a kick.
    // Human input slot is always 0; AI players use their PendingButtons cache.
    // ResolveButtonsForPlayer encapsulates the per-player routing.
    uint16_t buttons = ResolveButtonsForPlayer(frame, p, state.Match, playerIdx);

    Fixed64 speed, lift;
    if      (buttons & InputButton::Shoot) { speed = SimConst::ShotSpeed; lift = SimConst::ShotLift; }
    else if (buttons & InputButton::Chip)  { speed = SimConst::ChipSpeed; lift = SimConst::ChipLift; }
    else if (buttons & InputButton::Pass)  { speed = SimConst::PassSpeed; lift = SimConst::PassLift; }
    else                                   { return; }

    FixedVec3 to = b.Position - p.Position;
    Fixed64 distSq = to.X * to.X + to.Y * to.Y + to.Z * to.Z;
    Fixed64 reachSq = SimConst::KickReach * SimConst::KickReach;
    if (distSq.Raw > reachSq.Raw) return;

    // Re-kick lockout: if the ball is already moving fast (just kicked by
    // someone else, or by this carrier on a prior tick), skip. Otherwise
    // multiple players in a clump each "kick" the still-near ball every
    // tick with potentially-different intent directions, causing it to
    // teleport / vibrate. 500 cm/s = 5 m/s — well below a fresh pass
    // (15 m/s) but above the noise of a settling/rolling ball.
    Fixed64 ballSpeedSq = b.Velocity.X * b.Velocity.X + b.Velocity.Y * b.Velocity.Y;
    const Fixed64 kBallStationaryThresholdSq =
        Fixed64::FromInt(500) * Fixed64::FromInt(500);
    if (ballSpeedSq.Raw > kBallStationaryThresholdSq.Raw) return;

    FixedVec3 dir;
    // Pass: aim at intended target if set, else fall back to heading.
    if (buttons & InputButton::Pass) {
        if (p.IntendedPassTarget < (uint8_t)kSimPlayerCount) {
            FixedVec3 toMate = state.Players[p.IntendedPassTarget].Position - p.Position;
            Fixed64 d = SimMath::Sqrt(toMate.X * toMate.X + toMate.Y * toMate.Y);
            if (d.Raw > kMinDistCm.Raw) {
                // Normalize: divide Fixed64 by Fixed64 using operator/ (uses __int128 internally).
                dir.X = toMate.X / d;
                dir.Y = toMate.Y / d;
                dir.Z = Fixed64::FromInt(0);
            } else {
                Fixed32 cosH = SimMath::Cos(p.Heading);
                Fixed32 sinH = SimMath::Sin(p.Heading);
                dir.X = Fixed64::FromRaw((int64_t)cosH.Raw << 16);
                dir.Y = Fixed64::FromRaw((int64_t)sinH.Raw << 16);
                dir.Z = Fixed64::FromInt(0);
            }
        } else {
            Fixed32 cosH = SimMath::Cos(p.Heading);
            Fixed32 sinH = SimMath::Sin(p.Heading);
            dir.X = Fixed64::FromRaw((int64_t)cosH.Raw << 16);
            dir.Y = Fixed64::FromRaw((int64_t)sinH.Raw << 16);
            dir.Z = Fixed64::FromInt(0);
        }
        b.Velocity = { dir.X * speed, dir.Y * speed, lift };

        // Offside flag check: if the intended receiver is currently past the
        // opposing team's offside line, start a 30-tick grace window.
        if (p.IntendedPassTarget < (uint8_t)kSimPlayerCount) {
            const auto& mate = state.Players[p.IntendedPassTarget];
            Fixed64 posX  = mate.Position.X;
            Fixed64 lineX = state.Match.OffsideLineY[1 - p.TeamId];
            bool offside  = (p.TeamId == 0) ? (posX.Raw > lineX.Raw)
                                            : (posX.Raw < lineX.Raw);
            if (offside) {
                state.Match.PendingOffsideCallTeam = (uint8_t)p.TeamId;
                state.Match.PendingOffsideCallTick = state.TickNumber;
            }
        }
    } else {
        Fixed32 cosH = SimMath::Cos(p.Heading);
        Fixed32 sinH = SimMath::Sin(p.Heading);
        Fixed64 cosQ32 = Fixed64::FromRaw((int64_t)cosH.Raw << 16);
        Fixed64 sinQ32 = Fixed64::FromRaw((int64_t)sinH.Raw << 16);
        b.Velocity = { cosQ32 * speed, sinQ32 * speed, lift };
    }
    b.Flags &= ~BallFlag::Grounded;
}

}  // namespace edge26
