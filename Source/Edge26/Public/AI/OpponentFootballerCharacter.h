// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/FootballerCharacter.h"
#include "OpponentFootballerCharacter.generated.h"

/**
 * Drop-in opponent: behaves identically to AFootballerCharacter but auto-spawns
 * an AOpponentAIController on placement. Subclass in BP if you want to swap
 * meshes, materials, or per-team kit.
 */
UCLASS()
class EDGE26_API AOpponentFootballerCharacter : public AFootballerCharacter
{
	GENERATED_BODY()

public:
	AOpponentFootballerCharacter();
};
