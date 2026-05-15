# Edge 26 — Phase 2: Spatial Value Model + 22-Player AI Design

**Status:** Approved (2026-05-15)
**Author:** Jefferson + Claude
**Scope:** Second slice of `project_breakdown.md`. Builds on Phase 1's deterministic 50Hz sim core. Adds 20 more players, three-layer AI (Strategy / Unit Coordination / Individual), spatial value model, offside enforcement, simple goalkeeper AI, player switching, and a debug overlay.

**Dependencies:** Phase 1 Sim Core v0 (see `docs/superpowers/specs/2026-05-15-sim-core-v0-design.md`). All Phase 1 rules apply: fixed-point math only, sim-authoritative, animation/IK forever visual-only, determinism enforced via lint + CI matrix.

---

## 1. Goal and non-goals

### Goal

Take the Phase 1 sim from "1 ball + 2 player kinematic shells" to "an 11v11 football match where 21 AI players act with believable team coordination." The match plays out at 50Hz deterministically, sees emergent off-ball intelligence from a shared spatial value model, and supports the player switching between teammates the way every modern football game does.

### Non-goals (v0)

Set pieces (throw-ins, corners, free kicks), fouls, cards, stamina, player attributes, realistic kick power, real goalkeeper AI, substitutions, multiple formations, multiple difficulties, coach personality presets, run-type variants (in-behind/checking/overlap/third-man), halftime, broadcast presentation, career-mode simulation. Full list in §16 Scope Cuts.

---

## 2. Architectural decisions (decision log)

| # | Decision | Choice | Alternatives rejected |
|---|---|---|---|
| D1 | Roster | Full 11v11 (22 players) | 5v5-then-scale; 2-pawn incremental |
| D2 | AI layers | All three (A team strategy + B unit coordination + C individual) | Layer C only; Layer C + A |
| D3 | Match features | Open play + offside line + simple goalkeeper | Open play only; + halftime/throws/corners |
| D4 | Player switching | Auto on lose-ball + manual button | Manual only; no switching |
| D5 | Debug overlay | Heatmap toggle per field + per-player intent arrows | Minimum log-only; full UMG inspector |
| D6 | Spatial fields | All 5 (Space, DefCoverage, LaneOccupancy, PassReception, Threat) | Subset (Space + PassReception only) |
| D7 | Formation | Hardcoded 4-3-3 for both teams in v0 | Multiple selectable; BP-editable |
| D8 | Difficulty levels | Single fixed difficulty | Easy/Normal/Hard |
| D9 | GK AI | 3-tier minimal (goal-line, sweep, save) | Treat as outfield; full GK AI program |
| D10 | Tick rates | A=2Hz, B=10Hz, C=50Hz; fields recomputed 50Hz no LOD | LOD by ball distance |

Reversal of any of these is a re-design pass, not a patch.

---

## 3. Module structure

Building on Phase 1:

```
Source/Edge26Sim/Public/
├── AI/                                ← NEW
│   ├── Roles.h                        ERole enum + per-role FRoleWeights table
│   ├── Formations.h                   FFormationSlot[]; hardcoded kFormation_4_3_3
│   ├── SpatialValueModel.h            FSpatialValueModel + ESpatialField + update helpers
│   ├── PlayerDecisions.h              Layer C: UpdatePlayerAI + intent scoring
│   ├── UnitCoordination.h             Layer B: UpdateDefensiveUnit / Midfield / Attack
│   ├── TeamStrategy.h                 Layer A: UpdateTeamStrategy + FTeamPlan
│   └── GoalkeeperAI.h                 UpdateGoalkeeperAI + MaybeGoalkeeperSave
├── Sim/
│   ├── MatchState.h                   NEW. FMatchState — possession, switching, plans, units, score
│   ├── PlayerState.h                  EXTENDED. + TeamId, RoleId, CurrentIntent, AITargetPosition, IntendedPassTarget
│   ├── WorldState.h                   EXTENDED. kSimPlayerCount: 2 → 22; embed FMatchState + FSpatialValueModel
│   ├── PlayerSwitch.h                 NEW. ChooseHumanControlled pure function
│   └── SimWorld.h                     EXTENDED. Step() invokes new layers; new helpers
└── (Math/ unchanged from Phase 1)
```

```
Source/Edge26/Public/Adapter/
├── AIDebugRenderer.h                  ← NEW. Render-side heatmap + intent arrows + offside lines
└── PlayerSwitchController.h           ← NEW. APlayerController subclass; auto + manual switch.
                                         `ASoccerGameMode::PlayerControllerClass` switched to point at this.
```

**The hard boundary from Phase 1 is unchanged:** `Edge26Sim` still depends only on `Core`. All UE5-facing concerns (rendering, input, possession) live in `Edge26`. `Scripts/lint_sim.sh` continues to enforce the forbidden-token rule. CMake standalone keeps building Edge26Sim without UE5 toolchain.

