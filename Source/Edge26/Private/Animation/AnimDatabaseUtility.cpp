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

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimNode_Root.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_MotionMatching.h"
#include "AnimGraphNode_TwoBoneIK.h"
#include "BoneControllers/AnimNode_TwoBoneIK.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Animation/AnimTypes.h"
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

// -----------------------------------------------------------------------
// WireMotionMatchingAnimGraph
// -----------------------------------------------------------------------
bool UAnimDatabaseUtility::WireMotionMatchingAnimGraph(
	UAnimBlueprint* AnimBP,
	UPoseSearchDatabase* Database)
{
#if WITH_EDITOR
	if (!AnimBP)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::WireMotionMatchingAnimGraph — AnimBP is null"));
		return false;
	}
	if (!Database)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::WireMotionMatchingAnimGraph — Database is null"));
		return false;
	}

	// 1. Find the AnimGraph (the canonical "AnimGraph" UEdGraph on this blueprint).
	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* G : AnimBP->FunctionGraphs)
	{
		if (G && G->GetFName() == TEXT("AnimGraph"))
		{
			AnimGraph = G;
			break;
		}
	}
	if (!AnimGraph)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::WireMotionMatchingAnimGraph — AnimGraph not found on %s"), *AnimBP->GetName());
		return false;
	}

	// 2. Find the existing Root node (created by AnimationGraphSchema::CreateDefaultNodesForGraph).
	UAnimGraphNode_Root* RootNode = nullptr;
	for (UEdGraphNode* N : AnimGraph->Nodes)
	{
		if (UAnimGraphNode_Root* AsRoot = Cast<UAnimGraphNode_Root>(N))
		{
			RootNode = AsRoot;
			break;
		}
	}
	if (!RootNode)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::WireMotionMatchingAnimGraph — no AnimGraphNode_Root in AnimGraph"));
		return false;
	}

	// 3. Find or spawn a MotionMatching node. Idempotent.
	UAnimGraphNode_MotionMatching* MMNode = nullptr;
	for (UEdGraphNode* N : AnimGraph->Nodes)
	{
		if (UAnimGraphNode_MotionMatching* AsMM = Cast<UAnimGraphNode_MotionMatching>(N))
		{
			MMNode = AsMM;
			break;
		}
	}
	if (!MMNode)
	{
		FGraphNodeCreator<UAnimGraphNode_MotionMatching> NodeCreator(*AnimGraph);
		MMNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		NodeCreator.Finalize();

		// Lay out so the MM node sits to the left of the Root.
		MMNode->NodePosX = RootNode->NodePosX - 360;
		MMNode->NodePosY = RootNode->NodePosY;

		UE_LOG(LogAnimation, Log, TEXT("AnimDatabaseUtility: Spawned UAnimGraphNode_MotionMatching in %s"), *AnimBP->GetName());
	}
	else
	{
		UE_LOG(LogAnimation, Log, TEXT("AnimDatabaseUtility: Reusing existing UAnimGraphNode_MotionMatching in %s"), *AnimBP->GetName());
	}

	// 4. The MotionMatching node's Database field is private on both
	//    UAnimGraphNode_MotionMatching::Node and FAnimNode_MotionMatching::Database,
	//    AND Database carries `meta=(PinShownByDefault)` — meaning the actual
	//    runtime value comes from the pin default, not the struct field.
	//    Direct FProperty writes here do not persist through serialization.
	//    The Python wrapper assigns Database via set_editor_property on the
	//    returned struct (which goes through the full Edit pipeline including
	//    pin default propagation). The C++ helper only spawns + connects.
	//
	//    Refresh the node so pin layout is up to date for the caller.
	MMNode->ReconstructNode();

	// 5. Connect MM "Pose" output → Root "Result" input.
	UEdGraphPin* MMOut    = MMNode->FindPin(TEXT("Pose"),   EGPD_Output);
	UEdGraphPin* RootIn   = RootNode->FindPin(TEXT("Result"), EGPD_Input);
	if (!MMOut || !RootIn)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::WireMotionMatchingAnimGraph — could not find Pose/Result pins (MMOut=%p, RootIn=%p)"), MMOut, RootIn);
		return false;
	}

	// Break any pre-existing connections on Root.Result so we don't pile up
	// duplicate links on repeat runs.
	RootIn->BreakAllPinLinks(/*bNotifyNodes=*/false);

	// Use the schema's MakeConnection so the graph notifies/validates correctly.
	bool bConnected = false;
	if (const UEdGraphSchema* Schema = AnimGraph->GetSchema())
	{
		bConnected = Schema->TryCreateConnection(MMOut, RootIn);
	}
	if (!bConnected)
	{
		UE_LOG(LogAnimation, Warning, TEXT("AnimDatabaseUtility::WireMotionMatchingAnimGraph — schema rejected connection; falling back to MakeLinkTo"));
		MMOut->MakeLinkTo(RootIn);
	}

	// 6. Mark BP structurally modified and trigger a compile so the cached
	//    generated class is up to date for downstream tooling.
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);

	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(AnimBP, EBlueprintCompileOptions::None, &Results);
	const int32 ErrorCount   = Results.NumErrors;
	const int32 WarningCount = Results.NumWarnings;
	UE_LOG(LogAnimation, Log, TEXT("AnimDatabaseUtility: Compiled %s (errors=%d, warnings=%d)"),
		*AnimBP->GetName(), ErrorCount, WarningCount);

	return ErrorCount == 0;

