// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "BroadcastSpringArmComponent.generated.h"

/**
 * Spring arm tuned for football broadcast feel:
 *   - High pitch (looking down at the player).
 *   - Speed-driven dynamic zoom: arm length lerps between Base and Sprint.
 *   - Cinematic lag (location + rotation) so the camera feels weighted.
 *   - Mild upward boom so the player frames lower in screen, leaving sky/pitch visible.
 *
 * Drop-in: replace USpringArmComponent on the pawn with this class.
 * Tuning lives on the component so designers can override per-character.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class EDGE26_API UBroadcastSpringArmComponent : public USpringArmComponent
{
	GENERATED_BODY()

public:
	UBroadcastSpringArmComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Broadcast|Distance")
	float BaseArmLength = 620.0f;

	UPROPERTY(EditAnywhere, Category = "Broadcast|Distance")
	float SprintArmLength = 980.0f;

	/** Speed at which arm length reaches SprintArmLength (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Broadcast|Distance", meta = (ClampMin = "1.0"))
	float SpeedForFullZoomOut = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Broadcast|Distance")
	float ArmInterpSpeed = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Broadcast|Pose")
	FRotator BasePitchYawRoll = FRotator(-18.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "Broadcast|Pose")
	FVector BaseSocketOffset = FVector(0.0f, 0.0f, 80.0f);

	/** Lateral lead-camera offset proportional to horizontal velocity (cm per cm/s). */
	UPROPERTY(EditAnywhere, Category = "Broadcast|Lead")
	float LeadOffsetPerSpeed = 0.06f;

	UPROPERTY(EditAnywhere, Category = "Broadcast|Lead")
	float MaxLeadOffset = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Broadcast|Lead")
	float LeadInterpSpeed = 3.5f;

private:
	float CurrentLead = 0.0f;
};
