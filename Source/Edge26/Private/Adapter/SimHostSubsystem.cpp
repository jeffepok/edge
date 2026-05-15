// Copyright Edge26. All Rights Reserved.
#include "Adapter/SimHostSubsystem.h"
#include "Adapter/FootballerVisual.h"
#include "Adapter/SoccerBallVisual.h"
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

void USimHostSubsystem::RegisterFootballer(AFootballerVisual* Pawn, int32 ControllerIndex)
{
	if (!Pawn) return;
	Pawn->ControllerIndex = ControllerIndex;
	Footballers.Add(Pawn);
	if (Sim && ControllerIndex >= 0 && ControllerIndex < edge26::kSimPlayerCount)
	{
		Sim->MutableState().Players[ControllerIndex].ControllerIndex = (uint8)ControllerIndex;
	}
}

void USimHostSubsystem::RegisterBall(ASoccerBallVisual* InBall)
{
	Ball = InBall;
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
