"""Create an empty UPoseSearchDatabase asset for outfield locomotion.

Run:
  UnrealEditor-Cmd <project> -ExecutePythonScript=<this> -unattended -stdout
"""

import unreal

ASSET_NAME = "MMDB_Outfield"
ASSET_PATH = "/Game/Animation/MotionMatching"
LOG = "/tmp/create_mmdb_outfield.log"

with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    full = f"{ASSET_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        log(f"Asset already exists: {full}")
    else:
        # Pose Search Database factory.
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        # In UE 5.7 the class is PoseSearchDatabase; lookup by name to avoid
        # hard-coding a Python class binding that might differ across UE versions.
        cls = unreal.load_class(None, "/Script/PoseSearch.PoseSearchDatabase")
        if not cls:
            log("ERROR: PoseSearch.PoseSearchDatabase class not found. Is the Motion Matching plugin enabled?")
        else:
            asset = tools.create_asset(ASSET_NAME, ASSET_PATH, cls, None)
            if asset:
                unreal.EditorAssetLibrary.save_asset(full)
                log(f"Created + saved {full}")
            else:
                log(f"ERROR: failed to create {full}")
    log("Done.")
