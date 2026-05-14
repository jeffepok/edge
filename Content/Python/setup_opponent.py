"""
Edge26 — milestone 6 setup script.

Creates BP_OpponentFootballer (a child of BP_Footballer reparented to
AOpponentFootballerCharacter so it auto-spawns its AI controller) and
spawns one instance in the currently open level.

Run from the editor's Python console:
    py /Game/Python/setup_opponent.py

Or from the command line:
    "/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
        "$PWD/Edge26.uproject" \
        -ExecutePythonScript="$PWD/Content/Python/setup_opponent.py" \
        -unattended -nopause -nullrhi

Idempotent: re-running won't duplicate the asset, but will re-spawn an
instance if no opponent is currently in the level.
"""

import unreal


PLAYER_BP_PATH       = '/Game/Blueprints/Player/BP_Footballer'
OPPONENT_BP_DIR      = '/Game/Blueprints/AI'
OPPONENT_BP_PATH     = OPPONENT_BP_DIR + '/BP_OpponentFootballer'
OPPONENT_PARENT_CLS  = '/Script/Edge26.OpponentFootballerCharacter'

SPAWN_LOC = unreal.Vector(0.0, -3000.0, 110.0)
SPAWN_ROT = unreal.Rotator(0.0, 0.0, 0.0)


def log(msg):
    unreal.log('[Edge26 setup] ' + msg)


def warn(msg):
    unreal.log_warning('[Edge26 setup] ' + msg)


def err(msg):
    unreal.log_error('[Edge26 setup] ' + msg)


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def find_player_blueprint():
    if not unreal.EditorAssetLibrary.does_asset_exist(PLAYER_BP_PATH):
        err('BP_Footballer not found at ' + PLAYER_BP_PATH +
            '. Adjust PLAYER_BP_PATH at the top of this script if yours is elsewhere.')
        return None
    return unreal.EditorAssetLibrary.load_asset(PLAYER_BP_PATH)


def create_or_load_opponent_bp():
    if unreal.EditorAssetLibrary.does_asset_exist(OPPONENT_BP_PATH):
        log('Opponent BP already exists; loading.')
        return unreal.EditorAssetLibrary.load_asset(OPPONENT_BP_PATH)

    player_bp = find_player_blueprint()
    if not player_bp:
        return None

    ensure_directory(OPPONENT_BP_DIR)

    # Duplicate so we inherit the player's mesh / anim / IMC defaults — saves
    # designers from re-assigning everything for the opponent.
    duplicated = unreal.EditorAssetLibrary.duplicate_asset(PLAYER_BP_PATH, OPPONENT_BP_PATH)
    if not duplicated:
        err('Failed to duplicate BP_Footballer.')
        return None

    log('Duplicated BP_Footballer -> BP_OpponentFootballer.')
    return duplicated


def reparent_blueprint(blueprint):
    """Reparent BP_OpponentFootballer to AOpponentFootballerCharacter so the
    AI controller auto-spawns. Requires BlueprintEditorLibrary."""
    parent_cls = unreal.load_object(None, OPPONENT_PARENT_CLS)
    if not parent_cls:
        err('Could not load parent class ' + OPPONENT_PARENT_CLS +
            '. Has the project compiled with the AI changes?')
        return False

    try:
        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent_cls)
    except Exception as e:
        warn('reparent_blueprint failed: ' + str(e) +
             '. Open BP_OpponentFootballer and reparent manually: '
             'File -> Reparent Blueprint -> OpponentFootballerCharacter.')
        return False

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_asset(OPPONENT_BP_PATH)
    log('Reparented + compiled + saved opponent BP.')
    return True


def opponent_already_in_level():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not sub:
        return False
    for actor in sub.get_all_level_actors():
        cls = actor.get_class()
        # Match by class path so we accept BP_OpponentFootballer or any subclass.
        if cls and 'OpponentFootballer' in cls.get_name():
            return True
    return False


def spawn_opponent_in_level():
    if opponent_already_in_level():
        log('Opponent already present in level; not respawning.')
        return True

    actor_cls = unreal.EditorAssetLibrary.load_blueprint_class(OPPONENT_BP_PATH)
    if not actor_cls:
        err('Could not load class for ' + OPPONENT_BP_PATH)
        return False

    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not sub:
        err('EditorActorSubsystem unavailable.')
        return False

    spawned = sub.spawn_actor_from_class(actor_cls, SPAWN_LOC, SPAWN_ROT)
    if not spawned:
        err('Failed to spawn opponent in level.')
        return False

    spawned.set_actor_label('Opponent_AI_01')
    log('Spawned opponent at ' + str(SPAWN_LOC))
    return True


def main():
    log('Starting milestone 6 setup.')

    bp = create_or_load_opponent_bp()
    if not bp:
        err('Aborting.')
        return

    if not reparent_blueprint(bp):
        warn('Reparent failed; cannot guarantee AI possession. '
             'Continuing to spawn anyway — open the BP and reparent manually.')

    if not spawn_opponent_in_level():
        return

    # Save the current level so the opponent placement persists.
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_subsystem:
        level_subsystem.save_current_level()
    log('Done. Press Play and the opponent should chase the ball.')


if __name__ == '__main__':
    main()
