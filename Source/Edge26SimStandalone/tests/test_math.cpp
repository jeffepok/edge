#include "Math/Fixed.h"
#include "TestHarness.h"

using edge26::Fixed64;

TEST_CASE(Fixed64_FromInt_RoundTrip) {
    TEST_EXPECT_EQ(Fixed64::FromInt(0).ToInt(),     (int64_t)0);
    TEST_EXPECT_EQ(Fixed64::FromInt(1).ToInt(),     (int64_t)1);
    TEST_EXPECT_EQ(Fixed64::FromInt(-1).ToInt(),    (int64_t)-1);
    TEST_EXPECT_EQ(Fixed64::FromInt(12345).ToInt(), (int64_t)12345);
    return 0;
}

TEST_CASE(Fixed64_Add) {
    Fixed64 a = Fixed64::FromInt(3);
    Fixed64 b = Fixed64::FromInt(5);
    TEST_EXPECT_EQ((a + b).ToInt(), (int64_t)8);
    TEST_EXPECT_EQ((a - b).ToInt(), (int64_t)-2);
    return 0;
}

TEST_CASE(Fixed64_Negation) {
    Fixed64 a = Fixed64::FromInt(7);
    TEST_EXPECT_EQ((-a).ToInt(), (int64_t)-7);
    return 0;
}

int RunMathTests() {
    TEST_RUN(Fixed64_FromInt_RoundTrip);
    TEST_RUN(Fixed64_Add);
    TEST_RUN(Fixed64_Negation);
    return 0;
}
