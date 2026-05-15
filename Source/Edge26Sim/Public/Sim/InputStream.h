// Copyright Edge26. All Rights Reserved.
// Binary replay stream format. 16-byte header + N × FInputFrame records.
#pragma once

#include <cstdint>
#include "Sim/InputFrame.h"

namespace edge26 {

// Magic = "EDG26IN0" ASCII = 0x304E_4936_3247_4445 in little-endian.
constexpr uint64_t kInputStreamMagic = 0x304E493632474445ull;
constexpr uint32_t kInputStreamVersion = 1;

struct FInputStreamHeader {
    uint64_t Magic;       // kInputStreamMagic
    uint32_t Version;     // kInputStreamVersion
    uint32_t TickCount;   // number of FInputFrame records that follow
};
static_assert(sizeof(FInputStreamHeader) == 16, "FInputStreamHeader must be 16 bytes");

}  // namespace edge26
