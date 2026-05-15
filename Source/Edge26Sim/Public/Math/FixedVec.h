// Copyright Edge26. All Rights Reserved.
#pragma once

#include "Math/Fixed.h"

namespace edge26 {

struct FixedVec2 {
    Fixed64 X, Y;

    constexpr FixedVec2 operator+(FixedVec2 o) const { return {X + o.X, Y + o.Y}; }
    constexpr FixedVec2 operator-(FixedVec2 o) const { return {X - o.X, Y - o.Y}; }
    constexpr FixedVec2 operator-()              const { return {-X, -Y}; }
    FixedVec2          operator*(Fixed64 s)      const { return {X * s, Y * s}; }
    constexpr bool      operator==(FixedVec2 o)  const { return X == o.X && Y == o.Y; }
};

struct FixedVec3 {
    Fixed64 X, Y, Z;

    constexpr FixedVec3 operator+(FixedVec3 o) const { return {X + o.X, Y + o.Y, Z + o.Z}; }
    constexpr FixedVec3 operator-(FixedVec3 o) const { return {X - o.X, Y - o.Y, Z - o.Z}; }
    constexpr FixedVec3 operator-()              const { return {-X, -Y, -Z}; }
    FixedVec3          operator*(Fixed64 s)      const { return {X * s, Y * s, Z * s}; }
    constexpr bool      operator==(FixedVec3 o)  const { return X == o.X && Y == o.Y && Z == o.Z; }

    static constexpr FixedVec3 Zero() { return {Fixed64::FromRaw(0), Fixed64::FromRaw(0), Fixed64::FromRaw(0)}; }
};

inline Fixed64 Dot(FixedVec3 a, FixedVec3 b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }
inline Fixed64 Dot(FixedVec2 a, FixedVec2 b) { return a.X * b.X + a.Y * b.Y; }

}  // namespace edge26
