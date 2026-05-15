// Copyright Edge26. All Rights Reserved.
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include "Math/Atan2.h"

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

void StepPlayer(FSimPlayerState& p, const FInputFrame& frame) {
    if (p.ControllerIndex == kStationaryController) {
        return;
    }
    int8_t sx = frame.Move[p.ControllerIndex][0];
    int8_t sy = frame.Move[p.ControllerIndex][1];
    uint8_t buttons = frame.Buttons[p.ControllerIndex];

    bool isSprinting = (buttons & InputButton::Sprint) != 0;
    p.Flags = isSprinting ? (p.Flags | PlayerFlag::Sprinting)
                          : (p.Flags & ~PlayerFlag::Sprinting);

    // Stick → desired velocity in world space.
    Fixed64 stickX = Fixed64::FromRaw(((int64_t)sx * Fixed64::One) / 127);
    Fixed64 stickY = Fixed64::FromRaw(((int64_t)sy * Fixed64::One) / 127);
    Fixed64 maxSpeed = isSprinting ? SimConst::SprintSpeed : SimConst::JogSpeed;
    FixedVec3 desired{ stickX * maxSpeed, stickY * maxSpeed, Fixed64::FromInt(0) };

    Fixed64 maxStep = SimConst::Accel * SimConst::DT;
    p.Velocity = ApproachVec3(p.Velocity, desired, maxStep);

    p.Position = p.Position + p.Velocity * SimConst::DT;

    p.Position.X = Clamp(p.Position.X, -SimConst::PitchHalfLen, SimConst::PitchHalfLen);
    p.Position.Y = Clamp(p.Position.Y, -SimConst::PitchHalfWid, SimConst::PitchHalfWid);
    p.Position.Z = Max(p.Position.Z, SimConst::GroundZ);

    p.Flags = (p.Position.Z.Raw <= SimConst::GroundZ.Raw)
        ? (p.Flags | PlayerFlag::Grounded)
        : (p.Flags & ~PlayerFlag::Grounded);

    if (sx != 0 || sy != 0) {
        p.FacingTarget = SimMath::Atan2(stickY, stickX);
    }
    p.Heading = p.FacingTarget;
}

}  // namespace edge26
