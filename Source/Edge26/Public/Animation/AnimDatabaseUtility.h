// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimDatabaseUtility.generated.h"

class UPoseSearchSchema;
class UPoseSearchDatabase;
class UAnimSequence;
class UAnimMontage;
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
	 * Inserts two UAnimGraphNode_TwoBoneIK nodes (left + right foot) between the
	 * existing MotionMatching node and the Output Pose (Root) in the AnimBP's
	 * AnimGraph, producing the chain:
	 *
	 *   MotionMatching -> TwoBoneIK_LeftFoot -> TwoBoneIK_RightFoot -> Root
	 *
	 * Idempotent: re-running won't duplicate nodes (looks for existing IK nodes
	 * whose IKBone matches LeftFootBone / RightFootBone and reuses them).
	 *
	 * Each TwoBoneIK node is configured with:
	 *   - IKBone.BoneName = <Left|Right>FootBone (e.g. "foot_l" / "foot_r")
	 *   - JointTarget.BoneReference.BoneName = <Left|Right>JointBone (e.g. "calf_l" / "calf_r")
	 *   - EffectorLocationSpace / JointTargetLocationSpace = BCS_BoneSpace
	 *   - EffectorLocation = FVector::ZeroVector (i.e. stay at the bone's
	 *     animated position — see note below)
	 *   - Alpha = 1.0
	 *
	 * Note on the v0 IK policy: the milestone plan calls for an effector in
	 * world space with a dynamic Z-clamp to ground level. That requires a
	 * multi-node "Get Socket Transform -> Break -> Make Z=0 -> Effector" chain
	 * which is brittle to spawn headlessly. For v0 — flat pitch, anims authored
	 * at ground level — BCS_BoneSpace + zero offset means the IK pose-matches
	 * the source pose, i.e. these nodes are dormant until ball-contact
	 * montages, foot-plant detection, or terrain hook them in later milestones.
	 * Inserting them now provides the IK chain infrastructure without
	 * committing to a specific effector-targeting policy.
	 *
	 * Returns true on success (the AnimBP recompiles cleanly). Caller still
	 * needs to save the package.
	 */
	UFUNCTION(BlueprintCallable, Category = "Edge26 Anim")
	static bool InsertFootIKNodes(
		UAnimBlueprint* AnimBP,
		FName LeftFootBone,
		FName LeftJointBone,
		FName RightFootBone,
		FName RightJointBone);

	/**
	 * Inserts a UAnimGraphNode_PoseSearchHistoryCollector immediately downstream
	 * of the existing MotionMatching node, splicing it into whatever pose link
	 * MM currently feeds. Yields the chain:
	 *
	 *   MM -> Collector.Source -> Collector.Pose -> <whatever-was-after-MM>
	 *
	 * Examples after running on the two M5/M6/M9 AnimBPs:
	 *   ABP_Footballer_MM:  MM -> Collector -> TwoBoneIK_L -> TwoBoneIK_R -> Root
	 *   ABP_Goalkeeper:     MM -> Collector -> Root
	 *
	 * Why this fixes "LogPoseSearch: missing IPoseHistory":
	 * FAnimNode_MotionMatching::UpdateAssetPlayer looks up an FPoseHistoryProvider
	 * via Context.GetMessage<FPoseHistoryProvider>(). The history collector
	 * publishes that provider into the graph context inside its Update_AnyThread
	 * (TScopedGraphMessage) BEFORE recursing into its Source pin. Therefore the
	 * collector must be the IMMEDIATE PARENT of MM in pose-flow update order:
	 * placing the collector downstream of MM (collector.Source = MM.Pose) means
	 * when the graph traverses Update from Root downwards, the collector pushes
	 * the message and then calls Source.Update(Context), and MM's update sees
	 * the collector's message live on the stack. The previous topology had no
	 * collector at all, hence the warning + a stale/T-pose output.
	 *
	 * The collector is configured with bGenerateTrajectory = true so it
	 * synthesises a trajectory from the AnimInstance's motion (the schema's
	 * Trajectory feature channel needs future + past samples; we have no
	 * upstream trajectory predictor wired). All other settings stay at default
	 * (PoseCount=2 keeps memory minimal; SamplingInterval=0.04s = ~25 Hz).
	 *
	 * Idempotent: re-running won't duplicate the collector — finds the existing
	 * UAnimGraphNode_PoseSearchHistoryCollector if present and reuses it.
	 *
	 * Returns true on success (BP compiles cleanly). Caller still needs to save.
	 */
	UFUNCTION(BlueprintCallable, Category = "Edge26 Anim")
	static bool InsertPoseHistoryCollector(UAnimBlueprint* AnimBP);

	/**
	 * Saves the package containing AnimBP. Returns true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Edge26 Anim")
	static bool SaveAnimBlueprintAsset(UAnimBlueprint* AnimBP);

	/**
	 * Adds a string-named AnimNotify event at the given time (seconds from
	 * sequence start) to an AnimSequence. The notify is a plain UAnimNotify (no
	 * UObject backing, no notify-state) — the consuming AnimBP / AnimInstance
	 * listens by name (e.g. AnimNotify_BallContact / AnimNotify_RecoverEnd
	 * native handlers, or BP-level notify routing).
	 *
	 * Idempotent: if a notify with the same NotifyName already exists on this
	 * sequence at a time within 0.01s of TimeSec, the call is a no-op and
	 * returns true. Otherwise a new FAnimNotifyEvent is appended to the
	 * sequence's Notifies array via the public ENGINE_API path, the cache is
	 * refreshed via RefreshCacheData(), and the package is marked dirty.
	 *
	 * Caller is responsible for saving the asset (use
	 * `unreal.EditorAssetLibrary.save_asset(...)` from Python).
	 *
	 * Returns true on success, false on null sequence or non-editor build.
	 */
	UFUNCTION(BlueprintCallable, Category = "Edge26 Anim")
	static bool AddAnimNotify(UAnimSequence* Sequence, FName NotifyName, float TimeSec);

	/**
	 * Creates a UAnimMontage at `<PackagePath>/<AssetName>` containing a single
	 * slot ("DefaultSlot") with `SourceSequence` as the only segment. Mirrors
	 * what `UAnimMontageFactory::FactoryCreateNew` does when its
	 * `SourceAnimation` is set — bypassing the factory's modal skeleton picker
	 * (which doesn't survive headless Python invocations on UE5.7 because the
	 * `set_editor_property("source_animation", ...)` binding is unreliable
	 * across UE point releases).
	 *
	 * Operations performed (all editor-only):
	 *   1. Create / reuse the target package at `<PackagePath>/<AssetName>`.
	 *   2. NewObject<UAnimMontage> with RF_Public | RF_Standalone | RF_Transactional.
	 *   3. Push an FAnimSegment(SourceSequence) onto SlotAnimTracks[0].AnimTrack.AnimSegments.
	 *   4. Call SetCompositeLength(SourceSequence->GetPlayLength()).
	 *   5. Call SetSkeleton(SourceSequence->GetSkeleton()).
	 *   6. Call UpdateCommonTargetFrameRate().
	 *   7. UAnimMontageFactory::EnsureStartingSection (default section at T=0).
	 *   8. FAssetRegistryModule::AssetCreated + MarkPackageDirty + SavePackage.
	 *
	 * Idempotent: if the asset already exists at the target path, this returns
	 * the existing UAnimMontage and does not overwrite. Caller may still
	 * separately mutate the returned montage (e.g. to swap the source) and
	 * call SaveDatabaseAsset on the package.
	 *
	 * Returns the new (or existing) montage on success, nullptr otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Edge26 Anim")
	static UAnimMontage* CreateMontageFromSequence(
		UAnimSequence* SourceSequence,
		const FString& PackagePath,
		const FString& AssetName);
};
