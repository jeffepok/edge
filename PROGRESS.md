# Edge 26 — Progress

## Current status

We are in **Phase 1: Sim Core v0**, milestone **M7 of M7** (RUNBOOK +
final acceptance). M6 is complete: Edge26 module depends on Edge26Sim;
AFootballerVisual + ASoccerBallVisual are render-only shells driven by
SimHostSubsystem (50Hz fixed accumulator tick, render-frame interpolation);
SimInputCollector translates Enhanced Input to FInputFrame; SimHostBootstrap
forces subsystem init in PIE; SoccerGameMode kickoff resets go through
the sim. All legacy classes deleted. BPs re-parented headlessly via Python.
Editor builds clean. Next: RUNBOOK rewrite + acceptance pass (PIE test
is a user manual step).

## Roadmap

### Phase 1: Deterministic Sim Core v0  ←  current
- [x] M0. Module scaffolding (Build.cs, uproject, CMake, PROGRESS.md, lint script)
- [x] M1. Fixed-point math library (`Fixed64`, `Fixed32`, `FixedVec3`, trig, sqrt, RNG, hash)
- [x] M2. SimWorld tick loop + InputFrame + SimBall/SimPlayer state structs
- [x] M3. Snapshot/Restore + xxhash + RNG rollback-test
- [x] M4. Standalone headless binary + 3 input streams + replay generator
- [x] M5. `check_determinism.sh` + GitHub Actions workflow + baseline files
- [x] M6. UE5 adapter (SimHost subsystem, AFootballerVisual, ASoccerBallVisual, BP re-parent)
- [ ] M7. RUNBOOK rewrite + final acceptance pass

### Phase 2: Spatial Value Model + 22-player AI  (placeholder)
### Phase 3: Motion-matching animation + procedural ball-contact IK  (placeholder; render-side only per spec §3)
### Phase 4: Rollback netcode  (placeholder)
### Phase 5: Economy & compliance backend  (separate repo)

## Activity log

### 2026-05-15 — Session 1
- Read project_breakdown.md; aligned on first slice (deterministic sim core).
- Brainstormed v0 design; spec committed to `docs/superpowers/specs/2026-05-15-sim-core-v0-design.md`.
- Implementation plan committed to `docs/superpowers/plans/2026-05-15-sim-core-v0-plan.md`.
- Decisions D1–D9 locked (see spec §2).
- Next: M0 module scaffolding, then M1 fixed-point math library.
- M0 landed: Edge26Sim module + standalone CMake project compile; lint_sim.sh passes on empty tree; build/sim/edge26_sim_replay runs (no UE5 dylib linked); editor build green with Cpp default (UE 5.7 rejects pinned Cpp17). Installed cmake 4.3.2 via Homebrew.
- M1 landed: Fixed64/Fixed32/FixedVec/FixedAngle, Sin/Cos LUT (1024 entries, lerp tolerance 128 ulps), 8-iter Newton Sqrt, 20-iter CORDIC Atan2, xorshift64 Rng, xxhash64. 18 unit tests pass in standalone; lint clean.
- M2 landed: POD state structs (12+80+64+224 B) with explicit padding, SimWorld with zero-init ctor, StepPlayer kinematic, StepBall (physics-grounded settle criterion — avoided limit cycle), MaybeApplyKick. Byte-identical determinism across two-run check passes. Fixed two plan oversights inline: FInputFrame is 12 bytes not 16 (uint32 alignment), ball settle needed post-bounce-vs-gravity check not just a velocity threshold.
- M3 landed: Snapshot/Restore/HashState. Rollback_FullRoundTrip green (advance 50, snap, burn 40 divergent ticks, restore, advance 50 correct → hash matches single-run baseline). Per-tick hash stability test confirms no hidden state. xxhash64 over 224 bytes = ~30ns/tick.
- M4 landed: edge26_sim_replay CLI + replay_generator + 3 binary input streams (basic 500t / ball_only 1000t / rollback_torture 2000t) + matching expected.hashes baselines (3500 lines committed). Rollback-test mode walks the torture stream successfully.
- M5 landed: check_determinism.sh (lint + build + self-test + replay-vs-baseline + rollback round-trip, ~5s), update_determinism_baseline.sh (idempotent), GitHub Actions matrix for linux/macos/windows. CI runs the gate on push to main and on every PR.
- M6 landed: full UE5 adapter. New classes — AFootballerVisual (APawn, no CMC, transform driven by sim), ASoccerBallVisual (AActor, no physics), USimHostSubsystem (50Hz fixed accumulator + render-frame interp + ResetBall/ResetPlayer), USimInputCollector (Enhanced Input → InputFrame), ASimHostBootstrap. SoccerGameMode + SoccerHUD + GoalTrigger rewired to new classes. 10 legacy source files deleted (FootballerCharacter, FootballerAnimInstance, SoccerBall, OpponentFootballerCharacter, OpponentAIController). Three plan/UE5 quirks caught & fixed inline: UE5 module needed Private/UE5/Edge26SimModule.cpp with IMPLEMENT_MODULE; macOS linker stripped Edge26Sim symbols without EDGE26SIM_API annotation (added fallback header); -Werror,-Wshadow caught a Ball local shadowing the member field. BPs re-parented via headless Python commandlet (-nullrhi). PIE acceptance is the remaining user step.
