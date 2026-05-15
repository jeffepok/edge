# Edge 26 — Runbook

Editor + sim-side companion to the C++ source. Top-to-bottom for first run; section-by-section for later sessions.

> **Conventions:** **[CLI]** = terminal command. **[Editor]** = UE5 editor required. **[USER]** = manual step.

---

## 1. First-time bring-up

### 1.1 Clone + regenerate project files [CLI]

```bash
cd /Users/jeffersonaddai-poku/Desktop/projects/Games/Edge26
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh" \
    -project="$PWD/Edge26.uproject" -game -engine
```

### 1.2 Run the determinism gate (no UE5 needed) [CLI]

```bash
./Scripts/check_determinism.sh
```

Expected: `PASS: all determinism checks`. Builds the standalone binary, runs the lint, replays three streams, verifies hashes, runs rollback round-trip. Takes ~5 seconds after the first build.

Requires `cmake` (install via `brew install cmake` on macOS if missing).

### 1.3 Build the editor [CLI or Xcode]

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex
```

Expect 5–15 min for a clean build; subsequent builds ~10–60 s.

### 1.4 Open the editor [CLI]

```bash
open Edge26.uproject
```

---

## 2. Architecture in 90 seconds

- **`Source/Edge26Sim/`** is a UE5 module that depends *only* on `Core`. It is pure C++ — no `UCLASS`, no Engine, no Chaos. It holds the entire deterministic 50 Hz sim (ball + 2 player kinematic states in Q32.32 fixed-point).
- **`Source/Edge26SimStandalone/`** is a CMake project that compiles the same Edge26Sim sources outside of UE5. Produces `edge26_sim_replay` (the headless determinism harness) and `replay_generator` (binary input stream generator).
- **`Source/Edge26/`** is the existing UE5 module — now a thin adapter. `USimHostSubsystem` owns a `SimWorld`, ticks it at 50 Hz, and drives `AFootballerVisual` / `ASoccerBallVisual` actors with interpolated state every render frame.
- **Determinism rules** (spec §4): no float, no `unordered_*`, no threads, no wall-clock, no Engine includes inside `Edge26Sim/`. Enforced by `Scripts/lint_sim.sh`, the Core-only `Build.cs`, the CMake build (which would fail if any UE5 leak existed), and the cross-platform GitHub Actions matrix.
- **Sim/render boundary principle** (spec §3): animation, IK, ragdolls are *forever* visual-only. The sim writes contact events; the render layer honors them. No deterministic IK.

---

## 3. The sim development loop

You will rarely touch the UE5 editor when working on the sim. Cycle is:

1. Edit code in `Source/Edge26Sim/` or `Source/Edge26SimStandalone/`.
2. Run `./Scripts/check_determinism.sh`. If it fails on baselines, that's expected — your change altered behavior.
3. If the diff to the baselines is intentional, run `./Scripts/update_determinism_baseline.sh`, inspect `git diff`, then commit the baselines as part of your PR.
4. If the diff is *not* intentional, you've introduced a sneaky non-determinism (or a bug). Triage with `--rollback-test` and per-tick hash dumps.

`./Scripts/lint_sim.sh` runs first inside `check_determinism.sh` and catches forbidden tokens. If lint fires on a comment or genuinely safe usage, add `// SIM-LINT-OK: <reason>` on that line.

---

## 4. Editor-side gameplay assets [Editor]

The C++ classes are abstract scaffolding. Editor-created assets supply the meshes, sounds, animations, and input maps.

### 4.1 Asset folder layout

```
Content/
├── Levels/                  L_Pitch
├── Input/                   IMC_Player, IA_Move, IA_Sprint, IA_Pass, IA_Shoot, IA_Chip
├── Blueprints/
│   ├── Game/                BP_SoccerGameMode, BP_GoalTrigger, BP_SimHostBootstrap
│   ├── Ball/                BP_SoccerBall   (parent: SoccerBallVisual)
│   └── Player/              BP_Footballer, BP_OpponentFootballer (parent: FootballerVisual)
│                            ABP_Footballer (parent: AnimInstance — cosmetic)
└── Characters/              Mannequins/...
```

### 4.2 Set the `ControllerIndex` on each footballer

Open each `BP_Footballer` placement in the level. In the Details panel, set:
- `BP_Footballer` instance for P1: `ControllerIndex = 0`.
- `BP_OpponentFootballer` for P2 (or the second `BP_Footballer`): `ControllerIndex = 1`, or `255` (0xFF) to make it stationary.

### 4.3 Wire `SimInputCollector` on the player BP

On `BP_Footballer`, select the `InputCollector` component and set:
- `DefaultMappingContext` → `IMC_Player`
- `IA_Move` → the existing `IA_Move` Input Action
- `IA_Sprint` → `IA_Sprint`
- `IA_Pass`   → `IA_Pass`
- `IA_Shoot`  → `IA_Shoot`
- `IA_Chip`   → `IA_Chip`

