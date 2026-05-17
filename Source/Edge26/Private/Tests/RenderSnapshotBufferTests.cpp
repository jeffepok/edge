// Copyright Edge26. All Rights Reserved.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Adapter/RenderSnapshotBuffer.h"
#include "Sim/WorldState.h"
#include <cstring>

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
