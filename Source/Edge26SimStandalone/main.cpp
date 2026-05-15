// edge26_sim_replay — headless determinism harness for Edge26Sim.
#include <cstdio>
#include <cstring>

// Forward declare the test entry points. Each TEST_FILE has one.
int RunMathTests();
int RunSnapshotTests();

static int RunSelfTest() {
    std::printf("== Self-test ==\n");
    int rc = 0;
    rc = RunMathTests();     if (rc) return rc;
    rc = RunSnapshotTests(); if (rc) return rc;
    std::printf("== Self-test OK ==\n");
    return 0;
}

int main(int argc, char** argv) {
    bool selfTest = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--self-test") == 0) selfTest = true;
    }
    if (selfTest) return RunSelfTest();
    std::printf("edge26_sim_replay (M1 — use --self-test)\n");
    return 0;
}
