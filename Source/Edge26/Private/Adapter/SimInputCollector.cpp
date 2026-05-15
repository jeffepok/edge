// Copyright Edge26. All Rights Reserved.
#include "Adapter/SimInputCollector.h"
#include "Adapter/SimHostSubsystem.h"
#include "Adapter/FootballerVisual.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"

USimInputCollector::USimInputCollector()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USimInputCollector::BeginPlay()
{
	Super::BeginPlay();
	RegisterMappingContext();

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (auto* Input = Cast<UEnhancedInputComponent>(Pawn->InputComponent))
		{
			Bind(Input);
		}
	}
}

void USimInputCollector::RegisterMappingContext()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;
	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC) return;
	if (auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Sub->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void USimInputCollector::Bind(UEnhancedInputComponent* Component)
{
	if (!Component) return;
	if (IA_Move)
	{
		Component->BindAction(IA_Move, ETriggerEvent::Triggered, this, &USimInputCollector::OnMove);
		Component->BindAction(IA_Move, ETriggerEvent::Completed, this, &USimInputCollector::OnMove);
	}
	if (IA_Sprint)
	{
		Component->BindAction(IA_Sprint, ETriggerEvent::Started,   this, &USimInputCollector::OnSprint);
		Component->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &USimInputCollector::OnSprintReleased);
	}
	if (IA_Pass)   Component->BindAction(IA_Pass,  ETriggerEvent::Started, this, &USimInputCollector::OnPass);
	if (IA_Shoot)  Component->BindAction(IA_Shoot, ETriggerEvent::Started, this, &USimInputCollector::OnShoot);
	if (IA_Chip)   Component->BindAction(IA_Chip,  ETriggerEvent::Started, this, &USimInputCollector::OnChip);
}

static USimHostSubsystem* HostFor(const UActorComponent* Self)
{
	UWorld* World = Self ? Self->GetWorld() : nullptr;
	return World ? World->GetSubsystem<USimHostSubsystem>() : nullptr;
}

static int32 ControllerIndexOf(const UActorComponent* Self)
{
	if (!Self) return -1;
	if (auto* F = Cast<AFootballerVisual>(Self->GetOwner())) return F->ControllerIndex;
	return -1;
}

void USimInputCollector::OnMove(const FInputActionValue& Value)
{
	FVector2D v = Value.Get<FVector2D>();
	if (auto* H = HostFor(this)) H->SetMoveInput(ControllerIndexOf(this), v);
}

void USimInputCollector::OnSprint(const FInputActionValue&)
{
	if (auto* H = HostFor(this)) H->SetButton(ControllerIndexOf(this), 1 << 0, true);
}
void USimInputCollector::OnSprintReleased(const FInputActionValue&)
{
	if (auto* H = HostFor(this)) H->SetButton(ControllerIndexOf(this), 1 << 0, false);
}
void USimInputCollector::OnPass(const FInputActionValue&)
{
	if (auto* H = HostFor(this)) H->SetButton(ControllerIndexOf(this), 1 << 1, true);
}
void USimInputCollector::OnShoot(const FInputActionValue&)
{
	if (auto* H = HostFor(this)) H->SetButton(ControllerIndexOf(this), 1 << 2, true);
}
void USimInputCollector::OnChip(const FInputActionValue&)
{
	if (auto* H = HostFor(this)) H->SetButton(ControllerIndexOf(this), 1 << 3, true);
}
