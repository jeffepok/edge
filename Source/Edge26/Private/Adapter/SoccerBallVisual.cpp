// Copyright Edge26. All Rights Reserved.
#include "Adapter/SoccerBallVisual.h"
#include "Adapter/SimHostSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ASoccerBallVisual::ASoccerBallVisual()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);  // for goal-trigger overlap
}

void ASoccerBallVisual::BeginPlay()
{
	Super::BeginPlay();
	if (auto* World = GetWorld())
	{
		if (auto* Host = World->GetSubsystem<USimHostSubsystem>())
		{
			Host->RegisterBall(this);
		}
	}
}

void ASoccerBallVisual::DriveFromSim(const FTransform& InterpolatedTransform)
{
	SetActorTransform(InterpolatedTransform);
}
