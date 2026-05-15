# configure_blueprints.py — post-merge fixes for misconfigured assets.
import unreal


def fix_opponent_blueprint():
    asset_path = "/Game/Blueprints/AI/BP_OpponentFootballer"
    bp = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not bp:
        unreal.log_warning(f"Asset not found: {asset_path}")
        return
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("auto_possess_player", unreal.AutoReceiveInput.DISABLED)
    cdo.set_editor_property("ControllerIndex", 1)
    unreal.EditorAssetLibrary.save_asset(asset_path)
    unreal.log(f"Configured {asset_path}: AutoPossessPlayer=Disabled, ControllerIndex=1")


def fix_ia_look_value_type():
    # IA_Look is BOOLEAN but bound to mouse axes in IMC_Player. EnhancedInput
    # injects a Scalar modifier to convert the axis-to-bool, which ensure-fails
    # in the IMC tick — killing the whole input chain.
    # Mouse XY *should* be Axis2D. Fix the type.
    path = "/Game/Input/IA_Look"
    ia = unreal.EditorAssetLibrary.load_asset(path)
    if not ia:
        unreal.log_warning(f"Asset not found: {path}")
        return
    ia.set_editor_property("value_type", unreal.InputActionValueType.AXIS2D)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log(f"Set {path}.value_type = Axis2D (was Boolean)")


unreal.log("=== configure_blueprints.py: start ===")
fix_opponent_blueprint()
fix_ia_look_value_type()
unreal.log("=== configure_blueprints.py: done ===")
