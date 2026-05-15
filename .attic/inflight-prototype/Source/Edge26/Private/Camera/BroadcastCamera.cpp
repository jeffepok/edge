// Copyright Edge26. All Rights Reserved.

#include "Camera/BroadcastCamera.h"

#include "Ball/SoccerBall.h"
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
	for (TActorIterator<ASoccerBall> It(GetWorld()); It; ++It)
	{
		CachedBall = *It;
		break;
	}

	Camera->FieldOfView = FieldOfView;

	// Initialise position at the anchor; CurrentY follows the ball from frame one.
	CurrentY = CachedBall.IsValid() ? CachedBall->GetActorLocation().Y : TouchlineAnchor.Y;
	SetActorLocation(FVector(TouchlineAnchor.X, CurrentY, TouchlineAnchor.Z));

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

	if (CachedBall.IsValid())
	{
		const float BallY = CachedBall->GetActorLocation().Y;
		CurrentY = FMath::FInterpTo(CurrentY, BallY, DeltaTime, TrackInterpSpeed);
	}

	const FVector NewLocation(TouchlineAnchor.X, CurrentY, TouchlineAnchor.Z);
	SetActorLocation(NewLocation);

	// Look at the pitch directly across from the camera, at the same Y. Pure pan,
	// no roll, pitch derived from height vs horizontal distance.
	const FVector LookTarget(LookAtX, CurrentY, 0.0f);
	const FRotator NewRotation = (LookTarget - NewLocation).Rotation();
	SetActorRotation(NewRotation);

	// Lock controller rotation to camera yaw so OnMove's "forward" matches screen-up.
	// Pawn facing is unaffected (FootballerCharacter has bUseControllerRotationYaw=false).
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		const FRotator FlatYaw(0.0f, NewRotation.Yaw, 0.0f);
		PC->SetControlRotation(FlatYaw);
	}
}
