// Copyright Edge26. All Rights Reserved.
// Fixed-point types for deterministic sim arithmetic.
// Fixed64 = Q32.32 (~10^-10 precision, range ±2.1e9). Use for positions/velocities.
#pragma once

#include <cstdint>
#include "Math/Mul64.h"

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

    Fixed64 operator*(Fixed64 o) const { return Fixed64{Mul64Q32(Raw, o.Raw)}; }

    // Division: (a / b) in Q32.32 = (a << 32) / b. Use __int128 for the dividend.
    Fixed64 operator/(Fixed64 o) const {
#if defined(__SIZEOF_INT128__)
        __int128 dividend = (__int128)Raw << 32;
        return Fixed64{(int64_t)(dividend / o.Raw)};
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        // SIM-LINT-OK: same platform exception as Mul64.h
        int64_t hi = Raw >> 32;
        int64_t lo = Raw << 32;
        int64_t rem;
        return Fixed64{_div128(hi, (uint64_t)lo, o.Raw, &rem)};
#else
#error "edge26::Fixed64::operator/ needs 128/64 div implementation"
#endif
    }

    constexpr bool operator==(Fixed64 o) const { return Raw == o.Raw; }
    constexpr bool operator!=(Fixed64 o) const { return Raw != o.Raw; }
    constexpr bool operator<(Fixed64 o)  const { return Raw <  o.Raw; }
    constexpr bool operator<=(Fixed64 o) const { return Raw <= o.Raw; }
    constexpr bool operator>(Fixed64 o)  const { return Raw >  o.Raw; }
    constexpr bool operator>=(Fixed64 o) const { return Raw >= o.Raw; }
};

}  // namespace edge26
