"""
populate_mmdb_outfield.py
Headless UE5 Python script — run via UnrealEditor-Cmd with -ExecutePythonScript.

Populates /Game/Animation/MotionMatching/MMDB_Outfield with 17 locomotion clips
using the UAnimDatabaseUtility C++ helpers exposed via Blueprints/Python.

Usage:
    "/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
        "/path/to/Edge26.uproject" \
        -ExecutePythonScript="/path/to/Scripts/editor/populate_mmdb_outfield.py" \
        -unattended -stdout
"""

import unreal
import logging
import sys
import os

# ---------------------------------------------------------------------------
# Logging setup — file + stdout
# ---------------------------------------------------------------------------
LOG_PATH = "/tmp/populate_mmdb_outfield.log"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.FileHandler(LOG_PATH, mode="w"),
        logging.StreamHandler(sys.stdout),
    ],
)
log = logging.getLogger("PopulateMMDB")

log.info("populate_mmdb_outfield.py starting")

# ---------------------------------------------------------------------------
# Asset paths
# ---------------------------------------------------------------------------
DB_PATH         = "/Game/Animation/MotionMatching/MMDB_Outfield"
SCHEMA_PKG      = "/Game/Animation/MotionMatching"
SCHEMA_NAME     = "MMSchema_Outfield"
SKELETON_PATH   = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"

UNARMED_BASE    = "/Game/Characters/Mannequins/Anims/Unarmed"

ANIMATIONS = [
    # (relative_path, looping)
    ("MM_Idle",                         True),
    # Walk cardinal
    ("Walk/MF_Unarmed_Walk_Fwd",        True),
    ("Walk/MF_Unarmed_Walk_Bwd",        True),
    ("Walk/MF_Unarmed_Walk_Left",       True),
    ("Walk/MF_Unarmed_Walk_Right",      True),
    # Walk diagonal
    ("Walk/MF_Unarmed_Walk_Fwd_Left",   True),
    ("Walk/MF_Unarmed_Walk_Fwd_Right",  True),
    ("Walk/MF_Unarmed_Walk_Bwd_Left",   True),
    ("Walk/MF_Unarmed_Walk_Bwd_Right",  True),
    # Jog cardinal
    ("Jog/MF_Unarmed_Jog_Fwd",         True),
    ("Jog/MF_Unarmed_Jog_Bwd",         True),
    ("Jog/MF_Unarmed_Jog_Left",        True),
    ("Jog/MF_Unarmed_Jog_Right",       True),
    # Jog diagonal
    ("Jog/MF_Unarmed_Jog_Fwd_Left",    True),
    ("Jog/MF_Unarmed_Jog_Fwd_Right",   True),
    ("Jog/MF_Unarmed_Jog_Bwd_Left",    True),
    ("Jog/MF_Unarmed_Jog_Bwd_Right",   True),
]

assert len(ANIMATIONS) == 17, f"Expected 17 animations, got {len(ANIMATIONS)}"

# ---------------------------------------------------------------------------
# Helper — load asset and validate type
# ---------------------------------------------------------------------------
def load_asset(path, expected_type=None):
    obj = unreal.EditorAssetLibrary.load_asset(path)
    if obj is None:
        log.error("Failed to load asset: %s", path)
        return None
    if expected_type and not isinstance(obj, expected_type):
        log.error("Asset %s is %s, expected %s", path, type(obj).__name__, expected_type.__name__)
        return None
    log.info("Loaded %s (%s)", path, type(obj).__name__)
    return obj

# ---------------------------------------------------------------------------
# 1. Load the database
# ---------------------------------------------------------------------------
db = load_asset(DB_PATH, unreal.PoseSearchDatabase)
if db is None:
    log.error("Cannot continue — MMDB_Outfield failed to load. Aborting.")
    sys.exit(1)

# ---------------------------------------------------------------------------
# 2. Load skeleton
# ---------------------------------------------------------------------------
skeleton = load_asset(SKELETON_PATH, unreal.Skeleton)
if skeleton is None:
    log.error("Cannot continue — SK_Mannequin failed to load. Aborting.")
    sys.exit(1)

# ---------------------------------------------------------------------------
# 3. Create (or load existing) schema
# ---------------------------------------------------------------------------
schema_full_path = f"{SCHEMA_PKG}/{SCHEMA_NAME}"
existing_schema = unreal.EditorAssetLibrary.load_asset(schema_full_path)

if existing_schema and isinstance(existing_schema, unreal.PoseSearchSchema):
    schema = existing_schema
    log.info("Reusing existing schema: %s", schema_full_path)
else:
    log.info("Creating new schema at %s...", schema_full_path)
    schema = unreal.AnimDatabaseUtility.create_schema_with_default_channels(
        skeleton, SCHEMA_PKG, SCHEMA_NAME
    )
    if schema is None:
        log.error("CreateSchemaWithDefaultChannels returned None. Aborting.")
        sys.exit(1)
    log.info("Schema created: %s", schema.get_name())

# ---------------------------------------------------------------------------
# 4. Set schema on database
# ---------------------------------------------------------------------------
unreal.AnimDatabaseUtility.set_database_schema(db, schema)
log.info("Schema set on database.")

# ---------------------------------------------------------------------------
# 5. Add animations
# ---------------------------------------------------------------------------
added = 0
failed = []

for rel_path, looping in ANIMATIONS:
    full_path = f"{UNARMED_BASE}/{rel_path}"
    seq = load_asset(full_path, unreal.AnimSequence)
    if seq is None:
        failed.append(full_path)
        continue
    unreal.AnimDatabaseUtility.add_sequence_to_database(db, seq, looping)
    log.info("  Added [looping=%s] %s", looping, rel_path)
    added += 1

log.info("Added %d / %d animations. Failed: %d", added, len(ANIMATIONS), len(failed))

if failed:
    for f in failed:
        log.error("  MISSING: %s", f)

# ---------------------------------------------------------------------------
# 6. Save database
# ---------------------------------------------------------------------------
ok = unreal.AnimDatabaseUtility.save_database_asset(db)
if ok:
    log.info("Database saved successfully.")
else:
    log.error("Database save reported failure — check UE log for details.")

# ---------------------------------------------------------------------------
# 7. Save schema too (may already be saved, belt-and-suspenders)
# ---------------------------------------------------------------------------
unreal.EditorAssetLibrary.save_loaded_asset(schema)
log.info("Schema asset saved.")

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
log.info("=" * 60)
log.info("DONE — %d animations added to %s", added, DB_PATH)
log.info("Schema : %s", schema_full_path)
log.info("Log    : %s", LOG_PATH)
if failed:
    log.warning("INCOMPLETE — %d animation(s) not found. See log for paths.", len(failed))
    sys.exit(2)
