// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PitchMarkings.generated.h"

class UStaticMesh;
class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * Procedurally generates standard football pitch markings (touchlines, goal lines,
 * halfway line, centre circle/spot, penalty areas, goal areas, penalty spots) from
 * scaled cube static meshes.
 *
 * All dimensions are FIFA-regulation defaults (105m x 68m) but each is editable in
 * the Details panel. Markings are built on BeginPlay and parented to the actor so
 * moving the actor moves the whole pitch.
 *
 * Auto-spawned by AFootballerCharacter on first BeginPlay if no instance exists.
 */
UCLASS()
class EDGE26_API APitchMarkings : public AActor
{
	GENERATED_BODY()

public:
	APitchMarkings();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	// ===== Pitch dimensions (cm) — FIFA regulations =====

	UPROPERTY(EditAnywhere, Category = "Pitch")
	float PitchLength = 10500.0f;   // 105 m, along Y

	UPROPERTY(EditAnywhere, Category = "Pitch")
	float PitchWidth = 6800.0f;     // 68 m, along X

	UPROPERTY(EditAnywhere, Category = "Pitch")
	float CenterCircleRadius = 915.0f;  // 9.15 m

	UPROPERTY(EditAnywhere, Category = "Pitch")
	float PenaltyAreaDepth = 1650.0f;   // 16.5 m

	UPROPERTY(EditAnywhere, Category = "Pitch")
	float PenaltyAreaWidth = 4032.0f;   // 40.32 m

	UPROPERTY(EditAnywhere, Category = "Pitch")
	float GoalAreaDepth = 550.0f;       // 5.5 m

	UPROPERTY(EditAnywhere, Category = "Pitch")
	float GoalAreaWidth = 1832.0f;      // 18.32 m

	UPROPERTY(EditAnywhere, Category = "Pitch")
	float PenaltySpotDistance = 1100.0f; // 11 m from goal line

	/**
	 * If true, on BeginPlay the actor finds every AGoalTrigger in the level and snaps
	 * its location to the goal line at the matching end (DefendingTeamId 0 -> -Y end,
	 * 1 -> +Y end). Rotation is left alone so the goal frame's facing in BP isn't
	 * disturbed. Toggle off if you want to place goals manually.
	 */
	UPROPERTY(EditAnywhere, Category = "Pitch")
	bool bAlignGoalsToMarkings = true;

	/** Distance behind the goal line to place each goal so the trigger box sits over the line. */
	UPROPERTY(EditAnywhere, Category = "Pitch")
	float GoalBackOffset = 40.0f;

	// ===== Visual =====

	UPROPERTY(EditAnywhere, Category = "Pitch|Style")
	float LineThickness = 12.0f;   // 12 cm

	UPROPERTY(EditAnywhere, Category = "Pitch|Style")
	float LineHeightOffset = 1.0f; // sit just above grass

	UPROPERTY(EditAnywhere, Category = "Pitch|Style")
	float LineProfileHeight = 4.0f; // small Z extent so lines aren't paper-thin from low cameras

	UPROPERTY(EditAnywhere, Category = "Pitch|Style", meta = (ClampMin = "8", ClampMax = "256"))
	int32 CircleSegments = 48;

	UPROPERTY(EditAnywhere, Category = "Pitch|Style")
	float SpotSize = 24.0f;

	UPROPERTY(EditAnywhere, Category = "Pitch|Style")
	FLinearColor LineColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Pitch|Style")
	TObjectPtr<UStaticMesh> LineMesh;

	UPROPERTY(EditAnywhere, Category = "Pitch|Style")
	TObjectPtr<UMaterialInterface> LineMaterial;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent> Root;

	/** All line segments share this single instanced component — one draw call for the whole pitch. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> Lines;

private:
	void BuildMarkings();
	void AlignGoals();

	void AddLineSegment(const FVector& LocalCenter, const FVector& Size, float YawDeg = 0.0f);

	void AddCircle(const FVector& Center, float Radius);
	void AddDot(const FVector& Center, float Size);
};
