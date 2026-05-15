#include "Sim/WorldState.h"
#include "Sim/SimWorld.h"
#include "Sim/Constants.h"
#include "AI/Formations.h"
#include "AI/Roles.h"
#include "AI/SpatialValueModel.h"
#include "TestHarness.h"
#include <cstring>

// Forward declarations for field update functions (also declared in SpatialValueModel.h,
// but re-declared here for clarity with the test file).
namespace edge26 {
    void UpdateSpaceField(FSimWorldState& s, int teamId);
    void UpdateDefCoverageField(FSimWorldState& s, int teamId);
    void UpdateLaneOccupancyField(FSimWorldState& s);
    void UpdateThreatField(FSimWorldState& s, int teamId);
    void UpdatePassReceptionField(FSimWorldState& s, int teamId);  // T2.6
}

using namespace edge26;

TEST_CASE(WorldState_Sizes) {
    TEST_EXPECT_EQ((int64_t)sizeof(FSimBallState),   (int64_t)80);
    TEST_EXPECT_EQ((int64_t)sizeof(FSimPlayerState), (int64_t)88);
    // FSimWorldState size assertion is deferred — it grows substantially in M2.
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
    // All players are stationary after T1.5 (team/role/slot init; human binding in M9).
    TEST_EXPECT_EQ(s.Players[0].ControllerIndex, kStationaryController);
    TEST_EXPECT_EQ(s.Players[1].ControllerIndex, kStationaryController);
    TEST_EXPECT_EQ(s._pad0,                (uint32_t)0);
    return 0;
}

TEST_CASE(Player_StationaryNoInput) {
    SimWorld w{1};
    // After T1.5 all players start at their formation slots with kStationaryController.
    // Record the initial position and confirm it doesn't change after a step.
    int64_t initX = w.GetState().Players[0].Position.X.Raw;
    FInputFrame f{};
    f.TickNumber = 1;
    w.Step(f);
    TEST_EXPECT_EQ(w.GetState().Players[0].Position.X.Raw, initX);
    TEST_EXPECT_EQ(w.GetState().Players[0].Velocity.X.Raw, (int64_t)0);
    return 0;
}

TEST_CASE(Player_RespondsToStickInput) {
    SimWorld w{1};
    // After T1.5 all players have kStationaryController; wire player 0 to controller 0
    // explicitly to test input response.
    w.MutableState().Players[0].ControllerIndex = 0;
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
    // After T1.5 all players have kStationaryController; wire player 0 to controller 0
    // so the Pass button is processed.
    w.MutableState().Players[0].ControllerIndex = 0;

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

TEST_CASE(Formation_HomeAwaySymmetry) {
    using namespace edge26;
    // GK slot for home should be near -X (own goal); for away, near +X.
    FixedVec3 homeGK = SlotWorldPosition(0, 0);
    FixedVec3 awayGK = SlotWorldPosition(0, 1);
    TEST_EXPECT_TRUE(homeGK.X.Raw < 0);
    TEST_EXPECT_TRUE(awayGK.X.Raw > 0);
    // Y should be identical (both GKs in center)
    TEST_EXPECT_EQ(homeGK.Y.Raw, awayGK.Y.Raw);
    // The 11th slot (ST) for home should be near +X (opp goal); for away near -X.
    FixedVec3 homeST = SlotWorldPosition(10, 0);
    FixedVec3 awayST = SlotWorldPosition(10, 1);
    TEST_EXPECT_TRUE(homeST.X.Raw > 0);
    TEST_EXPECT_TRUE(awayST.X.Raw < 0);
    return 0;
}

TEST_CASE(World_22PlayersAtSlots) {
    SimWorld w{1};
    const auto& s = w.GetState();
    int homeCount = 0, awayCount = 0;
    int gkCount = 0;
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = s.Players[i];
        if (p.TeamId == 0) homeCount++;
        else if (p.TeamId == 1) awayCount++;
        if (p.RoleId == (uint8_t)ERole::GK) gkCount++;
    }
    TEST_EXPECT_EQ((int64_t)homeCount, (int64_t)11);
    TEST_EXPECT_EQ((int64_t)awayCount, (int64_t)11);
    TEST_EXPECT_EQ((int64_t)gkCount,   (int64_t)2);

    // GK home should be near -X, GK away near +X (sanity from T1.3)
    const auto& homeGK = s.Players[0];  // slot 0 is GK
    const auto& awayGK = s.Players[11]; // slot 0 of away team
    TEST_EXPECT_TRUE(homeGK.Position.X.Raw < 0);
    TEST_EXPECT_TRUE(awayGK.Position.X.Raw > 0);
    return 0;
}

