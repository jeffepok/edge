// Copyright Edge26. All Rights Reserved.
// Xorshift64 PRNG. State is a single uint64 — fits in the snapshot.
#pragma once

#include <cstdint>

namespace edge26 {

struct Rng {
    uint64_t State;  // must be non-zero to advance

    explicit constexpr Rng(uint64_t seed) : State(seed ? seed : 0xDEADBEEFCAFEBABEull) {}

    uint64_t NextU64() {
        uint64_t x = State;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        State = x;
        return x;
    }
};

}  // namespace edge26
