// Copyright Edge26. All Rights Reserved.

#include "Game/SoccerGameMode.h"

#include "Adapter/FootballerVisual.h"
#include "Adapter/SimHostSubsystem.h"
#include "Adapter/SoccerBallVisual.h"
#include "EngineUtils.h"
#include "Game/SoccerHUD.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"
#include "Edge26.h"
#if !UE_BUILD_SHIPPING
#include "Debug/Edge26CheatManager.h"
#endif

ASoccerGameMode::ASoccerGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AFootballerVisual::StaticClass();
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
	// Sync to sim so Layer A team-strategy sees the new score.
	if (USimHostSubsystem* Host = GetWorld() ? GetWorld()->GetSubsystem<USimHostSubsystem>() : nullptr)
	{
		Host->SetMatchScore((uint8)ScoringTeam, (uint16)Scores[ScoringTeam]);
	}
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
	USimHostSubsystem* Host = GetWorld() ? GetWorld()->GetSubsystem<USimHostSubsystem>() : nullptr;
	if (!Host)
	{
		UE_LOG(LogEdge26, Warning, TEXT("ResetForKickoff: SimHostSubsystem missing."));
		return;
	}

	// M1: place all 22 players at 4-3-3 slots (replaces PlayerStart iteration).
	Host->ResetAllPlayersTo4_3_3();
	// M12 fix: assign initial possession to the AWAY team's CDM (player 16,
	// slot 5) and park the ball at their feet. The user controls the nearest
	// HOME outfielder (auto-picked by ChooseHumanControlled). Putting
	// possession on the AI team at kickoff lets the user OBSERVE AI off-ball
	// behaviour immediately — the away CDM auto-passes/dribbles, home AI
	// presses + tracks, possession changes hands organically. Without this,
	// kickoff stalls (no carrier within KickReach of ball).
	Host->ResetBallAtCarrier(/*TeamId=*/1, /*PlayerIndex=*/16);
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

ASoccerBallVisual* ASoccerGameMode::FindOrCacheBall()
{
	if (CachedBall.IsValid())
	{
		return CachedBall.Get();
	}
	for (TActorIterator<ASoccerBallVisual> It(GetWorld()); It; ++It)
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

void ASoccerGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
#if !UE_BUILD_SHIPPING
	if (NewPlayer)
	{
		NewPlayer->CheatClass = UEdge26CheatManager::StaticClass();
		NewPlayer->EnableCheats();
	}
#endif
}
