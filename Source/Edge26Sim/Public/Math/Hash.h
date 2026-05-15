// Copyright Edge26. All Rights Reserved.
// Vendored xxHash64 (MIT). Reference: https://xxhash.com — single-pass variant.
// Trimmed implementation; only the functions we need.
#pragma once

#include <cstdint>
#include <cstring>

namespace edge26 { namespace Hash {

namespace detail {
constexpr uint64_t kPrime1 = 11400714785074694791ull;
constexpr uint64_t kPrime2 = 14029467366897019727ull;
constexpr uint64_t kPrime3 =  1609587929392839161ull;
constexpr uint64_t kPrime4 =  9650029242287828579ull;
constexpr uint64_t kPrime5 =  2870177450012600261ull;

inline uint64_t rotl(uint64_t v, int n) { return (v << n) | (v >> (64 - n)); }
inline uint64_t round_(uint64_t acc, uint64_t input) {
    acc += input * kPrime2;
    acc  = rotl(acc, 31);
    acc *= kPrime1;
    return acc;
}
inline uint64_t merge(uint64_t acc, uint64_t val) {
    val   = round_(0, val);
    acc  ^= val;
    acc   = acc * kPrime1 + kPrime4;
    return acc;
}
inline uint64_t load_le_u64(const uint8_t* p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return v;  // assume little-endian; PS5/Xbox/PC are all LE
}
inline uint32_t load_le_u32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
}  // namespace detail

inline uint64_t XXH64(const void* input, size_t len, uint64_t seed) {
    using namespace detail;
    const uint8_t* p   = static_cast<const uint8_t*>(input);
    const uint8_t* end = p + len;
    uint64_t h64;

    if (len >= 32) {
        const uint8_t* limit = end - 32;
        uint64_t v1 = seed + kPrime1 + kPrime2;
        uint64_t v2 = seed + kPrime2;
        uint64_t v3 = seed;
        uint64_t v4 = seed - kPrime1;
        do {
            v1 = round_(v1, load_le_u64(p)); p += 8;
            v2 = round_(v2, load_le_u64(p)); p += 8;
            v3 = round_(v3, load_le_u64(p)); p += 8;
            v4 = round_(v4, load_le_u64(p)); p += 8;
        } while (p <= limit);

        h64 = rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18);
        h64 = merge(h64, v1);
        h64 = merge(h64, v2);
        h64 = merge(h64, v3);
        h64 = merge(h64, v4);
    } else {
        h64 = seed + kPrime5;
    }

    h64 += (uint64_t)len;

    while (p + 8 <= end) {
        uint64_t k = round_(0, load_le_u64(p));
        h64 ^= k;
        h64  = rotl(h64, 27) * kPrime1 + kPrime4;
        p += 8;
    }
    if (p + 4 <= end) {
        h64 ^= (uint64_t)load_le_u32(p) * kPrime1;
        h64  = rotl(h64, 23) * kPrime2 + kPrime3;
        p += 4;
    }
    while (p < end) {
        h64 ^= (uint64_t)(*p) * kPrime5;
        h64  = rotl(h64, 11) * kPrime1;
        ++p;
    }

    h64 ^= h64 >> 33; h64 *= kPrime2;
    h64 ^= h64 >> 29; h64 *= kPrime3;
    h64 ^= h64 >> 32;
    return h64;
}

} }  // namespace edge26::Hash
