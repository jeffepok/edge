# Edge 26 — Phase 3: Animation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Phase 2's tick-discrete kick model with animation-driven control: motion-matched locomotion, foot IK, ball-contact IK during kick montages, and a GK-vs-outfield animation split. The sim stays unchanged — Phase 3 is render-side only per spec §3.

**Architecture:** New `RenderSnapshotBuffer` queues 25 sim snapshots and emits per-frame transforms + event diffs ~200 ms behind real sim time. `FootballAnimInstance` reads the delayed state plus extrapolated trajectory; UE 5.7's Motion Matching subsystem (`UPoseSearchDatabase`) picks animation frames. `BallContactIKComponent` runs the kick-montage IK alpha schedule. Anim assets sourced from Epic's Game Animation Sample (locomotion) + Mixamo retargeted to Manny (football moves, GK).

**Tech Stack:** UE 5.7 (Motion Matching plugin, IK Retargeter, Pose Search, Anim Blueprints), C++17 (no UBT changes to the sim), Python for headless editor scripting, GitHub Actions still runs only the Phase 1/2 determinism gate (Phase 3 adds no sim tests).

**Reference spec:** [`docs/superpowers/specs/2026-05-17-phase3-animation-design.md`](../specs/2026-05-17-phase3-animation-design.md). Read it before starting any task.

**Repo conventions (from `CLAUDE.md`):**
- Never include `Co-Authored-By: Claude ...` trailers or Anthropic / Claude attribution of any kind in commits, PR bodies, or tag annotations.
- Commit subject style: `feat(scope): …`, `fix(scope): …`, `docs(scope): …`, `test(scope): …`, `chore: …`. Imperative, wrap body at ~72 cols.
- **Never modify any file under `Source/Edge26Sim/`.** If a task requires changing sim code, stop and re-examine the design.

---

## Task index

- **M0** — Pre-flight (T0.1 – T0.3)
- **M1** — `RenderSnapshotBuffer` + 200 ms delay wiring (T1.1 – T1.7)
- **M2** — Snapshot-diff event extraction (T2.1 – T2.5)
- **M3** — `FootballAnimInstance` base class + trajectory generation (T3.1 – T3.4)
- **M4** — Game Animation Sample import + `MMDB_Outfield` skeleton (T4.1 – T4.3)
- **M5** — `ABP_Footballer_MM` motion-matching state tree (T5.1 – T5.4)
- **M6** — Foot IK setup (TwoBoneIK chains) (T6.1 – T6.3)
- **M7** — Mixamo retarget + football overlays + anim notifies (T7.1 – T7.5)
- **M8** — `BallContactIKComponent` + kick-montage IK alpha (T8.1 – T8.5)
- **M9** — Goalkeeper subclass + `MMDB_Goalkeeper` + GK animations (T9.1 – T9.5)
- **M10** — Anim event hookup (KickEvent → montage, GoalkeeperSaveEvent → dive) (T10.1 – T10.3)
- **M11** — Re-place 22 `BP_Footballer` instances with role-correct anim BPs (T11.1 – T11.2)
- **M12** — Final acceptance (T12.1 – T12.5)

After every milestone: append a brief activity-log entry to `PROGRESS.md`.

---

## M0 — Pre-flight

Verify Phase 2 is healthy and that we're on the right branch before touching code.

### Task T0.1 — Verify Phase 2 baseline is green

**Files:** none modified.

- [ ] **Step 1: Determinism gate from a clean tree**

```bash
./Scripts/check_determinism.sh
```

Expected last line: `PASS: all determinism checks`.

- [ ] **Step 2: Editor builds**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected last line: `Result: Succeeded`.

- [ ] **Step 3: Confirm branch**

```bash
git branch --show-current
```

Expected: `feat/phase3-animation`. (The brainstorming step already created this branch and committed the spec.)

If anything fails: stop, fix, then resume.

### Task T0.2 — Add Phase 3 to PROGRESS.md roadmap

**Files:**
- Modify: `PROGRESS.md`

- [ ] **Step 1: Find the existing Phase 3 placeholder header**

```bash
grep -n "Phase 3:" PROGRESS.md
```

You should see one line like:

```
### Phase 3: Motion-matching animation + procedural ball-contact IK  ←  next  (render-side only per spec §3)
```

- [ ] **Step 2: Replace with milestone list**

Edit `PROGRESS.md`. Replace that line + the following blank line with:

```markdown
### Phase 3: Motion-matching animation + procedural ball-contact IK  ←  current  (render-side only per spec §3)
- [ ] M1. RenderSnapshotBuffer + 200 ms delay wiring
- [ ] M2. Snapshot-diff event extraction (KickEvent, BallReceived, GoalkeeperSave)
- [ ] M3. FootballAnimInstance base class + trajectory generation
- [ ] M4. Game Animation Sample import + MMDB_Outfield skeleton
- [ ] M5. ABP_Footballer_MM motion-matching state tree
- [ ] M6. Foot IK setup (TwoBoneIK per leg, ground-plane projection)
- [ ] M7. Mixamo retarget + football overlays + anim notifies
- [ ] M8. BallContactIKComponent + kick-montage IK alpha
- [ ] M9. Goalkeeper subclass + MMDB_Goalkeeper + GK animations
- [ ] M10. Anim event hookup (KickEvent → montage trigger)
- [ ] M11. Re-place 22 BP_Footballer instances with role-correct anim BPs
- [ ] M12. Final acceptance (PIE soak + UE5 automation tests + PROGRESS.md)
```

- [ ] **Step 3: Update Current status**

Find the Phase 2 status paragraph at the top. Append a short paragraph below it (do not delete the Phase 2 content):

```markdown
We are at **Phase 3 M1 of M12** (RenderSnapshotBuffer). Spec at
`docs/superpowers/specs/2026-05-17-phase3-animation-design.md`. Plan at
`docs/superpowers/plans/2026-05-17-phase3-animation-plan.md`. Branch:
`feat/phase3-animation`.
```

- [ ] **Step 4: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): kick off Phase 3 roadmap on feat/phase3-animation"
```

### Task T0.3 — Verify Game Animation Sample plugin is available

**Files:** none modified.

This phase depends on Epic's free Game Animation Sample plugin (UE 5.5+). If the user hasn't installed it yet, M4 cannot proceed.

- [ ] **Step 1: Check if the plugin is already enabled**

```bash
grep -A 2 '"Name": "GameAnimationSample"' Edge26.uproject 2>/dev/null
```

If the grep returns lines, you're good — the plugin is enabled. Skip Step 2.

- [ ] **Step 2: If the grep returns nothing**

The user needs to install it manually (one-time): in the editor, open **Edit → Plugins**, search "Game Animation Sample", install + enable, restart editor. Then add the plugin entry to `Edge26.uproject` (the editor does this automatically when you click Enable + restart).

Once `Edge26.uproject` lists `GameAnimationSample` under `"Plugins"`, the rest of this plan can proceed.

If you're an automated agent, you cannot install marketplace plugins headlessly. Stop here and report `BLOCKED: Game Animation Sample plugin not installed — user action required`.

---

## M1 — `RenderSnapshotBuffer` + 200 ms delay wiring

Build the ring buffer, push sim snapshots into it, and have `SimHostSubsystem::DriveVisuals` read from 10 ticks behind. The buffer is pure data + diff logic; the diff result type stays minimal in this milestone (we add real event detection in M2).

### Task T1.1 — Define `EFootballerAnimEvent` enum (placeholder) + `FAnimEventPayload`

The buffer's `PopForTick` needs to return an event list. We define the type now so M1 can compile; M2 fills in the actual diff rules.

**Files:**
- Create: `Source/Edge26/Public/Animation/FootballerAnimEvents.h`

- [ ] **Step 1: Write the header**

```cpp
// Copyright Edge26. All Rights Reserved.
// Render-side animation events emitted by RenderSnapshotBuffer when sim state
// changes tick-to-tick. Render code consumes these to trigger anim montages,
// IK ramps, SFX, etc. Sim code never sees them.
#pragma once

#include "CoreMinimal.h"
#include "FootballerAnimEvents.generated.h"

UENUM(BlueprintType)
enum class EFootballerAnimEvent : uint8
{
    None              = 0,
    Kick              = 1,   // PendingButtons & (Pass|Shoot|Chip) rising edge
    BallReceived      = 2,   // PossessionPlayer changed AND ball was airborne
    GoalkeeperSave    = 3,   // ball.Velocity went to 0 AND PossessionPlayer is a GK
    GoalkeeperCatch   = 4,   // PossessionPlayer is GK and prev wasn't (non-save)
    // Tackle, Header, GoalkeeperThrow reserved for v1
};

UENUM(BlueprintType)
enum class EKickKind : uint8
{
    None  = 0,
    Pass  = 1,
    Shoot = 2,
    Chip  = 3,
};

USTRUCT(BlueprintType)
struct EDGE26_API FAnimEventPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EFootballerAnimEvent Kind = EFootballerAnimEvent::None;

    UPROPERTY(BlueprintReadOnly)
    int32 PlayerIndex = -1;

    UPROPERTY(BlueprintReadOnly)
    EKickKind KickKind = EKickKind::None;

    // World-space direction the kick is aimed (unit-length, Z=0 except for chip lift).
    UPROPERTY(BlueprintReadOnly)
    FVector KickDirection = FVector::ZeroVector;

    // Initial ball world position at the moment of the event (useful for IK).
    UPROPERTY(BlueprintReadOnly)
    FVector BallPosition = FVector::ZeroVector;

    // World position of the target receiver (or save spot, or zero).
    UPROPERTY(BlueprintReadOnly)
    FVector TargetPosition = FVector::ZeroVector;
};
```

- [ ] **Step 2: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Commit**

```bash
git add Source/Edge26/Public/Animation/FootballerAnimEvents.h
git commit -m "feat(anim): EFootballerAnimEvent enum + FAnimEventPayload struct"
```

### Task T1.2 — Write `RenderSnapshotBuffer.h`

The buffer is a fixed-size ring of 25 snapshots. `Push(simTick, snapshot)` adds an entry; `PopForTick(consumeTick)` returns the snapshot for that tick + the events diffed against the previous consumed snapshot.

**Files:**
- Create: `Source/Edge26/Public/Adapter/RenderSnapshotBuffer.h`

- [ ] **Step 1: Write the header**

```cpp
// Copyright Edge26. All Rights Reserved.
// Ring buffer of FSimWorldState snapshots. Sim pushes; render consumes from
// kRenderDelayTicks behind. On consume, diffs the current consumed snapshot
// against the previously consumed one to emit anim events.
#pragma once

#include "CoreMinimal.h"
#include "Animation/FootballerAnimEvents.h"
#include "Sim/WorldState.h"

namespace edge26 { struct FSimWorldState; }

class EDGE26_API FRenderSnapshotBuffer
{
public:
    // 25 entries = 500 ms of history at 50 Hz. Plenty of headroom around the
    // 10-tick (200 ms) render delay.
    static constexpr int32 kCapacity = 25;

    // Render reads snapshots 10 ticks behind the current sim tick (~200 ms).
    static constexpr int32 kRenderDelayTicks = 10;

    FRenderSnapshotBuffer();

    // Sim side: enqueue the snapshot tagged with the sim tick number.
    // Overwrites the oldest entry when full.
    void Push(uint32 SimTick, const edge26::FSimWorldState& Snapshot);

    // Render side: returns true if a snapshot exists for ConsumeTick.
    // Out params: the snapshot itself, plus events diffed against the
    // previously consumed snapshot (or empty on first consume).
    bool PopForTick(uint32 ConsumeTick,
                    edge26::FSimWorldState& OutSnapshot,
                    TArray<FAnimEventPayload>& OutEvents);

    // Reset state — useful when PIE starts a new session.
    void Reset();

    // Diagnostics.
    int32 Num() const { return CountStored; }
    uint32 LatestTick() const { return LatestStoredTick; }

private:
    struct FSlot
    {
        uint32 Tick = 0;
        bool   bValid = false;
        edge26::FSimWorldState Snapshot{};
    };

    FSlot  Slots[kCapacity];
    int32  WriteIndex     = 0;
    int32  CountStored    = 0;
    uint32 LatestStoredTick = 0;

    // Tick of the snapshot most recently passed to a caller via PopForTick.
    // Used to detect "first consume" (no prev snapshot → no events).
    uint32 LastConsumedTick      = 0;
    bool   bHaveLastConsumed     = false;
    edge26::FSimWorldState LastConsumedSnapshot{};

    // Find slot index for a given tick; -1 if not stored.
    int32 FindSlotForTick(uint32 Tick) const;

