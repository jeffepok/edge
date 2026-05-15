# Edge 26 — Progress

## Current status

**Phase 1: Sim Core v0 is COMPLETE and merged.** All seven milestones
(M1–M7) shipped: fixed-point math library (Q32.32 / Q16.16 with trig,
sqrt, atan2, rng, xxhash), POD sim state structs with explicit padding,
SimWorld tick (kinematic player + simple ball physics + kick impulses),
snapshot/restore/hash with rollback round-trip, headless replay binary
with 3 input streams and per-tick hash baselines, local + GitHub Actions
determinism gate, UE5 visual-shell adapter with SimHostSubsystem driving
interpolated transforms, rewritten RUNBOOK. Automated acceptance criteria
(spec §14 #1–#4, #6–#8) all pass; PIE acceptance (§14 #5) confirmed
working end-to-end.

We are at **Phase 2 M6 of M12** (Layer A team strategy). M5 (Layer B unit coordination)
is complete: PendingButtons field in FSimPlayerState (88 B layout preserved), PassSuccessProbability
+ BestPassReceiverIdx helpers, EvaluateOnBall evaluator (Pass/Shoot/Dribble/Hold/Clear), MaybeApplyKick
widened to consume AI PendingButtons (ResolveButtonsForPlayer helper), IntendedPassTarget-directed pass
normalization fixed (used Fixed64 operator/ to avoid overflow), UpdatePossession (pickup radius + out-of-pitch
clear, with overflow-safe delta clamping), Sim_PossessionFlipsOnPickup + Sim_AICarrierFiresPass tests pass;
28 self-tests; baselines verified; lint + CI gates green.
M5 (Layer B unit coordination) is complete: UnitOf(ERole) constexpr helper in Roles.h; UnitCoordination.h/.cpp
with UpdateDefensiveUnit (defensive line from avg CB/FB X + LineHeightBias, offside line from last-defender X vs
ball X, compactness as X-stddev, nearest-to-ball press nomination), UpdateMidfieldUnit (central-channel press,
line, compactness), UpdateAttackUnit (top-line from most-forward attacker, same-side FB overlap nomination);
UpdateAllUnits wired at 10 Hz in SimWorld::Step (before Layer C); Layer C Press block replaced with Layer B
nomination check (3× boost for nominee), MakeRunForward gets 2× overlap boost for nominated FB; overflow-safe
arithmetic used throughout (Fixed64::operator* instead of raw multiply-then-divide for sign flip); 48 self-tests
pass; baselines regenerated; lint + CI gates green. Judgment calls: plan's `raw * Sign.Raw / One` pattern
replaced with `Fixed64::operator*` to prevent int64 intermediate overflow.
Spec: `docs/superpowers/specs/2026-05-15-phase2-spatial-ai-design.md`. Plan:
`docs/superpowers/plans/2026-05-15-phase2-spatial-ai-plan.md`.

## Roadmap

### Phase 1: Deterministic Sim Core v0  ←  complete
- [x] M0. Module scaffolding (Build.cs, uproject, CMake, PROGRESS.md, lint script)
- [x] M1. Fixed-point math library (`Fixed64`, `Fixed32`, `FixedVec3`, trig, sqrt, RNG, hash)
- [x] M2. SimWorld tick loop + InputFrame + SimBall/SimPlayer state structs
- [x] M3. Snapshot/Restore + xxhash + RNG rollback-test
- [x] M4. Standalone headless binary + 3 input streams + replay generator
- [x] M5. `check_determinism.sh` + GitHub Actions workflow + baseline files
- [x] M6. UE5 adapter (SimHost subsystem, AFootballerVisual, ASoccerBallVisual, BP re-parent)
- [x] M7. RUNBOOK rewrite + final acceptance pass (PIE test + CI push remain as user manual steps)

### Phase 2: Spatial Value Model + 22-player AI  ←  current
- [x] M1. Roster expansion: 22 players, roles, formations, kickoff placement
- [x] M2. Spatial Value Model (5 fields × 1768 cells)
- [x] M3. Layer C off-ball intents
- [x] M4. Layer C on-ball decisions
- [x] M5. Layer B unit coordination (defensive line, press, overlap)
- [ ] M6. Layer A team strategy (mentality, late-game adjustments)
- [ ] M7. Offside enforcement
- [ ] M8. Simple goalkeeper AI
- [ ] M9. Player switching (auto + manual + camera)
- [ ] M10. AI debug overlay (heatmaps + intent arrows)
- [ ] M11. CI baselines for 22-player streams
- [ ] M12. AI tuning pass + final acceptance

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
- M7 landed: RUNBOOK fully rewritten for the new architecture (CMake/lint/PIE workflows, troubleshooting table, headless Python commandlet pattern). Automated acceptance criteria all green: determinism gate PASS, lint OK, Edge26Sim depends only on Core, standalone has no UE5 dylib, decision log D1–D9 current. PIE acceptance walk-through and CI push verification remain as user manual steps.
- All v0 acceptance criteria green except the two manual steps. Phase 1 is shippable.
- Post-merge PIE polish: filled BP-overlooked defaults into C++ constructors via ConstructorHelpers (SKM_Manny_Simple, ABP_Footballer, /Game/Input/* IAs, Engine sphere mesh) so re-parented BPs work without manual setup; SimHostSubsystem now seeds player/ball position from placed actor transforms (was teleporting to origin); IA_Look type fixed Boolean → Axis2D (Mouse XY can't bind to a Boolean action without ensure-fail); BP_OpponentFootballer set to AutoPossessPlayer=Disabled, ControllerIndex=1 via Python; IA_Move path mismatch fixed (loaded /Game/Input/IA_Move but IMC binds /Game/Input/Actions/IA_Move). WASD now drives the mannequin end-to-end. PIE acceptance criteria §14 #5 GREEN.
- M1 landed: kSimPlayerCount 2→22, ERole enum, FFormationSlot + kFormation_4_3_3, FSimPlayerState 64→88 B, SimWorld ctor places 22, ResetAllPlayersTo4_3_3 in adapter, 22-player snapshot baselines regenerated, all unit tests pass.
- M2 landed: FSpatialValueModel (70 KB) + FMatchState (184 B) embedded into FSimWorldState (72,936 B); 5 spatial-field update functions (Space, DefCoverage, LaneOccupancy, Threat, PassReception); UpdateSpatialFields wired into SimWorld::Step at 50 Hz; baselines regenerated; lint + CI gates green.
- M3 landed: EIntent enum (12 values, 7 off-ball + 5 on-ball stubs), FRoleWeights struct + kRoleWeightsTable[10 roles] with per-role multipliers, full EvaluateOffBall evaluator (7 intents: HoldPosition, MakeRunForward, DropToReceive, ProvideWidth, Press, TrackRunner, HoldDefensiveLine) scored from spatial value fields with saturation-safe distance arithmetic; UpdatePlayerAI wired into SimWorld::Step at 50 Hz; Sim_22PlayerTickStable smoke test (22 players, 100 ticks, position bounds check) passes; baselines regenerated; lint + CI gates green. Judgment call: F32() helper uses compile-time double with SIM-LINT-OK annotation (avoids 110 pre-computed integer literals).
- M4 landed: PendingButtons byte added to FSimPlayerState (at offset 62, _pad[2] → PendingButtons+_pad0, 88 B maintained); PassSuccessProbability + BestPassReceiverIdx helpers (blocker projection uses __int128 via SIM-LINT-OK); EvaluateOnBall evaluator (5 intents: Pass/Shoot/Dribble/Hold/Clear); UpdatePlayerAI routes on-ball vs off-ball per tick; MaybeApplyKick widened to `(ball, player, frame, worldState, playerIdx)` with ResolveButtonsForPlayer helper; IntendedPassTarget-directed pass using Fixed64 operator/ for overflow-safe normalization (plan used Raw*One/Raw which overflows for large distances); UpdatePossession (80cm pickup radius + out-of-pitch clear) with delta-clamped overflow protection; 28 self-tests (2 new: Sim_PossessionFlipsOnPickup, Sim_AICarrierFiresPass); baselines verified unchanged (replay streams don't exercise AI carriers near ball); lint + CI gates green. Judgment calls: (1) T4.1+T4.2 helper additions combined into single commit to satisfy -Werror,-Wunused-function; (2) plan's normalize formula `(raw * Fixed64::One) / d_raw` replaced with `Fixed64 operator/` to prevent int64 overflow.
- M5 landed: UnitCoordination.h/.cpp (UpdateDefensiveUnit, UpdateMidfieldUnit, UpdateAttackUnit, UpdateAllUnits); UnitOf(ERole) constexpr helper added to Roles.h; 10 Hz hook (every 5 ticks) in SimWorld::Step before Layer C; Layer C Press replaced with Layer B nomination (3× boost, nominee is nearest unit member to ball per unit type); MakeRunForward gets 2× boost when FB is the overlap-nominated player; overflow-safe arithmetic throughout (Fixed64::operator* instead of raw multiply/divide for sign-flip operations); 48 self-tests pass; baselines regenerated; lint + CI gates green. Judgment calls: plan's `bias.Raw * Sign.Raw / Fixed64::One` pattern overflows int64 for realistic bias values — replaced with Fixed64::operator* which uses __int128 internally; same fix applied to signed-forward ↔ absolute-X conversions in UpdateDefensiveUnit and UpdateAttackUnit.
