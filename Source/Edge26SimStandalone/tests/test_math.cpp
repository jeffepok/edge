#include "Math/Fixed.h"
#include "Math/Mul64.h"
#include "Math/FixedVec.h"
#include "Math/FixedAngle.h"
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

TEST_CASE(Fixed64_Helpers) {
    using namespace edge26;
    TEST_EXPECT_EQ(Abs(Fixed64::FromInt(-5)).ToInt(), (int64_t)5);
    TEST_EXPECT_EQ(Abs(Fixed64::FromInt( 5)).ToInt(), (int64_t)5);
    TEST_EXPECT_EQ(Min(Fixed64::FromInt(3), Fixed64::FromInt(7)).ToInt(), (int64_t)3);
    TEST_EXPECT_EQ(Max(Fixed64::FromInt(3), Fixed64::FromInt(7)).ToInt(), (int64_t)7);
    TEST_EXPECT_EQ(Clamp(Fixed64::FromInt(10),  Fixed64::FromInt(-2), Fixed64::FromInt(5)).ToInt(),  (int64_t)5);
    TEST_EXPECT_EQ(Clamp(Fixed64::FromInt(-10), Fixed64::FromInt(-2), Fixed64::FromInt(5)).ToInt(), (int64_t)-2);
    return 0;
}

TEST_CASE(Fixed32_Basics) {
    using edge26::Fixed32;
    TEST_EXPECT_EQ(Fixed32::FromInt(0).ToInt(),     (int32_t)0);
    TEST_EXPECT_EQ(Fixed32::FromInt(100).ToInt(),   (int32_t)100);
    TEST_EXPECT_EQ((Fixed32::FromInt(3) + Fixed32::FromInt(4)).ToInt(), (int32_t)7);
    TEST_EXPECT_EQ((Fixed32::FromInt(3) - Fixed32::FromInt(4)).ToInt(), (int32_t)-1);
    TEST_EXPECT_EQ((Fixed32::FromInt(6) * Fixed32::FromInt(7)).ToInt(), (int32_t)42);
    TEST_EXPECT_EQ((Fixed32::FromInt(20) / Fixed32::FromInt(4)).ToInt(), (int32_t)5);
    return 0;
}

TEST_CASE(FixedVec3_AddScale) {
    using namespace edge26;
    FixedVec3 a{Fixed64::FromInt(1), Fixed64::FromInt(2), Fixed64::FromInt(3)};
    FixedVec3 b{Fixed64::FromInt(4), Fixed64::FromInt(5), Fixed64::FromInt(6)};
    FixedVec3 sum = a + b;
    TEST_EXPECT_EQ(sum.X.ToInt(), (int64_t)5);
    TEST_EXPECT_EQ(sum.Y.ToInt(), (int64_t)7);
    TEST_EXPECT_EQ(sum.Z.ToInt(), (int64_t)9);

    FixedVec3 scaled = a * Fixed64::FromInt(3);
    TEST_EXPECT_EQ(scaled.X.ToInt(), (int64_t)3);
    TEST_EXPECT_EQ(scaled.Y.ToInt(), (int64_t)6);
    TEST_EXPECT_EQ(scaled.Z.ToInt(), (int64_t)9);
    return 0;
}

TEST_CASE(FixedVec3_Dot) {
    using namespace edge26;
    FixedVec3 a{Fixed64::FromInt(1), Fixed64::FromInt(2), Fixed64::FromInt(3)};
    FixedVec3 b{Fixed64::FromInt(4), Fixed64::FromInt(-5), Fixed64::FromInt(6)};
    TEST_EXPECT_EQ(Dot(a, b).ToInt(), (int64_t)12);
    return 0;
}

TEST_CASE(FixedAngle_Normalization) {
    using edge26::FixedAngle;
    // 2π raw should normalize to 0 (within 1 ulp).
    FixedAngle a = FixedAngle::FromRaw(FixedAngle::TwoPiRaw());
    TEST_EXPECT_NEAR_INT(a.Raw.Raw, 0, 1);

    // 3π should normalize to ≈ π (or -π)
    FixedAngle b = FixedAngle::FromRaw((int32_t)(3 * FixedAngle::PiRaw()));
    int32_t diffPi    = b.Raw.Raw - FixedAngle::PiRaw();
    int32_t diffNegPi = b.Raw.Raw + FixedAngle::PiRaw();
    int32_t minDiff = (diffPi < 0 ? -diffPi : diffPi);
    int32_t altDiff = (diffNegPi < 0 ? -diffNegPi : diffNegPi);
    if (altDiff < minDiff) minDiff = altDiff;
    TEST_EXPECT_TRUE(minDiff <= 1);
    return 0;
}

int RunMathTests() {
    TEST_RUN(Fixed64_FromInt_RoundTrip);
    TEST_RUN(Fixed64_Add);
    TEST_RUN(Fixed64_Negation);
    TEST_RUN(Mul64Q32_BasicVectors);
    TEST_RUN(Fixed64_Multiply);
    TEST_RUN(Fixed64_Divide);
    TEST_RUN(Fixed64_Helpers);
    TEST_RUN(Fixed32_Basics);
    TEST_RUN(FixedVec3_AddScale);
    TEST_RUN(FixedVec3_Dot);
    TEST_RUN(FixedAngle_Normalization);
    return 0;
}
