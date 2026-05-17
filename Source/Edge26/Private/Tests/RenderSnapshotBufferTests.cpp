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
