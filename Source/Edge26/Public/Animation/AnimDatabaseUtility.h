// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimDatabaseUtility.generated.h"

class UPoseSearchSchema;
class UPoseSearchDatabase;
class UAnimSequence;
class USkeleton;

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
};
