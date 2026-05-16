# Edge26 — Agent Notes

## Commit messages

- **Never** include `Co-Authored-By: Claude ...` trailers, `Generated with Claude Code` lines, or any other Claude/Anthropic attribution in commit messages, PR bodies, or tag annotations. Commits ship as the human author only.
- Stick to the existing repo style: imperative subject (`feat(scope): …`, `fix(scope): …`, `docs(scope): …`, `test(scope): …`, `tune(scope): …`, `chore: …`), wrap body at ~72 cols, lead with the why before the what.

## Workflow defaults

- Branch per phase (`feat/phaseN-<short-name>`) — never commit directly to `main` unless the user explicitly asks.
- After every meaningful sim-state change: build standalone (`cmake --build build/sim --parallel`), run `--self-test`, run `lint_sim.sh`, regenerate baselines if applicable, run `check_determinism.sh`, build the editor.
- The `feat/phaseN-...` branch must remain green on the 3-OS CI matrix before opening / updating PRs.

## Determinism guardrails (Edge26Sim module)

These are absolute — `Scripts/lint_sim.sh` greps for them. Never violate:
- No `float` / `double` (compile-time literals via `// SIM-LINT-OK` only when unavoidable)
- No `FVector` / `FRotator` / `FQuat` / `FMath::*`
- No `std::unordered_*` / `TMap<` / `TSet<` (any hash-ordered container)
- No `std::thread` / `std::async` / `ParallelFor` / `FRunnable`
- No `std::chrono` / `FDateTime` / `FPlatformTime`
- No `#include "Engine/..."` / `#include <GameFramework/...>` / `#include "Chaos/..."`
- No `std::rand` / `FMath::Rand` / `std::random_device`
- No `throw` / `try`

## Sim/render boundary

- Sim code lives only under `Source/Edge26Sim/`.
- Visual / UE5-side code (BroadcastCamera, AIDebugRenderer, SimInputCollector, etc.) lives under `Source/Edge26/` and may use floats.
- Animation, IK, ragdolls, particle effects — **forever render-side only.** They never influence the sim state.

## Cross-module symbol exports

Functions / globals declared in `Source/Edge26Sim/Public/` that the `Edge26` (UE5) module needs to link against must carry `EDGE26SIM_API` (defined in `Edge26SimAPI.h`). macOS strips otherwise.
