// Copyright Edge26. All Rights Reserved.

#include "Game/GoalTrigger.h"

#include "Ball/SoccerBall.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Game/SoccerGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Edge26.h"

AGoalTrigger::AGoalTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(40.0f, 365.0f, 122.0f));
	Trigger->SetCollisionProfileName(TEXT("OverlapAll"));
	Trigger->SetGenerateOverlapEvents(true);

	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(Trigger);
	FrameMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void AGoalTrigger::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGoalTrigger::OnBallEnter);
}

void AGoalTrigger::OnBallEnter(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	ASoccerBall* Ball = Cast<ASoccerBall>(Other);
	if (!Ball)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastTriggerTime < 1.0f)
	{
		return;
	}
	LastTriggerTime = Now;

	const int32 ScoringTeam = (DefendingTeamId == 0) ? 1 : 0;
	UE_LOG(LogEdge26, Log, TEXT("Goal! Team %d scored against %d."), ScoringTeam, DefendingTeamId);

	if (ASoccerGameMode* GM = Cast<ASoccerGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->RegisterGoal(ScoringTeam);
	}
}
