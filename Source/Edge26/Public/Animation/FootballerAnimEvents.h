// Copyright Edge26. All Rights Reserved.
// Render-side animation events emitted by RenderSnapshotBuffer when sim state
// changes tick-to-tick. Render code consumes these to trigger anim montages,
// IK ramps, SFX, etc. Sim code never sees them.
#pragma once

#include "CoreMinimal.h"
#include "FootballerAnimEvents.generated.h"

UENUM(BlueprintType)
enum class EFootballerAnimEvent : uint8
{
    None              = 0,
    Kick              = 1,   // PendingButtons & (Pass|Shoot|Chip) rising edge
    BallReceived      = 2,   // PossessionPlayer changed AND ball was airborne
    GoalkeeperSave    = 3,   // ball.Velocity went to 0 AND PossessionPlayer is a GK
    GoalkeeperCatch   = 4,   // PossessionPlayer is GK and prev wasn't (non-save)
    // Tackle, Header, GoalkeeperThrow reserved for v1
};

UENUM(BlueprintType)
enum class EKickKind : uint8
{
    None  = 0,
    Pass  = 1,
    Shoot = 2,
    Chip  = 3,
};

USTRUCT(BlueprintType)
struct EDGE26_API FAnimEventPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EFootballerAnimEvent Kind = EFootballerAnimEvent::None;

    UPROPERTY(BlueprintReadOnly)
    int32 PlayerIndex = -1;

    UPROPERTY(BlueprintReadOnly)
    EKickKind KickKind = EKickKind::None;

    // World-space direction the kick is aimed (unit-length, Z=0 except for chip lift).
    UPROPERTY(BlueprintReadOnly)
    FVector KickDirection = FVector::ZeroVector;

    // Initial ball world position at the moment of the event (useful for IK).
    UPROPERTY(BlueprintReadOnly)
    FVector BallPosition = FVector::ZeroVector;

    // World position of the target receiver (or save spot, or zero).
    UPROPERTY(BlueprintReadOnly)
    FVector TargetPosition = FVector::ZeroVector;
};
