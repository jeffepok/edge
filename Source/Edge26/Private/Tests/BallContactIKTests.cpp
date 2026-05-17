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
