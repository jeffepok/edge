// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FootballerAnimInstance.generated.h"

class AFootballerCharacter;

/**
 * Base AnimInstance the editor-side AnimBP should derive from.
 * Pulls all locomotion floats from the owning footballer once per Update,
 * then exposes them as BlueprintReadOnly so Blend Spaces / state machines
 * can bind without per-frame casts.
 */
UCLASS(Transient, Blueprintable)
class EDGE26_API UFootballerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float ForwardSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float RightSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float RelativeDirection = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float LeanAngle = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float YawRatePerSec = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	bool bIsInAir = false;

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Anim", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AFootballerCharacter> OwningFootballer;
};
