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
    if ((uint8_t)playerIdx == m.HumanControlledIndex) return (uint16_t)frame.Buttons[p.ControllerIndex];
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
}

void MaybeApplyKick(FSimBallState& b, FSimPlayerState& p, const FInputFrame& frame,
                    const FSimWorldState& state, int playerIdx)
{
    // Human players must have a bound controller; AI players use PendingButtons.
    // If this is a human player with no controller bound (kStationaryController), skip.
    if ((uint8_t)playerIdx == state.Match.HumanControlledIndex
        && p.ControllerIndex == kStationaryController) return;

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

    FixedVec3 dir;
    // Pass: aim at intended target if set, else fall back to heading.
    if (buttons & InputButton::Pass) {
        if (p.IntendedPassTarget < (uint8_t)kSimPlayerCount) {
            FixedVec3 toMate = state.Players[p.IntendedPassTarget].Position - p.Position;
            Fixed64 d = SimMath::Sqrt(toMate.X * toMate.X + toMate.Y * toMate.Y);
            if (d.Raw > 0) {
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
