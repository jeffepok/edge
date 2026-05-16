// Copyright Edge26. All Rights Reserved.
#include "Adapter/SimHostSubsystem.h"
#include "Adapter/FootballerVisual.h"
#include "Adapter/SoccerBallVisual.h"
#include "Edge26.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "AI/Formations.h"
#include "AI/Switching.h"
#include <cstring>

using namespace edge26;

void USimHostSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = new SimWorld(0xED9E26ull);
	Sim->Snapshot(PrevState);
	Sim->Snapshot(CurrState);
	std::memset(&CurrentInput, 0, sizeof(CurrentInput));
}

void USimHostSubsystem::Deinitialize()
{
	delete Sim; Sim = nullptr;
	Super::Deinitialize();
}

void USimHostSubsystem::Tick(float DeltaTime)
{
	if (!Sim) return;
	Accumulator += DeltaTime;
	int safetyCap = 5;
	while (Accumulator >= TickDuration && safetyCap-- > 0)
	{
		CurrentInput.TickNumber = CurrentTick;
		PrevState = CurrState;
		Sim->Step(CurrentInput);
		Sim->Snapshot(CurrState);

		// Clear one-shot bits after consumption so they don't latch across ticks.
		// Sprint (1<<0) is held by design — leave it alone.
		constexpr uint8 kOneShotMask = (1<<1) | (1<<2) | (1<<3) | (1<<4); // Pass, Shoot, Chip, Switch
		CurrentInput.Buttons[0] &= (uint8)~kOneShotMask;

		// M9: re-Possess the right pawn when the sim's HumanControlledIndex changes.
		const uint8 idxNow = (uint8)Sim->GetState().Match.HumanControlledIndex;
		if (idxNow != LastHumanControlledIndex && idxNow != 0xFF)
		{
			if (auto* PC = GetWorld()->GetFirstPlayerController())
			{
				for (TActorIterator<AFootballerVisual> It(GetWorld()); It; ++It)
				{
					if ((uint8)It->ControllerIndex == idxNow)
					{
						PC->Possess(*It);
						break;
					}
				}
			}
			LastHumanControlledIndex = idxNow;
		}

		CurrentTick++;
		Accumulator -= TickDuration;
	}
	if (safetyCap < 0) Accumulator = 0.0f;  // bail on tick spiral

	float Alpha = FMath::Clamp(Accumulator / TickDuration, 0.0f, 1.0f);
	DriveVisuals(Alpha);
}

static FVector ToUE(edge26::FixedVec3 v)
{
	// sim cm → UE5 cm (same unit; lossy float convert for render only).
	return FVector{
		(double)v.X.Raw / (double)edge26::Fixed64::One,
		(double)v.Y.Raw / (double)edge26::Fixed64::One,
		(double)v.Z.Raw / (double)edge26::Fixed64::One,
	};
}

static FRotator ToUEYaw(edge26::FixedAngle a)
{
	double rad = (double)a.Raw.Raw / (double)edge26::Fixed32::One;
	return FRotator(0.0, FMath::RadiansToDegrees(rad), 0.0);
}

void USimHostSubsystem::DriveVisuals(float Alpha)
{
	// Ball.
	if (Ball.IsValid())
	{
		FVector p0 = ToUE(PrevState.Ball.Position);
		FVector p1 = ToUE(CurrState.Ball.Position);
		FVector p  = FMath::Lerp(p0, p1, Alpha);
		Ball->DriveFromSim(FTransform(FRotator::ZeroRotator, p));
	}
	// Footballers.
	for (auto& Weak : Footballers)
	{
		AFootballerVisual* F = Weak.Get();
		if (!F) continue;
		const int32 idx = F->ControllerIndex;
		if (idx < 0 || idx >= edge26::kSimPlayerCount) continue;
		FVector  p0 = ToUE(PrevState.Players[idx].Position);
		FVector  p1 = ToUE(CurrState.Players[idx].Position);
		FVector  p  = FMath::Lerp(p0, p1, Alpha);
		FRotator r  = ToUEYaw(CurrState.Players[idx].Heading);
		F->DriveFromSim(FTransform(r, p));
	}
}

// Forward declare; defined below.
static edge26::Fixed64 ToFixed64(double cm);

void USimHostSubsystem::RegisterFootballer(AFootballerVisual* Pawn, int32 ControllerIndex)
{
	if (!Pawn) return;
	Pawn->ControllerIndex = ControllerIndex;
	Footballers.Add(Pawn);
	if (Sim && ControllerIndex >= 0 && ControllerIndex < edge26::kSimPlayerCount)
	{
		auto& P = Sim->MutableState().Players[ControllerIndex];
		// Visuals carry their identity (0..21). The sim's ControllerIndex was
		// a Phase 1 concept (which input slot to read); in Phase 2 it's vestigial
		// — the human is determined by Match.HumanControlledIndex, and AI players
		// have no input slot. Keep sim's ControllerIndex as the slot id only for
		// indices 0-1 (the two reachable input slots in v0).
		P.ControllerIndex = (ControllerIndex < 2)
		    ? (uint8)ControllerIndex
		    : edge26::kStationaryController;
		// Seed sim position from where the actor was placed in the level — otherwise
		// the next tick teleports the visual to (0,0,0) and it disappears below the floor.
		const FVector Loc = Pawn->GetActorLocation();
		P.Position = { ToFixed64(Loc.X), ToFixed64(Loc.Y), ToFixed64(Loc.Z) };
		// Sync interp cache so the first render frame doesn't lerp from (0,0,0) → placed pos.
		Sim->Snapshot(CurrState);
		PrevState = CurrState;
	}
}

