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

	// IMPORTANT: there are two IA_Move assets in the project (/Game/Input/IA_Move
	// and /Game/Input/Actions/IA_Move). IMC_Player binds to the latter. Match it.
	static ConstructorHelpers::FObjectFinder<UInputAction> IA_Move_Finder(
		TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
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

	static ConstructorHelpers::FObjectFinder<UInputAction> IA_Switch_Finder(
		TEXT("/Game/Input/Actions/IA_Switch.IA_Switch"));
	if (IA_Switch_Finder.Succeeded()) IA_Switch = IA_Switch_Finder.Object;
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
		else
		{
			UE_LOG(LogEdge26, Verbose, TEXT("SimInputCollector: Pawn->InputComponent is not UEnhancedInputComponent (pawn not player-possessed?)"));
		}
	}
}

void USimInputCollector::RegisterMappingContext()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;
	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC) return;  // AI-possessed pawns don't add the player mapping context.
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
	if (IA_Switch) Component->BindAction(IA_Switch, ETriggerEvent::Started, this, &USimInputCollector::OnSwitch);
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
	const FVector2D v = Value.Get<FVector2D>();
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

void USimInputCollector::OnSwitch(const FInputActionValue&)
{
	if (auto* H = HostFor(this)) H->SetButton(ControllerIndexOf(this), 1 << 4, true);
}
