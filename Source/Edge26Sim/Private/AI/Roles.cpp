// Copyright Edge26. All Rights Reserved.
#include "AI/Roles.h"

namespace edge26 {

// Helper: decimal-literal to Fixed32 at compile-eval time.
static constexpr Fixed32 F32(double v) {  // SIM-LINT-OK: compile-time only; no runtime double arithmetic
    return Fixed32::FromRaw((int32_t)(v * (double)Fixed32::One));  // SIM-LINT-OK: compile-time constant folding; equivalent to pre-computing raw integers
}

// M12 tune: weights below are the second-pass values after a 30-second PIE soak
// showed the game stalled mid-pitch with no shots taken. Changes:
//  - Strikers/wingers/CAM: MakeRunForward boosted to ≥1.5 (was 0.85–0.95) so
//    attackers consistently push beyond midfield instead of slotting back.
//  - Strikers/CAM: PreferShoot boosted (ST 1.0→1.5, CAM 0.85→1.2) so they
//    actually take shots when they reach the opp third.
//  - CDM/CM PreferLongBall raised (0.2/0.4→0.6) so midfielders launch the
//    striker on the run instead of recycling possession.
//  - CB/FB PreferLongBall raised (0.3/0.6→0.8) so defenders clear forward
//    rather than passing safely sideways under press.
const FRoleWeights kRoleWeightsTable[(int)ERole::Count] = {
    // GK — no off-ball intents at all; uses dedicated GK path (M8) not Layer C.
    { F32(0.0), F32(1.0), F32(0.0), F32(0.0), F32(0.0), F32(0.0), F32(0.0),
      F32(0.5), F32(0.0), F32(0.0), F32(1.0) },
    // CB — defensive line + tracking; clear forward instead of safe-pass.
    { F32(0.1), F32(0.9), F32(0.0), F32(0.0), F32(0.2), F32(0.95), F32(1.0),
      F32(0.6), F32(0.1), F32(0.0), F32(0.8) },
    // FB_L — overlap when nominated; longball when no overlap.
    { F32(0.7), F32(0.4), F32(0.0), F32(0.95), F32(0.4), F32(0.85), F32(0.7),
      F32(0.7), F32(0.2), F32(0.3), F32(0.6) },
    // FB_R (mirror)
    { F32(0.7), F32(0.4), F32(0.0), F32(0.95), F32(0.4), F32(0.85), F32(0.7),
      F32(0.7), F32(0.2), F32(0.3), F32(0.6) },
    // CDM — anchor, drop, press; long-ball forward to launch attackers.
    { F32(0.3), F32(0.8), F32(0.95), F32(0.0), F32(0.7), F32(0.6), F32(0.3),
      F32(1.0), F32(0.3), F32(0.2), F32(0.6) },
    // CM — push forward + long-ball more often.
    { F32(0.8), F32(0.5), F32(0.7), F32(0.0), F32(0.6), F32(0.5), F32(0.1),
      F32(0.95), F32(0.5), F32(0.4), F32(0.6) },
    // CAM — attack-minded; bigger MakeRunForward + bigger PreferShoot.
    { F32(1.5), F32(0.3), F32(0.4), F32(0.0), F32(0.4), F32(0.3), F32(0.0),
      F32(0.9), F32(1.2), F32(0.6), F32(0.0) },
    // W_L — hug touchline + push hard up the channel.
    { F32(1.5), F32(0.2), F32(0.0), F32(1.0), F32(0.3), F32(0.4), F32(0.0),
      F32(0.7), F32(0.8), F32(1.0), F32(0.0) },
    // W_R (mirror)
    { F32(1.5), F32(0.2), F32(0.0), F32(1.0), F32(0.3), F32(0.4), F32(0.0),
      F32(0.7), F32(0.8), F32(1.0), F32(0.0) },
    // ST — top-of-pitch finisher; shoot eagerly, run constantly.
    { F32(1.8), F32(0.1), F32(0.0), F32(0.0), F32(0.5), F32(0.0), F32(0.0),
      F32(0.5), F32(1.5), F32(0.5), F32(0.0) },
};

}  // namespace edge26
