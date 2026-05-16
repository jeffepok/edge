# spawn_22_players.py — ensures L_Pitch has exactly 22 BP_Footballer instances
# laid out at 4-3-3 slot positions (rough approximation; sim will overwrite
# exact positions at BeginPlay via ResetAllPlayersTo4_3_3).

import unreal
import os
import sys

LEVEL_PATH = "/Game/Levels/L_Pitch"
BP_FOOTBALLER = "/Game/Blueprints/Player/BP_Footballer"

LOG_FILE = "/tmp/spawn_22_players_log.txt"

# Approximate 4-3-3 slot positions in cm (X = up the pitch, Y = sideline).
# Pitch is 10500 cm x 6800 cm.
HOME_SLOTS = [
    (-4988,    0,  10),   # GK
    (-3413, -510,  10),   # CB
    (-3413,  510,  10),   # CB
    (-2888,-2210,  10),   # FB_L
    (-2888, 2210,  10),   # FB_R
    (-1313,    0,  10),   # CDM
    ( -525,-1020,  10),   # CM
    ( -525, 1020,  10),   # CM
    ( 2100,-2380,  10),   # W_L
    ( 2100, 2380,  10),   # W_R
    ( 2625,    0,  10),   # ST
]
AWAY_SLOTS = [(-x, y, z) for (x, y, z) in HOME_SLOTS]

_log_fh = open(LOG_FILE, "w", buffering=1)

def log(msg):
    """Write to file log, unreal log, and stderr."""
    _log_fh.write(msg + "\n")
    _log_fh.flush()
    unreal.log(msg)

def log_err(msg):
    _log_fh.write("ERROR: " + msg + "\n")
    _log_fh.flush()
    unreal.log_error(msg)

def main():
    log(f"Script started — opening level {LEVEL_PATH}")

    # Load the level explicitly (required in headless/commandlet mode).
    success = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    log(f"load_map returned: {success}")
    if not success:
        log_err(f"Failed to open level {LEVEL_PATH}")
        return

    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_FOOTBALLER)
    log(f"bp_class loaded: {bp_class is not None}")
    if not bp_class:
        log_err(f"Could not load {BP_FOOTBALLER}")
        return

    # Remove any existing Footballer actors to avoid duplicates.
    all_actors = unreal.EditorLevelLibrary.get_all_level_actors()
    log(f"Total actors in level: {len(all_actors)}")
    removed = 0
    for actor in all_actors:
        cls = actor.get_class()
        if cls and "Footballer" in cls.get_name():
            unreal.EditorLevelLibrary.destroy_actor(actor)
            removed += 1
    log(f"Removed {removed} existing Footballer-like actors")

    # Spawn 22 new ones.
    spawned = 0
    failed = 0
    for slots, team_id in [(HOME_SLOTS, 0), (AWAY_SLOTS, 1)]:
        for (x, y, z) in slots:
            loc = unreal.Vector(float(x), float(y), float(z))
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(bp_class, loc)
            if actor:
                # AFootballerVisual has a ControllerIndex int32 UPROPERTY.
                try:
                    actor.set_editor_property("ControllerIndex", spawned)
                except Exception as e:
                    log(f"Could not set ControllerIndex on actor {spawned}: {e}")
                spawned += 1
            else:
                failed += 1
                log(f"spawn_actor_from_class returned None for slot index {spawned + failed - 1}")
    log(f"Spawned {spawned} footballers ({failed} failures)")

    # Save using EditorLoadingAndSavingUtils (works correctly in commandlet mode).
    saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log(f"save_dirty_packages returned: {saved}")
    if saved:
        log("Saved L_Pitch")
    else:
        log_err("save_dirty_packages returned False")

    _log_fh.close()

main()
