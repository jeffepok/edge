// Copyright Edge26. All Rights Reserved.
#include "AI/Roles.h"

namespace edge26 {

// Helper: decimal-literal to Fixed32 at compile-eval time.
static constexpr Fixed32 F32(double v) {  // SIM-LINT-OK: compile-time only; no runtime double arithmetic
    return Fixed32::FromRaw((int32_t)(v * (double)Fixed32::One));  // SIM-LINT-OK: compile-time constant folding; equivalent to pre-computing raw integers
}

const FRoleWeights kRoleWeightsTable[(int)ERole::Count] = {
    // GK — no off-ball intents at all; uses dedicated GK path (M8) not Layer C.
    { F32(0.0), F32(1.0), F32(0.0), F32(0.0), F32(0.0), F32(0.0), F32(0.0),
      F32(0.5), F32(0.0), F32(0.0), F32(1.0) },
    // CB — defensive line + tracking
    { F32(0.1), F32(0.9), F32(0.0), F32(0.0), F32(0.2), F32(0.95), F32(1.0),
      F32(0.7), F32(0.1), F32(0.0), F32(0.6) },
    // FB_L
    { F32(0.5), F32(0.4), F32(0.0), F32(0.95), F32(0.4), F32(0.85), F32(0.7),
      F32(0.8), F32(0.2), F32(0.3), F32(0.3) },
    // FB_R (mirror of FB_L)
    { F32(0.5), F32(0.4), F32(0.0), F32(0.95), F32(0.4), F32(0.85), F32(0.7),
      F32(0.8), F32(0.2), F32(0.3), F32(0.3) },
    // CDM — anchor, drop, press
    { F32(0.2), F32(0.8), F32(0.95), F32(0.0), F32(0.7), F32(0.6), F32(0.3),
      F32(1.0), F32(0.3), F32(0.2), F32(0.4) },
    // CM
    { F32(0.6), F32(0.5), F32(0.7), F32(0.0), F32(0.6), F32(0.5), F32(0.1),
      F32(0.95), F32(0.5), F32(0.4), F32(0.2) },
    // CAM
    { F32(0.85), F32(0.4), F32(0.4), F32(0.0), F32(0.4), F32(0.3), F32(0.0),
      F32(0.9), F32(0.85), F32(0.6), F32(0.0) },
    // W_L
    { F32(0.9), F32(0.3), F32(0.0), F32(1.0), F32(0.3), F32(0.4), F32(0.0),
      F32(0.7), F32(0.7), F32(1.0), F32(0.0) },
    // W_R (mirror)
    { F32(0.9), F32(0.3), F32(0.0), F32(1.0), F32(0.3), F32(0.4), F32(0.0),
      F32(0.7), F32(0.7), F32(1.0), F32(0.0) },
    // ST
    { F32(0.95), F32(0.1), F32(0.0), F32(0.0), F32(0.5), F32(0.0), F32(0.0),
      F32(0.5), F32(1.0), F32(0.5), F32(0.0) },
};

}  // namespace edge26
