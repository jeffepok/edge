# add_switch_input.py — creates IA_Switch action asset and binds it to R key in IMC_Player.
# Safe to re-run: skips creation/binding if already present.
import unreal

# ---- 1. Create IA_Switch if it doesn't exist ----
ia_path = "/Game/Input/Actions/IA_Switch"
if not unreal.EditorAssetLibrary.does_asset_exist(ia_path):
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.InputAction_Factory()
    asset_tools.create_asset(
        asset_name="IA_Switch",
        package_path="/Game/Input/Actions",
        asset_class=unreal.InputAction,
        factory=factory,
    )
    unreal.log("Created IA_Switch asset.")
else:
    unreal.log("IA_Switch already exists; skipping creation.")

ia = unreal.EditorAssetLibrary.load_asset(ia_path)
if not ia:
    unreal.log_error("Failed to load IA_Switch — aborting.")
    raise SystemExit(1)

ia.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
unreal.EditorAssetLibrary.save_loaded_asset(ia)
unreal.log("IA_Switch.value_type = BOOLEAN")

# ---- 2. Bind IA_Switch -> R in IMC_Player ----
imc = unreal.EditorAssetLibrary.load_asset("/Game/Input/IMC_Player")
if not imc:
    unreal.log_error("Failed to load IMC_Player — aborting.")
    raise SystemExit(1)

# IMC_Player stores mappings in 'default_key_mappings.mappings'.
key_mappings_struct = imc.get_editor_property("default_key_mappings")
mappings = key_mappings_struct.get_editor_property("mappings")

# Skip if already present.
already_bound = False
for m in mappings:
    existing_action = m.get_editor_property("action")
    if existing_action and "IA_Switch" in existing_action.get_path_name():
        unreal.log("IA_Switch already bound in IMC_Player; skipping.")
        already_bound = True
        break

if not already_bound:
    # Use imc.map_key() which correctly mutates the asset in-place.
    r_key = unreal.Key()
    r_key.set_editor_property("key_name", "R")
    imc.map_key(ia, r_key)
    # save_asset with only_if_is_dirty=False ensures the file is written to disk
    # even if the package hasn't been formally marked dirty this session.
    unreal.EditorAssetLibrary.save_asset("/Game/Input/IMC_Player", only_if_is_dirty=False)
    unreal.log("Bound IA_Switch -> R in IMC_Player.")
