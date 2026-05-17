# Edge 26 — Phase 3: Motion-matching Animation + Procedural Ball-contact IK

**Status:** Approved 2026-05-17. Locked decisions D1-D10 below.

**Goal:** Replace Phase 2's tick-discrete kick model (which produces visible "ball flicker between players when contested") with animation-driven control. AAA-quality stack: motion matching for locomotion, foot IK for ground contact, ball-contact IK for kick / header / save moments, and a GK ↔ outfield role split.

**Constraint inherited from spec §3 of the project breakdown:** **"animation/IK forever visual-only."** The deterministic 50 Hz sim does not change in any way. All Phase 3 work lives in `Source/Edge26/` (the UE5 module). The sim continues to fire kick impulses on the tick its AI decides; the renderer hides that step-discontinuity behind animation.

---

## 1. Architecture

Three layers stacked render-side:

```
SimHostSubsystem (Edge26)
  │
  ├─ keeps a ring buffer of last ~25 sim snapshots (~500 ms history)
  │
  ▼
RenderSnapshotBuffer (NEW — Edge26)
  │ delays snapshot consumption by 10 ticks (~200 ms)
  │ emits per-render-frame transforms + deferred events (KickEvent, ...)
  │
  ▼
AFootballerVisual (Edge26)
  │ SetActorTransform from delayed snapshot
  │ broadcasts events to per-pawn anim instance + IK component
  │
  ▼
Anim Blueprint: ABP_Footballer (outfield) / ABP_Goalkeeper (GK)
  │ Motion Matching state-tree (pose search + chooser tables)
  │ TwoBoneIK foot planting (left + right leg)
  │ Kick montage slot triggered by KickEvent
  │ Foot-to-ball IK alpha during kick montage
```

**Key principle.** Render-time state is always ~200 ms behind sim-time state. The renderer's only "ahead-of-time" knowledge of a kick is that 200 ms render-state delay — the sim still ticks at 50 Hz writing snapshots, the renderer just reads from 10 ticks behind. During that 200 ms window, an animation montage plays the foot wind-up; the foot meets the ball at the contact frame; the ball "releases" along its already-computed sim trajectory.

This is how FIFA and eFootball coordinate animation with simulation. It's invisible to the user (everything is 200 ms delayed uniformly, including their own input → pawn motion).

---

## 2. Components

### New files

| File | Responsibility |
|---|---|
| `Source/Edge26/Public/Adapter/RenderSnapshotBuffer.h` + `Private/.cpp` | Ring buffer of `FSimWorldState` snapshots (25-slot, ~500 ms). `Push(simTick, snapshot)` enqueues; `PopForTick(consumeTick)` returns the snapshot for that tick plus a `TArray<FAnimEvent>` computed by diffing this snapshot's relevant fields against the previously consumed snapshot. |
| `Source/Edge26/Public/Animation/FootballerAnimEvents.h` | `UENUM EFootballerAnimEvent { Kick, Header, Tackle, BallReceived, GoalkeeperSave, GoalkeeperCatch, GoalkeeperThrow }` + `FAnimEventPayload` struct (player idx, kick direction, kick magnitude, ball target position). |
| `Source/Edge26/Public/Animation/FootballAnimInstance.h` + `Private/.cpp` | Base `UAnimInstance` subclass with motion-matching trajectory inputs (velocity, acceleration, future trajectory points), kick-montage slot, foot-IK targets, played-event queue (consumed by montage logic in BP). |
| `Source/Edge26/Public/Animation/BallContactIKComponent.h` + `Private/.cpp` | Component on `AFootballerVisual` that owns the foot-IK alpha curve during a kick montage. Reads the `BallContact` anim notify; ramps IK alpha 0→1 over the wind-up frames, snaps the foot bone to current ball position at the contact frame, ramps 1→0 over follow-through. |
| `Content/Animation/MotionMatching/MMDB_Outfield.uasset` | `UPoseSearchDatabase` populated from Game Animation Sample locomotion + Mixamo football overlays. |
| `Content/Animation/MotionMatching/MMDB_Goalkeeper.uasset` | `UPoseSearchDatabase` populated from Game Animation Sample upper-body neutral + Mixamo GK pack. |
| `Content/Blueprints/Player/ABP_Footballer_MM.uasset` | New motion-matched anim BP. Replaces (or shadows during transition) the existing strafe-blendspace `ABP_Footballer`. |
| `Content/Blueprints/Player/ABP_Goalkeeper.uasset` | GK-specific anim BP using `MMDB_Goalkeeper`. |
| `Content/Blueprints/Player/BP_Goalkeeper.uasset` | BP subclass of `AFootballerVisual` that defaults to `ABP_Goalkeeper`. |
| `Scripts/editor/import_game_anim_sample.py` | Headless Python that drops the Game Animation Sample plugin's animations into `MMDB_Outfield` and saves the database. |
| `Scripts/editor/retarget_mixamo_to_manny.py` | Headless Python that runs UE5's IK Retargeter on a folder of Mixamo FBX imports, outputs Manny-skeleton anims, adds `BallContact` + `RecoverEnd` anim notifies at configurable frames. |

