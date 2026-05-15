// Copyright Edge26. All Rights Reserved.
#include "AI/SpatialValueModel.h"
#include "Sim/WorldState.h"
#include "Math/Sqrt.h"

namespace edge26 {

// Map raw distance² (in cm²) → openness scalar in [0, 1].
// 0 distance → 0 openness (right next to opponent).
// 500 cm (5m) → ~1 openness (very open).
// Implementation: sqrt(distSq), then clamp/scale.
static Fixed32 DistanceToOpenness(Fixed64 distSq) {
    Fixed64 dist = SimMath::Sqrt(distSq);
    // Saturate at 500 cm: openness = min(dist, 500) / 500.
    constexpr int64_t kSaturationCm = 500;
    Fixed64 saturated = (dist.Raw >= Fixed64::FromInt(kSaturationCm).Raw)
        ? Fixed64::FromInt(kSaturationCm)
        : dist;
    // Convert Fixed64 → Fixed32 by ratio: openness = saturated.Raw / 500 → [0..One]
    int32_t openness = (int32_t)((saturated.Raw * (int64_t)Fixed32::One)
                              / Fixed64::FromInt(kSaturationCm).Raw);
    return Fixed32::FromRaw(openness);
}

// Saturation threshold: distances beyond 500 cm yield full openness.
// Cap individual axis deltas before squaring to avoid Fixed64 overflow.
// Max safe delta for squaring: sqrt(INT64_MAX / Fixed64::One) / Fixed64::One ≈ 46340 cm.
// We clamp at kSaturationCm (500 cm) since anything beyond yields openness = 1.0.
constexpr int64_t kSaturationCm = 500;
// kSaturationSq in Q32.32: (500 << 32) * (500 << 32) >> 32 = 500*500 << 32
// = 250000 << 32. This fits in int64 (250000 * 2^32 ≈ 1.07e15, well within range).
// kSaturationSq = 500^2 in Q32.32 = (500 * 2^32) * 500 / 2^32 * 2^32 = 500^2 << 32.
// Pre-computed: 250000 << 32 = 1073741824000000.
static constexpr Fixed64 kSaturationSq = Fixed64::FromRaw(
    (int64_t)500 * 500 * ((int64_t)1 << 32));

void UpdateSpaceField(FSimWorldState& s, int teamId) {
    auto& field = s.Spatial.Cells[teamId][(int)ESpatialField::Space];
    for (int c = 0; c < kPitchCells; ++c) {
        FixedVec3 cellPos = CellCenter(c);
        Fixed64 minDistSq = kSaturationSq;  // cap; beyond = fully open
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const FSimPlayerState& opp = s.Players[i];
            if (opp.TeamId == (uint8_t)teamId) continue;       // opponent only
            // Clamp each axis delta to [-kSaturationCm, +kSaturationCm] before
            // squaring to prevent Fixed64 overflow on far-away players.
            int64_t dx_cm = opp.Position.X.ToInt() - cellPos.X.ToInt();
            int64_t dy_cm = opp.Position.Y.ToInt() - cellPos.Y.ToInt();
            if (dx_cm >  kSaturationCm || dx_cm < -kSaturationCm ||
                dy_cm >  kSaturationCm || dy_cm < -kSaturationCm) {
                // Definitely beyond saturation radius — skip, treat as capped.
                continue;
            }
            Fixed64 dX = opp.Position.X - cellPos.X;
            Fixed64 dY = opp.Position.Y - cellPos.Y;
            Fixed64 distSq = dX * dX + dY * dY;
            if (distSq.Raw < minDistSq.Raw) minDistSq = distSq;
        }
        field[c] = DistanceToOpenness(minDistSq);
    }
}

// Block radius for lane occupancy: 80 cm.
// kLaneBlockRadiusSq in Q32.32: (80^2) << 32 = 6400 << 32
static constexpr Fixed64 kLaneBlockRadiusSq = Fixed64::FromRaw((int64_t)6400 * ((int64_t)1 << 32));
// Axis cap for lane block check: any axis delta > 80 cm can't be within 80 cm radius.
constexpr int64_t kLaneBlockCm = 80;

