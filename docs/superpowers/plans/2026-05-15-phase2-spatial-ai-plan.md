# Edge 26 — Phase 2: Spatial AI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Take the Phase 1 sim from 2-player kinematic shells to a full 11v11 football match with three-layer AI (Team Strategy / Unit Coordination / Individual), a 5-field spatial value model, offside, simple GK, FIFA-style player switching, and an AI debug overlay.

**Architecture:** Sim-authoritative, deterministic 50 Hz fixed-point. AI lives entirely inside `Edge26Sim` as plain C++. Layers A (2 Hz) → B (10 Hz) → C (50 Hz) cascade through shared `FSpatialValueModel` + `FMatchState`. Visual-shell adapter and debug overlay live in `Edge26`. Snapshot grows from 224 B to ~73 KB but remains POD/memcpy/xxhashable.

**Tech Stack:** C++17, UE5.7 UBT for in-engine build, CMake 3.20+ for standalone, xxHash64 (vendored), bash for CI scripts, GitHub Actions for the 3-OS determinism matrix, UE5 Python for asset editing.

**Reference spec:** [`docs/superpowers/specs/2026-05-15-phase2-spatial-ai-design.md`](../specs/2026-05-15-phase2-spatial-ai-design.md). Read it before starting any task.

**Determinism guardrails (read first, never violate):**
- Inside `Source/Edge26Sim/`: no `float`/`double`, no `FVector`/`FMath`, no `std::unordered_*`, no threads, no wall-clock, no engine includes, no heap allocation in `Step()` (or any layer's per-tick function). Full list in Phase 1 spec §4.
- All sim-state structs are POD with explicit padding and `static_assert` on size. Zero-init via `memset` before any field assignment.
- Iteration order is always linear (player index, cell index, intent enum). When two scores tie, first-evaluated wins.

---

## Task index

- **M0** — Pre-flight (T0.1 – T0.3)
- **M1** — Roster expansion to 22 players + roles + formations (T1.1 – T1.10)
- **M2** — Spatial Value Model (5 fields × 1768 cells) (T2.1 – T2.9)
- **M3** — Layer C off-ball intents (T3.1 – T3.5)
- **M4** — Layer C on-ball decisions (T4.1 – T4.6)
- **M5** — Layer B unit coordination (T5.1 – T5.7)
- **M6** — Layer A team strategy (T6.1 – T6.5)
- **M7** — Offside enforcement (T7.1 – T7.5)
- **M8** — Simple goalkeeper AI (T8.1 – T8.6)
- **M9** — Player switching (T9.1 – T9.7)
- **M10** — AI debug overlay (T10.1 – T10.7)
- **M11** — CI baselines for 22-player streams (T11.1 – T11.4)
- **M12** — AI tuning pass + final acceptance (T12.1 – T12.6)

After every milestone: update `PROGRESS.md` with a brief activity-log entry.

---

## M0 — Pre-flight

Before touching code, verify Phase 1 is healthy and create a feature branch for Phase 2 work. This avoids the "merging into a dirty main" pain we hit between v0 and PIE polish.

### Task T0.1 — Verify Phase 1 baseline is green

**Files:** none modified.

- [ ] **Step 1: Run the determinism gate to make sure we're starting clean**

```bash
./Scripts/check_determinism.sh
```

Expected last line: `PASS: all determinism checks`.

- [ ] **Step 2: Verify the editor still builds**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex
```

Expected last line: `Result: Succeeded`.

- [ ] **Step 3: Confirm we're on `main` with no uncommitted changes**

```bash
git status
```

Expected: `On branch main`, nothing to commit, working tree clean.

If anything fails: stop here, fix Phase 1 first. Don't proceed.

### Task T0.2 — Create the Phase 2 development branch

**Files:** branch-level.

- [ ] **Step 1: Create + switch to the feature branch**

```bash
git checkout -b feat/phase2-spatial-ai
```

- [ ] **Step 2: Verify**

```bash
git branch --show-current
```

Expected: `feat/phase2-spatial-ai`.

### Task T0.3 — Add Phase 2 to PROGRESS.md roadmap

**Files:**
- Modify: `PROGRESS.md`

- [ ] **Step 1: Replace the Phase 2 placeholder with milestone list**

Find this block:

```markdown
### Phase 2: Spatial Value Model + 22-player AI  (placeholder)
```

Replace with:

```markdown
### Phase 2: Spatial Value Model + 22-player AI  ←  current
- [ ] M1. Roster expansion: 22 players, roles, formations, kickoff placement
- [ ] M2. Spatial Value Model (5 fields × 1768 cells)
- [ ] M3. Layer C off-ball intents
- [ ] M4. Layer C on-ball decisions
- [ ] M5. Layer B unit coordination (defensive line, press, overlap)
- [ ] M6. Layer A team strategy (mentality, late-game adjustments)
- [ ] M7. Offside enforcement
- [ ] M8. Simple goalkeeper AI
- [ ] M9. Player switching (auto + manual + camera)
- [ ] M10. AI debug overlay (heatmaps + intent arrows)
- [ ] M11. CI baselines for 22-player streams
- [ ] M12. AI tuning pass + final acceptance
```

- [ ] **Step 2: Update the Current Status to point at Phase 2 M1**

Find the "Current status" paragraph at the top. Replace with:

```markdown
## Current status

**Phase 1: Sim Core v0 is COMPLETE.** Phase 2: Spatial AI is in flight.
Currently at **Phase 2 M1 of M12** (roster expansion). Branch:
`feat/phase2-spatial-ai`. Spec: `docs/superpowers/specs/2026-05-15-phase2-spatial-ai-design.md`.
Plan: `docs/superpowers/plans/2026-05-15-phase2-spatial-ai-plan.md`.
```

- [ ] **Step 3: Add a new activity log entry**

Append to the bottom of the activity log:

```markdown
### 2026-05-15 — Session 2 (Phase 2 brainstorming + plan)
- Brainstormed Phase 2 design. Spec committed: `docs/superpowers/specs/2026-05-15-phase2-spatial-ai-design.md`.
- Implementation plan committed: `docs/superpowers/plans/2026-05-15-phase2-spatial-ai-plan.md`.
- Decisions D1–D10 locked. Estimated 8-10 weeks.
- Next: M1 roster expansion.
```

- [ ] **Step 4: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): kick off; PROGRESS roadmap shows M1-M12"
```

---

## M1 — Roster expansion to 22 players + roles + formations

This milestone doesn't add AI yet. It expands the sim from 2 players to 22, adds team and role identity to each, and changes kickoff placement to use a formation. All 21 non-human players will stand still after kickoff (no AI yet — that's M3+). The point of this milestone is to verify the sim still passes the determinism gate at 22 players, the snapshot scales correctly, and the L_Pitch level can hold a full match roster.

### Task T1.1 — Define the `ERole` enum

**Files:**
- Create: `Source/Edge26Sim/Public/AI/Roles.h`

- [ ] **Step 1: Create the new directory**

```bash
mkdir -p Source/Edge26Sim/Public/AI Source/Edge26Sim/Private/AI
```

- [ ] **Step 2: Write `Roles.h` with the enum only**

```cpp
// Copyright Edge26. All Rights Reserved.
// Per-player role identity. Drives Layer C decision weights (FRoleWeights —
// added in M3) and formation slot mapping (FFormationSlot — added in T1.3).
#pragma once

#include <cstdint>

namespace edge26 {

enum class ERole : uint8_t {
    GK   = 0,   // Goalkeeper
    CB   = 1,   // Center Back
    FB_L = 2,   // Left Full Back
    FB_R = 3,   // Right Full Back
    CDM  = 4,   // Defensive Mid
    CM   = 5,   // Central Mid
    CAM  = 6,   // Attacking Mid
    W_L  = 7,   // Left Wing
    W_R  = 8,   // Right Wing
    ST   = 9,   // Striker
    Count = 10
};

}  // namespace edge26
```

- [ ] **Step 3: Smoke-build standalone**

```bash
cmake --build build/sim --parallel
```

