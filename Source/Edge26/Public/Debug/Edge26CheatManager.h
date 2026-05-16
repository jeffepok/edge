// Copyright Edge26. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "Edge26CheatManager.generated.h"

UCLASS()
class EDGE26_API UEdge26CheatManager : public UCheatManager
{
    GENERATED_BODY()
public:
    UFUNCTION(Exec) void edge26_ai_show_field(const FString& Field);
    UFUNCTION(Exec) void edge26_ai_team_perspective(int32 Team);
    UFUNCTION(Exec) void edge26_ai_intent_arrows(int32 On);
    UFUNCTION(Exec) void edge26_ai_offside_lines(int32 On);
};