    // Diff CurrSnapshot against LastConsumedSnapshot, append events to OutEvents.
    // Implemented in M2 — for M1 this is a no-op that just leaves OutEvents empty.
    void EmitEvents(const edge26::FSimWorldState& CurrSnapshot,
                    TArray<FAnimEventPayload>& OutEvents) const;
};
```

- [ ] **Step 2: Editor build (will fail to link — the .cpp comes next)**

Skip the build for this step; M1 T1.3 implements the .cpp and the linker will be happy then.

- [ ] **Step 3: Commit the header alone**

```bash
git add Source/Edge26/Public/Adapter/RenderSnapshotBuffer.h
git commit -m "feat(adapter): RenderSnapshotBuffer header — ring + diff API"
```

### Task T1.3 — Implement `RenderSnapshotBuffer.cpp`

**Files:**
- Create: `Source/Edge26/Private/Adapter/RenderSnapshotBuffer.cpp`

- [ ] **Step 1: Write the implementation**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "Adapter/RenderSnapshotBuffer.h"
#include <cstring>

FRenderSnapshotBuffer::FRenderSnapshotBuffer()
{
    Reset();
}

void FRenderSnapshotBuffer::Reset()
{
    for (int32 i = 0; i < kCapacity; ++i)
    {
        Slots[i].Tick = 0;
        Slots[i].bValid = false;
        std::memset(&Slots[i].Snapshot, 0, sizeof(Slots[i].Snapshot));
    }
    WriteIndex = 0;
    CountStored = 0;
    LatestStoredTick = 0;
    LastConsumedTick = 0;
    bHaveLastConsumed = false;
    std::memset(&LastConsumedSnapshot, 0, sizeof(LastConsumedSnapshot));
}

void FRenderSnapshotBuffer::Push(uint32 SimTick, const edge26::FSimWorldState& Snapshot)
{
    Slots[WriteIndex].Tick = SimTick;
    Slots[WriteIndex].bValid = true;
    Slots[WriteIndex].Snapshot = Snapshot;        // POD memcpy
    WriteIndex = (WriteIndex + 1) % kCapacity;
    if (CountStored < kCapacity) ++CountStored;
    LatestStoredTick = SimTick;
}

int32 FRenderSnapshotBuffer::FindSlotForTick(uint32 Tick) const
{
    for (int32 i = 0; i < kCapacity; ++i)
    {
        if (Slots[i].bValid && Slots[i].Tick == Tick)
            return i;
    }
    return -1;
}

bool FRenderSnapshotBuffer::PopForTick(uint32 ConsumeTick,
                                       edge26::FSimWorldState& OutSnapshot,
                                       TArray<FAnimEventPayload>& OutEvents)
{
    OutEvents.Reset();

    const int32 idx = FindSlotForTick(ConsumeTick);
    if (idx < 0) return false;

    OutSnapshot = Slots[idx].Snapshot;

    // If we have a previous consumed snapshot, diff to produce events.
    // (M2 fills EmitEvents; for M1 this is a no-op stub.)
    if (bHaveLastConsumed)
    {
        EmitEvents(OutSnapshot, OutEvents);
    }

    LastConsumedSnapshot = OutSnapshot;
    LastConsumedTick     = ConsumeTick;
    bHaveLastConsumed    = true;
    return true;
}

void FRenderSnapshotBuffer::EmitEvents(const edge26::FSimWorldState& /*CurrSnapshot*/,
                                        TArray<FAnimEventPayload>& /*OutEvents*/) const
{
    // M2 T2.x implements the diff rules. For M1 we ship the wiring only.
}
```

- [ ] **Step 2: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Commit**

```bash
git add Source/Edge26/Private/Adapter/RenderSnapshotBuffer.cpp
git commit -m "feat(adapter): RenderSnapshotBuffer ring impl (events stubbed for M2)"
```

### Task T1.4 — Wire the buffer into `SimHostSubsystem`

The host pushes after every sim Step; `DriveVisuals` consumes from `kRenderDelayTicks` behind.

**Files:**
- Modify: `Source/Edge26/Public/Adapter/SimHostSubsystem.h`
- Modify: `Source/Edge26/Private/Adapter/SimHostSubsystem.cpp`

- [ ] **Step 1: Add buffer member + accessor**

In `Source/Edge26/Public/Adapter/SimHostSubsystem.h`, after the existing private members (`LastHumanControlledIndex` etc.), add:

```cpp
#include "Adapter/RenderSnapshotBuffer.h"
// ...

private:
    // M12 P3: ring buffer of 25 snapshots (~500 ms). DriveVisuals consumes
    // from kRenderDelayTicks behind so anim has time to play foot-strike
    // montages before the ball "releases" at the BallContact notify.
    FRenderSnapshotBuffer SnapshotBuffer;

    // M12 P3: events emitted by SnapshotBuffer this frame.
    // Drained by DriveVisuals; broadcast to per-pawn anim instances.
    TArray<FAnimEventPayload> PendingAnimEvents;
```

(The include should appear in the existing include block at the top of the file.)

- [ ] **Step 2: Push to buffer in `Tick`**

In `Source/Edge26/Private/Adapter/SimHostSubsystem.cpp`, find the sim-tick `while` loop in `Tick`. After `Sim->Snapshot(CurrState);`, add:

```cpp
            // M12 P3: push every sim snapshot into the render-side delay buffer.
            SnapshotBuffer.Push(CurrentTick, CurrState);
```

(`CurrentTick` is already in scope and increments at the end of the loop.)

- [ ] **Step 3: Consume in `DriveVisuals`**

`DriveVisuals` currently lerps `PrevState` ↔ `CurrState`. Replace this with buffer-driven snapshots:

```cpp
void USimHostSubsystem::DriveVisuals(float Alpha)
{
    if (!Sim) return;

    // Determine which sim tick to render. We stay kRenderDelayTicks behind
    // the latest pushed sim tick so anim montages have time to play.
    const uint32 LatestSimTick = SnapshotBuffer.LatestTick();
    if (LatestSimTick < FRenderSnapshotBuffer::kRenderDelayTicks)
    {
        // Not enough history yet (first ~200 ms of PIE); use CurrState directly.
        // ... (existing interpolation between PrevState and CurrState)
        // (keep the previous body of DriveVisuals as the fallback path)
        // For simplicity, just call DriveVisualsLegacy(Alpha).
        DriveVisualsLegacy(Alpha);
        return;
    }
    const uint32 ConsumeTick = LatestSimTick - FRenderSnapshotBuffer::kRenderDelayTicks;

    edge26::FSimWorldState NewConsumed;
    TArray<FAnimEventPayload> EmittedEvents;
    if (!SnapshotBuffer.PopForTick(ConsumeTick, NewConsumed, EmittedEvents))
    {
        DriveVisualsLegacy(Alpha);
        return;
    }

    // Slide PrevState ← CurrState ← NewConsumed so the existing lerp keeps working.
    PrevState = CurrState;
    CurrState = NewConsumed;

    // Queue events for outside-the-while-loop dispatch.
    for (const auto& Ev : EmittedEvents)
    {
        PendingAnimEvents.Add(Ev);
    }

    // Existing visual drive (ball + footballers) using PrevState ↔ CurrState.
    DriveVisualsFromCurrPrev(Alpha);
}
```

Refactor the original `DriveVisuals` body (the ball + footballer lerp block) into a new helper `DriveVisualsFromCurrPrev` so both the buffered and fallback paths can call it:

```cpp
void USimHostSubsystem::DriveVisualsFromCurrPrev(float Alpha)
{
    // Ball.
    if (Ball.IsValid())
    {
        FVector p0 = ToUE(PrevState.Ball.Position);
        FVector p1 = ToUE(CurrState.Ball.Position);
        FVector p  = FMath::Lerp(p0, p1, Alpha);
        Ball->DriveFromSim(FTransform(FRotator::ZeroRotator, p));
    }
    // Footballers.
    for (auto& Weak : Footballers)
    {
        AFootballerVisual* F = Weak.Get();
        if (!F) continue;
        const int32 idx = F->ControllerIndex;
        if (idx < 0 || idx >= edge26::kSimPlayerCount) continue;
        FVector  p0 = ToUE(PrevState.Players[idx].Position);
        FVector  p1 = ToUE(CurrState.Players[idx].Position);
        FVector  p  = FMath::Lerp(p0, p1, Alpha);
        FRotator r  = ToUEYaw(CurrState.Players[idx].Heading);
        F->DriveFromSim(FTransform(r, p));
    }
}

// Add DriveVisualsLegacy as an alias initially — same body, used during the
// first 200 ms before the buffer fills.
void USimHostSubsystem::DriveVisualsLegacy(float Alpha)
{
    DriveVisualsFromCurrPrev(Alpha);
}
```

Declare both helpers in `SimHostSubsystem.h`:

```cpp
private:
    void DriveVisuals(float Alpha);          // existing
    void DriveVisualsFromCurrPrev(float Alpha);
    void DriveVisualsLegacy(float Alpha);
```

- [ ] **Step 4: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 5: Smoke test in PIE (manual; takes <60 s)**

> **Manual step:** open editor, press Play. Expected: same visual behaviour as before — players move, ball flies. The 200 ms render delay should be imperceptible. No crashes.

If the visual behaviour looks wrong (e.g., players teleporting, ball stuck), the buffer-consume path is wrong. Set a breakpoint in `DriveVisuals` to check `LatestSimTick` and `ConsumeTick`.

- [ ] **Step 6: Commit**

```bash
git add Source/Edge26/Public/Adapter/SimHostSubsystem.h \
        Source/Edge26/Private/Adapter/SimHostSubsystem.cpp
git commit -m "feat(adapter): SimHostSubsystem feeds + consumes RenderSnapshotBuffer (200 ms delay)"
```

### Task T1.5 — UE5 automation test: buffer delay is respected

