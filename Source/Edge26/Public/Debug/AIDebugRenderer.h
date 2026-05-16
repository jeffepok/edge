// Copyright Edge26. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIDebugRenderer.generated.h"

UENUM(BlueprintType)
enum class ESpatialFieldDebug : uint8
{
    None          UMETA(DisplayName="None"),
    Space         UMETA(DisplayName="Space"),
    DefCoverage   UMETA(DisplayName="DefCoverage"),
    Lane          UMETA(DisplayName="LaneOccupancy"),
    PassReception UMETA(DisplayName="PassReception"),
    Threat        UMETA(DisplayName="Threat"),
};

UCLASS()
class EDGE26_API AAIDebugRenderer : public AActor
{
    GENERATED_BODY()

public:
    AAIDebugRenderer();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category="AI Debug")
    ESpatialFieldDebug ActiveField = ESpatialFieldDebug::None;

    UPROPERTY(EditAnywhere, Category="AI Debug")
    int32 TeamPerspective = 0;       // 0 = home, 1 = away, -1 = combined

    UPROPERTY(EditAnywhere, Category="AI Debug")
    bool bShowIntentArrows = false;

    UPROPERTY(EditAnywhere, Category="AI Debug")
    bool bShowOffsideLines = false;
};
