// edge26_sim_replay — headless determinism harness for Edge26Sim.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "Sim/SimWorld.h"
#include "ReplayIO.h"

using namespace edge26;

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

struct Options {
    const char* inputPath  = nullptr;
    int hashEvery          = 1;
    bool rollbackTest      = false;
    uint64_t seed          = 0xC0FFEE;
    bool selfTest          = false;
};

static bool ParseArgs(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        if      (std::strcmp(argv[i], "--self-test")     == 0) opt.selfTest = true;
        else if (std::strcmp(argv[i], "--rollback-test") == 0) opt.rollbackTest = true;
        else if (std::strcmp(argv[i], "--input")         == 0 && i + 1 < argc) opt.inputPath = argv[++i];
        else if (std::strcmp(argv[i], "--hash-every")    == 0 && i + 1 < argc) opt.hashEvery = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--seed")          == 0 && i + 1 < argc) opt.seed = std::strtoull(argv[++i], nullptr, 0);
        else {
            std::fprintf(stderr, "unknown arg: %s\n", argv[i]);
            return false;
        }
    }
    return true;
}

static int RunReplay(const Options& opt) {
    std::vector<FInputFrame> frames;
    if (!ReadReplay(opt.inputPath, frames)) return 2;

    SimWorld w{opt.seed};
    FSimWorldState snap;
    int snapTick = -1;
    const int rollbackInterval = 30;
    const int rollbackWindow   = 5;

    for (size_t i = 0; i < frames.size(); ++i) {
        w.Step(frames[i]);
        uint32_t t = frames[i].TickNumber;
        if (opt.hashEvery > 0 && ((int)i % opt.hashEvery) == 0) {
            std::printf("%u %llx\n", t, (unsigned long long)w.HashState());
        }

        if (opt.rollbackTest) {
            if ((int)i % rollbackInterval == 0) {
                w.Snapshot(snap);
                snapTick = (int)i;
            }
            if (snapTick >= 0 && (int)i == snapTick + rollbackWindow) {
                uint64_t hashAtPlus5 = w.HashState();
                w.Restore(snap);
                for (int j = 0; j < rollbackWindow; ++j) {
                    w.Step(frames[snapTick + 1 + j]);
                }
                if (w.HashState() != hashAtPlus5) {
                    std::fprintf(stderr, "ROLLBACK MISMATCH at tick %d\n", (int)i);
                    return 3;
                }
                snapTick = -1;
            }
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    Options opt;
    if (!ParseArgs(argc, argv, opt)) return 1;

    if (opt.selfTest) return RunSelfTest();
    if (opt.inputPath != nullptr) return RunReplay(opt);

    std::fprintf(stderr, "usage: edge26_sim_replay [--self-test | --input <path> [--hash-every N] [--seed 0xN] [--rollback-test]]\n");
    return 1;
}
