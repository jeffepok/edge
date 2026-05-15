# Edge 26 — Progress

## Current status

We are in **Phase 1: Sim Core v0**, milestone **M4 of M7** (headless
replay binary). M3 is complete: SimWorld::Snapshot/Restore are POD
copies; HashState wraps xxhash64 over the whole state struct; full
rollback round-trip (advance N → snapshot → burn divergent ticks →
restore → advance N correct → hash matches first run) is green; per-tick
hash stability across two identical runs verified. 30 tests passing.
Next: standalone replay binary + 3 input streams + baselines.

## Roadmap

### Phase 1: Deterministic Sim Core v0  ←  current
- [x] M0. Module scaffolding (Build.cs, uproject, CMake, PROGRESS.md, lint script)
- [x] M1. Fixed-point math library (`Fixed64`, `Fixed32`, `FixedVec3`, trig, sqrt, RNG, hash)
- [x] M2. SimWorld tick loop + InputFrame + SimBall/SimPlayer state structs
- [x] M3. Snapshot/Restore + xxhash + RNG rollback-test
- [ ] M4. Standalone headless binary + 3 input streams + replay generator
- [ ] M5. `check_determinism.sh` + GitHub Actions workflow + baseline files
- [ ] M6. UE5 adapter (SimHost subsystem, AFootballerVisual, ASoccerBallVisual, BP re-parent)
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
