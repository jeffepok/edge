"""
add_anim_notifies.py
Headless UE5 Python script — run via UnrealEditor-Cmd with -ExecutePythonScript.

Adds BallContact + RecoverEnd AnimNotify events to the 7 action clips at
/Game/Animation/Mixamo_Retargeted (skips the 2 stance/idle clips). Notify
times are computed as fractions of each clip's play length, so the schedule
works regardless of the retargeted clip duration.

The notifies are consumed by:
  - UBallContactIKComponent::OnBallContactNotify (M8 IK snap-to-ball)
  - UFootballAnimInstance + any future BP-side handlers

Driver for UAnimDatabaseUtility::AddAnimNotify (M7 T7.4).

Usage:
    "/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
        "/path/to/Edge26.uproject" \
        -ExecutePythonScript="/path/to/Scripts/editor/add_anim_notifies.py" \
        -unattended -stdout
"""

import logging
import os
import sys

import unreal

# ---------------------------------------------------------------------------
# Logging — file + stdout
# ---------------------------------------------------------------------------
LOG_PATH = "/tmp/add_anim_notifies.log"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.FileHandler(LOG_PATH, mode="w"),
        logging.StreamHandler(sys.stdout),
    ],
)
log = logging.getLogger("AddAnimNotifies")

log.info("add_anim_notifies.py starting")

# ---------------------------------------------------------------------------
# Schedule:
#   anim name -> (BallContact fraction, RecoverEnd fraction)
# Skip stance/idle clips — Soccer_Idle_Anim and Goalkeeper_Idle_Anim don't
# need contact/recover notifies (they're loop poses, not actions).
# ---------------------------------------------------------------------------
BASE = "/Game/Animation/Mixamo_Retargeted"

NOTIFY_SCHEDULE = {
    "Soccer_Pass_Anim":                   (0.45, 0.95),
    "Soccer_Header_Anim":                 (0.55, 0.95),
    "Soccer_Tackle_Anim":                 (0.50, 0.95),
    "Strike_Foward_Jog_Anim":             (0.55, 0.95),
    "Goalkeeper_Diving_Save_Anim":        (0.55, 0.95),
    "Goalkeeper_Diving_Save__1__Anim":    (0.55, 0.95),
    "Goalkeeper_Overhand_Throw__1__Anim": (0.50, 0.95),
}

assert len(NOTIFY_SCHEDULE) == 7, (
    f"Expected 7 action clips in NOTIFY_SCHEDULE, got {len(NOTIFY_SCHEDULE)}"
)

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
total_notifies_added = 0
total_clips_touched = 0
failed_clips = []

util = unreal.AnimDatabaseUtility

for clip_name, (bc_frac, re_frac) in NOTIFY_SCHEDULE.items():
    full_path = f"{BASE}/{clip_name}"
    seq = unreal.EditorAssetLibrary.load_asset(full_path)
    if seq is None:
        log.error("FAILED to load %s", full_path)
        failed_clips.append(full_path)
        continue
    if not isinstance(seq, unreal.AnimSequence):
        log.error("%s is %s, expected AnimSequence", full_path, type(seq).__name__)
        failed_clips.append(full_path)
        continue

    length_sec = seq.get_play_length()
    bc_time = length_sec * bc_frac
    re_time = length_sec * re_frac

    log.info(
        "%s — len=%.3fs — BallContact @ %.3fs (%.0f%%), RecoverEnd @ %.3fs (%.0f%%)",
        clip_name, length_sec, bc_time, bc_frac * 100, re_time, re_frac * 100,
    )

    ok1 = util.add_anim_notify(seq, "BallContact", bc_time)
    ok2 = util.add_anim_notify(seq, "RecoverEnd",  re_time)
    if not (ok1 and ok2):
        log.error("  add_anim_notify reported failure (BallContact=%s, RecoverEnd=%s)", ok1, ok2)
        failed_clips.append(full_path)
        continue

    # Save the asset so the new notifies persist.
    saved = unreal.EditorAssetLibrary.save_asset(full_path)
    if not saved:
        log.error("  save_asset returned False for %s", full_path)
        failed_clips.append(full_path)
        continue

    total_clips_touched += 1
    total_notifies_added += 2

log.info("=" * 60)
log.info(
    "DONE — touched %d/%d clips, added %d notifies (2 per clip)",
    total_clips_touched, len(NOTIFY_SCHEDULE), total_notifies_added,
)
if failed_clips:
    log.error("FAILED clips: %s", failed_clips)
    sys.exit(2)
log.info("Log: %s", LOG_PATH)