#else
	UE_LOG(LogAnimation, Warning, TEXT("AnimDatabaseUtility::WireMotionMatchingAnimGraph is an editor-only operation."));
	return false;
#endif // WITH_EDITOR
}

// -----------------------------------------------------------------------
// InsertFootIKNodes
// -----------------------------------------------------------------------
#if WITH_EDITOR
namespace Edge26AnimDB_Internal
{
	/** Find or spawn a UAnimGraphNode_TwoBoneIK on Graph whose IKBone matches FootBone. */
	static UAnimGraphNode_TwoBoneIK* FindOrSpawnTwoBoneIK(
		UEdGraph* Graph,
		FName FootBone,
		FName JointBone,
		int32 PosX,
		int32 PosY)
	{
		check(Graph);

		// Idempotency: reuse an existing TwoBoneIK that already targets FootBone.
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (UAnimGraphNode_TwoBoneIK* AsIK = Cast<UAnimGraphNode_TwoBoneIK>(N))
			{
				if (AsIK->Node.IKBone.BoneName == FootBone)
				{
					return AsIK;
				}
			}
		}

		// Spawn a fresh node.
		FGraphNodeCreator<UAnimGraphNode_TwoBoneIK> NodeCreator(*Graph);
		UAnimGraphNode_TwoBoneIK* NewIK = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		NodeCreator.Finalize();

		NewIK->NodePosX = PosX;
		NewIK->NodePosY = PosY;

		// Configure the embedded runtime node.
		NewIK->Node.IKBone = FBoneReference(FootBone);
		NewIK->Node.JointTarget.BoneReference = FBoneReference(JointBone);
		// FBoneSocketTarget supports either a bone or a socket; pick "bone".
		NewIK->Node.JointTarget.bUseSocket = false;

		// BCS_BoneSpace + zero offset for both effector + joint target = no-op
		// IK pose-matching the source pose. See header comment.
		NewIK->Node.EffectorLocationSpace    = BCS_BoneSpace;
		NewIK->Node.JointTargetLocationSpace = BCS_BoneSpace;
		NewIK->Node.EffectorLocation         = FVector::ZeroVector;
		NewIK->Node.JointTargetLocation      = FVector::ZeroVector;
		NewIK->Node.Alpha                    = 1.0f;

		// Rebuild pins (so default values on the struct propagate onto pin
		// defaults for any PinShownByDefault pins like EffectorLocation /
		// JointTargetLocation / Alpha).
		NewIK->ReconstructNode();