Expected: builds cleanly (the header isn't included anywhere yet).

- [ ] **Step 4: Lint**

```bash
./Scripts/lint_sim.sh
```

Expected: `lint_sim.sh: OK`.

- [ ] **Step 5: Commit**

```bash
git add Source/Edge26Sim/Public/AI/Roles.h
git commit -m "feat(ai): ERole enum (GK/CB/FB/CDM/CM/CAM/W/ST)"
```

### Task T1.2 — Extend `FSimPlayerState` from 64 B to 88 B

Per spec §4: adds `TeamId`, `RoleId`, `CurrentIntent`, `IntendedPassTarget`, `AITargetPosition`. The struct size jumps by 24 bytes.

**Files:**
- Modify: `Source/Edge26Sim/Public/Sim/PlayerState.h`
- Modify: `Source/Edge26SimStandalone/tests/test_snapshot.cpp`

- [ ] **Step 1: Update the test for the new size**

Find the `WorldState_Sizes` test in `test_snapshot.cpp`. Update the player-state expected size:

```cpp
TEST_CASE(WorldState_Sizes) {
    TEST_EXPECT_EQ((int64_t)sizeof(FSimBallState),   (int64_t)80);
    TEST_EXPECT_EQ((int64_t)sizeof(FSimPlayerState), (int64_t)88);
    // FSimWorldState size assertion is deferred — it grows substantially in M2.
    return 0;
}
```

- [ ] **Step 2: Build, expect the world-state size test to fail and the player size to mismatch**

```bash
cmake --build build/sim --parallel 2>&1 | tail -10
```

Expected: build succeeds but `--self-test` will fail on `WorldState_Sizes` because PlayerState is still 64 B.

- [ ] **Step 3: Extend `FSimPlayerState`**

Replace the struct definition with:

```cpp
// Copyright Edge26. All Rights Reserved.
#pragma once

#include <cstdint>
#include "Math/FixedVec.h"
#include "Math/FixedAngle.h"

namespace edge26 {

namespace PlayerFlag {
    constexpr uint8_t Grounded  = 1 << 0;
    constexpr uint8_t Sprinting = 1 << 1;
}

constexpr uint8_t kStationaryController = 0xFF;

struct FSimPlayerState {
    FixedVec3   Position;            // 24 B (offset 0)
    FixedVec3   Velocity;            // 24 B (offset 24)
    FixedAngle  Heading;             //  4 B (offset 48)
    FixedAngle  FacingTarget;        //  4 B (offset 52)
    uint8_t     ControllerIndex;     //  1 B (offset 56) — vestigial; see Match.HumanControlledIndex (M1.5)
    uint8_t     Flags;               //  1 B (offset 57) — Grounded, Sprinting
    uint8_t     TeamId;              //  1 B (offset 58) — 0 = home, 1 = away
    uint8_t     RoleId;              //  1 B (offset 59) — ERole
    uint8_t     CurrentIntent;       //  1 B (offset 60) — EIntent (written by Layer C; 0 until M3)
    uint8_t     IntendedPassTarget;  //  1 B (offset 61) — player idx (0xFF if none)
    uint8_t     _pad[2];             //  2 B (offset 62-63) — explicit alignment
    FixedVec3   AITargetPosition;    // 24 B (offset 64-87) — where AI wants the player to go
};
static_assert(sizeof(FSimPlayerState) == 88, "FSimPlayerState must be 88 bytes");

}  // namespace edge26
```

- [ ] **Step 4: Build + run, expect pass for PlayerState size; sim tests for movement still pass (state is zero-initialized so new fields are 0)**

```bash
cmake --build build/sim --parallel
./build/sim/edge26_sim_replay --self-test 2>&1 | tail -10
```

Expected: all existing tests pass except they might be slow; the sim still runs at 2 players because `kSimPlayerCount` hasn't changed yet.

- [ ] **Step 5: Commit**

```bash
git add Source/Edge26Sim/Public/Sim/PlayerState.h Source/Edge26SimStandalone/tests/test_snapshot.cpp
git commit -m "feat(sim): extend FSimPlayerState to 88 bytes (TeamId, RoleId, intent fields)"
```

### Task T1.3 — Define `FFormationSlot` + hardcoded 4-3-3

**Files:**
- Create: `Source/Edge26Sim/Public/AI/Formations.h`
- Create: `Source/Edge26Sim/Private/AI/Formations.cpp`

- [ ] **Step 1: Write `Formations.h`**

```cpp
// Copyright Edge26. All Rights Reserved.
// Hardcoded 4-3-3 formation. v0 ships one formation per team (data-driven
// presets are a later phase). Slot positions are NORMALIZED ([-1, 1] in X
// and Y); world positions are derived per team in Formations.cpp.
#pragma once

#include <cstdint>
#include "AI/Roles.h"
#include "Math/Fixed.h"
#include "Math/FixedVec.h"

namespace edge26 {

struct FFormationSlot {
    ERole    Role;
    Fixed64  NormalizedX;   // -1 = own goal, +1 = opponent goal
    Fixed64  NormalizedY;   // -1 = left, +1 = right
};

// 11 slots. GK first, then defenders, mids, attackers.
extern const FFormationSlot kFormation_4_3_3[11];

// Compute the world position for a slot, given the team (0=home, 1=away).
// Home defends -Y end; away defends +Y end (so NormalizedX flips for away).
FixedVec3 SlotWorldPosition(int slotIndex, int teamId);

}  // namespace edge26
```

- [ ] **Step 2: Write `Formations.cpp`**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "AI/Formations.h"
#include "Sim/Constants.h"

namespace edge26 {

// Pitch is 105m × 68m, so half-len 5250 cm, half-wid 3400 cm.
// NormalizedX=-1 maps to -PitchHalfLen (own goal); +1 maps to +PitchHalfLen (opp goal).
// NormalizedY=-1 maps to -PitchHalfWid; +1 maps to +PitchHalfWid.

// Coefficients chosen so the 11 slots cover the pitch in a reasonable 4-3-3.
// Tuned by eye in v0; will be revisited during the M12 tuning pass.
const FFormationSlot kFormation_4_3_3[11] = {
    { ERole::GK,    Fixed64::FromRaw((int64_t)(-0.95 * (double)Fixed64::One)),  Fixed64::FromRaw(0) },
    { ERole::CB,    Fixed64::FromRaw((int64_t)(-0.65 * (double)Fixed64::One)),  Fixed64::FromRaw((int64_t)(-0.15 * (double)Fixed64::One)) },
    { ERole::CB,    Fixed64::FromRaw((int64_t)(-0.65 * (double)Fixed64::One)),  Fixed64::FromRaw((int64_t)( 0.15 * (double)Fixed64::One)) },
    { ERole::FB_L,  Fixed64::FromRaw((int64_t)(-0.55 * (double)Fixed64::One)),  Fixed64::FromRaw((int64_t)(-0.65 * (double)Fixed64::One)) },
    { ERole::FB_R,  Fixed64::FromRaw((int64_t)(-0.55 * (double)Fixed64::One)),  Fixed64::FromRaw((int64_t)( 0.65 * (double)Fixed64::One)) },
    { ERole::CDM,   Fixed64::FromRaw((int64_t)(-0.25 * (double)Fixed64::One)),  Fixed64::FromRaw(0) },
    { ERole::CM,    Fixed64::FromRaw((int64_t)(-0.10 * (double)Fixed64::One)),  Fixed64::FromRaw((int64_t)(-0.30 * (double)Fixed64::One)) },
    { ERole::CM,    Fixed64::FromRaw((int64_t)(-0.10 * (double)Fixed64::One)),  Fixed64::FromRaw((int64_t)( 0.30 * (double)Fixed64::One)) },
    { ERole::W_L,   Fixed64::FromRaw((int64_t)( 0.40 * (double)Fixed64::One)),  Fixed64::FromRaw((int64_t)(-0.70 * (double)Fixed64::One)) },
    { ERole::W_R,   Fixed64::FromRaw((int64_t)( 0.40 * (double)Fixed64::One)),  Fixed64::FromRaw((int64_t)( 0.70 * (double)Fixed64::One)) },
    { ERole::ST,    Fixed64::FromRaw((int64_t)( 0.50 * (double)Fixed64::One)),  Fixed64::FromRaw(0) },
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
```

- [ ] **Step 3: Write the failing test in `test_snapshot.cpp`**

Append:

```cpp
#include "AI/Formations.h"

TEST_CASE(Formation_HomeAwaySymmetry) {
    using namespace edge26;
    // GK slot for home should be near -X (own goal); for away, near +X.
    FixedVec3 homeGK = SlotWorldPosition(0, 0);
    FixedVec3 awayGK = SlotWorldPosition(0, 1);
    TEST_EXPECT_TRUE(homeGK.X.Raw < 0);
    TEST_EXPECT_TRUE(awayGK.X.Raw > 0);
    // Y should be identical (both GKs in center)
    TEST_EXPECT_EQ(homeGK.Y.Raw, awayGK.Y.Raw);
    // The 11th slot (ST) for home should be near +X (opp goal); for away near -X.
    FixedVec3 homeST = SlotWorldPosition(10, 0);
    FixedVec3 awayST = SlotWorldPosition(10, 1);
    TEST_EXPECT_TRUE(homeST.X.Raw > 0);
    TEST_EXPECT_TRUE(awayST.X.Raw < 0);
    return 0;
}
```

Add `TEST_RUN(Formation_HomeAwaySymmetry);` in `RunSnapshotTests()`.

- [ ] **Step 4: Build + run, expect pass**

```bash
cmake -S Source/Edge26SimStandalone -B build/sim -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -2
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "Formation_HomeAwaySymmetry|Self-test"
```

Expected: `RUN  Formation_HomeAwaySymmetry` and `Self-test OK`.

- [ ] **Step 5: Lint**

```bash
./Scripts/lint_sim.sh
```

Expected: `OK`.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat(ai): FFormationSlot + hardcoded 4-3-3 + home/away X-flip"
```

### Task T1.4 — Bump `kSimPlayerCount` from 2 to 22 (and accept the rolling consequences)

This is THE roster-expansion step. The compile breaks immediately because tests reference `kSimPlayerCount` to size arrays. We fix forward.

**Files:**
- Modify: `Source/Edge26Sim/Public/Sim/WorldState.h`

- [ ] **Step 1: Update the constant**

Find `constexpr int kSimPlayerCount = 2;` and change to:

```cpp
constexpr int kSimPlayerCount = 22;   // 11 per team. Was 2 in Phase 1.
```

- [ ] **Step 2: Update the `FSimWorldState` size assertion comment**

The assertion currently expects 224 B; the new size is `4 + 4 + 8 + 80 + 22*88 = 2032 B` plus future fields. For now we *remove* the strict assertion and replace it with a looser one:

```cpp
struct FSimWorldState {
    uint32_t        TickNumber;
    uint32_t        _pad0;
    uint64_t        RngState;
    FSimBallState   Ball;
    FSimPlayerState Players[kSimPlayerCount];
};
// Strict size assertion is deferred to M2 (when FMatchState + FSpatialValueModel
// are embedded). For now, validate alignment only.
static_assert(alignof(FSimWorldState) == 8, "FSimWorldState must be 8-aligned");
```

- [ ] **Step 3: Build, expect compile failure in tests because of the size assertion in `test_snapshot.cpp`**

```bash
cmake --build build/sim --parallel 2>&1 | tail -10
```

If the existing `WorldState_Sizes` test in `test_snapshot.cpp` has a `static_assert` or `TEST_EXPECT_EQ` on the world-state size, it will fail. Identify and remove (or update) that assertion.

- [ ] **Step 4: Remove the world-state size assertion from the test**

In `Source/Edge26SimStandalone/tests/test_snapshot.cpp`, find this test:

```cpp
TEST_CASE(WorldState_Sizes) {
    TEST_EXPECT_EQ((int64_t)sizeof(FSimBallState),   (int64_t)80);
    TEST_EXPECT_EQ((int64_t)sizeof(FSimPlayerState), (int64_t)88);
    TEST_EXPECT_EQ((int64_t)sizeof(FSimWorldState),  (int64_t)224);  // <-- delete this line
    return 0;
}
```

Delete the world-state size line. Keep only the ball + player state assertions.

- [ ] **Step 5: Build + run all unit tests**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | tail -10
```

Expected: all tests pass. The sim now models 22 players (mostly stationary because no AI yet).

- [ ] **Step 6: Verify the snapshot/restore tests still work at the new size**

The existing tests (Snapshot_RoundTrip, Hash_Stable, Rollback_FullRoundTrip, Hash_PerTickStable) build their own state, run a few ticks, snapshot, restore. They will pass because `FSimWorldState` is still POD — just larger.

```bash
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "Snapshot|Rollback|Hash_PerTick"
```

Expected: all four pass.

- [ ] **Step 7: Commit**

```bash
git add Source/Edge26Sim/Public/Sim/WorldState.h Source/Edge26SimStandalone/tests/test_snapshot.cpp
git commit -m "feat(sim): kSimPlayerCount 2 -> 22; WorldState size assertion deferred to M2"
```

### Task T1.5 — Update `SimWorld` constructor to initialize 22 players with team + role + slot

**Files:**
- Modify: `Source/Edge26Sim/Private/Sim/SimWorld.cpp`
- Modify: `Source/Edge26SimStandalone/tests/test_snapshot.cpp`

- [ ] **Step 1: Update the existing player-init loop**

In `SimWorld::SimWorld(uint64_t)`, find the existing loop that assigns `ControllerIndex = i`. Replace it with:

```cpp
// Initialize each player's TeamId / RoleId / slot world position based on
// the 4-3-3 formation. Players 0..10 = home; players 11..21 = away.
for (int i = 0; i < kSimPlayerCount; ++i) {
    FSimPlayerState& p = State.Players[i];
    int teamId         = (i < 11) ? 0 : 1;
    int slotIndex      = i % 11;
    const FFormationSlot& slot = kFormation_4_3_3[slotIndex];

    p.TeamId           = (uint8_t)teamId;
    p.RoleId           = (uint8_t)slot.Role;
    p.Position         = SlotWorldPosition(slotIndex, teamId);
    p.Velocity         = FixedVec3::Zero();
    p.AITargetPosition = p.Position;
    p.ControllerIndex  = kStationaryController;  // vestigial; will be replaced in M9
    p.IntendedPassTarget = 0xFF;
    p.CurrentIntent    = 0;  // EIntent::HoldPosition (defined in M3)
    p.Flags            = 0;
}
```

Add includes at the top:

```cpp
#include "AI/Formations.h"
#include "AI/Roles.h"
```

- [ ] **Step 2: Write the failing test**

In `test_snapshot.cpp`, append:

```cpp
TEST_CASE(World_22PlayersAtSlots) {
    SimWorld w{1};
    const auto& s = w.GetState();
    int homeCount = 0, awayCount = 0;
    int gkCount = 0;
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId == 0) homeCount++;
        else if (p.TeamId == 1) awayCount++;
        if (p.RoleId == (uint8_t)ERole::GK) gkCount++;
    }
    TEST_EXPECT_EQ((int64_t)homeCount, (int64_t)11);
    TEST_EXPECT_EQ((int64_t)awayCount, (int64_t)11);
    TEST_EXPECT_EQ((int64_t)gkCount,   (int64_t)2);

    // GK home should be near -X, GK away near +X (sanity from T1.3)
    const auto& homeGK = s.Players[0];  // slot 0 is GK
    const auto& awayGK = s.Players[11]; // slot 0 of away team
    TEST_EXPECT_TRUE(homeGK.Position.X.Raw < 0);
    TEST_EXPECT_TRUE(awayGK.Position.X.Raw > 0);
    return 0;
}
```

Add `TEST_RUN(World_22PlayersAtSlots);` in `RunSnapshotTests`. Add `#include "AI/Roles.h"` to `test_snapshot.cpp`.

- [ ] **Step 3: Build + run, expect pass**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "World_22Players|Self-test"
```

Expected: `RUN  World_22PlayersAtSlots`, `Self-test OK`.

- [ ] **Step 4: Lint**

```bash
./Scripts/lint_sim.sh
```

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat(sim): SimWorld ctor places 22 players at 4-3-3 slots (home + away)"
```

### Task T1.6 — Update `StepPlayer` to handle stationary controller index for all 21 AI players

Right now `StepPlayer` returns early when `p.ControllerIndex == kStationaryController`. With 22 stationary players, the human's controller still drives Move via the existing path. We're using `kStationaryController = 0xFF` on all 22 right now, so nothing moves. That's correct for M1 — they're all stationary until M3 brings AI.

**Files:** none new. Just verify behavior.

- [ ] **Step 1: Add a smoke test that confirms 22 stationary players are stable across 100 ticks**

In `test_snapshot.cpp`, append:

```cpp
TEST_CASE(World_22StationaryPlayersStable) {
    SimWorld w{1};
    // Snapshot positions
    FixedVec3 initial[kSimPlayerCount];
    for (int i = 0; i < kSimPlayerCount; ++i)
        initial[i] = w.GetState().Players[i].Position;

    FInputFrame f{};
    for (int tick = 0; tick < 100; ++tick) {
        f.TickNumber = (uint32_t)tick;
        w.Step(f);
    }
    // Every player must still be at their slot (all stationary).
    for (int i = 0; i < kSimPlayerCount; ++i) {
        TEST_EXPECT_EQ(w.GetState().Players[i].Position.X.Raw, initial[i].Position.X.Raw);
        TEST_EXPECT_EQ(w.GetState().Players[i].Position.Y.Raw, initial[i].Position.Y.Raw);
    }
    return 0;
}
```

Wait — this references `initial[i].Position` but `initial` is already `FixedVec3`, not `FSimPlayerState`. Fix:

```cpp
TEST_CASE(World_22StationaryPlayersStable) {
    SimWorld w{1};
    FixedVec3 initial[kSimPlayerCount];
    for (int i = 0; i < kSimPlayerCount; ++i)
        initial[i] = w.GetState().Players[i].Position;

    FInputFrame f{};
    for (int tick = 0; tick < 100; ++tick) {
        f.TickNumber = (uint32_t)tick;
        w.Step(f);
    }
    for (int i = 0; i < kSimPlayerCount; ++i) {
        TEST_EXPECT_EQ(w.GetState().Players[i].Position.X.Raw, initial[i].X.Raw);
        TEST_EXPECT_EQ(w.GetState().Players[i].Position.Y.Raw, initial[i].Y.Raw);
    }
    return 0;
}
```

Add `TEST_RUN(World_22StationaryPlayersStable);`.

- [ ] **Step 2: Build + run, expect pass**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | tail -5
```

Expected: all tests including `World_22StationaryPlayersStable` pass.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "test(sim): 22 stationary players remain at slots across 100 ticks"
```

### Task T1.7 — Regenerate the determinism baselines for 22-player state

The three `.expected.hashes` files were generated for 2 players. With 22 players, the snapshot is bigger and hashes differ. Regenerate.

**Files:**
- Modify: `Source/Edge26SimStandalone/tests/replays/*.expected.hashes`

- [ ] **Step 1: Run the baseline updater**

```bash
./Scripts/update_determinism_baseline.sh
```

Expected:
```
wrote Source/Edge26SimStandalone/tests/replays/basic.expected.hashes
wrote Source/Edge26SimStandalone/tests/replays/ball_only.expected.hashes
wrote Source/Edge26SimStandalone/tests/replays/rollback_torture.expected.hashes
Baselines updated. REVIEW THE GIT DIFF before committing.
```

- [ ] **Step 2: Verify the determinism gate still passes**

```bash
./Scripts/check_determinism.sh 2>&1 | tail -5
```

Expected: `PASS: all determinism checks`.

- [ ] **Step 3: Spot-check the diff**

```bash
git diff --stat Source/Edge26SimStandalone/tests/replays/
```

Expected: all three `.expected.hashes` files modified, ~3500 lines changed.

- [ ] **Step 4: Commit**

```bash
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): regenerate determinism baselines for 22-player snapshot"
```

### Task T1.8 — Update `ResetForKickoff` in `ASoccerGameMode` to use formation slots

This is the UE5-side update. Player visuals were getting `TeleportTo(Starts[Idx]->GetActorLocation(), ...)` based on PlayerStart actors. Replace with formation-driven placement.

**Files:**
- Modify: `Source/Edge26/Private/Game/SoccerGameMode.cpp`
- Modify: `Source/Edge26/Public/Adapter/SimHostSubsystem.h` (new helper)
- Modify: `Source/Edge26/Private/Adapter/SimHostSubsystem.cpp` (new helper)

- [ ] **Step 1: Add a `ResetForKickoffSnapTo4_3_3()` helper on `SimHostSubsystem`**

In `Source/Edge26/Public/Adapter/SimHostSubsystem.h`, after the existing `ResetPlayer(...)` method:

```cpp
// M1: position all 22 players at 4-3-3 slots (called by GameMode on kickoff).
// Iterates the sim state and writes both sim state AND visual actor transforms.
void ResetAllPlayersTo4_3_3();
```

- [ ] **Step 2: Implement the helper**

In `Source/Edge26/Private/Adapter/SimHostSubsystem.cpp`, add this function (use the same `ToFixed64` helper):

```cpp
#include "AI/Formations.h"

void USimHostSubsystem::ResetAllPlayersTo4_3_3() {
    if (!Sim) return;
    auto& State = Sim->MutableState();
    for (int i = 0; i < edge26::kSimPlayerCount; ++i) {
        int teamId = (i < 11) ? 0 : 1;
        int slotIndex = i % 11;
        edge26::FixedVec3 slotPos = edge26::SlotWorldPosition(slotIndex, teamId);
        State.Players[i].Position = slotPos;
        State.Players[i].Velocity = edge26::FixedVec3::Zero();
        State.Players[i].TeamId = (uint8_t)teamId;
        State.Players[i].RoleId = (uint8_t)edge26::kFormation_4_3_3[slotIndex].Role;
    }
    Sim->Snapshot(CurrState);
    PrevState = CurrState;
}
```

- [ ] **Step 3: Update `ASoccerGameMode::ResetForKickoff`**

In `Source/Edge26/Private/Game/SoccerGameMode.cpp`, find `ResetForKickoff()`. Replace the body with:

```cpp
void ASoccerGameMode::ResetForKickoff() {
    USimHostSubsystem* Host = GetWorld() ? GetWorld()->GetSubsystem<USimHostSubsystem>() : nullptr;
    if (!Host) {
        UE_LOG(LogEdge26, Warning, TEXT("ResetForKickoff: SimHostSubsystem missing."));
        return;
    }
    Host->ResetBall(BallSpawnLocation);
    // M1: place all 22 players at 4-3-3 slots (replaces PlayerStart iteration).
    Host->ResetAllPlayersTo4_3_3();
}
```

- [ ] **Step 4: Build the editor**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -8
```

Expected: `Result: Succeeded`.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat(adapter): ResetAllPlayersTo4_3_3 — formation-driven kickoff placement"
```

### Task T1.9 — Place 22 `BP_Footballer` instances in `L_Pitch` via Python

The level currently has 2 placed footballers. We need 22. Doing it by hand is tedious; let's automate.

**Files:**
- Create: `Scripts/editor/spawn_22_players.py`

- [ ] **Step 1: Write the Python script**

```python
# spawn_22_players.py — ensures L_Pitch has exactly 22 BP_Footballer instances
# laid out at 4-3-3 slot positions (rough approximation; sim will overwrite
# exact positions at BeginPlay via ResetAllPlayersTo4_3_3).
#
# Run:
#   "/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
#       "$PWD/Edge26.uproject" -run=PythonScript \
#       -script="$PWD/Scripts/editor/spawn_22_players.py" -nopause -unattended -nullrhi

import unreal

LEVEL_PATH = "/Game/Levels/L_Pitch"
BP_FOOTBALLER = "/Game/Blueprints/Player/BP_Footballer"

# Approximate 4-3-3 slot positions in cm (X = up the pitch, Y = sideline).
# Pitch is 10500 cm × 6800 cm.
HOME_SLOTS = [
    (-4988,    0,  10),   # GK
    (-3413, -510,  10),   # CB
    (-3413,  510,  10),   # CB
    (-2888,-2210,  10),   # FB_L
    (-2888, 2210,  10),   # FB_R
    (-1313,    0,  10),   # CDM
    ( -525,-1020,  10),   # CM
    ( -525, 1020,  10),   # CM
    ( 2100,-2380,  10),   # W_L
    ( 2100, 2380,  10),   # W_R
    ( 2625,    0,  10),   # ST
]
AWAY_SLOTS = [(-x, y, z) for (x, y, z) in HOME_SLOTS]

def main():
    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_FOOTBALLER)
    if not bp_class:
        unreal.log_error(f"Could not load {BP_FOOTBALLER}")
        return

    world = unreal.EditorLevelLibrary.get_editor_world()

    # Remove any existing Footballer actors to avoid duplicates.
    all_actors = unreal.EditorLevelLibrary.get_all_level_actors()
    removed = 0
    for actor in all_actors:
        cls = actor.get_class()
        if cls and "Footballer" in cls.get_name():
            unreal.EditorLevelLibrary.destroy_actor(actor)
            removed += 1
    unreal.log(f"Removed {removed} existing Footballer-like actors")

    # Spawn 22 new ones.
    spawned = 0
    for slots, team_id in [(HOME_SLOTS, 0), (AWAY_SLOTS, 1)]:
        for (x, y, z) in slots:
            loc = unreal.Vector(float(x), float(y), float(z))
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(bp_class, loc)
            if actor:
                # AFootballerVisual has a ControllerIndex int32 UPROPERTY.
                # Distinguish slots via 0..21 (we'll use it as visual ID only —
                # actual sim TeamId/RoleId is derived from index inside the sim).
                actor.set_editor_property("ControllerIndex", spawned)
                spawned += 1
    unreal.log(f"Spawned {spawned} footballers")

    # Save the level.
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("Saved L_Pitch")

main()
```

- [ ] **Step 2: USER MANUAL — run the script via headless UE5**

> **Manual step (user):** run:
>
> ```bash
> "/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
>     "$PWD/Edge26.uproject" -run=PythonScript \
>     -script="$PWD/Scripts/editor/spawn_22_players.py" \
>     -nopause -unattended -nullrhi
> ```
>
> Expected log lines: `Removed N existing Footballer-like actors`, `Spawned 22 footballers`, `Saved L_Pitch`.

- [ ] **Step 3: Commit**

```bash
git add Scripts/editor/spawn_22_players.py Content/Levels/L_Pitch.umap
git commit -m "level: 22 BP_Footballer placements (4-3-3 home + away) via Python"
```

### Task T1.10 — Mark M1 complete

- [ ] **Step 1: Update PROGRESS.md**

In the roadmap, tick M1:

```markdown
- [x] M1. Roster expansion: 22 players, roles, formations, kickoff placement
```

Update the current status:

```markdown
We are at **Phase 2 M2 of M12** (spatial value model). M1 (roster expansion)
is complete: 22 players spawn at 4-3-3 slots, kickoff places them via the
sim, determinism baselines regenerated, lint + CI gates still green.
```

Append to activity log:

```markdown
- M1 landed: kSimPlayerCount 2→22, ERole enum, FFormationSlot + kFormation_4_3_3, FSimPlayerState 64→88 B, SimWorld ctor places 22, ResetAllPlayersTo4_3_3 in adapter, 22-player snapshot baselines regenerated, all unit tests pass.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): M1 complete; advance to M2 (spatial value model)"
```

---

## M2 — Spatial Value Model

The five-field grid that powers Layers A/B/C. Each field is a `Fixed32[1768]` array per team perspective; total ~70 KB embedded into `FSimWorldState`. Updates happen every tick at the top of `Step()`.

### Task T2.1 — Define `ESpatialField` + `FSpatialValueModel` struct

**Files:**
- Create: `Source/Edge26Sim/Public/AI/SpatialValueModel.h`

- [ ] **Step 1: Write `SpatialValueModel.h`**

```cpp
// Copyright Edge26. All Rights Reserved.
// Per-cell scalar value fields over the pitch. 5 fields × 2 team perspectives.
// Read by every AI layer.
#pragma once

#include <cstdint>
#include "Math/Fixed.h"
#include "Math/FixedVec.h"
#include "Sim/Constants.h"

namespace edge26 {

constexpr int kPitchCellsX = 52;
constexpr int kPitchCellsY = 34;
constexpr int kPitchCells  = kPitchCellsX * kPitchCellsY;  // 1768

// Cell-size derived from pitch dimensions. Stays integer-clean.
// PitchHalfLen=5250, PitchHalfWid=3400.
// Cell width = (PitchHalfLen * 2) / kPitchCellsX = 10500/52 ≈ 202 cm
// Cell height = (PitchHalfWid * 2) / kPitchCellsY = 6800/34 = 200 cm exactly
constexpr int64_t kCellSizeX_cm = 10500 / kPitchCellsX;  // ≈ 201
constexpr int64_t kCellSizeY_cm = 6800  / kPitchCellsY;  // = 200

enum class ESpatialField : uint8_t {
    Space         = 0,   // openness; high = far from opponents
    DefCoverage   = 1,   // how well-defended; high = poorly defended (gap for opp)
    LaneOccupancy = 2,   // 0 = blocked, 1 = clear (team-independent; stored at [0])
    PassReception = 3,   // composite — "if a teammate were here, how good a pass target?"
    Threat        = 4,   // xG-like surface — value of being in possession here
    Count         = 5
};

struct FSpatialValueModel {
    // [team][field][cell]. Team is the ATTACKING team's perspective.
    Fixed32 Cells[2][(int)ESpatialField::Count][kPitchCells];
};
static_assert(sizeof(FSpatialValueModel) == 2 * 5 * kPitchCells * 4,
              "FSpatialValueModel size locked to 70720 B");

// World position → cell index. Out-of-bounds clamps to nearest valid cell.
inline int CellIndex(FixedVec3 worldPos) {
    // Map worldX in [-PitchHalfLen, +PitchHalfLen] → [0, kPitchCellsX-1].
    int64_t shiftedX = worldPos.X.ToInt() + SimConst::PitchHalfLen.ToInt();
    int64_t shiftedY = worldPos.Y.ToInt() + SimConst::PitchHalfWid.ToInt();
    int cellX = (int)(shiftedX / kCellSizeX_cm);
    int cellY = (int)(shiftedY / kCellSizeY_cm);
    if (cellX < 0) cellX = 0;
    if (cellX >= kPitchCellsX) cellX = kPitchCellsX - 1;
    if (cellY < 0) cellY = 0;
    if (cellY >= kPitchCellsY) cellY = kPitchCellsY - 1;
    return cellY * kPitchCellsX + cellX;
}

// Cell index → center world position.
inline FixedVec3 CellCenter(int cellIdx) {
    int cellX = cellIdx % kPitchCellsX;
    int cellY = cellIdx / kPitchCellsX;
    int64_t worldX_cm = (int64_t)cellX * kCellSizeX_cm - SimConst::PitchHalfLen.ToInt() + kCellSizeX_cm / 2;
    int64_t worldY_cm = (int64_t)cellY * kCellSizeY_cm - SimConst::PitchHalfWid.ToInt() + kCellSizeY_cm / 2;
    return FixedVec3{
        Fixed64::FromInt(worldX_cm),
        Fixed64::FromInt(worldY_cm),
        Fixed64::FromInt(0)
    };
}

// Update entry point — called once per sim tick from SimWorld::Step.
struct FSimWorldState;  // forward
void UpdateSpatialFields(FSimWorldState& state);

}  // namespace edge26
```

- [ ] **Step 2: Write the failing unit tests**

In `test_snapshot.cpp`, append (and add `#include "AI/SpatialValueModel.h"`):

```cpp
TEST_CASE(SpatialModel_CellIndexRoundtrip) {
    using namespace edge26;
    // Center cell of the pitch should map to a cell around (kPitchCellsX/2, kPitchCellsY/2).
    int center = CellIndex(FixedVec3::Zero());
    FixedVec3 back = CellCenter(center);
    // Should be near origin (within one cell radius)
    TEST_EXPECT_TRUE(SimMath::AbsRaw(back.X.Raw) < (kCellSizeX_cm * (int64_t)Fixed64::One));
    TEST_EXPECT_TRUE(SimMath::AbsRaw(back.Y.Raw) < (kCellSizeY_cm * (int64_t)Fixed64::One));
    return 0;
}

TEST_CASE(SpatialModel_CellIndexClampsOutOfBounds) {
    using namespace edge26;
    // Way off-pitch positions clamp to corner cells; no crash.
    int corner1 = CellIndex(FixedVec3{
        Fixed64::FromInt(-99999), Fixed64::FromInt(-99999), Fixed64::FromInt(0)
    });
    int corner2 = CellIndex(FixedVec3{
        Fixed64::FromInt( 99999), Fixed64::FromInt( 99999), Fixed64::FromInt(0)
    });
    TEST_EXPECT_EQ((int64_t)corner1, (int64_t)0);
    TEST_EXPECT_EQ((int64_t)corner2, (int64_t)(kPitchCells - 1));
    return 0;
}
```

Note: this test references `SimMath::AbsRaw` which doesn't exist. Use the existing `Abs(Fixed64)` instead:

Replace the assertion lines with:

```cpp
    TEST_EXPECT_TRUE(Abs(back.X).Raw < (Fixed64::FromInt(kCellSizeX_cm)).Raw);
    TEST_EXPECT_TRUE(Abs(back.Y).Raw < (Fixed64::FromInt(kCellSizeY_cm)).Raw);
```

Add `TEST_RUN(SpatialModel_CellIndexRoundtrip);` and `TEST_RUN(SpatialModel_CellIndexClampsOutOfBounds);`.

- [ ] **Step 3: Build + run**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "SpatialModel|Self-test"
```

Expected: both tests pass.

- [ ] **Step 4: Commit**

```bash
git add Source/Edge26Sim/Public/AI/SpatialValueModel.h Source/Edge26SimStandalone/tests/test_snapshot.cpp
git commit -m "feat(ai): FSpatialValueModel struct (70 KB, 5 fields × 1768 cells × 2 teams) + cell helpers"
```

### Task T2.2 — Implement `UpdateSpaceField`

Per spec §10: per cell, find min distance to any opponent. Clamp to [0, 1] openness scalar.

**Files:**
- Create: `Source/Edge26Sim/Private/AI/SpatialValueModel.cpp`

- [ ] **Step 1: Write `SpatialValueModel.cpp` with `UpdateSpaceField` only**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "AI/SpatialValueModel.h"
#include "Sim/WorldState.h"
#include "Math/Sqrt.h"

namespace edge26 {

// Map raw distance² (in cm²) → openness scalar in [0, 1].
// 0 distance → 0 openness (right next to opponent).
// 500 cm (5m) → ~1 openness (very open).
// Implementation: sqrt(distSq), then clamp/scale.
static Fixed32 DistanceToOpenness(Fixed64 distSq) {
    Fixed64 dist = SimMath::Sqrt(distSq);
    // Saturate at 500 cm: openness = min(dist, 500) / 500.
    constexpr int64_t kSaturationCm = 500;
    Fixed64 saturated = (dist.Raw >= Fixed64::FromInt(kSaturationCm).Raw)
        ? Fixed64::FromInt(kSaturationCm)
        : dist;
    // Convert Fixed64 → Fixed32 by ratio: openness = saturated.Raw / 500 → [0..One]
    int32_t openness = (int32_t)((saturated.Raw * (int64_t)Fixed32::One)
                              / Fixed64::FromInt(kSaturationCm).Raw);
    return Fixed32::FromRaw(openness);
}

void UpdateSpaceField(FSimWorldState& s, int teamId) {
    auto& field = s.Spatial.Cells[teamId][(int)ESpatialField::Space];
    for (int c = 0; c < kPitchCells; ++c) {
        FixedVec3 cellPos = CellCenter(c);
        Fixed64 minDistSq = Fixed64::FromInt(99999999);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const FSimPlayerState& opp = s.Players[i];
            if (opp.TeamId == (uint8_t)teamId) continue;       // opponent only
            FixedVec3 d = opp.Position - cellPos;
            Fixed64 distSq = d.X * d.X + d.Y * d.Y;
            if (distSq.Raw < minDistSq.Raw) minDistSq = distSq;
        }
        field[c] = DistanceToOpenness(minDistSq);
    }
}

}  // namespace edge26
```

- [ ] **Step 2: Write the failing test**

In `test_snapshot.cpp`, append:

```cpp
TEST_CASE(SpatialModel_SpaceFieldEmptyPitchIsFullyOpen) {
    using namespace edge26;
    SimWorld w{1};
    // Remove all opponents (team 1) by moving them off-pitch far away.
    auto& state = w.MutableState();
    for (int i = 11; i < kSimPlayerCount; ++i) {
        state.Players[i].Position = FixedVec3{
            Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0)
        };
    }
    UpdateSpaceField(state, 0);  // home team perspective
    // Every cell should be max openness (no nearby opponents).
    for (int c = 0; c < kPitchCells; ++c) {
        Fixed32 v = state.Spatial.Cells[0][(int)ESpatialField::Space][c];
        // Allow ±2 ulps tolerance for sqrt rounding
        TEST_EXPECT_TRUE(v.Raw >= (Fixed32::One - 2));
    }
    return 0;
}

TEST_CASE(SpatialModel_SpaceFieldZeroAtOpponent) {
    using namespace edge26;
    SimWorld w{1};
    auto& state = w.MutableState();
    // Place opponent (player 11, away team) at origin.
    state.Players[11].Position = FixedVec3::Zero();
    UpdateSpaceField(state, 0);
    int origin = CellIndex(FixedVec3::Zero());
    Fixed32 v = state.Spatial.Cells[0][(int)ESpatialField::Space][origin];
    // Cell at opponent's position should have very low openness.
    TEST_EXPECT_TRUE(v.Raw < (Fixed32::One / 5));  // less than 0.2
    return 0;
}
```

Forward-declare `UpdateSpaceField` at the top of the test file:

```cpp
namespace edge26 {
    void UpdateSpaceField(FSimWorldState& s, int teamId);
}
```

Add `TEST_RUN(SpatialModel_SpaceFieldEmptyPitchIsFullyOpen);` and `TEST_RUN(SpatialModel_SpaceFieldZeroAtOpponent);`.

- [ ] **Step 3: Reconfigure CMake (new source file) + build + run**

```bash
cmake -S Source/Edge26SimStandalone -B build/sim -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -2
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "SpaceField|Self-test"
```

Expected: both tests pass.

- [ ] **Step 4: Lint**

```bash
./Scripts/lint_sim.sh
```

Expected: `OK`.

- [ ] **Step 5: Commit**

```bash
git add Source/Edge26Sim/Private/AI/SpatialValueModel.cpp Source/Edge26SimStandalone/tests/test_snapshot.cpp
git commit -m "feat(ai): UpdateSpaceField — per-cell openness via min-distance-to-nearest-opponent"
```

### Task T2.3 — Implement `UpdateDefCoverageField`

Symmetric to Space, but distance to nearest TEAMMATE (used by the opposing team's offence to find gaps).

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/SpatialValueModel.cpp`

- [ ] **Step 1: Write the failing test**

In `test_snapshot.cpp`, append:

```cpp
TEST_CASE(SpatialModel_DefCoverageHighWhereTeammatesScarce) {
    using namespace edge26;
    SimWorld w{1};
    auto& state = w.MutableState();
    // Move all home players (team 0) to one corner, far from everything else.
    for (int i = 0; i < 11; ++i) {
        state.Players[i].Position = FixedVec3{
            Fixed64::FromInt(-5000), Fixed64::FromInt(-3000), Fixed64::FromInt(0)
        };
    }
    UpdateDefCoverageField(state, 0);  // home team's coverage from their perspective
    // A cell at the OPPOSITE corner has very poor coverage → field value should be high.
    int oppCorner = CellIndex(FixedVec3{
        Fixed64::FromInt(5000), Fixed64::FromInt(3000), Fixed64::FromInt(0)
    });
    Fixed32 v = state.Spatial.Cells[0][(int)ESpatialField::DefCoverage][oppCorner];
    TEST_EXPECT_TRUE(v.Raw > (Fixed32::One * 3 / 4));  // > 0.75
    return 0;
}
```

Forward-declare `UpdateDefCoverageField`. Add `TEST_RUN`.

- [ ] **Step 2: Implement**

Append to `SpatialValueModel.cpp`:

```cpp
void UpdateDefCoverageField(FSimWorldState& s, int teamId) {
    auto& field = s.Spatial.Cells[teamId][(int)ESpatialField::DefCoverage];
    for (int c = 0; c < kPitchCells; ++c) {
        FixedVec3 cellPos = CellCenter(c);
        Fixed64 minDistSq = Fixed64::FromInt(99999999);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const FSimPlayerState& mate = s.Players[i];
            if (mate.TeamId != (uint8_t)teamId) continue;     // teammates only
            FixedVec3 d = mate.Position - cellPos;
            Fixed64 distSq = d.X * d.X + d.Y * d.Y;
            if (distSq.Raw < minDistSq.Raw) minDistSq = distSq;
        }
        // High value = poorly covered (far from any teammate).
        field[c] = DistanceToOpenness(minDistSq);
    }
}
```

- [ ] **Step 3: Build + run, expect pass**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "DefCoverage|Self-test"
```

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(ai): UpdateDefCoverageField — per-cell distance to nearest teammate"
```

### Task T2.4 — Implement `UpdateLaneOccupancyField`

Per spec §10: for each cell, sample 5 points along (ball-origin → cell). If any sample is within 80 cm of an opponent, the lane is blocked → 0. Else clear → 1.

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/SpatialValueModel.cpp`

- [ ] **Step 1: Write the failing test**

In `test_snapshot.cpp`, append:

```cpp
TEST_CASE(SpatialModel_LaneOccupancyEmptyPitchAllClear) {
    using namespace edge26;
    SimWorld w{1};
    auto& state = w.MutableState();
    // Move every player far off pitch so no one blocks lanes.
    for (int i = 0; i < kSimPlayerCount; ++i) {
        state.Players[i].Position = FixedVec3{
            Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0)
        };
    }
    state.Ball.Position = FixedVec3::Zero();
    state.Match.PossessionPlayer = 0xFF;  // loose ball; lane origin = ball
    UpdateLaneOccupancyField(state);
    // Every cell should be fully clear (lane = 1).
    for (int c = 0; c < kPitchCells; ++c) {
        Fixed32 v = state.Spatial.Cells[0][(int)ESpatialField::LaneOccupancy][c];
        TEST_EXPECT_EQ(v.Raw, Fixed32::One);
    }
    return 0;
}
```

This test will fail to compile because `FMatchState` (referenced as `state.Match`) doesn't exist yet. We'll add it in T2.6. **For now, stub the test by writing it but commenting out the body until T2.6 lands.**

Actually, cleaner: defer the `state.Match.*` line — use `FixedVec3::Zero()` as the origin for lane occupancy unconditionally for v0 first, then refine.

Update `UpdateLaneOccupancyField` to use ball position as the lane origin (no possession-aware logic yet — that comes in T5/M5):

- [ ] **Step 2: Implement**

Append to `SpatialValueModel.cpp`:

```cpp
constexpr Fixed64 kLaneBlockRadius = Fixed64::FromInt(80);   // 80 cm

void UpdateLaneOccupancyField(FSimWorldState& s) {
    FixedVec3 origin = s.Ball.Position;
    auto& fieldTeam0 = s.Spatial.Cells[0][(int)ESpatialField::LaneOccupancy];
    auto& fieldTeam1 = s.Spatial.Cells[1][(int)ESpatialField::LaneOccupancy];

    for (int c = 0; c < kPitchCells; ++c) {
        FixedVec3 cellPos = CellCenter(c);
        bool blocked = false;
        // Sample 5 points at t = 1/6, 2/6, ..., 5/6 along origin→cell.
        for (int sample = 1; sample <= 5 && !blocked; ++sample) {
            Fixed64 t = Fixed64::FromInt(sample) / Fixed64::FromInt(6);
            FixedVec3 p = origin + (cellPos - origin) * t;
            for (int i = 0; i < kSimPlayerCount; ++i) {
                const auto& opp = s.Players[i];
                FixedVec3 d = opp.Position - p;
                Fixed64 distSq = d.X * d.X + d.Y * d.Y;
                if (distSq.Raw < (kLaneBlockRadius * kLaneBlockRadius).Raw) {
                    blocked = true;
                    break;
                }
            }
        }
        Fixed32 v = blocked ? Fixed32::FromRaw(0) : Fixed32::FromRaw(Fixed32::One);
        fieldTeam0[c] = v;
        fieldTeam1[c] = v;
    }
}
```

Remove the `state.Match.PossessionPlayer = 0xFF` line from the test (no `FMatchState` yet).

Forward-declare `UpdateLaneOccupancyField`. Add `TEST_RUN`.

- [ ] **Step 3: Build + run**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "LaneOccupancy|Self-test"
```

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(ai): UpdateLaneOccupancyField — sample-5-points raycast surrogate"
```

### Task T2.5 — Implement `UpdateThreatField`

Per spec §10: static xG-like surface, hot at top-of-box, half-spaces, six-yard line. v0 ships a hardcoded static table; "dynamic adjust for occupation" is deferred.

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/SpatialValueModel.cpp`

- [ ] **Step 1: Implement (no tests — pure data lookup)**

Append to `SpatialValueModel.cpp`:

```cpp
// Static xG-like threat by cell, per attacking team.
// Encoded as a function of normalized X (-1 own goal, +1 opp goal) and |Y|.
// Higher = more dangerous to attack TO this cell.
static Fixed32 StaticThreatAt(FixedVec3 cellPos, int attackingTeam) {
    // Attacking direction: home (team 0) attacks +X; away (team 1) attacks -X.
    int64_t signedX_cm = (attackingTeam == 0) ? cellPos.X.ToInt() : -cellPos.X.ToInt();
    int64_t absY_cm    = (cellPos.Y.Raw >= 0) ? cellPos.Y.ToInt() : -cellPos.Y.ToInt();

    // 6-yard box (550 cm from opp goal, ±915 cm wide) ≈ huge threat
    if (signedX_cm > 4400 && absY_cm < 1830) return Fixed32::FromRaw((int32_t)((double)Fixed32::One * 0.95));
    // 18-yard box (1650 cm, ±2000 cm)
    if (signedX_cm > 3600 && absY_cm < 2000) return Fixed32::FromRaw((int32_t)((double)Fixed32::One * 0.65));
    // Top of D (just outside box, central)
    if (signedX_cm > 3000 && absY_cm < 1500) return Fixed32::FromRaw((int32_t)((double)Fixed32::One * 0.40));
    // Half-spaces (wide of box)
    if (signedX_cm > 3200 && absY_cm < 2800) return Fixed32::FromRaw((int32_t)((double)Fixed32::One * 0.25));
    // Attacking third
    if (signedX_cm > 1750)                   return Fixed32::FromRaw((int32_t)((double)Fixed32::One * 0.10));
    return Fixed32::FromRaw(0);
}

void UpdateThreatField(FSimWorldState& s, int teamId) {
    auto& field = s.Spatial.Cells[teamId][(int)ESpatialField::Threat];
    for (int c = 0; c < kPitchCells; ++c) {
        FixedVec3 cellPos = CellCenter(c);
        field[c] = StaticThreatAt(cellPos, teamId);
    }
}
```

- [ ] **Step 2: Write the test**

In `test_snapshot.cpp`, append:

```cpp
TEST_CASE(SpatialModel_ThreatHighInOppBox) {
    using namespace edge26;
    SimWorld w{1};
    auto& state = w.MutableState();
    UpdateThreatField(state, 0);  // home attacks +X
    // Home's threat at +5000, 0 (deep in opp box) should be near max.
    int boxCell = CellIndex(FixedVec3{
        Fixed64::FromInt(5000), Fixed64::FromInt(0), Fixed64::FromInt(0)
    });
    Fixed32 v = state.Spatial.Cells[0][(int)ESpatialField::Threat][boxCell];
    TEST_EXPECT_TRUE(v.Raw > (Fixed32::One * 4 / 5));  // > 0.8
    // Home's threat at -5000, 0 (own box) should be ~zero.
    int ownBox = CellIndex(FixedVec3{
        Fixed64::FromInt(-5000), Fixed64::FromInt(0), Fixed64::FromInt(0)
    });
    Fixed32 v2 = state.Spatial.Cells[0][(int)ESpatialField::Threat][ownBox];
    TEST_EXPECT_EQ(v2.Raw, (int32_t)0);
    return 0;
}
```

Forward-declare `UpdateThreatField`. Add `TEST_RUN`.

- [ ] **Step 3: Build + run**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "Threat|Self-test"
```

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(ai): UpdateThreatField — static xG-like surface (hot in opp box, half-spaces)"
```

### Task T2.6 — Add `FMatchState` (needed by PassReception lane logic) + `UpdatePassReceptionField`

PassReception is the composite that reads Space + Lane + Threat + ForwardBonus. The ForwardBonus needs to know "which direction is forward for my team", which depends on team. Also FMatchState gets introduced now because Layer A/B (later milestones) need it; embedding it incrementally as we go.

**Files:**
- Create: `Source/Edge26Sim/Public/Sim/MatchState.h`
- Modify: `Source/Edge26Sim/Public/Sim/WorldState.h`
- Modify: `Source/Edge26Sim/Private/AI/SpatialValueModel.cpp`
- Modify: `Source/Edge26Sim/Private/Sim/SimWorld.cpp`

- [ ] **Step 1: Write `MatchState.h`** (full struct per spec §4)

```cpp
// Copyright Edge26. All Rights Reserved.
#pragma once

#include <cstdint>
#include "Math/Fixed.h"

namespace edge26 {

struct FTeamPlan {
    int8_t   Mentality;            // -2..+2
    int8_t   LineHeightBias;       // -1..+1
    uint8_t  PressIntensity;       // 0..3
    uint8_t  Tempo;                // 0..3
    uint8_t  BuildupStyle;         // 0..2
    uint8_t  CounterAttackBias;    // 0..3
    uint8_t  _pad0[2];
    Fixed32  PanicBias;
    Fixed32  HoldBias;
    Fixed32  MentalityShootBias;
    Fixed32  _pad1;
};
static_assert(sizeof(FTeamPlan) == 24);

struct FUnitState {
    Fixed64  LineY;
    Fixed32  Compactness;
    uint8_t  PressTrigger;
    uint8_t  PressTargetIdx;
    uint8_t  OverlapTriggerIdx;
    uint8_t  _pad;
};
static_assert(sizeof(FUnitState) == 16);

struct FMatchState {
    uint8_t   HumanControlledIndex;
    uint8_t   PossessionTeam;
    uint8_t   PossessionPlayer;
    uint8_t   KickoffTeam;
    uint8_t   PendingOffsideCallTeam;
    uint8_t   _pad0[3];
    uint32_t  PendingOffsideCallTick;
    uint32_t  _pad1;
    Fixed64   OffsideLineY[2];
    FTeamPlan Plans[2];
    FUnitState Units[2][3];
    uint16_t  Score[2];
    uint8_t   _pad2[4];
};
static_assert(sizeof(FMatchState) == 184);
static_assert(alignof(FMatchState) == 8);

}  // namespace edge26
```

- [ ] **Step 2: Embed `FMatchState` into `FSimWorldState`** (just FMatchState; FSpatialValueModel embed comes in T2.8)

In `WorldState.h`, modify:

```cpp
#include "Sim/MatchState.h"

struct FSimWorldState {
    uint32_t        TickNumber;
    uint32_t        _pad0;
    uint64_t        RngState;
    FSimBallState   Ball;
    FSimPlayerState Players[kSimPlayerCount];
    FMatchState     Match;                   // NEW (M2 T2.6)
};
```

- [ ] **Step 3: Zero-init Match in `SimWorld::SimWorld(uint64)`**

In `SimWorld.cpp`, the existing `std::memset(&State, 0, sizeof(State))` already zeros it. Verify the constructor still memsets the whole struct.

- [ ] **Step 4: Implement `UpdatePassReceptionField`**

Append to `SpatialValueModel.cpp`:

```cpp
void UpdatePassReceptionField(FSimWorldState& s, int teamId) {
    auto& field      = s.Spatial.Cells[teamId][(int)ESpatialField::PassReception];
    const auto& spaceField  = s.Spatial.Cells[teamId][(int)ESpatialField::Space];
    const auto& laneField   = s.Spatial.Cells[teamId][(int)ESpatialField::LaneOccupancy];
    const auto& threatField = s.Spatial.Cells[teamId][(int)ESpatialField::Threat];

    // "Forward" sign: home attacks +X (sign=+1); away attacks -X (sign=-1).
    Fixed64 forwardSign = (teamId == 0) ? Fixed64::FromInt(1) : Fixed64::FromInt(-1);
    Fixed64 ballX       = s.Ball.Position.X;

    for (int c = 0; c < kPitchCells; ++c) {
        Fixed32 space   = spaceField[c];
        Fixed32 lane    = laneField[c];
        Fixed32 threat  = threatField[c];

        // Forward bonus: cells ahead of the ball score higher.
        Fixed64 cellX = CellCenter(c).X;
        Fixed64 forwardDelta = (cellX - ballX) * forwardSign;
        // Normalize forwardDelta over half a pitch:
        Fixed32 forwardBonus = (forwardDelta.Raw > 0)
            ? Fixed32::FromRaw((int32_t)((forwardDelta.Raw * (int64_t)Fixed32::One)
                                        / SimConst::PitchHalfLen.Raw))
            : Fixed32::FromRaw(0);
        if (forwardBonus.Raw > Fixed32::One) forwardBonus = Fixed32::FromRaw(Fixed32::One);

        // Composite: space × lane × (1 + forward) × (1 + threat)
        Fixed32 oneF32 = Fixed32::FromRaw(Fixed32::One);
        Fixed32 value = space * lane * (oneF32 + forwardBonus) * (oneF32 + threat);
        field[c] = value;
    }
}
```

- [ ] **Step 5: Write the test**

In `test_snapshot.cpp`, append:

```cpp
TEST_CASE(SpatialModel_PassReceptionForwardOfBall) {
    using namespace edge26;
    SimWorld w{1};
    auto& state = w.MutableState();
    state.Ball.Position = FixedVec3::Zero();
    // Move opponents far away so Space + Lane are clear.
    for (int i = 11; i < kSimPlayerCount; ++i) {
        state.Players[i].Position = FixedVec3{
            Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0)
        };
    }
    UpdateSpaceField(state, 0);
    UpdateLaneOccupancyField(state);
    UpdateThreatField(state, 0);
    UpdatePassReceptionField(state, 0);

    // Cell at +3000 X (ahead of ball for home) should score higher than -3000.
    int aheadCell  = CellIndex(FixedVec3{Fixed64::FromInt(3000), Fixed64::FromInt(0), Fixed64::FromInt(0)});
    int behindCell = CellIndex(FixedVec3{Fixed64::FromInt(-3000), Fixed64::FromInt(0), Fixed64::FromInt(0)});
    Fixed32 ahead  = state.Spatial.Cells[0][(int)ESpatialField::PassReception][aheadCell];
    Fixed32 behind = state.Spatial.Cells[0][(int)ESpatialField::PassReception][behindCell];
    TEST_EXPECT_TRUE(ahead.Raw > behind.Raw);
    return 0;
}
```

Forward-declare `UpdatePassReceptionField`. Add `TEST_RUN`.

- [ ] **Step 6: Build + run**

```bash
cmake -S Source/Edge26SimStandalone -B build/sim -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -2
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "PassReception|Self-test"
```

Expected: pass.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat(ai): FMatchState struct + UpdatePassReceptionField composite (space×lane×forward×threat)"
```

