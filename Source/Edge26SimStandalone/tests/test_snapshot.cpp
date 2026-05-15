#include "Sim/WorldState.h"
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

int RunSnapshotTests() {
    TEST_RUN(WorldState_Sizes);
    TEST_RUN(WorldState_Aligned);
    return 0;
}
