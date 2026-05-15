// Copyright Edge26. All Rights Reserved.
// Render-only pawn. No physics, no movement component. Driven by SimHostSubsystem.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FootballerVisual.generated.h"

class USkeletalMeshComponent;
class UCameraComponent;
class UBroadcastSpringArmComponent;

UCLASS()
class EDGE26_API AFootballerVisual : public APawn
{
	GENERATED_BODY()

public:
	AFootballerVisual();

	/** Sim-side controller index (0=P1, 1=P2, 0xFF=stationary). Synced from BP defaults. */
	UPROPERTY(EditAnywhere, Category = "Sim")
	int32 ControllerIndex = 0;

	/** Called every render frame by SimHostSubsystem with the interpolated transform. */
	void DriveFromSim(const FTransform& InterpolatedTransform);

	/** Animation-facing state — written by DriveFromSim. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float ForwardSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float RightSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float RelativeDirection = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class USimInputCollector> InputCollector;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UBroadcastSpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UCameraComponent> Camera;

private:
	FTransform LastDrivenTransform;
};
