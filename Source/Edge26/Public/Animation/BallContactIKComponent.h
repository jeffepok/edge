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
