// Copyright Edge26. All Rights Reserved.
// Render-only pawn. No physics, no movement component. Driven by SimHostSubsystem.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Animation/FootballerAnimEvents.h"
#include "Animation/BallContactIKComponent.h"
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

	/** Fired by SimHostSubsystem when an anim event targets this pawn. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnimEvent, const FAnimEventPayload&, Event);

	UPROPERTY(BlueprintAssignable, Category="Anim")
	FOnAnimEvent OnAnimEvent;

	UFUNCTION()
	void HandleAnimEvent(const FAnimEventPayload& Event);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class USimInputCollector> InputCollector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class UBallContactIKComponent> KickIK;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UBroadcastSpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UCameraComponent> Camera;

public:
	// Sim-tracked velocity (computed each frame from actor location delta in
	// DriveFromSim). Override AActor::GetVelocity() so PoseSearchHistoryCollector's
	// bGenerateTrajectory mode reads a meaningful value instead of the engine
	// default (zero — we have no UCharacterMovementComponent).
	virtual FVector GetVelocity() const override { return SimTrackedVelocity; }

private:
	FTransform LastDrivenTransform;
	FVector SimTrackedVelocity = FVector::ZeroVector;
};
