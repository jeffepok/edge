// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "OpponentAIController.generated.h"

class AFootballerCharacter;
class ASoccerBall;
class AGoalTrigger;

UENUM(BlueprintType)
enum class EOpponentState : uint8
{
	Chase,    // Move toward the ball
	Carry,    // Have control — push ball toward goal
	Shoot,    // Within shoot range — fire toward goal
	Recover   // Just kicked / lost ball — brief pause
};

/**
 * Tickable, no-BehaviorTree AI for the prototype opponent.
 *
 * Decision cadence is per-frame and stateless aside from EOpponentState used
 * for short cooldowns. The controller drives its pawn by calling
 * AddMovementInput on the AFootballerCharacter and adjusting MaxWalkSpeed
 * to switch jog/sprint. Kicks go through the same ApplyKick API the player uses.
 *
 * Goal selection: picks the AGoalTrigger whose DefendingTeamId differs from
 * the controller's TeamId, cached on first use.
 */
UCLASS()
class EDGE26_API AOpponentAIController : public AAIController
{
	GENERATED_BODY()

public:
	AOpponentAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	int32 TeamId = 1;

	/** Within this radius the AI considers itself in striking position to kick. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Tuning")
	float KickRange = 140.0f;

	/** Distance from goal at which carry transitions to shoot. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Tuning")
	float ShootDistance = 1800.0f;

	/** When the ball is more than this far away, the AI sprints. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Tuning")
	float SprintBallDistance = 700.0f;

	/** Power for shots on goal. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Tuning")
	float ShotStrength = 3200.0f;

	/** Power for "carry" nudges that drive the ball toward the goal. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Tuning")
	float CarryStrength = 1200.0f;

	/** After a kick, brief idle so the AI doesn't flail at the ball. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Tuning")
	float RecoverDuration = 0.85f;

	/** Suppress kicks while the ball is faster than this (cm/s). Lets prior kicks complete. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Tuning")
	float NoKickIfBallSpeedAbove = 350.0f;

	/** How much shot direction varies, in degrees. 0 = perfect aim. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Tuning", meta = (ClampMin = "0.0"))
	float AimNoiseDegrees = 6.0f;

protected:
	ASoccerBall* FindBall() const;
	AGoalTrigger* FindTargetGoal() const;
	FVector ComputeAimDirection(const FVector& BallLoc, const FVector& GoalLoc) const;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AFootballerCharacter> ControlledFootballer;

	UPROPERTY(Transient)
	TWeakObjectPtr<ASoccerBall> CachedBall;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGoalTrigger> CachedGoal;

	EOpponentState State = EOpponentState::Chase;
	float StateExitTime = 0.0f;
};
