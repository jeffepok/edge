// Copyright Edge26. All Rights Reserved.
// All sim-tunable values live here as constexpr. NO ini, NO BP, NO runtime.
// Units: cm, cm/s, cm/s², radians, ticks.
#pragma once

#include "Math/Fixed.h"
#include "Math/FixedAngle.h"

namespace edge26 { namespace SimConst {

// --- Tick ---
constexpr int      TicksPerSecond  = 50;
// DT in seconds as Fixed64. 0.02 * 2^32 ≈ 85899345.92 → round to 85899346.
constexpr Fixed64  DT              = Fixed64::FromRaw(85899346);

// --- Pitch (rough; tuned later) ---
constexpr Fixed64  PitchHalfLen    = Fixed64::FromInt(5250);   // 105m
constexpr Fixed64  PitchHalfWid    = Fixed64::FromInt(3400);   // 68m
constexpr Fixed64  GroundZ         = Fixed64::FromInt(0);

// --- Player kinematic ---
constexpr Fixed64  JogSpeed        = Fixed64::FromInt(500);    // cm/s
constexpr Fixed64  SprintSpeed     = Fixed64::FromInt(820);    // cm/s
constexpr Fixed64  Accel           = Fixed64::FromInt(2000);   // cm/s² to approach target velocity
constexpr Fixed64  KickReach       = Fixed64::FromInt(180);    // cm

// --- Ball ---
constexpr Fixed64  Gravity         = Fixed64::FromInt(980);    // cm/s²
constexpr Fixed64  BallRadius      = Fixed64::FromInt(11);     // cm
// Linear drag per tick (raw): 0.005 * 2^32 ≈ 21474836
constexpr Fixed64  LinearDragPerTick = Fixed64::FromRaw(21474836);
// Restitution 0.55: 0.55 * 2^32 ≈ 2362232012
constexpr Fixed64  Restitution     = Fixed64::FromRaw(2362232012ll);
// Ground friction XY 0.85 per bounce: 0.85 * 2^32 ≈ 3650722201
constexpr Fixed64  GroundFrictionXY = Fixed64::FromRaw(3650722201ll);
// Settle threshold: 5 cm/s vertical
constexpr Fixed64  SettleThreshold = Fixed64::FromInt(5);

// --- Kick impulse magnitudes ---
constexpr Fixed64  PassSpeed       = Fixed64::FromInt(1500);
constexpr Fixed64  PassLift        = Fixed64::FromInt(100);
constexpr Fixed64  ShotSpeed       = Fixed64::FromInt(2500);
constexpr Fixed64  ShotLift        = Fixed64::FromInt(250);
constexpr Fixed64  ChipSpeed       = Fixed64::FromInt(1200);
constexpr Fixed64  ChipLift        = Fixed64::FromInt(700);

} }  // namespace edge26::SimConst
