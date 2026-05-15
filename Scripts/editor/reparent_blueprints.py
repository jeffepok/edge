# reparent_blueprints.py — re-parents Edge26 Blueprint assets to the new
# visual-shell classes after the v0 sim-core refactor.
#
# Run:
#   "/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
#       "$PWD/Edge26.uproject" -run=PythonScript -script="Scripts/editor/reparent_blueprints.py"
#
# Idempotent: re-parents only if the current parent doesn't already match.
import unreal

REPARENT = [
    ("/Game/Blueprints/Player/BP_Footballer",          "/Script/Edge26.FootballerVisual"),
    ("/Game/Blueprints/AI/BP_OpponentFootballer",      "/Script/Edge26.FootballerVisual"),
    ("/Game/Blueprints/Ball/BP_SoccerBall",            "/Script/Edge26.SoccerBallVisual"),
]


def reparent(asset_path: str, new_class_path: str) -> None:
    bp = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not bp:
        unreal.log_warning(f"Asset not found: {asset_path}")
        return
    new_class = unreal.load_object(None, new_class_path)
    if not new_class:
        unreal.log_warning(f"Class not found: {new_class_path}")
        return
    if bp.get_editor_property("ParentClass") == new_class:
        unreal.log(f"Already re-parented: {asset_path}")
        return
    bp.set_editor_property("ParentClass", new_class)
    unreal.EditorAssetLibrary.save_asset(asset_path)
    unreal.log(f"Re-parented {asset_path} -> {new_class_path}")


for asset_path, new_class in REPARENT:
    reparent(asset_path, new_class)

unreal.log("reparent_blueprints.py: done")
