/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#ifndef P3DS_TEST_FRAMEWORK_H
#define P3DS_TEST_FRAMEWORK_H

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace testing {

struct TestCase
{
    const char *name;
    void (*fn)();
};

inline std::vector<TestCase> &registry()
{
    static std::vector<TestCase> cases;
    return cases;
}

inline int &failureCount()
{
    static int count = 0;
    return count;
}

inline bool registerTest(const char *name, void (*fn)())
{
    registry().push_back({ name, fn });
    return true;
}

inline void reportFailure(const char *file, int line, const std::string &message)
{
    failureCount()++;
    printf("    FAIL %s:%d: %s\n", file, line, message.c_str());
}

/** Compares two floating point values with a tolerance. */
inline bool nearlyEqual(double a, double b, double tolerance)
{
    return std::fabs(a - b) <= tolerance;
}

inline int runAll()
{
    int failed = 0;

    for (size_t i = 0; i < registry().size(); i++)
    {
        int before = failureCount();
        printf("[ run  ] %s\n", registry()[i].name);
        registry()[i].fn();

        if (failureCount() != before)
        {
            failed++;
            printf("[ FAIL ] %s\n", registry()[i].name);
        }
        else
        {
            printf("[  ok  ] %s\n", registry()[i].name);
        }
    }

    printf("\n%zu tests, %d failed, %d checks failed\n", registry().size(), failed, failureCount());
    return failed ? 1 : 0;
}

} // namespace testing

#define TEST(name) \
    static void name(); \
    static bool name##_registered = (::testing::registerTest(#name, name), true); \
    static void name()

#define CHECK(cond) \
    do { if (!(cond)) ::testing::reportFailure(__FILE__, __LINE__, "CHECK failed: " #cond); } while (0)

#define CHECK_EQ(a, b) \
    do { if (!((a) == (b))) ::testing::reportFailure(__FILE__, __LINE__, \
        "CHECK_EQ failed: " #a " == " #b); } while (0)

#define CHECK_NE(a, b) \
    do { if (!((a) != (b))) ::testing::reportFailure(__FILE__, __LINE__, \
        "CHECK_NE failed: " #a " != " #b); } while (0)

#define CHECK_NEAR(a, b, tol) \
    do { if (!::testing::nearlyEqual((double)(a), (double)(b), (double)(tol))) \
        ::testing::reportFailure(__FILE__, __LINE__, \
        "CHECK_NEAR failed: " #a " ~= " #b); } while (0)

#endif // P3DS_TEST_FRAMEWORK_H