---

## 4. Sim state — extended structs

```cpp
// PlayerState.h — grows from 64 B to 80 B
struct FSimPlayerState {
    FixedVec3   Position;          // 24 B
    FixedVec3   Velocity;          // 24 B
    FixedAngle  Heading;           // 4 B
    FixedAngle  FacingTarget;      // 4 B
    uint8       ControllerIndex;   // 1 B (vestigial — see HumanControlledIndex)
    uint8       Flags;             // 1 B (Grounded, Sprinting)
    uint8       TeamId;            // 1 B (0=home, 1=away)
    uint8       RoleId;            // 1 B (ERole)
    uint8       CurrentIntent;     // 1 B (EIntent — written by Layer C)
    uint8       IntendedPassTarget;// 1 B (player idx the Layer C chose for next pass; 0xFF if none)
    uint8       _pad[2];           // explicit
    FixedVec3   AITargetPosition;  // 24 B (world cell the AI is moving toward)
};
static_assert(sizeof(FSimPlayerState) == 88, "FSimPlayerState size locked");
```

```cpp
// MatchState.h — NEW
struct FTeamPlan {
    int8     Mentality;            // -2..+2
    int8     LineHeightBias;       // -1..+1 (later scaled to cm offset)
    uint8    PressIntensity;       // 0..3
    uint8    Tempo;                // 0..3
    uint8    BuildupStyle;         // 0..2
    uint8    CounterAttackBias;    // 0..3
    uint8    _pad0[2];
    Fixed32  PanicBias;            // 4 B
    Fixed32  HoldBias;             // 4 B
    Fixed32  MentalityShootBias;   // 4 B
    Fixed32  _pad1;                // 4 B → struct = 24 B
};
static_assert(sizeof(FTeamPlan) == 24);

struct FUnitState {
    Fixed64  LineY;                // 8 B
    Fixed32  Compactness;          // 4 B
    uint8    PressTrigger;         // 0/1
    uint8    PressTargetIdx;       // 0..21/0xFF
    uint8    OverlapTriggerIdx;    // 0..21/0xFF
    uint8    _pad;                 // → 16 B
};
static_assert(sizeof(FUnitState) == 16);

struct FMatchState {
    uint8       HumanControlledIndex;     // offset 0
    uint8       PossessionTeam;           // 1   (0/1/0xFF — loose)
    uint8       PossessionPlayer;         // 2
    uint8       KickoffTeam;              // 3
    uint8       PendingOffsideCallTeam;   // 4   (0/1/0xFF)
    uint8       _pad0[3];                 // 5..7
    uint32      PendingOffsideCallTick;   // 8..11
    uint32      _pad1;                    // 12..15  — explicit pre-Fixed64 alignment pad
    Fixed64     OffsideLineY[2];          // 16..31  (8-aligned)
    FTeamPlan   Plans[2];                 // 32..79  (2 × 24)
    FUnitState  Units[2][3];              // 80..175 (6 × 16)
    uint16      Score[2];                 // 176..179
    uint8       _pad2[4];                 // 180..183
};
static_assert(sizeof(FMatchState) == 184);
static_assert(alignof(FMatchState) == 8);
```

```cpp
// SpatialValueModel.h
constexpr int kPitchCellsX = 52;
constexpr int kPitchCellsY = 34;
constexpr int kPitchCells  = kPitchCellsX * kPitchCellsY;  // 1768

enum class ESpatialField : uint8 {
    Space         = 0,
    DefCoverage   = 1,
    LaneOccupancy = 2,
    PassReception = 3,
    Threat        = 4,
    Count         = 5
};

struct FSpatialValueModel {
    Fixed32 Cells[2][(int)ESpatialField::Count][kPitchCells];
};
static_assert(sizeof(FSpatialValueModel) == 2 * 5 * 1768 * 4);  // 70,720 B
```

```cpp
// WorldState.h — extended
constexpr int kSimPlayerCount = 22;

struct FSimWorldState {
    uint32              TickNumber;
    uint32              _pad0;
    uint64              RngState;
    FSimBallState       Ball;                                  // 80 B
    FSimPlayerState     Players[kSimPlayerCount];              // 22 × 88 = 1936 B
    FMatchState         Match;                                 // 184 B
    FSpatialValueModel  Spatial;                               // 70,720 B
};
static_assert(sizeof(FSimWorldState) == 72944);
static_assert(alignof(FSimWorldState) == 8);
```

**Snapshot impact:** ~72 KB per snapshot. xxhash + memcpy stay well under 100 µs per tick on M4. Rollback ring buffer (12 ticks) = ~870 KB. Trivial.

