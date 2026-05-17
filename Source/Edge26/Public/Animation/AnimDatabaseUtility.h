// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimDatabaseUtility.generated.h"

class UPoseSearchSchema;
class UPoseSearchDatabase;
class UAnimSequence;
class USkeleton;
class UAnimBlueprint;

/**
 * Python/Blueprint-callable helpers for populating UPoseSearchDatabase assets
 * headlessly (editor-only operations guarded with WITH_EDITOR).
 *
 * Intended use: headless Python scripts run via UnrealEditor-Cmd to automate
 * Motion Matching database population without manual editor steps.
 */
UCLASS()
class EDGE26_API UAnimDatabaseUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates a new UPoseSearchSchema asset at <PackagePath>/<AssetName> with:
	 *   - The given Skeleton assigned via AddSkeleton()
	 *   - SampleRate = 30
	 *   - Default channels (Trajectory + Pose) via AddDefaultChannels()
	 * Saves the asset and returns it. Returns nullptr on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Edge26 Anim")
	static UPoseSearchSchema* CreateSchemaWithDefaultChannels(
		USkeleton* Skeleton,
		const FString& PackagePath,
		const FString& AssetName);

	/**
	 * Sets DB->Schema = Schema (direct UPROPERTY write, public on UPoseSearchDatabase).
	 */
	UFUNCTION(BlueprintCallable, Category = "Edge26 Anim")
	static void SetDatabaseSchema(UPoseSearchDatabase* DB, UPoseSearchSchema* Schema);

	/**
	 * Wraps Sequence in a FPoseSearchDatabaseAnimationAsset and appends it to DB
	 * via the public UPoseSearchDatabase::AddAnimationAsset() API.
	 */
	UFUNCTION(BlueprintCallable, Category = "Edge26 Anim")
	static void AddSequenceToDatabase(UPoseSearchDatabase* DB, UAnimSequence* Sequence, bool bLooping);

	/**
	 * Saves DB's package to disk via UEditorAssetLibrary / UPackage facilities.
	 */
	UFUNCTION(BlueprintCallable, Category = "Edge26 Anim")
	static bool SaveDatabaseAsset(UPoseSearchDatabase* DB);

	/**
	 * Wires the AnimGraph of an existing UAnimBlueprint to run Motion Matching.
	 * Spawns a UAnimGraphNode_MotionMatching (or reuses an existing one — the
	 * call is idempotent) and connects its Pose output to the existing Output
	 * Pose (UAnimGraphNode_Root) input. Marks the BP structurally modified and
	 * compiles it.
	 *
	 * Note on the Database parameter: it is accepted for API completeness but
	 * not written by this C++ helper. The MotionMatching node's Database is
	 * declared `PinShownByDefault`, so the runtime value lives on the pin
	 * default rather than the struct field; assigning the FProperty directly
	 * does not survive serialization. The companion Python script writes
	 * Database via `set_editor_property` on the returned node struct, which
	 * goes through the full property-set pipeline including pin defaults.
	 *
	 * The wired graph is the minimal pose chain: MotionMatching -> Root.
	 * Trajectory data is consumed at runtime via the schema's feature channels
	 * from the AnimInstance's inherited PoseHistory state; no explicit
	 * trajectory pin wiring is required (the MotionMatching node does not
	 * expose trajectory as a pin).
	 *
	 * Returns true on success. Callers should still save the package.
	 */
	UFUNCTION(BlueprintCallable, Category = "Edge26 Anim")
	static bool WireMotionMatchingAnimGraph(
		UAnimBlueprint* AnimBP,
		UPoseSearchDatabase* Database);

	/**
	 * Saves the package containing AnimBP. Returns true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Edge26 Anim")
	static bool SaveAnimBlueprintAsset(UAnimBlueprint* AnimBP);
};
