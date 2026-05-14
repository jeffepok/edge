// Copyright Edge26. All Rights Reserved.

#include "AI/OpponentAIController.h"

#include "Ball/SoccerBall.h"
#include "EngineUtils.h"
#include "Game/GoalTrigger.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/FootballerCharacter.h"
#include "Edge26.h"

AOpponentAIController::AOpponentAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bAttachToPawn = false;
	bWantsPlayerState = false;
}

void AOpponentAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ControlledFootballer = Cast<AFootballerCharacter>(InPawn);
	if (!ControlledFootballer.IsValid())
	{
		UE_LOG(LogEdge26, Warning, TEXT("OpponentAIController possessed non-footballer pawn %s"), *GetNameSafe(InPawn));
	}
}

void AOpponentAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AFootballerCharacter* Pawn = ControlledFootballer.Get();
	if (!Pawn) { return; }

	ASoccerBall* Ball = FindBall();
	if (!Ball) { return; }

	AGoalTrigger* Goal = FindTargetGoal();
	const FVector PawnLoc = Pawn->GetActorLocation();
	const FVector BallLoc = Ball->GetActorLocation();
	const FVector GoalLoc = Goal ? Goal->GetActorLocation() : (PawnLoc + FVector(0, 4500.0f, 0));

	const FVector ToBall2D = FVector(BallLoc.X - PawnLoc.X, BallLoc.Y - PawnLoc.Y, 0.0f);
	const float DistBall = ToBall2D.Size();
	const float DistBallToGoal = FVector::Dist2D(BallLoc, GoalLoc);

	const float Now = GetWorld()->GetTimeSeconds();

	// Recovery: brief idle after kick. Still face/move toward ball but don't re-kick.
	if (State == EOpponentState::Recover && Now >= StateExitTime)
	{
		State = EOpponentState::Chase;
	}

	// Decide phase. Suppress kick states while the ball is already in flight —
	// otherwise the AI re-kicks every cycle and the ball jitters.
	const bool bBallInFlight = Ball->GetSpeed2D() > NoKickIfBallSpeedAbove;
	if (State != EOpponentState::Recover)
	{
		if (DistBall < KickRange && !bBallInFlight)
		{
			State = (DistBallToGoal < ShootDistance) ? EOpponentState::Shoot : EOpponentState::Carry;
		}
		else
		{
			State = EOpponentState::Chase;
		}
	}

	// Move regardless of state — except in deep recovery, keep moving toward ball area.
	if (DistBall > 5.0f)
	{
		Pawn->AddMovementInput(ToBall2D.GetSafeNormal(), 1.0f);
	}

	// Sprint when far from ball (chasing), jog otherwise.
	if (UCharacterMovementComponent* Move = Pawn->GetCharacterMovement())
	{
		const bool bShouldSprint = (State == EOpponentState::Chase) && (DistBall > SprintBallDistance);
		Move->MaxWalkSpeed = bShouldSprint ? Pawn->SprintSpeed : Pawn->JogSpeed;
		Pawn->bIsSprinting = bShouldSprint;
	}

	// Kick decisions
	if (State == EOpponentState::Shoot)
	{
		const FVector AimDir = ComputeAimDirection(BallLoc, GoalLoc);
		Ball->ApplyKick(AimDir, ShotStrength, 0.05f);
		State = EOpponentState::Recover;
		StateExitTime = Now + RecoverDuration;
		UE_LOG(LogEdge26, Verbose, TEXT("AI %s shot toward goal."), *Pawn->GetName());
	}
	else if (State == EOpponentState::Carry)
	{
		const FVector AimDir = ComputeAimDirection(BallLoc, GoalLoc);
		Ball->ApplyKick(AimDir, CarryStrength, 0.0f);
		State = EOpponentState::Recover;
		StateExitTime = Now + RecoverDuration;
	}
}

ASoccerBall* AOpponentAIController::FindBall() const
{
	if (CachedBall.IsValid())
	{
		return CachedBall.Get();
	}

	for (TActorIterator<ASoccerBall> It(GetWorld()); It; ++It)
	{
		const_cast<AOpponentAIController*>(this)->CachedBall = *It;
		return *It;
	}
	return nullptr;
}

AGoalTrigger* AOpponentAIController::FindTargetGoal() const
{
	if (CachedGoal.IsValid())
	{
		return CachedGoal.Get();
	}

	const int32 OpposingDefenderId = (TeamId == 0) ? 1 : 0;
	for (TActorIterator<AGoalTrigger> It(GetWorld()); It; ++It)
	{
		AGoalTrigger* G = *It;
		if (G && G->DefendingTeamId == OpposingDefenderId)
		{
			const_cast<AOpponentAIController*>(this)->CachedGoal = G;
			return G;
		}
	}
	return nullptr;
}

FVector AOpponentAIController::ComputeAimDirection(const FVector& BallLoc, const FVector& GoalLoc) const
{
	FVector Dir = (GoalLoc - BallLoc);
	Dir.Z = 0.0f;
	if (!Dir.Normalize())
	{
		return FVector::ForwardVector;
	}

	if (AimNoiseDegrees > 0.0f)
	{
		const float NoiseDeg = FMath::FRandRange(-AimNoiseDegrees, AimNoiseDegrees);
		Dir = Dir.RotateAngleAxis(NoiseDeg, FVector::UpVector);
	}
	return Dir;
}