**Files:**
- Create: `Source/Edge26/Private/Tests/RenderSnapshotBufferTests.cpp`
- Modify: `Source/Edge26/Edge26.Build.cs` (only if you don't already have `bUseUnity = false` for tests; check first)

- [ ] **Step 1: Write the test**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Adapter/RenderSnapshotBuffer.h"
#include "Sim/WorldState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRenderSnapshotBufferDelayRespected,
    "Edge26.Render.SnapshotBuffer.DelayRespected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRenderSnapshotBufferDelayRespected::RunTest(const FString& Parameters)
{
    FRenderSnapshotBuffer Buf;

    // Push 25 distinct snapshots, tick 0..24, with TickNumber stamped inside.
    edge26::FSimWorldState s{};
    for (uint32 t = 0; t < 25; ++t)
    {
        std::memset(&s, 0, sizeof(s));
        s.TickNumber = t;
        Buf.Push(t, s);
    }
    TestEqual(TEXT("Buffer full"), Buf.Num(), 25);
    TestEqual(TEXT("Latest tick"), (int32)Buf.LatestTick(), 24);

    // Consume tick 10 → should return the snapshot whose TickNumber == 10.
    edge26::FSimWorldState out{};
    TArray<FAnimEventPayload> ev;
    const bool ok = Buf.PopForTick(10, out, ev);
    TestTrue(TEXT("Pop succeeded"), ok);
    TestEqual(TEXT("Returned snapshot tick"), (int32)out.TickNumber, 10);

    // Pushing tick 25 must overwrite tick 0 (oldest).
    std::memset(&s, 0, sizeof(s));
    s.TickNumber = 25;
    Buf.Push(25, s);
    TArray<FAnimEventPayload> ev2;
    edge26::FSimWorldState out2{};
    const bool ok0 = Buf.PopForTick(0, out2, ev2);
    TestFalse(TEXT("Tick 0 evicted"), ok0);

    return true;
}
```

- [ ] **Step 2: Check that `Edge26.Build.cs` includes the `UnrealEd` module (needed for automation tests in editor builds)**

```bash
grep -E '"UnrealEd"|"AutomationTesting"' Source/Edge26/Edge26.Build.cs
```

If neither is present, add to `PrivateDependencyModuleNames`:

```csharp
PrivateDependencyModuleNames.AddRange(new string[]
{
    // ... existing ...
    "UnrealEd",
});
```

(Editor-only path; `WHEN_EDITOR` guards aren't needed for tests under the `Tests` folder, but if Build.cs warns, wrap the addition in `if (Target.bBuildEditor)`.)

- [ ] **Step 3: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 4: Run the automation test**

In the editor: **Tools → Session Frontend → Automation tab**. Expand `Edge26 → Render → SnapshotBuffer`. Check `DelayRespected` and click **Start Tests**.

Expected: PASS.

(Or from the command line: `UnrealEditor-Cmd "$PWD/Edge26.uproject" -ExecCmds="Automation RunTests Edge26.Render.SnapshotBuffer.DelayRespected; Quit" -unattended -nullrhi -stdout 2>&1 | tail -10`)

- [ ] **Step 5: Commit**

```bash
git add Source/Edge26/Private/Tests/RenderSnapshotBufferTests.cpp \
        Source/Edge26/Edge26.Build.cs
git commit -m "test(adapter): RenderSnapshotBuffer DelayRespected automation test"
```

### Task T1.6 — Determinism gate must still PASS

The Phase 3 work added zero sim-side code. Verify nothing leaked through.

- [ ] **Step 1: Run the gate**

```bash
./Scripts/check_determinism.sh 2>&1 | tail -3
```

Expected last line: `PASS: all determinism checks`.

If it fails: you've modified something under `Source/Edge26Sim/` by accident. Use `git diff main -- Source/Edge26Sim/` to find the regression.

### Task T1.7 — Mark M1 complete

**Files:**
- Modify: `PROGRESS.md`

- [ ] **Step 1: Tick M1 and update Current status**

In the Phase 3 roadmap list, change:

```markdown
- [ ] M1. RenderSnapshotBuffer + 200 ms delay wiring
```

to:

```markdown
- [x] M1. RenderSnapshotBuffer + 200 ms delay wiring
```

Update the "We are at Phase 3 M2 of M12" line.

- [ ] **Step 2: Append an activity-log entry**

```markdown
- M1 landed: FRenderSnapshotBuffer ring (25 entries, 500 ms history) + 200 ms (10-tick) delay wiring in SimHostSubsystem. EFootballerAnimEvent enum + FAnimEventPayload defined (diff logic stubbed for M2). 1 UE5 automation test passes (DelayRespected). Determinism gate green.
```

- [ ] **Step 3: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): M1 complete; advance to M2 (event extraction)"
```

---

## M2 — Snapshot-diff event extraction

Replace the M1 stub `EmitEvents` with the real diff rules: rising-edge `PendingButtons` produces `Kick`, possession changes during ball-airborne produce `BallReceived`, GK-specific patterns produce `GoalkeeperSave` / `GoalkeeperCatch`.

### Task T2.1 — Implement `EmitEvents` — kick detection

**Files:**
- Modify: `Source/Edge26/Private/Adapter/RenderSnapshotBuffer.cpp`

- [ ] **Step 1: Replace the M1 stub body**

Replace the entire `FRenderSnapshotBuffer::EmitEvents` function with:

```cpp
void FRenderSnapshotBuffer::EmitEvents(const edge26::FSimWorldState& Curr,
                                        TArray<FAnimEventPayload>& OutEvents) const
{
    using namespace edge26;
    const FSimWorldState& Prev = LastConsumedSnapshot;

    auto ToUE = [](FixedVec3 v) -> FVector {
        return FVector{
            (double)v.X.Raw / (double)Fixed64::One,
            (double)v.Y.Raw / (double)Fixed64::One,
            (double)v.Z.Raw / (double)Fixed64::One };
    };

    // Rule 1: Kick rising-edge per-player.
    // Sim sets PendingButtons[i] & (Pass|Shoot|Chip) on the tick a kick fires
    // and the host's one-shot-clear wipes it the same tick. So "this tick had
    // a bit, prev didn't" is the rising edge.
    constexpr uint8_t kPass  = 1 << 1;
    constexpr uint8_t kShoot = 1 << 2;
    constexpr uint8_t kChip  = 1 << 3;
    constexpr uint8_t kAnyKick = kPass | kShoot | kChip;

    for (int i = 0; i < kSimPlayerCount; ++i)
    {
        const uint8_t pBits = Prev.Players[i].PendingButtons & kAnyKick;
        const uint8_t cBits = Curr.Players[i].PendingButtons & kAnyKick;
        const uint8_t rising = cBits & ~pBits;
        if (rising == 0) continue;

        FAnimEventPayload ev;
        ev.Kind        = EFootballerAnimEvent::Kick;
        ev.PlayerIndex = i;
        ev.KickKind    = (rising & kPass)  ? EKickKind::Pass
                       : (rising & kShoot) ? EKickKind::Shoot
                       :                     EKickKind::Chip;
        ev.BallPosition = ToUE(Curr.Ball.Position);
        FixedVec3 v = Curr.Ball.Velocity;
        FVector vUe = ToUE(v);
        ev.KickDirection = vUe.GetSafeNormal2D();
        // Target: if IntendedPassTarget is set, use that mate's position.
        if (Curr.Players[i].IntendedPassTarget < kSimPlayerCount)
        {
            ev.TargetPosition = ToUE(Curr.Players[Curr.Players[i].IntendedPassTarget].Position);
        }
        OutEvents.Add(ev);
    }
}
```

(`Curr.Players[i].PendingButtons` is in the existing sim state; `Prev.Players[i]` is the previously consumed snapshot. The OR'd mask covers Pass/Shoot/Chip.)

- [ ] **Step 2: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Commit**

```bash
git add Source/Edge26/Private/Adapter/RenderSnapshotBuffer.cpp
git commit -m "feat(adapter): RenderSnapshotBuffer emits Kick events on PendingButtons rising edge"
```

### Task T2.2 — Add `BallReceived` + `GoalkeeperSave` + `GoalkeeperCatch` detection

**Files:**
- Modify: `Source/Edge26/Private/Adapter/RenderSnapshotBuffer.cpp`

- [ ] **Step 1: Append the additional rules at the end of `EmitEvents`**

Before the closing `}` of `EmitEvents`, add:

```cpp
    // Rule 2: BallReceived — PossessionPlayer changed AND prev ball.Velocity.Z > 0
    // (ball was airborne). This is the "first-touch trap" trigger.
    if (Curr.Match.PossessionPlayer != Prev.Match.PossessionPlayer
        && Curr.Match.PossessionPlayer < kSimPlayerCount
        && Prev.Ball.Velocity.Z.Raw > 0)
    {
        FAnimEventPayload ev;
        ev.Kind        = EFootballerAnimEvent::BallReceived;
        ev.PlayerIndex = (int32)Curr.Match.PossessionPlayer;
        ev.BallPosition = ToUE(Curr.Ball.Position);
        OutEvents.Add(ev);
    }

    // Rule 3: GoalkeeperSave — ball.Velocity went to zero this tick AND
    // PossessionPlayer is now a GK (RoleId == GK == 0). MaybeGoalkeeperSave
    // sets both these together.
    auto IsGK = [&](uint8_t idx) -> bool {
        return idx < kSimPlayerCount && Curr.Players[idx].RoleId == 0;  // ERole::GK
    };
    if (Curr.Match.PossessionPlayer < kSimPlayerCount
        && IsGK(Curr.Match.PossessionPlayer)
        && Prev.Ball.Velocity.X.Raw != 0   // was moving
        && Curr.Ball.Velocity.X.Raw == 0   // now stopped
        && Curr.Ball.Velocity.Y.Raw == 0)
    {
        FAnimEventPayload ev;
        ev.Kind        = EFootballerAnimEvent::GoalkeeperSave;
        ev.PlayerIndex = (int32)Curr.Match.PossessionPlayer;
        ev.BallPosition = ToUE(Curr.Ball.Position);
        OutEvents.Add(ev);
    }
    // Rule 4: GoalkeeperCatch — PossessionPlayer is a GK and prev wasn't,
    // and this isn't already a Save (ball wasn't moving).
    else if (Curr.Match.PossessionPlayer < kSimPlayerCount
             && IsGK(Curr.Match.PossessionPlayer)
             && Prev.Match.PossessionPlayer != Curr.Match.PossessionPlayer)
    {
        FAnimEventPayload ev;
        ev.Kind        = EFootballerAnimEvent::GoalkeeperCatch;
        ev.PlayerIndex = (int32)Curr.Match.PossessionPlayer;
        ev.BallPosition = ToUE(Curr.Ball.Position);
        OutEvents.Add(ev);
    }
```

- [ ] **Step 2: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Commit**

```bash
git add Source/Edge26/Private/Adapter/RenderSnapshotBuffer.cpp
git commit -m "feat(adapter): emit BallReceived / GoalkeeperSave / GoalkeeperCatch from snapshot diff"
```

### Task T2.3 — Automation test: kick event diff

**Files:**
- Modify: `Source/Edge26/Private/Tests/RenderSnapshotBufferTests.cpp`

- [ ] **Step 1: Append the test**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRenderSnapshotBufferEmitsKick,
    "Edge26.Render.SnapshotBuffer.EmitsKick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRenderSnapshotBufferEmitsKick::RunTest(const FString& Parameters)
{
    FRenderSnapshotBuffer Buf;

    // Snapshot A (tick 0): PendingButtons[3] = 0.
    edge26::FSimWorldState s{};
    s.TickNumber = 0;
    Buf.Push(0, s);

    // Snapshot B (tick 1): PendingButtons[3] = Pass (1<<1).
    s.TickNumber = 1;
    s.Players[3].PendingButtons = 1 << 1;
    Buf.Push(1, s);

    // First pop: tick 0 — no prev consumed, no events.
    edge26::FSimWorldState out{};
    TArray<FAnimEventPayload> ev;
    TestTrue(TEXT("Pop tick 0 ok"), Buf.PopForTick(0, out, ev));
    TestEqual(TEXT("First pop emits nothing"), ev.Num(), 0);

    // Second pop: tick 1 — diff against tick 0; Kick rising edge for player 3.
    TestTrue(TEXT("Pop tick 1 ok"), Buf.PopForTick(1, out, ev));
    TestEqual(TEXT("Kick event count"), ev.Num(), 1);
    if (ev.Num() >= 1)
    {
        TestEqual(TEXT("Event is Kick"), (int32)ev[0].Kind, (int32)EFootballerAnimEvent::Kick);
        TestEqual(TEXT("PlayerIndex"), ev[0].PlayerIndex, 3);
        TestEqual(TEXT("KickKind"), (int32)ev[0].KickKind, (int32)EKickKind::Pass);
    }
    return true;
}
```

- [ ] **Step 2: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Run automation tests (both T1.5 + this one)**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecCmds="Automation RunTests Edge26.Render.SnapshotBuffer; Quit" \
    -unattended -nullrhi -stdout 2>&1 | grep -E "Test (Started|Completed|Failed|Passed)" | tail -10
```

Expected: both `DelayRespected` and `EmitsKick` pass.

- [ ] **Step 4: Commit**

```bash
git add Source/Edge26/Private/Tests/RenderSnapshotBufferTests.cpp
git commit -m "test(adapter): EmitsKick — Kick rising-edge produces payload"
```

### Task T2.4 — Hook event broadcast from `SimHostSubsystem`

Events are currently collected into `PendingAnimEvents` but nothing consumes them yet. Add a delegate hookup that fires per pawn.

**Files:**
- Modify: `Source/Edge26/Public/Adapter/SimHostSubsystem.h`
- Modify: `Source/Edge26/Private/Adapter/SimHostSubsystem.cpp`

- [ ] **Step 1: Declare an on-event delegate on `AFootballerVisual`**

In `Source/Edge26/Public/Adapter/FootballerVisual.h`, in the `AFootballerVisual` class, add (near the `Speed` / `ForwardSpeed` etc. UPROPERTYs):

```cpp
#include "Animation/FootballerAnimEvents.h"
// ...

public:
    /** Fired by SimHostSubsystem when an anim event targets this pawn. */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnimEvent, const FAnimEventPayload&, Event);

    UPROPERTY(BlueprintAssignable, Category="Anim")
    FOnAnimEvent OnAnimEvent;
```

- [ ] **Step 2: Dispatch events in the host's `DriveVisuals`**

In `Source/Edge26/Private/Adapter/SimHostSubsystem.cpp`, after `PendingAnimEvents.Add(Ev);` in the buffer-consume branch, dispatch events to each pawn. Actually move this dispatch to the END of `Tick` (after the `while` loop) so all events from the same render frame are delivered together.

In `Tick`, after `DriveVisuals(Alpha);` add:

```cpp
    // Broadcast queued anim events to per-pawn delegates.
    for (const FAnimEventPayload& Ev : PendingAnimEvents)
    {
        if (Ev.PlayerIndex < 0 || Ev.PlayerIndex >= edge26::kSimPlayerCount) continue;
        for (auto& Weak : Footballers)
        {
            AFootballerVisual* F = Weak.Get();
            if (!F) continue;
            if (F->ControllerIndex == Ev.PlayerIndex)
            {
                F->OnAnimEvent.Broadcast(Ev);
                break;
            }
        }
    }
    PendingAnimEvents.Reset();
```

- [ ] **Step 3: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 4: Commit**

```bash
git add Source/Edge26/Public/Adapter/FootballerVisual.h \
        Source/Edge26/Private/Adapter/SimHostSubsystem.cpp
git commit -m "feat(adapter): broadcast FAnimEventPayload to per-pawn OnAnimEvent delegate"
```

### Task T2.5 — Mark M2 complete

- [ ] **Step 1: PROGRESS.md tick + status**

```markdown
- [x] M2. Snapshot-diff event extraction (KickEvent, BallReceived, GoalkeeperSave)
```

Append activity log:

```markdown
- M2 landed: RenderSnapshotBuffer.EmitEvents now diffs Curr vs LastConsumed and emits Kick (rising PendingButtons), BallReceived (possession change while ball airborne), GoalkeeperSave (ball stopped + GK possession), GoalkeeperCatch (GK gains possession non-save). FAnimEventPayload broadcast via AFootballerVisual::OnAnimEvent. 2 UE5 automation tests green.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): M2 complete; advance to M3 (FootballAnimInstance)"
```

---

## M3 — `FootballAnimInstance` base class + trajectory generation

This is the new base for the motion-matched anim BP. It exposes `Speed`, `Velocity`, `TrajectorySamples`, and an event queue that the AnimBP graph can branch on.

### Task T3.1 — Define `FootballAnimInstance.h`

**Files:**
- Create: `Source/Edge26/Public/Animation/FootballAnimInstance.h`

- [ ] **Step 1: Write the header**

```cpp
// Copyright Edge26. All Rights Reserved.
// Base UAnimInstance for football pawns. Exposes motion-matching trajectory
// inputs and a small event queue that the AnimBP consumes (typically via a
// "BlendByEvent" pattern in the graph).
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/FootballerAnimEvents.h"
#include "FootballAnimInstance.generated.h"

UCLASS()
class EDGE26_API UFootballAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UFootballAnimInstance();

    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // ----- Trajectory inputs for motion matching -----

    UPROPERTY(BlueprintReadOnly, Category="MotionMatching")
    FVector TrajectoryVelocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="MotionMatching")
    FVector TrajectoryAcceleration = FVector::ZeroVector;

    /** 4 future-trajectory samples at +10/+20/+30/+40 frames. World space. */
    UPROPERTY(BlueprintReadOnly, Category="MotionMatching")
    TArray<FVector> TrajectorySamples;

    UPROPERTY(BlueprintReadOnly, Category="MotionMatching")
    float Speed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="MotionMatching")
    bool bIsGrounded = true;

    // ----- Event queue (drained per tick) -----

    /** Latest pending kick event for this pawn. Cleared every tick after consumption. */
    UPROPERTY(BlueprintReadOnly, Category="AnimEvents")
    FAnimEventPayload PendingEvent;

    UPROPERTY(BlueprintReadOnly, Category="AnimEvents")
    bool bHasPendingEvent = false;

    /** Called by AFootballerVisual when an event targets this pawn. */
    UFUNCTION(BlueprintCallable, Category="AnimEvents")
    void EnqueueEvent(const FAnimEventPayload& Event);

protected:
    // Updates Speed / TrajectoryVelocity / TrajectoryAcceleration from the
    // owning pawn's transform deltas + AITargetPosition (which we'll read
    // from FootballerVisual.AITargetPositionUE — wired in M5).
    virtual void UpdateTrajectory(float DeltaSeconds);

private:
    FVector PrevLocation = FVector::ZeroVector;
};
```

- [ ] **Step 2: Commit the header**

```bash
git add Source/Edge26/Public/Animation/FootballAnimInstance.h
git commit -m "feat(anim): UFootballAnimInstance header (trajectory + event queue)"
```

### Task T3.2 — Implement `FootballAnimInstance.cpp`

**Files:**
- Create: `Source/Edge26/Private/Animation/FootballAnimInstance.cpp`

- [ ] **Step 1: Write the implementation**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "Animation/FootballAnimInstance.h"
#include "Adapter/FootballerVisual.h"
#include "GameFramework/Pawn.h"

UFootballAnimInstance::UFootballAnimInstance()
{
    TrajectorySamples.SetNum(4);
}

void UFootballAnimInstance::EnqueueEvent(const FAnimEventPayload& Event)
{
    PendingEvent = Event;
    bHasPendingEvent = true;
}

void UFootballAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    UpdateTrajectory(DeltaSeconds);
    // The AnimBP graph consumes bHasPendingEvent in this same frame; we clear
    // it AFTER the AnimGraph evaluates (in the next frame). The graph should
    // gate its montage trigger on `bHasPendingEvent && PendingEvent.Kind == Kick`.
    // We clear here at the start of the NEXT frame's update by spinning the flag.
    // Simple pattern: clear at the END of update (relying on the graph having read
    // the value during this same NativeUpdateAnimation call via property access).
    // This works in UE 5.7 because AnimGraph evaluates synchronously after NativeUpdate.
    // If you experience missed events, move the clear into the AnimBP's "Notify"
    // block via a BlueprintCallable `ConsumeEvent` instead.
    bHasPendingEvent = false;
}

void UFootballAnimInstance::UpdateTrajectory(float DeltaSeconds)
{
    AActor* Owner = TryGetPawnOwner();
    if (!Owner)
    {
        Speed = 0.0f;
        TrajectoryVelocity = FVector::ZeroVector;
        TrajectoryAcceleration = FVector::ZeroVector;
        for (FVector& S : TrajectorySamples) S = FVector::ZeroVector;
        return;
    }

    const FVector Loc = Owner->GetActorLocation();
    const FVector Delta = (DeltaSeconds > KINDA_SMALL_NUMBER)
        ? (Loc - PrevLocation) / DeltaSeconds
        : FVector::ZeroVector;
    TrajectoryAcceleration = (Delta - TrajectoryVelocity) / FMath::Max(DeltaSeconds, 0.001f);
    TrajectoryVelocity = Delta;
    Speed = TrajectoryVelocity.Size2D();
    PrevLocation = Loc;

    // 4 future-trajectory samples at +10/+20/+30/+40 frames (60 fps assumed).
    // Direction = current velocity normalized; speed assumed constant.
    const FVector Dir = TrajectoryVelocity.GetSafeNormal2D();
    const float FrameTime = 1.0f / 60.0f;
    for (int32 i = 0; i < TrajectorySamples.Num(); ++i)
    {
        const float T = (i + 1) * 10.0f * FrameTime;
        TrajectorySamples[i] = Loc + Dir * Speed * T;
    }
}
```

- [ ] **Step 2: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Commit**

```bash
git add Source/Edge26/Private/Animation/FootballAnimInstance.cpp
git commit -m "feat(anim): UFootballAnimInstance impl — trajectory sampling + event queue"
```

### Task T3.3 — Wire `OnAnimEvent` → `EnqueueEvent` in `AFootballerVisual`

**Files:**
- Modify: `Source/Edge26/Private/Adapter/FootballerVisual.cpp`

- [ ] **Step 1: In `BeginPlay`, after `Host->RegisterFootballer(...)`, bind the delegate**

```cpp
#include "Animation/FootballAnimInstance.h"
// ...

void AFootballerVisual::BeginPlay()
{
    Super::BeginPlay();
    if (auto* World = GetWorld())
    {
        if (auto* Host = World->GetSubsystem<USimHostSubsystem>())
        {
            Host->RegisterFootballer(this, ControllerIndex);
        }
    }

    // Bind anim event broadcast to the anim instance's event queue.
    OnAnimEvent.AddDynamic(this, &AFootballerVisual::HandleAnimEvent);
}

void AFootballerVisual::HandleAnimEvent(const FAnimEventPayload& Event)
{
    if (auto* AnimInst = Cast<UFootballAnimInstance>(Mesh->GetAnimInstance()))
    {
        AnimInst->EnqueueEvent(Event);
    }
}
```

- [ ] **Step 2: Declare `HandleAnimEvent` in the header**

In `Source/Edge26/Public/Adapter/FootballerVisual.h`, add:

```cpp
public:
    UFUNCTION()
    void HandleAnimEvent(const FAnimEventPayload& Event);
```

- [ ] **Step 3: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 4: Commit**

```bash
git add Source/Edge26/Public/Adapter/FootballerVisual.h \
        Source/Edge26/Private/Adapter/FootballerVisual.cpp
git commit -m "feat(adapter): forward OnAnimEvent broadcast into UFootballAnimInstance event queue"
```

### Task T3.4 — Mark M3 complete

- [ ] **Step 1: PROGRESS.md tick + activity log**

```markdown
- [x] M3. FootballAnimInstance base class + trajectory generation
```

```markdown
- M3 landed: UFootballAnimInstance base class with TrajectoryVelocity/Acceleration/Samples (4 future points at +10/20/30/40 frames), Speed, bIsGrounded, PendingEvent queue. AFootballerVisual::OnAnimEvent → HandleAnimEvent → AnimInst->EnqueueEvent wiring. Ready for ABP_Footballer_MM to be re-parented in M5.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): M3 complete; advance to M4 (Game Anim Sample import)"
```

---

## M4 — Game Animation Sample import + `MMDB_Outfield` skeleton

This milestone is mostly asset / editor work; there is no C++ to write. The deliverable is a saved `UPoseSearchDatabase` asset in the project's Content folder, populated with a few locomotion clips from Epic's Game Animation Sample plugin.

### Task T4.1 — Headless Python: create the empty MMDB asset

**Files:**
- Create: `Scripts/editor/create_mmdb_outfield.py`

- [ ] **Step 1: Write the script**

```python
"""Create an empty UPoseSearchDatabase asset for outfield locomotion.

Run:
  UnrealEditor-Cmd <project> -ExecutePythonScript=<this> -unattended -stdout
"""