		return NewIK;
	}

	/** Wire FromNode.Pose -> ToNode.<ToPinName> via the schema, breaking pre-existing links on the input. */
	static bool ConnectPosePinTo(UEdGraph* Graph, UEdGraphNode* FromNode, UEdGraphNode* ToNode, FName ToPinName)
	{
		check(Graph && FromNode && ToNode);

		UEdGraphPin* OutPin = FromNode->FindPin(TEXT("Pose"), EGPD_Output);
		UEdGraphPin* InPin  = ToNode->FindPin(ToPinName,      EGPD_Input);
		if (!OutPin || !InPin)
		{
			UE_LOG(LogAnimation, Error,
				TEXT("AnimDatabaseUtility::InsertFootIKNodes — pin lookup failed (Pose=%p, %s=%p)"),
				OutPin, *ToPinName.ToString(), InPin);
			return false;
		}

		InPin->BreakAllPinLinks(/*bNotifyNodes=*/false);

		bool bConnected = false;
		if (const UEdGraphSchema* Schema = Graph->GetSchema())
		{
			bConnected = Schema->TryCreateConnection(OutPin, InPin);
		}
		if (!bConnected)
		{
			UE_LOG(LogAnimation, Warning,
				TEXT("AnimDatabaseUtility::InsertFootIKNodes — schema rejected connection; falling back to MakeLinkTo"));
			OutPin->MakeLinkTo(InPin);
			bConnected = true;
		}
		return bConnected;
	}
}
#endif // WITH_EDITOR

