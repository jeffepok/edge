#include "Sim/WorldState.h"
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include "TestHarness.h"

using namespace edge26;

TEST_CASE(WorldState_Sizes) {
    TEST_EXPECT_EQ((int64_t)sizeof(FSimBallState),   (int64_t)80);
    TEST_EXPECT_EQ((int64_t)sizeof(FSimPlayerState), (int64_t)64);
    TEST_EXPECT_EQ((int64_t)sizeof(FSimWorldState),  (int64_t)224);
    return 0;
}

TEST_CASE(WorldState_Aligned) {
    TEST_EXPECT_EQ((int64_t)alignof(FSimWorldState), (int64_t)8);
    return 0;
}

TEST_CASE(SimWorld_FreshIsZeroExceptSeed) {
    SimWorld w{0x123456789ABCDEF0ull};
    const FSimWorldState& s = w.GetState();
    TEST_EXPECT_EQ(s.TickNumber,    (uint32_t)0);
    TEST_EXPECT_EQ(s.RngState,      (uint64_t)0x123456789ABCDEF0ull);
    TEST_EXPECT_EQ(s.Ball.Position.X.Raw, (int64_t)0);
    TEST_EXPECT_EQ(s.Players[0].ControllerIndex, (uint8_t)0);
    TEST_EXPECT_EQ(s.Players[1].ControllerIndex, (uint8_t)1);
    TEST_EXPECT_EQ(s._pad0,                (uint32_t)0);
    return 0;
}

TEST_CASE(Player_StationaryNoInput) {
    SimWorld w{1};
    FInputFrame f{};
    f.TickNumber = 1;
    w.Step(f);
    TEST_EXPECT_EQ(w.GetState().Players[0].Position.X.Raw, (int64_t)0);
    TEST_EXPECT_EQ(w.GetState().Players[0].Velocity.X.Raw, (int64_t)0);
    return 0;
}

TEST_CASE(Player_RespondsToStickInput) {
    SimWorld w{1};
    FInputFrame f{};
    f.TickNumber = 1;
    f.Move[0][0] = 127;
    f.Move[0][1] = 0;
    w.Step(f);
    TEST_EXPECT_TRUE(w.GetState().Players[0].Velocity.X.Raw > 0);
    for (int i = 0; i < 50; ++i) { f.TickNumber++; w.Step(f); }
    int64_t got      = w.GetState().Players[0].Velocity.X.Raw;
    int64_t expected = SimConst::JogSpeed.Raw;
    int64_t diff     = got > expected ? got - expected : expected - got;
    TEST_EXPECT_TRUE(diff < (Fixed64::One / 10));
    return 0;
}

int RunSnapshotTests() {
    TEST_RUN(WorldState_Sizes);
    TEST_RUN(WorldState_Aligned);
    TEST_RUN(SimWorld_FreshIsZeroExceptSeed);
    TEST_RUN(Player_StationaryNoInput);
    TEST_RUN(Player_RespondsToStickInput);
    return 0;
}
