// Copyright Edge26. All Rights Reserved.

#include "Camera/BroadcastSpringArmComponent.h"

#include "GameFramework/Pawn.h"

UBroadcastSpringArmComponent::UBroadcastSpringArmComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	TargetArmLength = BaseArmLength;
	bUsePawnControlRotation = true;
	bInheritPitch = true;
	bInheritYaw = true;
	bInheritRoll = false;

	bEnableCameraLag = true;
	bEnableCameraRotationLag = true;
	CameraLagSpeed = 6.0f;
	CameraRotationLagSpeed = 8.0f;
	CameraLagMaxDistance = 250.0f;

	SocketOffset = BaseSocketOffset;
	SetRelativeRotation(BasePitchYawRoll);
}

void UBroadcastSpringArmComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	const FVector Vel = OwnerPawn->GetVelocity();
	const float Speed2D = FVector(Vel.X, Vel.Y, 0.0f).Size();

	// Dynamic zoom — broadcast cams pull back when the action speeds up.
	const float ZoomT = FMath::Clamp(Speed2D / FMath::Max(SpeedForFullZoomOut, 1.0f), 0.0f, 1.0f);
	const float TargetArm = FMath::Lerp(BaseArmLength, SprintArmLength, ZoomT);
	TargetArmLength = FMath::FInterpTo(TargetArmLength, TargetArm, DeltaTime, ArmInterpSpeed);

	// Lateral lead — camera anticipates direction of motion.
	const FRotator OwnerRot = OwnerPawn->GetActorRotation();
	const FVector LocalVel = OwnerRot.UnrotateVector(FVector(Vel.X, Vel.Y, 0.0f));
	const float DesiredLead = FMath::Clamp(LocalVel.Y * LeadOffsetPerSpeed, -MaxLeadOffset, MaxLeadOffset);
	CurrentLead = FMath::FInterpTo(CurrentLead, DesiredLead, DeltaTime, LeadInterpSpeed);

	SocketOffset = BaseSocketOffset + FVector(0.0f, CurrentLead, 0.0f);
}