**Determinism rule preserved:** all members POD; explicit padding; zero-init on `SimWorld` construction. Phase 1's `Scripts/lint_sim.sh` continues to enforce no-float / no-threads / no-engine-includes / no-heap-in-Step inside `Edge26Sim`.

---

## 5. The per-tick flow

The sim continues to tick at exactly 50 Hz. Within each tick, the order is deterministic:

```cpp
void SimWorld::Step(const FInputFrame& frame) {
    State.TickNumber = frame.TickNumber;

    // 1. Spatial fields — every tick. Order matters; later fields read earlier ones.
    UpdateSpatialFields(State);

    // 2. Layer A — every 25 ticks (2 Hz).
    if (State.TickNumber % 25 == 0) {
        for (int team = 0; team < 2; ++team)
            UpdateTeamStrategy(State.Match.Plans[team], State, team);
    }

    // 3. Layer B — every 5 ticks (10 Hz).
    if (State.TickNumber % 5 == 0) {
        for (int team = 0; team < 2; ++team) {
            UpdateDefensiveUnit(State.Match.Units[team][0], State, team);
            UpdateMidfieldUnit (State.Match.Units[team][1], State, team);
            UpdateAttackUnit   (State.Match.Units[team][2], State, team);
        }
    }

    // 4. Layer C — every tick. Iterate in ascending player index.
    for (int i = 0; i < kSimPlayerCount; ++i) {
        FSimPlayerState& p = State.Players[i];
        if (i == State.Match.HumanControlledIndex) {
            ApplyHumanInput(p, frame);
        } else if (p.RoleId == (uint8)ERole::GK) {
            UpdateGoalkeeperAI(p, State, i);
        } else {
            UpdatePlayerAI(p, State, i);
        }
    }

    // 5. Resolve kicks (including offside flag, but not the call yet).
    for (int i = 0; i < kSimPlayerCount; ++i)
        MaybeApplyKick(State.Ball, State.Players[i], frame, State.Match);

    // 6. Goalkeeper save check — before ball physics, after kicks.
    MaybeGoalkeeperSave(State.Ball, State);

    // 7. Offside resolution (if a flagged pass has now landed or timed out).
    ResolveOffsideCall(State);

    // 8. Update possession (loose-ball pickup, etc.).
    UpdatePossession(State);

    // 9. Ball physics (unchanged from Phase 1).
    StepBall(State.Ball);

    // 10. Player kinematics (unchanged from Phase 1).
    for (int i = 0; i < kSimPlayerCount; ++i)
        StepPlayer(State.Players[i], frame);
}
```

**Tick rate budget summary:**

| Layer | Rate | Per-tick cost estimate |
|---|---|---|
| Spatial fields | 50 Hz | ~100K ops |
| Layer A | 2 Hz | trivial |
| Layer B | 10 Hz | small |
| Layer C | 50 Hz | 22 × ~10 candidate intents × score = ~220 ops |
| Player kinematics | 50 Hz | unchanged from Phase 1 |

At 50 Hz total: ~5M ops/sec dominated by spatial-field recompute. Apple M4 budget: tens of microseconds per tick. Significant headroom for LOD later.

**Determinism caveats:**
- All field iteration in linear cell-index order.
- All player iteration in `i = 0..21` order. Two players evaluating the same target cell — first-evaluated wins ties.
- All scoring in `Fixed64`/`Fixed32`. No `std::sort`. When we need top-N, we use a fixed-size partial-sort with stable comparators.

---

## 6. Roles, formations, slot anchors

```cpp
enum class ERole : uint8 {
    GK   = 0,  // Goalkeeper
    CB   = 1,  // Center Back
    FB_L = 2,
    FB_R = 3,
    CDM  = 4,
    CM   = 5,
    CAM  = 6,
    W_L  = 7,
    W_R  = 8,
    ST   = 9,
    Count
};

struct FRoleWeights {
    // Off-ball
    Fixed32  MakeRunForward;
    Fixed32  HoldPosition;
    Fixed32  DropToReceive;
    Fixed32  ProvideWidth;
    Fixed32  Press;
    Fixed32  TrackRunner;
    Fixed32  HoldDefensiveLine;
    // On-ball biases (multipliers on the value-field-driven score)
    Fixed32  PreferPass;
    Fixed32  PreferShoot;
    Fixed32  PreferDribble;
    Fixed32  PreferLongBall;
};

// Hardcoded weights table. v0 ships tuned-by-eye values; designer iteration
// is part of M12 (the AI tuning pass).
constexpr FRoleWeights kRoleWeightsTable[(int)ERole::Count] = { ... };
```

