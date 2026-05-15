// Copyright Edge26. All Rights Reserved.
#pragma once

#include <cstdint>
#include "Sim/WorldState.h"
#include "Sim/InputFrame.h"

namespace edge26 {

class SimWorld {
public:
    explicit SimWorld(uint64_t rngSeed);

    void Step(const FInputFrame& frame);

    // Read-only access (snapshot/restore/hash come in M3).
    const FSimWorldState& GetState() const { return State; }
          FSimWorldState& MutableState()    { return State; }

private:
    FSimWorldState State;
};

}  // namespace edge26
