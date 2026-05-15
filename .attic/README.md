# .attic — preserved in-flight prototype work

This directory holds files that were uncommitted on `main` before the
sim-core v0 merge. They referenced legacy classes (`ASoccerBall`,
`AFootballerCharacter`, etc.) that the merge deleted, so they couldn't
remain in `Source/` without breaking the build.

Contents (all moved verbatim):

- `Source/Edge26/Public/Camera/BroadcastCamera.h` + `.cpp` — new camera
  component (depended on `ASoccerBall`).
- `Source/Edge26/Public/Pitch/PitchMarkings.h` + `.cpp` — new pitch
  markings system.
- `scripts/align_goals.py`, `scripts/bake_pitch_markings.py` — editor
  automation scripts (lowercase scripts/ dir — distinct from the
  uppercase `Scripts/` introduced by the sim-core merge).

To restore any of these, copy back to the original path and rewire the
dependencies through the new visual-shell classes
(`ASoccerBallVisual`, `AFootballerVisual`).

This directory is ignored at the Source/ scanning level (UBT only walks
under `Source/`), so nothing here participates in the build.
