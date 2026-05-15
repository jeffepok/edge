# Edge 26 — Deterministic Sim Core v0 Design

**Status:** Approved (2026-05-15)
**Author:** Jefferson + Claude
**Scope:** First foundational slice of the work described in `project_breakdown.md`. Establishes the deterministic 50Hz simulation module that all later systems (rollback netcode, motion-matching, spatial-AI) depend on.

---

## 1. Goal and non-goals

### Goal

Replace UE5's stock gameplay loop (CharacterMovementComponent, Chaos physics, AnimBP-as-decider) with a separate deterministic simulation module that owns the ground-truth state of the football match. UE5 becomes a rendering/IO host that draws what the sim says is true.

The v0 deliverable proves the **architecture** end-to-end on a deliberately small surface (1 ball + 2 players), so that scaling to 22 players + AI + animation later is a series of incremental, low-risk extensions rather than a rewrite.

### Non-goals (v0)

- 22 simultaneous players, AI, motion-matching, ragdolls, kicks-via-IK, set pieces, offside, fouls, goalkeepers, replays-as-feature, rollback netcode, anti-cheat, telemetry, console-platform CI runners, data-driven config, economy backend.

A full enumeration of intentionally-deferred items lives in **§13 Scope Cuts**.

---

## 2. Architectural decisions (decision log)

| # | Decision | Choice | Alternatives rejected |
|---|---|---|---|
| D1 | First slice | Deterministic sim core | Spatial AI, motion-matching, prototype polish |
| D2 | Determinism mechanism | Fixed-point math (Q32.32 + Q16.16) | Strict-float IEEE 754; soft-deterministic-now |
| D3 | Sim/UE5 ownership | Sim-authoritative; UE5 renders interp state | UE5-authoritative with sim shadow; ball-only sim |
| D4 | v0 scope | 1 ball + 2 players + full plumbing | Ball only; ball + 22 stationary; foundation only |
| D5 | Module layout | Separate `Edge26Sim` UE5 module + standalone CMake lib | Single module w/ folder discipline; separate module w/o standalone |
| D6 | Migration of existing classes | Delete + replace with fresh visual-shell actors | Strip + repurpose in place; keep both alongside |
| D7 | CI scope | Local script + GitHub Actions on Linux/macOS/Windows | Local only; with console runners |
| D8 | Determinism surface | Sim kinematics + ball + contact events + decisions only. **Animation/IK/pose-selection/ragdoll are visual-only, forever.** | Deterministic IK in sim tick (over-engineered; `project_breakdown.md` §1.7 explicitly proposed this — we override) |
| D9 | Determinism enforcement | Build-system boundary + explicit FORBIDDEN/REQUIRED rules in §4 + `Scripts/lint_sim.sh` grep gate + strict-warnings-as-errors | Convention/review only (loses to a tired contributor at 2 a.m.) |

These choices ripple through everything below; treat reversal of any of them as triggering a full re-design pass, not a patch.

---

## 3. Sim/Render boundary principle (load-bearing — applies to every phase)

**Only state that affects gameplay outcomes is deterministic. Everything visual is not, and never will be, part of the sim.** This principle overrides any conflicting language in `project_breakdown.md` and is the rule that future-phase designs must satisfy.

### What is in the sim (deterministic, fixed-point, in the snapshot)

- **Player kinematic state.** Position, velocity, heading, ground/sprint flags, balance scalar.
- **Ball physics state.** Position, velocity, angular velocity (spin), ground flag. Magnus force, drag, bounce, friction.
- **Contact events.** Tick, contact point in world space, which foot, foot velocity at contact, contact offset on the ball (for spin). *The sim decides the contact; rendering honors it.*
- **Reach/balance/composure budget checks.** "Can this player make this contact, given attributes and current balance?" Answered with geometry + attributes — a handful of dot products and threshold comparisons. **Not** by running an IK solver to completion. If the budget allows it, the sim writes the contact event; if not, the sim writes a degraded event (miscontrol, lunge, foul-away). Both are deterministic outcomes.
- **AI decisions.** Action chosen (pass/shoot/dribble/tackle/run), target picked. Computed from the spatial value field (which is itself deterministic).
- **Outcome state.** Possession-holder, set-piece state, scoreline, match clock as it affects logic (e.g., 90+1 minute), RNG state, tick number.

### What is NOT in the sim (cosmetic; can differ across machines)

- **Pose selection (motion matching).** UE5's Pose Search runs on the render thread. Two clients picking slightly different clips is fine — the player's capsule is sim-authoritative; the visual mesh is drawn around it. Ball physics doesn't read animation.
- **Two-bone IK during contact.** Once the sim has decided "foot lands here at this velocity," the render-side IK warps the leg to draw the contact. Joint angles can differ across clients with zero gameplay consequence.
- **Foot-roll IK orientation.** Same reasoning.
- **Approach warping / stride adjustment.** Visual-only adjustment of the gait around the sim-determined capsule path. The capsule does not move differently because of the warp.
- **Ragdolls.** Already cosmetic in the breakdown.
- **Look-at, head turn, facial.** Already cosmetic in the breakdown.
- **Animation graph state-machine transitions.** Can be slightly different across machines — they don't feed back.

### Why this is load-bearing

Making any of the above deterministic signs us up for: a deterministic-FP discipline in three more subsystems, deterministic ports of every UE5 anim node we touch, custom IK math libraries with their own determinism tests, golden-image animation regression suites, CI gates that fail whenever a UE5 update changes pose-search internals. Multiply by 22 players × 50Hz: this is a months-of-engineering rabbit hole with no gameplay payoff because none of the visual output feeds back into outcomes.

Keeping the deterministic surface to **player capsule kinematics, ball physics, contact events, decisions, RNG** is what makes rollback netcode and CI determinism gates tractable for one developer.

### The three-question test ("should this be in the sim?")

Before adding any subsystem to the sim, all three must be **yes**:

1. Does the output of this subsystem feed back into player kinematics, ball physics, or a decision event?
2. If two machines computed this slightly differently, would the ball or a player end up in a measurably different place 5 seconds later?
3. Is this cheap enough to run at 50Hz × 22 entities under our determinism constraints?

Any **no** → render-side. Most "should X be deterministic?" questions for football have the answer "no."

### What this changes in v0

Nothing concrete — v0 has no IK, no motion matching, no ragdolls. But codifying the principle now prevents the breakdown's IK-in-sim language from leaking into phase 3's design, and it tells future-us exactly where to draw the line.

