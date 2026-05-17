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
