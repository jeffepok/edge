"""Create ABP_Footballer_MM, a UAnimBlueprint subclass of UFootballAnimInstance
targeting the Manny skeleton. The graph is empty; M5.2 fills it in the editor.
"""

import unreal

ASSET_NAME = "ABP_Footballer_MM"
ASSET_PATH = "/Game/Blueprints/Player"
PARENT_CLASS_PATH = "/Script/Edge26.FootballAnimInstance"
SKELETON_PATH = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
LOG = "/tmp/create_abp_footballer_mm.log"

with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    full = f"{ASSET_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        log(f"Already exists: {full}")
    else:
        parent = unreal.load_class(None, PARENT_CLASS_PATH)
        skel   = unreal.EditorAssetLibrary.load_asset(SKELETON_PATH)
        if not parent:
            log(f"ERROR: parent class not loaded: {PARENT_CLASS_PATH}")
        elif not skel:
            log(f"ERROR: skeleton asset not found: {SKELETON_PATH}")
        else:
            factory = unreal.AnimBlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            factory.set_editor_property("target_skeleton", skel)
            tools = unreal.AssetToolsHelpers.get_asset_tools()
            asset = tools.create_asset(ASSET_NAME, ASSET_PATH, unreal.AnimBlueprint, factory)
            if asset:
                unreal.EditorAssetLibrary.save_asset(full)
                log(f"Created + saved {full}")
            else:
                log("ERROR: create_asset returned None")
    log("Done.")
