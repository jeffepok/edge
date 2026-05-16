// Copyright Edge26. All Rights Reserved.
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include "Sim/WorldState.h"
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

void StepPlayer(FSimPlayerState& p, const FInputFrame& frame, int playerIdx, const FSimWorldState& state) {
    const FMatchState& match = state.Match;
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
            // Fix 1c: epsilon-clamp — skip division when dist is near-zero to prevent
            // huge unit vectors. kMinDistCm = 1 cm (defined in Math/Sqrt.h).
            if (dist.Raw > kMinDistCm.Raw) {
                // Use Fixed64::operator/ for safe normalization (uses 128-bit internally).
                desired = FixedVec3{
                    (toTarget.X / dist) * maxSpeed,
                    (toTarget.Y / dist) * maxSpeed,
                    Fixed64::FromInt(0)
                };
            } else {
                desired = FixedVec3::Zero();
            }
        } else {
            desired = FixedVec3::Zero();
        }

        // Fix 2: Spatial separation — sum inverse-distance repulsion from nearby players.
        // Prevents the exact-overlap state that triggers velocity explosion when 3+
        // players converge on the same ball position (all running Press intent).
        {
            const Fixed64 kSepRadius   = Fixed64::FromInt(80);   // 80 cm ≈ 2 player widths
            const Fixed64 kSepRSq      = kSepRadius * kSepRadius;
            const Fixed64 kSepStrength = SimConst::JogSpeed;     // strong push at zero distance
            const Fixed64 kSatClamp    = Fixed64::FromInt(15000);
            FixedVec3 separation       = FixedVec3::Zero();

            for (int j = 0; j < kSimPlayerCount; ++j) {
                if (j == playerIdx) continue;
                const FSimPlayerState& other = state.Players[j];
                Fixed64 ox = p.Position.X - other.Position.X;
                Fixed64 oy = p.Position.Y - other.Position.Y;
                // Saturation clamp to prevent overflow in the squared product.
                if (ox.Raw >  kSatClamp.Raw) ox = kSatClamp;
                if (ox.Raw < -kSatClamp.Raw) ox.Raw = -kSatClamp.Raw;
                if (oy.Raw >  kSatClamp.Raw) oy = kSatClamp;
                if (oy.Raw < -kSatClamp.Raw) oy.Raw = -kSatClamp.Raw;
                Fixed64 dSq = ox * ox + oy * oy;
                if (dSq.Raw >= kSepRSq.Raw) continue;  // outside separation radius
                Fixed64 d = SimMath::Sqrt(dSq);
                if (d.Raw < kMinDistCm.Raw) continue;  // exact/near overlap — skip (Fix 1 handles it)
                // Push proportional to (1 - d/radius) in the direction away from other.
                // strength = kSepStrength * (kSepRadius - d) / kSepRadius.
                // Use raw int64 for the scale ratio to avoid a Fixed64/Fixed64 chain.
                int64_t scaleRaw = ((kSepRadius.Raw - d.Raw) << 32) / kSepRadius.Raw;  // SIM-LINT-OK: single raw ratio, same pattern as PassSuccessProbability
                Fixed64 strength = Fixed64::FromRaw(scaleRaw) * kSepStrength;
                separation.X = separation.X + (ox / d) * strength;
                separation.Y = separation.Y + (oy / d) * strength;
            }
            // Blend separation into desired velocity.
            desired.X = desired.X + separation.X;
            desired.Y = desired.Y + separation.Y;
        }

        // FacingTarget was already set by Layer C / GK AI; honor it.
    }

    Fixed64 maxStep = SimConst::Accel * SimConst::DT;
    p.Velocity = ApproachVec3(p.Velocity, desired, maxStep);
    p.Position = p.Position + p.Velocity * SimConst::DT;

    // Fix 3: Defensive velocity clamp. Sprint speed is the legitimate maximum;
    // anything >2× SprintSpeed is a math bug — renormalize back to SprintSpeed
    // to prevent any residual divide-by-near-zero error from sending players to
    // infinity.
    {
        const Fixed64 kMaxLegit   = SimConst::SprintSpeed * Fixed64::FromInt(2);
        const Fixed64 kMaxLegitSq = kMaxLegit * kMaxLegit;
        Fixed64 velSq = p.Velocity.X * p.Velocity.X + p.Velocity.Y * p.Velocity.Y;
        if (velSq.Raw > kMaxLegitSq.Raw) {
            Fixed64 velMag = SimMath::Sqrt(velSq);
            if (velMag.Raw > kMinDistCm.Raw) {  // safe divide (always true here, but guard anyway)
                p.Velocity.X = (p.Velocity.X / velMag) * kMaxLegit;
                p.Velocity.Y = (p.Velocity.Y / velMag) * kMaxLegit;
            }
        }
    }

    p.Position.X = Clamp(p.Position.X, -SimConst::PitchHalfLen, SimConst::PitchHalfLen);
    p.Position.Y = Clamp(p.Position.Y, -SimConst::PitchHalfWid, SimConst::PitchHalfWid);
    p.Position.Z = Max(p.Position.Z, SimConst::GroundZ);

    p.Flags = (p.Position.Z.Raw <= SimConst::GroundZ.Raw)
        ? (p.Flags | PlayerFlag::Grounded)
        : (p.Flags & ~PlayerFlag::Grounded);

    p.Heading = p.FacingTarget;
}

}  // namespace edge26
