"""
populate_mmdbs_football.py
Headless UE5 Python script — run via UnrealEditor-Cmd with -ExecutePythonScript.

Phase 3 T9.2: populates MMDB_Outfield with 5 outfield retargeted football clips
(stacking on top of the 17 locomotion clips already there from M4), and
MMDB_Goalkeeper with 4 GK retargeted clips. For MMDB_Goalkeeper, also creates
MMSchema_Goalkeeper if not yet present (mirrors the M4 schema-creation flow).

Driver for UAnimDatabaseUtility helpers (M4 + M7 + T9.2):
  - create_schema_with_default_channels
  - set_database_schema
  - add_sequence_to_database
  - save_database_asset

Usage:
    "/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
        "/path/to/Edge26.uproject" \
        -ExecutePythonScript="/path/to/Scripts/editor/populate_mmdbs_football.py" \
        -unattended -stdout
"""

import logging
import sys

import unreal

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
LOG_PATH = "/tmp/populate_mmdbs_football.log"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.FileHandler(LOG_PATH, mode="w"),
        logging.StreamHandler(sys.stdout),
    ],
)
log = logging.getLogger("PopulateMMDBsFootball")

log.info("populate_mmdbs_football.py starting")

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
MIXAMO_BASE     = "/Game/Animation/Mixamo_Retargeted"
MM_PKG          = "/Game/Animation/MotionMatching"
OUTFIELD_DB     = f"{MM_PKG}/MMDB_Outfield"
GK_DB           = f"{MM_PKG}/MMDB_Goalkeeper"
GK_SCHEMA_NAME  = "MMSchema_Goalkeeper"
SKELETON_PATH   = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"

# 5 outfield action/stance clips (non-looping; idle is the only one that loops).
OUTFIELD_CLIPS = [
    ("Soccer_Pass_Anim",         False),
    ("Soccer_Header_Anim",       False),
    ("Soccer_Tackle_Anim",       False),
    ("Strike_Foward_Jog_Anim",   False),
    ("Soccer_Idle_Anim",         True),
]

# 4 GK clips (idle loops; rest are one-shots).
GK_CLIPS = [
    ("Goalkeeper_Diving_Save_Anim",        False),
    ("Goalkeeper_Diving_Save__1__Anim",    False),
    ("Goalkeeper_Idle_Anim",               True),
    ("Goalkeeper_Overhand_Throw__1__Anim", False),
]

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


def ensure_schema(db, schema_pkg, schema_name, skeleton):
    """Returns a schema on the database (creates one if missing)."""
    current = db.get_editor_property("schema")
    if current is not None:
        log.info("DB %s already has schema %s", db.get_name(), current.get_name())
        return current

    full = f"{schema_pkg}/{schema_name}"
    existing = unreal.EditorAssetLibrary.load_asset(full)
    if existing and isinstance(existing, unreal.PoseSearchSchema):
        log.info("Reusing existing schema %s", full)
        util.set_database_schema(db, existing)
        util.save_database_asset(db)
        return existing

    log.info("Creating new schema %s ...", full)
    schema = util.create_schema_with_default_channels(skeleton, schema_pkg, schema_name)
    if schema is None:
        log.error("create_schema_with_default_channels returned None for %s", full)
        return None
    util.set_database_schema(db, schema)
    util.save_database_asset(db)
    return schema


def populate(db, anim_entries, label):
    added = 0
    failed = []
    for name, looping in anim_entries:
        full_path = f"{MIXAMO_BASE}/{name}"
        seq = load_required(full_path, unreal.AnimSequence)
        if seq is None:
            failed.append(full_path)
            continue
        util.add_sequence_to_database(db, seq, looping)
        log.info("  %s += [looping=%s] %s", label, looping, name)
        added += 1
    return added, failed


# ---------------------------------------------------------------------------
# 1. MMDB_Outfield — append 5 retargeted football clips on top of existing 17.
# ---------------------------------------------------------------------------
log.info("--- MMDB_Outfield ---")
outfield_db = load_required(OUTFIELD_DB, unreal.PoseSearchDatabase)
if outfield_db is None:
    log.error("Cannot continue — MMDB_Outfield missing. Aborting.")
    sys.exit(1)

# Outfield schema already exists (M4); just validate.
of_schema = outfield_db.get_editor_property("schema")
if of_schema is None:
    log.error("MMDB_Outfield has no schema set — should have been wired in M4. Aborting.")
    sys.exit(1)
log.info("MMDB_Outfield schema = %s", of_schema.get_name())

of_added, of_failed = populate(outfield_db, OUTFIELD_CLIPS, "MMDB_Outfield")
ok = util.save_database_asset(outfield_db)
log.info("Saved MMDB_Outfield (ok=%s)", ok)

# ---------------------------------------------------------------------------
# 2. MMDB_Goalkeeper — ensure schema, then populate 4 clips.
# ---------------------------------------------------------------------------
log.info("--- MMDB_Goalkeeper ---")
gk_db = load_required(GK_DB, unreal.PoseSearchDatabase)
if gk_db is None:
    log.error("Cannot continue — MMDB_Goalkeeper missing. Aborting.")
    sys.exit(1)

skeleton = load_required(SKELETON_PATH, unreal.Skeleton)
if skeleton is None:
    log.error("Cannot continue — SK_Mannequin missing. Aborting.")
    sys.exit(1)

gk_schema = ensure_schema(gk_db, MM_PKG, GK_SCHEMA_NAME, skeleton)
if gk_schema is None:
    log.error("Failed to attach schema to MMDB_Goalkeeper. Aborting.")
    sys.exit(1)

gk_added, gk_failed = populate(gk_db, GK_CLIPS, "MMDB_Goalkeeper")
ok = util.save_database_asset(gk_db)
log.info("Saved MMDB_Goalkeeper (ok=%s)", ok)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
log.info("=" * 60)
log.info("MMDB_Outfield   : +%d clips (failed=%d)", of_added, len(of_failed))
log.info("MMDB_Goalkeeper : +%d clips (failed=%d)", gk_added, len(gk_failed))
log.info("Log: %s", LOG_PATH)

if of_failed or gk_failed:
    for f in of_failed:
        log.error("  MISSING: %s", f)
    for f in gk_failed:
        log.error("  MISSING: %s", f)
    sys.exit(2)
