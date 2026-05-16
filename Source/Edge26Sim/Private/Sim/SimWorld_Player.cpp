// Copyright Edge26. All Rights Reserved.
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include "Sim/MatchState.h"
#include "Math/Atan2.h"
#include "Math/Sqrt.h"

namespace edge26 {

namespace {

Fixed64 ApproachScalar(Fixed64 current, Fixed64 target, Fixed64 maxStep) {
    Fixed64 diff = target - current;
    if (diff.Raw >  maxStep.Raw) return current + maxStep;
    if (diff.Raw < -maxStep.Raw) return current + Fixed64::FromRaw(-maxStep.Raw);
    return target;
}

FixedVec3 ApproachVec3(FixedVec3 current, FixedVec3 target, Fixed64 maxStep) {
    return {
        ApproachScalar(current.X, target.X, maxStep),
        ApproachScalar(current.Y, target.Y, maxStep),
        ApproachScalar(current.Z, target.Z, maxStep),
    };
}

}  // namespace

void StepPlayer(FSimPlayerState& p, const FInputFrame& frame, int playerIdx, const FMatchState& match) {
    const bool isHuman = ((uint8_t)playerIdx == match.HumanControlledIndex);

    Fixed64 maxSpeed = SimConst::JogSpeed;
    FixedVec3 desired;

    if (isHuman) {
        // Human always reads from input slot 0 (single local player in v0).
        int8_t sx = frame.Move[0][0];
        int8_t sy = frame.Move[0][1];
        uint8_t buttons = frame.Buttons[0];

        bool isSprinting = (buttons & InputButton::Sprint) != 0;
        p.Flags = isSprinting ? (p.Flags | PlayerFlag::Sprinting)
                              : (p.Flags & ~PlayerFlag::Sprinting);

        Fixed64 stickX = Fixed64::FromRaw(((int64_t)sx * Fixed64::One) / 127);
        Fixed64 stickY = Fixed64::FromRaw(((int64_t)sy * Fixed64::One) / 127);
        maxSpeed = isSprinting ? SimConst::SprintSpeed : SimConst::JogSpeed;
        desired = FixedVec3{ stickX * maxSpeed, stickY * maxSpeed, Fixed64::FromInt(0) };

        if (sx != 0 || sy != 0) {
            p.FacingTarget = SimMath::Atan2(stickY, stickX);
        }
    } else {
        // AI: move toward AITargetPosition. Layer C wrote it earlier this tick.
        FixedVec3 toTarget = p.AITargetPosition - p.Position;
        // Saturation clamp to prevent Fixed64 overflow when target is far.
        Fixed64 dx = toTarget.X, dy = toTarget.Y;
        const Fixed64 clamp = Fixed64::FromInt(15000);
        if (dx.Raw >  clamp.Raw) dx = clamp;
        if (dx.Raw < -clamp.Raw) dx = Fixed64::FromRaw(-clamp.Raw);
        if (dy.Raw >  clamp.Raw) dy = clamp;
        if (dy.Raw < -clamp.Raw) dy = Fixed64::FromRaw(-clamp.Raw);
        Fixed64 distSq = dx * dx + dy * dy;

        const Fixed64 kArriveSq = Fixed64::FromInt(50 * 50);  // 50 cm arrival radius
        if (distSq.Raw > kArriveSq.Raw) {
            Fixed64 dist = SimMath::Sqrt(distSq);
            // Use Fixed64::operator/ for safe normalization (uses 128-bit internally).
            desired = FixedVec3{
                (toTarget.X / dist) * maxSpeed,
                (toTarget.Y / dist) * maxSpeed,
                Fixed64::FromInt(0)
            };
        } else {
            desired = FixedVec3::Zero();
        }
        // FacingTarget was already set by Layer C / GK AI; honor it.
    }

    Fixed64 maxStep = SimConst::Accel * SimConst::DT;
    p.Velocity = ApproachVec3(p.Velocity, desired, maxStep);
    p.Position = p.Position + p.Velocity * SimConst::DT;

    p.Position.X = Clamp(p.Position.X, -SimConst::PitchHalfLen, SimConst::PitchHalfLen);
    p.Position.Y = Clamp(p.Position.Y, -SimConst::PitchHalfWid, SimConst::PitchHalfWid);
    p.Position.Z = Max(p.Position.Z, SimConst::GroundZ);

    p.Flags = (p.Position.Z.Raw <= SimConst::GroundZ.Raw)
        ? (p.Flags | PlayerFlag::Grounded)
        : (p.Flags & ~PlayerFlag::Grounded);

    p.Heading = p.FacingTarget;
}

}  // namespace edge26
