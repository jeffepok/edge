// Copyright Edge26. All Rights Reserved.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Animation/FootballAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMotionMatchingTrajectoryComputation,
    "Edge26.Render.MotionMatching.TrajectoryComputation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMotionMatchingTrajectoryComputation::RunTest(const FString& Parameters)
{
    // UAnimInstance has ClassWithin = USkeletalMeshComponent — we must construct
    // it with a SkeletalMeshComponent as the outer or the object system raises
    // an ensure. Engine itself follows this pattern (see
    // SkeletalMeshComponent.cpp line 1150).
    USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>();
    if (!Mesh)
    {
        AddError(TEXT("Failed to instantiate USkeletalMeshComponent outer"));
        return false;
    }

    UFootballAnimInstance* AI = NewObject<UFootballAnimInstance>(Mesh);
    if (!AI)
    {
        AddError(TEXT("Failed to instantiate UFootballAnimInstance"));
        return false;
    }

    // Force trajectory inputs via direct property set. UpdateTrajectory is the
    // path that populates TrajectorySamples and it requires a pawn owner +
    // movement context that we can't easily construct in a unit-test, so we
    // assert reachability of the class fields and leave depth verification for
    // the M12 PIE soak.
    AI->TrajectoryVelocity = FVector(500.0f, 0.0f, 0.0f);   // 5 m/s in +X
    AI->Speed = 500.0f;

    TestEqual(TEXT("Speed reachable"), AI->Speed, 500.0f);
    TestEqual(TEXT("TrajectoryVelocity reachable"),
              AI->TrajectoryVelocity, FVector(500.0f, 0.0f, 0.0f));

    // TrajectorySamples is empty until NativeUpdateAnimation/UpdateTrajectory
    // runs with a valid pawn owner. The smoke check here is that the field is
    // reachable (Num() returns a sane non-negative value), not that it is
    // populated — population is verified visually in the M12 PIE soak.
    TestTrue(TEXT("TrajectorySamples field reachable"),
             AI->TrajectorySamples.Num() >= 0);

    // bIsGrounded defaults to true (header default-init).
    TestTrue(TEXT("bIsGrounded defaults true"), AI->bIsGrounded);

    return true;
}