### Modified files

| File | Change |
|---|---|
| `Source/Edge26/Private/Adapter/SimHostSubsystem.cpp` | After each `Sim->Step()`, push the new snapshot into `RenderSnapshotBuffer`. `DriveVisuals` reads from buffer (10 ticks behind) instead of from `CurrState` directly. Broadcast emitted events to per-pawn `FootballAnimInstance`s and `BallContactIKComponent`s. |
| `Source/Edge26/Public/Adapter/SimHostSubsystem.h` | Add `RenderSnapshotBuffer Buffer` member; expose `OnAnimEvent(playerIdx, event)` route. |
| `Source/Edge26/Public/Adapter/FootballerVisual.h` + `.cpp` | New properties for anim BP: `FVector TrajectoryVelocity`, `FVector TrajectoryAcceleration`, `TArray<FVector> TrajectorySamples` (length 4). Owns a `BallContactIKComponent`. |
| `Content/Blueprints/Player/BP_Footballer.uasset` | Default `AnimInstanceClass` flipped from `ABP_Footballer` (blendspace) → `ABP_Footballer_MM` (motion matching). |
| `Content/Levels/L_Pitch.umap` | Two of the 22 placed `BP_Footballer` instances (players 0 + 11, the GKs) re-parented to `BP_Goalkeeper`. Done via headless Python so it's reproducible. |

### Sim-side: zero changes

The sim still ticks 50 Hz, writes `PendingButtons` per player per tick, fires `MaybeApplyKick`, updates `PossessionPlayer`, etc., exactly as before. The Phase 2 determinism gate (`Scripts/check_determinism.sh`) continues to pass without any modification. The animation pipeline is downstream.

---

## 3. Data flow per tick

### Sim-tick path (50 Hz, unchanged)

1. Sim runs Layer A/B/C, fires `MaybeApplyKick`, computes new ball state, settles possession.
2. `SimHostSubsystem::Tick` does `Sim->Snapshot(CurrState)` and **pushes a copy into `RenderSnapshotBuffer`** (ring buffer of 25 entries indexed by tick number).

### Render-frame path (60 – 120 Hz, new)

1. `SimHostSubsystem::Tick` (render-frame portion) computes `consumeTick = currentSimTick - kRenderDelayTicks` (= 10).
2. `RenderSnapshotBuffer::PopForTick(consumeTick)` returns the snapshot for that tick along with a `TArray<FAnimEventPayload>` of events computed by diffing this snapshot's `PendingButtons[*]` / `PossessionPlayer` / `Match.PendingOffsideCallTeam` etc. against the previously consumed snapshot.
3. Snapshot is split into:
   - **Transforms** → `DriveVisuals(alpha)` interpolates between the two most recently consumed snapshots, applies transforms to `AFootballerVisual` and `ASoccerBallVisual`.
   - **Events** → broadcast to each affected pawn's `BallContactIKComponent` and `FootballAnimInstance`.
4. Anim BP reads the new state — `Speed`, `RelativeDirection`, `TrajectoryVelocity`, `TrajectorySamples`, any active events — and the motion-matching pose-search picks the best frame to blend to.
5. `BallContactIKComponent` advances any active kick montage; on `BallContact` anim notify, the ball's visual position is "released" to start moving along its sim trajectory (which has already been computed 200 ms ago — the foot just had to catch up).

