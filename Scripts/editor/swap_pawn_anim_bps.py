"""Phase 3 M11: Switch BP_Footballer pawns to ABP_Footballer_MM for outfield,
and re-spawn the two goalkeeper slots (ControllerIndex 0 and 11) as
BP_Goalkeeper so they pick up ABP_Goalkeeper via the BP class's default
SkeletalMesh.AnimInstanceClass.

Run WITHOUT -nullrhi (SpawnActor crashes under -nullrhi in 5.7; see Phase 2 M10).
"""

import unreal

LEVEL_PATH    = "/Game/Levels/L_Pitch"
ABP_OUTFIELD  = "/Game/Blueprints/Player/ABP_Footballer_MM"
BP_GOALKEEPER = "/Game/Blueprints/Player/BP_Goalkeeper"
LOG_FILE      = "/tmp/swap_pawn_anim_bps.log"

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
    log(f"Script started - opening level {LEVEL_PATH}")

    success = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    log(f"load_map returned: {success}")
    if not success:
        log_err(f"Failed to open level {LEVEL_PATH}")
        return

    outfield_anim_cls = unreal.EditorAssetLibrary.load_blueprint_class(ABP_OUTFIELD)
    log(f"Outfield anim BP class loaded: {outfield_anim_cls is not None}")
    if not outfield_anim_cls:
        log_err(f"Could not load {ABP_OUTFIELD}")
        return

    gk_bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_GOALKEEPER)
    log(f"GK BP class loaded: {gk_bp_class is not None}")
    if not gk_bp_class:
        log_err(f"Could not load {BP_GOALKEEPER}")
        return

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actor_subsystem.get_all_level_actors()
    log(f"Total actors in level: {len(all_actors)}")

    # Match BP_Footballer_C (and any subclass starting with "BP_Footballer").
    # Note: BP_Goalkeeper_C does NOT start with "BP_Footballer", so re-runs are safe.
    footballers = [a for a in all_actors
                   if a.get_class().get_name().startswith("BP_Footballer")]
    log(f"Found {len(footballers)} footballer pawns")

    updated = 0
    replaced = 0
    spawn_failed = 0

    for a in footballers:
        idx = a.get_editor_property("ControllerIndex")
        if idx == 0 or idx == 11:
            pos = a.get_actor_location()
            rot = a.get_actor_rotation()
            label = a.get_actor_label()
            log(f"  GK slot {idx}: destroying old pawn '{label}' at "
                f"({pos.x:.0f},{pos.y:.0f},{pos.z:.0f})")
            actor_subsystem.destroy_actor(a)
            new_gk = unreal.EditorLevelLibrary.spawn_actor_from_class(
                gk_bp_class, pos, rot)
            if new_gk:
                new_gk.set_editor_property("ControllerIndex", idx)
                new_gk.set_actor_label(f"BP_Goalkeeper_{idx}")
                replaced += 1
                log(f"  GK slot {idx}: spawned BP_Goalkeeper")
            else:
                spawn_failed += 1
                log_err(f"  GK slot {idx}: SpawnActor returned None")
        else:
            mesh = a.get_component_by_class(unreal.SkeletalMeshComponent)
            if mesh:
                mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_BLUEPRINT)
                mesh.set_anim_instance_class(outfield_anim_cls)
                updated += 1
            else:
                log_err(f"  Outfielder slot {idx}: no SkeletalMeshComponent")

    # Save: prefer save_dirty_packages (proven from spawn_22_players.py).
    saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log(f"save_dirty_packages returned: {saved}")

    log(f"Updated {updated} outfielders, replaced {replaced} GKs"
        f" (spawn failures: {spawn_failed})")
    log("Done.")
    _log_fh.close()


main()