bool UAnimDatabaseUtility::InsertFootIKNodes(
	UAnimBlueprint* AnimBP,
	FName LeftFootBone,
	FName LeftJointBone,
	FName RightFootBone,
	FName RightJointBone)
{
#if WITH_EDITOR
	if (!AnimBP)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::InsertFootIKNodes — AnimBP is null"));
		return false;
	}
	if (LeftFootBone.IsNone() || LeftJointBone.IsNone() ||
	    RightFootBone.IsNone() || RightJointBone.IsNone())
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::InsertFootIKNodes — bone names must all be non-None"));
		return false;
	}

	// 1. Find AnimGraph.
	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* G : AnimBP->FunctionGraphs)
	{
		if (G && G->GetFName() == TEXT("AnimGraph"))
		{
			AnimGraph = G;
			break;
		}
	}
	if (!AnimGraph)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::InsertFootIKNodes — AnimGraph not found on %s"), *AnimBP->GetName());
		return false;
	}

	// 2. Find existing MotionMatching + Root nodes (must already exist; M5 wired them).
	UAnimGraphNode_MotionMatching* MMNode = nullptr;
	UAnimGraphNode_Root*           RootNode = nullptr;
	for (UEdGraphNode* N : AnimGraph->Nodes)
	{
		if (!MMNode)
		{
			if (UAnimGraphNode_MotionMatching* AsMM = Cast<UAnimGraphNode_MotionMatching>(N))
			{
				MMNode = AsMM;
			}
		}
		if (!RootNode)
		{
			if (UAnimGraphNode_Root* AsRoot = Cast<UAnimGraphNode_Root>(N))
			{
				RootNode = AsRoot;
			}
		}
	}
	if (!MMNode)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::InsertFootIKNodes — no MotionMatching node found (M5 prerequisite)"));
		return false;
	}
	if (!RootNode)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::InsertFootIKNodes — no Root node found"));
		return false;
	}

	// 3. Spawn (or reuse) the two IK nodes.
	UAnimGraphNode_TwoBoneIK* LeftIK = Edge26AnimDB_Internal::FindOrSpawnTwoBoneIK(
		AnimGraph, LeftFootBone, LeftJointBone,
		/*PosX=*/RootNode->NodePosX - 720,
		/*PosY=*/RootNode->NodePosY);

	UAnimGraphNode_TwoBoneIK* RightIK = Edge26AnimDB_Internal::FindOrSpawnTwoBoneIK(
		AnimGraph, RightFootBone, RightJointBone,
		/*PosX=*/RootNode->NodePosX - 360,
		/*PosY=*/RootNode->NodePosY);

	if (!LeftIK || !RightIK)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::InsertFootIKNodes — failed to spawn TwoBoneIK node(s)"));
		return false;
	}

	// 4. Even if the IK nodes already existed, re-apply the configured properties
	//    so re-runs converge on the canonical settings.
	{
		auto Configure = [](UAnimGraphNode_TwoBoneIK* IK, FName FootBone, FName JointBone)
		{
			IK->Node.IKBone                     = FBoneReference(FootBone);
			IK->Node.JointTarget.BoneReference  = FBoneReference(JointBone);
			IK->Node.JointTarget.bUseSocket     = false;
			IK->Node.EffectorLocationSpace      = BCS_BoneSpace;
			IK->Node.JointTargetLocationSpace   = BCS_BoneSpace;
			IK->Node.EffectorLocation           = FVector::ZeroVector;
			IK->Node.JointTargetLocation        = FVector::ZeroVector;
			IK->Node.Alpha                      = 1.0f;
			IK->ReconstructNode();
		};
		Configure(LeftIK,  LeftFootBone,  LeftJointBone);
		Configure(RightIK, RightFootBone, RightJointBone);
	}

	// 5. Rewire: MM -> LeftIK.ComponentPose, LeftIK -> RightIK.ComponentPose,
	//    RightIK -> Root.Result. Pin name for the SkeletalControl input is
	//    "ComponentPose" (auto-generated from the FStructProperty in
	//    FAnimNode_SkeletalControlBase). The Root input is "Result".
	if (!Edge26AnimDB_Internal::ConnectPosePinTo(AnimGraph, MMNode,  LeftIK,   TEXT("ComponentPose")))
	{
		return false;
	}
	if (!Edge26AnimDB_Internal::ConnectPosePinTo(AnimGraph, LeftIK,  RightIK,  TEXT("ComponentPose")))
	{
		return false;
	}
	if (!Edge26AnimDB_Internal::ConnectPosePinTo(AnimGraph, RightIK, RootNode, TEXT("Result")))
	{
		return false;
	}

	// 6. Mark BP structurally modified and recompile.
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);

	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(AnimBP, EBlueprintCompileOptions::None, &Results);
	const int32 ErrorCount   = Results.NumErrors;
	const int32 WarningCount = Results.NumWarnings;
	UE_LOG(LogAnimation, Log, TEXT("AnimDatabaseUtility::InsertFootIKNodes: Compiled %s (errors=%d, warnings=%d)"),
		*AnimBP->GetName(), ErrorCount, WarningCount);

	return ErrorCount == 0;

#else
	UE_LOG(LogAnimation, Warning, TEXT("AnimDatabaseUtility::InsertFootIKNodes is an editor-only operation."));
	return false;
#endif // WITH_EDITOR
}

// -----------------------------------------------------------------------
// SaveAnimBlueprintAsset
// -----------------------------------------------------------------------
bool UAnimDatabaseUtility::SaveAnimBlueprintAsset(UAnimBlueprint* AnimBP)
{
	if (!AnimBP)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::SaveAnimBlueprintAsset — AnimBP is null"));
		return false;
	}

#if WITH_EDITOR
	UPackage* Pkg = AnimBP->GetOutermost();
	if (!Pkg)
	{
		UE_LOG(LogAnimation, Error, TEXT("AnimDatabaseUtility::SaveAnimBlueprintAsset — could not get package for %s"), *AnimBP->GetName());
		return false;
	}

	const FString PackageName     = Pkg->GetName();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags     = SAVE_NoError;
	const bool bSuccess = UPackage::SavePackage(Pkg, AnimBP, *PackageFilename, SaveArgs);

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
	UE_LOG(LogAnimation, Warning, TEXT("AnimDatabaseUtility::SaveAnimBlueprintAsset is an editor-only operation."));
	return false;
#endif // WITH_EDITOR
}
