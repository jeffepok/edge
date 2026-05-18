// Copyright Edge26. All Rights Reserved.
#include "Adapter/FootballerVisual.h"
#include "Adapter/SimInputCollector.h"
#include "Adapter/SimHostSubsystem.h"
#include "Animation/AnimInstance.h"
#include "Animation/FootballAnimInstance.h"
#include "Camera/BroadcastSpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AFootballerVisual::AFootballerVisual()
{
	PrimaryActorTick.bCanEverTick = false;

	// Auto-possess so the placed BP becomes the player pawn even without a Player Start.
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Default mesh + anim class so a fresh BP subclass is visible out of the box.
	// BP subclasses can still override these in the editor.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> DefaultMeshFinder(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (DefaultMeshFinder.Succeeded())
	{
		Mesh->SetSkeletalMeshAsset(DefaultMeshFinder.Object);
		// SKM_Manny is authored facing +Y; rotate so +X is forward.
		Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		// Drop Z so the mesh sits on the ground (otherwise the root pivot puts feet below).
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	}
	static ConstructorHelpers::FClassFinder<UAnimInstance> DefaultAnimClassFinder(
		TEXT("/Game/Blueprints/Player/ABP_Footballer"));
	if (DefaultAnimClassFinder.Succeeded())
	{
		Mesh->SetAnimInstanceClass(DefaultAnimClassFinder.Class);
	}

	SpringArm = CreateDefaultSubobject<UBroadcastSpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Mesh);
	SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	SpringArm->TargetArmLength = 620.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	InputCollector = CreateDefaultSubobject<USimInputCollector>(TEXT("InputCollector"));
	KickIK = CreateDefaultSubobject<UBallContactIKComponent>(TEXT("KickIK"));
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

	// M3 P3: route AnimEvent broadcasts into the anim instance's event queue.
	OnAnimEvent.AddDynamic(this, &AFootballerVisual::HandleAnimEvent);
}

void AFootballerVisual::HandleAnimEvent(const FAnimEventPayload& Event)
{
	// M8 P3: Kick events drive the foot-IK alpha curve.
	if (Event.Kind == EFootballerAnimEvent::Kick && KickIK)
	{
		KickIK->StartKickMontage(Event);
	}

	if (auto* AnimInst = Cast<UFootballAnimInstance>(Mesh->GetAnimInstance()))
	{
		AnimInst->EnqueueEvent(Event);
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

	// Expose to AActor::GetVelocity() — motion-matching's PoseSearchHistoryCollector
	// reads this to synthesize its trajectory query (we have no UCharacterMovementComponent).
	SimTrackedVelocity = LinVel;

	SetActorTransform(InterpolatedTransform);
	LastDrivenTransform = InterpolatedTransform;
}
