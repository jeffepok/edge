"""Insert TwoBoneIK nodes (left + right foot) into ABP_Footballer_MM.

Runs the UAnimDatabaseUtility::InsertFootIKNodes helper headlessly. The C++
helper does all the heavy lifting (UE5.7 does not expose graph-node spawning
or pin connection through Python). This driver:

  1. Loads ABP_Footballer_MM.
  2. Calls InsertFootIKNodes(foot_l, calf_l, foot_r, calf_r).
  3. Verifies exactly 2 TwoBoneIK nodes exist on the graph.
  4. Saves the AnimBP package.

Idempotent: re-running won't duplicate nodes; the C++ helper looks up
existing IK nodes by IKBone name and reuses them.
"""

import sys
import unreal

ANIMBP_PATH = "/Game/Blueprints/Player/ABP_Footballer_MM"
LOG_PATH = "/tmp/insert_foot_ik_nodes.log"

# Bone naming matches the Mannequin / SK_Footballer skeleton used by M5.
LEFT_FOOT_BONE  = "foot_l"
LEFT_JOINT_BONE = "calf_l"
RIGHT_FOOT_BONE = "foot_r"
RIGHT_JOINT_BONE = "calf_r"


def main() -> int:
    f = open(LOG_PATH, "w", buffering=1)

    def log(msg: str) -> None:
        f.write(msg + "\n")
        unreal.log(msg)

    log(f"Loading AnimBP at {ANIMBP_PATH} ...")
    anim_bp = unreal.EditorAssetLibrary.load_asset(ANIMBP_PATH)
    if not anim_bp:
        log(f"ERROR: failed to load AnimBP at {ANIMBP_PATH}")
        return 1
    log(f"  -> {anim_bp}")

    log("Calling UAnimDatabaseUtility.insert_foot_ik_nodes ...")
    util = unreal.AnimDatabaseUtility
    ok = util.insert_foot_ik_nodes(
        anim_bp,
        LEFT_FOOT_BONE,
        LEFT_JOINT_BONE,
        RIGHT_FOOT_BONE,
        RIGHT_JOINT_BONE,
    )
    log(f"  insert_foot_ik_nodes -> {ok}")
    if not ok:
        log("ERROR: InsertFootIKNodes returned false")
        return 1

    # Post-condition: exactly 2 TwoBoneIK nodes (left + right foot).
    ik_cls = unreal.load_class(None, "/Script/AnimGraph.AnimGraphNode_TwoBoneIK")
    if ik_cls is None:
        log("ERROR: could not load UAnimGraphNode_TwoBoneIK class")
        return 1

    ik_nodes = anim_bp.get_nodes_of_class(ik_cls)
    log(f"AnimGraph now contains {len(ik_nodes)} TwoBoneIK node(s):")
    for n in ik_nodes:
        # FBoneReference is exposed through Python as a struct whose only
        # editable property is the bone name. Attribute access varies between
        # FStructOnScope wrappers — wrap in try/except so the verification
        # never blocks the save step.
        details = ""
        try:
            node_struct = n.get_editor_property("node")
            ik_bone = node_struct.get_editor_property("ik_bone")
            details = f" IKBone={ik_bone}"
        except Exception as exc:  # pragma: no cover — debug only.
            details = f" (introspection failed: {exc})"
        log(f"  - {n.get_name()}{details}")
    if len(ik_nodes) != 2:
        log(f"ERROR: expected exactly 2 TwoBoneIK nodes, got {len(ik_nodes)}")
        return 1

    # Save the package.
    log("Saving AnimBP package ...")
    saved = util.save_anim_blueprint_asset(anim_bp)
    log(f"  save_anim_blueprint_asset = {saved}")
    if not saved:
        log("ERROR: save failed")
        return 1

    log("Done.")
    f.close()
    return 0


sys.exit(main())
