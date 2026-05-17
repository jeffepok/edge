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
