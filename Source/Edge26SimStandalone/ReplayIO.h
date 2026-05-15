// ReplayIO.h — read/write FInputFrame binary streams. Standalone-only.
#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>
#include "Sim/InputFrame.h"
#include "Sim/InputStream.h"

namespace edge26 {

// Returns true on success. On failure, prints to stderr and returns false.
bool ReadReplay (const char* path, std::vector<FInputFrame>& outFrames);
bool WriteReplay(const char* path, const std::vector<FInputFrame>& frames);

}  // namespace edge26
