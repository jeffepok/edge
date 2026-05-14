// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FootballerCharacter.generated.h"

class UInputAction;
class UInputMappingContext;
class UCameraComponent;
class UBroadcastSpringArmComponent;
class ASoccerBall;
struct FInputActionValue;

/**
 * Player-controllable footballer.
 *
 * Movement is camera-relative with explicit jog/sprint tiers driven by the sprint
 * input. The CharacterMovementComponent handles inertia/braking; we expose a
 * curated set of float properties (Speed, RelativeDirection, LeanAngle,
 * YawRatePerSec, bIsAccelerating) so a Blend-Space-driven AnimBP can pull
 * everything it needs without re-reading the velocity vector.
 *
 * Kick interaction: the character does not own a "current ball". Pass/Shoot
 * fire a sphere trace; the nearest ASoccerBall within KickReach gets the kick
 * applied. This keeps possession emergent rather than stateful.
 */
UCLASS()
class EDGE26_API AFootballerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AFootballerCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;

	// ===== Animation-facing state =====

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float Speed = 0.0f;

	/** Signed forward speed (cm/s) — positive when running forward relative to actor facing. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float ForwardSpeed = 0.0f;

	/** Signed strafe speed (cm/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float RightSpeed = 0.0f;

	/** -180..180 degrees: angle of velocity relative to actor forward. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float RelativeDirection = 0.0f;

	/** Lateral body lean (degrees). Positive = leaning right. Driven by yaw rate × speed. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float LeanAngle = 0.0f;

	/** Yaw delta per second (deg/s). Drives turn-in-place vs running-turn animations. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float YawRatePerSec = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	bool bIsSprinting = false;

	// ===== Tuning =====

	/** Walking ceiling when no movement input given continuously? Unused — we always run on input. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float JogSpeed = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeed = 820.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float WalkSpeed = 220.0f;

	/** Cosine of dot-product threshold for sprint to engage — must press input mostly forward. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float SprintForwardDotMin = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float LeanGainDegPerYawRate = 0.06f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float LeanInterpSpeed = 6.0f;

	// ===== Kick =====

	UPROPERTY(EditDefaultsOnly, Category = "Kick")
	float KickReach = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kick")
	float PassStrength = 1500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kick")
	float ShotStrength = 3500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kick")
	float ChipLiftRatio = 0.45f;

	// ===== Input bindings (assigned in BP subclass) =====

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Sprint;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Pass;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Shoot;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Chip;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnSprintPressed(const FInputActionValue& Value);
	void OnSprintReleased(const FInputActionValue& Value);
	void OnPass(const FInputActionValue& Value);
	void OnShoot(const FInputActionValue& Value);
	void OnChip(const FInputActionValue& Value);

	/** Locate the nearest ball within KickReach in front of the character. */
	ASoccerBall* FindKickableBall() const;

	void ExecuteKick(float Strength, float LiftRatio);

	void RegisterMappingContext();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBroadcastSpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

private:
	FVector2D MovementInput = FVector2D::ZeroVector;
	bool bSprintHeld = false;
	float LastYawDeg = 0.0f;
};