TEST_CASE(World_22StationaryPlayersStable) {
    SimWorld w{1};
    FixedVec3 initial[kSimPlayerCount];
    for (int i = 0; i < kSimPlayerCount; ++i)
        initial[i] = w.GetState().Players[i].Position;

    FInputFrame f{};
    for (int tick = 0; tick < 100; ++tick) {
        f.TickNumber = (uint32_t)tick;
        w.Step(f);
    }
    for (int i = 0; i < kSimPlayerCount; ++i) {
        TEST_EXPECT_EQ(w.GetState().Players[i].Position.X.Raw, initial[i].X.Raw);
        TEST_EXPECT_EQ(w.GetState().Players[i].Position.Y.Raw, initial[i].Y.Raw);
    }
    return 0;
}

// ----- T2.2: UpdateSpaceField -----

TEST_CASE(SpatialModel_SpaceFieldEmptyPitchIsFullyOpen) {
    using namespace edge26;
    SimWorld w{1};
    // Remove all opponents (team 1) by moving them off-pitch far away.
    auto& state = w.MutableState();
    for (int i = 11; i < kSimPlayerCount; ++i) {
        state.Players[i].Position = FixedVec3{
            Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0)
        };
    }
    UpdateSpaceField(state, 0);  // home team perspective
    // Every cell should be max openness (no nearby opponents).
    for (int c = 0; c < kPitchCells; ++c) {
        Fixed32 v = state.Spatial.Cells[0][(int)ESpatialField::Space][c];
        // Allow ±2 ulps tolerance for sqrt rounding
        TEST_EXPECT_TRUE(v.Raw >= (Fixed32::One - 2));
    }
    return 0;
}

TEST_CASE(SpatialModel_SpaceFieldZeroAtOpponent) {
    using namespace edge26;
    SimWorld w{1};
    auto& state = w.MutableState();
    // Place opponent (player 11, away team) at the center of the origin cell,
    // so that cell's distance to the opponent is exactly 0.
    int originCell = CellIndex(FixedVec3::Zero());
    state.Players[11].Position = CellCenter(originCell);
    UpdateSpaceField(state, 0);
    Fixed32 v = state.Spatial.Cells[0][(int)ESpatialField::Space][originCell];
    // Cell exactly at opponent's position should have zero openness (distance = 0).
    TEST_EXPECT_TRUE(v.Raw < (Fixed32::One / 5));  // less than 0.2 (actually 0)
    return 0;
}

// ----- T2.1: SpatialValueModel struct + cell helpers -----

TEST_CASE(SpatialModel_CellIndexRoundtrip) {
    using namespace edge26;
    // Center cell of the pitch should map to a cell around (kPitchCellsX/2, kPitchCellsY/2).
    int center = CellIndex(FixedVec3::Zero());
    FixedVec3 back = CellCenter(center);
    // Should be near origin (within one cell radius)
    TEST_EXPECT_TRUE(Abs(back.X).Raw < (Fixed64::FromInt(kCellSizeX_cm)).Raw);
    TEST_EXPECT_TRUE(Abs(back.Y).Raw < (Fixed64::FromInt(kCellSizeY_cm)).Raw);
    return 0;
}

TEST_CASE(SpatialModel_CellIndexClampsOutOfBounds) {
    using namespace edge26;
    // Way off-pitch positions clamp to corner cells; no crash.
    int corner1 = CellIndex(FixedVec3{
        Fixed64::FromInt(-99999), Fixed64::FromInt(-99999), Fixed64::FromInt(0)
    });
    int corner2 = CellIndex(FixedVec3{
        Fixed64::FromInt( 99999), Fixed64::FromInt( 99999), Fixed64::FromInt(0)
    });
    TEST_EXPECT_EQ((int64_t)corner1, (int64_t)0);
    TEST_EXPECT_EQ((int64_t)corner2, (int64_t)(kPitchCells - 1));
    return 0;
}

// ----- T2.5: UpdateThreatField -----

TEST_CASE(SpatialModel_ThreatHighInOppBox) {
    using namespace edge26;
    SimWorld w{1};
    auto& state = w.MutableState();
    UpdateThreatField(state, 0);  // home attacks +X
    // Home's threat at +5000, 0 (deep in opp box) should be near max.
    int boxCell = CellIndex(FixedVec3{
        Fixed64::FromInt(5000), Fixed64::FromInt(0), Fixed64::FromInt(0)
    });
    Fixed32 v = state.Spatial.Cells[0][(int)ESpatialField::Threat][boxCell];
    TEST_EXPECT_TRUE(v.Raw > (Fixed32::One * 4 / 5));  // > 0.8
    // Home's threat at -5000, 0 (own box) should be ~zero.
    int ownBox = CellIndex(FixedVec3{
        Fixed64::FromInt(-5000), Fixed64::FromInt(0), Fixed64::FromInt(0)
    });
    Fixed32 v2 = state.Spatial.Cells[0][(int)ESpatialField::Threat][ownBox];
    TEST_EXPECT_EQ(v2.Raw, (int32_t)0);
    return 0;
}