### Task T2.7 — Embed `FSpatialValueModel` into `FSimWorldState` + `UpdateSpatialFields` entry point

**Files:**
- Modify: `Source/Edge26Sim/Public/Sim/WorldState.h`
- Modify: `Source/Edge26Sim/Private/AI/SpatialValueModel.cpp`

- [ ] **Step 1: Embed the model in WorldState**

In `WorldState.h`:

```cpp
#include "AI/SpatialValueModel.h"

struct FSimWorldState {
    uint32_t            TickNumber;
    uint32_t            _pad0;
    uint64_t            RngState;
    FSimBallState       Ball;
    FSimPlayerState     Players[kSimPlayerCount];
    FMatchState         Match;
    FSpatialValueModel  Spatial;        // NEW (M2 T2.7)
};
static_assert(alignof(FSimWorldState) == 8);
```

- [ ] **Step 2: Implement `UpdateSpatialFields` (the orchestrator)**

Append to `SpatialValueModel.cpp`:

```cpp
void UpdateSpatialFields(FSimWorldState& s) {
    for (int t = 0; t < 2; ++t) UpdateSpaceField(s, t);
    for (int t = 0; t < 2; ++t) UpdateDefCoverageField(s, t);
    UpdateLaneOccupancyField(s);
    for (int t = 0; t < 2; ++t) UpdateThreatField(s, t);
    for (int t = 0; t < 2; ++t) UpdatePassReceptionField(s, t);
}
```

Order matters: PassReception reads Space/Lane/Threat → those run first.

- [ ] **Step 3: Wire into `SimWorld::Step` at the top of the tick**

In `SimWorld.cpp` `Step()`, after `State.TickNumber = frame.TickNumber;` and before the player update loop, add:

```cpp
UpdateSpatialFields(State);
```

Add `#include "AI/SpatialValueModel.h"` to `SimWorld.cpp`.

- [ ] **Step 4: Write the test**

In `test_snapshot.cpp`, append:

```cpp
TEST_CASE(SpatialModel_StepUpdatesFields) {
    using namespace edge26;
    SimWorld w{1};
    FInputFrame f{};
    f.TickNumber = 1;
    w.Step(f);   // should call UpdateSpatialFields
    // After one tick, the threat field should be non-trivial (hot in opp box).
    int boxCell = CellIndex(FixedVec3{
        Fixed64::FromInt(5000), Fixed64::FromInt(0), Fixed64::FromInt(0)
    });
    Fixed32 v = w.GetState().Spatial.Cells[0][(int)ESpatialField::Threat][boxCell];
    TEST_EXPECT_TRUE(v.Raw > (Fixed32::One * 4 / 5));
    return 0;
}
```

Add `TEST_RUN(SpatialModel_StepUpdatesFields);`.

- [ ] **Step 5: Build + run**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "StepUpdatesFields|Self-test"
```

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat(ai): embed FSpatialValueModel in WorldState; UpdateSpatialFields wired into SimWorld::Step"
```

### Task T2.8 — Regenerate baselines for the larger snapshot (~73 KB)

The snapshot now includes FMatchState + FSpatialValueModel. Hashes change radically. Regenerate.

- [ ] **Step 1: Run the updater**

```bash
./Scripts/update_determinism_baseline.sh
```

- [ ] **Step 2: Verify the gate passes**

```bash
./Scripts/check_determinism.sh 2>&1 | tail -3
```

Expected: `PASS: all determinism checks`.

- [ ] **Step 3: Commit**

```bash
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): regenerate baselines after embedding FMatchState + FSpatialValueModel"
```

### Task T2.9 — Mark M2 complete

- [ ] **Step 1: Update PROGRESS.md**

Tick M2. Update status: "**Phase 2 M3 of M12** (Layer C off-ball intents)". Add activity log entry:

```markdown
- M2 landed: 5-field spatial value model (Space, DefCoverage, LaneOccupancy, PassReception, Threat) updated each tick. Snapshot grew to ~73 KB. Determinism baselines regenerated. All field-level unit tests green.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): M2 complete; advance to M3 (Layer C off-ball intents)"
```

---

## M3 — Layer C off-ball intents

Each of the 21 AI players (all but the human) decides every tick what to do when their team doesn't have the ball or they're not the carrier. Decisions are scored from spatial fields + role weights.

### Task T3.1 — Define `EIntent` enum + `FRoleWeights`

**Files:**
- Create: `Source/Edge26Sim/Public/AI/Intents.h`
- Modify: `Source/Edge26Sim/Public/AI/Roles.h`

- [ ] **Step 1: Write `Intents.h`**

```cpp
// Copyright Edge26. All Rights Reserved.
// All possible decisions a player can make in a tick.
#pragma once

#include <cstdint>

namespace edge26 {

enum class EIntent : uint8_t {
    // Off-ball
    HoldPosition      = 0,
    MakeRunForward    = 1,
    DropToReceive     = 2,
    ProvideWidth      = 3,
    Press             = 4,
    TrackRunner       = 5,
    HoldDefensiveLine = 6,
    // On-ball (filled in M4)
    Pass              = 7,
    Shoot             = 8,
    Dribble           = 9,
    Hold              = 10,
    Clear             = 11,
    Count             = 12
};

}  // namespace edge26
```

- [ ] **Step 2: Add `FRoleWeights` to `Roles.h`**

Append to `Source/Edge26Sim/Public/AI/Roles.h`:

```cpp
#include "Math/Fixed.h"

namespace edge26 {

struct FRoleWeights {
    // Off-ball multipliers (all in [0..2] roughly)
    Fixed32  MakeRunForward;
    Fixed32  HoldPosition;
    Fixed32  DropToReceive;
    Fixed32  ProvideWidth;
    Fixed32  Press;
    Fixed32  TrackRunner;
    Fixed32  HoldDefensiveLine;
    // On-ball multipliers
    Fixed32  PreferPass;
    Fixed32  PreferShoot;
    Fixed32  PreferDribble;
    Fixed32  PreferLongBall;
};

// Hardcoded role-weight table indexed by ERole. Defined in Roles.cpp.
extern const FRoleWeights kRoleWeightsTable[(int)ERole::Count];

}  // namespace edge26
```

- [ ] **Step 3: Implement the table in a new `Roles.cpp`**

Create `Source/Edge26Sim/Private/AI/Roles.cpp`:

```cpp
// Copyright Edge26. All Rights Reserved.
#include "AI/Roles.h"

namespace edge26 {

// Helper: float-literal to Fixed32 at compile-eval time.
static constexpr Fixed32 F32(double v) {
    return Fixed32::FromRaw((int32_t)(v * (double)Fixed32::One));
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
```

These values are first-pass; M12 (tuning) revisits them after observing AI behavior.

- [ ] **Step 4: Build + lint + commit**

```bash
cmake -S Source/Edge26SimStandalone -B build/sim -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -2
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Public/AI/Intents.h Source/Edge26Sim/Public/AI/Roles.h \
        Source/Edge26Sim/Private/AI/Roles.cpp
git commit -m "feat(ai): EIntent enum + FRoleWeights + kRoleWeightsTable[10 roles]"
```

### Task T3.2 — `PlayerDecisions.h` interface + `SlotIndexForPlayer` helper

**Files:**
- Create: `Source/Edge26Sim/Public/AI/PlayerDecisions.h`
- Create: `Source/Edge26Sim/Private/AI/PlayerDecisions.cpp`

- [ ] **Step 1: Write the header**

```cpp
// Copyright Edge26. All Rights Reserved.
#pragma once

namespace edge26 {

struct FSimWorldState;
struct FSimPlayerState;

// Layer C entry point. Called per AI player per tick. Writes p.CurrentIntent,
// p.FacingTarget, p.AITargetPosition. Does NOT move the player — that's
// StepPlayer's job (it reads FacingTarget and integrates velocity toward it).
void UpdatePlayerAI(FSimPlayerState& p, const FSimWorldState& s, int playerIdx);

// Helper: which formation slot does this player occupy? Used to anchor HoldPosition.
int  SlotIndexForPlayer(int playerIdx);    // 0..10 (mod 11)

}  // namespace edge26
```

- [ ] **Step 2: Write minimal `PlayerDecisions.cpp` with just `SlotIndexForPlayer`**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "AI/PlayerDecisions.h"
#include "AI/Formations.h"
#include "AI/Intents.h"
#include "AI/Roles.h"
#include "AI/SpatialValueModel.h"
#include "Sim/WorldState.h"
#include "Math/Atan2.h"

namespace edge26 {

int SlotIndexForPlayer(int playerIdx) { return playerIdx % 11; }

void UpdatePlayerAI(FSimPlayerState& p, const FSimWorldState& s, int playerIdx) {
    // Stub for now — fleshed out in T3.3 onwards.
    // Default: stand at slot, face ball.
    int slotIdx = SlotIndexForPlayer(playerIdx);
    p.AITargetPosition = SlotWorldPosition(slotIdx, p.TeamId);
    p.CurrentIntent = (uint8_t)EIntent::HoldPosition;
    p.FacingTarget = SimMath::Atan2(
        s.Ball.Position.Y - p.Position.Y,
        s.Ball.Position.X - p.Position.X);
}

}  // namespace edge26
```

- [ ] **Step 3: Build + commit**

```bash
cmake -S Source/Edge26SimStandalone -B build/sim -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -2
cmake --build build/sim --parallel 2>&1 | tail -3
git add -A
git commit -m "feat(ai): PlayerDecisions.h interface + minimal stub (slot-anchor + face-ball)"
```

### Task T3.3 — Implement `EvaluateOffBall` with all 7 intents

This is the meat of Layer C. Each intent has a target-cell picker + a score function. We implement them all in one task because they share helpers.

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/PlayerDecisions.cpp`

- [ ] **Step 1: Replace the stub with the full implementation**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "AI/PlayerDecisions.h"
#include "AI/Formations.h"
#include "AI/Intents.h"
#include "AI/Roles.h"
#include "AI/SpatialValueModel.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"
#include "Math/Atan2.h"

namespace edge26 {

int SlotIndexForPlayer(int playerIdx) { return playerIdx % 11; }

// ---- Helpers ----------------------------------------------------------------

static Fixed64 SignForTeam(int teamId) {
    return (teamId == 0) ? Fixed64::FromInt(1) : Fixed64::FromInt(-1);
}

// Returns true if cell (its center) is past the opponents' offside line.
static bool CellIsOffsidePub(int cellIdx, int attackingTeam, const FMatchState& m) {
    Fixed64 cellY = CellCenter(cellIdx).X;   // pitch X is up-pitch; spec uses Y but our X axis is up-pitch
    Fixed64 lineY = m.OffsideLineY[1 - attackingTeam];
    return (attackingTeam == 0) ? (cellY.Raw > lineY.Raw) : (cellY.Raw < lineY.Raw);
}

// Iterate cells and find the one with maximum value among those passing `filter`.
template <typename FilterFn>
static int ArgMaxCellFiltered(const FSpatialValueModel& sm, int teamId,
                              ESpatialField field, FilterFn filter)
{
    int bestIdx = 0;
    Fixed32 bestVal = Fixed32::FromRaw(INT32_MIN);
    for (int c = 0; c < kPitchCells; ++c) {
        if (!filter(c)) continue;
        Fixed32 v = sm.Cells[teamId][(int)field][c];
        if (v.Raw > bestVal.Raw) { bestVal = v; bestIdx = c; }
    }
    return bestIdx;
}

// Distance penalty: prefer nearby targets (cheaper to reach in finite stamina).
static Fixed32 DistancePenalty(FixedVec3 target, FixedVec3 from) {
    FixedVec3 d = target - from;
    Fixed64 distSq = d.X * d.X + d.Y * d.Y;
    Fixed64 dist   = SimMath::Sqrt(distSq);
    // Penalty grows linearly to ~Fixed32::One at 30m (3000 cm).
    int64_t penaltyRaw = (dist.Raw * (int64_t)Fixed32::One) / Fixed64::FromInt(3000).Raw;
    if (penaltyRaw > Fixed32::One) penaltyRaw = Fixed32::One;
    return Fixed32::FromRaw((int32_t)penaltyRaw);
}

static Fixed32 BiasFromMentality(int8_t mentality, int8_t sign) {
    // Mentality is -2..+2; sign is +1 (attacking bias) or -1 (defensive bias).
    // Return a multiplier in roughly [0.5..1.5].
    int adjusted = mentality * sign;
    return Fixed32::FromRaw((int32_t)(Fixed32::One + adjusted * (Fixed32::One / 4)));
}

// ---- The main off-ball evaluator -------------------------------------------

static void EvaluateOffBall(FSimPlayerState& p, const FSimWorldState& s,
                            const FRoleWeights& W, int playerIdx,
                            EIntent& bestIntent, FixedVec3& bestTarget, Fixed32& bestScore)
{
    const FTeamPlan& Plan = s.Match.Plans[p.TeamId];
    const bool ownTeamHasBall = (s.Match.PossessionTeam == p.TeamId);

    // 1. HoldPosition — anchor to slot.
    {
        int slot = SlotIndexForPlayer(playerIdx);
        FixedVec3 target = SlotWorldPosition(slot, p.TeamId);
        Fixed32 score = W.HoldPosition * BiasFromMentality(Plan.Mentality, 0);
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::HoldPosition; bestTarget = target; bestScore = score;
        }
    }