```cpp
// FFormationSlot: pitch-normalized X,Y in [-1, 1]. World position derived per team.
struct FFormationSlot {
    ERole    Role;
    Fixed64  NormalizedX;
    Fixed64  NormalizedY;
};

constexpr FFormationSlot kFormation_4_3_3[11] = {
    { ERole::GK,   -0.95_f64,  0.0_f64 },
    { ERole::CB,   -0.65_f64, -0.15_f64 },
    { ERole::CB,   -0.65_f64,  0.15_f64 },
    { ERole::FB_L, -0.55_f64, -0.65_f64 },
    { ERole::FB_R, -0.55_f64,  0.65_f64 },
    { ERole::CDM,  -0.25_f64,  0.0_f64 },
    { ERole::CM,   -0.10_f64, -0.30_f64 },
    { ERole::CM,   -0.10_f64,  0.30_f64 },
    { ERole::W_L,   0.40_f64, -0.70_f64 },
    { ERole::W_R,   0.40_f64,  0.70_f64 },
    { ERole::ST,    0.50_f64,  0.0_f64 },
};
```

**Slot positions are soft anchors, not constraints.** Layer C uses them to bias `HoldPosition`. When a player drifts from their slot, the bias grows. When other intents score higher, the player leaves. When the ball clears and nothing else scores well, slot proximity pulls them back. This is how real football AI keeps shape without explicit position commands.

**Kickoff placement:** `ResetForKickoff()` (extended) snaps each player to their slot's world position based on `TeamId` (own half) and the formation. Away team's `NormalizedX` is flipped.

**TeamId / RoleId assignment:** set on each `BP_Footballer` instance in `L_Pitch` via the Details panel. The Python `configure_blueprints.py` script can assign in bulk based on placement order.

---

## 7. Layer C — Individual Player AI (50 Hz)

The decision loop, simplified:

```cpp
void UpdatePlayerAI(FSimPlayerState& p, const FSimWorldState& s, int playerIdx) {
    const FRoleWeights& W   = kRoleWeightsTable[(int)p.RoleId];
    const FTeamPlan& Plan   = s.Match.Plans[p.TeamId];
    const bool onBall       = (s.Match.PossessionPlayer == playerIdx);

    EIntent   bestIntent = EIntent::HoldPosition;
    FixedVec3 bestTarget = SlotWorldPosition(p, s);
    Fixed32   bestScore  = MinFixed32;

    if (onBall)
        EvaluateOnBall (p, s, W, Plan, playerIdx, bestIntent, bestTarget, bestScore);
    else
        EvaluateOffBall(p, s, W, Plan, playerIdx, bestIntent, bestTarget, bestScore);

    p.CurrentIntent      = (uint8)bestIntent;
    p.FacingTarget       = AngleTowards(p.Position, bestTarget);
    p.AITargetPosition   = bestTarget;
}
```

**Off-ball intents and their target-cell pickers:**

| Intent | Target cell picker | Gate |
|---|---|---|
| `MakeRunForward` | `ArgMaxCell` on `PassReception`, filtered: ahead of carrier, not offside, not occupied | own team has possession |
| `HoldPosition` | Slot world position | always |
| `DropToReceive` | open cell behind carrier (high `Space`) | own team possession |
| `ProvideWidth` | nearest sideline cell at carrier's Y | own team possession |
| `Press` | ball-carrier's position | opp possession AND playerIdx == Units[team][unit].PressTargetIdx |
| `TrackRunner` | position of nearest unmarked opposing forward | opp possession |
| `HoldDefensiveLine` | `(p.X, Units[team][Def].LineY)` | always (defenders) |

**On-ball intents:**

| Intent | Target | Score driver |
|---|---|---|
| `Pass` | best teammate's position | `PassReceptionAt(teammate) × PassSuccessProb × W.PreferPass`; forward-pass bonus |
| `Shoot` | opponent's goal center | `Threat[carrier_cell] × W.PreferShoot × Plan.MentalityShootBias`; gated by "in opponent third" |
| `Dribble` | best adjacent high-space cell | `Space × W.PreferDribble - nearest_opp_dist_penalty` |
| `Hold` | current position | `Plan.HoldBias × small_constant` |
| `Clear` | long-ball target up-pitch | `W.PreferLongBall × Threat[own_position(opp_perspective)] × Plan.PanicBias`; gated by defender role |

**Acting on the chosen intent.** After Layer C writes `CurrentIntent`, `FacingTarget`, and `AITargetPosition`, the existing `StepPlayer` kinematics integrate position toward the heading. For on-ball intents (`Pass`, `Shoot`, `Chip`, `Clear`), the AI sets a synthetic button bit on the player's pending input that `MaybeApplyKick` consumes — bypassing the human input pipeline but reusing the kick impulse path.

**Determinism properties:**
- Linear iteration over intents AND candidate teammates → deterministic.
- Tie-breaking: first-evaluated wins.
- All scoring in `Fixed64`/`Fixed32`. RNG (for any future stochasticity) draws from seeded `Rng` in `FSimWorldState`.

---

## 8. Layer B — Unit Coordination (10 Hz, 3 units × 2 teams)

