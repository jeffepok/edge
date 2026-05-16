#!/usr/bin/env bash
# check_determinism.sh — local + CI gate. Builds standalone, runs lint, runs
# self-test, replays all checked-in streams, diffs against baselines, runs
# rollback round-trip on the torture stream.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

# Use Ninja everywhere so the build layout is consistent across platforms
# (single-config; binaries land directly in $BUILD_DIR, not $BUILD_DIR/Release).
# Falls back to the default generator if Ninja isn't installed.
GENERATOR_ARGS=()
if command -v ninja >/dev/null 2>&1; then
    GENERATOR_ARGS=(-G Ninja)
fi

# Windows exe suffix.
EXE_SUFFIX=""
case "${OSTYPE:-}" in
    msys*|cygwin*|win32*) EXE_SUFFIX=".exe" ;;
esac

REPLAY_BIN="build/sim/edge26_sim_replay${EXE_SUFFIX}"

echo "==> lint_sim.sh"
bash Scripts/lint_sim.sh || { echo "FAIL: lint"; exit 1; }

echo "==> configure standalone (generator: ${GENERATOR_ARGS[*]:-default})"
# bash 3.2 expansion guard: only inline the array if it has entries.
cmake -S Source/Edge26SimStandalone -B build/sim -DCMAKE_BUILD_TYPE=Release \
    ${GENERATOR_ARGS[@]+"${GENERATOR_ARGS[@]}"} \
    || { echo "FAIL: cmake configure"; exit 1; }

echo "==> build standalone"
cmake --build build/sim --parallel --config Release \
    || { echo "FAIL: build"; exit 1; }

echo "==> self-test"
"./${REPLAY_BIN}" --self-test || { echo "FAIL: self-test"; exit 1; }

REPLAY_DIR="Source/Edge26SimStandalone/tests/replays"
for name in basic ball_only rollback_torture ai_match_30s; do
    echo "==> replay: $name"
    ACTUAL="$("./${REPLAY_BIN}" --input "$REPLAY_DIR/${name}.input" --hash-every 1)"
    EXPECTED="$(cat "$REPLAY_DIR/${name}.expected.hashes")"
    if [[ "$ACTUAL" != "$EXPECTED" ]]; then
        echo "FAIL: $name baseline mismatch"
        diff <(echo "$EXPECTED") <(echo "$ACTUAL") | head -20
        exit 1
    fi
done

echo "==> rollback round-trip on rollback_torture"
"./${REPLAY_BIN}" --input "$REPLAY_DIR/rollback_torture.input" --rollback-test --hash-every 0 \
    || { echo "FAIL: rollback"; exit 1; }

echo "PASS: all determinism checks"