    // 2. MakeRunForward — ahead of ball, not offside.
    if (ownTeamHasBall && p.RoleId != (uint8_t)ERole::GK) {
        int cell = ArgMaxCellFiltered(s.Spatial, p.TeamId, ESpatialField::PassReception,
            [&](int c) {
                // Cell must be ahead of ball for the attacking team.
                Fixed64 cellX = CellCenter(c).X;
                Fixed64 signedDelta = (cellX - s.Ball.Position.X) * SignForTeam(p.TeamId);
                if (signedDelta.Raw <= 0) return false;
                // Not past offside line.
                if (CellIsOffsidePub(c, p.TeamId, s.Match)) return false;
                return true;
            });
        FixedVec3 target = CellCenter(cell);
        Fixed32 score = s.Spatial.Cells[p.TeamId][(int)ESpatialField::PassReception][cell]
                      * W.MakeRunForward
                      * BiasFromMentality(Plan.Mentality, +1);
        // Subtract small distance penalty
        Fixed32 penalty = DistancePenalty(target, p.Position);
        if (score.Raw > penalty.Raw) score = Fixed32::FromRaw(score.Raw - penalty.Raw);
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::MakeRunForward; bestTarget = target; bestScore = score;
        }
    }

    // 3. DropToReceive — only when own team has possession.
    if (ownTeamHasBall && p.RoleId != (uint8_t)ERole::GK) {
        // Pick a high-Space cell behind the ball.
        int cell = ArgMaxCellFiltered(s.Spatial, p.TeamId, ESpatialField::Space,
            [&](int c) {
                Fixed64 cellX = CellCenter(c).X;
                Fixed64 signedDelta = (cellX - s.Ball.Position.X) * SignForTeam(p.TeamId);
                return signedDelta.Raw < 0;   // behind ball
            });
        FixedVec3 target = CellCenter(cell);
        Fixed32 score = s.Spatial.Cells[p.TeamId][(int)ESpatialField::Space][cell] * W.DropToReceive;
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::DropToReceive; bestTarget = target; bestScore = score;
        }
    }

    // 4. ProvideWidth — touchline at ball's pitch-X.
    if (ownTeamHasBall && p.RoleId != (uint8_t)ERole::GK) {
        // Pick sideline (max |Y|) at ball's X.
        Fixed64 sideY = (p.Position.Y.Raw < 0) ? -SimConst::PitchHalfWid : SimConst::PitchHalfWid;
        FixedVec3 target { s.Ball.Position.X, sideY * Fixed64::FromRaw(Fixed64::One * 9 / 10),
                           Fixed64::FromInt(0) };
        Fixed32 score = W.ProvideWidth;  // simple flat score; cell-lookup at sideline omitted
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::ProvideWidth; bestTarget = target; bestScore = score;
        }
    }

    // 5. Press — only when opposing team has the ball AND this player is closest to ball.
    if (!ownTeamHasBall && s.Match.PossessionTeam != 0xFF
        && p.RoleId != (uint8_t)ERole::GK)
    {
        // For v0 (pre Layer-B nomination): each player computes "am I nearest to ball on my team?".
        FixedVec3 ballPos = s.Ball.Position;
        Fixed64 ourDistSq = (p.Position.X - ballPos.X) * (p.Position.X - ballPos.X)
                          + (p.Position.Y - ballPos.Y) * (p.Position.Y - ballPos.Y);
        bool iAmNearest = true;
        for (int i = 0; i < kSimPlayerCount; ++i) {
            if (i == playerIdx) continue;
            const auto& other = s.Players[i];
            if (other.TeamId != p.TeamId) continue;
            Fixed64 otherDistSq =
                (other.Position.X - ballPos.X) * (other.Position.X - ballPos.X)
              + (other.Position.Y - ballPos.Y) * (other.Position.Y - ballPos.Y);
            if (otherDistSq.Raw < ourDistSq.Raw) { iAmNearest = false; break; }
        }
        if (iAmNearest) {
            Fixed32 score = W.Press
                * Fixed32::FromRaw(Fixed32::One * (1 + (int)Plan.PressIntensity) / 2);
            if (score.Raw > bestScore.Raw) {
                bestIntent = EIntent::Press; bestTarget = ballPos; bestScore = score;
            }
        }
    }

    // 6. TrackRunner — defending; find nearest opposing forward.
    if (!ownTeamHasBall && s.Match.PossessionTeam != 0xFF) {
        int nearestOpp = -1;
        Fixed64 nearestSq = Fixed64::FromInt(99999999);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const auto& opp = s.Players[i];
            if (opp.TeamId == p.TeamId) continue;
            if (opp.RoleId == (uint8_t)ERole::GK) continue;
            Fixed64 dSq = (opp.Position.X - p.Position.X) * (opp.Position.X - p.Position.X)
                        + (opp.Position.Y - p.Position.Y) * (opp.Position.Y - p.Position.Y);
            if (dSq.Raw < nearestSq.Raw) { nearestSq = dSq; nearestOpp = i; }
        }
        if (nearestOpp >= 0) {
            Fixed32 score = W.TrackRunner;
            if (score.Raw > bestScore.Raw) {
                bestIntent = EIntent::TrackRunner;
                bestTarget = s.Players[nearestOpp].Position;
                bestScore = score;
            }
        }
    }

    // 7. HoldDefensiveLine — defenders only. Layer B fills LineY (default 0 pre-M5).
    if (p.RoleId == (uint8_t)ERole::CB || p.RoleId == (uint8_t)ERole::FB_L
        || p.RoleId == (uint8_t)ERole::FB_R)
    {
        FixedVec3 target { p.Position.X,
                           s.Match.Units[p.TeamId][0].LineY,   // [0] = Defense unit
                           Fixed64::FromInt(0) };
        Fixed32 score = W.HoldDefensiveLine;
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::HoldDefensiveLine; bestTarget = target; bestScore = score;
        }
    }
}

void UpdatePlayerAI(FSimPlayerState& p, const FSimWorldState& s, int playerIdx) {
    const FRoleWeights& W = kRoleWeightsTable[p.RoleId];

    EIntent   bestIntent = EIntent::HoldPosition;
    FixedVec3 bestTarget = SlotWorldPosition(SlotIndexForPlayer(playerIdx), p.TeamId);
    Fixed32   bestScore  = Fixed32::FromRaw(INT32_MIN);

    EvaluateOffBall(p, s, W, playerIdx, bestIntent, bestTarget, bestScore);
    // On-ball evaluation comes in M4.

    p.CurrentIntent = (uint8_t)bestIntent;
    p.AITargetPosition = bestTarget;
    p.FacingTarget = SimMath::Atan2(
        bestTarget.Y - p.Position.Y,
        bestTarget.X - p.Position.X);
}

}  // namespace edge26
```

- [ ] **Step 2: Wire `UpdatePlayerAI` into `SimWorld::Step`**

In `SimWorld.cpp`, after `UpdateSpatialFields(State);`, add:

```cpp
for (int i = 0; i < kSimPlayerCount; ++i) {
    FSimPlayerState& p = State.Players[i];
    if (i == State.Match.HumanControlledIndex) continue;   // human is handled below
    UpdatePlayerAI(p, State, i);
}
```

Add `#include "AI/PlayerDecisions.h"`.

The human-player input flow is unchanged for now — the human is `State.Match.HumanControlledIndex`, which defaults to 0 (the first home outfield player). We'll formalize this in M9.

- [ ] **Step 3: Smoke test — 22-player tick doesn't crash**

In `test_snapshot.cpp`, append:

```cpp
TEST_CASE(Sim_22PlayerTickStable) {
    using namespace edge26;
    SimWorld w{1};
    FInputFrame f{};
    for (int tick = 0; tick < 100; ++tick) {
        f.TickNumber = (uint32_t)tick;
        w.Step(f);
    }
    // Sim should still be alive — verify world tick number advanced and no
    // player has NaN-like fixed values (e.g., absurd positions).
    TEST_EXPECT_EQ(w.GetState().TickNumber, (uint32_t)99);
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = w.GetState().Players[i];
        TEST_EXPECT_TRUE(Abs(p.Position.X).Raw < (SimConst::PitchHalfLen * Fixed64::FromInt(2)).Raw);
        TEST_EXPECT_TRUE(Abs(p.Position.Y).Raw < (SimConst::PitchHalfWid * Fixed64::FromInt(2)).Raw);
    }
    return 0;
}
```

Add `TEST_RUN(Sim_22PlayerTickStable);`.

- [ ] **Step 4: Build + run**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | tail -5
```

- [ ] **Step 5: Lint + commit**

```bash
./Scripts/lint_sim.sh
git add -A
git commit -m "feat(ai): Layer C off-ball evaluator — 7 intents scored from value fields"
```

### Task T3.4 — Regenerate baselines (Layer C changes everything)

- [ ] **Step 1: Update baselines**

```bash
./Scripts/update_determinism_baseline.sh
./Scripts/check_determinism.sh 2>&1 | tail -3
```

Expected: `PASS: all determinism checks`.

- [ ] **Step 2: Commit**

```bash
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): regenerate baselines after Layer C off-ball AI"
```

### Task T3.5 — Mark M3 complete

- [ ] **Step 1: Update PROGRESS.md** (tick M3; advance status to M4; activity log entry)

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): M3 complete; advance to M4 (Layer C on-ball)"
```

---

## M4 — Layer C on-ball decisions

The carrier evaluates 5 intents: `Pass`, `Shoot`, `Dribble`, `Hold`, `Clear`. The winner becomes `p.CurrentIntent`; for kick-like intents the AI raises a synthetic button bit on a per-player AI-input cache that the kick path consumes.

After M4, AI players actually pass and shoot. Possession will start to change hands organically (instead of staying with whichever player happened to be near the ball at kickoff).

### Task T4.1 — `PassSuccessProbability` helper + best-teammate selector

Both Pass and Clear need to score a candidate teammate. We isolate the helpers first so the next task is small.

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/PlayerDecisions.cpp`

- [ ] **Step 1: Add a forward-pass success heuristic**

Append near the existing static helpers (after `BiasFromMentality`, before `EvaluateOffBall`):

```cpp
// Returns a probability in [0..1] (as Fixed32) that a pass from `from` to `to`
// completes successfully. Cheap heuristic: 1 - (n_blockers / 4), clamped to [0,1].
// "Blocker" = opposing player within 1.5m of the straight-line segment.
static Fixed32 PassSuccessProbability(FixedVec3 from, FixedVec3 to,
                                      int passingTeam, const FSimWorldState& s)
{
    FixedVec3 seg = to - from;
    Fixed64   segLenSq = seg.X * seg.X + seg.Y * seg.Y;
    if (segLenSq.Raw <= 0) return Fixed32::FromRaw(Fixed32::One);   // zero-length: trivially "succeeds"
    Fixed64 segLen = SimMath::Sqrt(segLenSq);

    // 1.5m = 150 cm
    const Fixed64 kBlockerRadius = Fixed64::FromInt(150);
    const Fixed64 kBlockerRSq    = kBlockerRadius * kBlockerRadius;

    int blockers = 0;
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& opp = s.Players[i];
        if (opp.TeamId == passingTeam) continue;
        if (opp.RoleId == (uint8_t)ERole::GK) continue;

        // Project opp onto segment.
        FixedVec3 v = opp.Position - from;
        // t = dot(v, seg) / segLen^2, clamped to [0,1]
        Fixed64 dotvs = v.X * seg.X + v.Y * seg.Y;
        if (dotvs.Raw < 0)                continue;        // before `from`
        if (dotvs.Raw > segLenSq.Raw)     continue;        // past `to`
        // Perp distance squared = |v|^2 - (dot)^2 / segLen^2
        Fixed64 vLenSq = v.X * v.X + v.Y * v.Y;
        Fixed64 projLenSq = Fixed64::FromRaw(
            (int64_t)((__int128)dotvs.Raw * dotvs.Raw / segLenSq.Raw));
        Fixed64 perpSq = Fixed64::FromRaw(vLenSq.Raw - projLenSq.Raw);
        if (perpSq.Raw < kBlockerRSq.Raw) ++blockers;
    }

    // success = max(0, 1 - blockers * 0.25)
    int32_t successRaw = Fixed32::One - blockers * (Fixed32::One / 4);
    if (successRaw < 0) successRaw = 0;
    return Fixed32::FromRaw(successRaw);
}

// Pick the best teammate to pass to (or -1 if no candidates exist).
// Score = PassReception[teammate's cell] * PassSuccessProb * forward bonus.
static int BestPassReceiverIdx(const FSimPlayerState& carrier,
                               const FSimWorldState& s,
                               int passingTeam,
                               Fixed32& outBestScore)
{
    int bestIdx = -1;
    outBestScore = Fixed32::FromRaw(INT32_MIN);
    for (int i = 0; i < kSimPlayerCount; ++i) {
        if (i == s.Match.PossessionPlayer) continue;  // can't pass to self
        const auto& mate = s.Players[i];
        if (mate.TeamId != passingTeam) continue;
        if (mate.RoleId == (uint8_t)ERole::GK) continue;
        if (mate.Position == carrier.Position) continue;

        int cellIdx = CellIndex(mate.Position);
        Fixed32 prVal = s.Spatial.Cells[passingTeam][(int)ESpatialField::PassReception][cellIdx];
        Fixed32 succ  = PassSuccessProbability(carrier.Position, mate.Position, passingTeam, s);

        // Forward bonus: prefer up-pitch teammates.
        Fixed64 forwardDelta = (mate.Position.X - carrier.Position.X) * SignForTeam(passingTeam);
        Fixed32 forwardBonus = (forwardDelta.Raw > 0)
            ? Fixed32::FromRaw((int32_t)((forwardDelta.Raw * (int64_t)Fixed32::One)
                                         / SimConst::PitchHalfLen.Raw))
            : Fixed32::FromRaw(0);
        Fixed32 oneF32 = Fixed32::FromRaw(Fixed32::One);
        Fixed32 score = prVal * succ * (oneF32 + forwardBonus);

        if (score.Raw > outBestScore.Raw) {
            outBestScore = score;
            bestIdx = i;
        }
    }
    return bestIdx;
}
```

Note: the `__int128` use is wrapped in a portable form in `Math/Mul64.h` — if you find lint complains, switch the `projLenSq` line to call `SimMath::MulDiv64(dotvs.Raw, dotvs.Raw, segLenSq.Raw)` and add that helper to Mul64.h. For now `__int128` is acceptable inline because it's the same path Phase 1 uses (gated by `__SIZEOF_INT128__`); on MSVC, replace with `_mul128`. (`lint_sim.sh` doesn't currently forbid this.)

- [ ] **Step 2: Build (the helpers aren't called yet) + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Private/AI/PlayerDecisions.cpp
git commit -m "feat(ai): PassSuccessProbability + BestPassReceiverIdx helpers"
```

### Task T4.2 — `EvaluateOnBall` implementation

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/PlayerDecisions.cpp`
- Modify: `Source/Edge26Sim/Public/Sim/PlayerState.h` (add `Pending*` AI-input fields if not already there — see Step 1 below)

- [ ] **Step 1: Add AI synthetic-input cache to `FSimPlayerState`**

The AI doesn't go through `FInputFrame`. It writes synthetic input on a per-player buffer that `MaybeApplyKick` will consult in T4.3. Look at the existing `FSimPlayerState` and confirm whether you have a `PendingButtons` / `PendingMoveX` / `PendingMoveY` field. If not, append them to `PlayerState.h` **and** bump the `static_assert` size accordingly.

If the current struct is exactly 88 B with the layout from T1.2, the simplest place to put 8 bytes of AI-input cache is by replacing `_pad[2]` with a wider pad + new fields, keeping the total at 88. For example:

```cpp
struct FSimPlayerState {
    FixedVec3   Position;          // 0..23
    FixedVec3   Velocity;          // 24..47
    FixedAngle  Heading;           // 48..51
    FixedAngle  FacingTarget;      // 52..55
    uint8_t     ControllerIndex;   // 56
    uint8_t     Flags;             // 57
    uint8_t     TeamId;            // 58
    uint8_t     RoleId;            // 59
    uint8_t     CurrentIntent;     // 60
    uint8_t     IntendedPassTarget;// 61
    uint8_t     PendingButtons;    // 62  NEW (M4): AI-set synthetic buttons
    uint8_t     _pad0;             // 63
    FixedVec3   AITargetPosition;  // 64..87
};
static_assert(sizeof(FSimPlayerState) == 88);
```

If your current `_pad` already lives in this position, just rename one byte to `PendingButtons` and another to `_pad0`. **Verify by reading the file first**; the static_assert is the safety net.

- [ ] **Step 2: Replace `UpdatePlayerAI` to call both evaluators**

In `PlayerDecisions.cpp`, replace the existing `UpdatePlayerAI` with:

```cpp
static void EvaluateOnBall(FSimPlayerState& p, const FSimWorldState& s,
                           const FRoleWeights& W, int playerIdx,
                           EIntent& bestIntent, FixedVec3& bestTarget, Fixed32& bestScore)
{
    const FTeamPlan& Plan = s.Match.Plans[p.TeamId];
    int carrierCell = CellIndex(p.Position);

    // 1. Pass
    {
        Fixed32 bestPassScore = Fixed32::FromRaw(INT32_MIN);
        int receiverIdx = BestPassReceiverIdx(p, s, p.TeamId, bestPassScore);
        if (receiverIdx >= 0) {
            Fixed32 score = bestPassScore * W.PreferPass;
            if (score.Raw > bestScore.Raw) {
                bestIntent = EIntent::Pass;
                bestTarget = s.Players[receiverIdx].Position;
                bestScore  = score;
                p.IntendedPassTarget = (uint8_t)receiverIdx;
            }
        }
    }

    // 2. Shoot — gated by "in opponent's third".
    {
        Fixed64 thirdLine = SimConst::PitchHalfLen * Fixed64::FromInt(1) / Fixed64::FromInt(3);
        Fixed64 inOppThird = (p.Position.X * SignForTeam(p.TeamId)) - thirdLine;
        if (inOppThird.Raw > 0) {
            Fixed32 threat = s.Spatial.Cells[p.TeamId][(int)ESpatialField::Threat][carrierCell];
            Fixed32 score  = threat * W.PreferShoot * Plan.MentalityShootBias;
            // Target = opponent goal center.
            FixedVec3 goalCenter {
                SimConst::PitchHalfLen * SignForTeam(p.TeamId),
                Fixed64::FromInt(0),
                Fixed64::FromInt(0)
            };
            if (score.Raw > bestScore.Raw) {
                bestIntent = EIntent::Shoot;
                bestTarget = goalCenter;
                bestScore  = score;
            }
        }
    }

    // 3. Dribble — pick the best-Space adjacent cell minus opp proximity penalty.
    {
        // 4-neighborhood in cell space.
        int bestCellIdx = -1;
        Fixed32 bestDribbleScore = Fixed32::FromRaw(INT32_MIN);
        int cx = carrierCell % kPitchCellsX;
        int cy = carrierCell / kPitchCellsX;
        const int dxs[4] = { +1, -1,  0,  0 };
        const int dys[4] = {  0,  0, +1, -1 };
        for (int n = 0; n < 4; ++n) {
            int nx = cx + dxs[n];
            int ny = cy + dys[n];
            if (nx < 0 || nx >= kPitchCellsX || ny < 0 || ny >= kPitchCellsY) continue;
            int neighborCell = ny * kPitchCellsX + nx;
            Fixed32 space = s.Spatial.Cells[p.TeamId][(int)ESpatialField::Space][neighborCell];

            FixedVec3 cellPos = CellCenter(neighborCell);
            // Nearest opponent penalty.
            Fixed64 nearestSq = Fixed64::FromInt(99999999);
            for (int i = 0; i < kSimPlayerCount; ++i) {
                const auto& o = s.Players[i];
                if (o.TeamId == p.TeamId) continue;
                Fixed64 dSq = (o.Position.X - cellPos.X) * (o.Position.X - cellPos.X)
                            + (o.Position.Y - cellPos.Y) * (o.Position.Y - cellPos.Y);
                if (dSq.Raw < nearestSq.Raw) nearestSq = dSq;
            }
            Fixed64 nearestDist = SimMath::Sqrt(nearestSq);
            // Penalty: max(0, 5m - nearestDist) / 5m
            Fixed64 fiveM = Fixed64::FromInt(500);
            int32_t penaltyRaw = 0;
            if (nearestDist.Raw < fiveM.Raw) {
                penaltyRaw = (int32_t)(((fiveM.Raw - nearestDist.Raw) * (int64_t)Fixed32::One)
                                        / fiveM.Raw);
            }
            Fixed32 score = space * W.PreferDribble;
            score = Fixed32::FromRaw(score.Raw - penaltyRaw);
            if (score.Raw > bestDribbleScore.Raw) {
                bestDribbleScore = score;
                bestCellIdx = neighborCell;
            }
        }
        if (bestCellIdx >= 0 && bestDribbleScore.Raw > bestScore.Raw) {
            bestIntent = EIntent::Dribble;
            bestTarget = CellCenter(bestCellIdx);
            bestScore  = bestDribbleScore;
        }
    }

    // 4. Hold — flat fallback weighted by Plan.HoldBias.
    {
        Fixed32 score = Plan.HoldBias;
        // Multiply by small constant (0.5) so Hold is the "do nothing" default.
        score = Fixed32::FromRaw(score.Raw / 2);
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::Hold;
            bestTarget = p.Position;
            bestScore  = score;
        }
    }

    // 5. Clear — defender-only panic ball.
    if (p.RoleId == (uint8_t)ERole::CB || p.RoleId == (uint8_t)ERole::FB_L
        || p.RoleId == (uint8_t)ERole::FB_R)
    {
        // Target: 30m up-pitch, on carrier's lateral side.
        Fixed64 thirtyM = Fixed64::FromInt(3000);
        FixedVec3 target {
            p.Position.X + thirtyM * SignForTeam(p.TeamId),
            p.Position.Y,
            Fixed64::FromInt(0)
        };
        // Estimate self-cell Threat (higher threat near own goal => panic ball more valuable).
        Fixed32 ownThreat = s.Spatial.Cells[1 - p.TeamId][(int)ESpatialField::Threat][carrierCell];
        Fixed32 score = W.PreferLongBall * ownThreat * Plan.PanicBias;
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::Clear;
            bestTarget = target;
            bestScore  = score;
        }
    }
}

void UpdatePlayerAI(FSimPlayerState& p, const FSimWorldState& s, int playerIdx) {
    // Reset per-tick synthetic buttons (carrier may set them below).
    p.PendingButtons = 0;

    const FRoleWeights& W   = kRoleWeightsTable[p.RoleId];
    const bool onBall       = (s.Match.PossessionPlayer == (uint8_t)playerIdx);

    EIntent   bestIntent = EIntent::HoldPosition;
    FixedVec3 bestTarget = SlotWorldPosition(SlotIndexForPlayer(playerIdx), p.TeamId);
    Fixed32   bestScore  = Fixed32::FromRaw(INT32_MIN);

    if (onBall) {
        EvaluateOnBall (p, s, W, playerIdx, bestIntent, bestTarget, bestScore);
    } else {
        EvaluateOffBall(p, s, W, playerIdx, bestIntent, bestTarget, bestScore);
    }

    p.CurrentIntent    = (uint8_t)bestIntent;
    p.AITargetPosition = bestTarget;
    p.FacingTarget     = SimMath::Atan2(
        bestTarget.Y - p.Position.Y,
        bestTarget.X - p.Position.X);

    // On-ball intents that fire a kick raise a synthetic button bit. The bit
    // layout matches FInputFrame.Buttons (Sprint=1, Pass=2, Shoot=4, Chip=8).
    switch (bestIntent) {
        case EIntent::Pass:  p.PendingButtons |= (1 << 1); break;   // Pass
        case EIntent::Shoot: p.PendingButtons |= (1 << 2); break;   // Shoot
        case EIntent::Clear: p.PendingButtons |= (1 << 3); break;   // Chip (reuse chip impulse)
        default: break;
    }
}
```

- [ ] **Step 3: Build + run smoke test**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | tail -5
```

Self-test should still pass — no behavioral change yet because we haven't wired `PendingButtons` into `MaybeApplyKick`.

- [ ] **Step 4: Lint + commit**

```bash
./Scripts/lint_sim.sh
git add -A
git commit -m "feat(ai): EvaluateOnBall — Pass/Shoot/Dribble/Hold/Clear scoring"
```

### Task T4.3 — Wire AI synthetic input into `MaybeApplyKick` + `IntendedPassTarget`

The kick path in Phase 1 read buttons off the `FInputFrame`. AI players don't appear in the input frame; we need a per-player buttons source.

**Files:**
- Modify: `Source/Edge26Sim/Private/Sim/SimWorld.cpp` (the `MaybeApplyKick` function)
- Modify: `Source/Edge26Sim/Private/AI/PlayerDecisions.cpp` (only if the signature changes)

- [ ] **Step 1: Read the existing `MaybeApplyKick` and locate its button read**

```bash
grep -n "MaybeApplyKick\|Buttons" Source/Edge26Sim/Private/Sim/SimWorld.cpp | head -20
```

Find the line(s) where `frame.Buttons` is read. In Phase 1 it was something like:

```cpp
if (frame.Buttons & (1<<1)) { /* Pass */ }
```

- [ ] **Step 2: Make the kick path read AI buttons for non-human players**

Replace the `Buttons` accesses with a per-player resolved value. Two clean ways; pick whichever fits your existing call sites:

```cpp
static uint16_t ResolveButtonsForPlayer(const FInputFrame& frame,
                                        const FSimPlayerState& p,
                                        const FMatchState& m,
                                        int playerIdx)
{
    if ((uint8_t)playerIdx == m.HumanControlledIndex) return (uint16_t)frame.Buttons;
    return (uint16_t)p.PendingButtons;
}
```

Then in `MaybeApplyKick(...)` (which already receives the player), call `ResolveButtonsForPlayer(...)` instead of `frame.Buttons`. If `MaybeApplyKick` doesn't currently take `playerIdx` or `FMatchState`, change its signature to do so; update the call site in `SimWorld::Step`.

```cpp
// In SimWorld::Step (call site update)
for (int i = 0; i < kSimPlayerCount; ++i) {
    MaybeApplyKick(State.Ball, State.Players[i], frame, State.Match, i);
}
```

- [ ] **Step 3: Use `IntendedPassTarget` to aim the Pass impulse**

In `MaybeApplyKick`'s Pass branch, if `p.IntendedPassTarget < kSimPlayerCount`, override the impulse direction:

```cpp
if (buttons & (1<<1)) {                                // Pass
    FixedVec3 dir;
    if (p.IntendedPassTarget < kSimPlayerCount) {
        FixedVec3 toMate = state.Players[p.IntendedPassTarget].Position - p.Position;
        // Normalize.
        Fixed64 d = SimMath::Sqrt(toMate.X*toMate.X + toMate.Y*toMate.Y);
        if (d.Raw > 0) {
            dir.X = Fixed64::FromRaw((toMate.X.Raw * Fixed64::One) / d.Raw);
            dir.Y = Fixed64::FromRaw((toMate.Y.Raw * Fixed64::One) / d.Raw);
            dir.Z = Fixed64::FromInt(0);
        } else {
            dir = FixedVec3{ Fixed64::FromInt(1), Fixed64::FromInt(0), Fixed64::FromInt(0) };
        }
    } else {
        dir = HeadingToVector(p.Heading);              // existing fallback
    }
    ball.Velocity = dir * kPassImpulseSpeed;           // existing constant
}
```

The Shoot branch can keep aiming at heading (already toward `bestTarget` = opp goal center). The Chip/Clear branch keeps its current behavior — Layer C aimed `FacingTarget` at the long-ball target before `StepPlayer` re-aligned heading.

`MaybeApplyKick` now needs `FSimWorldState&` (or at least `Players[]`) to read `IntendedPassTarget`'s position. Adjust the signature.