Three units per team (`Defense`, `Midfield`, `Attack`). Layer B publishes coordinated state into `FUnitState` that Layer C reads.

**Defense unit:**
- `LineY`: averaged Y of CBs + FBs, adjusted by `Plan.LineHeightBias`. Set offside-trap target.
- `OffsideLineY` (in `FMatchState`): `MaxY(last_defender, ball.Y)`.
- `PressTrigger`: 1 when opponent has bad touch, slow back-pass, slow square ball, or low-line carrier — gated by `Plan.PressIntensity`.
- `PressTargetIdx`: nearest unit-member to ball (only this player gets the Press intent multiplier).
- `Compactness`: stddev of unit positions.

**Midfield unit:**
- `LineY`: midline averaged over CMs/CDM/CAM.
- Press triggers when ball moves laterally in the central channel.
- One CM nominated to track each runner through midfield (by lateral channel).
- Compactness — bias midfielders to bunch when stretched.

**Attack unit:**
- `LineY`: highest player Y.
- `OverlapTriggerIdx`: when carrier is on a flank, nominate the same-side FB to overlap. Layer C boosts that FB's `MakeRunForward` score.
- v0 attacking pattern: just "overlap." Third-man-run / switch-of-play come in 2.1.

**How Layer C consumes Layer B output:**

```cpp
// In EvaluateOffBall — Press intent gated by being nominated
if (s.Match.PossessionTeam != p.TeamId
    && playerIdx == s.Match.Units[p.TeamId][unit_for_role(p.RoleId)].PressTargetIdx) {
    // Press scores much higher for the nominated presser
    pressScore *= Fixed32::FromInt(3);
}

// HoldDefensiveLine target is the unit's LineY
{
    FixedVec3 target = { p.Position.X, s.Match.Units[p.TeamId][Defense].LineY, GroundZ };
    Fixed32   score  = W.HoldDefensiveLine * BiasFromCompactness(u.Compactness);
}

// Overlap nomination boosts MakeRunForward for that FB
if ((p.RoleId == FB_L || p.RoleId == FB_R)
    && playerIdx == s.Match.Units[p.TeamId][Attack].OverlapTriggerIdx) {
    makeRunScore *= Fixed32::FromInt(2);
}
```

---

## 9. Layer A — Team Strategy (2 Hz, one per team)

Sets the "dials" Layers B and C read.

```cpp
void UpdateTeamStrategy(FTeamPlan& Plan, const FSimWorldState& s, int teamId) {
    const int   scoreDiff = s.Match.Score[teamId] - s.Match.Score[1 - teamId];
    const auto  secsLeft  = MatchSecondsRemaining(s);

    // Baseline 4-3-3 with default mentality.
    Plan.Mentality          = 0;
    Plan.LineHeightBias     = 0;
    Plan.PressIntensity     = 2;
    Plan.Tempo              = 2;
    Plan.BuildupStyle       = 1;  // mixed
    Plan.CounterAttackBias  = 1;

    // Trailing late: push everyone forward.
    if (scoreDiff < 0 && secsLeft < 30 * 60) {
        Plan.Mentality = +2;
        Plan.LineHeightBias = +1;
        Plan.PressIntensity = 3;
    }
    // Leading late: drop deep.
    else if (scoreDiff > 0 && secsLeft < 20 * 60) {
        Plan.Mentality = -1;
        Plan.LineHeightBias = -1;
        Plan.PressIntensity = 1;
        Plan.Tempo = 1;
        Plan.PanicBias = Fixed32::FromRaw(0_5_f32_raw);
    }
    // Drawn late: cautious push.
    else if (scoreDiff == 0 && secsLeft < 20 * 60) {
        Plan.Mentality = +1;
    }

    // Per-team personality (v0 hardcode; data-driven later).
    if (teamId == 0) {  // home = possession
        Plan.BuildupStyle = 0;
        Plan.HoldBias = Fixed32::FromRaw(0_3_f32_raw);
    } else {            // away = counter
        Plan.BuildupStyle = 2;
        Plan.CounterAttackBias = 3;
    }
}
```

What this delivers in v0:
- Late-and-losing teams throw bodies forward.
- Late-and-leading teams sit deep and game-manage.
- Home plays out from back; away plays direct/counter.

What's out: real coach presets (Klopp / Mourinho / Pep distinct identities). That table is a separate data-driven slice.

---

## 10. Spatial Value Model (50 Hz, 5 fields)

```cpp
// 52×34 cells over a 105m × 68m pitch (≈2m cells). 1768 cells per field.
// 5 fields × 2 team perspectives = 10 arrays.

inline int CellIndex(FixedVec3 worldPos);
inline FixedVec3 CellCenter(int cellIdx);
```

**The 5 fields:**

