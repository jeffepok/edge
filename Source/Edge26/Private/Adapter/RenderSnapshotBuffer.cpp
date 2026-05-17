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
