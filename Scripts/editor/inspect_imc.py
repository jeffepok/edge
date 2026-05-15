import unreal

lines = []
def out(s):
    lines.append(str(s))

def inspect_ia(path):
    ia = unreal.EditorAssetLibrary.load_asset(path)
    if not ia:
        out(f"  IA not found: {path}")
        return
    name = ia.get_name()
    try:
        value_type = str(ia.value_type)
    except Exception as e:
        value_type = f"?({e})"
    out(f"=== {name} ({value_type}) at {path} ===")

    for attr in ["modifiers", "default_modifiers", "triggers", "default_triggers"]:
        try:
            v = ia.get_editor_property(attr)
            if v is None:
                continue
            n = len(v) if hasattr(v, "__len__") else None
            out(f"  {attr}: type={type(v).__name__} len={n}")
            for i, item in enumerate(v or []):
                if item:
                    out(f"    [{i}] {item.get_class().get_name()}")
        except Exception:
            pass

for p in [
    "/Game/Input/Actions/IA_Move",
    "/Game/Input/IA_Look",
    "/Game/Input/IA_Sprint",
    "/Game/Input/IA_Pass",
    "/Game/Input/IA_Shoot",
    "/Game/Input/IA_Chip",
]:
    inspect_ia(p)

with open("/tmp/imc_dump.txt", "w") as f:
    f.write("\n".join(lines))
unreal.log(f"Wrote {len(lines)} lines.")