| Field | Per cell, this is... | Update cost |
|---|---|---|
| `Space` | `Sqrt(min_distSq_to_nearest_opponent)`, clamped to [0,1] | ~39K ops |
| `DefCoverage` | nearest-teammate distance (opp perspective) | ~39K |
| `LaneOccupancy` | sample 5 points along (ball → cell); blocked if any point ≤ 80cm of opponent | ~9K |
| `PassReception` | `Space × Lane × (1+ForwardBonus) × (1+Threat)` — composite | ~7K |
| `Threat` | static xG surface + dynamic occupation adjustment | ~3K |

**Update order matters** (later fields read earlier ones):

```cpp
void UpdateSpatialFields(FSimWorldState& s) {
    for (int t = 0; t < 2; ++t) {
        UpdateSpaceField        (s, t);
        UpdateDefCoverageField  (s, t);
    }
    UpdateLaneOccupancyField(s);                // team-independent
    for (int t = 0; t < 2; ++t) {
        UpdatePassReceptionField(s, t);          // reads Space + Lane + Threat
        UpdateThreatField       (s, t);          // mostly static + small dynamic adjust
    }
}
```

**`ArgMaxCell` is the universal "find the best cell" helper used by Layer C off-ball pickers:**

```cpp
template <typename Filter>
int ArgMaxCell(const FSpatialValueModel& sm, int teamId, ESpatialField field, Filter f) {
    int bestIdx = -1;
    Fixed32 bestVal = MinFixed32;
    for (int c = 0; c < kPitchCells; ++c) {
        if (!f(c)) continue;
        Fixed32 v = sm.Cells[teamId][(int)field][c];
        if (v.Raw > bestVal.Raw) { bestVal = v; bestIdx = c; }
    }
    return bestIdx;
}
```

Iteration in linear order → deterministic; first cell of equal score wins; identical on every platform.

**Emergent coordination property:** when one striker takes the high-value cell, the field's `PassReception` drops at that cell (because a teammate now occupies it). The second striker scores higher on a different cell. Off-ball spacing falls out of the shared field; no explicit messaging needed.

---

## 11. Offside enforcement

`OffsideLineY` published by Layer B every 5 ticks. Enforced in two places per tick:

1. **In Layer C's `MakeRunForward` target picking** — cells past the line score 0 from `PassReception`. AI never runs offside intentionally.
2. **In `MaybeApplyKick` when a pass fires** — check `IntendedPassTarget`'s position against opp `OffsideLineY` at the moment the kick releases. If past line, flag `Match.PendingOffsideCall*` with a 30-tick (0.6s) grace window. After the grace window or when the receiver controls the ball, `ResolveOffsideCall` awards possession to the defending team. No restart positioning in v0 (set-pieces deferred).

```cpp
bool CellIsOffside(int cellIdx, int attackingTeam, const FMatchState& m) {
    Fixed64 cellY = CellCenter(cellIdx).Y;
    Fixed64 lineY = m.OffsideLineY[1 - attackingTeam];
    return (SignForTeam(attackingTeam) > 0) ? (cellY > lineY) : (cellY < lineY);
}
```

---

## 12. Goalkeeper AI

