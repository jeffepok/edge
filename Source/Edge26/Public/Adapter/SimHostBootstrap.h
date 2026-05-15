// Copyright Edge26. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimHostBootstrap.generated.h"

UCLASS()
class EDGE26_API ASimHostBootstrap : public AActor
{
	GENERATED_BODY()
public:
	ASimHostBootstrap();
protected:
	virtual void BeginPlay() override;
};
