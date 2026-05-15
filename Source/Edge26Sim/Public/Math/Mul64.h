// Copyright Edge26. All Rights Reserved.
// 64x64 -> 128 multiply, then right-shift by 32 to return Q32.32 product.
// This is the ONLY platform-conditional file in Edge26Sim. See spec §4.3.
// SIM-LINT-OK: this entire file documents the single allowed exception.
#pragma once

#include <cstdint>

namespace edge26 {

#if defined(__SIZEOF_INT128__)

inline int64_t Mul64Q32(int64_t a, int64_t b) {
    __int128 prod = (__int128)a * (__int128)b;
    return (int64_t)(prod >> 32);
}

#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))

#include <intrin.h>
inline int64_t Mul64Q32(int64_t a, int64_t b) {
    int64_t hi;
    int64_t lo = _mul128(a, b, &hi);
    return (int64_t)((uint64_t)lo >> 32) | (hi << 32);
}

#else
#error "edge26::Mul64Q32 needs a 64x64->128 mul implementation for this platform"
#endif

}  // namespace edge26
