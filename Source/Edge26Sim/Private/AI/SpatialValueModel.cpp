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

}  // namespace edge26
