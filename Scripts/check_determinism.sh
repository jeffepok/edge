#!/usr/bin/env bash
# check_determinism.sh — local + CI gate. Builds standalone, runs lint, runs
# self-test, replays all checked-in streams, diffs against baselines, runs
# rollback round-trip on the torture stream.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

echo "==> lint_sim.sh"
bash Scripts/lint_sim.sh || { echo "FAIL: lint"; exit 1; }

echo "==> configure standalone"
cmake -S Source/Edge26SimStandalone -B build/sim -DCMAKE_BUILD_TYPE=Release \
    > /dev/null || { echo "FAIL: cmake configure"; exit 1; }

echo "==> build standalone"
cmake --build build/sim --parallel > /dev/null || { echo "FAIL: build"; exit 1; }

echo "==> self-test"
./build/sim/edge26_sim_replay --self-test > /dev/null || { echo "FAIL: self-test"; exit 1; }

REPLAY_DIR="Source/Edge26SimStandalone/tests/replays"
for name in basic ball_only rollback_torture; do
    echo "==> replay: $name"
    ACTUAL="$(./build/sim/edge26_sim_replay --input "$REPLAY_DIR/${name}.input" --hash-every 1)"
    EXPECTED="$(cat "$REPLAY_DIR/${name}.expected.hashes")"
    if [[ "$ACTUAL" != "$EXPECTED" ]]; then
        echo "FAIL: $name baseline mismatch"
        diff <(echo "$EXPECTED") <(echo "$ACTUAL") | head -20
        exit 1
    fi
done

echo "==> rollback round-trip on rollback_torture"
./build/sim/edge26_sim_replay --input "$REPLAY_DIR/rollback_torture.input" --rollback-test --hash-every 0 \
    || { echo "FAIL: rollback"; exit 1; }

echo "PASS: all determinism checks"
