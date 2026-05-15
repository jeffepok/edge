"""Bake APitchMarkings into L_Pitch.

Spawns one APitchMarkings actor at the world origin (idempotent — does nothing if
one is already in the level). The actor's OnConstruction populates its line ISM
component, and save_current_level persists everything to L_Pitch.umap so the
markings are visible in the editor without playing.

Run via:
  UnrealEditor Edge26.uproject -run=pythonscript -script=scripts/bake_pitch_markings.py
"""

import unreal
import traceback

LEVEL_PATH = '/Game/Levels/L_Pitch'
PITCH_MARKINGS_CLASS_PATH = '/Script/Edge26.PitchMarkings'

LOG_PATH = '/tmp/bake_pitch_markings.log'
_log_file = open(LOG_PATH, 'w')

def log(msg):
    line = "[bake_pitch_markings] " + str(msg)
    print(line)
    _log_file.write(line + "\n")
    _log_file.flush()

try:
    log("starting")

    loaded = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    log("loaded {}: {}".format(LEVEL_PATH, loaded))
    if not loaded:
        raise RuntimeError("Failed to load level " + LEVEL_PATH)

    pitch_class = unreal.load_class(None, PITCH_MARKINGS_CLASS_PATH)
    log("loaded class {}: {}".format(PITCH_MARKINGS_CLASS_PATH, pitch_class))
    if not pitch_class:
        raise RuntimeError("Could not load PitchMarkings class")

    editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = editor.get_all_level_actors()

    existing = [a for a in all_actors if a.get_class().get_name() == 'PitchMarkings']
    log("existing PitchMarkings actors: {}".format(len(existing)))

    if existing:
        log("level already has a PitchMarkings — re-running OnConstruction to refresh ISM data")
        for a in existing:
            a.rerun_construction_scripts()
    else:
        spawned = editor.spawn_actor_from_class(pitch_class, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
        log("spawned: {}".format(spawned))
        if spawned:
            spawned.set_actor_label('PitchMarkings')

    saved = unreal.EditorLoadingAndSavingUtils.save_current_level()
    log("save_current_level returned: {}".format(saved))

    log("done")
except Exception as ex:
    log("EXCEPTION: " + repr(ex))
    log(traceback.format_exc())
finally:
    _log_file.close()