---

## 4. Determinism rules (the enforceable checklist)

§3 is the *principle*. This section is the *enforceable rules*. A future contributor — or future-me — should be able to read this in 30 seconds and know exactly what is and isn't allowed inside `Edge26Sim/`.

### 4.1 FORBIDDEN inside `Edge26Sim/`

These are flat-out banned. Most are also caught by the build system (the module's `Build.cs` only depends on `Core`, the CMake standalone build doesn't link any of them). A few aren't caught at build time and rely on review + grep — those are flagged.

| Forbidden | Why | Catch |
|---|---|---|
| Floating-point types (`float`, `double`, `FVector`, `FRotator`, `FQuat`, `FMath::*`, `Vector3D`) | Not bit-identical across compilers/SIMD/FMA | `grep` lint in CI (`grep -rn 'float\|double\|FVector\|FRotator\|FMath' Source/Edge26Sim/`) |
| Hash-ordered containers (`std::unordered_map`, `std::unordered_set`, `TMap`, `TSet`) | Iteration order depends on bucket layout = non-deterministic | grep lint |
| Threads / async (`std::thread`, `std::async`, `ParallelFor`, `FRunnable`, `Async()`) | Introduces ordering variance | grep lint |
| Wall-clock time (`std::chrono::*::now`, `FDateTime::Now`, `FPlatformTime::*`, `time()`, `clock()`) | Different across machines / runs | grep lint |
| UE5 Engine APIs (`#include "Engine/...`, `#include "GameFramework/...`, `#include "Chaos/...`, `UObject`, `AActor`, anything `UCLASS`/`UFUNCTION`/`UPROPERTY`) | Pulls non-deterministic engine state into sim | `Build.cs` (Core-only dep) + grep |
| Non-PRNG randomness (`std::rand`, `std::random_device`, `FMath::RandRange`, `std::mt19937` with non-snapshotted state) | State outside the snapshot | grep lint |
| Heap allocation during `Step()` (`new`, `malloc`, `TArray::Add`, `std::vector::push_back`, anything that grows containers) | Allocator behavior + addresses leak into state | grep lint on the `Step()` call-path source files |
| Exceptions (`throw`, `try/catch`) | Behavior differs across compilers; slow path | grep lint |
| C++ static / global mutable state | Hidden state outside the snapshot | grep lint (`static [a-zA-Z]` outside `constexpr`) |
| Virtual functions in sim-state structs | vtable pointers in serialized state = chaos | review |
| Signed-integer-overflow-as-arithmetic (relying on UB wrap) | UB; behavior varies by compiler/flags | use unsigned for wrap, or compile with `-fwrapv` (still flagged) |
| Conditional compilation that changes behavior (`#ifdef PLATFORM_X` producing different math) | Different platforms compute differently | review — the only allowed `#ifdef` is `Math/Mul64.h` (64×64→128 multiply), which is exhaustively tested |
| Reading any UE5 frame delta (`DeltaSeconds` from a Tick callback) inside the sim | Wall-clock leakage | review — sim always uses its own `TickDuration` constant |

### 4.2 REQUIRED inside `Edge26Sim/`

| Required | Reason |
|---|---|
| Deterministic iteration order | Iterate over fixed-size arrays by index (`for i in 0..N`), never over hash containers. When iterating two sets of entities, define a stable order (e.g., players in ascending `ControllerIndex`). |
| Fixed update order within a tick | Players updated in index order; ball after players; AI decisions before kinematics. Documented at the top of `SimWorld::Step`. |
| Explicit zero-initialization of state structs | `memset(&state, 0, sizeof(state))` before any field assignment. Default constructors don't zero implicit pad bytes; only `memset` does. |
| Stable hashing | xxhash64 (vendored, MIT). Same bytes in → same bytes out, on every platform, forever. |
| Fixed iteration counts in iterative solvers | `Sqrt` runs 8 Newton iterations always. `Atan2` runs 20 CORDIC iterations always. **Never** "loop until error < epsilon" — that's compiler-dependent. |
| `static_assert` on every state struct size and alignment | Catches accidental layout changes from refactors. |
| All sim parameters as `constexpr` | No `.ini` reads, no runtime tunables, no UObject CDOs. Constants live in `Sim/Constants.h`. |
| Single PRNG instance per `SimWorld`, state in the snapshot | All randomness flows from one `uint64` seeded state; rollback re-rolls identically. |
| All sim-state structs POD | No constructors, destructors, virtuals. `static_assert(std::is_trivially_copyable_v<T>)` on each. |
| `Step()` is allocation-free | Verified by overriding `operator new`/`operator delete` to assert in test builds when called inside `Step()`. |
| Compile with strict warnings as errors | `-Wall -Wextra -Werror` (clang/gcc); `/W4 /WX` (MSVC). Catches uninitialized reads, narrowing conversions, sign mismatches. |

### 4.3 The single allowed platform-conditional code

`Source/Edge26Sim/Public/Math/Mul64.h` — the 64×64 → 128-bit multiply needed for Q32.32 multiplication. Three implementations gated by `#if defined(__SIZEOF_INT128__)` / `#elif defined(_MSC_VER)` / `#else #error`. Tested by a checked-in vector table of (a, b, expected_result) tuples that the CI determinism gate runs on every platform; if any platform's implementation produces a different result for any vector, the build fails before sim tests even start.

This is the **only** platform-conditional code permitted in `Edge26Sim/`. New ones require a doc PR justifying the exception and adding test vectors.

### 4.4 Build-system enforcement (in addition to the rules above)

- `Edge26Sim.Build.cs` declares `Core` as its only public/private dependency. Any attempt to add `Engine`, `Chaos`, `AnimGraphRuntime`, etc. fails review.
- `Source/Edge26SimStandalone/CMakeLists.txt` builds the same sim sources without any UE5 toolchain. Any accidental UE5 include in `Edge26Sim/` breaks the standalone build, caught by CI on all three runner OSes.
- `Scripts/lint_sim.sh` runs the grep lints (forbidden tokens) on every PR. Exits non-zero if it finds any. Hooked into `check_determinism.sh` so the local pre-push run also catches them.

### 4.5 What `Scripts/lint_sim.sh` looks for (illustrative — exact list lives in the script)

