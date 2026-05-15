// Copyright Edge26. All Rights Reserved.
// Owns the SimWorld; ticks at 50Hz; drives visual actors with interpolated state.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sim/SimWorld.h"
#include "Sim/InputFrame.h"
#include "SimHostSubsystem.generated.h"

class AFootballerVisual;
class ASoccerBallVisual;

UCLASS()
class EDGE26_API USimHostSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(USimHostSubsystem, STATGROUP_Tickables);
	}

	// Visual-shell registration.
	void RegisterFootballer(AFootballerVisual* Pawn, int32 ControllerIndex);
	void RegisterBall(ASoccerBallVisual* Ball);

	// Input pipeline (called by SimInputCollector).
	void SetMoveInput(int32 ControllerIndex, FVector2D Stick);
	void SetButton(int32 ControllerIndex, uint8 ButtonMask, bool bDown);

	// Goal-trigger reads (spec §11).
	FVector GetBallPositionWorld() const;

	// Kickoff reset helpers (GameMode calls these).
	void ResetBall(FVector WorldPos);
	void ResetPlayer(int32 ControllerIndex, FVector WorldPos, FRotator WorldRot);

	// M1: position all 22 players at 4-3-3 slots (called by GameMode on kickoff).
	// Iterates the sim state and writes both sim state AND visual actor transforms.
	void ResetAllPlayersTo4_3_3();

private:
	void DriveVisuals(float Alpha);

	edge26::SimWorld* Sim = nullptr;
	float Accumulator = 0.0f;
	static constexpr float TickDuration = 1.0f / 50.0f;
	uint32 CurrentTick = 0;

	edge26::FInputFrame CurrentInput{};

	// Per-tick transform cache for interpolation.
	edge26::FSimWorldState PrevState{};
	edge26::FSimWorldState CurrState{};

	TArray<TWeakObjectPtr<AFootballerVisual>> Footballers;
	TWeakObjectPtr<ASoccerBallVisual> Ball;
};
