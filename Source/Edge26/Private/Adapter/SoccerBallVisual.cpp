// Copyright Edge26. All Rights Reserved.
#include "Adapter/SoccerBallVisual.h"
#include "Adapter/SimHostSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ASoccerBallVisual::ASoccerBallVisual()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);  // for goal-trigger overlap

	// Default to the engine sphere mesh, scaled to ~22cm diameter (sim BallRadius=11cm).
	// The engine sphere is 100cm diameter, so scale = 0.22.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		Mesh->SetStaticMesh(SphereFinder.Object);
		Mesh->SetRelativeScale3D(FVector(0.22f));
	}
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
