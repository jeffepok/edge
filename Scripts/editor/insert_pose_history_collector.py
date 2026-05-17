"""Insert a PoseSearchHistoryCollector into both MM-driven AnimBPs.

Fixes the Phase 3 v0 blocker discovered at M11: ABP_Footballer_MM and
ABP_Goalkeeper had their FAnimNode_MotionMatching wired without an upstream
FAnimNode_PoseSearchHistoryCollector. MM consumes pose-history from the graph
context message bus (Context.GetMessage<FPoseHistoryProvider>()), and only the
collector node publishes that provider. Without it, MM's
"LogPoseSearch: missing IPoseHistory" warning fires every frame and the
schema's Trajectory feature channel has nothing to match against (output is a
held T-pose / last-good-frame).

What this script does:

  1. Loads /Game/Blueprints/Player/ABP_Footballer_MM and ABP_Goalkeeper.
  2. Calls UAnimDatabaseUtility.insert_pose_history_collector(abp), which
     spawns a UAnimGraphNode_PoseSearchHistoryCollector and splices it
     immediately downstream of the existing MotionMatching node, retargeting
     whatever MM was previously feeding (TwoBoneIK_L for the outfield BP, Root
     for the goalkeeper BP).
  3. Verifies exactly one collector node exists per AnimGraph.
  4. Saves each AnimBP package.

Idempotent: re-running won't duplicate the collector (the C++ helper finds the
existing UAnimGraphNode_PoseSearchHistoryCollector if present and reuses it).
"""

import sys
import unreal

ANIMBP_PATHS = [
    "/Game/Blueprints/Player/ABP_Footballer_MM",
    "/Game/Blueprints/Player/ABP_Goalkeeper",
]
LOG_PATH = "/tmp/insert_pose_history_collector.log"


def process_one(log, anim_bp_path: str) -> int:
    log(f"Loading AnimBP at {anim_bp_path} ...")
    anim_bp = unreal.EditorAssetLibrary.load_asset(anim_bp_path)
    if not anim_bp:
        log(f"ERROR: failed to load AnimBP at {anim_bp_path}")
        return 1
    log(f"  -> {anim_bp}")

    util = unreal.AnimDatabaseUtility
    log("Calling UAnimDatabaseUtility.insert_pose_history_collector ...")
    ok = util.insert_pose_history_collector(anim_bp)
    log(f"  insert_pose_history_collector -> {ok}")
    if not ok:
        log("ERROR: InsertPoseHistoryCollector returned false")
        return 1

    # Post-condition: exactly one PoseSearchHistoryCollector node on the graph.
    collector_cls = unreal.load_class(
        None,
        "/Script/PoseSearchEditor.AnimGraphNode_PoseSearchHistoryCollector",
    )
    if collector_cls is None:
        log("ERROR: could not load UAnimGraphNode_PoseSearchHistoryCollector class")
        return 1

    collector_nodes = anim_bp.get_nodes_of_class(collector_cls)
    log(f"AnimGraph now contains {len(collector_nodes)} PoseSearchHistoryCollector node(s):")
    for n in collector_nodes:
        details = ""
        try:
            node_struct = n.get_editor_property("node")
            gen_traj = node_struct.get_editor_property("b_generate_trajectory")
            pose_count = node_struct.get_editor_property("pose_count")
            sampling = node_struct.get_editor_property("sampling_interval")
            details = (
                f" bGenerateTrajectory={gen_traj}"
                f" PoseCount={pose_count}"
                f" SamplingInterval={sampling:.3f}"
            )
        except Exception as exc:  # pragma: no cover — debug only.
            details = f" (introspection failed: {exc})"
        log(f"  - {n.get_name()}{details}")
    if len(collector_nodes) != 1:
        log(f"ERROR: expected exactly 1 PoseSearchHistoryCollector node, got {len(collector_nodes)}")
        return 1

    # Save the package.
    log("Saving AnimBP package ...")
    saved = util.save_anim_blueprint_asset(anim_bp)
    log(f"  save_anim_blueprint_asset = {saved}")
    if not saved:
        log("ERROR: save failed")
        return 1

    log("")
    return 0


def main() -> int:
    f = open(LOG_PATH, "w", buffering=1)

    def log(msg: str) -> None:
        f.write(msg + "\n")
        unreal.log(msg)

    for path in ANIMBP_PATHS:
        log(f"=== {path} ===")
        rc = process_one(log, path)
        if rc != 0:
            log(f"ABORT: failure processing {path}")
            f.close()
            return rc

    log("All AnimBPs patched.")
    f.close()
    return 0


sys.exit(main())