```bash
# Anti-patterns inside Source/Edge26Sim/
PATTERNS=(
    '\bfloat\b'  '\bdouble\b'  'FVector\b'  'FRotator\b'  'FQuat\b'  'FMath::'
    'std::unordered_'  'TMap<'  'TSet<'
    'std::thread'  'std::async'  'ParallelFor'  'FRunnable'  'Async\('
    'std::chrono'  'FDateTime'  'FPlatformTime'  '\btime\('  '\bclock\('
    '#include\s*"Engine/'  '#include\s*"GameFramework/'  '#include\s*"Chaos/'
    'std::rand'  'std::random_device'  'FMath::Rand'
    '\bnew\b'  '\bmalloc\b'  '\btry\b'  '\bthrow\b'
)
```

False positives (e.g., `floating-point` in a comment) are suppressed by an inline `// SIM-LINT-OK: <reason>` marker. Comments are encouraged because they document *why* the exception is safe.

---

## 5. Module structure

```
Edge26/
├── Source/
│   ├── Edge26Sim/                  ← NEW. Pure deterministic sim.
│   │   ├── Edge26Sim.Build.cs      ← Dependencies: Core ONLY. No Engine, no Chaos, no AnimGraphRuntime.
│   │   ├── Public/
│   │   │   ├── Math/Fixed.h        ← Fixed64 (Q32.32), Fixed32 (Q16.16)
│   │   │   ├── Math/FixedVec.h     ← FixedVec2, FixedVec3
│   │   │   ├── Math/FixedAngle.h   ← Normalized angle wrapper
│   │   │   ├── Math/Trig.h         ← Sin, Cos via 1024-entry LUT + lerp
│   │   │   ├── Math/Sqrt.h         ← Newton-Raphson, fixed iteration
│   │   │   ├── Math/Atan2.h        ← CORDIC, fixed iteration
│   │   │   ├── Math/Rng.h          ← xorshift64 PRNG
│   │   │   ├── Math/Hash.h         ← xxhash64 wrapper
│   │   │   ├── Sim/InputFrame.h    ← Quantized per-tick input
│   │   │   ├── Sim/BallState.h     ← FSimBallState POD
│   │   │   ├── Sim/PlayerState.h   ← FSimPlayerState POD
│   │   │   ├── Sim/WorldState.h    ← FSimWorldState (everything)
│   │   │   └── Sim/SimWorld.h      ← Step(InputFrame[]), Snapshot, Restore, HashState
│   │   └── Private/                ← Implementations. No UE5 includes anywhere.
│   │
│   ├── Edge26SimStandalone/        ← NEW. CMake project, builds the headless test exe.
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp                ← Argument parsing, replay loop, hash output
│   │   └── tests/
│   │       ├── test_math.cpp       ← Unit tests for fixed-point math
│   │       ├── test_snapshot.cpp   ← Snapshot/restore round-trip
│   │       └── replays/
│   │           ├── basic.input
│   │           ├── basic.expected.hashes
│   │           ├── ball_only.input
│   │           ├── ball_only.expected.hashes
│   │           ├── rollback_torture.input
│   │           └── rollback_torture.expected.hashes
│   │
│   └── Edge26/                     ← Existing module, shrinks dramatically.
│       ├── Public/
│       │   ├── Adapter/SimHostSubsystem.h    ← UWorldSubsystem; owns SimWorld; ticks at 50Hz. Public API:
│       │   │                                    GetBallPositionWorld(), GetPlayerPositionWorld(idx), QueueInput(...)
│       │   ├── Adapter/SimInputCollector.h   ← Enhanced Input → quantized FInputFrame
│       │   ├── Adapter/FootballerVisual.h    ← APawn, no CMC; transform = sim interp
│       │   ├── Adapter/SoccerBallVisual.h    ← AActor, no physics; transform = sim interp
│       │   ├── Adapter/FootballerVisualAnimInstance.h ← Cosmetic only; computes Speed/Direction from transform deltas
│       │   ├── Adapter/SimHostBootstrap.h    ← AActor placed in level to force subsystem init in PIE
│       │   ├── Game/SoccerGameMode.h         ← Lightly adjusted; spawns subsystem on BeginPlay
│       │   ├── Game/SoccerHUD.h              ← Unchanged
│       │   └── Game/GoalTrigger.h            ← Queries sim ball position
│       └── Private/
│
├── Scripts/
│   ├── editor/
│   │   └── reparent_blueprints.py            ← Headless UE5 Python; re-parents BPs to new C++ classes
│   ├── check_determinism.sh                  ← Builds standalone binary + runs replay tests
│   └── update_determinism_baseline.sh        ← Regenerates expected hash files (manual confirmation step)
│
├── .github/
│   └── workflows/
│       └── determinism.yml                   ← Builds Edge26Sim on linux/macos/windows; runs determinism gate
│
├── docs/
│   └── superpowers/
│       └── specs/
│           └── 2026-05-15-sim-core-v0-design.md   ← This file.
│
├── PROGRESS.md                                ← Living status doc; updated each session
├── RUNBOOK.md                                 ← Rewritten in M7 of this phase
└── README.md
```

### The hard boundary

`Edge26Sim.Build.cs` declares only `Core` as a dependency. If a developer tries to `#include "Engine/..."`, `#include "GameFramework/..."`, `#include "Chaos/..."`, etc., the build fails. The compiler enforces the boundary; not code review, not convention. **This rule is non-negotiable.**

The standalone CMake project compiles the same source files outside the UE5 build system. If any sim file accidentally introduces a UE5 dependency (e.g. via a transitive `#include`), the CMake build breaks and CI catches it. This is the second enforcement layer.

---

## 6. The sim tick

### Fixed timestep with accumulator

The sim ticks at exactly **50 Hz (20ms per tick)**. Render runs at whatever the GPU/monitor delivers (typically 60–144Hz). Decoupled via accumulator:

```cpp
void USimHostSubsystem::Tick(float DeltaSeconds) {
    Accumulator += DeltaSeconds;
    while (Accumulator >= TickDuration /* 0.020s */) {
        FInputFrame Frame = InputCollector->PullForTick(CurrentTick);
        SimWorld.Step(Frame);
        CurrentTick++;
        Accumulator -= TickDuration;
    }
    float Alpha = Accumulator / TickDuration;   // [0, 1)
    DriveVisualActors(Alpha);
}
```

- At 60Hz render: ~1.2 ticks per frame → mostly 1 tick, occasionally 0 or 2.
- At 144Hz render: ~0.35 ticks per frame → mostly 0, occasionally 1.
- If the render falls behind (e.g. 20fps during a hitch): accumulator can grow, capped at 5 ticks per frame to prevent the spiral of death. Excess time is dropped.

