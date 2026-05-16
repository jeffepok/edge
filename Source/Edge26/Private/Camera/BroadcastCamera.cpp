// Copyright Edge26. All Rights Reserved.

#include "Camera/BroadcastCamera.h"

#include "Adapter/SoccerBallVisual.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Edge26.h"

ABroadcastCamera::ABroadcastCamera()
{
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	SetRootComponent(Camera);
	Camera->FieldOfView = FieldOfView;
}

void ABroadcastCamera::BeginPlay()
{
	Super::BeginPlay();

	// Cache the ball — lazy resolve so designer placement order doesn't matter.
	for (TActorIterator<ASoccerBallVisual> It(GetWorld()); It; ++It)
	{
		CachedBall = *It;
		break;
	}

	Camera->FieldOfView = FieldOfView;

	// Initialise X position at the ball's current X (or anchor X if no ball yet).
	CurrentX = CachedBall.IsValid() ? CachedBall->GetActorLocation().X : TouchlineAnchor.X;
	SetActorLocation(FVector(CurrentX, TouchlineAnchor.Y, TouchlineAnchor.Z));

	if (bAutoActivateOnBeginPlay)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->SetViewTargetWithBlend(this, ActivateBlendTime);
			UE_LOG(LogEdge26, Log, TEXT("BroadcastCamera became view target."));
		}
	}
}

void ABroadcastCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// X is up-pitch in Phase 2. Camera follows the ball's X smoothly.
	if (CachedBall.IsValid())
	{
		const float BallX = CachedBall->GetActorLocation().X;
		CurrentX = FMath::FInterpTo(CurrentX, BallX, DeltaTime, TrackInterpSpeed);
	}

	const FVector NewLocation(CurrentX, TouchlineAnchor.Y, TouchlineAnchor.Z);
	SetActorLocation(NewLocation);

	// Look at a point on the opposite touchline at the same X. Pure pan,
	// no roll, pitch derived from height vs lateral distance.
	const FVector LookTarget(CurrentX, LookAtY, 0.0f);
	const FRotator NewRotation = (LookTarget - NewLocation).Rotation();
	SetActorRotation(NewRotation);

	// Defend against auto-switch's PC->Possess stealing the view target.
	if (bForceViewTarget)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->GetViewTarget() != this)
			{
				PC->SetViewTarget(this);
			}
		}
	}
}
