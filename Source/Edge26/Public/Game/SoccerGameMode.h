// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SoccerGameMode.generated.h"

class ASoccerBall;
class APlayerStart;
class AFootballerCharacter;

UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	Kickoff   UMETA(DisplayName = "Kickoff"),
	Playing   UMETA(DisplayName = "Playing"),
	GoalCeleb UMETA(DisplayName = "Goal Celebration"),
	FullTime  UMETA(DisplayName = "Full Time")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGoalScored, int32, ScoringTeam, int32, NewTeamScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChanged, EMatchPhase, NewPhase);

/**
 * Scoreboard + match-flow controller.
 *
 *  - Scores are int32[2], indexed by team (0 home, 1 away).
 *  - RegisterGoal increments the appropriate score and schedules a kickoff
 *    reset after GoalCelebrationDuration seconds.
 *  - ResetForKickoff teleports the ball to BallSpawnLocation and any
 *    registered footballers back to their bound start transforms.
 *  - Match-length timer drives the FullTime transition.
 *
 * Ball/player references resolve lazily from the world, so designers can
 * place actors freely in the level — no manual wiring needed.
 */
UCLASS()
class EDGE26_API ASoccerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASoccerGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Match")
	void RegisterGoal(int32 ScoringTeam);

	UFUNCTION(BlueprintCallable, Category = "Match")
	void ResetForKickoff();

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetScore(int32 TeamId) const;

	UFUNCTION(BlueprintPure, Category = "Match")
	float GetMatchTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	EMatchPhase GetMatchPhase() const { return MatchPhase; }

	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnGoalScored OnGoalScored;

	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnPhaseChanged OnPhaseChanged;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float MatchDurationSeconds = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float GoalCelebrationDuration = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	FVector BallSpawnLocation = FVector(0.0f, 0.0f, 30.0f);

protected:
	void SetPhase(EMatchPhase NewPhase);
	void EndKickoff();
	ASoccerBall* FindOrCacheBall();

private:
	UPROPERTY(Transient)
	int32 Scores[2] = { 0, 0 };

	UPROPERTY(Transient)
	EMatchPhase MatchPhase = EMatchPhase::Kickoff;

	UPROPERTY(Transient)
	float MatchClock = 0.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<ASoccerBall> CachedBall;

	FTimerHandle KickoffResetHandle;
	FTimerHandle KickoffStartHandle;
};