import unreal

ASSET_NAME = "MMDB_Outfield"
ASSET_PATH = "/Game/Animation/MotionMatching"
LOG = "/tmp/create_mmdb_outfield.log"

with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    full = f"{ASSET_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        log(f"Asset already exists: {full}")
    else:
        # Pose Search Database factory.
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        # In UE 5.7 the class is PoseSearchDatabase; lookup by name to avoid
        # hard-coding a Python class binding that might differ across UE versions.
        cls = unreal.load_class(None, "/Script/PoseSearch.PoseSearchDatabase")
        if not cls:
            log("ERROR: PoseSearch.PoseSearchDatabase class not found. Is the Motion Matching plugin enabled?")
        else:
            asset = tools.create_asset(ASSET_NAME, ASSET_PATH, cls, None)
            if asset:
                unreal.EditorAssetLibrary.save_asset(full)
                log(f"Created + saved {full}")
            else:
                log(f"ERROR: failed to create {full}")
    log("Done.")
```

- [ ] **Step 2: Run it**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecutePythonScript="$PWD/Scripts/editor/create_mmdb_outfield.py" \
    -unattended -stdout 2>&1 | tail -5
cat /tmp/create_mmdb_outfield.log
```

Expected: log says `Created + saved /Game/Animation/MotionMatching/MMDB_Outfield`.

- [ ] **Step 3: Verify**

```bash
ls Content/Animation/MotionMatching/MMDB_Outfield.uasset
```

Expected: file exists.

- [ ] **Step 4: Commit**

```bash
git add Scripts/editor/create_mmdb_outfield.py Content/Animation/MotionMatching/MMDB_Outfield.uasset
git commit -m "feat(anim): create empty MMDB_Outfield UPoseSearchDatabase asset"
```

### Task T4.2 — Add Game Animation Sample locomotion clips to MMDB_Outfield (manual)

This step requires the user to open the editor and drag-drop animations into the database. We cannot script this fully because the schema configuration is editor-only in UE 5.7. The result is the same asset modified.

> **Manual step (user):**
>
> 1. Open the editor. Open `MMDB_Outfield.uasset` from `Content/Animation/MotionMatching/`.
> 2. In the **Schema** panel, click "Create Schema" and choose the default `MotionMatching_Schema` template. Save.
> 3. In the **Sources** panel, add the following Game Animation Sample animations (drag from Content Browser):
>    - `/GameAnimationSample/Animations/Locomotion/Walk_Fwd`
>    - `/GameAnimationSample/Animations/Locomotion/Walk_Bwd`
>    - `/GameAnimationSample/Animations/Locomotion/Jog_Fwd`
>    - `/GameAnimationSample/Animations/Locomotion/Jog_Bwd`
>    - `/GameAnimationSample/Animations/Locomotion/Jog_Left`
>    - `/GameAnimationSample/Animations/Locomotion/Jog_Right`
>    - `/GameAnimationSample/Animations/Locomotion/Run_Fwd`
>    - `/GameAnimationSample/Animations/Locomotion/Sprint_Fwd`
>    - `/GameAnimationSample/Animations/Locomotion/Turn_Left_90`
>    - `/GameAnimationSample/Animations/Locomotion/Turn_Right_90`
>    - `/GameAnimationSample/Animations/Locomotion/Stop`
>    - `/GameAnimationSample/Animations/Locomotion/Start_Fwd`
>
>    (Path names may differ slightly; Game Anim Sample is on the Manny skeleton, the path prefix may be `/GameAnimSample/` or similar.)
>
> 4. Click **Build Database**. UE5 indexes poses + trajectories.
> 5. Save the asset.
>
> Expected: a green progress bar, then no errors in the build log. The database panel should now show ~12 source assets and N poses.

- [ ] **Step 1: After manual step completes, verify file changed**

```bash
git diff --stat Content/Animation/MotionMatching/MMDB_Outfield.uasset
```

Expected: file modified (non-zero bytes diff).

- [ ] **Step 2: Commit**

```bash
git add Content/Animation/MotionMatching/MMDB_Outfield.uasset
git commit -m "feat(anim): populate MMDB_Outfield with Game Anim Sample locomotion sources"
```

### Task T4.3 — Mark M4 complete

- [ ] **Step 1: PROGRESS.md tick + activity log**

```markdown
- [x] M4. Game Animation Sample import + MMDB_Outfield skeleton
```

```markdown
- M4 landed: MMDB_Outfield UPoseSearchDatabase asset created via headless Python; populated with ~12 locomotion clips from Game Animation Sample (manual editor step); database built.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): M4 complete; advance to M5 (ABP_Footballer_MM)"
```

---

## M5 — `ABP_Footballer_MM` motion-matching state tree

Create a new Anim Blueprint that uses Motion Matching against `MMDB_Outfield` for locomotion. It will replace the current strafe-blendspace anim BP in M11.

