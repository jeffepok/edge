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

struct FInputFrame {
    uint32_t TickNumber;
    int8_t   Move[2][2];   // [player][axis], range [-127, 127]; axis 0=X, 1=Y
    uint8_t  Buttons[2];   // per-player bitfield (InputButton flags)
    uint16_t _pad;         // explicit pad to 16 bytes total
};
static_assert(sizeof(FInputFrame) == 16, "FInputFrame must be 16 bytes");

}  // namespace edge26
