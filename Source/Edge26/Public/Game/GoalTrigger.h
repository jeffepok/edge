// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GoalTrigger.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ASoccerBall;

/**
 * Detects when the ball fully crosses the goal line.
 *
 * Place the trigger box just behind the line, sized to the goal mouth.
 * DefendingTeamId identifies the team whose net this is — when the ball
 * enters, the OPPOSITE team is awarded the goal.
 *
 * Ball-only debouncing (1s) prevents spam if physics intersect on reset.
 */
UCLASS()
class EDGE26_API AGoalTrigger : public AActor
{
	GENERATED_BODY()

public:
	AGoalTrigger();

	/** 0 = home, 1 = away. The team that defends this goal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goal", meta = (ClampMin = "0", ClampMax = "1"))
	int32 DefendingTeamId = 0;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnBallEnter(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> FrameMesh;

	float LastTriggerTime = -100.0f;
};
