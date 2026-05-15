// TestHarness.h — minimal zero-dependency test macros for the standalone.
#pragma once

#include <cstdio>
#include <cstdint>

#define TEST_FAIL(fmt, ...) do { \
    std::printf("  FAIL %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    return 1; \
} while (0)

#define TEST_EXPECT_TRUE(cond) do { \
    if (!(cond)) TEST_FAIL("expected true: %s", #cond); \
} while (0)

#define TEST_EXPECT_EQ(a, b) do { \
    auto _ta = (a); auto _tb = (b); \
    if (!(_ta == _tb)) TEST_FAIL("%s == %s -- got %lld, expected %lld", \
        #a, #b, (long long)_ta, (long long)_tb); \
} while (0)

#define TEST_EXPECT_NEAR_INT(a, b, tol) do { \
    auto _ta = (long long)(a); auto _tb = (long long)(b); auto _tt = (long long)(tol); \
    long long _diff = _ta > _tb ? _ta - _tb : _tb - _ta; \
    if (_diff > _tt) TEST_FAIL("%s ~ %s (tol %lld) -- got %lld, expected %lld, diff %lld", \
        #a, #b, _tt, _ta, _tb, _diff); \
} while (0)

#define TEST_CASE(name)  static int name()
#define TEST_RUN(name)   do { std::printf("  RUN  %s\n", #name); \
    int _r = name(); if (_r != 0) { return _r; } } while (0)
