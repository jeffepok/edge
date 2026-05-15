// Copyright Edge26. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerBallVisual.generated.h"

class UStaticMeshComponent;

UCLASS()
class EDGE26_API ASoccerBallVisual : public AActor
{
	GENERATED_BODY()

public:
	ASoccerBallVisual();
	void DriveFromSim(const FTransform& InterpolatedTransform);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent> Mesh;
};
