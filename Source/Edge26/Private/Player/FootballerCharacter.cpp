// Copyright Edge26. All Rights Reserved.

#include "Player/FootballerCharacter.h"

#include "Ball/SoccerBall.h"
#include "Camera/CameraComponent.h"
#include "Camera/BroadcastSpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Kismet/KismetMathLibrary.h"
#include "Edge26.h"

AFootballerCharacter::AFootballerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Capsule sized for an average footballer (~180cm).
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 90.0f);

	// We do NOT want the controller's full rotation driving the pawn — only yaw input rotates body.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	Move->bConstrainToPlane = false;
	Move->bSnapToPlaneAtStart = false;
	Move->JumpZVelocity = 420.0f;
	Move->AirControl = 0.15f;
	Move->MaxWalkSpeed = JogSpeed;
	Move->MinAnalogWalkSpeed = 60.0f;
	Move->BrakingDecelerationWalking = 1600.0f;
	Move->BrakingFrictionFactor = 1.0f;
	Move->GroundFriction = 7.0f;
	Move->MaxAcceleration = 1500.0f;
	Move->PerchRadiusThreshold = 12.0f;

	SpringArm = CreateDefaultSubobject<UBroadcastSpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
}

void AFootballerCharacter::BeginPlay()
{
	Super::BeginPlay();
	RegisterMappingContext();
	LastYawDeg = GetActorRotation().Yaw;
}

void AFootballerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	RegisterMappingContext();
}

void AFootballerCharacter::RegisterMappingContext()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsys->ClearAllMappings();
				Subsys->AddMappingContext(DefaultMappingContext, 0);
			}
			else
			{
				UE_LOG(LogEdge26, Warning, TEXT("FootballerCharacter %s has no DefaultMappingContext assigned."), *GetName());
			}
		}
	}
}

void AFootballerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogEdge26, Error, TEXT("FootballerCharacter expected EnhancedInputComponent."));
		return;
	}

	if (IA_Move)    EIC->BindAction(IA_Move,    ETriggerEvent::Triggered, this, &AFootballerCharacter::OnMove);
	if (IA_Move)    EIC->BindAction(IA_Move,    ETriggerEvent::Completed, this, &AFootballerCharacter::OnMove);
	if (IA_Look)    EIC->BindAction(IA_Look,    ETriggerEvent::Triggered, this, &AFootballerCharacter::OnLook);
	if (IA_Sprint)  EIC->BindAction(IA_Sprint,  ETriggerEvent::Started,   this, &AFootballerCharacter::OnSprintPressed);
	if (IA_Sprint)  EIC->BindAction(IA_Sprint,  ETriggerEvent::Completed, this, &AFootballerCharacter::OnSprintReleased);
	if (IA_Pass)    EIC->BindAction(IA_Pass,    ETriggerEvent::Started,   this, &AFootballerCharacter::OnPass);
	if (IA_Shoot)   EIC->BindAction(IA_Shoot,   ETriggerEvent::Started,   this, &AFootballerCharacter::OnShoot);
	if (IA_Chip)    EIC->BindAction(IA_Chip,    ETriggerEvent::Started,   this, &AFootballerCharacter::OnChip);
}

void AFootballerCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	MovementInput = Axis;

	if (Axis.IsNearlyZero())
	{
		return;
	}

	const AController* Ctrl = GetController();
	if (!Ctrl)
	{
		return;
	}

	const FRotator ControlYaw(0.0f, Ctrl->GetControlRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(ControlYaw).GetUnitAxis(EAxis::X);
	const FVector Right   = FRotationMatrix(ControlYaw).GetUnitAxis(EAxis::Y);

	const FVector WorldDir = (Forward * Axis.Y + Right * Axis.X).GetClampedToMaxSize(1.0f);
	AddMovementInput(WorldDir, 1.0f);

	// Sprint engages only when input is mostly forward relative to current facing.
	if (bSprintHeld)
	{
		const FVector ActorFwd = GetActorForwardVector();
		const float Dot = FVector::DotProduct(ActorFwd, WorldDir);
		const bool bShouldSprint = (Dot >= SprintForwardDotMin);
		const float Target = bShouldSprint ? SprintSpeed : JogSpeed;
		GetCharacterMovement()->MaxWalkSpeed = Target;
		bIsSprinting = bShouldSprint;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = JogSpeed;
		bIsSprinting = false;
	}
}

void AFootballerCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->AddYawInput(Axis.X);
		PC->AddPitchInput(Axis.Y);
	}
}

void AFootballerCharacter::OnSprintPressed(const FInputActionValue&) { bSprintHeld = true; }
void AFootballerCharacter::OnSprintReleased(const FInputActionValue&) { bSprintHeld = false; bIsSprinting = false; GetCharacterMovement()->MaxWalkSpeed = JogSpeed; }

void AFootballerCharacter::OnPass(const FInputActionValue&)
{
	ExecuteKick(PassStrength, 0.0f);
}

void AFootballerCharacter::OnShoot(const FInputActionValue&)
{
	ExecuteKick(ShotStrength, 0.0f);
}

void AFootballerCharacter::OnChip(const FInputActionValue&)
{
	ExecuteKick(PassStrength * 1.1f, ChipLiftRatio);
}

void AFootballerCharacter::ExecuteKick(float Strength, float LiftRatio)
{
	ASoccerBall* Ball = FindKickableBall();
	if (!Ball)
	{
		return;
	}

	const FVector ToBall = Ball->GetActorLocation() - GetActorLocation();
	FVector KickDir = GetActorForwardVector();
	// Bias kick direction toward player facing but allow slight redirection if the ball is off-axis.
	const FVector ToBall2D = FVector(ToBall.X, ToBall.Y, 0.0f).GetSafeNormal();
	if (!ToBall2D.IsNearlyZero())
	{
		KickDir = (KickDir * 0.7f + ToBall2D * 0.3f).GetSafeNormal();
	}

	Ball->ApplyKick(KickDir, Strength, LiftRatio);

	UE_LOG(LogEdge26, Verbose, TEXT("%s kicked: strength=%.0f lift=%.2f"), *GetName(), Strength, LiftRatio);
}

ASoccerBall* AFootballerCharacter::FindKickableBall() const
{
	const FVector Origin = GetActorLocation() + GetActorForwardVector() * (KickReach * 0.4f);
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Edge26_KickFind), false, this);
	FCollisionObjectQueryParams ObjQuery;
	ObjQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	const bool bAny = GetWorld()->OverlapMultiByObjectType(
		Hits,
		Origin,
		FQuat::Identity,
		ObjQuery,
		FCollisionShape::MakeSphere(KickReach),
		Params);

	if (!bAny)
	{
		return nullptr;
	}

	ASoccerBall* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector Self = GetActorLocation();
	for (const FOverlapResult& O : Hits)
	{
		if (ASoccerBall* B = Cast<ASoccerBall>(O.GetActor()))
		{
			const float DSq = FVector::DistSquared(Self, B->GetActorLocation());
			if (DSq < BestDistSq)
			{
				BestDistSq = DSq;
				Best = B;
			}
		}
	}
	return Best;
}

void AFootballerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector Vel = GetVelocity();
	Speed = Vel.Size2D();

	const FRotator ActorRot = GetActorRotation();
	const FVector LocalVel = ActorRot.UnrotateVector(FVector(Vel.X, Vel.Y, 0.0f));
	ForwardSpeed = LocalVel.X;
	RightSpeed   = LocalVel.Y;

	if (Speed > 5.0f)
	{
		const FVector VelDir = FVector(Vel.X, Vel.Y, 0.0f).GetSafeNormal();
		const FVector Fwd = ActorRot.Vector();
		RelativeDirection = UKismetMathLibrary::DegAtan2(
			FVector::CrossProduct(Fwd, VelDir).Z,
			FVector::DotProduct(Fwd, VelDir));
	}
	else
	{
		RelativeDirection = 0.0f;
	}

	const float NewYaw = ActorRot.Yaw;
	const float YawDelta = FMath::FindDeltaAngleDegrees(LastYawDeg, NewYaw);
	LastYawDeg = NewYaw;
	YawRatePerSec = (DeltaSeconds > KINDA_SMALL_NUMBER) ? (YawDelta / DeltaSeconds) : 0.0f;

	const float TargetLean = FMath::Clamp(YawRatePerSec * LeanGainDegPerYawRate * (Speed / 600.0f), -25.0f, 25.0f);
	LeanAngle = FMath::FInterpTo(LeanAngle, TargetLean, DeltaSeconds, LeanInterpSpeed);

	bIsAccelerating = !MovementInput.IsNearlyZero(0.05f);
}
