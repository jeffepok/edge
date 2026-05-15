"""Align BP_GoalTrigger actors in L_Pitch to the goal lines drawn by APitchMarkings.

Run via:
  UnrealEditor Edge26.uproject -run=pythonscript -script=scripts/align_goals.py
"""

import unreal
import traceback

PITCH_HALF_LENGTH = 5250.0  # APitchMarkings::PitchLength * 0.5
GOAL_BACK_OFFSET = 40.0     # APitchMarkings::GoalBackOffset; trigger box X extent
LEVEL_PATH = '/Game/Levels/L_Pitch'

# Always write to a known log file so we can debug regardless of stdout routing.
LOG_PATH = '/tmp/align_goals_script.log'
_log_file = open(LOG_PATH, 'w')

def log(msg):
    line = "[align_goals] " + str(msg)
    print(line)
    _log_file.write(line + "\n")
    _log_file.flush()

try:
    log("starting")

    loaded = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    log("loaded {}: {}".format(LEVEL_PATH, loaded))
    if not loaded:
        raise RuntimeError("Failed to load level {}".format(LEVEL_PATH))

    editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = editor.get_all_level_actors()
    log("level has {} actors".format(len(all_actors)))

    class_counts = {}
    for a in all_actors:
        cn = a.get_class().get_name()
        class_counts[cn] = class_counts.get(cn, 0) + 1
    for cn, n in sorted(class_counts.items()):
        log("  class {}: {}".format(cn, n))

    goals = [a for a in all_actors if a.get_class().get_name() == 'BP_GoalTrigger_C']
    log("BP_GoalTrigger_C instances: {}".format(len(goals)))

    if not goals:
        log("no goals matched — aborting save.")
    else:
        moved = 0
        for g in goals:
            team = g.get_editor_property('DefendingTeamId')
            sign = -1.0 if team == 0 else 1.0
            old_loc = g.get_actor_location()
            new_loc = unreal.Vector(0.0, sign * (PITCH_HALF_LENGTH + GOAL_BACK_OFFSET), old_loc.z)
            g.set_actor_location(new_loc, sweep=False, teleport=True)
            log("  moved {} (team {}): ({:.1f},{:.1f},{:.1f}) -> ({:.1f},{:.1f},{:.1f})".format(
                g.get_actor_label(), team,
                old_loc.x, old_loc.y, old_loc.z,
                new_loc.x, new_loc.y, new_loc.z))
            moved += 1

        saved = unreal.EditorLoadingAndSavingUtils.save_current_level()
        log("moved {} goals; save_current_level returned {}".format(moved, saved))

    log("done")
except Exception as ex:
    log("EXCEPTION: " + repr(ex))
    log(traceback.format_exc())
finally:
    _log_file.close()
