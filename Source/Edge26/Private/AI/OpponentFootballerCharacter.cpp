// Copyright Edge26. All Rights Reserved.

#include "AI/OpponentFootballerCharacter.h"

#include "AI/OpponentAIController.h"

AOpponentFootballerCharacter::AOpponentFootballerCharacter()
{
	AIControllerClass = AOpponentAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}
