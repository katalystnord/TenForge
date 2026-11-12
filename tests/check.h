#pragma once

// A minimal check harness for the engine's tests: no Qt, no framework, so
// the tf_analysis tests build with exactly what tf_analysis builds with.
//
//   TF_CASE(name_of_the_case) { TF_REQUIRE(p); TF_CHECK(x == 3); TF_NEAR(a, b, tol); }
//   int main() { return tf::test::run(); }

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace tf::test {

struct Case {
    const char *name;
    std::function<void()> body;
};

inline std::vector<Case> &cases()
{
    static std::vector<Case> all;
    return all;
}

inline int &failures()
{
    static int n = 0;
    return n;
}

struct Register {
    Register(const char *name, std::function<void()> body)
    {
        cases().push_back({name, std::move(body)});
    }
};

inline void fail(const char *file, int line, const std::string &what)
{
    std::printf("  FAIL %s:%d: %s\n", file, line, what.c_str());
    ++failures();
}

inline int run()
{
    int failedCases = 0;
    for (const Case &c : cases()) {
        const int before = failures();
        c.body();
        const bool ok = failures() == before;
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok)
            ++failedCases;
    }
    std::printf("%zu cases, %d failed\n", cases().size(), failedCases);
    return failedCases == 0 ? 0 : 1;
}

} // namespace tf::test

#define TF_CASE(name)                                                         \
    static void name();                                                       \
    static tf::test::Register name##_reg(#name, name);                        \
    static void name()

#define TF_CHECK(cond)                                                        \
    do {                                                                      \
        if (!(cond))                                                          \
            tf::test::fail(__FILE__, __LINE__, #cond);                        \
    } while (0)

// Like TF_CHECK, but ends the case on failure: for a precondition the rest
// of the case depends on (an Outcome about to be dereferenced).
#define TF_REQUIRE(cond)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            tf::test::fail(__FILE__, __LINE__, #cond);                        \
            return;                                                           \
        }                                                                     \
    } while (0)

// |actual - expected| <= tol, with both values printed on failure.
#define TF_NEAR(actual, expected, tol)                                        \
    do {                                                                      \
        const double tf_a_ = (actual), tf_e_ = (expected), tf_t_ = (tol);     \
        if (!(std::fabs(tf_a_ - tf_e_) <= tf_t_)) {                           \
            char tf_buf_[256];                                                \
            std::snprintf(tf_buf_, sizeof tf_buf_,                            \
                          "%s = %.10g, expected %.10g within %.3g",           \
                          #actual, tf_a_, tf_e_, tf_t_);                      \
            tf::test::fail(__FILE__, __LINE__, tf_buf_);                      \
        }                                                                     \
    } while (0)
