// Copyright Edge26. All Rights Reserved.

#include "Animation/AnimDatabaseUtility.h"

#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "PackageHelperFunctions.h"
#endif // WITH_EDITOR

// -----------------------------------------------------------------------
// CreateSchemaWithDefaultChannels
// -----------------------------------------------------------------------
UPoseSearchSchema* UAnimDatabaseUtility::CreateSchemaWithDefaultChannels(
	USkeleton* Skeleton,
	const FString& PackagePath,
	const FString& AssetName)
{
#if WITH_EDITOR
	if (!Skeleton)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::CreateSchemaWithDefaultChannels — Skeleton is null"));
		return nullptr;
	}

	// Build full package name, e.g. /Game/Animation/MotionMatching/MMSchema_Outfield
	const FString PackageName = PackagePath / AssetName;

	// Check whether the asset already exists; if so load and return it.
	UPackage* Pkg = FindPackage(nullptr, *PackageName);
	if (!Pkg)
	{
		Pkg = LoadPackage(nullptr, *PackageName, LOAD_NoWarn | LOAD_Quiet);
	}
	if (Pkg)
	{
		UPoseSearchSchema* Existing = FindObject<UPoseSearchSchema>(Pkg, *AssetName);
		if (Existing)
		{
			UE_LOG(LogAnimation, Log, TEXT("AnimDatabaseUtility: Schema already exists at %s, returning existing asset"), *PackageName);
			return Existing;
		}
	}

	// Create a new package + asset.
	Pkg = CreatePackage(*PackageName);
	check(Pkg);
	Pkg->FullyLoad();

	UPoseSearchSchema* Schema = NewObject<UPoseSearchSchema>(
		Pkg,
		*AssetName,
		RF_Public | RF_Standalone | RF_Transactional);

	if (!Schema)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility: Failed to create UPoseSearchSchema at %s"), *PackageName);
		return nullptr;
	}

	// Assign skeleton (using the public API).
	Schema->AddSkeleton(Skeleton);

	// Set sample rate (public UPROPERTY).
	Schema->SampleRate = 30;

	// Add default channels: Trajectory + Pose (calls Finalize internally).
	Schema->AddDefaultChannels();

	// Notify asset registry.
	FAssetRegistryModule::AssetCreated(Schema);

	// Mark dirty so it gets saved.
	Pkg->MarkPackageDirty();

	// Save.
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	// SAVE_BulkDataByReference marks this as a procedural (non-interactive) save,
	// which skips async DDC rebuild triggered by PostSaveRoot on UPoseSearchDatabase.
	SaveArgs.SaveFlags = SAVE_NoError | SAVE_BulkDataByReference;
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(Pkg, Schema, *PackageFilename, SaveArgs);

	UE_LOG(LogAnimation, Log, TEXT("AnimDatabaseUtility: Created schema %s"), *PackageName);
	return Schema;

#else
	UE_LOG(LogAnimation, Warning, TEXT("AnimDatabaseUtility::CreateSchemaWithDefaultChannels is an editor-only operation."));
	return nullptr;
#endif // WITH_EDITOR
}

// -----------------------------------------------------------------------
// SetDatabaseSchema
// -----------------------------------------------------------------------
void UAnimDatabaseUtility::SetDatabaseSchema(UPoseSearchDatabase* DB, UPoseSearchSchema* Schema)
{
	if (!DB)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::SetDatabaseSchema — DB is null"));
		return;
	}
	// Schema is a public UPROPERTY on UPoseSearchDatabase; direct assignment is valid.
	DB->Schema = Schema;
	DB->MarkPackageDirty();
}

// -----------------------------------------------------------------------
// AddSequenceToDatabase
// -----------------------------------------------------------------------
void UAnimDatabaseUtility::AddSequenceToDatabase(UPoseSearchDatabase* DB, UAnimSequence* Sequence, bool bLooping)
{
	if (!DB)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::AddSequenceToDatabase — DB is null"));
		return;
	}
	if (!Sequence)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::AddSequenceToDatabase — Sequence is null"));
		return;
	}

	// UPoseSearchDatabase::AddAnimationAsset(const FPoseSearchDatabaseAnimationAsset&)
	// is a public, non-deprecated API in UE5.7. AnimAsset accepts any UObject*.
	FPoseSearchDatabaseAnimationAsset Entry;
	Entry.AnimAsset = Sequence;
	// bLooping is advisory in FPoseSearchDatabaseAnimationAsset (derived from IsLooping()
	// on the anim asset itself). The struct doesn't have a direct bLooping field;
	// looping is determined at index time from the sequence's looping property.
	// We still mark the entry enabled.
#if WITH_EDITORONLY_DATA
	Entry.bEnabled = true;
#endif

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DB->AddAnimationAsset(Entry);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	DB->MarkPackageDirty();

	UE_LOG(LogAnimation, Log, TEXT("AnimDatabaseUtility: Added %s to %s"),
		*Sequence->GetName(), *DB->GetName());
}

// -----------------------------------------------------------------------
// SaveDatabaseAsset
// -----------------------------------------------------------------------
bool UAnimDatabaseUtility::SaveDatabaseAsset(UPoseSearchDatabase* DB)
{
	if (!DB)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::SaveDatabaseAsset — DB is null"));
		return false;
	}

#if WITH_EDITOR
	UPackage* Pkg = DB->GetOutermost();
	if (!Pkg)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::SaveDatabaseAsset — could not get package for %s"), *DB->GetName());
		return false;
	}

	const FString PackageName  = Pkg->GetName();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	// SAVE_BulkDataByReference marks this as a procedural (non-interactive) save.
	// This sets IsProceduralSave()=true on FObjectPostSaveRootContext, which causes
	// UPoseSearchDatabase::PostSaveRoot to skip RequestAsyncBuildIndex — otherwise
	// it tries to build the DDC index immediately, crashing on uncompressed anim data.
	// The database will be (re-)indexed the first time it is opened in the full editor.
	SaveArgs.SaveFlags = SAVE_NoError | SAVE_BulkDataByReference;
	const bool bSuccess = UPackage::SavePackage(Pkg, DB, *PackageFilename, SaveArgs);

	if (bSuccess)
	{
		UE_LOG(LogAnimation, Log, TEXT("AnimDatabaseUtility: Saved %s"), *PackageName);
	}
	else
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility: Failed to save %s"), *PackageName);
	}
	return bSuccess;

#else
	UE_LOG(LogAnimation, Warning, TEXT("AnimDatabaseUtility::SaveDatabaseAsset is an editor-only operation."));
	return false;
#endif // WITH_EDITOR
}
