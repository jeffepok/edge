// Copyright Edge26. All Rights Reserved.
#include "Animation/FootballAnimInstance.h"
#include "Adapter/FootballerVisual.h"
#include "Animation/AnimMontage.h"
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

    // M10: drain the pending anim event by playing the corresponding montage.
    // The C++ "controller pattern": this base class decides which montage maps
    // to which event so the AnimBPs only need the montage assets assigned on
    // the CDO — no event-graph wiring or anim-state-machine notify routing.
    //
    // Outfield AnimBP assigns KickMontage; GK AnimBP assigns GoalkeeperSaveMontage.
    // The opposite slot stays null and is just ignored.
    //
    // We guard with Montage_IsPlaying so back-to-back same-kind events don't
    // restart the clip mid-windup (preserves visual continuity). A future
    // milestone may want to interrupt-and-restart for chained shots.
    if (bHasPendingEvent)
    {
        UAnimMontage* MontageToPlay = nullptr;
        switch (PendingEvent.Kind)
        {
            case EFootballerAnimEvent::Kick:
                MontageToPlay = KickMontage;
                break;
            case EFootballerAnimEvent::GoalkeeperSave:
                MontageToPlay = GoalkeeperSaveMontage;
                break;
            default:
                break;
        }
        if (MontageToPlay && !Montage_IsPlaying(MontageToPlay))
        {
            Montage_Play(MontageToPlay, 1.0f);
        }
    }

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
