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

constexpr Fixed64 Abs(Fixed64 a)  { return a.Raw < 0 ? Fixed64{-a.Raw} : a; }
constexpr Fixed64 Min(Fixed64 a, Fixed64 b) { return a.Raw < b.Raw ? a : b; }
constexpr Fixed64 Max(Fixed64 a, Fixed64 b) { return a.Raw > b.Raw ? a : b; }
constexpr Fixed64 Clamp(Fixed64 v, Fixed64 lo, Fixed64 hi) {
    return v.Raw < lo.Raw ? lo : (v.Raw > hi.Raw ? hi : v);
}

struct Fixed32 {
    int32_t Raw;

    static constexpr int32_t Shift = 16;
    static constexpr int32_t One   = 1 << 16;
    static constexpr int32_t Zero  = 0;

    static constexpr Fixed32 FromRaw(int32_t r) { return Fixed32{r}; }
    static constexpr Fixed32 FromInt(int32_t i) { return Fixed32{i << Shift}; }

    constexpr int32_t ToInt() const { return Raw >> Shift; }

    constexpr Fixed32 operator+(Fixed32 o) const { return Fixed32{Raw + o.Raw}; }
    constexpr Fixed32 operator-(Fixed32 o) const { return Fixed32{Raw - o.Raw}; }
    constexpr Fixed32 operator-()           const { return Fixed32{-Raw}; }

    // 32x32 -> 64 then >>16 for Q16.16 multiply. Always safe in int64.
    constexpr Fixed32 operator*(Fixed32 o) const {
        return Fixed32{(int32_t)(((int64_t)Raw * (int64_t)o.Raw) >> Shift)};
    }
    constexpr Fixed32 operator/(Fixed32 o) const {
        return Fixed32{(int32_t)(((int64_t)Raw << Shift) / o.Raw)};
    }

    constexpr bool operator==(Fixed32 o) const { return Raw == o.Raw; }
    constexpr bool operator!=(Fixed32 o) const { return Raw != o.Raw; }
    constexpr bool operator<(Fixed32 o)  const { return Raw <  o.Raw; }
    constexpr bool operator<=(Fixed32 o) const { return Raw <= o.Raw; }
    constexpr bool operator>(Fixed32 o)  const { return Raw >  o.Raw; }
    constexpr bool operator>=(Fixed32 o) const { return Raw >= o.Raw; }
};

constexpr Fixed32 Abs(Fixed32 a)  { return a.Raw < 0 ? Fixed32{-a.Raw} : a; }
constexpr Fixed32 Min(Fixed32 a, Fixed32 b) { return a.Raw < b.Raw ? a : b; }
constexpr Fixed32 Max(Fixed32 a, Fixed32 b) { return a.Raw > b.Raw ? a : b; }
constexpr Fixed32 Clamp(Fixed32 v, Fixed32 lo, Fixed32 hi) {
    return v.Raw < lo.Raw ? lo : (v.Raw > hi.Raw ? hi : v);
}

}  // namespace edge26