// ----- T2.4: UpdateLaneOccupancyField -----

TEST_CASE(SpatialModel_LaneOccupancyEmptyPitchAllClear) {
    using namespace edge26;
    SimWorld w{1};
    auto& state = w.MutableState();
    // Move every player far off pitch so no one blocks lanes.
    for (int i = 0; i < kSimPlayerCount; ++i) {
        state.Players[i].Position = FixedVec3{
            Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0)
        };
    }
    state.Ball.Position = FixedVec3::Zero();
    // state.Match.PossessionPlayer deferred — FMatchState lands in T2.6.
    // v0: ball position is used unconditionally as lane origin.
    UpdateLaneOccupancyField(state);
    // Every cell should be fully clear (lane = 1).
    for (int c = 0; c < kPitchCells; ++c) {
        Fixed32 v = state.Spatial.Cells[0][(int)ESpatialField::LaneOccupancy][c];
        TEST_EXPECT_EQ(v.Raw, Fixed32::One);
    }
    return 0;
}

// ----- T2.6: UpdatePassReceptionField -----

TEST_CASE(SpatialModel_PassReceptionForwardOfBall) {
    using namespace edge26;
    SimWorld w{1};
    auto& state = w.MutableState();
    state.Ball.Position = FixedVec3::Zero();
    // Move opponents far away so Space + Lane are clear.
    for (int i = 11; i < kSimPlayerCount; ++i) {
        state.Players[i].Position = FixedVec3{
            Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0)
        };
    }
    UpdateSpaceField(state, 0);
    UpdateLaneOccupancyField(state);
    UpdateThreatField(state, 0);
    UpdatePassReceptionField(state, 0);

    // Cell at +3000 X (ahead of ball for home) should score higher than -3000.
    int aheadCell  = CellIndex(FixedVec3{Fixed64::FromInt(3000), Fixed64::FromInt(0), Fixed64::FromInt(0)});
    int behindCell = CellIndex(FixedVec3{Fixed64::FromInt(-3000), Fixed64::FromInt(0), Fixed64::FromInt(0)});
    Fixed32 ahead  = state.Spatial.Cells[0][(int)ESpatialField::PassReception][aheadCell];
    Fixed32 behind = state.Spatial.Cells[0][(int)ESpatialField::PassReception][behindCell];
    TEST_EXPECT_TRUE(ahead.Raw > behind.Raw);
    return 0;
}

// ----- T2.7: UpdateSpatialFields (orchestrator via SimWorld::Step) -----

TEST_CASE(SpatialModel_StepUpdatesFields) {
    using namespace edge26;
    SimWorld w{1};
    FInputFrame f{};
    f.TickNumber = 1;
    w.Step(f);   // should call UpdateSpatialFields
    // After one tick, the threat field should be non-trivial (hot in opp box).
    int boxCell = CellIndex(FixedVec3{
        Fixed64::FromInt(5000), Fixed64::FromInt(0), Fixed64::FromInt(0)
    });
    Fixed32 v = w.GetState().Spatial.Cells[0][(int)ESpatialField::Threat][boxCell];
    TEST_EXPECT_TRUE(v.Raw > (Fixed32::One * 4 / 5));
    return 0;
}

// ----- T2.3: UpdateDefCoverageField -----

TEST_CASE(SpatialModel_DefCoverageHighWhereTeammatesScarce) {
    using namespace edge26;
    SimWorld w{1};
    auto& state = w.MutableState();
    // Move all home players (team 0) to one corner, far from everything else.
    for (int i = 0; i < 11; ++i) {
        state.Players[i].Position = FixedVec3{
            Fixed64::FromInt(-5000), Fixed64::FromInt(-3000), Fixed64::FromInt(0)
        };
    }
    UpdateDefCoverageField(state, 0);  // home team's coverage from their perspective
    // A cell at the OPPOSITE corner has very poor coverage → field value should be high.
    int oppCorner = CellIndex(FixedVec3{
        Fixed64::FromInt(5000), Fixed64::FromInt(3000), Fixed64::FromInt(0)
    });
    Fixed32 v = state.Spatial.Cells[0][(int)ESpatialField::DefCoverage][oppCorner];
    TEST_EXPECT_TRUE(v.Raw > (Fixed32::One * 3 / 4));  // > 0.75
    return 0;
}