### Input flow

```
Hardware  →  Enhanced Input  →  SimInputCollector  →  FInputFrame  →  SimWorld.Step
                                       ↓
                       (records tick number, quantizes axes/buttons)
```

`FInputFrame`:

```cpp
struct FInputFrame {
    uint32 TickNumber;
    int8   Move[2][2];      // Per-player [x, y] stick, range [-127, 127]
    uint8  Buttons[2];      // Per-player bitfield: Sprint=0x01, Pass=0x02, Shoot=0x04, Chip=0x08
};
```

Quantization to `int8` is deliberate — it's the format that will serialize over the wire when rollback netcode arrives, and reducing axis precision now prevents "feels different in MP" surprises later.

### Player 2 input defaulting

- P1 = keyboard+mouse.
- P2 = gamepad if connected; otherwise sim-driven stationary dummy (no input emitted; ControllerIndex = 0xFF).
- Both can be overridden by `BP_SimHostBootstrap` properties for debug.

### Render interpolation

Each `AFootballerVisual` / `ASoccerBallVisual` stores `PrevTickTransform` and `CurrentTickTransform`. On every render tick, `SimHostSubsystem::DriveVisualActors(Alpha)` does:

```cpp
for (each visual) {
    FTransform Drawn = LerpTransform(visual.PrevTickTransform, visual.CurrentTickTransform, Alpha);
    visual->SetActorTransform(Drawn);
}
```

