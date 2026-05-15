// Copyright Edge26. All Rights Reserved.
#pragma once

#include <cstdint>
#include "Edge26SimAPI.h"
#include "Sim/WorldState.h"
#include "Sim/InputFrame.h"

namespace edge26 {

class EDGE26SIM_API SimWorld {
public:
    explicit SimWorld(uint64_t rngSeed);

    void Step(const FInputFrame& frame);

    void Snapshot(FSimWorldState& out) const;
    void Restore(const FSimWorldState& in);
    uint64_t HashState() const;

    const FSimWorldState& GetState() const { return State; }
          FSimWorldState& MutableState()    { return State; }

private:
    FSimWorldState State;
};

}  // namespace edge26
