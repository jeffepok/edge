"""Create BP_Goalkeeper as a Blueprint subclass of BP_Footballer that defaults
its mesh AnimInstanceClass to ABP_Goalkeeper.
"""

import unreal

ASSET_NAME = "BP_Goalkeeper"
ASSET_PATH = "/Game/Blueprints/Player"
PARENT_BP   = "/Game/Blueprints/Player/BP_Footballer"
ANIM_CLASS  = "/Game/Blueprints/Player/ABP_Goalkeeper"
LOG = "/tmp/create_bp_goalkeeper.log"

with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    full = f"{ASSET_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        log(f"Already exists: {full}")
    else:
        parent = unreal.EditorAssetLibrary.load_blueprint_class(PARENT_BP)
        if not parent:
            log(f"ERROR: parent BP not found: {PARENT_BP}")
        else:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            tools = unreal.AssetToolsHelpers.get_asset_tools()
            asset = tools.create_asset(ASSET_NAME, ASSET_PATH, unreal.Blueprint, factory)
            if asset:
                # Set AnimInstanceClass default on the mesh component CDO.
                anim_cls = unreal.EditorAssetLibrary.load_blueprint_class(ANIM_CLASS)
                if anim_cls:
                    gen = asset.generated_class()
                    cdo = unreal.get_default_object(gen)
                    mesh = cdo.get_editor_property("Mesh")
                    if mesh:
                        mesh.set_editor_property("anim_class", anim_cls)
                        log(f"Set anim_class on Mesh component to {anim_cls}")
                    else:
                        log("WARN: BP_Footballer CDO has no 'Mesh' property; anim_class default not set")
                else:
                    log(f"ERROR: ABP_Goalkeeper class not found: {ANIM_CLASS}")
                unreal.EditorAssetLibrary.save_asset(full)
                log(f"Created + saved {full}")
            else:
                log("ERROR: BlueprintFactory.create_asset returned None")
    log("Done.")
