// Copyright Edge26. All Rights Reserved.

#include "Game/SoccerHUD.h"

#include "Adapter/SoccerBallVisual.h"
#include "Adapter/SimHostSubsystem.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "Game/SoccerGameMode.h"
#include "Kismet/GameplayStatics.h"

void ASoccerHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	ASoccerGameMode* GM = Cast<ASoccerGameMode>(UGameplayStatics::GetGameMode(this));
	if (!GM)
	{
		return;
	}

	DrawScoreline(GM);
	DrawClock(GM);
	DrawPhaseBanner(GM);

	// Ball debug HUD: position from the sim. Velocity isn't exposed via the
	// adapter yet — a follow-up can add a SimHost::GetBallVelocity() accessor.
	for (TActorIterator<ASoccerBallVisual> It(GetWorld()); It; ++It)
	{
		ASoccerBallVisual* Ball = *It;
		const FVector Vel = FVector::ZeroVector;  // placeholder; sim velocity readout TBD
		const FVector Loc = Ball->GetActorLocation();
		const FString Line1 = FString::Printf(TEXT("BALL  speed=%.0f  v=(%+5.0f, %+5.0f, %+5.0f)"),
			Vel.Size(), Vel.X, Vel.Y, Vel.Z);
		const FString Line2 = FString::Printf(TEXT("      pos=(%+6.0f, %+6.0f, %+5.0f)"),
			Loc.X, Loc.Y, Loc.Z);

		UFont* Font = GEngine->GetSmallFont();
		FCanvasTextItem T1(FVector2D(20.0f, Canvas->SizeY - 60.0f), FText::FromString(Line1), Font, FLinearColor(1.0f, 1.0f, 0.5f));
		T1.Scale = FVector2D(1.1f, 1.1f);
		Canvas->DrawItem(T1);

		FCanvasTextItem T2(FVector2D(20.0f, Canvas->SizeY - 40.0f), FText::FromString(Line2), Font, FLinearColor(0.9f, 0.9f, 0.9f));
		T2.Scale = FVector2D(1.1f, 1.1f);
		Canvas->DrawItem(T2);
		break;
	}
}

void ASoccerHUD::DrawScoreline(ASoccerGameMode* GM)
{
	const float CenterX = Canvas->SizeX * 0.5f;
	const float TopY = 20.0f;
	const float PanelW = 280.0f;
	const float PanelH = 56.0f;

	FCanvasTileItem Panel(FVector2D(CenterX - PanelW * 0.5f, TopY), GWhiteTexture, FVector2D(PanelW, PanelH), PanelColor);
	Panel.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Panel);

	UFont* Font = GEngine->GetLargeFont();
	const FString Score = FString::Printf(TEXT("%s  %d  -  %d  %s"), *HomeName, GM->GetScore(0), GM->GetScore(1), *AwayName);
	FCanvasTextItem Text(FVector2D(CenterX, TopY + PanelH * 0.5f), FText::FromString(Score), Font, TextColor);
	Text.bCentreX = true;
	Text.bCentreY = true;
	Text.Scale = FVector2D(1.4f, 1.4f);
	Canvas->DrawItem(Text);
}

void ASoccerHUD::DrawClock(ASoccerGameMode* GM)
{
	const float Remaining = FMath::Max(0.0f, GM->GetMatchTimeRemaining());
	const int32 Mins = FMath::FloorToInt(Remaining / 60.0f);
	const int32 Secs = FMath::FloorToInt(Remaining) % 60;

	const FString Time = FString::Printf(TEXT("%02d:%02d"), Mins, Secs);
	UFont* Font = GEngine->GetSmallFont();
	FCanvasTextItem Text(FVector2D(Canvas->SizeX * 0.5f, 80.0f), FText::FromString(Time), Font, TextColor);
	Text.bCentreX = true;
	Text.Scale = FVector2D(1.3f, 1.3f);
	Canvas->DrawItem(Text);
}

void ASoccerHUD::DrawPhaseBanner(ASoccerGameMode* GM)
{
	const EMatchPhase Phase = GM->GetMatchPhase();
	if (Phase == EMatchPhase::Playing)
	{
		return;
	}

	FString Banner;
	FLinearColor Color = FLinearColor::White;
	switch (Phase)
	{
		case EMatchPhase::Kickoff:   Banner = TEXT("KICKOFF"); break;
		case EMatchPhase::GoalCeleb: Banner = TEXT("GOAL!");   Color = FLinearColor(1.0f, 0.85f, 0.3f); break;
		case EMatchPhase::FullTime:  Banner = TEXT("FULL TIME"); break;
		default: return;
	}

	UFont* Font = GEngine->GetLargeFont();
	FCanvasTextItem Text(FVector2D(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.35f), FText::FromString(Banner), Font, Color);
	Text.bCentreX = true;
	Text.Scale = FVector2D(3.0f, 3.0f);
	Canvas->DrawItem(Text);
}