void USimHostSubsystem::RegisterBall(ASoccerBallVisual* InBall)
{
	Ball = InBall;
	if (Sim && InBall)
	{
		const FVector Loc = InBall->GetActorLocation();
		auto& BallState = Sim->MutableState().Ball;
		BallState.Position = { ToFixed64(Loc.X), ToFixed64(Loc.Y), ToFixed64(Loc.Z) };
		BallState.Velocity = edge26::FixedVec3::Zero();
		BallState.Flags = 0;
		Sim->Snapshot(CurrState);
		PrevState = CurrState;
	}
}

void USimHostSubsystem::SetMoveInput(int32 ControllerIndex, FVector2D Stick)
{
	if (ControllerIndex < 0 || ControllerIndex >= 2) return;
	CurrentInput.Move[ControllerIndex][0] = (int8)FMath::Clamp(Stick.X * 127.0f, -127.0f, 127.0f);
	CurrentInput.Move[ControllerIndex][1] = (int8)FMath::Clamp(Stick.Y * 127.0f, -127.0f, 127.0f);
}

void USimHostSubsystem::SetButton(int32 ControllerIndex, uint8 ButtonMask, bool bDown)
{
	if (ControllerIndex < 0 || ControllerIndex >= 2) return;
	if (bDown) CurrentInput.Buttons[ControllerIndex] |=  ButtonMask;
	else       CurrentInput.Buttons[ControllerIndex] &= ~ButtonMask;
}

FVector USimHostSubsystem::GetBallPositionWorld() const
{
	if (!Sim) return FVector::ZeroVector;
	return ToUE(Sim->GetState().Ball.Position);
}

// Convert UE5 cm (double) to sim Q32.32 Fixed64. Lossy but only used for resets.
static edge26::Fixed64 ToFixed64(double cm)
{
	const double raw = cm * (double)edge26::Fixed64::One;
	return edge26::Fixed64::FromRaw((int64_t)raw);
}

void USimHostSubsystem::ResetBall(FVector WorldPos)
{
	if (!Sim) return;
	auto& BallState = Sim->MutableState().Ball;
	BallState.Position = { ToFixed64(WorldPos.X), ToFixed64(WorldPos.Y), ToFixed64(WorldPos.Z) };
	BallState.Velocity = edge26::FixedVec3::Zero();
	BallState.AngularVelocity = edge26::FixedVec3::Zero();
	BallState.Flags = 0;
	// Sync interp cache so visuals don't lerp from the pre-reset position.
	Sim->Snapshot(CurrState);
	PrevState = CurrState;
}

void USimHostSubsystem::ResetBallAtCarrier(int32 TeamId, int32 PlayerIndex)
{
	if (!Sim) return;
	if (PlayerIndex < 0 || PlayerIndex >= edge26::kSimPlayerCount) return;
	auto& State = Sim->MutableState();
	State.Ball.Position = State.Players[PlayerIndex].Position;
	State.Ball.Velocity = edge26::FixedVec3::Zero();
	State.Ball.AngularVelocity = edge26::FixedVec3::Zero();
	State.Ball.Flags = 0;
	State.Match.PossessionTeam   = (uint8_t)TeamId;
	State.Match.PossessionPlayer = (uint8_t)PlayerIndex;
	// Human (always team 0 in v0) controls the nearest home outfielder to the
	// ball. ChooseHumanControlled would do this on next tick, but pin it now
	// so the manual-switch-cooldown-suppressed first 25 ticks have a sensible
	// focus even when the carrier is on the away team.
	int humanIdx = edge26::ChooseHumanControlled(State, 0);
	if (humanIdx >= 0) State.Match.HumanControlledIndex = (uint8_t)humanIdx;
	Sim->Snapshot(CurrState);
	PrevState = CurrState;
}

void USimHostSubsystem::ResetPlayer(int32 ControllerIndex, FVector WorldPos, FRotator WorldRot)
{
	if (!Sim) return;
	if (ControllerIndex < 0 || ControllerIndex >= edge26::kSimPlayerCount) return;
	auto& P = Sim->MutableState().Players[ControllerIndex];
	P.Position = { ToFixed64(WorldPos.X), ToFixed64(WorldPos.Y), ToFixed64(WorldPos.Z) };
	P.Velocity = edge26::FixedVec3::Zero();
	const double yawRad = FMath::DegreesToRadians(WorldRot.Yaw);
	const int32_t yawRaw = (int32_t)(yawRad * (double)edge26::Fixed32::One);
	P.Heading      = edge26::FixedAngle::FromRaw(yawRaw);
	P.FacingTarget = P.Heading;
	Sim->Snapshot(CurrState);
	PrevState = CurrState;
}

void USimHostSubsystem::SetMatchScore(uint8 TeamId, uint16 NewScore)
{
	if (!Sim || TeamId > 1) return;
	Sim->MutableState().Match.Score[TeamId] = NewScore;
}

void USimHostSubsystem::ResetAllPlayersTo4_3_3()
{
	if (!Sim) return;
	auto& State = Sim->MutableState();
	for (int i = 0; i < edge26::kSimPlayerCount; ++i)
	{
		int teamId = (i < 11) ? 0 : 1;
		int slotIndex = i % 11;
		edge26::FixedVec3 slotPos = edge26::SlotWorldPosition(slotIndex, teamId);
		State.Players[i].Position = slotPos;
		State.Players[i].Velocity = edge26::FixedVec3::Zero();
		State.Players[i].TeamId = (uint8_t)teamId;
		State.Players[i].RoleId = (uint8_t)edge26::kFormation_4_3_3[slotIndex].Role;
	}
	Sim->Snapshot(CurrState);
	PrevState = CurrState;
}
