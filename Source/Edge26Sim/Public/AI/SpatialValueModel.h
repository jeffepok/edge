// Copyright Edge26. All Rights Reserved.
// Per-cell scalar value fields over the pitch. 5 fields × 2 team perspectives.
// Read by every AI layer.
#pragma once

#include <cstdint>
#include "Math/Fixed.h"
#include "Math/FixedVec.h"
#include "Sim/Constants.h"

namespace edge26 {

constexpr int kPitchCellsX = 52;
constexpr int kPitchCellsY = 34;
constexpr int kPitchCells  = kPitchCellsX * kPitchCellsY;  // 1768

// Cell-size derived from pitch dimensions. Stays integer-clean.
// PitchHalfLen=5250, PitchHalfWid=3400.
// Cell width = (PitchHalfLen * 2) / kPitchCellsX = 10500/52 ≈ 202 cm
// Cell height = (PitchHalfWid * 2) / kPitchCellsY = 6800/34 = 200 cm exactly
constexpr int64_t kCellSizeX_cm = 10500 / kPitchCellsX;  // ≈ 201
constexpr int64_t kCellSizeY_cm = 6800  / kPitchCellsY;  // = 200

enum class ESpatialField : uint8_t {
    Space         = 0,   // openness; high = far from opponents
    DefCoverage   = 1,   // how well-defended; high = poorly defended (gap for opp)
    LaneOccupancy = 2,   // 0 = blocked, 1 = clear (team-independent; stored at [0])
    PassReception = 3,   // composite — "if a teammate were here, how good a pass target?"
    Threat        = 4,   // xG-like surface — value of being in possession here
    Count         = 5
};

struct FSpatialValueModel {
    // [team][field][cell]. Team is the ATTACKING team's perspective.
    Fixed32 Cells[2][(int)ESpatialField::Count][kPitchCells];
};
static_assert(sizeof(FSpatialValueModel) == 2 * 5 * kPitchCells * 4,
              "FSpatialValueModel size locked to 70720 B");

// World position → cell index. Out-of-bounds clamps to nearest valid cell.
inline int CellIndex(FixedVec3 worldPos) {
    // Map worldX in [-PitchHalfLen, +PitchHalfLen] → [0, kPitchCellsX-1].
    int64_t shiftedX = worldPos.X.ToInt() + SimConst::PitchHalfLen.ToInt();
    int64_t shiftedY = worldPos.Y.ToInt() + SimConst::PitchHalfWid.ToInt();
    int cellX = (int)(shiftedX / kCellSizeX_cm);
    int cellY = (int)(shiftedY / kCellSizeY_cm);
    if (cellX < 0) cellX = 0;
    if (cellX >= kPitchCellsX) cellX = kPitchCellsX - 1;
    if (cellY < 0) cellY = 0;
    if (cellY >= kPitchCellsY) cellY = kPitchCellsY - 1;
    return cellY * kPitchCellsX + cellX;
}

// Cell index → center world position.
inline FixedVec3 CellCenter(int cellIdx) {
    int cellX = cellIdx % kPitchCellsX;
    int cellY = cellIdx / kPitchCellsX;
    int64_t worldX_cm = (int64_t)cellX * kCellSizeX_cm - SimConst::PitchHalfLen.ToInt() + kCellSizeX_cm / 2;
    int64_t worldY_cm = (int64_t)cellY * kCellSizeY_cm - SimConst::PitchHalfWid.ToInt() + kCellSizeY_cm / 2;
    return FixedVec3{
        Fixed64::FromInt(worldX_cm),
        Fixed64::FromInt(worldY_cm),
        Fixed64::FromInt(0)
    };
}

// Update entry point — called once per sim tick from SimWorld::Step.
struct FSimWorldState;  // forward
void UpdateSpatialFields(FSimWorldState& state);
void UpdateSpaceField(FSimWorldState& s, int teamId);
void UpdateDefCoverageField(FSimWorldState& s, int teamId);
void UpdateLaneOccupancyField(FSimWorldState& s);
void UpdateThreatField(FSimWorldState& s, int teamId);

}  // namespace edge26
