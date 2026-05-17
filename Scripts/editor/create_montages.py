"""
create_montages.py
Headless UE5 Python script — run via UnrealEditor-Cmd with -ExecutePythonScript.

Phase 3 M10: creates AM_Kick_OutField + AM_GK_DiveSave montages from the two
relevant retargeted Mixamo clips, then assigns them to the two AnimBP CDOs
(ABP_Footballer_MM.KickMontage, ABP_Goalkeeper.GoalkeeperSaveMontage).

Driver for UAnimDatabaseUtility::create_montage_from_sequence (M10).

The C++ helper bypasses UAnimMontageFactory's modal skeleton picker — which
doesn't survive headless invocation — by manually wiring SlotAnimTracks[0] and
calling EnsureStartingSection internally.

Usage:
    "/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
        "/path/to/Edge26.uproject" \
        -ExecutePythonScript="/path/to/Scripts/editor/create_montages.py" \
        -unattended -stdout
"""

import logging
import sys

import unreal

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
LOG_PATH = "/tmp/create_montages.log"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.FileHandler(LOG_PATH, mode="w"),
        logging.StreamHandler(sys.stdout),
    ],
)
log = logging.getLogger("CreateMontages")

log.info("create_montages.py starting")

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
MIXAMO_BASE     = "/Game/Animation/Mixamo_Retargeted"
MONTAGE_PKG     = "/Game/Animation/Montages"

KICK_SRC        = f"{MIXAMO_BASE}/Soccer_Pass_Anim"
KICK_NAME       = "AM_Kick_OutField"

GK_SAVE_SRC     = f"{MIXAMO_BASE}/Goalkeeper_Diving_Save_Anim"
GK_SAVE_NAME    = "AM_GK_DiveSave"

ABP_OUTFIELD    = "/Game/Blueprints/Player/ABP_Footballer_MM"
ABP_GK          = "/Game/Blueprints/Player/ABP_Goalkeeper"

util = unreal.AnimDatabaseUtility


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def load_required(path, expected_type=None):
    obj = unreal.EditorAssetLibrary.load_asset(path)
    if obj is None:
        log.error("FAILED to load %s", path)
        return None
    if expected_type and not isinstance(obj, expected_type):
        log.error("%s is %s, expected %s", path, type(obj).__name__, expected_type.__name__)
        return None
    log.info("Loaded %s (%s)", path, type(obj).__name__)
    return obj


def create_montage(source_path, target_name, target_pkg):
    src = load_required(source_path, unreal.AnimSequence)
    if src is None:
        return None
    log.info(
        "Creating montage %s/%s from %s (len=%.3fs) ...",
        target_pkg, target_name, source_path, src.get_play_length(),
    )
    montage = util.create_montage_from_sequence(src, target_pkg, target_name)
    if montage is None:
        log.error("create_montage_from_sequence returned None for %s", target_name)
        return None
    log.info("  -> %s", montage.get_path_name())
    return montage


def assign_montage_to_cdo(abp_path, prop_name, montage):
    """Write to a UAnimBlueprint's generated-class CDO so the AnimBP-instanced
    AnimInstance ends up with the montage assigned by default.

    UE5 binds UPROPERTY defaults via the generated-class CDO (not the BP itself
    — the BP is the asset wrapper). We need to:
      1. Load the AnimBP asset.
      2. Compile it (in case prior milestones left it dirty).
      3. Resolve generated_class().
      4. get_default_object(cls).set_editor_property(prop_name, montage).
      5. Mark BP dirty + compile + save so the CDO write persists.
    """
    abp = load_required(abp_path, unreal.AnimBlueprint)
    if abp is None:
        return False

    # Compile first so generated_class is fresh.
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    gen_cls = abp.generated_class()
    if gen_cls is None:
        log.error("AnimBP %s has no generated_class — was it compiled?", abp_path)
        return False

    cdo = unreal.get_default_object(gen_cls)
    if cdo is None:
        log.error("get_default_object returned None for %s", gen_cls.get_name())
        return False

    cdo.set_editor_property(prop_name, montage)
    log.info("  %s.CDO.%s = %s", abp.get_name(), prop_name, montage.get_name())

    # Mark BP dirty so the CDO write survives the save.
    abp.modify()
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    saved = util.save_anim_blueprint_asset(abp)
    log.info("  saved %s (ok=%s)", abp_path, saved)
    return saved


# ---------------------------------------------------------------------------
# 1. Create both montages.
# ---------------------------------------------------------------------------
kick_montage = create_montage(KICK_SRC, KICK_NAME, MONTAGE_PKG)
gk_montage   = create_montage(GK_SAVE_SRC, GK_SAVE_NAME, MONTAGE_PKG)

if kick_montage is None or gk_montage is None:
    log.error("Montage creation failed; cannot continue.")
    sys.exit(1)

# ---------------------------------------------------------------------------
# 2. Assign montages to CDOs of the two AnimBPs.
# ---------------------------------------------------------------------------
log.info("--- Assigning ABP_Footballer_MM.KickMontage ---")
ok1 = assign_montage_to_cdo(ABP_OUTFIELD, "KickMontage", kick_montage)

log.info("--- Assigning ABP_Goalkeeper.GoalkeeperSaveMontage ---")
ok2 = assign_montage_to_cdo(ABP_GK, "GoalkeeperSaveMontage", gk_montage)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
log.info("=" * 60)
log.info("Montages created    : %s, %s", kick_montage.get_name(), gk_montage.get_name())
log.info("CDO assignments     : ABP_Footballer_MM=%s, ABP_Goalkeeper=%s", ok1, ok2)
log.info("Log: %s", LOG_PATH)

if not (ok1 and ok2):
    sys.exit(2)
