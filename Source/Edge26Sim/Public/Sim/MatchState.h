// Copyright Edge26. All Rights Reserved.
#pragma once

#include <cstdint>
#include "Math/Fixed.h"

namespace edge26 {

struct FTeamPlan {
    int8_t   Mentality;            // -2..+2
    int8_t   LineHeightBias;       // -1..+1
    uint8_t  PressIntensity;       // 0..3
    uint8_t  Tempo;                // 0..3
    uint8_t  BuildupStyle;         // 0..2
    uint8_t  CounterAttackBias;    // 0..3
    uint8_t  _pad0[2];
    Fixed32  PanicBias;
    Fixed32  HoldBias;
    Fixed32  MentalityShootBias;
    Fixed32  _pad1;
};
static_assert(sizeof(FTeamPlan) == 24);

struct FUnitState {
    Fixed64  LineY;
    Fixed32  Compactness;
    uint8_t  PressTrigger;
    uint8_t  PressTargetIdx;
    uint8_t  OverlapTriggerIdx;
    uint8_t  _pad;
};
static_assert(sizeof(FUnitState) == 16);

struct FMatchState {
    uint8_t   HumanControlledIndex;
    uint8_t   PossessionTeam;
    uint8_t   PossessionPlayer;
    uint8_t   KickoffTeam;
    uint8_t   PendingOffsideCallTeam;
    uint8_t   _pad0[3];
    uint32_t  PendingOffsideCallTick;
    uint32_t  _pad1;
    Fixed64   OffsideLineY[2];
    FTeamPlan Plans[2];
    FUnitState Units[2][3];
    uint16_t  Score[2];
    uint8_t   _pad2[4];
};
static_assert(sizeof(FMatchState) == 184);
static_assert(alignof(FMatchState) == 8);

}  // namespace edge26
