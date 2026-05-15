#include "Sim/WorldState.h"
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include "TestHarness.h"
#include <cstring>

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

TEST_CASE(Ball_FallsUnderGravity) {
    SimWorld w{1};
    w.MutableState().Ball.Position.Z = Fixed64::FromInt(500);
    w.MutableState().Ball.Velocity   = FixedVec3::Zero();
    w.MutableState().Ball.Flags      = 0;
    FInputFrame f{};
    for (int i = 0; i < 5; ++i) { f.TickNumber = (uint32_t)i; w.Step(f); }
    TEST_EXPECT_TRUE(w.GetState().Ball.Velocity.Z.Raw < 0);
    TEST_EXPECT_TRUE(w.GetState().Ball.Position.Z.Raw < Fixed64::FromInt(500).Raw);
    return 0;
}

TEST_CASE(Ball_SettlesOnGround) {
    SimWorld w{1};
    w.MutableState().Ball.Position.Z = Fixed64::FromInt(100);
    w.MutableState().Ball.Velocity   = FixedVec3::Zero();
    FInputFrame f{};
    for (int i = 0; i < 500; ++i) { f.TickNumber = (uint32_t)i; w.Step(f); }
    TEST_EXPECT_TRUE(w.GetState().Ball.Position.Z.Raw <= (SimConst::BallRadius.Raw + (Fixed64::One / 10)));
    TEST_EXPECT_TRUE((w.GetState().Ball.Flags & BallFlag::Grounded) != 0);
    return 0;
}

TEST_CASE(Kick_PassImpulse) {
    SimWorld w{1};
    w.MutableState().Ball.Position = FixedVec3::Zero();
    w.MutableState().Ball.Velocity = FixedVec3::Zero();
    w.MutableState().Ball.Position.Z = SimConst::BallRadius;
    w.MutableState().Players[0].Position = {Fixed64::FromInt(0), Fixed64::FromInt(50), Fixed64::FromInt(0)};
    w.MutableState().Players[0].Heading = FixedAngle::FromRaw(-FixedAngle::PiRaw() / 2);

    FInputFrame f{};
    f.TickNumber = 1;
    f.Buttons[0] = InputButton::Pass;
    w.Step(f);
    // Ball should have nonzero velocity after the kick.
    bool moved = w.GetState().Ball.Velocity.X.Raw != 0 ||
                 w.GetState().Ball.Velocity.Y.Raw != 0 ||
                 w.GetState().Ball.Velocity.Z.Raw != 0;
    TEST_EXPECT_TRUE(moved);
    return 0;
}

TEST_CASE(Sim_TwoRunsIdentical) {
    SimWorld a{0xABCDEFull};
    SimWorld b{0xABCDEFull};
    FInputFrame f{};
    for (int tick = 0; tick < 200; ++tick) {
        f.TickNumber  = (uint32_t)tick;
        int phase = tick % 60;
        f.Move[0][0] = (int8_t)(phase < 30 ? 120 : -120);
        f.Move[0][1] = (int8_t)(phase < 15 || phase >= 45 ? 90 : -90);
        f.Buttons[0] = (tick % 50 == 49) ? InputButton::Pass : 0;
        a.Step(f); b.Step(f);
    }
    int cmp = std::memcmp(&a.GetState(), &b.GetState(), sizeof(FSimWorldState));
    TEST_EXPECT_EQ((int64_t)cmp, (int64_t)0);
    return 0;
}

TEST_CASE(Snapshot_RoundTrip) {
    SimWorld w{0xABCDEF};
    FInputFrame f{};
    for (int i = 0; i < 50; ++i) {
        f.TickNumber = (uint32_t)i;
        f.Move[0][0] = (int8_t)(i % 3 - 1) * 100;
        w.Step(f);
    }
    FSimWorldState snap;
    w.Snapshot(snap);
    for (int i = 50; i < 60; ++i) { f.TickNumber = (uint32_t)i; w.Step(f); }
    w.Restore(snap);
    int cmp = std::memcmp(&w.GetState(), &snap, sizeof(FSimWorldState));
    TEST_EXPECT_EQ((int64_t)cmp, (int64_t)0);
    return 0;
}

TEST_CASE(Hash_Stable) {
    SimWorld a{0x1234}; SimWorld b{0x1234};
    FInputFrame f{};
    for (int i = 0; i < 100; ++i) {
        f.TickNumber = (uint32_t)i;
        a.Step(f); b.Step(f);
    }
    TEST_EXPECT_EQ(a.HashState(), b.HashState());
    return 0;
}

TEST_CASE(Rollback_FullRoundTrip) {
    FInputFrame f{};
    uint64_t finalHash_run1;
    {
        SimWorld w{0xC0FFEE};
        for (int i = 0; i < 100; ++i) {
            f.TickNumber = (uint32_t)i;
            f.Move[0][0] = (int8_t)((i * 7) % 200 - 100);
            f.Move[0][1] = (int8_t)((i * 11) % 200 - 100);
            w.Step(f);
        }
        finalHash_run1 = w.HashState();
    }

    SimWorld w{0xC0FFEE};
    for (int i = 0; i < 50; ++i) {
        f.TickNumber = (uint32_t)i;
        f.Move[0][0] = (int8_t)((i * 7) % 200 - 100);
        f.Move[0][1] = (int8_t)((i * 11) % 200 - 100);
        w.Step(f);
    }
    FSimWorldState snap;
    w.Snapshot(snap);
    // Burn ticks down a dead path.
    for (int i = 50; i < 90; ++i) {
        f.TickNumber = (uint32_t)i;
        f.Move[0][0] = 127;
        w.Step(f);
    }
    w.Restore(snap);
    for (int i = 50; i < 100; ++i) {
        f.TickNumber = (uint32_t)i;
        f.Move[0][0] = (int8_t)((i * 7) % 200 - 100);
        f.Move[0][1] = (int8_t)((i * 11) % 200 - 100);
        w.Step(f);
    }
    TEST_EXPECT_EQ(w.HashState(), finalHash_run1);
    return 0;
}

TEST_CASE(Hash_PerTickStable) {
    FInputFrame f{};
    uint64_t hashes[100];
    {
        SimWorld w{0xFEED};
        for (int i = 0; i < 100; ++i) {
            f.TickNumber = (uint32_t)i;
            f.Move[0][0] = (int8_t)((i * 3) % 200 - 100);
            w.Step(f);
            hashes[i] = w.HashState();
        }
    }
    SimWorld w{0xFEED};
    for (int i = 0; i < 100; ++i) {
        f.TickNumber = (uint32_t)i;
        f.Move[0][0] = (int8_t)((i * 3) % 200 - 100);
        w.Step(f);
        if (w.HashState() != hashes[i]) TEST_FAIL("divergence at tick %d", i);
    }
    return 0;
}

int RunSnapshotTests() {
    TEST_RUN(WorldState_Sizes);
    TEST_RUN(WorldState_Aligned);
    TEST_RUN(SimWorld_FreshIsZeroExceptSeed);
    TEST_RUN(Player_StationaryNoInput);
    TEST_RUN(Player_RespondsToStickInput);
    TEST_RUN(Ball_FallsUnderGravity);
    TEST_RUN(Ball_SettlesOnGround);
    TEST_RUN(Kick_PassImpulse);
    TEST_RUN(Sim_TwoRunsIdentical);
    TEST_RUN(Snapshot_RoundTrip);
    TEST_RUN(Hash_Stable);
    TEST_RUN(Rollback_FullRoundTrip);
    TEST_RUN(Hash_PerTickStable);
    return 0;
}
