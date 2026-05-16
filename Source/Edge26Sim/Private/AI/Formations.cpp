// Copyright Edge26. All Rights Reserved.
#include "AI/Formations.h"
#include "Sim/Constants.h"

namespace edge26 {

// Pitch is 105m x 68m, so half-len 5250 cm, half-wid 3400 cm.
// NormalizedX=-1 maps to -PitchHalfLen (own goal); +1 maps to +PitchHalfLen (opp goal).
// NormalizedY=-1 maps to -PitchHalfWid; +1 maps to +PitchHalfWid.
//
// Raw values are pre-computed Q32.32 integers: value = Fixed64::One * normalized_coefficient (computed offline).
// Pre-computing avoids floating-point in the translation unit entirely.
// Tuned by eye in v0; will be revisited during the M12 tuning pass.
const FFormationSlot kFormation_4_3_3[11] = {
    // GK: NX=-0.95, NY=0.00
    { ERole::GK,   Fixed64::FromRaw((int64_t)-4080218931LL), Fixed64::FromRaw((int64_t)0) },
    // CB: NX=-0.65, NY=-0.15
    { ERole::CB,   Fixed64::FromRaw((int64_t)-2791728742LL), Fixed64::FromRaw((int64_t)-644245094LL) },
    // CB: NX=-0.65, NY=+0.15
    { ERole::CB,   Fixed64::FromRaw((int64_t)-2791728742LL), Fixed64::FromRaw((int64_t) 644245094LL) },
    // FB_L: NX=-0.55, NY=-0.65
    { ERole::FB_L, Fixed64::FromRaw((int64_t)-2362232012LL), Fixed64::FromRaw((int64_t)-2791728742LL) },
    // FB_R: NX=-0.55, NY=+0.65
    { ERole::FB_R, Fixed64::FromRaw((int64_t)-2362232012LL), Fixed64::FromRaw((int64_t) 2791728742LL) },
    // CDM: NX=-0.25, NY=0.00
    { ERole::CDM,  Fixed64::FromRaw((int64_t)-1073741824LL), Fixed64::FromRaw((int64_t)0) },
    // CM: NX=-0.10, NY=-0.30
    { ERole::CM,   Fixed64::FromRaw((int64_t)-429496729LL),  Fixed64::FromRaw((int64_t)-1288490188LL) },
    // CM: NX=-0.10, NY=+0.30
    { ERole::CM,   Fixed64::FromRaw((int64_t)-429496729LL),  Fixed64::FromRaw((int64_t) 1288490188LL) },
    // W_L: NX=+0.40, NY=-0.70
    { ERole::W_L,  Fixed64::FromRaw((int64_t) 1717986918LL), Fixed64::FromRaw((int64_t)-3006477107LL) },
    // W_R: NX=+0.40, NY=+0.70
    { ERole::W_R,  Fixed64::FromRaw((int64_t) 1717986918LL), Fixed64::FromRaw((int64_t) 3006477107LL) },
    // ST: NX=+0.50, NY=0.00
    { ERole::ST,   Fixed64::FromRaw((int64_t) 2147483648LL), Fixed64::FromRaw((int64_t)0) },
};

FixedVec3 SlotWorldPosition(int slotIndex, int teamId) {
    const FFormationSlot& slot = kFormation_4_3_3[slotIndex];
    // Home (teamId 0): NormalizedX as-is. Away (teamId 1): flip sign.
    Fixed64 signX = (teamId == 0) ? Fixed64::FromInt(1) : Fixed64::FromInt(-1);
    Fixed64 x = slot.NormalizedX * signX * SimConst::PitchHalfLen;
    Fixed64 y = slot.NormalizedY * SimConst::PitchHalfWid;
    return FixedVec3{ x, y, Fixed64::FromInt(0) };
}

}  // namespace edge26
