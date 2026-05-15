// Copyright Edge26. All Rights Reserved.
// ActorComponent that translates Enhanced Input on a Footballer pawn into
// SimHostSubsystem::SetMoveInput / SetButton calls.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimInputCollector.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS(ClassGroup=(Edge26), meta=(BlueprintSpawnableComponent))
class EDGE26_API USimInputCollector : public UActorComponent
{
	GENERATED_BODY()

public:
	USimInputCollector();

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Sprint;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Pass;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Shoot;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Chip;

	void Bind(class UEnhancedInputComponent* Component);

protected:
	virtual void BeginPlay() override;

	void OnMove(const FInputActionValue& Value);
	void OnSprint(const FInputActionValue& Value);
	void OnSprintReleased(const FInputActionValue& Value);
	void OnPass(const FInputActionValue& Value);
	void OnShoot(const FInputActionValue& Value);
	void OnChip(const FInputActionValue& Value);

	void RegisterMappingContext();
};