### Timing diagram

```
sim:    T-10  T-9  T-8 ... T-1  T   ← sim is HERE, tick T just finished
buffer: [s-25 ... s-10 ... s-1 s]   ← all 25 snapshots stored
render:                  ↑ reads here  (consumeTick = T-10)
                         │
                         └─ if PendingButtons[carrier] & Pass at consumeTick,
                            renderer plays kick montage NOW. BallContact notify
                            lands ~200 ms into the montage — i.e., at exactly
                            real-wall-clock-T, when the ball would have been
                            "released" in sim time. Foot meets ball.
```

### Event extraction rules (snapshot diff)

| Sim signal (rising-edge on consumed snapshot vs previous) | Render event |
|---|---|
| `PendingButtons[i] & Pass` set this tick, unset last | `Kick(playerIdx=i, kind=Pass, dir=BallVelocity, magnitude=PassSpeed)` |
| `PendingButtons[i] & Shoot` rising edge | `Kick(playerIdx=i, kind=Shoot, ...)` |
| `PendingButtons[i] & Chip` rising edge | `Kick(playerIdx=i, kind=Chip, ...)` |
| `PossessionPlayer` changed AND prev snapshot's `Ball.Velocity.Z > 0` (ball was airborne) | `BallReceived(playerIdx=new carrier)` — play first-touch trap |
| `Ball.Velocity` went from non-zero to zero AND `PossessionPlayer` is a GK | `GoalkeeperSave(playerIdx=GK)` |
| `PossessionPlayer` is a GK AND prev wasn't | `GoalkeeperCatch(playerIdx=GK)` |
| `PendingOffsideCallTeam != 0xFF` (rising edge) | Reserved for v1 (whistle SFX); no anim for v0 |

---

## 4. Motion matching setup

### Two databases, one per role-bucket

| Database | Source animations | Trajectory inputs |
|---|---|---|
| `MMDB_Outfield` | Game Animation Sample locomotion subset (walk/jog/run, strafes, stop/start, turn-in-place) + Mixamo football overlays (kick wind-ups, header attempts, slide tackles, dribble idle, ball-roll under-foot) | Velocity (cm/s, world XY), Acceleration, **future trajectory** (4 sample points at +10/+20/+30/+40 frames), Facing direction relative to velocity, IsGrounded |
| `MMDB_Goalkeeper` | Game Animation Sample upper-body neutral + Mixamo GK pack (stance, side-shuffle, dive left/right at three heights, parry, catch, throw, kick-from-hands, goal-kick) | Velocity, Acceleration, future trajectory, distance-to-ball (X axis), ball-velocity-toward-goal scalar |

### Future-trajectory generation

The `FootballAnimInstance` extrapolates the player's intended motion from the sim's `AITargetPosition` and current velocity, then feeds the pose-search:

```cpp
// Each render frame, before pose-search runs:
FVector currentVel  = TrajectoryVelocity;      // from interpolated transform delta
FVector targetPos   = AITargetPosition;        // from latest consumed sim snapshot
FVector futureDir   = (targetPos - GetActorLocation()).GetSafeNormal2D();
float   targetSpeed = currentVel.Size();       // sim-determined

// Build 4 future trajectory samples (10, 20, 30, 40 frames ahead):
for (int i = 0; i < 4; ++i) {
    float t = (i + 1) * 10 / 60.0f;            // seconds into future
    FVector sample = GetActorLocation() + futureDir * targetSpeed * t;
    TrajectorySamples[i] = sample;
}
```

Pose search then picks the animation pose that best matches **both** the current body velocity and the projected future trajectory, producing natural acceleration / deceleration / cornering anims without any explicit blendspace authoring.

### Authoring workflow (one-time setup, not on the hot path)

1. Import Epic's Game Animation Sample plugin (free, UE 5.5+ Marketplace).
2. Retarget Mixamo packs onto the Manny skeleton via UE 5.7's IK Retargeter (`IKR_Mixamo_to_Manny`).
3. Open `MMDB_Outfield` in editor; drag locomotion anims into the "Locomotion" source group, Mixamo football overlays into a "Football" source group.
4. For each Mixamo football anim, add anim notifies: `BallContact` (the frame where foot hits ball) and `RecoverEnd` (frame where weight is back centered).
5. Hit "Build Database" — UE5 hashes poses + trajectories for fast nearest-neighbor lookup at runtime.
6. Repeat for `MMDB_Goalkeeper`.

