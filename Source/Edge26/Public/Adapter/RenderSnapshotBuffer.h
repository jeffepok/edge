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
