// Copyright Edge26. All Rights Reserved.
// Fixed-point types for deterministic sim arithmetic.
// Fixed64 = Q32.32 (~10^-10 precision, range ±2.1e9). Use for positions/velocities.
#pragma once

#include <cstdint>

namespace edge26 {

struct Fixed64 {
    int64_t Raw;

    static constexpr int64_t Shift = 32;
    static constexpr int64_t One   = (int64_t)1 << 32;
    static constexpr int64_t Zero  = 0;

    static constexpr Fixed64 FromRaw(int64_t r) { return Fixed64{r}; }
    static constexpr Fixed64 FromInt(int64_t i) { return Fixed64{i << Shift}; }

    constexpr int64_t ToInt() const { return Raw >> Shift; }

    constexpr Fixed64 operator+(Fixed64 o) const { return Fixed64{Raw + o.Raw}; }
    constexpr Fixed64 operator-(Fixed64 o) const { return Fixed64{Raw - o.Raw}; }
    constexpr Fixed64 operator-()           const { return Fixed64{-Raw}; }

    constexpr bool operator==(Fixed64 o) const { return Raw == o.Raw; }
    constexpr bool operator!=(Fixed64 o) const { return Raw != o.Raw; }
    constexpr bool operator<(Fixed64 o)  const { return Raw <  o.Raw; }
    constexpr bool operator<=(Fixed64 o) const { return Raw <= o.Raw; }
    constexpr bool operator>(Fixed64 o)  const { return Raw >  o.Raw; }
    constexpr bool operator>=(Fixed64 o) const { return Raw >= o.Raw; }
};

}  // namespace edge26
