// Copyright Edge26. All Rights Reserved.
#include "Adapter/FootballerVisual.h"
#include "Adapter/SimInputCollector.h"
#include "Adapter/SimHostSubsystem.h"
#include "Camera/BroadcastSpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

AFootballerVisual::AFootballerVisual()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SpringArm = CreateDefaultSubobject<UBroadcastSpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Mesh);
	SpringArm->TargetArmLength = 400.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	InputCollector = CreateDefaultSubobject<USimInputCollector>(TEXT("InputCollector"));
}

void AFootballerVisual::BeginPlay()
{
	Super::BeginPlay();
	if (auto* World = GetWorld())
	{
		if (auto* Host = World->GetSubsystem<USimHostSubsystem>())
		{
			Host->RegisterFootballer(this, ControllerIndex);
		}
	}
}

void AFootballerVisual::DriveFromSim(const FTransform& InterpolatedTransform)
{
	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	const FVector NewLoc = InterpolatedTransform.GetLocation();
	const FVector OldLoc = LastDrivenTransform.GetLocation();
	const FVector LinVel = (DeltaTime > KINDA_SMALL_NUMBER)
		? (NewLoc - OldLoc) / DeltaTime
		: FVector::ZeroVector;

	Speed = LinVel.Size();
	const FVector Fwd   = InterpolatedTransform.GetRotation().GetForwardVector();
	const FVector Right = InterpolatedTransform.GetRotation().GetRightVector();
	ForwardSpeed = FVector::DotProduct(LinVel, Fwd);
	RightSpeed   = FVector::DotProduct(LinVel, Right);
	RelativeDirection = FMath::RadiansToDegrees(FMath::Atan2(RightSpeed, ForwardSpeed));

	SetActorTransform(InterpolatedTransform);
	LastDrivenTransform = InterpolatedTransform;
}
