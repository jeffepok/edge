// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerBall.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;

/**
 * Physics-driven football. Tuned for realistic mass, drag, and surface response.
 * All kicks go through ApplyKick — characters never set velocity directly.
 */
UCLASS()
class EDGE26_API ASoccerBall : public AActor
{
	GENERATED_BODY()

public:
	ASoccerBall();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Apply a kick to the ball.
	 * @param WorldDirection  Normalized horizontal direction.
	 * @param Strength        Target speed in cm/s (e.g. 1500 pass, 3500 shot).
	 * @param LiftRatio       0..1 — fraction of the impulse redirected upward (chip).
	 * @param SpinAxis        Optional world-space spin axis. Zero = no spin.
	 * @param SpinMagnitude   Spin in rad/s applied along SpinAxis.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ball")
	void ApplyKick(const FVector& WorldDirection, float Strength, float LiftRatio = 0.0f, const FVector& SpinAxis = FVector::ZeroVector, float SpinMagnitude = 0.0f);

	/** Hard reset: zeroes velocity and teleports to a location. Used by GameMode for kickoff. */
	UFUNCTION(BlueprintCallable, Category = "Ball")
	void ResetTo(const FVector& WorldLocation);

	/** Current horizontal speed (cm/s). Useful for HUD/camera. */
	UFUNCTION(BlueprintPure, Category = "Ball")
	float GetSpeed2D() const;

	UFUNCTION(BlueprintPure, Category = "Ball")
	FVector GetVelocity2D() const;

	USphereComponent* GetCollision() const { return Collision; }

protected:
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAudioComponent> AudioComp;

	/** FIFA size 5 ball: 22cm diameter ⇒ 11cm radius. */
	UPROPERTY(EditAnywhere, Category = "Ball|Physics", meta = (ClampMin = "1.0"))
	float Radius = 11.0f;

	/** Real ball mass ~0.43kg. */
	UPROPERTY(EditAnywhere, Category = "Ball|Physics", meta = (ClampMin = "0.05"))
	float MassKg = 0.43f;

	/** Linear damping — represents air drag + grass rolling resistance. */
	UPROPERTY(EditAnywhere, Category = "Ball|Physics")
	float LinearDamping = 0.45f;

	/** Angular damping — slows spin over time. */
	UPROPERTY(EditAnywhere, Category = "Ball|Physics")
	float AngularDamping = 0.6f;

	/** Cap horizontal speed so kicks don't fly to orbit. */
	UPROPERTY(EditAnywhere, Category = "Ball|Physics")
	float MaxSpeed2D = 4500.0f;

	/** Total 3D speed cap. Stops glancing player hits launching the ball straight up. */
	UPROPERTY(EditAnywhere, Category = "Ball|Physics")
	float MaxSpeed3D = 2800.0f;

	/** When a Pawn bumps the ball, post-collision ball speed is clamped to PawnSpeed * this multiplier. */
	UPROPERTY(EditAnywhere, Category = "Ball|Physics", meta = (ClampMin = "1.0"))
	float PawnBumpSpeedMult = 1.2f;

	/** Floor for pawn-bump clamp — even a stationary pawn can produce a small nudge. */
	UPROPERTY(EditAnywhere, Category = "Ball|Physics", meta = (ClampMin = "0.0"))
	float PawnBumpMinSpeed = 250.0f;

	/** Time after a real kick during which pawn collisions should NOT dampen velocity. */
	UPROPERTY(EditAnywhere, Category = "Ball|Physics", meta = (ClampMin = "0.0"))
	float KickInFlightWindow = 0.6f;

	/** Cooldown between kicks so two characters don't double-kick on same frame. */
	UPROPERTY(EditAnywhere, Category = "Ball|Gameplay", meta = (ClampMin = "0.0"))
	float KickCooldown = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Ball|FX")
	TObjectPtr<USoundBase> KickSound;

	UPROPERTY(EditAnywhere, Category = "Ball|FX")
	TObjectPtr<USoundBase> BounceSound;

	float LastKickTime = -100.0f;
	float LastBounceTime = -100.0f;
};