TEST_CASE(Sim_22PlayerTickStable) {
    using namespace edge26;
    SimWorld w{1};
    FInputFrame f{};
    for (int tick = 0; tick < 100; ++tick) {
        f.TickNumber = (uint32_t)tick;
        w.Step(f);
    }
    // Sim should still be alive — verify world tick number advanced and no
    // player has NaN-like fixed values (e.g., absurd positions).
    TEST_EXPECT_EQ(w.GetState().TickNumber, (uint32_t)99);
    for (int i = 0; i < kSimPlayerCount; ++i) {
        const auto& p = w.GetState().Players[i];
        TEST_EXPECT_TRUE(Abs(p.Position.X).Raw < (SimConst::PitchHalfLen * Fixed64::FromInt(2)).Raw);
        TEST_EXPECT_TRUE(Abs(p.Position.Y).Raw < (SimConst::PitchHalfWid * Fixed64::FromInt(2)).Raw);
    }
    return 0;
}

TEST_CASE(Sim_PossessionFlipsOnPickup) {
    using namespace edge26;
    SimWorld w{1};
    auto& st = w.MutableState();
    st.Match.PossessionTeam   = 0xFF;
    st.Match.PossessionPlayer = 0xFF;
    st.Match.HumanControlledIndex = 0xFF;
    // Park player 1 (home) on the ball; everyone else far away.
    for (int i = 0; i < kSimPlayerCount; ++i)
        st.Players[i].Position = FixedVec3{ Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0) };
    st.Players[1].Position = FixedVec3{ Fixed64::FromInt(0), Fixed64::FromInt(0), Fixed64::FromInt(0) };
    st.Ball.Position       = st.Players[1].Position;
    st.Ball.Velocity       = FixedVec3::Zero();

    FInputFrame f{};
    f.TickNumber = 1;
    w.Step(f);
    TEST_EXPECT_EQ(st.Match.PossessionPlayer, (uint8_t)1);
    TEST_EXPECT_EQ(st.Match.PossessionTeam,   (uint8_t)0);
    return 0;
}

TEST_CASE(Sim_AICarrierFiresPass) {
    using namespace edge26;
    SimWorld w{1};
    auto& st = w.MutableState();
    // Put two home players + ball at a clear spot; make player 1 the carrier.
    st.Match.PossessionTeam = 0;
    st.Match.PossessionPlayer = 1;
    st.Match.HumanControlledIndex = 0xFF;   // no human
    st.Players[1].Position = FixedVec3{ Fixed64::FromInt(0), Fixed64::FromInt(0), Fixed64::FromInt(0) };
    st.Players[2].Position = FixedVec3{ Fixed64::FromInt(1000), Fixed64::FromInt(0), Fixed64::FromInt(0) };
    st.Ball.Position       = st.Players[1].Position;
    st.Ball.Velocity       = FixedVec3::Zero();
    // Push opponents off pitch so they don't block the pass.
    for (int i = 11; i < kSimPlayerCount; ++i)
        st.Players[i].Position = FixedVec3{ Fixed64::FromInt(99999), Fixed64::FromInt(99999), Fixed64::FromInt(0) };

    FInputFrame f{};
    for (int t = 0; t < 5; ++t) {
        f.TickNumber = (uint32_t)t;
        w.Step(f);
        if (st.Ball.Velocity.X.Raw != 0) break;
    }
    TEST_EXPECT_TRUE(st.Ball.Velocity.X.Raw != 0);   // pass fired
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
    TEST_RUN(Formation_HomeAwaySymmetry);
    TEST_RUN(World_22PlayersAtSlots);
    TEST_RUN(World_22StationaryPlayersStable);
    TEST_RUN(SpatialModel_SpaceFieldEmptyPitchIsFullyOpen);
    TEST_RUN(SpatialModel_SpaceFieldZeroAtOpponent);
    TEST_RUN(SpatialModel_CellIndexRoundtrip);
    TEST_RUN(SpatialModel_CellIndexClampsOutOfBounds);
    TEST_RUN(SpatialModel_DefCoverageHighWhereTeammatesScarce);
    TEST_RUN(SpatialModel_LaneOccupancyEmptyPitchAllClear);
    TEST_RUN(SpatialModel_ThreatHighInOppBox);
    TEST_RUN(SpatialModel_PassReceptionForwardOfBall);
    TEST_RUN(SpatialModel_StepUpdatesFields);
    TEST_RUN(Sim_22PlayerTickStable);
    TEST_RUN(Sim_PossessionFlipsOnPickup);
    TEST_RUN(Sim_AICarrierFiresPass);
    return 0;
}
