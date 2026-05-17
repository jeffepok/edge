// Copyright Edge26. All Rights Reserved.
#include "Animation/BallContactIKComponent.h"
#include "Adapter/SimHostSubsystem.h"
#include "Sim/WorldState.h"
#include "Engine/World.h"

UBallContactIKComponent::UBallContactIKComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UBallContactIKComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UBallContactIKComponent::StartKickMontage(const FAnimEventPayload& Event)
{
    bMontageActive = true;
    FrameIdx = 0;
    InitialBallPos = Event.BallPosition;
    FootIKTarget = InitialBallPos;
    FootIKAlpha = 0.0f;
    bContactFired = false;
}

void UBallContactIKComponent::OnBallContactNotify()
{
    // Called by anim notify when the foot is supposed to meet the ball.
    // Snap target to current ball position and ramp alpha to 1 instantly.
    if (auto* World = GetWorld())
    {
        if (auto* Host = World->GetSubsystem<USimHostSubsystem>())
        {
            const edge26::FixedVec3 bp = Host->GetState().Ball.Position;
            FootIKTarget = FVector{
                (double)bp.X.Raw / (double)edge26::Fixed64::One,
                (double)bp.Y.Raw / (double)edge26::Fixed64::One,
                (double)bp.Z.Raw / (double)edge26::Fixed64::One };
        }
    }
    FootIKAlpha = 1.0f;
    bContactFired = true;
    // Move FrameIdx to right after the wind-up window so TickComponent
    // begins follow-through immediately on the next tick.
    FrameIdx = WindUpFrames;
}

void UBallContactIKComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bMontageActive) return;

    if (FrameIdx < WindUpFrames)
    {
        // Wind-up: ramp alpha 0 → 1 linearly.
        FootIKAlpha = (float)FrameIdx / (float)WindUpFrames;
        MontageProgress = 0.5f * (float)FrameIdx / (float)WindUpFrames;
        FrameIdx++;
    }
    else if (FrameIdx < WindUpFrames + FollowThroughFrames)
    {
        // Follow-through: alpha 1 → 0 over FollowThroughFrames.
        const int32 ftIdx = FrameIdx - WindUpFrames;
        FootIKAlpha = 1.0f - ((float)ftIdx / (float)FollowThroughFrames);
        MontageProgress = 0.5f + 0.5f * (float)ftIdx / (float)FollowThroughFrames;
        FrameIdx++;
    }
    else
    {
        // Done.
        bMontageActive = false;
        FootIKAlpha = 0.0f;
        MontageProgress = 1.0f;
    }
}
