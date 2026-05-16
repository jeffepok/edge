#!/usr/bin/env bash
# update_determinism_baseline.sh — regenerate the *.expected.hashes files.
# Run ONLY when you intend to change sim behavior. The diff is the receipt.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

cmake -S Source/Edge26SimStandalone -B build/sim -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build build/sim --parallel > /dev/null

# (Re)generate the .input files too — sim schema changes can invalidate them.
./build/sim/replay_generator Source/Edge26SimStandalone/tests/replays

REPLAY_DIR="Source/Edge26SimStandalone/tests/replays"
for name in basic ball_only rollback_torture ai_match_30s; do
    ./build/sim/edge26_sim_replay --input "$REPLAY_DIR/${name}.input" --hash-every 1 \
        > "$REPLAY_DIR/${name}.expected.hashes"
    echo "wrote $REPLAY_DIR/${name}.expected.hashes"
done
echo "Baselines updated. REVIEW THE GIT DIFF before committing."