- [ ] **Step 4: Build + self-test**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | tail -5
```

Existing tests should still pass — the only behavioral change is for AI players, and the self-test doesn't exercise multi-player kicks (yet).

- [ ] **Step 5: Add a directed AI-kick test**

In `test_snapshot.cpp`, append:

```cpp
TEST_CASE(Sim_AICarrierFiresPass) {
    using namespace edge26;
    SimWorld w{1};
    auto& st = w.MutableState();
    // Put two home players + ball at a clear spot; make player 1 the carrier.
    st.Match.PossessionTeam = 0;
    st.Match.PossessionPlayer = 1;
    st.Match.HumanControlledIndex = 0xFF;   // no human
    st.Players[1].Position = FixedVec3{ Fixed64::FromInt(0), Fixed64::FromInt(0), Fixed64::FromInt(0) };
    st.Players[2].Position = FixedVec3{ Fixed64::FromInt(1000), Fixed64::FromInt(0), Fixed64::FromInt(0) };
    st.Ball.Position       = st.Players[1].Position;
    st.Ball.Velocity       = FixedVec3::Zero();
    // Push opponents off pitch so they don't block the pass.
    for (int i = 11; i < kSimPlayerCount; ++i)
        st.Players[i].Position = FixedVec3{ Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0) };

    FInputFrame f{};
    for (int t = 0; t < 5; ++t) {
        f.TickNumber = (uint32_t)t;
        w.Step(f);
        if (st.Ball.Velocity.X.Raw != 0) break;
    }
    TEST_EXPECT_TRUE(st.Ball.Velocity.X.Raw != 0);   // pass fired
    return 0;
}
```

Add `TEST_RUN(Sim_AICarrierFiresPass);`.

- [ ] **Step 6: Run + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "AICarrierFiresPass|Self-test"
./Scripts/lint_sim.sh
git add -A
git commit -m "feat(ai): wire PendingButtons into MaybeApplyKick — AI carriers can pass/shoot/clear"
```

### Task T4.4 — `UpdatePossession` helper

Possession needs to flip when:
1. A non-carrier opponent gets within ~80 cm of the ball after a kick releases it.
2. Ball goes out of play (just clears possession; we don't restart in v0).
3. GK collects (M8 sets this directly).

We implement (1) + (2) here. (3) lands with the GK in M8.

**Files:**
- Modify: `Source/Edge26Sim/Private/Sim/SimWorld.cpp`

- [ ] **Step 1: Add `UpdatePossession` near `MaybeApplyKick`**

```cpp
static void UpdatePossession(FSimWorldState& s)
{
    // Ball out of pitch → clear possession (no restart in v0).
    if (Abs(s.Ball.Position.X).Raw > SimConst::PitchHalfLen.Raw ||
        Abs(s.Ball.Position.Y).Raw > SimConst::PitchHalfWid.Raw)
    {
        s.Match.PossessionTeam   = 0xFF;
        s.Match.PossessionPlayer = 0xFF;
        return;
    }

    // Nearest outfield player within pickup radius gains possession.
    const Fixed64 kPickupRadius = Fixed64::FromInt(80);   // 80 cm
    const Fixed64 kPickupRSq    = kPickupRadius * kPickupRadius;
    int   bestIdx = -1;
    Fixed64 bestSq = kPickupRSq;
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        Fixed64 dSq = (p.Position.X - s.Ball.Position.X) * (p.Position.X - s.Ball.Position.X)
                    + (p.Position.Y - s.Ball.Position.Y) * (p.Position.Y - s.Ball.Position.Y);
        if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
    }
    if (bestIdx >= 0) {
        s.Match.PossessionPlayer = (uint8_t)bestIdx;
        s.Match.PossessionTeam   = s.Players[bestIdx].TeamId;
    }
}
```

- [ ] **Step 2: Call it in `SimWorld::Step` between offside resolution and ball physics**

```cpp
// (existing) UpdateSpatialFields, Layers A/B/C, kicks, GK save, offside resolve...

UpdatePossession(State);                 // NEW (M4 T4.4)
StepBall(State.Ball);                    // existing
```

(If you haven't added the offside-resolve and GK-save lines yet — they come in M7 / M8 — that's fine; insert `UpdatePossession` just before `StepBall` for now.)

- [ ] **Step 3: Test**

In `test_snapshot.cpp`, append:

```cpp
TEST_CASE(Sim_PossessionFlipsOnPickup) {
    using namespace edge26;
    SimWorld w{1};
    auto& st = w.MutableState();
    st.Match.PossessionTeam   = 0xFF;
    st.Match.PossessionPlayer = 0xFF;
    st.Match.HumanControlledIndex = 0xFF;
    // Park player 1 (home) on the ball; everyone else far away.
    for (int i = 0; i < kSimPlayerCount; ++i)
        st.Players[i].Position = FixedVec3{ Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0) };
    st.Players[1].Position = FixedVec3{ Fixed64::FromInt(0), Fixed64::FromInt(0), Fixed64::FromInt(0) };
    st.Ball.Position       = st.Players[1].Position;
    st.Ball.Velocity       = FixedVec3::Zero();

    FInputFrame f{};
    f.TickNumber = 1;
    w.Step(f);
    TEST_EXPECT_EQ(st.Match.PossessionPlayer, (uint8_t)1);
    TEST_EXPECT_EQ(st.Match.PossessionTeam,   (uint8_t)0);
    return 0;
}
```

Add `TEST_RUN(Sim_PossessionFlipsOnPickup);`.

- [ ] **Step 4: Run + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "PossessionFlips|Self-test"
./Scripts/lint_sim.sh
git add -A
git commit -m "feat(sim): UpdatePossession — pickup-radius assignment + out-of-pitch clear"
```

### Task T4.5 — Regenerate baselines

- [ ] **Step 1: Update baselines**

```bash
./Scripts/update_determinism_baseline.sh
./Scripts/check_determinism.sh 2>&1 | tail -3
```

Expected: `PASS: all determinism checks`.

- [ ] **Step 2: Commit**

```bash
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): regenerate baselines after Layer C on-ball + possession tracking"
```

### Task T4.6 — Mark M4 complete

- [ ] **Step 1: Update PROGRESS.md** (tick M4; advance status to M5; activity log entry)

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): M4 complete; advance to M5 (Layer B unit coordination)"
```

---

## M5 — Layer B unit coordination

Three units per team: Defense (0), Midfield (1), Attack (2). Layer B runs at 10 Hz (every 5 ticks) and publishes into `FMatchState::Units[team][unit]` the fields Layer C reads: `LineY`, `Compactness`, `PressTrigger`, `PressTargetIdx`, `OverlapTriggerIdx`.

After M5, you should observe in PIE: one defender stepping to the ball-carrier (others holding), defenders moving as a line, wide-side FB overlapping when teammates carry the ball on their flank.

### Task T5.1 — `UnitOf(role)` helper + new file `UnitCoordination.h/.cpp`

**Files:**
- Create: `Source/Edge26Sim/Public/AI/UnitCoordination.h`
- Create: `Source/Edge26Sim/Private/AI/UnitCoordination.cpp`
- Modify: `Source/Edge26Sim/Public/AI/Roles.h` (add `UnitOf` inline helper)

- [ ] **Step 1: Add `UnitOf(role)` to `Roles.h`**

Append at the bottom of the `namespace edge26 {` block:

```cpp
// Returns the unit index a role belongs to.
// 0 = Defense, 1 = Midfield, 2 = Attack. GK gets a dedicated path (M8) but
// returns 0 here for cleanliness.
constexpr int UnitOf(ERole r) {
    switch (r) {
        case ERole::GK:
        case ERole::CB:
        case ERole::FB_L:
        case ERole::FB_R: return 0;
        case ERole::CDM:
        case ERole::CM:
        case ERole::CAM:  return 1;
        case ERole::W_L:
        case ERole::W_R:
        case ERole::ST:   return 2;
        default:          return 1;
    }
}
```

- [ ] **Step 2: Write `UnitCoordination.h`**

```cpp
// Copyright Edge26. All Rights Reserved.
#pragma once

namespace edge26 {

struct FSimWorldState;
struct FUnitState;

// Entry points called once per (team, unit) every 5 ticks.
void UpdateDefensiveUnit(FUnitState& u, FSimWorldState& s, int teamId);
void UpdateMidfieldUnit (FUnitState& u, FSimWorldState& s, int teamId);
void UpdateAttackUnit   (FUnitState& u, FSimWorldState& s, int teamId);

// Convenience: runs all three for both teams. Cheap; called from SimWorld::Step.
void UpdateAllUnits(FSimWorldState& s);

}  // namespace edge26
```

- [ ] **Step 3: Write minimal `UnitCoordination.cpp` stub**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "AI/UnitCoordination.h"
#include "AI/Roles.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"

namespace edge26 {

void UpdateDefensiveUnit(FUnitState&, FSimWorldState&, int)  {}
void UpdateMidfieldUnit (FUnitState&, FSimWorldState&, int)  {}
void UpdateAttackUnit   (FUnitState&, FSimWorldState&, int)  {}

void UpdateAllUnits(FSimWorldState& s) {
    for (int team = 0; team < 2; ++team) {
        UpdateDefensiveUnit(s.Match.Units[team][0], s, team);
        UpdateMidfieldUnit (s.Match.Units[team][1], s, team);
        UpdateAttackUnit   (s.Match.Units[team][2], s, team);
    }
}

}  // namespace edge26
```

- [ ] **Step 4: Wire `UpdateAllUnits` into `SimWorld::Step` at 10 Hz**

In `SimWorld.cpp`, near the top of `Step`:

```cpp
#include "AI/UnitCoordination.h"
// ...
// (after UpdateSpatialFields)
if (State.TickNumber % 5 == 0) {
    UpdateAllUnits(State);
}
```

- [ ] **Step 5: Build + commit**

```bash
cmake -S Source/Edge26SimStandalone -B build/sim -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -2
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Public/AI/UnitCoordination.h \
        Source/Edge26Sim/Private/AI/UnitCoordination.cpp \
        Source/Edge26Sim/Public/AI/Roles.h \
        Source/Edge26Sim/Private/Sim/SimWorld.cpp
git commit -m "feat(ai): UnitCoordination scaffolding + UnitOf + 10Hz hook in SimWorld"
```

### Task T5.2 — Implement `UpdateDefensiveUnit`

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/UnitCoordination.cpp`

- [ ] **Step 1: Replace the `UpdateDefensiveUnit` stub**

```cpp
// Helpers (top of file, after includes)
static Fixed64 SignForTeam(int teamId) {
    return (teamId == 0) ? Fixed64::FromInt(1) : Fixed64::FromInt(-1);
}

void UpdateDefensiveUnit(FUnitState& u, FSimWorldState& s, int teamId) {
    // Average X of CBs + FBs (X is up-pitch in our frame).
    Fixed64 sumX     = Fixed64::FromInt(0);
    int     countX   = 0;
    Fixed64 maxXForward = Fixed64::FromInt(-99999999) * SignForTeam(teamId);
    int     lastDefIdx  = -1;

    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId != teamId) continue;
        if (UnitOf((ERole)p.RoleId) != 0) continue;
        if (p.RoleId == (uint8_t)ERole::GK) continue;
        sumX = sumX + p.Position.X;
        ++countX;
        // Find last defender (furthest from their own goal).
        Fixed64 forward = p.Position.X * SignForTeam(teamId);
        Fixed64 bestForward = maxXForward * SignForTeam(teamId);
        if (forward.Raw > bestForward.Raw) {
            maxXForward = p.Position.X;
            lastDefIdx  = i;
        }
    }
    if (countX > 0) {
        u.LineY = Fixed64::FromRaw(sumX.Raw / countX);
        // LineHeightBias scales by ±5m.
        const FTeamPlan& Plan = s.Match.Plans[teamId];
        Fixed64 bias = Fixed64::FromInt((int)Plan.LineHeightBias * 500);   // 5m per step
        u.LineY = Fixed64::FromRaw(u.LineY.Raw + bias.Raw * SignForTeam(teamId).Raw / Fixed64::One);
    }

    // OffsideLineY = max-forward defender's X OR ball X, whichever is closer to
    // their own goal. (Standard offside rule.)
    Fixed64 ballX = s.Ball.Position.X;
    if (lastDefIdx >= 0) {
        Fixed64 a = maxXForward * SignForTeam(teamId);
        Fixed64 b = ballX        * SignForTeam(teamId);
        Fixed64 lineForward = (a.Raw < b.Raw) ? a : b;
        s.Match.OffsideLineY[teamId] = Fixed64::FromRaw(lineForward.Raw * SignForTeam(teamId).Raw / Fixed64::One);
    } else {
        s.Match.OffsideLineY[teamId] = Fixed64::FromInt(0);
    }

    // Compactness = stddev of X positions (cheap proxy).
    if (countX > 1) {
        Fixed64 meanX = u.LineY;
        Fixed64 sumSq = Fixed64::FromInt(0);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const auto& p = s.Players[i];
            if (p.TeamId != teamId) continue;
            if (UnitOf((ERole)p.RoleId) != 0) continue;
            if (p.RoleId == (uint8_t)ERole::GK) continue;
            Fixed64 d = p.Position.X - meanX;
            sumSq = sumSq + d * d;
        }
        Fixed64 var = Fixed64::FromRaw(sumSq.Raw / countX);
        Fixed64 std = SimMath::Sqrt(var);
        // Project to Fixed32 (1m ≈ 0.1 raw).
        u.Compactness = Fixed32::FromRaw((int32_t)(std.Raw * Fixed32::One / Fixed64::FromInt(1000).Raw));
    }

    // Press nomination: only when opponent has the ball.
    if (s.Match.PossessionTeam != (uint8_t)teamId && s.Match.PossessionTeam != 0xFF) {
        u.PressTrigger = 1;
        // Pick nearest unit-member to ball.
        int   bestIdx = 0xFF;
        Fixed64 bestSq = Fixed64::FromInt(99999999);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const auto& p = s.Players[i];
            if (p.TeamId != teamId) continue;
            if (UnitOf((ERole)p.RoleId) != 0) continue;
            if (p.RoleId == (uint8_t)ERole::GK) continue;
            Fixed64 dSq = (p.Position.X - s.Ball.Position.X) * (p.Position.X - s.Ball.Position.X)
                        + (p.Position.Y - s.Ball.Position.Y) * (p.Position.Y - s.Ball.Position.Y);
            if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
        }
        u.PressTargetIdx = (uint8_t)bestIdx;
    } else {
        u.PressTrigger   = 0;
        u.PressTargetIdx = 0xFF;
    }
    u.OverlapTriggerIdx = 0xFF;       // defense unit doesn't overlap
}
```

Add `#include "Math/Sqrt.h"` (or whichever header `SimMath::Sqrt` comes from in this codebase).

- [ ] **Step 2: Build + lint + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Private/AI/UnitCoordination.cpp
git commit -m "feat(ai): UpdateDefensiveUnit — line + offside + press nomination + compactness"
```

### Task T5.3 — Implement `UpdateMidfieldUnit`

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/UnitCoordination.cpp`

- [ ] **Step 1: Replace the stub**

```cpp
void UpdateMidfieldUnit(FUnitState& u, FSimWorldState& s, int teamId) {
    Fixed64 sumX = Fixed64::FromInt(0);
    int count = 0;
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId != teamId) continue;
        if (UnitOf((ERole)p.RoleId) != 1) continue;
        sumX = sumX + p.Position.X;
        ++count;
    }
    if (count > 0) u.LineY = Fixed64::FromRaw(sumX.Raw / count);

    // Press: only when opp has the ball AND it's in the central channel
    // (|ball.Y| < 1/3 pitch width).
    const Fixed64 centralChannel = SimConst::PitchHalfWid / Fixed64::FromInt(3);
    bool oppHasBall = (s.Match.PossessionTeam != (uint8_t)teamId
                        && s.Match.PossessionTeam != 0xFF);
    if (oppHasBall && Abs(s.Ball.Position.Y).Raw < centralChannel.Raw) {
        u.PressTrigger = 1;
        int bestIdx = 0xFF;
        Fixed64 bestSq = Fixed64::FromInt(99999999);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const auto& p = s.Players[i];
            if (p.TeamId != teamId) continue;
            if (UnitOf((ERole)p.RoleId) != 1) continue;
            Fixed64 dSq = (p.Position.X - s.Ball.Position.X) * (p.Position.X - s.Ball.Position.X)
                        + (p.Position.Y - s.Ball.Position.Y) * (p.Position.Y - s.Ball.Position.Y);
            if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
        }
        u.PressTargetIdx = (uint8_t)bestIdx;
    } else {
        u.PressTrigger   = 0;
        u.PressTargetIdx = 0xFF;
    }

    // Compactness — same proxy as defense.
    if (count > 1) {
        Fixed64 meanX = u.LineY;
        Fixed64 sumSq = Fixed64::FromInt(0);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const auto& p = s.Players[i];
            if (p.TeamId != teamId) continue;
            if (UnitOf((ERole)p.RoleId) != 1) continue;
            Fixed64 d = p.Position.X - meanX;
            sumSq = sumSq + d * d;
        }
        Fixed64 var = Fixed64::FromRaw(sumSq.Raw / count);
        Fixed64 std = SimMath::Sqrt(var);
        u.Compactness = Fixed32::FromRaw((int32_t)(std.Raw * Fixed32::One / Fixed64::FromInt(1000).Raw));
    }
    u.OverlapTriggerIdx = 0xFF;
}
```

- [ ] **Step 2: Build + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Private/AI/UnitCoordination.cpp
git commit -m "feat(ai): UpdateMidfieldUnit — central-channel press + line + compactness"
```

### Task T5.4 — Implement `UpdateAttackUnit`

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/UnitCoordination.cpp`

- [ ] **Step 1: Replace the stub**

```cpp
void UpdateAttackUnit(FUnitState& u, FSimWorldState& s, int teamId) {
    // LineY = highest player X (further forward = larger up-pitch sign).
    Fixed64 bestForward = Fixed64::FromInt(-99999999);
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId != teamId) continue;
        if (UnitOf((ERole)p.RoleId) != 2) continue;
        Fixed64 forward = p.Position.X * SignForTeam(teamId);
        if (forward.Raw > bestForward.Raw) bestForward = forward;
    }
    u.LineY = Fixed64::FromRaw(bestForward.Raw * SignForTeam(teamId).Raw / Fixed64::One);

    // Overlap nomination: if carrier is on a flank AND a same-side FB exists,
    // nominate that FB for the overlap.
    u.OverlapTriggerIdx = 0xFF;
    if (s.Match.PossessionTeam == (uint8_t)teamId
        && s.Match.PossessionPlayer != 0xFF)
    {
        const auto& carrier = s.Players[s.Match.PossessionPlayer];
        // "Flank" = |Y| > 2/3 half-width.
        Fixed64 flankCut = SimConst::PitchHalfWid * Fixed64::FromInt(2) / Fixed64::FromInt(3);
        if (Abs(carrier.Position.Y).Raw > flankCut.Raw) {
            uint8_t wantedFBRole = (carrier.Position.Y.Raw > 0)
                ? (uint8_t)ERole::FB_R
                : (uint8_t)ERole::FB_L;
            for (int i = 0; i < kSimPlayerCount; ++i) {
                const auto& p = s.Players[i];
                if (p.TeamId != teamId) continue;
                if (p.RoleId != wantedFBRole) continue;
                u.OverlapTriggerIdx = (uint8_t)i;
                break;
            }
        }
    }

    // Press nomination: attack unit only presses when opp has ball in opp's
    // own third (high press). For v0 keep this off — Defense + Midfield handle it.
    u.PressTrigger   = 0;
    u.PressTargetIdx = 0xFF;
    u.Compactness    = Fixed32::FromRaw(0);
}
```

- [ ] **Step 2: Build + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Private/AI/UnitCoordination.cpp
git commit -m "feat(ai): UpdateAttackUnit — top-line + overlap nomination"
```

### Task T5.5 — Layer C consumes Layer B nominations (Press, Overlap, LineY)

Layer C already reads `Units[][0].LineY` (HoldDefensiveLine target). Now make Press fire higher when nominated, and MakeRunForward boost when an FB is the overlap-trigger.

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/PlayerDecisions.cpp`

- [ ] **Step 1: Boost Press when nominated**

In `EvaluateOffBall`, find the `Press` block (intent 5). Replace it with:

```cpp
// 5. Press — opp possession + I'm the nominated presser for my unit.
if (!ownTeamHasBall && s.Match.PossessionTeam != 0xFF
    && p.RoleId != (uint8_t)ERole::GK)
{
    int myUnit = UnitOf((ERole)p.RoleId);
    const FUnitState& unit = s.Match.Units[p.TeamId][myUnit];
    bool iAmNominated = (unit.PressTrigger != 0)
                      && (unit.PressTargetIdx == (uint8_t)playerIdx);

    if (iAmNominated) {
        Fixed32 score = W.Press
            * Fixed32::FromRaw(Fixed32::One * (1 + (int)Plan.PressIntensity) / 2)
            * Fixed32::FromInt(3);                          // 3× boost for nominee
        if (score.Raw > bestScore.Raw) {
            bestIntent = EIntent::Press; bestTarget = s.Ball.Position; bestScore = score;
        }
    }
}
```

This replaces the old "I happen to be nearest" check with the deterministic Layer B nomination.

- [ ] **Step 2: Boost MakeRunForward for overlap-nominated FBs**

Still in `EvaluateOffBall`, find the `MakeRunForward` block (intent 2). After the `score` computation but before the bestScore comparison, add:

```cpp
// Overlap nomination: 2× boost for the nominated FB.
const FUnitState& attackUnit = s.Match.Units[p.TeamId][2];
if ((p.RoleId == (uint8_t)ERole::FB_L || p.RoleId == (uint8_t)ERole::FB_R)
    && attackUnit.OverlapTriggerIdx == (uint8_t)playerIdx)
{
    score = score * Fixed32::FromInt(2);
}
```

- [ ] **Step 3: Don't forget the `#include` for `UnitOf`**

At the top of `PlayerDecisions.cpp` (or its existing `#include "AI/Roles.h"` already gives it — verify):

```bash
grep -n "AI/Roles.h\|UnitOf" Source/Edge26Sim/Private/AI/PlayerDecisions.cpp
```

If `UnitOf` resolves, you're good.

- [ ] **Step 4: Build + lint + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Private/AI/PlayerDecisions.cpp
git commit -m "feat(ai): Layer C consumes Layer B — Press nomination (3x) + Overlap nomination (2x)"
```

### Task T5.6 — Regenerate baselines

- [ ] **Step 1: Update baselines**

```bash
./Scripts/update_determinism_baseline.sh
./Scripts/check_determinism.sh 2>&1 | tail -3
```

Expected: `PASS: all determinism checks`.

- [ ] **Step 2: Commit**

```bash
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): regenerate baselines after Layer B unit coordination"
```

### Task T5.7 — Mark M5 complete

- [ ] **Step 1: Update PROGRESS.md** (tick M5; advance status to M6; activity log entry)

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): M5 complete; advance to M6 (Layer A team strategy)"
```

---

## M6 — Layer A team strategy

Two `UpdateTeamStrategy` calls per 25 ticks (one per team). Writes `Plans[team]`. Layers B + C read it next tick. The visible effect: trailing teams late in the match push higher, leading teams drop deeper.

Match seconds remaining is not yet in the sim — Phase 2 v0 hard-codes a "match length = 90 minutes" assumption and derives it from `TickNumber`. We expose it as a helper.

### Task T6.1 — `MatchSecondsRemaining` helper + `UpdateTeamStrategy` skeleton

**Files:**
- Create: `Source/Edge26Sim/Public/AI/TeamStrategy.h`
- Create: `Source/Edge26Sim/Private/AI/TeamStrategy.cpp`

- [ ] **Step 1: Write the header**

```cpp
// Copyright Edge26. All Rights Reserved.
#pragma once
#include <cstdint>

namespace edge26 {

struct FSimWorldState;
struct FTeamPlan;

// Total match length in seconds (90 minutes for v0). Hardcoded.
constexpr int32_t kMatchTotalSeconds = 90 * 60;

// Tick rate of the sim (per Phase 1 spec). Used to convert TickNumber → seconds.
constexpr int32_t kSimTickHz = 50;

int32_t MatchSecondsRemaining(const FSimWorldState& s);

// Entry point called once per team every 25 ticks (2 Hz).
void UpdateTeamStrategy(FTeamPlan& Plan, const FSimWorldState& s, int teamId);

// Convenience: runs both teams. Called from SimWorld::Step.
void UpdateAllTeamStrategy(FSimWorldState& s);

}  // namespace edge26
```

- [ ] **Step 2: Write the cpp with stub bodies**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "AI/TeamStrategy.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"

namespace edge26 {

int32_t MatchSecondsRemaining(const FSimWorldState& s) {
    int32_t elapsed = (int32_t)(s.TickNumber / (uint32_t)kSimTickHz);
    int32_t remaining = kMatchTotalSeconds - elapsed;
    if (remaining < 0) remaining = 0;
    return remaining;
}

void UpdateTeamStrategy(FTeamPlan& Plan, const FSimWorldState&, int) {
    // Stub: defaults will be set in T6.2.
    (void)Plan;
}

void UpdateAllTeamStrategy(FSimWorldState& s) {
    for (int team = 0; team < 2; ++team) {
        UpdateTeamStrategy(s.Match.Plans[team], s, team);
    }
}

}  // namespace edge26
```

- [ ] **Step 3: Wire into `SimWorld::Step` at 2 Hz**

In `SimWorld.cpp`:

```cpp
#include "AI/TeamStrategy.h"
// ...
// (after UpdateSpatialFields, before UpdateAllUnits)
if (State.TickNumber % 25 == 0) {
    UpdateAllTeamStrategy(State);
}
```

- [ ] **Step 4: Build + commit**

```bash
cmake -S Source/Edge26SimStandalone -B build/sim -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -2
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Public/AI/TeamStrategy.h \
        Source/Edge26Sim/Private/AI/TeamStrategy.cpp \
        Source/Edge26Sim/Private/Sim/SimWorld.cpp
git commit -m "feat(ai): TeamStrategy scaffolding + MatchSecondsRemaining + 2Hz hook"
```

### Task T6.2 — Implement `UpdateTeamStrategy`

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/TeamStrategy.cpp`

- [ ] **Step 1: Replace the stub with the full body**

