"""Inspect BP_SoccerGameMode: parent class + overridden events.

Goal: confirm whether BP inherits from ASoccerGameMode (so C++ Super calls work)
and identify any events that might be intercepting StartPlay/PostLogin without
calling Super.

Run:
  UnrealEditor-Cmd <project> -ExecutePythonScript=<this> -unattended -stdout
"""

import unreal

BP_PATH = "/Game/Blueprints/Game/BP_SoccerGameMode"
LOG = "/tmp/inspect_bp_game_mode.log"

with open(LOG, "w", buffering=1) as f:
    def log(msg):
        f.write(msg + "\n")
        unreal.log(msg)

    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    if not bp:
        log(f"ERROR: could not load {BP_PATH}")
    else:
        log(f"Loaded: {bp}")

        # Generated class + parent.
        gen_class = bp.generated_class()
        log(f"Generated class: {gen_class}")
        if gen_class:
            log(f"Generated class path: {gen_class.get_path_name()}")
            try:
                # ClassReflectionHelpers / direct __class__ tree
                cdo = unreal.get_default_object(gen_class)
                log(f"CDO type: {type(cdo).__mro__}")
            except Exception as e:
                log(f"mro error: {e}")

        # CDO inspection — see if any GameMode properties were tweaked.
        if gen_class:
            cdo = unreal.get_default_object(gen_class)
            for prop in ("player_controller_class", "default_pawn_class", "game_state_class", "hud_class"):
                try:
                    v = cdo.get_editor_property(prop)
                    log(f"{prop}: {v}")
                except Exception as e:
                    log(f"({prop} not readable: {e})")

        log("Done.")
