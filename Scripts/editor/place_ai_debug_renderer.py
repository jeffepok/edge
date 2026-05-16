# place_ai_debug_renderer.py — places one AAIDebugRenderer actor in L_Pitch.
# Safe to run multiple times: skips if already present.
#
# Implementation note: UE 5.7's SpawnActorFromClass calls GetHitProxy internally
# which crashes in -nullrhi mode. This script creates a thin Blueprint subclass
# of AAIDebugRenderer (BP_AIDebugRenderer) in /Game/Blueprints/Debug/ and spawns
# that instead. The Blueprint is saved as a real content asset so map references
# remain valid across editor restarts.
#
# Run WITHOUT -nullrhi:
#   UnrealEditor-Cmd <project> -ExecutePythonScript=<this> -stdout -unattended

import unreal

LEVEL_PATH   = "/Game/Levels/L_Pitch"
BP_ASSET_PKG = "/Game/Blueprints/Debug/BP_AIDebugRenderer"
LOG_FILE     = "/tmp/place_ai_debug_renderer_log.txt"

_log_fh = open(LOG_FILE, "w", buffering=1)

def log(msg):
    _log_fh.write(msg + "\n")
    _log_fh.flush()
    unreal.log(msg)

def log_err(msg):
    _log_fh.write("ERROR: " + msg + "\n")
    _log_fh.flush()
    unreal.log_error(msg)

def main():
    log(f"Script started — opening level {LEVEL_PATH}")

    # Load the C++ class first.
    renderer_class = unreal.load_class(None, "/Script/Edge26.AIDebugRenderer")
    log(f"AIDebugRenderer C++ class loaded: {renderer_class is not None}")
    if not renderer_class:
        log_err("Could not load /Script/Edge26.AIDebugRenderer — is the module compiled?")
        return

    # Create (or load existing) a Blueprint subclass of AAIDebugRenderer.
    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_ASSET_PKG)
    if not bp_class:
        log("Blueprint not found — creating BP_AIDebugRenderer...")
        import os
        os.makedirs(
            "/Users/jeffersonaddai-poku/Desktop/projects/Games/Edge26/Content/Blueprints/Debug",
            exist_ok=True)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", renderer_class)
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        bp_asset = asset_tools.create_asset(
            "BP_AIDebugRenderer",
            "/Game/Blueprints/Debug",
            unreal.Blueprint,
            factory)
        if not bp_asset:
            log_err("Failed to create BP_AIDebugRenderer Blueprint")
            return
        unreal.EditorAssetLibrary.save_asset(BP_ASSET_PKG)
        log(f"Blueprint asset created and saved: {bp_asset}")
        bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_ASSET_PKG)
    else:
        log(f"Blueprint already exists: {bp_class}")

    if not bp_class:
        log_err("Failed to get Blueprint class")
        return

    # Open L_Pitch.
    success = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    log(f"load_map returned: {success}")
    if not success:
        log_err(f"Failed to open level {LEVEL_PATH}")
        return

    # Check for existing AIDebugRenderer (match by parent class).
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actor_subsystem.get_all_level_actors()
    log(f"Total actors in level: {len(all_actors)}")

    existing = [a for a in all_actors
                if "AIDebugRenderer" in a.get_class().get_name()]
    if existing:
        log(f"AIDebugRenderer already present ({len(existing)} instance(s)); skipping.")
        return

    # Spawn.
    log("Spawning BP_AIDebugRenderer actor...")
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        bp_class,
        unreal.Vector(0, 0, 0))
    log(f"Spawn result: {actor}")

    if not actor:
        log_err("SpawnActor returned None for BP_AIDebugRenderer")
        return

    actor.set_actor_label("AIDebugRenderer")
    unreal.EditorLevelLibrary.save_current_level()
    log("Placed AIDebugRenderer in L_Pitch.")

main()
