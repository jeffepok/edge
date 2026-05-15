// Copyright Edge26. All Rights Reserved.
// Quantized per-tick input. Same struct will later serialize over the wire.
#pragma once

#include <cstdint>

namespace edge26 {

// Button bitfield positions (per player).
namespace InputButton {
    constexpr uint8_t Sprint = 1 << 0;
    constexpr uint8_t Pass   = 1 << 1;
    constexpr uint8_t Shoot  = 1 << 2;
    constexpr uint8_t Chip   = 1 << 3;
}

// Natural layout: 4 (Tick) + 4 (Move[2][2]) + 2 (Buttons) + 2 (_pad) = 12 bytes.
// Max alignment = uint32 = 4, so no trailing pad. All bytes named; deterministic on every compiler.
struct FInputFrame {
    uint32_t TickNumber;
    int8_t   Move[2][2];   // [player][axis], range [-127, 127]; axis 0=X, 1=Y
    uint8_t  Buttons[2];   // per-player bitfield (InputButton flags)
    uint16_t _pad;         // explicit padding (deterministic)
};
static_assert(sizeof(FInputFrame) == 12, "FInputFrame must be 12 bytes");

}  // namespace edge26