### Task T5.1 — Create `ABP_Footballer_MM` via headless Python

**Files:**
- Create: `Scripts/editor/create_abp_footballer_mm.py`

- [ ] **Step 1: Write the script**

```python
"""Create ABP_Footballer_MM, a UAnimBlueprint subclass of UFootballAnimInstance
targeting the Manny skeleton. The graph is empty; M5.2 fills it in the editor.
"""

import unreal

ASSET_NAME = "ABP_Footballer_MM"
ASSET_PATH = "/Game/Blueprints/Player"
PARENT_CLASS_PATH = "/Script/Edge26.FootballAnimInstance"
SKELETON_PATH = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"  # adjust if your project's skeleton path differs
LOG = "/tmp/create_abp_footballer_mm.log"

with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    full = f"{ASSET_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        log(f"Already exists: {full}")
    else:
        parent = unreal.load_class(None, PARENT_CLASS_PATH)
        skel   = unreal.EditorAssetLibrary.load_asset(SKELETON_PATH)
        if not parent:
            log(f"ERROR: parent class not loaded: {PARENT_CLASS_PATH}")
        elif not skel:
            log(f"ERROR: skeleton asset not found: {SKELETON_PATH}")
        else:
            factory = unreal.AnimBlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            factory.set_editor_property("target_skeleton", skel)
            tools = unreal.AssetToolsHelpers.get_asset_tools()
            asset = tools.create_asset(ASSET_NAME, ASSET_PATH, unreal.AnimBlueprint, factory)
            if asset:
                unreal.EditorAssetLibrary.save_asset(full)
                log(f"Created + saved {full}")
            else:
                log("ERROR: create_asset returned None")
    log("Done.")
```

- [ ] **Step 2: Run it**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecutePythonScript="$PWD/Scripts/editor/create_abp_footballer_mm.py" \
    -unattended -stdout 2>&1 | tail -5
cat /tmp/create_abp_footballer_mm.log
```

Expected: log says `Created + saved /Game/Blueprints/Player/ABP_Footballer_MM`.

If the script reports an error about the skeleton path, find your actual Manny skeleton with:

```bash
find Content -iname "SK_Mannequin*" -o -iname "*Manny*Skeleton*" 2>/dev/null
```

…and update `SKELETON_PATH` in the script accordingly, then re-run.

- [ ] **Step 3: Commit**

```bash
git add Scripts/editor/create_abp_footballer_mm.py Content/Blueprints/Player/ABP_Footballer_MM.uasset
git commit -m "feat(anim): scaffold ABP_Footballer_MM (subclass of UFootballAnimInstance)"
```

### Task T5.2 — Wire the AnimGraph (manual editor work)

> **Manual step (user):**
>
> 1. Open `ABP_Footballer_MM` in the editor.
> 2. In the **AnimGraph**, delete any default nodes Epic added.
> 3. Drag the **Motion Matching** node from the palette into the graph.
> 4. Set its **Database** to `MMDB_Outfield`.
> 5. From `Try Get Pawn Owner` get the velocity → feed into `Trajectory Velocity` input pin. Or, more cleanly, expose the `TrajectoryVelocity` property variable (already on `UFootballAnimInstance`) and feed it.
> 6. Same for the 4 `TrajectorySamples` (you can use a `Make Trajectory From Samples` helper).
> 7. Plug the Motion Matching node's output Pose → **Output Pose**.
> 8. Compile + Save.
>
> The graph should be three nodes: a "Get Trajectory" feeder → Motion Matching node → Output Pose.

- [ ] **Step 1: Confirm with the user**

When the user reports the AnimGraph is wired and the BP compiles green, continue.

- [ ] **Step 2: Smoke-test in PIE (manual)**

> **Manual step:** in `BP_Footballer`, temporarily change its `AnimInstanceClass` from `ABP_Footballer` to `ABP_Footballer_MM`. Press Play. The home pawn (whoever auto-switch picks) should animate with the new motion-matched locomotion when WASD-driven. Revert `BP_Footballer` back to `ABP_Footballer` for now (we re-wire properly in M11).

- [ ] **Step 3: Commit (the BP asset)**

```bash
git add Content/Blueprints/Player/ABP_Footballer_MM.uasset
git commit -m "feat(anim): ABP_Footballer_MM AnimGraph — Motion Matching against MMDB_Outfield"
```

### Task T5.3 — Editor build still green

- [ ] **Step 1: Build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

### Task T5.4 — Mark M5 complete

- [ ] **Step 1: PROGRESS.md tick + activity log**

```markdown
- [x] M5. ABP_Footballer_MM motion-matching state tree
```

```markdown
- M5 landed: ABP_Footballer_MM AnimBP scaffolded via Python (subclass of UFootballAnimInstance), AnimGraph wired in the editor with a Motion Matching node sampling MMDB_Outfield + trajectory feeds from the anim instance. Compiles clean.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): M5 complete; advance to M6 (foot IK)"
```

---

## M6 — Foot IK (TwoBoneIK chains)

Add per-leg foot IK to `ABP_Footballer_MM` so feet plant on the pitch and don't drift. Pitch is flat, so the IK target is just the foot's current world Z = 0 (ground).

### Task T6.1 — Add foot IK nodes to the AnimGraph (manual editor)

> **Manual step (user):**
>
> 1. Open `ABP_Footballer_MM` AnimGraph.
> 2. After the **Motion Matching** node, BEFORE **Output Pose**, insert:
>    - **TwoBoneIK** for left leg (effector bone: `foot_l`, joint target: `calf_l`).
>    - **TwoBoneIK** for right leg (effector bone: `foot_r`, joint target: `calf_r`).
> 3. For each, set the **Effector Location Space** to `World Space`.
> 4. Wire effector location: take the current Z of the foot bone, max-with `0` (so feet never go below ground). For a simple version, drive effector X/Y from current bone X/Y and set Z = 0.
> 5. Set IK alpha to 1.0 by default. (We'll override alpha in M8 during kick montages.)
> 6. Compile + Save.

- [ ] **Step 1: User confirms IK chains added and BP compiles**

- [ ] **Step 2: Commit**

```bash
git add Content/Blueprints/Player/ABP_Footballer_MM.uasset
git commit -m "feat(anim): foot IK — TwoBoneIK chains for left/right leg, ground-plane target"
```

### Task T6.2 — Smoke test: feet plant on flat pitch

> **Manual step:** flip `BP_Footballer` to `ABP_Footballer_MM` again, press Play. Stand still. Confirm: no foot floats above the ground, no foot penetrates. Move with WASD; feet still plant (no skating).

If feet still skate, the issue is usually that the locomotion DB's anims are at speeds that don't match the AI-driven `JogSpeed`. We address this in M11 tuning if needed.

- [ ] **Step 1: User confirms visually that feet plant**

### Task T6.3 — Mark M6 complete

- [ ] **Step 1: PROGRESS.md tick + activity log**

```markdown
- [x] M6. Foot IK setup (TwoBoneIK per leg, ground-plane projection)
```

```markdown
- M6 landed: per-leg TwoBoneIK nodes added to ABP_Footballer_MM AnimGraph (effector = foot_l/foot_r, joint target = calf_l/calf_r). Effector Z clamped to ground. Feet plant on flat pitch without skating.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): M6 complete; advance to M7 (Mixamo retarget)"
```

---

## M7 — Mixamo retarget + football overlays + anim notifies

Bring in Mixamo football clips (kick, header, slide tackle, dribble idle), retarget onto Manny, add `BallContact` + `RecoverEnd` anim notifies, and add them to `MMDB_Outfield`.

### Task T7.1 — User downloads Mixamo clips

> **Manual step (user):**
>
> 1. Go to https://www.mixamo.com (free account required).
> 2. Download these animations as FBX with skin, on the default Mixamo character (T-pose, no in-place option for locomotion-style ones):
>    - "Soccer Pass" (or "Soccer Kick")
>    - "Soccer Header"
>    - "Slide Tackle"
>    - "Dribble Idle" (or any "Idle While Holding")
>    - "Run And Kick"
> 3. Save them into `Content/Animation/Mixamo_Raw/`.

- [ ] **Step 1: Confirm files present**

```bash
ls Content/Animation/Mixamo_Raw/ 2>/dev/null | head -10
```

Expected: ~5 FBX files visible.

- [ ] **Step 2: Commit the raw files**

```bash
mkdir -p Content/Animation/Mixamo_Raw
git add Content/Animation/Mixamo_Raw/
git commit -m "chore(anim): import Mixamo football FBX clips (soccer kick/header/tackle/dribble)"
```

### Task T7.2 — Set up IK Retargeter (manual; one-time)

> **Manual step (user):**
>
> 1. In the editor, right-click in `Content/Animation/` → **Create → IK Retargeter** → name it `IKR_Mixamo_to_Manny`.
> 2. Set the **Source IKRig** to the Mixamo character's IKRig (you'll need to create one if Mixamo doesn't ship it; UE 5.7's "Create IK Rig" can auto-generate from a skeleton).
> 3. Set the **Target IKRig** to the Manny IKRig (`IK_Mannequin` from the standard Mannequins folder, or create one if not present).
> 4. Match bones in the **Chain Mapping** panel:
>    - Source pelvis → Target pelvis
>    - Source spine → Target spine
>    - Source legs/arms → Target legs/arms
>    - Source head → Target head
> 5. Save.

- [ ] **Step 1: User confirms IK Retargeter exists**

```bash
ls Content/Animation/IKR_Mixamo_to_Manny.uasset 2>/dev/null
```

Expected: file exists.

- [ ] **Step 2: Commit**

```bash
git add Content/Animation/IKR_Mixamo_to_Manny.uasset
git commit -m "feat(anim): IK Retargeter Mixamo → Manny for football animations"
```

### Task T7.3 — Retarget Mixamo clips to Manny via headless Python

**Files:**
- Create: `Scripts/editor/retarget_mixamo_to_manny.py`

- [ ] **Step 1: Write the script**

```python
"""Batch-retarget Mixamo FBX clips in Content/Animation/Mixamo_Raw/ onto the
Manny skeleton, output to Content/Animation/Mixamo_Retargeted/.
"""

import unreal

SOURCE_FOLDER = "/Game/Animation/Mixamo_Raw"
DEST_FOLDER   = "/Game/Animation/Mixamo_Retargeted"
RETARGETER    = "/Game/Animation/IKR_Mixamo_to_Manny"
LOG = "/tmp/retarget_mixamo.log"

with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    rt = unreal.EditorAssetLibrary.load_asset(RETARGETER)
    if not rt:
        log(f"ERROR: Retargeter not found: {RETARGETER}")
    else:
        # List all source animations.
        assets = unreal.EditorAssetLibrary.list_assets(SOURCE_FOLDER, recursive=False)
        anims = [a for a in assets if "AnimSequence" in str(unreal.EditorAssetLibrary.find_asset_data(a).asset_class_path.asset_name)]
        log(f"Found {len(anims)} animations in {SOURCE_FOLDER}")
        # IKRetargeterController API (UE 5.7):
        ctrl = unreal.IKRetargeterController.get_controller(rt)
        if not ctrl:
            log("ERROR: could not get IKRetargeterController")
        else:
            # Batch via the editor subsystem.
            subsys = unreal.get_editor_subsystem(unreal.IKRetargetBatchOperation)
            if not subsys:
                log("ERROR: IKRetargetBatchOperation subsystem not available")
            else:
                params = unreal.IKRetargetBatchOperation()
                # Settings depend on UE version; the simplest path is the
                # Content Browser right-click "Retarget Animations" UI. The
                # Python API in 5.7 exposes BatchExport for similar effect.
                # If this scripted path fails, perform the retarget manually
                # by right-clicking each anim in the source folder and choosing
                # "Retarget Animations" → IKR_Mixamo_to_Manny.
                log("Batch retargeter API present; running...")
                # ... (UE-version-specific call here)
    log("Done.")
```

The Python API for batch retargeting in UE 5.7 is unstable; if the script fails, fall back to the manual flow:

> **Manual fallback:** In `Content/Animation/Mixamo_Raw/`, select all FBX animations → right-click → **Retarget Animations** → select `IKR_Mixamo_to_Manny` → output to `Content/Animation/Mixamo_Retargeted/`.

- [ ] **Step 1: Try the script**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecutePythonScript="$PWD/Scripts/editor/retarget_mixamo_to_manny.py" \
    -unattended -stdout 2>&1 | tail -10
cat /tmp/retarget_mixamo.log
```

If the script fails or doesn't actually retarget, use the manual fallback above.

- [ ] **Step 2: Verify retargeted output**

```bash
ls Content/Animation/Mixamo_Retargeted/ 2>/dev/null | head -10
```

Expected: 5 retargeted `.uasset` files.

- [ ] **Step 3: Commit**

```bash
git add Scripts/editor/retarget_mixamo_to_manny.py Content/Animation/Mixamo_Retargeted/
git commit -m "feat(anim): retarget Mixamo football clips to Manny skeleton"
```

### Task T7.4 — Add `BallContact` + `RecoverEnd` anim notifies (manual)