```cpp
// Helper: build a Fixed32 from a numerator/denominator pair without floats.
static constexpr Fixed32 F32FromFraction(int num, int denom) {
    return Fixed32::FromRaw((int32_t)((int64_t)num * Fixed32::One / denom));
}

void UpdateTeamStrategy(FTeamPlan& Plan, const FSimWorldState& s, int teamId) {
    const int    scoreDiff = (int)s.Match.Score[teamId] - (int)s.Match.Score[1 - teamId];
    const int32_t secsLeft = MatchSecondsRemaining(s);

    // Baseline 4-3-3 defaults.
    Plan.Mentality          = 0;
    Plan.LineHeightBias     = 0;
    Plan.PressIntensity     = 2;
    Plan.Tempo              = 2;
    Plan.BuildupStyle       = 1;        // mixed
    Plan.CounterAttackBias  = 1;
    Plan.PanicBias          = F32FromFraction(2, 10);    // 0.2
    Plan.HoldBias           = F32FromFraction(2, 10);    // 0.2
    Plan.MentalityShootBias = F32FromFraction(10, 10);   // 1.0

    // Trailing late: push everyone forward.
    if (scoreDiff < 0 && secsLeft < 30 * 60) {
        Plan.Mentality      = +2;
        Plan.LineHeightBias = +1;
        Plan.PressIntensity = 3;
        Plan.MentalityShootBias = F32FromFraction(15, 10);  // 1.5
    }
    // Leading late: drop deep, game-manage.
    else if (scoreDiff > 0 && secsLeft < 20 * 60) {
        Plan.Mentality      = -1;
        Plan.LineHeightBias = -1;
        Plan.PressIntensity = 1;
        Plan.Tempo          = 1;
        Plan.PanicBias      = F32FromFraction(5, 10);       // 0.5
        Plan.HoldBias       = F32FromFraction(7, 10);       // 0.7
    }
    // Drawn late: cautious push.
    else if (scoreDiff == 0 && secsLeft < 20 * 60) {
        Plan.Mentality = +1;
    }

    // Per-team personality (v0 hardcode; data-driven later).
    if (teamId == 0) {                  // home = possession
        Plan.BuildupStyle = 0;
        Plan.HoldBias     = F32FromFraction(3, 10);         // 0.3
    } else {                            // away = counter
        Plan.BuildupStyle = 2;
        Plan.CounterAttackBias = 3;
    }
}
```

- [ ] **Step 2: Build + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Private/AI/TeamStrategy.cpp
git commit -m "feat(ai): UpdateTeamStrategy — late-game mentality shifts + per-team personality"
```

### Task T6.3 — Test: late-game mentality flips for the trailing team

**Files:**
- Modify: `Source/Edge26SimStandalone/tests/test_snapshot.cpp`

- [ ] **Step 1: Append the test**

```cpp
TEST_CASE(AI_LateGameMentalityShift) {
    using namespace edge26;
    SimWorld w{1};
    auto& st = w.MutableState();

    // Home is trailing by 1 with 15 minutes left.
    st.Match.Score[0] = 0;
    st.Match.Score[1] = 1;
    // TickNumber such that secsLeft = 15 minutes (900s)
    // elapsed = 90*60 - 900 = 4500 seconds = 4500 * 50 = 225000 ticks.
    st.TickNumber = 225000;

    FInputFrame f{};
    f.TickNumber = 225000;
    w.Step(f);

    TEST_EXPECT_EQ((int)st.Match.Plans[0].Mentality, 2);    // home pushed up
    TEST_EXPECT_EQ((int)st.Match.Plans[1].Mentality, -1);   // away dropping (leading)
    return 0;
}
```

Add `TEST_RUN(AI_LateGameMentalityShift);`.

- [ ] **Step 2: Build + run + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "LateGameMentality|Self-test"
./Scripts/lint_sim.sh
git add Source/Edge26SimStandalone/tests/test_snapshot.cpp
git commit -m "test(ai): late-game mentality shift for trailing/leading teams"
```

### Task T6.4 — Regenerate baselines

- [ ] **Step 1: Update baselines**

```bash
./Scripts/update_determinism_baseline.sh
./Scripts/check_determinism.sh 2>&1 | tail -3
```

Expected: `PASS: all determinism checks`.

- [ ] **Step 2: Commit**

```bash
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): regenerate baselines after Layer A team strategy"
```

### Task T6.5 — Mark M6 complete

- [ ] **Step 1: Update PROGRESS.md** (tick M6; advance status to M7; activity log entry)

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): M6 complete; advance to M7 (offside enforcement)"
```

---

## M7 — Offside enforcement

Two paths feed the offside system:
1. Layer C already filters offside cells in `MakeRunForward` (done in M3).
2. When a Pass kick releases, we check the intended receiver against the opponents' `OffsideLineY` at that tick. If past line, flag a 30-tick grace window. After the window expires OR the receiver controls the ball (whichever first), award possession to the defending team.

### Task T7.1 — Flag pending offside in the Pass kick path

**Files:**
- Modify: `Source/Edge26Sim/Private/Sim/SimWorld.cpp`

- [ ] **Step 1: Add the helper near `UpdatePossession`**

```cpp
static bool CellIsOffside(FixedVec3 worldPos, int attackingTeam, const FMatchState& m)
{
    Fixed64 posX = worldPos.X;
    Fixed64 lineX = m.OffsideLineY[1 - attackingTeam];
    return (attackingTeam == 0) ? (posX.Raw > lineX.Raw) : (posX.Raw < lineX.Raw);
}
```

(We named it `OffsideLineY` to match the spec struct field, but the value is the X-axis position because our up-pitch axis is X.)

- [ ] **Step 2: Flag in `MaybeApplyKick`'s Pass branch**

Where the Pass branch fires (after the impulse is applied), insert:

```cpp
if (buttons & (1<<1)) {
    // ... existing impulse application ...

    // Offside flag check: if the intended receiver is currently past opp's
    // offside line, set a 30-tick grace window.
    if (p.IntendedPassTarget < kSimPlayerCount) {
        const auto& mate = state.Players[p.IntendedPassTarget];
        if (CellIsOffside(mate.Position, p.TeamId, state.Match)) {
            state.Match.PendingOffsideCallTeam = (uint8_t)p.TeamId;
            state.Match.PendingOffsideCallTick = state.TickNumber;
        }
    }
}
```

(Adapt to your exact `MaybeApplyKick` signature — it now needs `state` not just `ball`. If you passed just the ball + player + match in M4, you'll need to widen to a full `FSimWorldState&` reference.)

- [ ] **Step 3: Build + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Private/Sim/SimWorld.cpp
git commit -m "feat(sim): flag pending offside when Pass releases to a receiver past line"
```

### Task T7.2 — `ResolveOffsideCall` implementation

**Files:**
- Modify: `Source/Edge26Sim/Private/Sim/SimWorld.cpp`

- [ ] **Step 1: Add `ResolveOffsideCall`**

```cpp
static void ResolveOffsideCall(FSimWorldState& s)
{
    if (s.Match.PendingOffsideCallTeam == 0xFF) return;

    const uint8_t attackingTeam = s.Match.PendingOffsideCallTeam;
    const uint32_t startedTick  = s.Match.PendingOffsideCallTick;
    const uint32_t graceTicks   = 30;   // 0.6 s

    // Resolution trigger 1: grace expired.
    bool expired = (s.TickNumber >= startedTick + graceTicks);
    // Resolution trigger 2: the attacking team controls the ball (receiver picked it up).
    bool received = (s.Match.PossessionTeam == attackingTeam);

    if (expired || received) {
        // Award possession to the defending team.
        s.Match.PossessionTeam   = (uint8_t)(1 - attackingTeam);
        // Find nearest defending-team outfielder to the ball.
        int   bestIdx = 0xFF;
        Fixed64 bestSq = Fixed64::FromInt(99999999);
        for (int i = 0; i < kSimPlayerCount; ++i) {
            const auto& p = s.Players[i];
            if (p.TeamId != (uint8_t)(1 - attackingTeam)) continue;
            if (p.RoleId == (uint8_t)ERole::GK) continue;
            Fixed64 dSq = (p.Position.X - s.Ball.Position.X) * (p.Position.X - s.Ball.Position.X)
                        + (p.Position.Y - s.Ball.Position.Y) * (p.Position.Y - s.Ball.Position.Y);
            if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
        }
        s.Match.PossessionPlayer = (uint8_t)bestIdx;
        // Stop the ball (no set-piece restart in v0; just give it to defender).
        s.Ball.Velocity        = FixedVec3::Zero();
        s.Ball.AngularVelocity = FixedVec3::Zero();
        if (bestIdx != 0xFF) {
            s.Ball.Position = s.Players[bestIdx].Position;
        }
        // Clear the flag.
        s.Match.PendingOffsideCallTeam = 0xFF;
        s.Match.PendingOffsideCallTick = 0;
    }
}
```

- [ ] **Step 2: Wire it into `SimWorld::Step`**

After `MaybeGoalkeeperSave` (or right after the kick loop if GK isn't wired yet):

```cpp
// (after all MaybeApplyKick / MaybeGoalkeeperSave)
ResolveOffsideCall(State);
UpdatePossession(State);
```

(Ensure ResolveOffsideCall runs **before** `UpdatePossession` so the defending-team award sticks for the rest of the tick.)

- [ ] **Step 3: Initialize the flag in the constructor**

In `SimWorld::SimWorld(uint64_t seed)` — after the existing `memset` — explicitly set:

```cpp
State.Match.PendingOffsideCallTeam = 0xFF;
State.Match.PossessionTeam         = 0xFF;
State.Match.PossessionPlayer       = 0xFF;
State.Match.HumanControlledIndex   = 0;   // will be reset by host
```

The `memset` already zeros them, but `0xFF` is the sentinel we use for "no value"; setting it explicitly makes intent clear.

- [ ] **Step 4: Build + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Private/Sim/SimWorld.cpp
git commit -m "feat(sim): ResolveOffsideCall — 30-tick grace then defender ball"
```

### Task T7.3 — Test: pass to offside teammate → defender gains possession within 30 ticks

**Files:**
- Modify: `Source/Edge26SimStandalone/tests/test_snapshot.cpp`

- [ ] **Step 1: Append the test**

```cpp
TEST_CASE(Sim_OffsideFlagAndResolve) {
    using namespace edge26;
    SimWorld w{1};
    auto& st = w.MutableState();
    // Home defends -X; their offside line is at +1000 (10m past midline).
    // Park last home defender far back so offside line for away = ball.X.
    // Simpler: hand-set OffsideLineY[1] = 0 (away's line) and put receiver at +2000.
    st.Match.OffsideLineY[1] = Fixed64::FromInt(0);

    // Push everyone off pitch except carrier (away, idx 12), receiver (away, idx 13).
    for (int i = 0; i < kSimPlayerCount; ++i)
        st.Players[i].Position = FixedVec3{ Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0) };
    st.Players[12].TeamId = 1;
    st.Players[13].TeamId = 1;
    st.Players[12].RoleId = (uint8_t)ERole::CM;
    st.Players[13].RoleId = (uint8_t)ERole::ST;
    st.Players[12].Position = FixedVec3{ Fixed64::FromInt(-500), Fixed64::FromInt(0), Fixed64::FromInt(0) };
    st.Players[13].Position = FixedVec3{ Fixed64::FromInt(-2000), Fixed64::FromInt(0), Fixed64::FromInt(0) };

    // One home defender to receive after the call.
    st.Players[1].TeamId = 0;
    st.Players[1].RoleId = (uint8_t)ERole::CB;
    st.Players[1].Position = FixedVec3{ Fixed64::FromInt(-2200), Fixed64::FromInt(100), Fixed64::FromInt(0) };

    st.Match.PossessionTeam   = 1;
    st.Match.PossessionPlayer = 12;
    st.Match.HumanControlledIndex = 0xFF;
    st.Players[12].PendingButtons = (1<<1);     // fire pass
    st.Players[12].IntendedPassTarget = 13;
    st.Ball.Position = st.Players[12].Position;

    // Tick once: pass fires, offside flagged.
    FInputFrame f{};
    f.TickNumber = 1;
    w.Step(f);
    TEST_EXPECT_EQ(st.Match.PendingOffsideCallTeam, (uint8_t)1);

    // Tick ~35 more times — flag should resolve and home defender should have ball.
    for (int t = 2; t < 40; ++t) {
        f.TickNumber = (uint32_t)t;
        w.Step(f);
    }
    TEST_EXPECT_EQ(st.Match.PendingOffsideCallTeam, (uint8_t)0xFF);
    TEST_EXPECT_EQ(st.Match.PossessionTeam, (uint8_t)0);
    return 0;
}
```

Add `TEST_RUN(Sim_OffsideFlagAndResolve);`.

- [ ] **Step 2: Run + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "OffsideFlag|Self-test"
./Scripts/lint_sim.sh
git add Source/Edge26SimStandalone/tests/test_snapshot.cpp
git commit -m "test(sim): offside-flag-and-resolve covers 30-tick grace window"
```

### Task T7.4 — Regenerate baselines

- [ ] **Step 1: Update baselines**

```bash
./Scripts/update_determinism_baseline.sh
./Scripts/check_determinism.sh 2>&1 | tail -3
```

Expected: `PASS: all determinism checks`.

- [ ] **Step 2: Commit**

```bash
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): regenerate baselines after offside enforcement"
```

### Task T7.5 — Mark M7 complete

- [ ] **Step 1: Update PROGRESS.md** (tick M7; advance status to M8; activity log entry)

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): M7 complete; advance to M8 (simple goalkeeper AI)"
```

---

## M8 — Simple goalkeeper AI

GK has a dedicated path (NOT Layer C's intent loop). Two responsibilities:
1. `UpdateGoalkeeperAI` — pick the GK's target position each tick (3 tiers: stance / sweeper / save target).
2. `MaybeGoalkeeperSave` — between kicks and ball physics, intercept ball if within reach + moving toward goal.

### Task T8.1 — GK constants + helpers

**Files:**
- Create: `Source/Edge26Sim/Public/AI/GoalkeeperAI.h`
- Create: `Source/Edge26Sim/Private/AI/GoalkeeperAI.cpp`
- Modify: `Source/Edge26Sim/Public/Sim/SimConstants.h` (or wherever pitch/goal constants live)

- [ ] **Step 1: Add constants**

In `SimConstants.h` (or wherever `SimConst::` constants are defined), append:

```cpp
namespace SimConst {
    // GK reach radius — a shot needs to clear this to score.
    constexpr Fixed64 kGKReachRadius = Fixed64::FromInt(180);   // 1.8m
    // Half-width of goal mouth (cm).
    constexpr Fixed64 kGoalHalfWidth = Fixed64::FromInt(366);   // 7.32m / 2
    // GK stance distance off goal line.
    constexpr Fixed64 kGKStanceOffset = Fixed64::FromInt(100);  // 1m
    // Box (penalty area) half-extent on Y axis (from goal line forward).
    constexpr Fixed64 kBoxDepth = Fixed64::FromInt(1650);       // 16.5m
}
```

(If `SimConst::PitchHalfLen` is the up-pitch half-extent, the goal sits at `±PitchHalfLen` on the X axis.)

- [ ] **Step 2: Write `GoalkeeperAI.h`**

```cpp
// Copyright Edge26. All Rights Reserved.
#pragma once
#include <cstdint>

namespace edge26 {

struct FSimWorldState;
struct FSimPlayerState;
struct FSimBallState;

// Layer-C-equivalent for the GK. Sets the GK's AITargetPosition + FacingTarget.
// Called from SimWorld::Step's player loop instead of UpdatePlayerAI for
// the two GKs.
void UpdateGoalkeeperAI(FSimPlayerState& gk, const FSimWorldState& s, int gkIdx);

// Runs between MaybeApplyKick and ball physics. If a ball heading toward goal
// is within either GK's reach, intercept it.
void MaybeGoalkeeperSave(FSimBallState& b, FSimWorldState& s);

// Helper used by host (player-switch policy etc.).
int  FindGoalkeeper(const FSimWorldState& s, int teamId);

}  // namespace edge26
```

- [ ] **Step 3: Write `GoalkeeperAI.cpp` skeleton (stubs)**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "AI/GoalkeeperAI.h"
#include "AI/Roles.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"
#include "Sim/SimConstants.h"
#include "Math/Atan2.h"

namespace edge26 {

int FindGoalkeeper(const FSimWorldState& s, int teamId) {
    for (int i = 0; i < kSimPlayerCount; ++i) {
        if (s.Players[i].TeamId == (uint8_t)teamId
            && s.Players[i].RoleId == (uint8_t)ERole::GK) return i;
    }
    return -1;
}

void UpdateGoalkeeperAI(FSimPlayerState&, const FSimWorldState&, int) {
    // Fleshed out in T8.2.
}

void MaybeGoalkeeperSave(FSimBallState&, FSimWorldState&) {
    // Fleshed out in T8.3.
}

}  // namespace edge26
```

- [ ] **Step 4: Build + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Public/AI/GoalkeeperAI.h \
        Source/Edge26Sim/Private/AI/GoalkeeperAI.cpp \
        Source/Edge26Sim/Public/Sim/SimConstants.h
git commit -m "feat(ai): GoalkeeperAI scaffolding + reach/goal constants"
```

### Task T8.2 — `UpdateGoalkeeperAI` 3-tier target picker

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/GoalkeeperAI.cpp`

- [ ] **Step 1: Replace the stub**

```cpp
static Fixed64 SignForTeam(int teamId) {
    return (teamId == 0) ? Fixed64::FromInt(1) : Fixed64::FromInt(-1);
}

// Returns true if any opponent of `gkTeam` is within `r` cm of the ball.
static bool OpponentNearBall(const FSimWorldState& s, int gkTeam, Fixed64 r) {
    Fixed64 rSq = r * r;
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId == (uint8_t)gkTeam) continue;
        Fixed64 dSq = (p.Position.X - s.Ball.Position.X) * (p.Position.X - s.Ball.Position.X)
                    + (p.Position.Y - s.Ball.Position.Y) * (p.Position.Y - s.Ball.Position.Y);
        if (dSq.Raw < rSq.Raw) return true;
    }
    return false;
}

void UpdateGoalkeeperAI(FSimPlayerState& gk, const FSimWorldState& s, int gkIdx)
{
    gk.PendingButtons = 0;
    gk.CurrentIntent = (uint8_t)EIntent::HoldPosition;

    const int   teamId   = gk.TeamId;
    const Fixed64 sign   = SignForTeam(teamId);
    const Fixed64 goalX  = -SimConst::PitchHalfLen * sign;  // team's own goal X
    const Fixed64 stanceX = goalX + sign * SimConst::kGKStanceOffset;

    // Tier 1 — Goal-line stance, leaning toward ball laterally.
    Fixed64 leanY = s.Ball.Position.Y;
    if (leanY.Raw >  SimConst::kGoalHalfWidth.Raw) leanY = SimConst::kGoalHalfWidth;
    if (leanY.Raw < -SimConst::kGoalHalfWidth.Raw) leanY = Fixed64::FromInt(0) - SimConst::kGoalHalfWidth;
    FixedVec3 target { stanceX, leanY, Fixed64::FromInt(0) };

    // Tier 2 — Sweeper: ball in own box AND no opponent within 3m of ball.
    Fixed64 ballSidedness = (s.Ball.Position.X - goalX) * sign;
    bool ballInBox = (ballSidedness.Raw > 0) && (ballSidedness.Raw < SimConst::kBoxDepth.Raw);
    if (ballInBox && !OpponentNearBall(s, teamId, Fixed64::FromInt(300))) {
        target = s.Ball.Position;
    }

    // Tier 3 — Save target: ball moving toward our goal (X velocity toward goal).
    Fixed64 ballVxToGoal = s.Ball.Velocity.X * (-sign);
    bool incoming = (ballVxToGoal.Raw > Fixed64::FromInt(200).Raw);   // > 2 m/s toward goal
    if (incoming) {
        // Predict ball Y when it reaches goal line.
        Fixed64 dx = goalX - s.Ball.Position.X;
        // dt = dx / vx; clamp to a tick budget so we don't extrapolate forever.
        if (s.Ball.Velocity.X.Raw != 0) {
            Fixed64 dt = Fixed64::FromRaw((dx.Raw * Fixed64::One) / s.Ball.Velocity.X.Raw);
            Fixed64 predY = s.Ball.Position.Y + Fixed64::FromRaw(
                (s.Ball.Velocity.Y.Raw * dt.Raw) / Fixed64::One);
            if (predY.Raw >  SimConst::kGoalHalfWidth.Raw) predY = SimConst::kGoalHalfWidth;
            if (predY.Raw < -SimConst::kGoalHalfWidth.Raw) predY = Fixed64::FromInt(0) - SimConst::kGoalHalfWidth;
            target = FixedVec3{ stanceX, predY, Fixed64::FromInt(0) };
        }
    }

    gk.AITargetPosition = target;
    gk.FacingTarget = SimMath::Atan2(
        s.Ball.Position.Y - gk.Position.Y,
        s.Ball.Position.X - gk.Position.X);
    (void)gkIdx;
}
```

- [ ] **Step 2: Build + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Private/AI/GoalkeeperAI.cpp
git commit -m "feat(ai): UpdateGoalkeeperAI — stance / sweeper / save-prediction tiers"
```

### Task T8.3 — `MaybeGoalkeeperSave` interception

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/GoalkeeperAI.cpp`

- [ ] **Step 1: Add `BallMovingTowardGoal` helper + replace the `MaybeGoalkeeperSave` stub**

```cpp
static bool BallMovingTowardGoal(const FSimBallState& b, int gkTeam) {
    // Goal is at -X for team 0 (they defend +X end of pitch). Actually we
    // chose: home defends -X end, attacks +X. So home GK is at -PitchHalfLen.
    // Therefore: ball moving toward home goal = ball.Velocity.X < 0.
    Fixed64 sign = (gkTeam == 0) ? Fixed64::FromInt(1) : Fixed64::FromInt(-1);
    return (b.Velocity.X * (-sign)).Raw > 0;
}

void MaybeGoalkeeperSave(FSimBallState& b, FSimWorldState& s)
{
    for (int t = 0; t < 2; ++t) {
        int gkIdx = FindGoalkeeper(s, t);
        if (gkIdx < 0) continue;
        const FSimPlayerState& gk = s.Players[gkIdx];

        FixedVec3 toBall = b.Position - gk.Position;
        Fixed64 dist = SimMath::Sqrt(toBall.X * toBall.X + toBall.Y * toBall.Y);
        if (dist.Raw > SimConst::kGKReachRadius.Raw) continue;
        if (!BallMovingTowardGoal(b, t)) continue;

        // Save: zero ball velocity, GK gains possession.
        b.Velocity        = FixedVec3::Zero();
        b.AngularVelocity = FixedVec3::Zero();
        b.Position        = gk.Position + FixedVec3{
            Fixed64::FromInt(20), Fixed64::FromInt(0), Fixed64::FromInt(50)
        };
        s.Match.PossessionTeam   = (uint8_t)t;
        s.Match.PossessionPlayer = (uint8_t)gkIdx;
        return;     // first save in linear order wins (deterministic).
    }
}
```

- [ ] **Step 2: Build + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Private/AI/GoalkeeperAI.cpp
git commit -m "feat(ai): MaybeGoalkeeperSave — reach-radius intercept + possession assign"
```

### Task T8.4 — Wire GK paths into `SimWorld::Step`

**Files:**
- Modify: `Source/Edge26Sim/Private/Sim/SimWorld.cpp`

- [ ] **Step 1: Add the include + route GKs to dedicated AI**

```cpp
#include "AI/GoalkeeperAI.h"
```

In the Layer C player loop in `Step`, replace:

```cpp
for (int i = 0; i < kSimPlayerCount; ++i) {
    FSimPlayerState& p = State.Players[i];
    if (i == State.Match.HumanControlledIndex) continue;
    UpdatePlayerAI(p, State, i);
}
```

with:

```cpp
for (int i = 0; i < kSimPlayerCount; ++i) {
    FSimPlayerState& p = State.Players[i];
    if (i == State.Match.HumanControlledIndex) continue;
    if (p.RoleId == (uint8_t)ERole::GK) {
        UpdateGoalkeeperAI(p, State, i);
    } else {
        UpdatePlayerAI(p, State, i);
    }
}
```

- [ ] **Step 2: Call `MaybeGoalkeeperSave` between kicks and offside resolve**

```cpp
// (existing) for-loop calling MaybeApplyKick
MaybeGoalkeeperSave(State.Ball, State);  // NEW (M8 T8.4)
ResolveOffsideCall(State);
UpdatePossession(State);
```

- [ ] **Step 3: Test — shot toward home goal at long range is saved by GK**

In `test_snapshot.cpp`:

```cpp
TEST_CASE(Sim_GKSavesIncomingShot) {
    using namespace edge26;
    SimWorld w{1};
    auto& st = w.MutableState();
    // Park everyone off pitch.
    for (int i = 0; i < kSimPlayerCount; ++i)
        st.Players[i].Position = FixedVec3{ Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0) };
    // Home GK at -PitchHalfLen + 100 (own goal-line stance).
    st.Players[0].TeamId = 0;
    st.Players[0].RoleId = (uint8_t)ERole::GK;
    st.Players[0].Position = FixedVec3{
        -SimConst::PitchHalfLen + Fixed64::FromInt(100),
        Fixed64::FromInt(0), Fixed64::FromInt(0)
    };
    // Ball coming straight at the GK from 50cm out.
    st.Ball.Position = st.Players[0].Position + FixedVec3{
        Fixed64::FromInt(50), Fixed64::FromInt(0), Fixed64::FromInt(0)
    };
    st.Ball.Velocity = FixedVec3{
        Fixed64::FromInt(-1500), Fixed64::FromInt(0), Fixed64::FromInt(0)
    };  // -15 m/s

    FInputFrame f{};
    f.TickNumber = 1;
    w.Step(f);
    // Save should have zeroed velocity and assigned possession to GK.
    TEST_EXPECT_EQ(st.Ball.Velocity.X.Raw, (int64_t)0);
    TEST_EXPECT_EQ(st.Match.PossessionTeam,   (uint8_t)0);
    TEST_EXPECT_EQ(st.Match.PossessionPlayer, (uint8_t)0);
    return 0;
}
```

Add `TEST_RUN(Sim_GKSavesIncomingShot);`.

- [ ] **Step 4: Build + run + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "GKSaves|Self-test"
./Scripts/lint_sim.sh
git add -A
git commit -m "feat(ai): wire goalkeeper paths into SimWorld::Step + save test"
```