### 4.4 Place the bootstrap actor

Create a `BP_SimHostBootstrap` Blueprint subclass of `ASimHostBootstrap` (or use the C++ class directly). Drag one into `L_Pitch` at origin. Save the level.

### 4.5 PIE check (acceptance criteria from spec §14 #5)

Hit **Play in Editor**:
- HUD shows `HOM 0 - 0 AWY` + `KICKOFF` banner.
- WASD moves P1; movement is deliberately simple (no momentum preservation through turns — this is the v0 RC-car feel).
- Pass / Shoot / Chip near the ball impulses it.
- Ball into goal trigger fires `GOAL!`.

---

## 5. Re-parenting Blueprints after a C++ rename

If a C++ class an asset depends on is renamed, moved, or deleted:

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
    "$PWD/Edge26.uproject" -run=PythonScript \
    -script="$PWD/Scripts/editor/reparent_blueprints.py" -nopause -unattended -nullrhi
```

The script is idempotent. The `-nullrhi` flag skips the renderer load so the commandlet completes in ~1 minute instead of 5+. Commit the resulting `.uasset` diffs.

Edit the `REPARENT` list in the script to add/remove BP-to-class mappings.

---

## 6. Useful CLI

```bash
# Standalone determinism gate (no UE5):
./Scripts/check_determinism.sh

# Regenerate baselines after a deliberate sim behavior change:
./Scripts/update_determinism_baseline.sh

# Run a single replay and print hashes:
./build/sim/edge26_sim_replay --input Source/Edge26SimStandalone/tests/replays/basic.input --hash-every 1

# Rollback round-trip on the torture stream:
./build/sim/edge26_sim_replay --input Source/Edge26SimStandalone/tests/replays/rollback_torture.input --rollback-test --hash-every 0

# Self-test (math + snapshot tests):
./build/sim/edge26_sim_replay --self-test

# UE5 editor build:
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex

# Headless Python commandlet (any script):
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
    "$PWD/Edge26.uproject" -run=PythonScript \
    -script="$PWD/Scripts/editor/<script>.py" -nopause -unattended -nullrhi
```

---

## 7. Common pitfalls

| Symptom | Likely cause | Fix |
|---|---|---|
| `lint_sim.sh: FORBIDDEN (...)` | Sim code introduced a banned token | Replace with the deterministic equivalent or add `// SIM-LINT-OK: <reason>` if safe |
| `ROLLBACK MISMATCH at tick N` | State outside the snapshot | Byte-diff before/after the round-trip; the differing struct field is the culprit |
| macOS hashes ≠ Linux hashes in CI | Non-deterministic FP / hash-map iteration / wall-clock | Run `lint_sim.sh`; if clean, look for `__SIZEOF_INT128__` path differences in `Mul64.h` |
| Player doesn't move in PIE | `InputCollector` IA references not assigned in BP, or `IMC_Player` not set | Open `BP_Footballer`, fill the `InputCollector` component properties (§4.3) |
| Ball doesn't react to kick | `BP_SoccerBall` parent isn't `SoccerBallVisual`, or BP_Footballer is not facing the ball | Run `reparent_blueprints.py`; confirm Parent Class in the BP editor |
| Visuals lag a tick or teleport | Render-frame interpolation needs both `PrevState`/`CurrState` initialized; the `SimHostBootstrap` actor ensures the subsystem comes up in time | Drop a Bootstrap actor into `L_Pitch` |
| Edge26Sim symbols not found at link | `EDGE26SIM_API` missing on a new class; macOS linker dead-strips it | Add `EDGE26SIM_API` to the class declaration in the header (the macro is a no-op in standalone CMake) |
| Compile error `Cpp17 is no longer supported` | Pinned `CppStandard = CppStandardVersion.Cpp17` in a Build.cs | Remove the line; UE 5.7 default (Cpp20+) is fine |
| `-Werror,-Wshadow` build error in adapter | Local variable shadows a class field | Rename the local (e.g. `Ball` → `BallState`) |

---

## 8. Where I would start tomorrow

The sim core is foundation. Next slices (in priority order — pick one and brainstorm it into its own spec):

1. **Phase 2 — Spatial Value Model + 22-player AI.** The most visible quality jump. Builds on the sim's deterministic kinematic loop.
2. **Phase 3 — Motion-matching animation + procedural ball-contact IK (render-side per spec §3).** The most visible feel jump. Requires real football mocap.
3. **Phase 4 — Rollback netcode.** Builds on the snapshot/restore + headless harness.

`project_breakdown.md` has the high-level vision for all of them. `docs/superpowers/specs/` is where individual phase designs land. `docs/superpowers/plans/` is where they get broken down into bite-sized tasks like this v0 plan was.
