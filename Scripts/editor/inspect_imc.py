import unreal

lines = []
def out(s):
    lines.append(str(s))

imc = unreal.EditorAssetLibrary.load_asset("/Game/Input/IMC_Player")
data = imc.get_editor_property("default_key_mappings")
mappings = data.get_editor_property("mappings") or []
out(f"=== IMC_Player has {len(mappings)} mappings ===")

for i, m in enumerate(mappings):
    action = m.get_editor_property("action")
    action_name = action.get_name() if action else "?"
    value_type = str(action.value_type) if action else "?"
    key = m.get_editor_property("key")
    # FKey has a key_name FName
    key_name = "<unset>"
    try:
        key_name = key.export_text()
    except Exception as e:
        try:
            key_name = key.get_editor_property("key_name")
        except Exception as e2:
            key_name = f"<err {e} / {e2}>"

    # also dump dir of the key
    if i == 0:
        out(f"  FKey dir: {[a for a in dir(key) if not a.startswith('_')]}")

    mods = []
    for mod in (m.get_editor_property("modifiers") or []):
        if mod:
            mods.append(mod.get_class().get_name())
    out(f"  [{i}] action={action_name} ({value_type}) key='{key_name}' modifiers={mods}")

with open("/tmp/imc_dump.txt", "w") as f:
    f.write("\n".join(lines))