The databases are content assets, not code. They commit to `Content/Animation/MotionMatching/`. The C++ side just references them by path in the AnimBP.

### Foot IK + ball-contact IK

- **Foot IK (always-on):** `TwoBoneIK` per leg, target = ground-plane projection of the foot bone + small Z offset (flat pitch → trivial: feet just don't penetrate the floor and don't float). Solver bias: hip drops if both feet would otherwise float.

- **Ball-contact IK (only during kick montage):**

  ```cpp
  // BallContactIKComponent::TickComponent
  if (CurrentMontageProgress < kWindUpFrames) {
      // Foot IK alpha ramps 0 → 1 as foot approaches contact frame.
      footIKAlpha = lerp(0, 1, progress / kWindUpFrames);
      footIKTarget = ComputeFootInterceptPos(currentBallPos, montageEndPos);
  } else if (CurrentMontageProgress == kContactFrame) {
      // Snap foot bone exactly to ball position. Ball is "released" from this frame.
      footIKTarget = currentBallPos;
      footIKAlpha = 1.0f;
      BallContactReceived.Broadcast();  // tells visual to start rendering ball from sim trajectory
  } else {
      // Follow-through: alpha ramps 1 → 0 as foot returns to anim's natural path.
      footIKAlpha = lerp(1, 0, (progress - kContactFrame) / kFollowThroughFrames);
  }
  ```

---

## 5. Testing & acceptance

### Determinism gate stays unchanged

The sim isn't modified — `Scripts/check_determinism.sh` continues to pass without any work. This is the central architectural guarantee: Phase 3 is render-side, and render-side has no determinism contract.

### New automated tests (UE5-side, not in standalone sim)

| Test | Type | Verifies |
|---|---|---|
| `RenderSnapshotBuffer.Test_DelayRespected` | UE5 C++ Automation Test | Push 30 snapshots tagged with tick 0..29; verify `PopForTick(consumeTick=10)` returns tick 0's snapshot; tick 20 returns tick 10's. Decoupling proof. |
| `RenderSnapshotBuffer.Test_EventDiffEmitsKick` | UE5 C++ Automation Test | Snapshot A: PendingButtons[0]=0. Snapshot B: PendingButtons[0]=Pass. Push both; pop in order; verify the second pop returns a `KickEvent(kind=Pass, playerIdx=0)` event in the deferred-event list. |
| `BallContactIK.Test_AlphaRampSchedule` | UE5 C++ Automation Test | Drive a synthetic kick montage through 30 frames; verify `footIKAlpha` traces the 0 → 1 → 1 → 0 envelope (wind-up rise, contact spike, follow-through fall). |
| `MotionMatching.Test_TrajectoryComputation` | UE5 C++ Automation Test | Given current velocity + AITargetPosition, verify the 4 future-trajectory samples land at the right cm offsets. No anim runtime needed. |

All four are unit tests of render-side logic with no UE5 viewport / PIE dependency. They run in `Edge26Editor` via the Session Frontend automation tab.

### Acceptance criteria (Phase 3 v0)