GK gets a separate AI path (not Layer C's intent loop). Three tiers per tick:

1. **Goal-line stance.** Default: 1m off line, leaning toward ball laterally (clamped to ±3m of goal center).
2. **Sweeper.** If ball is in own box AND no opponent within reach of it, come collect.
3. **Save.** If ball is moving toward own goal and within GK reach radius, predict its position at the goal line and intercept.

**Save mechanic** (`MaybeGoalkeeperSave` runs between kicks and ball physics):

```cpp
void MaybeGoalkeeperSave(FSimBallState& b, FSimWorldState& s) {
    for (int t = 0; t < 2; ++t) {
        int gkIdx = FindGoalkeeper(s, t);
        if (gkIdx < 0) continue;
        const FSimPlayerState& gk = s.Players[gkIdx];

        FixedVec3 toBall = b.Position - gk.Position;
        Fixed64 dist = SimMath::Sqrt(Dot(toBall, toBall));
        if (dist.Raw > kGKReachRadius.Raw) continue;
        if (!BallMovingTowardGoal(b, t)) continue;

        // Save: zero velocity, GK gains possession.
        b.Velocity = FixedVec3::Zero();
        b.AngularVelocity = FixedVec3::Zero();
        b.Position = gk.Position + FixedVec3{ Fixed64::FromInt(20), Fixed64::Zero, gkArmsHeight };
        s.Match.PossessionTeam   = (uint8)t;
        s.Match.PossessionPlayer = (uint8)gkIdx;
    }
}
```

Goal-scoring still uses the existing `AGoalTrigger` overlap from Phase 1. A shot that beats the GK's reach reaches the trigger; a saved shot is intercepted before reaching it.

---

## 13. Player switching

Sim-side: pure-function `ChooseHumanControlled(state, humanTeam) → int32_t playerIdx`:
- If `humanTeam` has possession AND `PossessionPlayer.TeamId == humanTeam` → return `PossessionPlayer`.
- Else → return the outfield (non-GK) teammate nearest to the ball.

UE5 side: `USimHostSubsystem::Tick` calls the policy after `Sim->Step()`. When the policy returns a different index than the current `HumanControlledIndex`, update both the sim's value AND `PlayerController->Possess(target_pawn)`. The BroadcastSpringArm camera follows automatically.

**Manual switch** (`OnSwitch` bound to R2/RShift in `IMC_Player`): cycles to the next-nearest teammate to the ball; sets a 0.5s cooldown that suppresses auto-switch (so manual choice "sticks" briefly).

**Determinism:** the switch policy is a pure function of `FSimWorldState`. Both clients observing identical state arrive at the same switching decision. Manual switch is part of the `FInputFrame` bitfield, replicated like any other button.

What's out: cycle-through-teammates UX (we only swap to nearest), right-stick directional switch, currently-controlled-pawn highlight ring.

---

## 14. AI debug overlay

All render-side, in `Edge26` module:

```cpp
UCLASS()
class EDGE26_API AAIDebugRenderer : public AActor {
    GENERATED_BODY()
public:
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category="AI Debug")
    ESpatialFieldDebug ActiveField = ESpatialFieldDebug::None;

    UPROPERTY(EditAnywhere, Category="AI Debug")
    int32 TeamPerspective = 0;

    UPROPERTY(EditAnywhere, Category="AI Debug")
    bool bShowIntentArrows = true;

    UPROPERTY(EditAnywhere, Category="AI Debug")
    bool bShowOffsideLines = true;
};
```

**Console commands** (registered via Edge26 cheat manager):
```
edge26.ai.show_field None | Space | DefCoverage | Lane | PassReception | Threat
edge26.ai.team_perspective 0 | 1 | -1
edge26.ai.intent_arrows 0 | 1
edge26.ai.offside_lines 0 | 1
```

**Rendering:**
- Heatmap: `DrawDebugBox` per cell colored by field value (black → green). ~1800 boxes/frame when active; off by default.
- Intent arrows: `DrawDebugDirectionalArrow` from player to chosen target cell, colored by intent, labeled with role + intent name.
- Offside lines: `DrawDebugLine` horizontal at each team's `OffsideLineY`, colored per team.

**Production builds** compile this out via `#if !UE_BUILD_SHIPPING`.

---

## 15. Acceptance criteria

v0 is done when all of these are green:

1. **22 players + 1 ball** spawn on `L_Pitch` in 4-3-3 formation at kickoff. Home defends -Y, away defends +Y.
2. **`Scripts/check_determinism.sh` PASSES** on Linux/macOS/Windows. New 22-player baselines regenerated and committed.
3. **`lint_sim.sh` clean** — no banned tokens leaked into the expanded `Edge26Sim`.
4. **Snapshot budget** — `FSimWorldState` = 72,944 B (static_asserted). xxhash + memcpy under 100 µs/tick on M4.
5. **5-minute PIE soak** plays without crashes, ensures, NaN-equivalents (we have no floats in sim), or log spam.
6. **AI sanity in PIE:**
   - Off-ball teammates make forward runs into open space when you have the ball.
   - Defenders track opposing forwards (intent arrow = `TrackRunner`).
   - Defensive line moves as a unit (visible via offside-line debug overlay).
   - Pressing fires conditionally (one defender steps to ball-carrier; others hold).
   - Late-and-losing team's line height shifts up and players push forward.
7. **Offside enforcement** observable via debug overlay; a through-ball past the line turns possession over within 30 ticks.
8. **Goalkeeper saves** — most shots on target are blocked; shots beating GK's reach trigger `GOAL!`.
9. **Player switching** — auto-switch on lose-ball confirmed; manual switch via R2/RShift confirmed; 0.5s cooldown observed.
10. **Debug overlay** — `edge26.ai.show_field Space` toggles colored heatmap; `intent_arrows 1` shows labels; both off by default.
11. **`PROGRESS.md` updated** — Phase 2 milestones checked + activity log entry.

---

## 16. Scope cuts (explicitly NOT in Phase 2 v0)

| Cut | Lives in (future phase) |
|---|---|
| Throw-ins, corners, goal kicks, free kicks | Set Pieces |
| Fouls + cards | Set Pieces + Discipline |
| Stamina | Player Feel polish |
| Player attributes (pace, vision, finishing) | Attributes & Tuning |
| Realistic kick power / contact quality | Phase 3 (motion-matching + IK) |
| Real goalkeeper AI (dives, reflexes, distribution choice) | Dedicated GK phase |
| Substitutions / squad management | Career / Match Management |
| Multiple formations beyond hardcoded 4-3-3 | Tactics |
| Multiple difficulty levels | Difficulty Scaling |
| Coach personality presets (Klopp, Mourinho, Pep) | Tactics (data-driven) |
| Pass success probability with attribute spread | Attributes |
| Run-type variants (in-behind / checking / overlap / underlap / third-man / decoy) | Phase 2.1 polish |
| Third-man-run pattern detection | Phase 2.1 polish |
| Halftime, fulltime ceremony, match clock progression | Match Flow |
| Crowd, stadium, broadcast presentation | Visual Polish |
| Multi-match career simulation via headless sim | Career Mode |
| Live tunable parameters (slider UI for `Plan.Mentality` etc.) | Tuning Tools |
| Layer A/B inspector UMG window | Tuning Tools |
| Pawn-switching UX polish (cycle, directional, highlight ring) | Player Feel |
| Configurable human team (home vs away) | UX polish |

---

## 17. Out-of-scope decisions parked for later

| # | Decision | Deferred to |
|---|---|---|
| OS1 | LOD on spatial fields (per ball distance) | Performance optimization slice when 22 isn't enough |
| OS2 | Switching across hash-ordered iteration to fixed-order containers | Phase 1 already settled this — no change |
| OS3 | Network transport choice (rollback over UDP, yojimbo, UE5) | Phase 4 |
| OS4 | AI difficulty knobs (decision quality vs reaction latency vs press aggression) | Difficulty Scaling phase |
| OS5 | Data format for role-weights table (CSV / DataTable / .ini / hardcoded) | Tuning Tools |
| OS6 | Match clock semantics (real time vs ticks vs 90-minute representation) | Match Flow |
| OS7 | Replay format for Phase 2 (re-use Phase 1 InputFrame? or extend?) | Phase 4 |
| OS8 | Crowd reactions tied to match state | Visual Polish |

---

## Appendix A — New files created in Phase 2 v0

```
Source/Edge26Sim/Public/
    AI/Roles.h
    AI/Formations.h
    AI/SpatialValueModel.h
    AI/PlayerDecisions.h
    AI/UnitCoordination.h
    AI/TeamStrategy.h
    AI/GoalkeeperAI.h
    Sim/MatchState.h
    Sim/PlayerSwitch.h
Source/Edge26Sim/Private/
    AI/Roles.cpp                  (role weights table; constexpr-mostly)
    AI/Formations.cpp             (kFormation_4_3_3 data)
    AI/SpatialValueModel.cpp      (5 update functions)
    AI/PlayerDecisions.cpp        (Layer C scoring)
    AI/UnitCoordination.cpp       (Layer B)
    AI/TeamStrategy.cpp           (Layer A)
    AI/GoalkeeperAI.cpp           (GK)
    Sim/MatchState.cpp            (helpers)
    Sim/SimWorld.cpp              (EXTENDED Step + new helpers)
    Sim/SimWorld_Player.cpp       (EXTENDED with TeamId/RoleId)
    Sim/SimWorld_Ball.cpp         (EXTENDED with offside flag handling)

Source/Edge26/Public/Adapter/
    AIDebugRenderer.h
    PlayerSwitchController.h
Source/Edge26/Private/Adapter/
    AIDebugRenderer.cpp
    PlayerSwitchController.cpp
    SimHostSubsystem.cpp          (EXTENDED with auto-switch dispatch)
```

## Appendix B — Files modified in Phase 2 v0

```
Source/Edge26Sim/Public/Sim/PlayerState.h       (FSimPlayerState 64 → 88 B)
Source/Edge26Sim/Public/Sim/WorldState.h        (kSimPlayerCount 2 → 22; embed Match + Spatial)
Source/Edge26Sim/Public/Sim/SimWorld.h          (new helpers in Step signature)
Source/Edge26/Public/Adapter/SimHostSubsystem.h (new switch hooks, GetSimState now ~73KB)
Content/Levels/L_Pitch.umap                     (22 BP_Footballer placements + role/team configured)
Content/Input/IMC_Player.uasset                 (new IA_Switch binding)
PROGRESS.md                                      (Phase 2 milestones)
RUNBOOK.md                                       (overlay console commands, switching keys)
Source/Edge26SimStandalone/tests/replays/       (regenerated for 22-player streams)
```

## Appendix C — Files unchanged

All Phase 1 math (`Fixed*`, `FixedVec*`, `Trig`, `Sqrt`, `Atan2`, `Rng`, `Hash`), the standalone CMake project, `lint_sim.sh`, `check_determinism.sh`, `update_determinism_baseline.sh`, the GitHub Actions workflow, the legacy game/HUD/GoalTrigger classes (already migrated in Phase 1). The Phase 1 spec doc is the canonical source for determinism rules and stays authoritative.

---

*End of design.*
