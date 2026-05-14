// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SoccerHUD.generated.h"

class ASoccerGameMode;

/**
 * Minimal canvas-drawn HUD: scoreline, clock, and match phase banner.
 * Replace with a UMG widget when visual polish is needed; the GameMode
 * already broadcasts OnGoalScored / OnPhaseChanged for binding.
 */
UCLASS()
class EDGE26_API ASoccerHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FLinearColor PanelColor = FLinearColor(0.05f, 0.06f, 0.08f, 0.78f);

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FLinearColor TextColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FString HomeName = TEXT("HOM");

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FString AwayName = TEXT("AWY");

protected:
	void DrawScoreline(ASoccerGameMode* GM);
	void DrawClock(ASoccerGameMode* GM);
	void DrawPhaseBanner(ASoccerGameMode* GM);
};