### Task T8.5 — Regenerate baselines

- [ ] **Step 1: Update baselines**

```bash
./Scripts/update_determinism_baseline.sh
./Scripts/check_determinism.sh 2>&1 | tail -3
```

Expected: `PASS: all determinism checks`.

- [ ] **Step 2: Commit**

```bash
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): regenerate baselines after GK AI"
```

### Task T8.6 — Mark M8 complete

- [ ] **Step 1: Update PROGRESS.md** (tick M8; advance status to M9; activity log entry)

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): M8 complete; advance to M9 (player switching)"
```

---

## M9 — Player switching

Sim-side pure function `ChooseHumanControlled(state, humanTeam)` picks the human's pawn each tick. Manual switch (button) advances to the next-nearest teammate and arms a 0.5 s (25-tick) cooldown that suppresses auto-switch. The UE5 host calls `PlayerController->Possess()` when the index changes.

`FMatchState` already has a 32-bit `_pad1` field at offset 12. We rename it to `LastManualSwitchTick` to hold the cooldown state without growing the struct (still 184 B, static_assert preserved).

### Task T9.1 — Rename `_pad1` → `LastManualSwitchTick`, add Switch bit

**Files:**
- Modify: `Source/Edge26Sim/Public/Sim/MatchState.h`
- Modify: `Source/Edge26Sim/Public/Sim/InputFrame.h` (or wherever button bit constants live)

- [ ] **Step 1: Rename `_pad1` to `LastManualSwitchTick` in `FMatchState`**

In `MatchState.h`, change:

```cpp
uint32_t  _pad1;
```

to:

```cpp
uint32_t  LastManualSwitchTick;   // tick of last manual-switch input (cooldown)
```

The `static_assert(sizeof(FMatchState) == 184)` should still hold (same byte count).

- [ ] **Step 2: Add a `Switch` button bit**

In whichever header defines the button bits (often `InputFrame.h` or `SimConstants.h`), append:

```cpp
namespace SimButton {
    constexpr uint8_t Sprint = 1 << 0;
    constexpr uint8_t Pass   = 1 << 1;
    constexpr uint8_t Shoot  = 1 << 2;
    constexpr uint8_t Chip   = 1 << 3;
    constexpr uint8_t Switch = 1 << 4;   // NEW (M9)
}
```

(If you already use `1<<0` literals in the codebase, define just the new `Switch` constant and use `SimButton::Switch` going forward.)

- [ ] **Step 3: Initialize the field in `SimWorld::SimWorld`**

After the existing `memset`, append:

```cpp
State.Match.LastManualSwitchTick = 0;
```

(The memset already zeros it; this is documentation.)

- [ ] **Step 4: Build + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Public/Sim/MatchState.h \
        Source/Edge26Sim/Public/Sim/InputFrame.h \
        Source/Edge26Sim/Private/Sim/SimWorld.cpp
git commit -m "feat(sim): rename FMatchState._pad1 -> LastManualSwitchTick + add Switch button bit"
```

### Task T9.2 — `ChooseHumanControlled` pure function

**Files:**
- Create: `Source/Edge26Sim/Public/AI/Switching.h`
- Create: `Source/Edge26Sim/Private/AI/Switching.cpp`

- [ ] **Step 1: Write the header**

```cpp
// Copyright Edge26. All Rights Reserved.
#pragma once
#include <cstdint>

namespace edge26 {

struct FSimWorldState;

// Returns the index of the player the human should currently control.
// Policy: if humanTeam has possession, return the carrier. Otherwise return
// the nearest non-GK teammate to the ball. Tie-break: lower index wins.
// Returns -1 only if humanTeam has no outfield players (never in v0).
int ChooseHumanControlled(const FSimWorldState& s, int humanTeam);

// Pick the next-nearest teammate to the ball for manual-switch cycling.
// Skips the currently-controlled player + the GK. Returns -1 if none found.
int NextSwitchTarget(const FSimWorldState& s, int humanTeam, int currentIdx);

}  // namespace edge26
```

- [ ] **Step 2: Write the cpp**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "AI/Switching.h"
#include "AI/Roles.h"
#include "Sim/WorldState.h"
#include "Sim/MatchState.h"

namespace edge26 {

int ChooseHumanControlled(const FSimWorldState& s, int humanTeam)
{
    // 1. Possession on our team → carrier.
    if (s.Match.PossessionTeam == (uint8_t)humanTeam
        && s.Match.PossessionPlayer != 0xFF)
    {
        return (int)s.Match.PossessionPlayer;
    }

    // 2. Nearest non-GK teammate to ball.
    int   bestIdx = -1;
    Fixed64 bestSq = Fixed64::FromInt(99999999);
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId != (uint8_t)humanTeam) continue;
        if (p.RoleId == (uint8_t)ERole::GK) continue;
        Fixed64 dSq = (p.Position.X - s.Ball.Position.X) * (p.Position.X - s.Ball.Position.X)
                    + (p.Position.Y - s.Ball.Position.Y) * (p.Position.Y - s.Ball.Position.Y);
        if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
    }
    return bestIdx;
}

int NextSwitchTarget(const FSimWorldState& s, int humanTeam, int currentIdx)
{
    int   bestIdx = -1;
    Fixed64 bestSq = Fixed64::FromInt(99999999);
    for (int i = 0; i < kSimPlayerCount; ++i) {
        if (i == currentIdx) continue;
        const auto& p = s.Players[i];
        if (p.TeamId != (uint8_t)humanTeam) continue;
        if (p.RoleId == (uint8_t)ERole::GK) continue;
        Fixed64 dSq = (p.Position.X - s.Ball.Position.X) * (p.Position.X - s.Ball.Position.X)
                    + (p.Position.Y - s.Ball.Position.Y) * (p.Position.Y - s.Ball.Position.Y);
        if (dSq.Raw < bestSq.Raw) { bestSq = dSq; bestIdx = i; }
    }
    return bestIdx;
}

}  // namespace edge26
```

- [ ] **Step 3: Test the pure function**

In `test_snapshot.cpp`:

```cpp
TEST_CASE(Sim_ChooseHumanControlled_Carrier) {
    using namespace edge26;
    SimWorld w{1};
    auto& st = w.MutableState();
    st.Match.PossessionTeam   = 0;
    st.Match.PossessionPlayer = 7;
    TEST_EXPECT_EQ(ChooseHumanControlled(st, 0), 7);
    return 0;
}
TEST_CASE(Sim_ChooseHumanControlled_NoPossession_PicksNearest) {
    using namespace edge26;
    SimWorld w{1};
    auto& st = w.MutableState();
    st.Match.PossessionTeam = 0xFF;
    st.Match.PossessionPlayer = 0xFF;
    // Default constructed positions are zero; pick a deterministic spot
    // and put one home outfielder near the ball.
    for (int i = 0; i < kSimPlayerCount; ++i)
        st.Players[i].Position = FixedVec3{ Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0) };
    st.Players[3].TeamId = 0;
    st.Players[3].RoleId = (uint8_t)ERole::CM;
    st.Players[3].Position = FixedVec3{ Fixed64::FromInt(100), Fixed64::FromInt(0), Fixed64::FromInt(0) };
    st.Ball.Position = FixedVec3{ Fixed64::FromInt(0), Fixed64::FromInt(0), Fixed64::FromInt(0) };
    TEST_EXPECT_EQ(ChooseHumanControlled(st, 0), 3);
    return 0;
}
```

Add both `TEST_RUN`s.

- [ ] **Step 4: Build + run + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "ChooseHumanControlled|Self-test"
./Scripts/lint_sim.sh
git add Source/Edge26Sim/Public/AI/Switching.h \
        Source/Edge26Sim/Private/AI/Switching.cpp \
        Source/Edge26SimStandalone/tests/test_snapshot.cpp
git commit -m "feat(ai): ChooseHumanControlled + NextSwitchTarget pure-function policies"
```

### Task T9.3 — Wire auto-switch + manual-switch into `SimWorld::Step`

**Files:**
- Modify: `Source/Edge26Sim/Private/Sim/SimWorld.cpp`

- [ ] **Step 1: Add `ApplySwitching` near the bottom of the step**

```cpp
#include "AI/Switching.h"

static void ApplySwitching(FSimWorldState& s, const FInputFrame& frame, int humanTeam)
{
    // Manual switch — button edge: advance to next-nearest, set cooldown.
    if (frame.Buttons & (1 << 4)) {
        int nxt = NextSwitchTarget(s, humanTeam, s.Match.HumanControlledIndex);
        if (nxt >= 0) {
            s.Match.HumanControlledIndex = (uint8_t)nxt;
            s.Match.LastManualSwitchTick = s.TickNumber;
        }
        return;     // skip auto-switch this tick
    }
    // Auto-switch suppressed during 25-tick cooldown after a manual switch.
    const uint32_t kCooldownTicks = 25;
    if (s.TickNumber < s.Match.LastManualSwitchTick + kCooldownTicks) return;

    int target = ChooseHumanControlled(s, humanTeam);
    if (target >= 0) s.Match.HumanControlledIndex = (uint8_t)target;
}
```

- [ ] **Step 2: Call it at the end of `Step`**

Right before `Step` returns (after `StepPlayer` loop):

```cpp
// Auto/manual switching for the human team.
// HumanTeam in v0 is always 0 (home). Future: read from match config.
ApplySwitching(State, frame, /*humanTeam*/ 0);
```

- [ ] **Step 3: Test cooldown gating**

In `test_snapshot.cpp`:

```cpp
TEST_CASE(Sim_ManualSwitchSuppressesAutoSwitch) {
    using namespace edge26;
    SimWorld w{1};
    auto& st = w.MutableState();
    // Two home outfielders at different distances to ball; possession unowned.
    for (int i = 0; i < kSimPlayerCount; ++i)
        st.Players[i].Position = FixedVec3{ Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0) };
    st.Players[2].TeamId = 0; st.Players[2].RoleId = (uint8_t)ERole::CM;
    st.Players[2].Position = FixedVec3{ Fixed64::FromInt(0), Fixed64::FromInt(0), Fixed64::FromInt(0) };
    st.Players[3].TeamId = 0; st.Players[3].RoleId = (uint8_t)ERole::CM;
    st.Players[3].Position = FixedVec3{ Fixed64::FromInt(500), Fixed64::FromInt(0), Fixed64::FromInt(0) };
    st.Match.HumanControlledIndex = 2;
    st.Match.PossessionTeam   = 0xFF;
    st.Match.PossessionPlayer = 0xFF;

    // Manual switch: should jump from 2 → 3 (next nearest is player 3).
    FInputFrame f{};
    f.TickNumber = 100;
    f.Buttons    = (1 << 4);
    w.Step(f);
    TEST_EXPECT_EQ(st.Match.HumanControlledIndex, (uint8_t)3);

    // Next tick (no button) — auto-switch would normally return 2 (nearer to ball)
    // but cooldown should suppress it.
    f.Buttons    = 0;
    f.TickNumber = 101;
    w.Step(f);
    TEST_EXPECT_EQ(st.Match.HumanControlledIndex, (uint8_t)3);

    // After cooldown (25 ticks), auto-switch reasserts.
    for (int t = 102; t < 130; ++t) {
        f.TickNumber = (uint32_t)t;
        w.Step(f);
    }
    TEST_EXPECT_EQ(st.Match.HumanControlledIndex, (uint8_t)2);
    return 0;
}
```

Add `TEST_RUN(Sim_ManualSwitchSuppressesAutoSwitch);`.

- [ ] **Step 4: Build + run + commit**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
./build/sim/edge26_sim_replay --self-test 2>&1 | grep -E "ManualSwitch|Self-test"
./Scripts/lint_sim.sh
git add -A
git commit -m "feat(sim): ApplySwitching at end of Step — auto-switch + 25-tick manual cooldown"
```

### Task T9.4 — Add IA_Switch input action + IMC binding

**Files:**
- Create: `Content/Input/Actions/IA_Switch.uasset` (via Python)
- Modify: `Content/Input/IMC_Player.uasset` (via Python — add IA_Switch trigger)

- [ ] **Step 1: Add a Python script that creates `IA_Switch` + binds it**

Create `Scripts/editor/add_switch_input.py`:

```python
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.InputActionFactory()
ia_path = "/Game/Input/Actions/IA_Switch"
if not unreal.EditorAssetLibrary.does_asset_exist(ia_path):
    asset_tools.create_asset(
        asset_name="IA_Switch",
        package_path="/Game/Input/Actions",
        asset_class=unreal.InputAction,
        factory=factory,
    )
ia = unreal.EditorAssetLibrary.load_asset(ia_path)
ia.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
unreal.EditorAssetLibrary.save_loaded_asset(ia)

imc = unreal.EditorAssetLibrary.load_asset("/Game/Input/IMC_Player")
mappings = imc.get_editor_property("mappings")
# Skip if already present.
for m in mappings:
    if m.action == ia:
        unreal.log("IA_Switch already bound in IMC_Player; skipping.")
        break
else:
    new_mapping = unreal.EnhancedActionKeyMapping()
    new_mapping.action = ia
    new_mapping.key = unreal.InputCoreLibrary.get_key_by_name("R")
    mappings.append(new_mapping)
    imc.set_editor_property("mappings", mappings)
    unreal.EditorAssetLibrary.save_loaded_asset(imc)
    unreal.log("Bound IA_Switch -> R in IMC_Player.")
```

- [ ] **Step 2: Run headless**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecutePythonScript="$PWD/Scripts/editor/add_switch_input.py" \
    -nullrhi -stdout -unattended 2>&1 | tail -10
```

Expected: "Bound IA_Switch -> R in IMC_Player." in the log.

- [ ] **Step 3: Commit**

```bash
git add Scripts/editor/add_switch_input.py \
        Content/Input/Actions/IA_Switch.uasset \
        Content/Input/IMC_Player.uasset
git commit -m "feat(ue5): IA_Switch input action + R key binding in IMC_Player"
```

### Task T9.5 — `USimInputCollector` consumes `IA_Switch`

**Files:**
- Modify: `Source/Edge26/Public/Adapter/SimInputCollector.h`
- Modify: `Source/Edge26/Private/Adapter/SimInputCollector.cpp`
- Modify: `Source/Edge26/Public/Adapter/SimHostSubsystem.h` (add `SetSwitchPressed`)
- Modify: `Source/Edge26/Private/Adapter/SimHostSubsystem.cpp` (forward into the InputFrame)

- [ ] **Step 1: Add `IA_Switch` UPROPERTY + `OnSwitch` handler**

In `SimInputCollector.h`, add near the other `UInputAction*` properties:

```cpp
UPROPERTY(EditAnywhere, Category="Input")
UInputAction* IA_Switch = nullptr;
```

And declare:

```cpp
UFUNCTION() void OnSwitch(const FInputActionValue& Value);
```

- [ ] **Step 2: Constructor: load default IA_Switch**

In `SimInputCollector.cpp`, in the constructor, append after the IA_Chip loader:

```cpp
static ConstructorHelpers::FObjectFinder<UInputAction> IA_Switch_Finder(
    TEXT("/Game/Input/Actions/IA_Switch.IA_Switch"));
if (IA_Switch_Finder.Succeeded()) IA_Switch = IA_Switch_Finder.Object;
```

- [ ] **Step 3: Bind in `Bind(...)`**

After the existing `IA_Chip` binding:

```cpp
if (IA_Switch) Component->BindAction(IA_Switch, ETriggerEvent::Started, this, &USimInputCollector::OnSwitch);
```

- [ ] **Step 4: Implement `OnSwitch`**

```cpp
void USimInputCollector::OnSwitch(const FInputActionValue&)
{
    if (auto* H = HostFor(this)) H->SetButton(ControllerIndexOf(this), 1 << 4, true);
}
```

- [ ] **Step 5: SimHostSubsystem — forward the bit into the next sim input frame**

Confirm `SetButton(...)` already routes into `PendingInput.Buttons`. If yes, no further change needed: the bit appears in `frame.Buttons` next tick and `ApplySwitching` consumes it.

- [ ] **Step 6: Build editor**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 7: Commit**

```bash
git add Source/Edge26/Public/Adapter/SimInputCollector.h \
        Source/Edge26/Private/Adapter/SimInputCollector.cpp
git commit -m "feat(ue5): SimInputCollector binds IA_Switch -> Switch button bit"
```

### Task T9.6 — `USimHostSubsystem::Tick` re-possesses when `HumanControlledIndex` changes

**Files:**
- Modify: `Source/Edge26/Public/Adapter/SimHostSubsystem.h` (add `LastHumanControlledIndex` cached member)
- Modify: `Source/Edge26/Private/Adapter/SimHostSubsystem.cpp`

- [ ] **Step 1: Cache the last index in the subsystem**

Header, in `USimHostSubsystem`:

```cpp
private:
    uint8 LastHumanControlledIndex = 0xFF;
```

- [ ] **Step 2: After `Sim->Step()`, check + re-possess**

In `Tick(...)` (or wherever Sim->Step is called), after the step:

```cpp
const uint8 idxNow = Sim->GetState().Match.HumanControlledIndex;
if (idxNow != LastHumanControlledIndex && idxNow != 0xFF)
{
    if (auto* PC = GetWorld()->GetFirstPlayerController())
    {
        // Each AFootballerVisual knows its sim index via ControllerIndex.
        for (TActorIterator<AFootballerVisual> It(GetWorld()); It; ++It)
        {
            if ((uint8)It->ControllerIndex == idxNow)
            {
                PC->Possess(*It);
                break;
            }
        }
    }
    LastHumanControlledIndex = idxNow;
}
```

Add `#include "EngineUtils.h"` and `#include "Adapter/FootballerVisual.h"`.

- [ ] **Step 3: Build editor**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 4: Commit**

```bash
git add Source/Edge26/Public/Adapter/SimHostSubsystem.h \
        Source/Edge26/Private/Adapter/SimHostSubsystem.cpp
git commit -m "feat(ue5): SimHostSubsystem re-Possess on HumanControlledIndex change"
```

### Task T9.7 — Regenerate baselines + mark M9 complete

- [ ] **Step 1: Regenerate**

```bash
./Scripts/update_determinism_baseline.sh
./Scripts/check_determinism.sh 2>&1 | tail -3
```

Expected: `PASS: all determinism checks`.

- [ ] **Step 2: Commit**

```bash
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): regenerate baselines after player switching"
```

- [ ] **Step 3: Update PROGRESS.md** (tick M9; advance status to M10)

- [ ] **Step 4: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): M9 complete; advance to M10 (AI debug overlay)"
```

---

## M10 — AI debug overlay

Pure render-side, in `Edge26` module. `AAIDebugRenderer` Ticks each frame, reads the latest `FSimWorldState` snapshot via `USimHostSubsystem::GetState()`, and draws three overlays under console-toggleable flags:
- Heatmap of one `ESpatialField` (`Space`, `DefCoverage`, `Lane`, `PassReception`, `Threat`) for one team.
- Intent arrow per player to its `AITargetPosition`, color-coded by intent, labeled with role + intent.
- Horizontal offside line for each team at `OffsideLineY`.

Wrapped in `#if !UE_BUILD_SHIPPING` so production builds drop it.

### Task T10.1 — `AAIDebugRenderer` skeleton + UProperty flags

**Files:**
- Create: `Source/Edge26/Public/Debug/AIDebugRenderer.h`
- Create: `Source/Edge26/Private/Debug/AIDebugRenderer.cpp`

- [ ] **Step 1: Write the header**

```cpp
// Copyright Edge26. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIDebugRenderer.generated.h"

UENUM(BlueprintType)
enum class ESpatialFieldDebug : uint8
{
    None          UMETA(DisplayName="None"),
    Space         UMETA(DisplayName="Space"),
    DefCoverage   UMETA(DisplayName="DefCoverage"),
    Lane          UMETA(DisplayName="LaneOccupancy"),
    PassReception UMETA(DisplayName="PassReception"),
    Threat        UMETA(DisplayName="Threat"),
};

UCLASS()
class EDGE26_API AAIDebugRenderer : public AActor
{
    GENERATED_BODY()

public:
    AAIDebugRenderer();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category="AI Debug")
    ESpatialFieldDebug ActiveField = ESpatialFieldDebug::None;

    UPROPERTY(EditAnywhere, Category="AI Debug")
    int32 TeamPerspective = 0;       // 0 = home, 1 = away, -1 = combined

    UPROPERTY(EditAnywhere, Category="AI Debug")
    bool bShowIntentArrows = false;

    UPROPERTY(EditAnywhere, Category="AI Debug")
    bool bShowOffsideLines = false;
};
```

- [ ] **Step 2: Write the cpp skeleton**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "Debug/AIDebugRenderer.h"

AAIDebugRenderer::AAIDebugRenderer()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AAIDebugRenderer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
    // Filled in T10.2/T10.3/T10.4.
#endif
}
```

- [ ] **Step 3: Build editor + commit**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
git add Source/Edge26/Public/Debug/AIDebugRenderer.h \
        Source/Edge26/Private/Debug/AIDebugRenderer.cpp
git commit -m "feat(ue5): AAIDebugRenderer skeleton + ESpatialFieldDebug enum"
```

### Task T10.2 — Heatmap rendering (DrawDebugBox per cell)

**Files:**
- Modify: `Source/Edge26/Private/Debug/AIDebugRenderer.cpp`

- [ ] **Step 1: Implement the heatmap in `Tick`**

Inside the `#if !UE_BUILD_SHIPPING` block:

```cpp
#include "Adapter/SimHostSubsystem.h"
#include "AI/SpatialValueModel.h"
#include "Sim/WorldState.h"
#include "DrawDebugHelpers.h"

// inside Tick:
auto* Host = GetWorld() ? GetWorld()->GetSubsystem<USimHostSubsystem>() : nullptr;
if (!Host) return;
const edge26::FSimWorldState& s = Host->GetState();

// --- Heatmap ---
if (ActiveField != ESpatialFieldDebug::None)
{
    const int teamPick = (TeamPerspective < 0) ? 0 : TeamPerspective;
    const int fieldIdx = (int)ActiveField - 1;   // None=0 in enum → shift
    const auto& cells = s.Spatial.Cells[teamPick][fieldIdx];

    // Compute max value for normalization (cheap; 1768 cells).
    int32 maxRaw = 1;
    for (int c = 0; c < edge26::kPitchCells; ++c)
        if (cells[c].Raw > maxRaw) maxRaw = cells[c].Raw;
    if (maxRaw < 1) maxRaw = 1;

    // 105 m × 68 m pitch → 200 cm cell width.
    const float cellW = 200.0f;
    const float cellH = 200.0f;
    const FVector pitchOrigin{ -10500.f * 0.5f, -6800.f * 0.5f, 5.f };  // cm

    for (int c = 0; c < edge26::kPitchCells; ++c)
    {
        int cx = c % edge26::kPitchCellsX;
        int cy = c / edge26::kPitchCellsX;
        FVector center {
            pitchOrigin.X + (cx + 0.5f) * cellW,
            pitchOrigin.Y + (cy + 0.5f) * cellH,
            pitchOrigin.Z
        };
        float v = (float)cells[c].Raw / (float)maxRaw;  // 0..1
        if (v < 0) v = 0;
        if (v > 1) v = 1;
        FColor col = FColor((uint8)(0), (uint8)(255 * v), (uint8)(0), 96);
        DrawDebugBox(GetWorld(), center,
                     FVector(cellW * 0.45f, cellH * 0.45f, 1.0f),
                     col, false, -1.f, 0, 1.f);
    }
}
```

(Adjust `pitchOrigin` Z to clear the pitch mesh; tweak alpha for visibility.)

- [ ] **Step 2: Build editor + commit**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
git add Source/Edge26/Private/Debug/AIDebugRenderer.cpp
git commit -m "feat(ue5): AAIDebugRenderer heatmap — DrawDebugBox per spatial cell"
```

### Task T10.3 — Intent arrows + labels

**Files:**
- Modify: `Source/Edge26/Private/Debug/AIDebugRenderer.cpp`

- [ ] **Step 1: Append the intent-arrow block in `Tick`**

```cpp
if (bShowIntentArrows)
{
    static const FColor IntentColors[(int)edge26::EIntent::Count] = {
        FColor::Yellow,    // HoldPosition
        FColor::Green,     // MakeRunForward
        FColor::Cyan,      // DropToReceive
        FColor::Blue,      // ProvideWidth
        FColor::Red,       // Press
        FColor::Orange,    // TrackRunner
        FColor::Purple,    // HoldDefensiveLine
        FColor(255,200,0), // Pass
        FColor(255,80,80), // Shoot
        FColor(180,100,255),// Dribble
        FColor::White,     // Hold
        FColor(255,140,0), // Clear
    };
    static const TCHAR* IntentNames[(int)edge26::EIntent::Count] = {
        TEXT("Hold"), TEXT("RunFwd"), TEXT("Drop"), TEXT("Width"),
        TEXT("Press"), TEXT("Track"), TEXT("Line"),
        TEXT("Pass"), TEXT("Shoot"), TEXT("Drib"), TEXT("Hold"), TEXT("Clear"),
    };
    static const TCHAR* RoleNames[(int)edge26::ERole::Count] = {
        TEXT("GK"), TEXT("CB"), TEXT("FBL"), TEXT("FBR"),
        TEXT("CDM"), TEXT("CM"), TEXT("CAM"),
        TEXT("WL"), TEXT("WR"), TEXT("ST")
    };

    for (int i = 0; i < edge26::kSimPlayerCount; ++i)
    {
        const auto& p = s.Players[i];
        FVector from{
            (float)p.Position.X.ToFloat(),
            (float)p.Position.Y.ToFloat(),
            120.f
        };
        FVector to{
            (float)p.AITargetPosition.X.ToFloat(),
            (float)p.AITargetPosition.Y.ToFloat(),
            120.f
        };
        const uint8 ix = p.CurrentIntent;
        FColor col = (ix < (uint8)edge26::EIntent::Count) ? IntentColors[ix] : FColor::Black;
        DrawDebugDirectionalArrow(GetWorld(), from, to, 60.f, col, false, -1.f, 0, 2.f);

        FString label = FString::Printf(TEXT("%d %s %s"),
            i,
            (p.RoleId < (uint8)edge26::ERole::Count) ? RoleNames[p.RoleId] : TEXT("?"),
            (ix < (uint8)edge26::EIntent::Count)    ? IntentNames[ix]    : TEXT("?"));
        DrawDebugString(GetWorld(),
            from + FVector(0, 0, 60),
            label, nullptr, col, 0.0f, true);
    }
}
```

(If `Fixed64::ToFloat` doesn't exist, add it as an inline helper in the renderer cpp: `static float FixedToFloat(edge26::Fixed64 v) { return (float)v.Raw / (float)edge26::Fixed64::One; }` and use it instead.)

- [ ] **Step 2: Build + commit**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
git add Source/Edge26/Private/Debug/AIDebugRenderer.cpp
git commit -m "feat(ue5): AAIDebugRenderer intent arrows + role/intent labels"
```