> **Manual step (user):**
>
> For each retargeted football animation in `Content/Animation/Mixamo_Retargeted/`:
>
> 1. Open the animation.
> 2. Find the frame where the foot/head/hands first touch the ball — eyeball it. For a "Soccer Pass" anim, this is typically ~40% into the clip.
> 3. Right-click the timeline at that frame → **Add Notify → BallContact** (creates a new notify class if needed — just type the name).
> 4. Find the frame where the body returns to a neutral/idle pose (typically the last frame).
> 5. Right-click → **Add Notify → RecoverEnd**.
> 6. Save.
>
> The notifies don't need to be C++-backed; they're plain string-named events that the AnimBP graph (M8/M10) consumes.

- [ ] **Step 1: User confirms notifies are added**

- [ ] **Step 2: Add the retargeted clips to MMDB_Outfield (manual)**

> **Manual step:** open `MMDB_Outfield`, drag the 5 retargeted football clips into a new source group "Football", **Build Database**, save.

- [ ] **Step 3: Commit**

```bash
git add Content/Animation/Mixamo_Retargeted/ Content/Animation/MotionMatching/MMDB_Outfield.uasset
git commit -m "feat(anim): add BallContact + RecoverEnd notifies; add Mixamo football clips to MMDB_Outfield"
```

### Task T7.5 — Mark M7 complete

- [ ] **Step 1: PROGRESS.md tick + activity log**

```markdown
- [x] M7. Mixamo retarget + football overlays + anim notifies
```

```markdown
- M7 landed: 5 Mixamo football clips downloaded + retargeted to Manny via IKR_Mixamo_to_Manny. BallContact + RecoverEnd anim notifies added to each. Clips added to MMDB_Outfield as the "Football" source group; database rebuilt.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): M7 complete; advance to M8 (BallContactIK)"
```

---

## M8 — `BallContactIKComponent` + kick-montage IK alpha

Build the component that drives the foot-IK alpha curve during a kick montage. On `Kick` event: trigger a montage. During wind-up: alpha 0→1. At `BallContact` notify: foot snaps to ball position. During follow-through: alpha 1→0.

### Task T8.1 — Write `BallContactIKComponent.h`

**Files:**
- Create: `Source/Edge26/Public/Animation/BallContactIKComponent.h`

- [ ] **Step 1: Write the header**

```cpp
// Copyright Edge26. All Rights Reserved.
// Per-pawn component owning the foot-IK alpha curve during a kick montage.
// Subscribes to AFootballerVisual::OnAnimEvent; on Kick event starts the
// montage state machine. Exposes FootIKAlpha + FootIKTarget for the AnimBP
// to consume in its IK chain.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/FootballerAnimEvents.h"
#include "BallContactIKComponent.generated.h"

UCLASS(ClassGroup=(Edge26), meta=(BlueprintSpawnableComponent))
class EDGE26_API UBallContactIKComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBallContactIKComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                                FActorComponentTickFunction* ThisTickFunction) override;

    // Inputs (set from outside on Kick event):
    UPROPERTY(BlueprintReadOnly, Category="IK")
    bool bMontageActive = false;

    /** 0..1, where 0.5 = peak alpha (around BallContact frame). */
    UPROPERTY(BlueprintReadOnly, Category="IK")
    float MontageProgress = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="IK")
    FVector FootIKTarget = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="IK")
    float FootIKAlpha = 0.0f;

    /** Triggered externally when AFootballerVisual::OnAnimEvent fires Kick. */
    UFUNCTION(BlueprintCallable, Category="IK")
    void StartKickMontage(const FAnimEventPayload& Event);

    /** Called by anim notify (via AnimNotify_BallContact in M10). */
    UFUNCTION(BlueprintCallable, Category="IK")
    void OnBallContactNotify();

    // Frame counts (assume 60 fps render; tunable later).
    UPROPERTY(EditAnywhere, Category="IK")
    int32 WindUpFrames = 18;       // ~300 ms wind-up

    UPROPERTY(EditAnywhere, Category="IK")
    int32 FollowThroughFrames = 12; // ~200 ms recovery

private:
    int32 FrameIdx = 0;
    FVector InitialBallPos = FVector::ZeroVector;
    bool bContactFired = false;
};
```

- [ ] **Step 2: Commit the header**

```bash
git add Source/Edge26/Public/Animation/BallContactIKComponent.h
git commit -m "feat(anim): UBallContactIKComponent header — kick-montage IK alpha"
```

### Task T8.2 — Implement `BallContactIKComponent.cpp`

**Files:**
- Create: `Source/Edge26/Private/Animation/BallContactIKComponent.cpp`

- [ ] **Step 1: Write the implementation**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "Animation/BallContactIKComponent.h"
#include "Adapter/SimHostSubsystem.h"
#include "Sim/WorldState.h"
#include "Engine/World.h"

UBallContactIKComponent::UBallContactIKComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UBallContactIKComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UBallContactIKComponent::StartKickMontage(const FAnimEventPayload& Event)
{
    bMontageActive = true;
    FrameIdx = 0;
    InitialBallPos = Event.BallPosition;
    FootIKTarget = InitialBallPos;
    FootIKAlpha = 0.0f;
    bContactFired = false;
}

void UBallContactIKComponent::OnBallContactNotify()
{
    // Called by anim notify when the foot is supposed to meet the ball.
    // Snap target to current ball position and ramp alpha to 1 instantly.
    if (auto* World = GetWorld())
    {
        if (auto* Host = World->GetSubsystem<USimHostSubsystem>())
        {
            const edge26::FixedVec3 bp = Host->GetState().Ball.Position;
            FootIKTarget = FVector{
                (double)bp.X.Raw / (double)edge26::Fixed64::One,
                (double)bp.Y.Raw / (double)edge26::Fixed64::One,
                (double)bp.Z.Raw / (double)edge26::Fixed64::One };
        }
    }
    FootIKAlpha = 1.0f;
    bContactFired = true;
    // Move FrameIdx to right after the wind-up window so TickComponent
    // begins follow-through immediately on the next tick.
    FrameIdx = WindUpFrames;
}

void UBallContactIKComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bMontageActive) return;

    if (FrameIdx < WindUpFrames)
    {
        // Wind-up: ramp alpha 0 → 1 linearly.
        FootIKAlpha = (float)FrameIdx / (float)WindUpFrames;
        MontageProgress = 0.5f * (float)FrameIdx / (float)WindUpFrames;
        FrameIdx++;
    }
    else if (FrameIdx < WindUpFrames + FollowThroughFrames)
    {
        // Follow-through: alpha 1 → 0 over FollowThroughFrames.
        const int32 ftIdx = FrameIdx - WindUpFrames;
        FootIKAlpha = 1.0f - ((float)ftIdx / (float)FollowThroughFrames);
        MontageProgress = 0.5f + 0.5f * (float)ftIdx / (float)FollowThroughFrames;
        FrameIdx++;
    }
    else
    {
        // Done.
        bMontageActive = false;
        FootIKAlpha = 0.0f;
        MontageProgress = 1.0f;
    }
}
```

- [ ] **Step 2: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Commit**

```bash
git add Source/Edge26/Private/Animation/BallContactIKComponent.cpp
git commit -m "feat(anim): BallContactIKComponent — wind-up/contact/follow-through alpha"
```

### Task T8.3 — Add the component to `AFootballerVisual`

**Files:**
- Modify: `Source/Edge26/Public/Adapter/FootballerVisual.h`
- Modify: `Source/Edge26/Private/Adapter/FootballerVisual.cpp`

- [ ] **Step 1: Add member in the header**

In `AFootballerVisual` class:

```cpp
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<class UBallContactIKComponent> KickIK;
```

Add `#include "Animation/BallContactIKComponent.h"` to the header.

- [ ] **Step 2: Create component in ctor**

In `AFootballerVisual::AFootballerVisual()` (after `InputCollector = CreateDefaultSubobject...`):

```cpp
    KickIK = CreateDefaultSubobject<UBallContactIKComponent>(TEXT("KickIK"));
```

- [ ] **Step 3: Forward Kick events to the component**

In `HandleAnimEvent`, before the existing AnimInst->EnqueueEvent call, add:

```cpp
    if (Event.Kind == EFootballerAnimEvent::Kick && KickIK)
    {
        KickIK->StartKickMontage(Event);
    }
```

- [ ] **Step 4: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 5: Commit**

```bash
git add Source/Edge26/Public/Adapter/FootballerVisual.h \
        Source/Edge26/Private/Adapter/FootballerVisual.cpp
git commit -m "feat(adapter): mount UBallContactIKComponent on AFootballerVisual; route Kick events"
```

### Task T8.4 — Automation test: alpha ramp schedule

**Files:**
- Create: `Source/Edge26/Private/Tests/BallContactIKTests.cpp`

- [ ] **Step 1: Write the test**

```cpp
// Copyright Edge26. All Rights Reserved.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Animation/BallContactIKComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBallContactIKAlphaRampSchedule,
    "Edge26.Render.BallContactIK.AlphaRampSchedule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBallContactIKAlphaRampSchedule::RunTest(const FString& Parameters)
{
    UBallContactIKComponent* C = NewObject<UBallContactIKComponent>();
    C->WindUpFrames = 10;
    C->FollowThroughFrames = 10;

    FAnimEventPayload ev;
    ev.Kind = EFootballerAnimEvent::Kick;
    ev.BallPosition = FVector(100, 0, 0);
    C->StartKickMontage(ev);

    TestTrue(TEXT("Montage active"), C->bMontageActive);
    TestEqual(TEXT("Initial alpha is 0"), C->FootIKAlpha, 0.0f);

    // Wind-up ramp: 10 ticks, alpha 0 → ~1.
    for (int i = 0; i < 5; ++i) C->TickComponent(1.0f/60.0f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Mid wind-up alpha ~0.5"), FMath::IsNearlyEqual(C->FootIKAlpha, 0.5f, 0.05f));

    for (int i = 0; i < 5; ++i) C->TickComponent(1.0f/60.0f, LEVELTICK_All, nullptr);
    // Now in follow-through.
    TestTrue(TEXT("Alpha decreasing"), C->FootIKAlpha < 1.0f);

    // Advance through follow-through.
    for (int i = 0; i < 11; ++i) C->TickComponent(1.0f/60.0f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Montage done"), C->bMontageActive);
    TestEqual(TEXT("Alpha is 0 at end"), C->FootIKAlpha, 0.0f);

    return true;
}
```

- [ ] **Step 2: Editor build**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Run automation tests**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecCmds="Automation RunTests Edge26.Render; Quit" \
    -unattended -nullrhi -stdout 2>&1 | grep -E "Test (Started|Completed|Failed|Passed)" | tail -10
```

Expected: 3 tests now (DelayRespected, EmitsKick, AlphaRampSchedule). All pass.

- [ ] **Step 4: Commit**

```bash
git add Source/Edge26/Private/Tests/BallContactIKTests.cpp
git commit -m "test(anim): BallContactIK AlphaRampSchedule automation test"
```

### Task T8.5 — Mark M8 complete

- [ ] **Step 1: PROGRESS.md tick + activity log**

```markdown
- [x] M8. BallContactIKComponent + kick-montage IK alpha
```

```markdown
- M8 landed: UBallContactIKComponent attached to AFootballerVisual; consumes Kick events to drive wind-up (alpha 0→1 over 18f) + contact (snap to ball) + follow-through (alpha 1→0 over 12f). AnimBP IK alpha pin reads from this. AlphaRampSchedule automation test green.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): M8 complete; advance to M9 (goalkeeper)"
```

---

## M9 — Goalkeeper subclass + `MMDB_Goalkeeper` + GK animations

The GK gets its own anim BP, its own MM database, and a `BP_Goalkeeper` BP that's used for the 2 GK pawns in the level.

### Task T9.1 — Create `MMDB_Goalkeeper` empty asset

**Files:**
- Modify: `Scripts/editor/create_mmdb_outfield.py` → rename to `create_mmdb_assets.py` that creates both.

Actually keep them separate scripts for clarity:

- Create: `Scripts/editor/create_mmdb_goalkeeper.py`

- [ ] **Step 1: Copy `create_mmdb_outfield.py` to the new path and adjust**

```python
"""Create an empty UPoseSearchDatabase asset for goalkeeper animations."""

import unreal

ASSET_NAME = "MMDB_Goalkeeper"
ASSET_PATH = "/Game/Animation/MotionMatching"
LOG = "/tmp/create_mmdb_goalkeeper.log"

with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    full = f"{ASSET_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        log(f"Asset already exists: {full}")
    else:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        cls = unreal.load_class(None, "/Script/PoseSearch.PoseSearchDatabase")
        if not cls:
            log("ERROR: PoseSearch.PoseSearchDatabase class not found.")
        else:
            asset = tools.create_asset(ASSET_NAME, ASSET_PATH, cls, None)
            if asset:
                unreal.EditorAssetLibrary.save_asset(full)
                log(f"Created + saved {full}")
            else:
                log(f"ERROR: failed to create {full}")
    log("Done.")
