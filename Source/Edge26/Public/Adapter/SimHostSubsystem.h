// Copyright Edge26. All Rights Reserved.
// Owns the SimWorld; ticks at 50Hz; drives visual actors with interpolated state.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sim/SimWorld.h"
#include "Sim/InputFrame.h"
#include "Adapter/RenderSnapshotBuffer.h"
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

	// Read-only snapshot of the latest sim state (used by debug renderer).
	const edge26::FSimWorldState& GetState() const { return CurrState; }

	// Kickoff reset helpers (GameMode calls these).
	void ResetBall(FVector WorldPos);
	void ResetPlayer(int32 ControllerIndex, FVector WorldPos, FRotator WorldRot);

	// M1: position all 22 players at 4-3-3 slots (called by GameMode on kickoff).
	// Iterates the sim state and writes both sim state AND visual actor transforms.
	void ResetAllPlayersTo4_3_3();

	// M12: place ball at the given player's feet and grant them possession.
	// Also nominates that player as the human at kickoff. Used by ResetForKickoff
	// to bootstrap on-ball AI evaluation.
	void ResetBallAtCarrier(int32 TeamId, int32 PlayerIndex);

	// Sync UE5-side score into the sim so Layer A team-strategy reads the real score.
	void SetMatchScore(uint8 TeamId, uint16 NewScore);

private:
	void DriveVisuals(float Alpha);
    void DriveVisualsFromCurrPrev(float Alpha);
    void DriveVisualsLegacy(float Alpha);

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

	// M9: tracks last sim HumanControlledIndex to detect changes and re-Possess.
	uint8 LastHumanControlledIndex = 0xFF;

    // M12 P3: ring buffer of 25 snapshots (~500 ms). DriveVisuals consumes
    // from kRenderDelayTicks behind so anim has time to play foot-strike
    // montages before the ball "releases" at the BallContact notify.
    FRenderSnapshotBuffer SnapshotBuffer;

    // M12 P3: events emitted by SnapshotBuffer this frame.
    // Drained by DriveVisuals; broadcast to per-pawn anim instances.
    TArray<FAnimEventPayload> PendingAnimEvents;
};
