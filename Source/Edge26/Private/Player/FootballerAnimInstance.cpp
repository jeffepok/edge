// Copyright Edge26. All Rights Reserved.

#include "Player/FootballerAnimInstance.h"

#include "Player/FootballerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void UFootballerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwningFootballer = Cast<AFootballerCharacter>(TryGetPawnOwner());
}

void UFootballerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwningFootballer)
	{
		OwningFootballer = Cast<AFootballerCharacter>(TryGetPawnOwner());
		if (!OwningFootballer)
		{
			return;
		}
	}

	Speed             = OwningFootballer->Speed;
	ForwardSpeed      = OwningFootballer->ForwardSpeed;
	RightSpeed        = OwningFootballer->RightSpeed;
	RelativeDirection = OwningFootballer->RelativeDirection;
	LeanAngle         = OwningFootballer->LeanAngle;
	YawRatePerSec     = OwningFootballer->YawRatePerSec;
	bIsAccelerating   = OwningFootballer->bIsAccelerating;
	bIsSprinting      = OwningFootballer->bIsSprinting;
	bIsInAir          = OwningFootballer->GetCharacterMovement() ? OwningFootballer->GetCharacterMovement()->IsFalling() : false;
}