void UpdateDefCoverageField(FSimWorldState& s, int teamId) {
    auto& field = s.Spatial.Cells[teamId][(int)ESpatialField::DefCoverage];
    for (int c = 0; c < kPitchCells; ++c) {
        FixedVec3 cellPos = CellCenter(c);
        Fixed64 minDistSq = kSaturationSq;  // cap; beyond = fully open (poorly covered)
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const FSimPlayerState& mate = s.Players[i];
            if (mate.TeamId != (uint8_t)teamId) continue;     // teammates only
            // Clamp each axis delta before squaring to avoid Fixed64 overflow.
            int64_t dx_cm = mate.Position.X.ToInt() - cellPos.X.ToInt();
            int64_t dy_cm = mate.Position.Y.ToInt() - cellPos.Y.ToInt();
            if (dx_cm >  kSaturationCm || dx_cm < -kSaturationCm ||
                dy_cm >  kSaturationCm || dy_cm < -kSaturationCm) {
                // Beyond saturation radius — treat as max distance (fully uncovered).
                continue;
            }
            Fixed64 dX = mate.Position.X - cellPos.X;
            Fixed64 dY = mate.Position.Y - cellPos.Y;
            Fixed64 distSq = dX * dX + dY * dY;
            if (distSq.Raw < minDistSq.Raw) minDistSq = distSq;
        }
        // High value = poorly covered (far from any teammate).
        field[c] = DistanceToOpenness(minDistSq);
    }
}

void UpdateLaneOccupancyField(FSimWorldState& s) {
    FixedVec3 origin = s.Ball.Position;
    auto& fieldTeam0 = s.Spatial.Cells[0][(int)ESpatialField::LaneOccupancy];
    auto& fieldTeam1 = s.Spatial.Cells[1][(int)ESpatialField::LaneOccupancy];

    // Pre-compute fractions t = sample/6 as Fixed64 to avoid repeated division.
    // t_k = k/6 for k in {1,2,3,4,5}.  In Q32.32: k * (1<<32) / 6.
    static const Fixed64 kSampleT[5] = {
        Fixed64::FromInt(1) / Fixed64::FromInt(6),
        Fixed64::FromInt(2) / Fixed64::FromInt(6),
        Fixed64::FromInt(3) / Fixed64::FromInt(6),
        Fixed64::FromInt(4) / Fixed64::FromInt(6),
        Fixed64::FromInt(5) / Fixed64::FromInt(6),
    };

    for (int c = 0; c < kPitchCells; ++c) {
        FixedVec3 cellPos = CellCenter(c);
        FixedVec3 ray = cellPos - origin;
        bool blocked = false;
        // Sample 5 points at t = 1/6, 2/6, ..., 5/6 along origin→cell.
        for (int sample = 0; sample < 5 && !blocked; ++sample) {
            FixedVec3 p = origin + ray * kSampleT[sample];
            for (int i = 0; i < kSimPlayerCount; ++i) {
                const auto& opp = s.Players[i];
                // Axis-level early-out to prevent overflow before squaring.
                int64_t dx_cm = opp.Position.X.ToInt() - p.X.ToInt();
                int64_t dy_cm = opp.Position.Y.ToInt() - p.Y.ToInt();
                if (dx_cm > kLaneBlockCm || dx_cm < -kLaneBlockCm ||
                    dy_cm > kLaneBlockCm || dy_cm < -kLaneBlockCm) {
                    continue;
                }
                Fixed64 dX = opp.Position.X - p.X;
                Fixed64 dY = opp.Position.Y - p.Y;
                Fixed64 distSq = dX * dX + dY * dY;
                if (distSq.Raw < kLaneBlockRadiusSq.Raw) {
                    blocked = true;
                    break;
                }
            }
        }
        Fixed32 v = blocked ? Fixed32::FromRaw(0) : Fixed32::FromRaw(Fixed32::One);
        fieldTeam0[c] = v;
        fieldTeam1[c] = v;
    }
}

}  // namespace edge26
