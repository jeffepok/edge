// Copyright Edge26. All Rights Reserved.
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include "Math/Trig.h"

namespace edge26 {

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
            if (absVz.Raw < SimConst::SettleThreshold.Raw) {
                // Settle.
                b.Velocity.Z = Fixed64::FromRaw(0);
                b.Velocity.X = b.Velocity.X * SimConst::GroundFrictionXY;
                b.Velocity.Y = b.Velocity.Y * SimConst::GroundFrictionXY;
                b.Flags |= BallFlag::Grounded;
            } else {
                // Bounce.
                b.Velocity.Z = absVz * SimConst::Restitution;
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

}  // namespace edge26
