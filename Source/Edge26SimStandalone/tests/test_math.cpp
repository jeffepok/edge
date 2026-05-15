#include "Math/Fixed.h"
#include "Math/Mul64.h"
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

TEST_CASE(Mul64Q32_BasicVectors) {
    // a * b in Q32.32 where a, b are raw values.
    int64_t a = (int64_t)2 << 32;
    int64_t b = (int64_t)3 << 32;
    int64_t prod = edge26::Mul64Q32(a, b);
    TEST_EXPECT_EQ(prod >> 32, (int64_t)6);

    a = (int64_t)-4 << 32;
    b = (int64_t)5  << 32;
    prod = edge26::Mul64Q32(a, b);
    TEST_EXPECT_EQ(prod >> 32, (int64_t)-20);
    return 0;
}

TEST_CASE(Fixed64_Multiply) {
    Fixed64 a = Fixed64::FromInt(6);
    Fixed64 b = Fixed64::FromInt(7);
    TEST_EXPECT_EQ((a * b).ToInt(), (int64_t)42);

    Fixed64 c = Fixed64::FromInt(-3);
    Fixed64 d = Fixed64::FromInt(5);
    TEST_EXPECT_EQ((c * d).ToInt(), (int64_t)-15);
    return 0;
}

TEST_CASE(Fixed64_Divide) {
    Fixed64 a = Fixed64::FromInt(20);
    Fixed64 b = Fixed64::FromInt(4);
    TEST_EXPECT_EQ((a / b).ToInt(), (int64_t)5);

    Fixed64 half = Fixed64::FromInt(1) / Fixed64::FromInt(2);
    TEST_EXPECT_EQ(half.Raw, Fixed64::One / 2);
    return 0;
}

int RunMathTests() {
    TEST_RUN(Fixed64_FromInt_RoundTrip);
    TEST_RUN(Fixed64_Add);
    TEST_RUN(Fixed64_Negation);
    TEST_RUN(Mul64Q32_BasicVectors);
    TEST_RUN(Fixed64_Multiply);
    TEST_RUN(Fixed64_Divide);
    return 0;
}