1. **Motion matching active for all 22 players.** Visible: no two players in identical idle poses; locomotion blends smoothly through start/stop/turn transitions.
2. **Foot IK plants feet on pitch.** Visible: no sliding, no foot floating, no foot penetrating floor.
3. **Kick events trigger animation montages.** Visible: when carrier passes, the foot visibly swings; the ball "releases" from the foot at the contact frame (not from the player's pelvis).
4. **GK has dedicated animation set.** Visible: when GK saves a shot, plays a dive animation; when GK is in stance, plays goalkeeper stance (not the outfielder idle).
5. **No regression to Phase 2 acceptance.** `check_determinism.sh` still PASS on 3 OSes; AI debug overlay still functional; player switching still works.
6. **5-minute PIE soak.** No crashes, no anim-blueprint errors in Output Log, no missing-asset warnings.
7. **Render delay is imperceptible.** Subjective: PIE feels responsive; user can't tell the renderer is 200 ms behind the sim.
8. **`PROGRESS.md` updated** — Phase 3 milestones logged.

### Out of scope for v0 (parked for Phase 3.1 / 3.2 / later phases)

| Cut | Lives in (future phase) |
|---|---|
| Ragdoll on tackle / collision | Physics & collisions phase |
| Crowd / stadium ambience | Visual polish phase |
| Replay / instant-replay system respecting the buffer | Replay phase |
| Crowd reactions to goals / saves | Visual polish phase |
| Referee / linesman animations | Set Pieces & Officials phase |
| Cinematic celebration sequences post-goal | Match Flow phase |
| Per-player animation variations beyond GK / outfield (e.g., tall striker vs short winger) | Phase 3.1 (data-driven roles) |
| Stamina-driven animation slowdowns | Player Feel phase |
| Per-foot preference (left- vs right-footed kick selection) | Phase 3.1 |
| Header in midair (jump anim hookup) | Phase 3.1 |
| IK for hand-on-ball (GK catching pose) | Phase 3.1 |

---

## 6. Decision log

| # | Decision | Rationale |
|---|---|---|
| D1 | Full AAA stack v0: motion matching + foot IK + ball-contact IK + GK/outfield split | User chose maximum fidelity over incremental polish |
| D2 | Animation library: Game Animation Sample (locomotion) + Mixamo (football moves), retargeted to Manny | Free, MM-ready, fast to assemble |
| D3 | Two role-bucket anim DBs: `MMDB_Outfield`, `MMDB_Goalkeeper`. No further granularity in v0 | GK + outfield split gives ~90% of the visual impact at ~30% of the asset work |
| D4 | Render-side 200 ms (10-tick) delay buffer; sim is unchanged | Honors "animation forever visual-only" rule; matches FIFA / eFootball pipeline |
| D5 | Sim → render event channel via snapshot diff in `RenderSnapshotBuffer` (no new sim state) | Determinism gate untouched |
| D6 | Foot IK: TwoBoneIK per leg, ground-plane projection. No FullBodyIK in v0 | Pitch is flat; TwoBoneIK is sufficient and cheap |
| D7 | Ball-contact IK: wind-up alpha ramp 0→1, snap-to-ball at `BallContact` notify, follow-through 1→0 | Simplest control envelope that looks right |
| D8 | Ball visual position pulled from delayed snapshot; "releases" at `BallContact` notify | Aligns visual ball motion with foot strike |
| D9 | No Chaos / no ragdoll / no physics in v0 | Out of scope; render-only |
| D10 | New tests are UE5 C++ Automation, not standalone-sim self-test | They test render-side logic; not part of determinism gate |

---

## 7. Proposed milestones

The full implementation plan is the next document (`writing-plans` skill). These are the high-level cuts the plan will expand into bite-sized tasks.

| # | Milestone | Approx scope |
|---|---|---|
| M0 | Pre-flight: verify Phase 2 baseline green; create `feat/phase3-animation` branch | Verification |
| M1 | `RenderSnapshotBuffer` + 200 ms delay wiring | Pure C++; 2 automation tests; no asset work |
| M2 | Snapshot-diff event extraction (`KickEvent`, `BallReceivedEvent`, `GoalkeeperSaveEvent`) | Pure C++; 1 automation test |
| M3 | `FootballAnimInstance` base class + trajectory generation | Pure C++; 1 automation test; no anim BP work yet |
| M4 | Import Game Animation Sample plugin; build `MMDB_Outfield` skeleton (empty DB, structure only) | Asset work; smoke test that it loads |
| M5 | `ABP_Footballer_MM` motion-matching state tree wiring; populates `MMDB_Outfield` with Game Anim Sample locomotion clips | Anim BP + asset work |
| M6 | Foot IK setup: TwoBoneIK chains in `ABP_Footballer_MM`; ground-plane projection | Anim BP work |
| M7 | Mixamo kick / header / tackle retarget; populate `MMDB_Outfield` with football overlays + anim notifies (`BallContact`, `RecoverEnd`) | Asset work, retargeting |
| M8 | `BallContactIKComponent` + kick-montage IK alpha ramp | C++ + anim BP wiring |
| M9 | Goalkeeper subclass: `ABP_Goalkeeper` + `MMDB_Goalkeeper` + Mixamo GK animations (dive, parry, catch, throw, goal-kick) | Full asset + BP work |
| M10 | Anim event hookup: kick montage triggered by `KickEvent`, GK montage triggered by `GoalkeeperSaveEvent` | C++ + anim BP wiring |
| M11 | Re-place 22 `BP_Footballer` instances in `L_Pitch` with role-correct anim BPs (GKs get `BP_Goalkeeper`) via Python | Asset work |
| M12 | Final acceptance: 5-min PIE soak, automation tests green, no anim-BP warnings | Acceptance + docs |

---

## 8. Determinism guardrails (unchanged from Phase 1 / Phase 2)

Render-side code lives under `Source/Edge26/`, where the lint script does not run. Floats, UE5 types, threads, hash containers — all allowed. The constraint is one-way: **nothing render-side may write back into `FSimWorldState`.** All sim state flows downstream through the snapshot buffer; nothing flows upstream.

`Scripts/lint_sim.sh` (Edge26Sim only) continues to enforce the determinism rules on the sim side. Phase 3 should not modify any file under `Source/Edge26Sim/` — if it does, that's a design smell that needs rethinking.

---

## 9. Out-of-scope decisions parked for later

| # | Decision | Deferred to |
|---|---|---|
| OS1 | Whether GK has separate hand-on-ball IK (vs the foot-only contact in v0) | Phase 3.1 polish |
| OS2 | Animation budget per pawn (cull pose-search for distant players) | Performance optimization slice |
| OS3 | Crowd anim instances (replicated stadium-side ambient anims) | Visual polish phase |
| OS4 | Replay system that scrubs through the snapshot buffer + plays anims in reverse | Replay phase |
| OS5 | Network ping compensation (Phase 4 rollback netcode interacts with the snapshot buffer) | Phase 4 |
| OS6 | Stamina / fatigue affecting anim playback speed | Player Feel phase |
| OS7 | Per-foot kick selection (left- vs right-foot) | Phase 3.1 |
| OS8 | Dynamic camera (broadcast cam → spring-arm follow on goal celeb) | Visual polish phase |

---

## Appendix A — New files created in Phase 3 v0

```
Source/Edge26/Public/Adapter/RenderSnapshotBuffer.h
Source/Edge26/Private/Adapter/RenderSnapshotBuffer.cpp
Source/Edge26/Public/Animation/FootballerAnimEvents.h
Source/Edge26/Public/Animation/FootballAnimInstance.h
Source/Edge26/Private/Animation/FootballAnimInstance.cpp
Source/Edge26/Public/Animation/BallContactIKComponent.h
Source/Edge26/Private/Animation/BallContactIKComponent.cpp

Content/Animation/MotionMatching/MMDB_Outfield.uasset
Content/Animation/MotionMatching/MMDB_Goalkeeper.uasset
Content/Blueprints/Player/ABP_Footballer_MM.uasset
Content/Blueprints/Player/ABP_Goalkeeper.uasset
Content/Blueprints/Player/BP_Goalkeeper.uasset

Scripts/editor/import_game_anim_sample.py
Scripts/editor/retarget_mixamo_to_manny.py
Scripts/editor/replace_gk_anim_bps.py
```

## Appendix B — Files modified in Phase 3 v0

```
Source/Edge26/Public/Adapter/SimHostSubsystem.h        (add Buffer member, event route)
Source/Edge26/Private/Adapter/SimHostSubsystem.cpp     (push/pop snapshot, broadcast events)
Source/Edge26/Public/Adapter/FootballerVisual.h        (trajectory properties, IK component)
Source/Edge26/Private/Adapter/FootballerVisual.cpp     (component wiring)
Content/Blueprints/Player/BP_Footballer.uasset         (default AnimInstanceClass)
Content/Levels/L_Pitch.umap                            (GKs re-parented to BP_Goalkeeper)
PROGRESS.md                                            (Phase 3 milestones)
```

## Appendix C — Files unchanged

Everything under `Source/Edge26Sim/` is unchanged. Period. The determinism baselines in `Source/Edge26SimStandalone/tests/replays/` are unchanged. `Scripts/check_determinism.sh` and `Scripts/lint_sim.sh` are unchanged. This is a render-side phase.
