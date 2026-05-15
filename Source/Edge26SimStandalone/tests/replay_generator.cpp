// replay_generator — generates the checked-in .input files from a hard-coded DSL.
// Run after a sim behavior change to regenerate the binary inputs.
// Usage: replay_generator <output-dir>
#include "ReplayIO.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <string>

using namespace edge26;

static std::vector<FInputFrame> MakeBasic() {
    // 500 ticks: kickoff, P1 moves forward 200 ticks, takes pass at 250, both wander.
    std::vector<FInputFrame> out(500);
    for (uint32_t t = 0; t < out.size(); ++t) {
        FInputFrame& f = out[t];
        std::memset(&f, 0, sizeof(f));
        f.TickNumber = t;
        if (t < 200)         { f.Move[0][0] = 100; }
        else if (t == 250)   { f.Buttons[0] = InputButton::Pass; }
        else if (t < 400)    { f.Move[0][1] = (int8_t)(t % 50 < 25 ? 80 : -80); }
        f.Move[1][0] = (int8_t)((t * 3) % 200 - 100);
    }
    return out;
}

static std::vector<FInputFrame> MakeBallOnly() {
    // 1000 ticks: zero input throughout (players stand still, ball settles under gravity).
    std::vector<FInputFrame> out(1000);
    for (uint32_t t = 0; t < out.size(); ++t) {
        out[t] = FInputFrame{};
        out[t].TickNumber = t;
    }
    return out;
}

static std::vector<FInputFrame> MakeRollbackTorture() {
    // 2000 ticks of erratic stick reversal on P1; P2 occasional kicks.
    std::vector<FInputFrame> out(2000);
    for (uint32_t t = 0; t < out.size(); ++t) {
        FInputFrame& f = out[t];
        std::memset(&f, 0, sizeof(f));
        f.TickNumber = t;
        bool flip = (t / 5) & 1;
        f.Move[0][0] = (int8_t)(flip ? 120 : -120);
        f.Move[0][1] = (int8_t)(((t / 7) & 1) ? 90 : -90);
        f.Buttons[0] = (t % 100 == 99) ? InputButton::Shoot : 0;
        f.Move[1][0] = (int8_t)((t * 11) % 200 - 100);
        f.Buttons[1] = (t % 150 == 149) ? InputButton::Chip : 0;
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: replay_generator <out-dir>\n"); return 1; }
    std::string dir = argv[1];
    bool ok = true;
    ok &= WriteReplay((dir + "/basic.input").c_str(),             MakeBasic());
    ok &= WriteReplay((dir + "/ball_only.input").c_str(),         MakeBallOnly());
    ok &= WriteReplay((dir + "/rollback_torture.input").c_str(),  MakeRollbackTorture());
    if (!ok) return 1;
    std::printf("replay_generator: wrote 3 streams to %s\n", dir.c_str());
    return 0;
}
