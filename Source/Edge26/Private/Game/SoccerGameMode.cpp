// Copyright Edge26. All Rights Reserved.

#include "Game/SoccerGameMode.h"

#include "Ball/SoccerBall.h"
#include "EngineUtils.h"
#include "Game/SoccerHUD.h"
#include "GameFramework/PlayerStart.h"
#include "Player/FootballerCharacter.h"
#include "TimerManager.h"
#include "Edge26.h"

ASoccerGameMode::ASoccerGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AFootballerCharacter::StaticClass();
	HUDClass = ASoccerHUD::StaticClass();
}

void ASoccerGameMode::StartPlay()
{
	Super::StartPlay();

	Scores[0] = 0;
	Scores[1] = 0;
	MatchClock = MatchDurationSeconds;
	SetPhase(EMatchPhase::Kickoff);
	ResetForKickoff();

	GetWorldTimerManager().SetTimer(KickoffStartHandle, this, &ASoccerGameMode::EndKickoff, 1.5f, false);
}

void ASoccerGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MatchPhase == EMatchPhase::Playing)
	{
		MatchClock = FMath::Max(0.0f, MatchClock - DeltaSeconds);
		if (MatchClock <= 0.0f)
		{
			SetPhase(EMatchPhase::FullTime);
		}
	}
}

void ASoccerGameMode::RegisterGoal(int32 ScoringTeam)
{
	if (MatchPhase != EMatchPhase::Playing && MatchPhase != EMatchPhase::Kickoff)
	{
		return;
	}
	if (ScoringTeam < 0 || ScoringTeam > 1)
	{
		return;
	}

	Scores[ScoringTeam]++;
	UE_LOG(LogEdge26, Log, TEXT("Score: Home %d - %d Away"), Scores[0], Scores[1]);
	OnGoalScored.Broadcast(ScoringTeam, Scores[ScoringTeam]);

	SetPhase(EMatchPhase::GoalCeleb);

	GetWorldTimerManager().ClearTimer(KickoffResetHandle);
	GetWorldTimerManager().SetTimer(KickoffResetHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ResetForKickoff();
		SetPhase(EMatchPhase::Kickoff);
		GetWorldTimerManager().SetTimer(KickoffStartHandle, this, &ASoccerGameMode::EndKickoff, 1.5f, false);
	}), GoalCelebrationDuration, false);
}

void ASoccerGameMode::ResetForKickoff()
{
	if (ASoccerBall* Ball = FindOrCacheBall())
	{
		Ball->ResetTo(BallSpawnLocation);
	}
	else
	{
		UE_LOG(LogEdge26, Warning, TEXT("ResetForKickoff: no ball in level."));
	}

	// Move every footballer back to its nearest PlayerStart by tag (or by index).
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Starts.Add(*It);
	}

	int32 Idx = 0;
	for (TActorIterator<AFootballerCharacter> It(GetWorld()); It; ++It)
	{
		AFootballerCharacter* F = *It;
		if (Starts.IsValidIndex(Idx))
		{
			F->TeleportTo(Starts[Idx]->GetActorLocation(), Starts[Idx]->GetActorRotation(), false, true);
		}
		++Idx;
	}
}

void ASoccerGameMode::EndKickoff()
{
	if (MatchPhase == EMatchPhase::Kickoff)
	{
		SetPhase(EMatchPhase::Playing);
	}
}

void ASoccerGameMode::SetPhase(EMatchPhase NewPhase)
{
	if (MatchPhase == NewPhase)
	{
		return;
	}
	MatchPhase = NewPhase;
	OnPhaseChanged.Broadcast(NewPhase);
	UE_LOG(LogEdge26, Verbose, TEXT("Match phase → %d"), (int32)NewPhase);
}

ASoccerBall* ASoccerGameMode::FindOrCacheBall()
{
	if (CachedBall.IsValid())
	{
		return CachedBall.Get();
	}
	for (TActorIterator<ASoccerBall> It(GetWorld()); It; ++It)
	{
		CachedBall = *It;
		return *It;
	}
	return nullptr;
}

int32 ASoccerGameMode::GetScore(int32 TeamId) const
{
	if (TeamId < 0 || TeamId > 1) return 0;
	return Scores[TeamId];
}

float ASoccerGameMode::GetMatchTimeRemaining() const
{
	return MatchClock;
}
