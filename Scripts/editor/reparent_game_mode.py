"""Re-parent BP_SoccerGameMode to ASoccerGameMode.

The BP was created with parent = GameModeBase (UE5 default), so C++ overrides
(StartPlay, PostLogin) on ASoccerGameMode never run. This fix re-parents the
BP so the C++ kickoff + cheat-manager-attach code chain is reachable.

Run:
  UnrealEditor-Cmd <project> -ExecutePythonScript=<this> -unattended -stdout
"""

import unreal

BP_PATH = "/Game/Blueprints/Game/BP_SoccerGameMode"
TARGET_PARENT_PATH = "/Script/Edge26.SoccerGameMode"
LOG = "/tmp/reparent_game_mode.log"


with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    if not bp:
        log(f"ERROR: could not load {BP_PATH}")
    else:
        target = unreal.load_class(None, TARGET_PARENT_PATH)
        if not target:
            log(f"ERROR: could not load target parent {TARGET_PARENT_PATH}")
        else:
            log(f"Re-parenting {BP_PATH} -> {TARGET_PARENT_PATH}")
            try:
                unreal.BlueprintEditorLibrary.reparent_blueprint(bp, target)
                # Compile + save.
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
                unreal.EditorAssetLibrary.save_asset(BP_PATH, only_if_is_dirty=False)
                # Verify
                gen = bp.generated_class()
                cdo = unreal.get_default_object(gen)
                log(f"After re-parent CDO MRO: {type(cdo).__mro__}")
                log("Re-parent complete + asset saved.")
            except Exception as e:
                log(f"reparent_blueprint error: {e}")

    log("Done.")
