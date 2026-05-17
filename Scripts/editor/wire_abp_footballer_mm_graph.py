"""Wire the AnimGraph of /Game/Blueprints/Player/ABP_Footballer_MM headlessly.

Adds (or updates) a UAnimGraphNode_MotionMatching driving the inherited Output
Pose, with its Database property set to MMDB_Outfield. Compiles and saves.

Runs all heavy lifting through UAnimDatabaseUtility::WireMotionMatchingAnimGraph
because UE5.7 does not expose graph-node spawning or pin connection through
Python directly.

Idempotent: re-running won't duplicate the MM node.
"""

import sys
import unreal

ANIMBP_PATH = "/Game/Blueprints/Player/ABP_Footballer_MM"
DATABASE_PATH = "/Game/Animation/MotionMatching/MMDB_Outfield"
LOG_PATH = "/tmp/wire_abp_footballer_mm_graph.log"


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

    log(f"Loading database at {DATABASE_PATH} ...")
    db = unreal.EditorAssetLibrary.load_asset(DATABASE_PATH)
    if not db:
        log(f"ERROR: failed to load database at {DATABASE_PATH}")
        return 1
    log(f"  -> {db}")

    log("Calling UAnimDatabaseUtility.wire_motion_matching_anim_graph ...")
    util = unreal.AnimDatabaseUtility
    ok = util.wire_motion_matching_anim_graph(anim_bp, db)
    log(f"  wire result: {ok}")
    if not ok:
        log("ERROR: WireMotionMatchingAnimGraph returned false")
        return 1

    # Post-condition: there should be exactly one MotionMatching node now.
    mm_cls = unreal.load_class(None, "/Script/PoseSearchEditor.AnimGraphNode_MotionMatching")
    mm_nodes = anim_bp.get_nodes_of_class(mm_cls)
    log(f"AnimGraph now contains {len(mm_nodes)} MotionMatching node(s).")
    for n in mm_nodes:
        log(f"  - {n.get_name()}")
    if len(mm_nodes) != 1:
        log("ERROR: expected exactly 1 MotionMatching node")
        return 1
    mm_node = mm_nodes[0]

    # Set Database on the embedded FAnimNode_MotionMatching struct from Python.
    # The C++ helper deliberately skips this step because the field carries
    # `meta=(PinShownByDefault)` — direct FProperty writes don't survive
    # serialization. Python's set_editor_property on the struct triggers the
    # full Edit pipeline and persists.
    log("Setting MotionMatching.Database via Python set_editor_property ...")
    node_struct = mm_node.get_editor_property("node")
    node_struct.set_editor_property("database", db)
    mm_node.set_editor_property("node", node_struct)

    # Verify in-memory before save.
    refetched = mm_node.get_editor_property("node").get_editor_property("database")
    log(f"  in-memory database = {refetched}")
    if refetched is None:
        log("ERROR: failed to set Database on MotionMatching node")
        return 1

    # Re-compile after the Database change so the cached generated class
    # picks up the value.
    log("Recompiling AnimBP after Database set ...")
    unreal.BlueprintEditorLibrary.compile_blueprint(anim_bp)

    log("Saving AnimBP package ...")
    saved = util.save_anim_blueprint_asset(anim_bp)
    log(f"  save_anim_blueprint_asset = {saved}")
    if not saved:
        log("ERROR: save failed")
        return 1

    # Re-load + verify post-save persistence.
    log("Verifying Database persisted across re-load ...")
    abp_check = unreal.EditorAssetLibrary.load_asset(ANIMBP_PATH)
    mm_check = abp_check.get_nodes_of_class(mm_cls)[0]
    db_check = mm_check.get_editor_property("node").get_editor_property("database")
    log(f"  reloaded database = {db_check}")
    if db_check is None:
        log("ERROR: Database did not persist across save/reload")
        return 1

    log("Done.")
    f.close()
    return 0


sys.exit(main())