Visual transforms are `FTransform` (UE5's float-based), converted from `FixedVec3` only at draw time. The sim never reads back from these.

### Animation in v0

The AnimInstance is **purely cosmetic**. It computes `Speed`, `RelativeDirection`, `LeanAngle` from the visual's transform deltas (after interpolation) and feeds them to a 1D/2D blend space using the existing Mannequin unarmed-jog animations. The sim does not consult animation; the animation does not feed back into the sim.

---

## 7. Fixed-point math library

### Types

```cpp
struct Fixed64 {                       // Q32.32, range ±2.1B, precision ~2.3e-10
    int64_t Raw;
    static constexpr int64_t Shift = 32;
    static constexpr int64_t One = 1LL << 32;
    // operators: + - * / unary-
    // FromInt(int64_t), FromFloat(double) -- lossy, init only
    // ToFloat() -- lossy, render-only
};

struct Fixed32 {                       // Q16.16, range ±32k, precision ~1.5e-5
    int32_t Raw;
    static constexpr int32_t Shift = 16;
    static constexpr int32_t One = 1 << 16;
};

struct FixedVec2 { Fixed64 X, Y; };
struct FixedVec3 { Fixed64 X, Y, Z; };

struct FixedAngle {                    // Always normalized to [-π, π)
    Fixed32 Raw;
    static FixedAngle FromRadians(Fixed32);
    static FixedAngle FromDegrees(Fixed32);
};
```

### Multiplication implementation

`Fixed64 * Fixed64` needs a 128-bit intermediate. Single header wraps the platform difference:

```cpp
// Fixed_Mul64.h
#if defined(__SIZEOF_INT128__)
    inline int64_t Mul64Q32(int64_t a, int64_t b) {
        return (int64_t)(((__int128)a * (__int128)b) >> 32);
    }
#elif defined(_MSC_VER)
    #include <intrin.h>
    inline int64_t Mul64Q32(int64_t a, int64_t b) {
        int64_t high;
        int64_t low = _mul128(a, b, &high);
        return (int64_t)((uint64_t)low >> 32) | (high << 32);
    }
#else
    #error "Add 64x64->128 mul implementation for this platform"
#endif
```

This is the only platform-conditional code in `Edge26Sim`. It is unit-tested against a comprehensive vector table to prove cross-platform identity.

### Trig

1024-entry sin table covers `[0, π/2)`. Other quadrants are reflections.

```cpp
namespace SimMath {
    Fixed32 Sin(FixedAngle);
    Fixed32 Cos(FixedAngle);            // = Sin(a + π/2)
    Fixed64 Sqrt(Fixed64);              // Newton-Raphson, 8 iterations
    FixedAngle Atan2(Fixed64 y, Fixed64 x);  // CORDIC, 20 iterations
}
```

Fixed iteration counts make these bit-deterministic by construction. No "converge until error < epsilon" loops.

### Tests

Built into the standalone binary and run by `check_determinism.sh` before any sim tests:

1. **Round-trip**: `Fixed64::FromInt(n).ToInt() == n` for `n ∈ [-1e9, 1e9]` stepped by 12345.
2. **Multiplication identity**: 10,000 random `(a, b)` pairs (seeded), result hash must match a checked-in golden value.
3. **Trig identity**: `Sin²(a) + Cos²(a)` within 2 ulps of `Fixed32::One` for `a` swept across `[-π, π]` in 1° steps.
4. **Sqrt monotonicity**: `Sqrt(n+1) ≥ Sqrt(n)` for `n ∈ [0, 1e9]`.
5. **Atan2 quadrant coverage**: 256 quadrant-sweep test vectors, golden hash must match.

If any test fails, `check_determinism.sh` exits non-zero and the binary does not proceed to sim replay tests.

---

## 8. Sim world state and step function

### State (POD; everything that affects gameplay)

```cpp
struct FSimBallState {
    FixedVec3   Position;        // world-space, cm    (24 B, 8-aligned)
    FixedVec3   Velocity;        // cm/s              (24 B)
    FixedVec3   AngularVelocity; // rad/s, unused v0  (24 B)
    uint8       Flags;           // bit 0: Grounded
    uint8       _pad[7];         // EXPLICIT padding to 80 B total
};
static_assert(sizeof(FSimBallState) == 80);

struct FSimPlayerState {
    FixedVec3   Position;        // 24 B
    FixedVec3   Velocity;        // 24 B
    FixedAngle  Heading;         // 4 B (wraps Fixed32 = int32)
    FixedAngle  FacingTarget;    // 4 B
    uint8       ControllerIndex; // 0=P1, 1=P2, 0xFF=stationary
    uint8       Flags;           // bit 0: Grounded, bit 1: Sprinting
    uint8       _pad[6];         // EXPLICIT trailing padding to 64 B total (8-aligned)
};
static_assert(sizeof(FSimPlayerState) == 64);

struct FSimWorldState {
    uint32          TickNumber;          // 4 B
    uint32          _pad0;               // EXPLICIT padding before 8-aligned RngState
    uint64          RngState;            // 8 B
    FSimBallState   Ball;                // 80 B
    FSimPlayerState Players[2];          // 128 B; v0 hardcoded 2, becomes MAX_PLAYERS=22 later
};
static_assert(sizeof(FSimWorldState) == 224);
static_assert(alignof(FSimWorldState) == 8);
```

All fields are POD. No pointers. No virtuals. `FSimWorldState` can be `memcpy`-snapshotted and `xxhash`-hashed.

**Padding is explicit, named, and zero-initialized** (see below). Implicit C++ padding is the #1 subtle source of hash divergence — two "logically identical" structs whose padding bytes hold different garbage hash to different values. Explicit padding makes the layout reviewable and lets us assert sizes.

**Zero-initialization rule.** `FSimWorldState` is *always* constructed via `memset(&state, 0, sizeof(state))` followed by named-field assignment. Default constructors don't touch the explicit `_pad*` members; only the `memset` guarantees they're zero before hashing. The `SimWorld` constructor enforces this; `Snapshot()` doesn't need to because it's copying an already-zeroed struct.

### Step function (per tick)

```cpp
void SimWorld::Step(const FInputFrame& Frame) {
    State.TickNumber = Frame.TickNumber;

    // 1. Translate inputs into per-player intents.
    PlayerIntent Intents[2];
    for (int i = 0; i < 2; ++i) {
        Intents[i] = TranslateInput(Frame, i, State.Players[i]);
    }

    // 2. Step each player kinematically.
    for (int i = 0; i < 2; ++i) {
        StepPlayer(State.Players[i], Intents[i]);
    }

    // 3. Process player→ball actions (Pass/Shoot/Chip impulses).
    for (int i = 0; i < 2; ++i) {
        MaybeApplyKick(State.Ball, State.Players[i], Intents[i]);
    }

    // 4. Step ball physics.
    StepBall(State.Ball);
}
```

### Player kinematic model (v0; simple by design)

```cpp
void SimWorld::StepPlayer(FSimPlayerState& P, const PlayerIntent& I) {
    // Desired velocity from stick input.
    Fixed64 MaxSpeed = (P.Flags & SPRINT) ? SprintSpeed : JogSpeed;  // constexpr Fixed64
    FixedVec3 Desired = I.StickWorldDir * MaxSpeed;

    // Approach desired velocity at fixed accel.
    P.Velocity = ApproachVec3(P.Velocity, Desired, Accel * DT);

    // Turn heading toward facing target at fixed turn rate.
    P.Heading = ApproachAngle(P.Heading, P.FacingTarget, TurnRate * DT);

    // Integrate position.
    P.Position += P.Velocity * DT;

    // Clamp to pitch bounds (large constant for v0).
    P.Position.X = Clamp(P.Position.X, -PitchHalfLen, PitchHalfLen);
    P.Position.Y = Clamp(P.Position.Y, -PitchHalfWid, PitchHalfWid);
    P.Position.Z = MaxFixed(P.Position.Z, GroundZ);

    // Grounded flag.
    P.Flags = (P.Position.Z <= GroundZ) ? (P.Flags | GROUNDED) : (P.Flags & ~GROUNDED);
}
```

**No** momentum, **no** acceleration curves, **no** stamina, **no** balance, **no** player-player collisions. This produces RC-car movement. That's the explicit v0 feel.

### Ball kinematic model (v0; simple by design)

```cpp
void SimWorld::StepBall(FSimBallState& B) {
    // Gravity.
    B.Velocity.Z -= Gravity * DT;

    // Linear drag (air, constant).
    B.Velocity = B.Velocity * (Fixed64::One - LinearDragPerTick);

    // Integrate.
    B.Position += B.Velocity * DT;

    // Ground bounce.
    if (B.Position.Z < BallRadius) {
        B.Position.Z = BallRadius;
        if (B.Velocity.Z < 0) {
            B.Velocity.Z = -B.Velocity.Z * Restitution;        // bounce
            B.Velocity.X *= GroundFrictionXY;
            B.Velocity.Y *= GroundFrictionXY;
            if (AbsFixed(B.Velocity.Z) < SettleThreshold) {
                B.Velocity.Z = Fixed64::Zero;
                B.Flags |= GROUNDED;
            }
        }
    } else {
        B.Flags &= ~GROUNDED;
    }
}
```

**No** Magnus spin force (spin is stored but unused). **No** surface-dependent friction. **No** rolling-vs-sliding distinction. **No** wind. These come in the ball-physics slice.

### Kick application (v0; placeholder)

If a player's intent flags include Pass/Shoot/Chip and the ball is within `KickReach` (~1.5m) and ahead of their facing, apply a fixed impulse in the facing direction:

- Pass: `BallVel = (HeadingDir * PassSpeed) + (UpVec * PassLift)` — flat-ish 15 m/s pass.
- Shoot: `BallVel = (HeadingDir * ShotSpeed) + (UpVec * ShotLift)` — 25 m/s drive, slightly lifted.
- Chip: `BallVel = (HeadingDir * ChipSpeed) + (UpVec * ChipLift)` — 12 m/s with strong lift.

No aiming, no power meter, no contact quality. Real kicks belong to the motion-matching slice.

---

## 9. Snapshot / Restore / Hash

```cpp
class SimWorld {
public:
    void Snapshot(FSimWorldState& Out) const { Out = State; }
    void Restore(const FSimWorldState& In)   { State = In; }
    uint64 HashState() const                 { return XXH64(&State, sizeof(State), 0); }
private:
    FSimWorldState State;
};
```

Trivial because `FSimWorldState` is POD. `XXH64` is vendored as a single header in `Edge26Sim/Public/Math/Hash.h` (MIT-licensed). It is deterministic by spec.

### What goes in the snapshot

Everything the next tick's `Step` reads from. Specifically:
- TickNumber
- RngState
- Ball state
- All player states

### What is NOT in the snapshot

- Match clock, score, possession — cosmetic / derived.
- Goal trigger state — overlap check is run against the *visual* ball position in PIE; acceptable for v0.
- Animation state — cosmetic.
- UE5 actor transforms — derived from sim state every render frame.

The snapshot is the **whole truth** of the sim. If the next tick produces different output for the same input, something not in the snapshot is leaking state — that's a bug.

### Rollback test (built into standalone binary, runs every CI build)

Every 30 ticks of the replay: snapshot. Advance 5 ticks. Restore. Advance 5 ticks again. Verify the post-restore hash at +5 matches the original-run hash at +5. Any mismatch = "something is not in the snapshot" = build fails. This is the single most valuable test in the system; it catches the bug class that has historically destroyed rollback netcode implementations.

---

## 10. Headless test binary and determinism gate

### The binary

`Edge26SimStandalone/main.cpp`:

```
edge26_sim_replay [options]
  --input <path>          Path to input stream binary
  --ticks <n>             Number of ticks to run (default: from input file)
  --seed <hex>            Initial RNG seed (default: 0)
  --hash-every <n>        Print hash every N ticks (default: 1)
  --out <path>            Write per-tick state log to file
  --rollback-test         Enable in-line rollback round-trip test
  --self-test             Run math + snapshot unit tests, exit
```

Input stream binary format:

```
[16 bytes header: magic "EDG26IN0" + version(uint32) + tickCount(uint32)]
[N records of FInputFrame, sizeof = 16 bytes each]
```

So an input stream of 90-minute match = 270,000 records × 16 bytes = 4.3 MB. Tiny.

### Three checked-in test streams

`Source/Edge26SimStandalone/tests/replays/`:

| File | Ticks | Description |
|---|---|---|
| `basic.input` | 500 | Kickoff, both players move toward center, P1 takes shot, ball rolls to goal area |
| `ball_only.input` | 1000 | Both players stationary (ControllerIndex = 0xFF); ball given initial impulse, rolls/bounces/settles |
| `rollback_torture.input` | 2000 | Erratic stick reversal every 5 ticks on P1; tests snapshot/restore via `--rollback-test` |

Alongside each input, a checked-in `<name>.expected.hashes` file: one line per tick = `TICK HASH` in hex.

### Generating the test streams

`tests/replay_generator.cpp` is a tiny separate executable that produces the binary `.input` files from a hand-readable script (literally a sequence of `(tick, p1_stick_x, p1_stick_y, p1_buttons, p2_stick_x, ...)` tuples). The generator and its scripts are committed; the `.input` files are derived but also committed (they're tiny). This means a reviewer can inspect the source-of-truth for any replay without parsing the binary, and we can regenerate streams when the `FInputFrame` schema evolves.

### The local gate

`Scripts/check_determinism.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

bash Scripts/lint_sim.sh                       # §4 grep gate runs first; cheap

cmake -S Source/Edge26SimStandalone -B build/sim -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Werror"  # §4 strict-warnings
cmake --build build/sim --parallel

./build/sim/edge26_sim_replay --self-test

for replay in basic ball_only rollback_torture; do
    ACTUAL="$(./build/sim/edge26_sim_replay \
        --input  Source/Edge26SimStandalone/tests/replays/${replay}.input \
        --rollback-test)"
    EXPECTED="$(cat Source/Edge26SimStandalone/tests/replays/${replay}.expected.hashes)"
    if [[ "$ACTUAL" != "$EXPECTED" ]]; then
        diff <(echo "$EXPECTED") <(echo "$ACTUAL") | head -20
        echo "FAIL: ${replay}"
        exit 1
    fi
done
echo "PASS: all determinism checks"
```

Run time: ~5 seconds.

### Baseline update

`Scripts/update_determinism_baseline.sh`: runs the binary, captures stdout, overwrites the `.expected.hashes` files. **Only run this when you intend to change behavior.** The expected-hashes file diff goes in the same PR as the behavior change so reviewers see what shifted.

### GitHub Actions

`.github/workflows/determinism.yml`:

```yaml
name: determinism
on: [push, pull_request]
jobs:
  determinism:
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Run determinism gate
        run: bash Scripts/check_determinism.sh
```

The same `*.expected.hashes` baseline must match across all three runners. **This is what catches the cross-platform float-math class of bugs.** If a developer accidentally introduces a non-deterministic call (e.g. `std::sin`, `FMath::Sqrt`), the macOS hash differs from the Linux hash within one tick and the gate fails.

PS5/Xbox runners are explicitly out-of-scope for v0 (require dev kits in a lab). The breakdown's eventual three-console gate is a posture to grow into; the three-OS gate is the now-state and exercises the same bug class.

---

## 11. Existing-asset migration

### Blueprint re-parenting

| Asset | Old parent | New parent | Notes |
|---|---|---|---|
| `BP_Footballer` | `AFootballerCharacter` | `AFootballerVisual` | Loses CMC, loses input bindings (now in `SimInputCollector`) |
| `BP_OpponentFootballer` | `AOpponentFootballerCharacter` | `AFootballerVisual` | `ControllerIndex = 0xFF` (sim-driven stationary) |
| `BP_SoccerBall` | `ASoccerBall` | `ASoccerBallVisual` | Disables simulate-physics on mesh component |
| `BP_GoalTrigger` | `AGoalTrigger` | `AGoalTrigger` (same class, lightly adjusted) | Queries `SimHostSubsystem::GetBallPositionWorld()` |
| `BP_SoccerGameMode` | `ASoccerGameMode` | `ASoccerGameMode` | Spawns subsystem on `BeginPlay` |
| `ABP_Footballer` | `UFootballerAnimInstance` | `UFootballerVisualAnimInstance` | Computes Speed/Direction from interpolated transform deltas |

Re-parenting is driven by `Scripts/editor/reparent_blueprints.py`, executed by:

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
    "$PWD/Edge26.uproject" -run=PythonScript -script="Scripts/editor/reparent_blueprints.py"
```

The script is **idempotent**: running it twice is a no-op. The resulting `.uasset` files are committed to the repo (UE5 BPs serialize parent class into the asset).

### C++ classes deleted in v0

- `AFootballerCharacter` (`Source/Edge26/Public/Player/FootballerCharacter.h` + `.cpp`)
- `UFootballerAnimInstance` (`Source/Edge26/Public/Player/FootballerAnimInstance.h` + `.cpp`)
- `ASoccerBall` (`Source/Edge26/Public/Ball/SoccerBall.h` + `.cpp`)
- `AOpponentFootballerCharacter` (`Source/Edge26/Public/AI/OpponentFootballerCharacter.h` + `.cpp`)
- `AOpponentAIController` (`Source/Edge26/Public/AI/OpponentAIController.h` + `.cpp`)

### C++ classes kept

- `ASoccerGameMode`, `ASoccerHUD`, `AGoalTrigger`, `UBroadcastSpringArmComponent` — these are I/O / UI / camera. They don't own gameplay state and don't need to move.

### Mannequin animations

Keep for v0 cosmetics only:
- `Content/Characters/Mannequins/Anims/Unarmed/MM_Idle.uasset`
- `Content/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.uasset`
- `Content/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.uasset`

All other Mannequin clips (rifle/pistol/death/attack) are deleted in a **follow-up cleanup PR** after v0 ships — not in v0 itself.

### Other follow-up cleanup (not v0)

- `Content/ThirdPerson/` — third-person template residue
- `Content/LevelPrototyping/Interactable/JumpPad/`, `/Door/` — not football
- `Content/Input/Actions/IA_Jump.uasset`, `IA_MouseLook.uasset`, `IMC_Default.uasset`, `IMC_MouseLook.uasset` — unused
- `Content/NewBlueprint.uasset` — orphan

### Level

`Content/Levels/L_Pitch.umap` stays. Two Player Start actors, two `BP_GoalTrigger` placements, one `BP_SimHostBootstrap` (new), one `BP_SoccerBall`, two `BP_Footballer` placements. The Mannequin meshes stay assigned to `BP_Footballer`.

### RUNBOOK rewrite

`RUNBOOK.md` is rewritten as the final milestone (M7) of v0. The new content covers:
- New project bring-up: clone, generate project files, run `check_determinism.sh`, build editor.
- Editor-side workflow: how to set up the level, where the BPs are, what the new Python re-parent script does and when to run it.
- Sim-side workflow: how the standalone binary works, how to add a new test replay, how to update baselines.
- Pitfalls table (updated for the new architecture).

The current RUNBOOK's "where I would start tomorrow" section is removed — that was a prototype-era artifact.

---

## 12. PROGRESS.md format

`PROGRESS.md` lives at the repo root and is the living status tracker. Updated at the end of every coherent unit of work (not after every commit). Three sections, in this order:

### 12.1 Current status

One paragraph. Rewritten every update — never appended. Always reflects "where are we right now."

Example:
> We are in **Phase 1: Sim Core v0**, milestone **M3 of M7** (Snapshot/Restore). Fixed-point math library is complete and unit-tested across Linux/macOS/Windows; the SimWorld tick + state structs are landed. Working on snapshot/restore + xxhash + RNG next. ETA on v0 completion: ~N more sessions.

### 12.2 Roadmap

Checkboxes by phase and milestone. Items checked when they land on `main`. Phases beyond Phase 1 are placeholders that get expanded as they begin.

```markdown
### Phase 1: Deterministic Sim Core v0  ←  current
- [x] M1. Fixed-point math library (`Fixed64`, `Fixed32`, `FixedVec3`, trig, sqrt)
- [x] M2. SimWorld tick loop + InputFrame + SimBall/SimPlayer state structs
- [ ] M3. Snapshot/Restore + xxhash + RNG
- [ ] M4. Standalone headless binary + 3 input streams
- [ ] M5. `check_determinism.sh` + GitHub Actions workflow + baseline files
- [ ] M6. UE5 adapter (SimHost subsystem, AFootballerVisual, ASoccerBallVisual, reparent script)
- [ ] M7. RUNBOOK rewrite for the new architecture

### Phase 2: Spatial Value Model + 22-player AI  (placeholder)
### Phase 3: Motion-matching animation + procedural ball-contact IK  (placeholder; render-side only per §3)
### Phase 4: Rollback netcode  (placeholder)
### Phase 5: Economy & compliance backend  (separate repo)
```

Renaming items is fine; adding items mid-phase is fine; deleting completed items is not (the historical record matters).

### 12.3 Activity log

Dated, newest first. One entry per work session. Format: *what landed, what's blocked, what's next.* Bullets, not paragraphs. No editorializing — no "great progress!", no success theater. If a session produced extensive detail it lives in commit messages and PR descriptions; the log is the executive summary.

```markdown
### 2026-05-15 — Session 1
- Read project_breakdown.md; aligned with user on first slice (deterministic sim core).
- Brainstormed v0 design end-to-end; spec committed to docs/superpowers/specs/2026-05-15-sim-core-v0-design.md.
- Decisions locked: D1–D9 (see spec §2).
- Next: implementation plan via writing-plans skill, then M1 (fixed-point math library).
```

### 12.4 Rules

1. Status paragraph is rewritten every update — never append-only.
2. Roadmap items checked when work merges. Don't pre-check.
3. Activity log: newest first, factual only, brief.
4. The doc is the source of truth for "what's the project state"; commit messages and PRs are the source of truth for "what was done in this change."

---

## 13. Scope cuts (what is explicitly NOT in v0)

These are deliberate cuts. Mid-implementation, when you (or I) feel the pull to add one of these, the answer is no — it goes in a separate slice.

### Gameplay

- AI for off-ball players. Both v0 players are human-controlled or stationary.
- Ball physics realism (Magnus spin force, surface-dependent friction, wind).
- Player movement realism (momentum, inertia, stamina, agility-attribute scaling).
- Player-player collisions. Two players can occupy the same space in v0.
- Player-ball "control" (touches, redirects, dribbling cadence). The ball does not react to body contact in v0.
- Realistic kicking (power meter, aiming, contact quality, foot-roll IK). Kicks are fixed-impulse placeholders in v0.
- Goalkeepers (treated as outfield).
- Set pieces, restarts, fouls, offsides, throw-ins.
- Match clock progression, halftime, fulltime. Clock runs cosmetically.

### Architecture

- Rollback execution. The snapshot/restore primitive exists and is tested by the standalone binary, but the `SimHostSubsystem` does not use it during PIE.
- Multiplayer of any kind.
- Console determinism runners (PS5, Xbox).
- Replays-as-feature (input stream is already a replay; no in-game UI to record/play back).
- Anti-cheat integration.
- Telemetry / analytics pipeline.
- Server-authoritative ranked match mode.
- Data-driven config (sim parameters in `.ini` or BP-editable tables). v0 uses `constexpr` C++.
- Economy / compliance backend. Different repo entirely.

### Animation

- Motion-matching (UE5 Pose Search integration).
- Procedural ball-contact IK.
- Layered rig (locomotion + upper-body action overlay + IK + look-at).
- Mocap database. The breakdown's 8–15h is multi-year, multi-vendor work.
- Foot planting.

If the user (or I) asks "can we just add small thing X from this list" mid-v0, the answer is: open a separate doc, scope it as its own slice, ship v0 first.

---

## 14. Acceptance criteria (v0 is done when…)

1. `Scripts/check_determinism.sh` exits 0 on Linux, macOS, and Windows runners in a passing CI workflow.
2. `Scripts/lint_sim.sh` (the §4 grep gate) exits 0 on a fresh `Edge26Sim/` tree.
3. `Source/Edge26Sim/Edge26Sim.Build.cs` lists `Core` as its only public dependency, and the module compiles with `-Wall -Wextra -Werror` (`/W4 /WX` on MSVC).
4. `Source/Edge26SimStandalone` builds with `cmake --build` on any of the three platforms without a UE5 toolchain present.
4. The standalone binary's `--rollback-test` mode passes — snapshot/restore round-trip produces identical hashes.
5. In PIE on a developer's machine:
   - `BP_Footballer` instance visible in `L_Pitch` is driven by sim state. Verification: console command `edge26.sim.set_player_pos 0 100 200 0` updates `SimWorld.State.Players[0].Position`; visual actor's drawn transform tracks that across multiple ticks.
   - WASD moves P1; movement is deliberately simple — direction follows stick with bounded acceleration, no momentum preservation through turns, no stamina. Identical input from identical state produces identical motion frame-to-frame.
   - Pressing Pass/Shoot/Chip near the ball applies a fixed impulse; the ball flies/rolls per the v0 ball model in §8.
   - The ball entering a goal trigger raises a `GOAL!` HUD event.

   A small `SimDebug.h` exposes the console commands needed for verification (`set_player_pos`, `set_ball_pos`, `set_ball_vel`, `dump_state`). These are debug-build-only and disabled in shipping configs.
6. `PROGRESS.md` exists at repo root with the format from §12 of this doc, and reflects the milestones above as checked.
7. `RUNBOOK.md` is rewritten and accurate against the new architecture.
8. The decision log in §2 of this doc is up to date — any decisions revised during implementation are recorded with reasoning.

---

## 15. Out-of-scope decisions parked for later

Decisions deferred to subsequent slices, listed so we don't relitigate them while building v0:

- **Physical units inside the sim.** v0 uses cm + cm/s (matches UE5 default and is convenient for the visual layer). The breakdown's later motion-matching slice may prefer meters; revisit then.
- **Wall-clock time vs. tick-time for match clock.** v0 runs the clock as ticks × 20ms cosmetically; final answer comes with the spectator-mode / TV-broadcast layer.
- **Networking transport.** Rollback design chooses between UE5's transport, plain UDP, and a third-party library (e.g. yojimbo). Decided when rollback ships, not before.
- **Console-platform CI strategy.** Real PS5/Xbox runners require publisher/dev-kit relationships. Decided when that relationship exists.
- **Asset pipeline for football-specific content** (meshes, kits, stadium, crowd). Decided when the motion-matching / visual-polish slice starts.

---

## Appendix A — File-level inventory of new files created in v0

```
Source/Edge26Sim/
    Edge26Sim.Build.cs
    Public/Edge26Sim.h
    Public/Math/Fixed.h
    Public/Math/FixedVec.h
    Public/Math/FixedAngle.h
    Public/Math/Trig.h
    Public/Math/Sqrt.h
    Public/Math/Atan2.h
    Public/Math/Rng.h
    Public/Math/Hash.h            (vendors xxhash64; MIT)
    Public/Math/Mul64.h           (platform-cond. 64x64→128 mul; only platform-conditional code)
    Public/Sim/InputFrame.h
    Public/Sim/BallState.h
    Public/Sim/PlayerState.h
    Public/Sim/WorldState.h
    Public/Sim/SimWorld.h
    Public/Sim/Constants.h        (Fixed64 PitchHalfLen, JogSpeed, ShotSpeed, Gravity, etc.)
    Private/Edge26Sim.cpp
    Private/Math/Trig.cpp         (LUT)
    Private/Math/Atan2.cpp
    Private/Math/Sqrt.cpp
    Private/Sim/SimWorld.cpp
    Private/Sim/SimWorld_Player.cpp
    Private/Sim/SimWorld_Ball.cpp

Source/Edge26SimStandalone/
    CMakeLists.txt
    main.cpp
    tests/test_math.cpp
    tests/test_snapshot.cpp
    tests/replays/basic.input
    tests/replays/basic.expected.hashes
    tests/replays/ball_only.input
    tests/replays/ball_only.expected.hashes
    tests/replays/rollback_torture.input
    tests/replays/rollback_torture.expected.hashes
    tests/replay_generator.cpp    (produces the .input files from a small DSL; committed alongside)

Source/Edge26/Public/Adapter/
    SimHostSubsystem.h
    SimInputCollector.h
    FootballerVisual.h
    SoccerBallVisual.h
    FootballerVisualAnimInstance.h
    SimHostBootstrap.h
Source/Edge26/Private/Adapter/  (mirrored .cpp tree)

Scripts/
    check_determinism.sh
    update_determinism_baseline.sh
    lint_sim.sh                       (§4 grep gate; FORBIDDEN-token check)
    editor/reparent_blueprints.py

.github/workflows/
    determinism.yml

docs/superpowers/specs/
    2026-05-15-sim-core-v0-design.md   (this file)

PROGRESS.md
```

## Appendix B — Files modified in v0

```
Source/Edge26/Edge26.Build.cs                          (depends on Edge26Sim)
Source/Edge26/Private/Game/SoccerGameMode.cpp          (spawn subsystem on BeginPlay)
Source/Edge26/Private/Game/GoalTrigger.cpp             (query subsystem for ball pos)
Edge26.uproject                                         (declare Edge26Sim module)
RUNBOOK.md                                              (rewritten as M7)
```

## Appendix C — Files deleted in v0

```
Source/Edge26/Public/Player/FootballerCharacter.h
Source/Edge26/Private/Player/FootballerCharacter.cpp
Source/Edge26/Public/Player/FootballerAnimInstance.h
Source/Edge26/Private/Player/FootballerAnimInstance.cpp
Source/Edge26/Public/Ball/SoccerBall.h
Source/Edge26/Private/Ball/SoccerBall.cpp
Source/Edge26/Public/AI/OpponentFootballerCharacter.h
Source/Edge26/Private/AI/OpponentFootballerCharacter.cpp
Source/Edge26/Public/AI/OpponentAIController.h
Source/Edge26/Private/AI/OpponentAIController.cpp
```

---

*End of design.*
