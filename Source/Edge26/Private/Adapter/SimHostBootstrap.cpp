// Copyright Edge26. All Rights Reserved.
#include "Adapter/SimHostBootstrap.h"
#include "Adapter/SimHostSubsystem.h"
#include "Engine/World.h"

ASimHostBootstrap::ASimHostBootstrap()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASimHostBootstrap::BeginPlay()
{
	Super::BeginPlay();
	if (auto* World = GetWorld())
	{
		// Touching the subsystem ensures it's initialized.
		(void)World->GetSubsystem<USimHostSubsystem>();
	}
}
