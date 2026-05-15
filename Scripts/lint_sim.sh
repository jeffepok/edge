#!/usr/bin/env bash
# lint_sim.sh — grep for forbidden tokens inside Edge26Sim/.
# See spec §4 for the rule set.
#
# Lines containing `// SIM-LINT-OK: <reason>` are suppressed.
# Exit non-zero if any unsuppressed violation is found.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SIM_DIR="$REPO_ROOT/Source/Edge26Sim"

if [[ ! -d "$SIM_DIR" ]]; then
    echo "lint_sim.sh: $SIM_DIR not found"
    exit 2
fi

# Pattern, friendly-name pairs. Whole-word where it matters.
PATTERNS=(
    '\bfloat\b|float type'
    '\bdouble\b|double type'
    '\bFVector\b|UE5 float vector'
    '\bFRotator\b|UE5 rotator'
    '\bFQuat\b|UE5 quat'
    '\bFMath::|UE5 FMath'
    'std::unordered_|hash-ordered container'
    '\bTMap<|UE5 hash map'
    '\bTSet<|UE5 hash set'
    'std::thread|thread'
    'std::async|async'
    '\bParallelFor\b|parallel-for'
    '\bFRunnable\b|UE5 thread runnable'
    'std::chrono|chrono'
    '\bFDateTime\b|UE5 wall-clock'
    '\bFPlatformTime\b|UE5 wall-clock'
    '#include\s*"Engine/|UE5 Engine include'
    '#include\s*<GameFramework/|UE5 GameFramework include'
    '#include\s*"Chaos/|UE5 Chaos include'
    'std::rand\b|non-PRNG randomness'
    'std::random_device|non-PRNG randomness'
    'FMath::Rand|non-PRNG randomness'
    '\bthrow\b|exception'
    '\btry\b|exception'
)

FAILED=0

# bash 3.2 (macOS default) lacks globstar — use find instead.
FILE_LIST="$(find "$SIM_DIR/Public" "$SIM_DIR/Private" -type f \( -name '*.h' -o -name '*.cpp' \) 2>/dev/null)"

for entry in "${PATTERNS[@]}"; do
    PATTERN="${entry%%|*}"
    NAME="${entry##*|}"
    while IFS= read -r f; do
        [[ -z "$f" || ! -f "$f" ]] && continue
        # Find matches, then drop any line carrying SIM-LINT-OK
        while IFS= read -r match; do
            if [[ -n "$match" && "$match" != *"SIM-LINT-OK"* ]]; then
                echo "FORBIDDEN ($NAME): $match"
                FAILED=1
            fi
        done < <(grep -nE "$PATTERN" "$f" 2>/dev/null || true)
    done <<< "$FILE_LIST"
done

if [[ $FAILED -eq 0 ]]; then
    echo "lint_sim.sh: OK"
fi
exit $FAILED
