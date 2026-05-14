"""
Edge26 — pitch grass material.

Creates /Game/Materials/M_Pitch_Grass:
    - Procedural mowed-stripe grass (no texture asset required).
    - Two-tone dark/light green driven by WorldPosition.Y so stripes run
      across the pitch's long axis.
    - High roughness, no specular pop.

Then finds the largest StaticMeshActor in the current level (assumed to be
the pitch plane) and assigns the material to its element 0.

Run from editor Python console:
    py "/Users/jeffersonaddai-poku/Desktop/projects/Games/Edge26/Content/Python/setup_pitch_material.py"
"""

import unreal


MATERIAL_PATH = '/Game/Materials/M_Pitch_Grass'
MATERIAL_DIR  = '/Game/Materials'

DARK_GREEN  = unreal.LinearColor(0.045, 0.180, 0.060, 1.0)
LIGHT_GREEN = unreal.LinearColor(0.075, 0.260, 0.085, 1.0)
STRIPE_FREQUENCY = 0.0035   # smaller = wider stripes; tuned for ~3 m bands


def log(m): unreal.log('[Edge26 grass] ' + m)
def warn(m): unreal.log_warning('[Edge26 grass] ' + m)
def err(m): unreal.log_error('[Edge26 grass] ' + m)


def ensure_dir(p):
    if not unreal.EditorAssetLibrary.does_directory_exist(p):
        unreal.EditorAssetLibrary.make_directory(p)


def create_or_load_material():
    # Always recreate. Earlier failed runs can leave a partial asset on disk;
    # easiest to nuke and rebuild than to detect partial state.
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH):
        log('Existing material found; deleting and recreating.')
        unreal.EditorAssetLibrary.delete_asset(MATERIAL_PATH)

    ensure_dir(MATERIAL_DIR)

    factory = unreal.MaterialFactoryNew()
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_Pitch_Grass', MATERIAL_DIR, unreal.Material, factory
    )
    if not material:
        err('Failed to create material asset.')
        return None

    mel = unreal.MaterialEditingLibrary

    # World position so stripes are tied to world space, not UV scale.
    world_pos = mel.create_material_expression(
        material, unreal.MaterialExpressionWorldPosition, -1000, 0
    )

    # Mask Y component.
    mask_y = mel.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -800, 0
    )
    mask_y.set_editor_property('R', False)
    mask_y.set_editor_property('G', True)
    mask_y.set_editor_property('B', False)
    mask_y.set_editor_property('A', False)
    mel.connect_material_expressions(world_pos, '', mask_y, '')

    # Multiply by frequency.
    freq_const = mel.create_material_expression(
        material, unreal.MaterialExpressionConstant, -800, 120
    )
    freq_const.set_editor_property('R', STRIPE_FREQUENCY)

    mul_freq = mel.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -600, 50
    )
    mel.connect_material_expressions(mask_y, '', mul_freq, 'A')
    mel.connect_material_expressions(freq_const, '', mul_freq, 'B')

    # Sine.
    sine = mel.create_material_expression(
        material, unreal.MaterialExpressionSine, -400, 50
    )
    mel.connect_material_expressions(mul_freq, '', sine, '')

    # Map sine [-1,1] → [0,1] using ConstantBiasScale (Bias 1, Scale 0.5).
    bias_scale = mel.create_material_expression(
        material, unreal.MaterialExpressionConstantBiasScale, -200, 50
    )
    bias_scale.set_editor_property('Bias', 1.0)
    bias_scale.set_editor_property('Scale', 0.5)
    mel.connect_material_expressions(sine, '', bias_scale, '')

    # Lerp between dark and light green using the bias_scale as alpha.
    dark = mel.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -400, -200
    )
    dark.set_editor_property('Constant', DARK_GREEN)

    light = mel.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -400, -350
    )
    light.set_editor_property('Constant', LIGHT_GREEN)

    lerp = mel.create_material_expression(
        material, unreal.MaterialExpressionLinearInterpolate, 0, -100
    )
    mel.connect_material_expressions(dark, '', lerp, 'A')
    mel.connect_material_expressions(light, '', lerp, 'B')
    mel.connect_material_expressions(bias_scale, '', lerp, 'Alpha')

    # Hook lerp into BaseColor.
    mel.connect_material_property(lerp, '', unreal.MaterialProperty.MP_BASE_COLOR)

    # Roughness — flat 0.85 looks like dry grass under sky lighting.
    rough = mel.create_material_expression(
        material, unreal.MaterialExpressionConstant, 0, 80
    )
    rough.set_editor_property('R', 0.85)
    mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)

    # Specular subtle.
    spec = mel.create_material_expression(
        material, unreal.MaterialExpressionConstant, 0, 200
    )
    spec.set_editor_property('R', 0.15)
    mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)

    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH)
    log('Created and saved M_Pitch_Grass.')
    return material


def find_pitch_plane():
    """Find the pitch plane. Filter to FLAT actors (Z extent << horizontal),
    excluding sky/dome actors. Picks the largest remaining by horizontal area."""
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not sub:
        return None

    SKIP_LABEL_PREFIXES = ('Sky', 'SM_Sky', 'BP_Sky')
    FLATNESS_RATIO_MAX = 0.15  # Z must be < 15% of max horizontal extent.

    best = None
    best_extent = 0.0
    for actor in sub.get_all_level_actors():
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        label = actor.get_actor_label()
        if any(label.startswith(p) for p in SKIP_LABEL_PREFIXES):
            continue
        comp = actor.static_mesh_component
        if not comp or not comp.static_mesh:
            continue

        origin, box_extent = actor.get_actor_bounds(only_colliding_components=False)
        horiz = max(box_extent.x, box_extent.y)
        if horiz <= 0:
            continue
        flatness = box_extent.z / horiz
        if flatness > FLATNESS_RATIO_MAX:
            continue   # this is a tall thing (sphere, cube, post), not a pitch

        area = box_extent.x * box_extent.y
        if area > best_extent:
            best_extent = area
            best = actor

    if best is None:
        unreal.log_warning('[Edge26 grass] No flat pitch plane found. '
                           'Apply M_Pitch_Grass manually to your floor actor.')
    else:
        unreal.log('[Edge26 grass] Selected pitch plane: ' + best.get_actor_label())
    return best


def apply_material(material, actor):
    if not actor:
        warn('No pitch plane found in level. Apply M_Pitch_Grass manually.')
        return False
    comp = actor.static_mesh_component
    comp.set_material(0, material)
    log('Applied grass material to ' + actor.get_actor_label())
    return True


def main():
    log('Building pitch material.')
    mat = create_or_load_material()
    if not mat:
        return

    plane = find_pitch_plane()
    apply_material(mat, plane)

    level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_sub:
        level_sub.save_current_level()
    log('Done.')


if __name__ == '__main__':
    main()