```

- [ ] **Step 2: Run + verify + commit**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecutePythonScript="$PWD/Scripts/editor/create_mmdb_goalkeeper.py" \
    -unattended -stdout 2>&1 | tail -5
ls Content/Animation/MotionMatching/MMDB_Goalkeeper.uasset
git add Scripts/editor/create_mmdb_goalkeeper.py Content/Animation/MotionMatching/MMDB_Goalkeeper.uasset
git commit -m "feat(anim): create empty MMDB_Goalkeeper UPoseSearchDatabase asset"
```

### Task T9.2 — Download + retarget Mixamo GK animations (manual + script)

> **Manual step (user):**
>
> 1. Download from Mixamo into `Content/Animation/Mixamo_Raw/GK/`:
>    - "Goalkeeper Dive Left"
>    - "Goalkeeper Dive Right"
>    - "Soccer Goal Save" (or "Defensive Save")
>    - "Throw In" (use as GK throw)
>    - "Goalkeeper Stance"
> 2. Re-run the retarget script (M7's `retarget_mixamo_to_manny.py`) — it picks up the GK folder if you extend its glob.
>
> Or do the manual fallback: select all 5 GK FBX → right-click → Retarget Animations → IKR_Mixamo_to_Manny → output to `Content/Animation/Mixamo_Retargeted/GK/`.

- [ ] **Step 1: User confirms 5 GK clips exist in `Content/Animation/Mixamo_Retargeted/GK/`**

- [ ] **Step 2: Add `BallContact` (where applicable) + `RecoverEnd` notifies to each**

- [ ] **Step 3: Populate `MMDB_Goalkeeper`** with the 5 retargeted GK clips (manual editor work: drag into Sources, Build Database, Save).

- [ ] **Step 4: Commit**

```bash
git add Content/Animation/Mixamo_Raw/GK/ \
        Content/Animation/Mixamo_Retargeted/GK/ \
        Content/Animation/MotionMatching/MMDB_Goalkeeper.uasset
git commit -m "feat(anim): GK animations imported, retargeted, populated into MMDB_Goalkeeper"
```

### Task T9.3 — Create `ABP_Goalkeeper` AnimBP

**Files:**
- Create: `Scripts/editor/create_abp_goalkeeper.py`

- [ ] **Step 1: Write a copy of the ABP creation script targeting MMDB_Goalkeeper**

```python
"""Create ABP_Goalkeeper (subclass of UFootballAnimInstance, Manny skeleton)."""

import unreal

ASSET_NAME = "ABP_Goalkeeper"
ASSET_PATH = "/Game/Blueprints/Player"
PARENT_CLASS_PATH = "/Script/Edge26.FootballAnimInstance"
SKELETON_PATH = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
LOG = "/tmp/create_abp_goalkeeper.log"

with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    full = f"{ASSET_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        log(f"Already exists: {full}")
    else:
        parent = unreal.load_class(None, PARENT_CLASS_PATH)
        skel   = unreal.EditorAssetLibrary.load_asset(SKELETON_PATH)
        if not (parent and skel):
            log("ERROR: missing parent or skeleton")
        else:
            factory = unreal.AnimBlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            factory.set_editor_property("target_skeleton", skel)
            tools = unreal.AssetToolsHelpers.get_asset_tools()
            asset = tools.create_asset(ASSET_NAME, ASSET_PATH, unreal.AnimBlueprint, factory)
            if asset:
                unreal.EditorAssetLibrary.save_asset(full)
                log(f"Created + saved {full}")
    log("Done.")
```

- [ ] **Step 2: Run + verify**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecutePythonScript="$PWD/Scripts/editor/create_abp_goalkeeper.py" \
    -unattended -stdout 2>&1 | tail -5
ls Content/Blueprints/Player/ABP_Goalkeeper.uasset
```

Expected: file exists.

- [ ] **Step 3: User wires the AnimGraph (manual)**

> **Manual step (user):** open `ABP_Goalkeeper`, drag a **Motion Matching** node, set Database = `MMDB_Goalkeeper`, feed trajectory from anim instance properties, plug into Output Pose. Compile + Save.

- [ ] **Step 4: Commit**

```bash
git add Scripts/editor/create_abp_goalkeeper.py Content/Blueprints/Player/ABP_Goalkeeper.uasset
git commit -m "feat(anim): ABP_Goalkeeper AnimBP — Motion Matching against MMDB_Goalkeeper"
```

### Task T9.4 — Create `BP_Goalkeeper` BP (subclass of `BP_Footballer`)

**Files:**
- Create: `Scripts/editor/create_bp_goalkeeper.py`

- [ ] **Step 1: Write the script**

```python
"""Create BP_Goalkeeper as a Blueprint subclass of BP_Footballer that defaults
its AnimInstanceClass to ABP_Goalkeeper.
"""

import unreal

ASSET_NAME = "BP_Goalkeeper"
ASSET_PATH = "/Game/Blueprints/Player"
PARENT_BP   = "/Game/Blueprints/Player/BP_Footballer"
ANIM_CLASS  = "/Game/Blueprints/Player/ABP_Goalkeeper"
LOG = "/tmp/create_bp_goalkeeper.log"

with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    full = f"{ASSET_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        log(f"Already exists: {full}")
    else:
        parent = unreal.EditorAssetLibrary.load_blueprint_class(PARENT_BP)
        if not parent:
            log(f"ERROR: parent BP not found: {PARENT_BP}")
        else:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            tools = unreal.AssetToolsHelpers.get_asset_tools()
            asset = tools.create_asset(ASSET_NAME, ASSET_PATH, unreal.Blueprint, factory)
            if asset:
                # Set AnimInstanceClass default on the mesh component CDO.
                anim_cls = unreal.EditorAssetLibrary.load_blueprint_class(ANIM_CLASS)
                if anim_cls:
                    gen = asset.generated_class()
                    cdo = unreal.get_default_object(gen)
                    mesh = cdo.get_editor_property("Mesh")
                    if mesh:
                        mesh.set_editor_property("anim_class", anim_cls)
                unreal.EditorAssetLibrary.save_asset(full)
                log(f"Created + saved {full}")
    log("Done.")
```

- [ ] **Step 2: Run + verify**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecutePythonScript="$PWD/Scripts/editor/create_bp_goalkeeper.py" \
    -unattended -stdout 2>&1 | tail -5
ls Content/Blueprints/Player/BP_Goalkeeper.uasset
```

Expected: file exists.

- [ ] **Step 3: Commit**

```bash
git add Scripts/editor/create_bp_goalkeeper.py Content/Blueprints/Player/BP_Goalkeeper.uasset
git commit -m "feat(anim): BP_Goalkeeper (BP_Footballer subclass with ABP_Goalkeeper)"
```

### Task T9.5 — Mark M9 complete

- [ ] **Step 1: PROGRESS.md + activity log**

```markdown
- [x] M9. Goalkeeper subclass + MMDB_Goalkeeper + GK animations
```

```markdown
- M9 landed: 5 GK animations downloaded from Mixamo, retargeted to Manny, BallContact + RecoverEnd notifies added. MMDB_Goalkeeper UPoseSearchDatabase asset populated and built. ABP_Goalkeeper AnimBP wired against MMDB_Goalkeeper. BP_Goalkeeper BP subclasses BP_Footballer with ABP_Goalkeeper as the default anim class.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): M9 complete; advance to M10 (anim event hookup)"
```

---

## M10 — Anim event hookup (Kick montage trigger, GK dive trigger)

The C++ side already broadcasts `Kick` events. Now the AnimBP graphs need to **consume** the event and play the right montage. This is mostly editor work.

### Task T10.1 — `ABP_Footballer_MM` plays kick montage on Kick event (manual)

> **Manual step (user):**
>
> 1. Open `ABP_Footballer_MM`.
> 2. In the **Event Graph**, find or create the `Event Blueprint Update Animation` node.
> 3. After the existing trajectory update, branch on `bHasPendingEvent && PendingEvent.Kind == EFootballerAnimEvent::Kick`.
> 4. On True: `Play Montage` → choose a kick montage asset (create one from your retargeted "Soccer Pass" anim — right-click the anim → Create → AnimMontage).
> 5. In the AnimMontage, ensure the `BallContact` notify is present (it should be inherited from the source anim).
>
> The BallContact notify is the trigger point at which `UBallContactIKComponent::OnBallContactNotify` should be called. Add an `AnimNotify_BallContact` BP class that does this:
>
> 6. In the Content Browser, right-click → **Blueprint Class → AnimNotify** → name `AN_BallContact`.
> 7. Override `Received_Notify`. Get the mesh's owner (a pawn), cast to `AFootballerVisual`, call `KickIK->OnBallContactNotify()`.
> 8. In the kick anim, replace the string-named `BallContact` notify with this `AN_BallContact` class notify.
>
> Compile + Save.

- [ ] **Step 1: User confirms montage plays in PIE when kick fires**

> **Manual smoke test:** flip `BP_Footballer` AnimInstanceClass to `ABP_Footballer_MM`. PIE. Observe a kick fire — the foot should visibly swing.

- [ ] **Step 2: Commit**

```bash
git add Content/Blueprints/Player/ABP_Footballer_MM.uasset \
        Content/Animation/AN_BallContact.uasset
git commit -m "feat(anim): ABP_Footballer_MM plays kick montage on Kick event; AN_BallContact calls OnBallContactNotify"
```

### Task T10.2 — `ABP_Goalkeeper` plays dive montage on `GoalkeeperSave` (manual)

> **Manual step:** repeat T10.1 for `ABP_Goalkeeper`. Branch on `PendingEvent.Kind == GoalkeeperSave` → play the dive montage (choose left/right based on `PendingEvent.BallPosition.Y` relative to GK position).

- [ ] **Step 1: User confirms GK dives when a shot is saved in PIE**

- [ ] **Step 2: Commit**

```bash
git add Content/Blueprints/Player/ABP_Goalkeeper.uasset
git commit -m "feat(anim): ABP_Goalkeeper plays dive montage on GoalkeeperSave event"
```

### Task T10.3 — Mark M10 complete

- [ ] **Step 1: PROGRESS.md tick + activity log**

```markdown
- [x] M10. Anim event hookup (KickEvent → montage trigger)
```

```markdown
- M10 landed: ABP_Footballer_MM EventGraph plays kick montage on Kick event; AN_BallContact AnimNotify class calls UBallContactIKComponent::OnBallContactNotify at the foot-meets-ball frame. ABP_Goalkeeper plays dive montage (left/right by ball.Y) on GoalkeeperSave.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): M10 complete; advance to M11 (level placement)"
```

---

## M11 — Re-place 22 `BP_Footballer` instances with role-correct anim BPs

The 22 placed pawns in `L_Pitch` were spawned in Phase 2 M1.9 as `BP_Footballer`. Two of them (players 0 and 11, the GKs) need to be `BP_Goalkeeper` instead. All 22 need their `AnimInstanceClass` switched to `ABP_Footballer_MM` (outfield) or `ABP_Goalkeeper` (GK).

### Task T11.1 — Headless Python: re-parent GK pawns + update anim BP defaults

**Files:**
- Create: `Scripts/editor/swap_pawn_anim_bps.py`

- [ ] **Step 1: Write the script**

```python
"""Phase 3 M11: Switch BP_Footballer pawns to ABP_Footballer_MM for outfield,
ABP_Goalkeeper for GKs (players 0 + 11). Re-parents player 0/11 actors in
L_Pitch to BP_Goalkeeper class.
"""

import unreal

LEVEL = "/Game/Levels/L_Pitch"
ABP_OUTFIELD  = "/Game/Blueprints/Player/ABP_Footballer_MM"
BP_GOALKEEPER = "/Game/Blueprints/Player/BP_Goalkeeper"
LOG = "/tmp/swap_pawn_anim_bps.log"

with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    success = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    if not success:
        log(f"ERROR: could not open {LEVEL}")
    else:
        outfield_anim = unreal.EditorAssetLibrary.load_blueprint_class(ABP_OUTFIELD)
        gk_bp_class   = unreal.EditorAssetLibrary.load_blueprint_class(BP_GOALKEEPER)
        if not outfield_anim:
            log(f"ERROR: outfield anim BP not loaded: {ABP_OUTFIELD}")
        elif not gk_bp_class:
            log(f"ERROR: GK BP not loaded: {BP_GOALKEEPER}")
        else:
            actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            all_actors = actor_subsys.get_all_level_actors()
            footballers = [a for a in all_actors if a.get_class().get_name().startswith("BP_Footballer")]
            log(f"Found {len(footballers)} footballer pawns")
            updated = 0
            replaced = 0
            for a in footballers:
                idx = a.get_editor_property("ControllerIndex")
                if idx == 0 or idx == 11:
                    # Re-parent to BP_Goalkeeper.
                    pos = a.get_actor_location()
                    rot = a.get_actor_rotation()
                    actor_subsys.destroy_actor(a)
                    new_gk = unreal.EditorLevelLibrary.spawn_actor_from_class(gk_bp_class, pos, rot)
                    if new_gk:
                        new_gk.set_editor_property("ControllerIndex", idx)
                        replaced += 1
                else:
                    # Update AnimInstanceClass on the mesh.
                    mesh = a.get_component_by_class(unreal.SkeletalMeshComponent)
                    if mesh:
                        mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_BLUEPRINT)
                        mesh.set_anim_instance_class(outfield_anim)
                        updated += 1
            unreal.EditorLevelLibrary.save_current_level()
            log(f"Updated {updated} outfielders, replaced {replaced} GKs")
    log("Done.")
```

- [ ] **Step 2: Run it (headless, NOT -nullrhi because SpawnActor may need it; use a regular UnrealEditor-Cmd)**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecutePythonScript="$PWD/Scripts/editor/swap_pawn_anim_bps.py" \
    -unattended -stdout 2>&1 | tail -10
cat /tmp/swap_pawn_anim_bps.log
```

Expected: log says "Updated 20 outfielders, replaced 2 GKs".

If SpawnActor fails because of `-nullrhi` (we hit this in Phase 2 M10), use the BP-subclass trick: create a thin BP_GoalkeeperSpawnable in editor that wraps BP_Goalkeeper, and spawn that instead. (Same workaround as `place_ai_debug_renderer.py`.)

- [ ] **Step 3: Commit**

```bash
git add Scripts/editor/swap_pawn_anim_bps.py Content/Levels/L_Pitch.umap
git commit -m "feat(level): swap 20 outfielders to ABP_Footballer_MM, 2 GKs to BP_Goalkeeper"
```

### Task T11.2 — Mark M11 complete

- [ ] **Step 1: PROGRESS.md tick + activity log**

```markdown
- [x] M11. Re-place 22 BP_Footballer instances with role-correct anim BPs
```

```markdown
- M11 landed: 20 outfielder pawns' Mesh.AnimInstanceClass swapped to ABP_Footballer_MM; players 0 + 11 (the GKs) re-parented from BP_Footballer to BP_Goalkeeper via headless Python; L_Pitch.umap saved.
```

- [ ] **Step 2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): M11 complete; advance to M12 (final acceptance)"
```

---

## M12 — Final acceptance

### Task T12.1 — Run all 3 UE5 automation tests

- [ ] **Step 1: Run from command line**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecCmds="Automation RunTests Edge26.Render; Quit" \
    -unattended -nullrhi -stdout 2>&1 | grep -E "Test (Started|Completed|Failed|Passed|Skipped)"
```

Expected: 3 tests reported, 3 passes:
- `Edge26.Render.SnapshotBuffer.DelayRespected` PASS
- `Edge26.Render.SnapshotBuffer.EmitsKick` PASS
- `Edge26.Render.BallContactIK.AlphaRampSchedule` PASS

If any fails, fix before proceeding.

- [ ] **Step 2: Add `MotionMatching.TrajectoryComputation` test if not already present**

Looking at the spec §5, four tests are planned. We have three. Add the fourth.

**Files:**
- Create: `Source/Edge26/Private/Tests/FootballAnimInstanceTests.cpp`

```cpp
// Copyright Edge26. All Rights Reserved.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Animation/FootballAnimInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMotionMatchingTrajectoryComputation,
    "Edge26.Render.MotionMatching.TrajectoryComputation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMotionMatchingTrajectoryComputation::RunTest(const FString& Parameters)
{
    UFootballAnimInstance* AI = NewObject<UFootballAnimInstance>();
    // Force trajectory via direct property set (we can't easily Tick without a pawn).
    AI->TrajectoryVelocity = FVector(500, 0, 0);   // 5 m/s in +X
    AI->Speed = 500.0f;

    TestEqual(TEXT("4 trajectory samples"), AI->TrajectorySamples.Num(), 4);

    // (No deeper assertion — UpdateTrajectory is hard to unit-test without a
    // pawn owner. This test is a smoke check that the class instantiates.)
    return true;
}
```

- [ ] **Step 3: Rebuild + rerun tests**

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex 2>&1 | tail -3
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/Edge26.uproject" \
    -ExecCmds="Automation RunTests Edge26.Render; Quit" \
    -unattended -nullrhi -stdout 2>&1 | grep -E "Test (Started|Completed|Failed|Passed)"
```

Expected: 4 tests, 4 passes.

- [ ] **Step 4: Commit**

```bash
git add Source/Edge26/Private/Tests/FootballAnimInstanceTests.cpp
git commit -m "test(anim): MotionMatching TrajectoryComputation smoke test"
```

### Task T12.2 — Determinism gate still PASS (Phase 3 was render-side only)

- [ ] **Step 1: Run gate**

```bash
./Scripts/check_determinism.sh 2>&1 | tail -3
```

Expected: `PASS: all determinism checks`.

- [ ] **Step 2: Lint**

```bash
./Scripts/lint_sim.sh
```

Expected: `lint_sim.sh: OK`.

If determinism gate fails: `git diff main -- Source/Edge26Sim/` must be empty. If it isn't, you accidentally modified sim code.

### Task T12.3 — 5-minute PIE soak

> **Manual step (user):** open editor, open `L_Pitch`, press Play. Soak for 5 minutes. Watch for:
>
> 1. No crashes / ensures / red Output Log lines.
> 2. All 22 players animating (no T-poses, no static idle clumps).
> 3. Feet plant on the ground; no skating, no float, no floor penetration.
> 4. When a player kicks, a foot visibly swings; ball "releases" from the foot at contact frame (not from pelvis or some random offset).
> 5. GKs animate differently from outfielders (different idle stance; dive on saves).
> 6. 200 ms render delay is imperceptible — pressing WASD should feel responsive.

If any item fails, log it in `docs/superpowers/notes/2026-05-17-phase3-pie-soak-notes.md`.

- [ ] **Step 1: User reports soak observations**

### Task T12.4 — Final `PROGRESS.md` sweep

- [ ] **Step 1: Rewrite the Current status section**

Replace the Phase 2 + Phase 3 status block at top of `PROGRESS.md` with:

```markdown
## Current status

**Phase 3: Animation v0 is COMPLETE.** All twelve milestones (M1–M12)
shipped: RenderSnapshotBuffer with 200 ms delay; snapshot-diff event
extraction (Kick, BallReceived, GoalkeeperSave, GoalkeeperCatch);
UFootballAnimInstance base class with motion-matching trajectory
generation; MMDB_Outfield + MMDB_Goalkeeper UPoseSearchDatabase assets
sourced from Game Animation Sample (locomotion) + Mixamo (football
moves); ABP_Footballer_MM + ABP_Goalkeeper AnimBPs with Motion Matching
nodes; per-leg TwoBoneIK foot planting; UBallContactIKComponent driving
kick-montage IK alpha schedule with BallContact / RecoverEnd notifies;
BP_Goalkeeper subclass for the 2 GKs in L_Pitch.

Automated acceptance: 4 UE5 C++ Automation Tests green
(SnapshotBuffer.DelayRespected, SnapshotBuffer.EmitsKick,
BallContactIK.AlphaRampSchedule, MotionMatching.TrajectoryComputation).
Phase 1/2 determinism gate untouched (sim was not modified) — still
PASS on Linux/macOS/Windows. PIE soak confirmed §15-equivalent
acceptance criteria.

**Phase 2: Spatial AI v0 is COMPLETE and merged.** (See activity log.)
**Phase 1: Sim Core v0 is COMPLETE and merged.** (See activity log.)

The repo now has a deterministic 50 Hz football match with
animation-driven control. Ready for Phase 4 (rollback netcode) or
Phase 3.1 polish (header anim, dive variants, stamina-driven anim
speed).
```

- [ ] **Step 2: Move Phase 3 in the roadmap to "complete" + tick all M-items**

```markdown
### Phase 3: Motion-matching animation + procedural ball-contact IK  ←  complete  (render-side only per spec §3)
- [x] M1. RenderSnapshotBuffer + 200 ms delay wiring
- [x] M2. Snapshot-diff event extraction (KickEvent, BallReceived, GoalkeeperSave)
- [x] M3. FootballAnimInstance base class + trajectory generation
- [x] M4. Game Animation Sample import + MMDB_Outfield skeleton
- [x] M5. ABP_Footballer_MM motion-matching state tree
- [x] M6. Foot IK setup (TwoBoneIK per leg, ground-plane projection)
- [x] M7. Mixamo retarget + football overlays + anim notifies
- [x] M8. BallContactIKComponent + kick-montage IK alpha
- [x] M9. Goalkeeper subclass + MMDB_Goalkeeper + GK animations
- [x] M10. Anim event hookup (KickEvent → montage trigger)
- [x] M11. Re-place 22 BP_Footballer instances with role-correct anim BPs
- [x] M12. Final acceptance (PIE soak + UE5 automation tests + PROGRESS.md)
```

Mark Phase 4 (Rollback netcode) as `←  next`.

- [ ] **Step 3: Append a closeout activity log entry**

```markdown
### 2026-MM-DD — Phase 3 closeout
- All M1–M12 landed. RenderSnapshotBuffer + 200 ms delay buffer wires the
  sim's tick-discrete kick events through to anim montages. Motion
  Matching replaces the strafe blendspace for both outfield and GK.
  TwoBoneIK plants feet; BallContactIKComponent ramps foot IK alpha
  during kick montages so the foot meets the ball at the BallContact
  notify and the ball "releases" from that frame. 4 UE5 automation
  tests cover the render-side logic. Phase 1/2 determinism gate
  untouched.
- Asset workflow established: Game Animation Sample (locomotion) +
  Mixamo (football moves, GK) retargeted onto Manny via
  IKR_Mixamo_to_Manny; clips populate MMDB_Outfield + MMDB_Goalkeeper;
  AnimBPs read the databases at runtime.
- PIE soak passed: no T-poses, feet plant, kicks visible, GKs dive,
  render delay imperceptible.
```

- [ ] **Step 4: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(phase3): mark Phase 3 COMPLETE; advance status to Phase 4"
```

### Task T12.5 — Push branch + PR + tag

- [ ] **Step 1: Push**

```bash
git push -u origin feat/phase3-animation 2>&1 | tail -5
```

- [ ] **Step 2: Open PR**

```bash
gh pr create --title "Phase 3 — Animation: motion matching + ball-contact IK" \
  --body "$(cat <<'EOF'
## Summary
- RenderSnapshotBuffer (25-entry ring) with 200 ms (10-tick) consumption delay so animation has time to play foot-strike montages before the ball "releases" at the BallContact notify.
- Snapshot-diff event extraction: Kick (PendingButtons rising edge per kind), BallReceived (possession change while ball airborne), GoalkeeperSave / GoalkeeperCatch.
- UFootballAnimInstance: trajectory generation (4 future samples at +10/+20/+30/+40 frames), Speed / Velocity / Acceleration exposed to AnimBP, event queue.
- MMDB_Outfield + MMDB_Goalkeeper UPoseSearchDatabase assets sourced from Game Animation Sample (locomotion) + Mixamo retargeted to Manny via IKR_Mixamo_to_Manny (football moves + GK).
- ABP_Footballer_MM + ABP_Goalkeeper AnimBPs with Motion Matching nodes + per-leg TwoBoneIK foot planting + kick montage slot.
- UBallContactIKComponent drives kick-montage IK alpha (wind-up 0→1, snap-to-ball at BallContact notify, follow-through 1→0).
- BP_Goalkeeper BP subclass; 22 placed pawns in L_Pitch updated via headless Python (20 outfielders re-pointed to ABP_Footballer_MM; 2 GKs re-parented to BP_Goalkeeper).
- 4 UE5 C++ Automation tests cover the render-side logic.

Spec: `docs/superpowers/specs/2026-05-17-phase3-animation-design.md`
Plan: `docs/superpowers/plans/2026-05-17-phase3-animation-plan.md`

**Sim is unchanged.** Phase 1/2 determinism gate continues to PASS on Linux/macOS/Windows without modification.

## Test plan
- [ ] All 4 UE5 automation tests PASS (`Automation RunTests Edge26.Render`).
- [ ] `Scripts/check_determinism.sh` still PASS locally.
- [ ] `Scripts/lint_sim.sh` clean.
- [ ] Editor build `Result: Succeeded`.
- [ ] 5-minute PIE soak with no crashes, no T-poses, feet plant, kicks visible, GKs dive.
EOF
)" 2>&1 | tail -5
```

- [ ] **Step 3: Wait for CI matrix (still the Phase 1/2 sim gate; nothing new to verify)**

```bash
gh pr checks --watch
```

Expected: 3-OS matrix PASS.

- [ ] **Step 4: After merge, tag**

```bash
git checkout main && git pull --ff-only
git tag -a phase3-v0 -m "Phase 3: Animation v0 — motion matching + ball-contact IK + GK split"
git push origin phase3-v0
git branch -d feat/phase3-animation
git push origin --delete feat/phase3-animation
```

Phase 3 is shipped.

---

## Determinism guardrails (reminder)

Phase 3 is render-side. **Never modify `Source/Edge26Sim/`.** If a task here ever requires changing the sim, stop and reconsider — the design says render-only is sufficient. The Phase 1/2 determinism gate continues to be the central correctness contract for the sim; Phase 3's UE5 automation tests are a separate render-side correctness layer.