### Task T10.4 — Offside lines

**Files:**
- Modify: `Source/Edge26/Private/Debug/AIDebugRenderer.cpp`

- [ ] **Step 1: Append the offside-line block in `Tick`**

```cpp
if (bShowOffsideLines)
{
    // Pitch half-width = 34m = 3400 cm.
    const float halfWidthCm = 3400.f;
    for (int team = 0; team < 2; ++team)
    {
        float xCm = (float)s.Match.OffsideLineY[team].ToFloat();
        FVector a{ xCm, -halfWidthCm, 30.f };
        FVector b{ xCm,  halfWidthCm, 30.f };
        FColor col = (team == 0) ? FColor::Cyan : FColor::Red;
        DrawDebugLine(GetWorld(), a, b, col, false, -1.f, 0, 6.f);
    }
}
```

- [ ] **Step 2: Build + commit**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
git add Source/Edge26/Private/Debug/AIDebugRenderer.cpp
git commit -m "feat(ue5): AAIDebugRenderer offside-line draws (cyan home, red away)"
```

### Task T10.5 — Console commands via cheat manager

**Files:**
- Create: `Source/Edge26/Public/Debug/Edge26CheatManager.h`
- Create: `Source/Edge26/Private/Debug/Edge26CheatManager.cpp`
- Modify: `Source/Edge26/Private/SoccerGameMode.cpp` (set `PlayerControllerClass`'s `CheatClass` to ours, or set `CheatManagerClass`)

- [ ] **Step 1: Write the cheat manager header**

```cpp
// Copyright Edge26. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "Edge26CheatManager.generated.h"

UCLASS()
class EDGE26_API UEdge26CheatManager : public UCheatManager
{
    GENERATED_BODY()
public:
    UFUNCTION(Exec) void edge26_ai_show_field(const FString& Field);
    UFUNCTION(Exec) void edge26_ai_team_perspective(int32 Team);
    UFUNCTION(Exec) void edge26_ai_intent_arrows(int32 On);
    UFUNCTION(Exec) void edge26_ai_offside_lines(int32 On);
};
```

(UE5 maps `edge26_ai_show_field MyName` to the console command `edge26.ai.show_field MyName` when registered via the cheat manager pattern — different versions vary; if `.` doesn't bind, this underscore-style still works as `edge26_ai_show_field`.)

- [ ] **Step 2: Implement the cheat manager**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "Debug/Edge26CheatManager.h"
#include "Debug/AIDebugRenderer.h"
#include "EngineUtils.h"
#include "Engine/World.h"

static AAIDebugRenderer* FindRenderer(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AAIDebugRenderer> It(World); It; ++It) return *It;
    // None present → spawn one.
    FActorSpawnParameters params;
    return World->SpawnActor<AAIDebugRenderer>(params);
}

void UEdge26CheatManager::edge26_ai_show_field(const FString& Field)
{
    auto* R = FindRenderer(GetWorld());
    if (!R) return;
    if      (Field == TEXT("None"))          R->ActiveField = ESpatialFieldDebug::None;
    else if (Field == TEXT("Space"))         R->ActiveField = ESpatialFieldDebug::Space;
    else if (Field == TEXT("DefCoverage"))   R->ActiveField = ESpatialFieldDebug::DefCoverage;
    else if (Field == TEXT("Lane"))          R->ActiveField = ESpatialFieldDebug::Lane;
    else if (Field == TEXT("PassReception")) R->ActiveField = ESpatialFieldDebug::PassReception;
    else if (Field == TEXT("Threat"))        R->ActiveField = ESpatialFieldDebug::Threat;
    else UE_LOG(LogTemp, Warning, TEXT("edge26.ai.show_field: unknown field '%s'"), *Field);
}
void UEdge26CheatManager::edge26_ai_team_perspective(int32 Team)
{
    if (auto* R = FindRenderer(GetWorld())) R->TeamPerspective = Team;
}
void UEdge26CheatManager::edge26_ai_intent_arrows(int32 On)
{
    if (auto* R = FindRenderer(GetWorld())) R->bShowIntentArrows = (On != 0);
}
void UEdge26CheatManager::edge26_ai_offside_lines(int32 On)
{
    if (auto* R = FindRenderer(GetWorld())) R->bShowOffsideLines = (On != 0);
}
```

- [ ] **Step 3: Wire the cheat manager class into the game mode's `PlayerControllerClass`**

In `SoccerGameMode.cpp` (or wherever the PlayerController class is configured), in the constructor:

```cpp
#include "Debug/Edge26CheatManager.h"
// ...
PlayerControllerClass = APlayerController::StaticClass();
// Set the cheat-manager class via a small subclass, OR set on the default PC at runtime:
```

If you don't have a custom PlayerController class, the simplest path is to set `CheatClass` on the default PC at `InitGame`:

```cpp
void ASoccerGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    // Allow the cheat manager to be created in shipping-non-cheat builds for now.
    // (Standard pattern: spawn at PostLogin if Cheats enabled.)
}
void ASoccerGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
#if !UE_BUILD_SHIPPING
    if (NewPlayer)
    {
        NewPlayer->CheatClass = UEdge26CheatManager::StaticClass();
        NewPlayer->EnableCheats();
    }
#endif
}
```

Add `virtual void InitGame(...)` and `virtual void PostLogin(APlayerController*)` to `SoccerGameMode.h`.

- [ ] **Step 4: Build editor + commit**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
git add Source/Edge26/Public/Debug/Edge26CheatManager.h \
        Source/Edge26/Private/Debug/Edge26CheatManager.cpp \
        Source/Edge26/Public/SoccerGameMode.h \
        Source/Edge26/Private/SoccerGameMode.cpp
git commit -m "feat(ue5): UEdge26CheatManager exec commands for AI debug overlay"
```

### Task T10.6 — Drop renderer into the level + verify in PIE

**Files:**
- Modify: `Content/Maps/L_Pitch.umap` (via Python — add an `AAIDebugRenderer` actor)

- [ ] **Step 1: Add a Python script to place the renderer**

Create `Scripts/editor/place_ai_debug_renderer.py`:

```python
import unreal
els = unreal.EditorLevelLibrary
existing = [a for a in els.get_all_level_actors() if a.get_class().get_name() == "AIDebugRenderer"]
if not existing:
    actor = els.spawn_actor_from_class(
        unreal.load_class(None, "/Script/Edge26.AIDebugRenderer"),
        unreal.Vector(0, 0, 0))
    actor.set_actor_label("AIDebugRenderer")
    els.save_current_level()
    unreal.log("Placed AIDebugRenderer in L_Pitch.")
else:
    unreal.log("AIDebugRenderer already present; skipping.")
```

- [ ] **Step 2: Run headless**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecutePythonScript="$PWD/Scripts/editor/place_ai_debug_renderer.py" \
    -nullrhi -stdout -unattended 2>&1 | tail -10
```

Expected: "Placed AIDebugRenderer in L_Pitch."

- [ ] **Step 3: Commit**

```bash
git add Scripts/editor/place_ai_debug_renderer.py Content/Maps/L_Pitch.umap
git commit -m "feat(ue5): place AAIDebugRenderer in L_Pitch via Python"
```

### Task T10.7 — Mark M10 complete

- [ ] **Step 1: Update PROGRESS.md** (tick M10; advance status to M11)

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): M10 complete; advance to M11 (CI baselines for 22-player streams)"
```

---

## M11 — CI baselines for 22-player streams

Phase 1's three replay streams (`basic`, `ball_only`, `rollback_torture`) were generated against the 2-player sim. They've been getting regenerated after each milestone in this plan, which keeps the determinism gate green but doesn't actually exercise the new code paths.

This milestone adds a fourth stream — `ai_match_30s` — that produces 1500 ticks (30 s) of real 22-player AI play, with the human controller driving a scripted pattern. It also documents the regenerate workflow so future maintainers don't get confused.

### Task T11.1 — Add the `ai_match_30s` stream to `replay_generator`

**Files:**
- Modify: `Source/Edge26SimStandalone/replay_generator/main.cpp` (or wherever the generator's stream registry lives)

- [ ] **Step 1: Locate the existing stream registry**

```bash
grep -rn "basic\|ball_only\|rollback_torture" Source/Edge26SimStandalone/ | head -20
```

Find the function (likely `WriteStream(name, ...)` or a registration table) that the three existing streams use.

- [ ] **Step 2: Add an `ai_match_30s` generator**

In the same file/module, append:

```cpp
// 30-second 22-player AI match. Human controls index 0. Scripted input:
// - Sprint held continuously.
// - WASD direction cycles every 100 ticks (right, up, left, down).
// - Pass button fires every 250 ticks.
static void WriteAIMatch30s(const std::string& outDir)
{
    using namespace edge26;
    SimWorld w{ /*seed*/ 0xA1FFA1FFA1FFA1FFull };

    const int kTicks   = 1500;
    std::ofstream input(outDir + "/ai_match_30s.input", std::ios::binary);

    auto writeFrame = [&](const FInputFrame& f) {
        input.write(reinterpret_cast<const char*>(&f), sizeof(f));
    };

    for (int t = 0; t < kTicks; ++t) {
        FInputFrame f{};
        f.TickNumber = (uint32_t)t;
        f.Buttons    = (1 << 0);              // Sprint held
        if ((t % 250) == 100) f.Buttons |= (1 << 1);    // Pass at t=100,350,...

        // 8-bit signed move: cycle direction every 100 ticks.
        int phase = (t / 100) % 4;
        int8_t mx = 0, my = 0;
        switch (phase) {
            case 0: mx = +96; my =   0; break;   // right
            case 1: mx =   0; my = +96; break;   // up
            case 2: mx = -96; my =   0; break;   // left
            case 3: mx =   0; my = -96; break;   // down
        }
        f.MoveX = mx;
        f.MoveY = my;
        writeFrame(f);
        w.Step(f);
    }
}
```

Register it in the same place where the other three streams are written. Use the same file layout the existing replays use (raw `FInputFrame` records back-to-back — same as M4).

- [ ] **Step 3: Build the generator**

```bash
cmake --build build/sim --parallel 2>&1 | tail -3
ls build/sim/replay_generator 2>/dev/null || ls build/sim 2>/dev/null | grep generator
```

Verify the generator binary exists.

- [ ] **Step 4: Generate the new stream**

```bash
./build/sim/replay_generator ai_match_30s Source/Edge26SimStandalone/tests/replays/
ls -la Source/Edge26SimStandalone/tests/replays/ai_match_30s*
```

Expected: `ai_match_30s.input` ~12 KB (1500 × 8 B).

- [ ] **Step 5: Commit the generator change** (baseline `.expected.hashes` comes in T11.4)

```bash
git add Source/Edge26SimStandalone/replay_generator/main.cpp \
        Source/Edge26SimStandalone/tests/replays/ai_match_30s.input
git commit -m "feat(test): ai_match_30s replay stream — 1500-tick 22-player scripted match"
```

### Task T11.2 — Update `check_determinism.sh` to include the new stream

**Files:**
- Modify: `Scripts/check_determinism.sh:43`

- [ ] **Step 1: Add `ai_match_30s` to the replay loop**

In `check_determinism.sh`, find the line:

```bash
for name in basic ball_only rollback_torture; do
```

Replace with:

```bash
for name in basic ball_only rollback_torture ai_match_30s; do
```

- [ ] **Step 2: Commit**

```bash
git add Scripts/check_determinism.sh
git commit -m "ci: include ai_match_30s in determinism replay loop"
```

### Task T11.3 — Update `update_determinism_baseline.sh` (if it has a hardcoded list)

**Files:**
- Modify: `Scripts/update_determinism_baseline.sh`

- [ ] **Step 1: Check + update the baseline regenerator**

```bash
grep -n "basic\|ball_only\|rollback_torture" Scripts/update_determinism_baseline.sh
```

If a hardcoded list exists, add `ai_match_30s` the same way as T11.2.

- [ ] **Step 2: Commit (only if changed)**

```bash
git add Scripts/update_determinism_baseline.sh
git commit -m "ci: include ai_match_30s in baseline regenerator"
```

### Task T11.4 — Regenerate all baselines + run gate

- [ ] **Step 1: Regenerate**

```bash
./Scripts/update_determinism_baseline.sh
ls -la Source/Edge26SimStandalone/tests/replays/*.expected.hashes
```

Expected: 4 `.expected.hashes` files, including `ai_match_30s.expected.hashes` (~1500 lines if hashing every tick).

- [ ] **Step 2: Run the gate**

```bash
./Scripts/check_determinism.sh 2>&1 | tail -10
```

Expected last line: `PASS: all determinism checks`. Lookout: if the new stream fails on first run (some new code path hits an asymmetry), fix the asymmetry in code rather than the baseline.

- [ ] **Step 3: Commit baselines**

```bash
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): add ai_match_30s baseline (1500-tick hash trace)"
```

- [ ] **Step 4: Push branch and confirm CI matrix green**

```bash
git push -u origin feat/phase2-spatial-ai
gh pr create --title "Phase 2 — Spatial AI for 22 players" \
    --body "$(cat <<'EOF'
## Summary
- 22-player roster + roles + 4-3-3 formation
- 5-field spatial value model (Space / DefCoverage / Lane / PassReception / Threat)
- Three-layer AI: Team Strategy (2 Hz) / Unit Coordination (10 Hz) / Individual (50 Hz)
- Offside enforcement with 30-tick grace
- Simple goalkeeper AI (stance/sweeper/save)
- FIFA-style player switching (auto + manual with 0.5 s cooldown)
- AI debug overlay (heatmaps, intent arrows, offside lines) gated by cheat manager

Spec: docs/superpowers/specs/2026-05-15-phase2-spatial-ai-design.md
Plan: docs/superpowers/plans/2026-05-15-phase2-spatial-ai-plan.md

## Test plan
- [ ] `Scripts/check_determinism.sh` PASS on Linux/macOS/Windows
- [ ] `lint_sim.sh` clean
- [ ] 5-minute PIE soak — no crashes/ensures/log spam
- [ ] Acceptance criteria §15 #1–#11 all green (see spec)

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

Wait for the CI matrix to come back green on all three OSes (Linux/macOS/Windows). If any leg fails, debug + push fixes before continuing.

- [ ] **Step 5: Mark M11 complete in PROGRESS.md + commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): M11 complete; advance to M12 (tuning + final acceptance)"
```

---

## M12 — AI tuning pass + final acceptance

The first 11 milestones produce *working* AI: every layer fires, every intent fires, the determinism gate stays green. M12 is the qualitative pass — does it *look* like football in PIE?

You do this milestone by playing the game, watching the debug overlay, and tweaking the weights in `kRoleWeightsTable` + the constants in `UpdateTeamStrategy` until each acceptance criterion in spec §15 actually holds.

The tuning loop is short: edit values → recompile → PIE → observe → repeat. **Never use floats.** All tweaks are integer adjustments to the `F32(x.y)` literals in the role table or the `F32FromFraction(num, denom)` calls in team strategy.

### Task T12.1 — 5-minute PIE soak with notes

**Files:**
- Create: `docs/superpowers/notes/2026-05-15-phase2-pie-soak-notes.md` (scratchpad, not user-facing)

- [ ] **Step 1: Launch PIE with overlay on**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Open editor, open `L_Pitch`, press Play.

In the in-game console (~):

```
edge26_ai_intent_arrows 1
edge26_ai_offside_lines 1
```

Then optionally:

```
edge26_ai_show_field PassReception
```

- [ ] **Step 2: Soak for 5 minutes, take notes**

Open `docs/superpowers/notes/2026-05-15-phase2-pie-soak-notes.md` and as you watch, log observations against each acceptance criterion in spec §15:

```markdown
# Phase 2 PIE Soak Notes — YYYY-MM-DD

## Acceptance §15 #1 — 22 players spawn in 4-3-3 at kickoff
- [ ] PASS / FAIL / observed:

## Acceptance §15 #6 — AI sanity
- [ ] off-ball forward runs:
- [ ] defenders track:
- [ ] defensive line moves as unit:
- [ ] press fires conditionally:
- [ ] late-and-losing line height shifts:

## Acceptance §15 #7 — Offside enforced
- [ ] visible line:
- [ ] through-ball turnover within 30 ticks:

## Acceptance §15 #8 — GK saves
- [ ] most on-target shots saved:
- [ ] goal triggers when shots beat reach:

## Acceptance §15 #9 — Player switching
- [ ] auto-switch on lose-ball:
- [ ] manual switch (R key) works:
- [ ] 0.5s cooldown observed:

## Acceptance §15 #10 — Debug overlay
- [ ] show_field toggles heatmap:
- [ ] intent_arrows toggles arrows:
- [ ] off by default:

## Bugs / tuning issues
- (list)
```

- [ ] **Step 3: Crash / log-spam check**

After 5 minutes:

```bash
tail -200 ~/Library/Logs/Unreal\ Engine/Edge26Editor/Edge26.log | grep -E "Warning|Error|ensure"
```

Expected: clean (or only known third-party log spam).

- [ ] **Step 4: Commit notes**

```bash
mkdir -p docs/superpowers/notes
git add docs/superpowers/notes/2026-05-15-phase2-pie-soak-notes.md
git commit -m "docs(phase2): PIE soak notes — first-pass tuning observations"
```

### Task T12.2 — Tune role weights from observations

Based on the soak notes, iterate on `kRoleWeightsTable` in `Source/Edge26Sim/Private/AI/Roles.cpp`. The most common first-pass corrections (in order):

1. **Defenders bunching forward** → drop `ST/CAM/W_L/W_R.MakeRunForward` slightly; raise `CB/FB_L/FB_R.HoldDefensiveLine`.
2. **Too many simultaneous pressers** → already gated by Layer B nomination; if still wrong, drop role-table `Press` everywhere by 0.1.
3. **AI never passes** → raise `PreferPass` for everyone by 0.1; lower `PreferDribble` by 0.1.
4. **AI shoots from impossible angles** → in `EvaluateOnBall`, the "in opp third" gate is `PitchHalfLen/3`; widen to `PitchHalfLen/2` if needed.
5. **Wingers don't hold width when team has ball** → raise `W_L/W_R.ProvideWidth` toward 1.5.

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/Roles.cpp`

- [ ] **Step 1: Apply 1 tuning change at a time**

Make ONE change. Rebuild. PIE. Observe. Decide if it helped. Either accept (commit) or revert (`git checkout -- Source/Edge26Sim/Private/AI/Roles.cpp`) before the next change. This keeps the bisect surface small if something regresses.

Example (one change cycle):

```bash
# edit Roles.cpp: bump W_L.ProvideWidth from F32(1.0) to F32(1.3)
cmake --build build/sim --parallel 2>&1 | tail -3
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
# PIE soak 60 s; observe wingers hugging touchline
# if better: commit
git add Source/Edge26Sim/Private/AI/Roles.cpp
git commit -m "tune(ai): bump W_L/W_R.ProvideWidth 1.0 -> 1.3 — wingers now hold width"
# if not: revert
# git checkout -- Source/Edge26Sim/Private/AI/Roles.cpp
```

- [ ] **Step 2: Repeat for each observation that needs fixing**

Plan on ~5-10 tuning commits in this task.

- [ ] **Step 3: Regenerate baselines after the final commit in this task**

```bash
./Scripts/update_determinism_baseline.sh
./Scripts/check_determinism.sh 2>&1 | tail -3
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): regenerate baselines after role-weight tuning pass"
```

### Task T12.3 — Tune late-game mentality thresholds

If the soak shows the trailing/leading shifts don't fire convincingly (e.g., team falls behind, doesn't push forward), revisit thresholds in `UpdateTeamStrategy`.

**Files:**
- Modify: `Source/Edge26Sim/Private/AI/TeamStrategy.cpp`

- [ ] **Step 1: Possible tweaks**

Most common adjustments:

- Trailing threshold currently fires at `secsLeft < 30 * 60` (30 min). Lower to `20 * 60` for tighter "late-and-losing" window.
- Leading threshold currently `< 20 * 60` (20 min). Raise to `< 25 * 60` so the lead-protect happens earlier.
- `LineHeightBias` magnitude (`+1`/`-1`) scaled by 5 m. If line moves don't show on the offside-line debug, raise the cm factor in `UpdateDefensiveUnit` (currently `* 500` in `bias`).

Same one-change-at-a-time discipline as T12.2.

- [ ] **Step 2: Regenerate baselines after the final commit + run gate**

```bash
./Scripts/update_determinism_baseline.sh
./Scripts/check_determinism.sh 2>&1 | tail -3
git add Source/Edge26SimStandalone/tests/replays/*.expected.hashes
git commit -m "test(sim): regenerate baselines after mentality threshold tuning"
```

### Task T12.4 — Verify all 11 acceptance criteria

**Files:**
- Modify: `docs/superpowers/notes/2026-05-15-phase2-pie-soak-notes.md` (final pass)

- [ ] **Step 1: Run the determinism gate (criterion #2)**

```bash
./Scripts/check_determinism.sh
```

Expected: `PASS: all determinism checks`.

- [ ] **Step 2: Run lint (criterion #3)**

```bash
./Scripts/lint_sim.sh
```

Expected: no banned tokens.

- [ ] **Step 3: Verify snapshot size (criterion #4)**

```bash
grep -n "static_assert(sizeof(FSimWorldState)" Source/Edge26Sim/Public/Sim/WorldState.h
```

Expected: `static_assert(sizeof(FSimWorldState) == 72944)`.

- [ ] **Step 4: 5-minute PIE soak — second pass (criteria #1, #5, #6, #7, #8, #9, #10)**

Same procedure as T12.1, but now check every box. Update the soak notes file with PASS / FAIL per criterion.

- [ ] **Step 5: Verify PROGRESS.md (criterion #11)**

```bash
grep -A 14 "Phase 2: Spatial Value Model + 22-player AI" PROGRESS.md
```

Expected: all 12 milestone checkboxes ticked.

- [ ] **Step 6: If anything FAILED — go back to T12.2 or T12.3 (or open a bugfix loop)**

Do not declare Phase 2 done with red criteria. Fix and recheck.

- [ ] **Step 7: Commit the green-acceptance soak notes**

```bash
git add docs/superpowers/notes/2026-05-15-phase2-pie-soak-notes.md
git commit -m "docs(phase2): final acceptance soak — all 11 criteria GREEN"
```

### Task T12.5 — Final PROGRESS.md update

**Files:**
- Modify: `PROGRESS.md`

- [ ] **Step 1: Rewrite the "Current status" section**

Replace the current status block with:

```markdown
## Current status

**Phase 2: Spatial AI is COMPLETE and verified in PIE.** All twelve milestones
(M1–M12) shipped: 22-player roster with roles + 4-3-3 formation, 5-field
spatial value model, three-layer AI (Team Strategy 2 Hz / Unit Coordination
10 Hz / Individual 50 Hz), offside enforcement, simple goalkeeper AI, FIFA-style
player switching, and AI debug overlay.

Automated acceptance criteria (spec §15 #2, #3, #4) all pass: determinism gate
green on Linux/macOS/Windows, lint clean, `FSimWorldState = 72,944 B`. PIE
acceptance (§15 #1, #5–#10) confirmed working in 5-minute soak. PROGRESS.md
fully up to date (§15 #11).

The repo now has a deterministic 50 Hz football match (22 v 22) ready for
Phase 3 (motion matching, render-side only) or Phase 4 (rollback netcode).
```

- [ ] **Step 2: Move the "Phase 2" roadmap entry from "current" to completed**

Replace:

```markdown
### Phase 2: Spatial Value Model + 22-player AI  ←  current
```

with:

```markdown
### Phase 2: Spatial Value Model + 22-player AI
```

And add a `←  current` marker on Phase 3 (or whichever is next).

- [ ] **Step 3: Append a session entry to the activity log**

```markdown
### YYYY-MM-DD — Phase 2 complete
- M1–M12 landed. 22-player 4-3-3, 5-field spatial value model (1768 cells),
  3-layer AI stack, offside, simple GK, player switching, AI debug overlay.
- FSimWorldState grew 224 B → 72,944 B; xxhash + memcpy under the
  spec's 100 µs/tick budget on M4.
- Tuning pass made N adjustments to kRoleWeightsTable + mentality thresholds.
- PIE soak: all 11 acceptance criteria green. Determinism CI matrix green on
  Linux/macOS/Windows.
```

- [ ] **Step 4: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase2): mark Phase 2 COMPLETE; advance status to Phase 3"
```

### Task T12.6 — Merge to main + tag

**Files:** branch-level / git.

- [ ] **Step 1: Push the final commits and verify CI matrix**

```bash
git push
gh pr checks --watch
```

Expected: all 3 OS checks green.

- [ ] **Step 2: Squash-merge the PR (or rebase-merge — match the team's convention)**

```bash
gh pr merge --squash --delete-branch
git checkout main
git pull --ff-only
```

- [ ] **Step 3: Tag the release**

```bash
git tag -a phase2-v0 -m "Phase 2: Spatial AI v0 — 22 players, 3-layer AI, offside, GK, switching, overlay"
git push origin phase2-v0
```

- [ ] **Step 4: Final verification on `main`**

```bash
./Scripts/check_determinism.sh 2>&1 | tail -3
./Scripts/lint_sim.sh
```

Expected: both pass.

Phase 2 is shipped. Phase 3 (motion-matching animation + procedural ball-contact IK, render-side only per spec §3) or Phase 4 (rollback netcode) is next.

---
