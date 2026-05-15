// Copyright Edge26. All Rights Reserved.
#include "Debug/Edge26CheatManager.h"
#include "Debug/AIDebugRenderer.h"
#include "EngineUtils.h"
#include "Engine/World.h"

static AAIDebugRenderer* FindRenderer(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AAIDebugRenderer> It(World); It; ++It) return *It;
    // None present → spawn one.
    FActorSpawnParameters params;
    return World->SpawnActor<AAIDebugRenderer>(params);
}

void UEdge26CheatManager::edge26_ai_show_field(const FString& Field)
{
    auto* R = FindRenderer(GetWorld());
    if (!R) return;
    if      (Field == TEXT("None"))          R->ActiveField = ESpatialFieldDebug::None;
    else if (Field == TEXT("Space"))         R->ActiveField = ESpatialFieldDebug::Space;
    else if (Field == TEXT("DefCoverage"))   R->ActiveField = ESpatialFieldDebug::DefCoverage;
    else if (Field == TEXT("Lane"))          R->ActiveField = ESpatialFieldDebug::Lane;
    else if (Field == TEXT("PassReception")) R->ActiveField = ESpatialFieldDebug::PassReception;
    else if (Field == TEXT("Threat"))        R->ActiveField = ESpatialFieldDebug::Threat;
    else UE_LOG(LogTemp, Warning, TEXT("edge26.ai.show_field: unknown field '%s'"), *Field);
}

void UEdge26CheatManager::edge26_ai_team_perspective(int32 Team)
{
    if (auto* R = FindRenderer(GetWorld())) R->TeamPerspective = Team;
}

void UEdge26CheatManager::edge26_ai_intent_arrows(int32 On)
{
    if (auto* R = FindRenderer(GetWorld())) R->bShowIntentArrows = (On != 0);
}

void UEdge26CheatManager::edge26_ai_offside_lines(int32 On)
{
    if (auto* R = FindRenderer(GetWorld())) R->bShowOffsideLines = (On != 0);
}
