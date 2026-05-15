// Copyright Edge26. All Rights Reserved.
#include "Adapter/SimInputCollector.h"
#include "Adapter/SimHostSubsystem.h"
#include "Adapter/FootballerVisual.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Edge26.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

USimInputCollector::USimInputCollector()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Load default IMC + IA assets from /Game/Input/ so a fresh BP works without
	// manual wiring. BP subclasses can override these in the Details panel.
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCFinder(
		TEXT("/Game/Input/IMC_Player.IMC_Player"));
	if (IMCFinder.Succeeded()) DefaultMappingContext = IMCFinder.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> IA_Move_Finder(
		TEXT("/Game/Input/IA_Move.IA_Move"));
	if (IA_Move_Finder.Succeeded()) IA_Move = IA_Move_Finder.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> IA_Sprint_Finder(
		TEXT("/Game/Input/IA_Sprint.IA_Sprint"));
	if (IA_Sprint_Finder.Succeeded()) IA_Sprint = IA_Sprint_Finder.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> IA_Pass_Finder(
		TEXT("/Game/Input/IA_Pass.IA_Pass"));
	if (IA_Pass_Finder.Succeeded()) IA_Pass = IA_Pass_Finder.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> IA_Shoot_Finder(
		TEXT("/Game/Input/IA_Shoot.IA_Shoot"));
	if (IA_Shoot_Finder.Succeeded()) IA_Shoot = IA_Shoot_Finder.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> IA_Chip_Finder(
		TEXT("/Game/Input/IA_Chip.IA_Chip"));
	if (IA_Chip_Finder.Succeeded()) IA_Chip = IA_Chip_Finder.Object;
}

void USimInputCollector::BeginPlay()
{
	Super::BeginPlay();

	APawn* Pawn = Cast<APawn>(GetOwner());
	UE_LOG(LogEdge26, Warning, TEXT("SimInputCollector::BeginPlay owner=%s controller=%s IMC=%s IA_Move=%s"),
		Pawn ? *Pawn->GetName() : TEXT("null"),
		Pawn && Pawn->GetController() ? *Pawn->GetController()->GetName() : TEXT("null"),
		DefaultMappingContext ? *DefaultMappingContext->GetName() : TEXT("null"),
		IA_Move ? *IA_Move->GetName() : TEXT("null"));

	RegisterMappingContext();

	if (Pawn)
	{
		UE_LOG(LogEdge26, Warning, TEXT("SimInputCollector pawn InputComponent=%s"),
			Pawn->InputComponent ? *Pawn->InputComponent->GetClass()->GetName() : TEXT("null"));
		if (auto* Input = Cast<UEnhancedInputComponent>(Pawn->InputComponent))
		{
			Bind(Input);
		}
		else
		{
			UE_LOG(LogEdge26, Error, TEXT("SimInputCollector: Pawn->InputComponent is not UEnhancedInputComponent — input won't bind"));
		}
	}
}

void USimInputCollector::RegisterMappingContext()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) { UE_LOG(LogEdge26, Error, TEXT("RegisterMappingContext: owner not a pawn")); return; }
	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC) { UE_LOG(LogEdge26, Error, TEXT("RegisterMappingContext: pawn has no PlayerController (yet)")); return; }
	if (auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Sub->AddMappingContext(DefaultMappingContext, 0);
			UE_LOG(LogEdge26, Warning, TEXT("RegisterMappingContext: added IMC %s"), *DefaultMappingContext->GetName());
		}
		else
		{
			UE_LOG(LogEdge26, Error, TEXT("RegisterMappingContext: DefaultMappingContext is null"));
		}
	}
	else
	{
		UE_LOG(LogEdge26, Error, TEXT("RegisterMappingContext: no EnhancedInputLocalPlayerSubsystem"));
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
	const int32 idx = ControllerIndexOf(this);
	// Diagnostic — temporary. Bump back to VeryVerbose once input is confirmed working.
	UE_LOG(LogEdge26, Warning, TEXT("OnMove idx=%d v=(%.3f, %.3f)"), idx, v.X, v.Y);
	if (auto* H = HostFor(this)) H->SetMoveInput(idx, v);
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
