// Series statistics and energy: the remaining Phase 1 primitives.
//
// Expected values are worked by hand or come from published tables; the
// Weibull answer from an independent bisection in Python (written out in
// the case), never from the code under test.

#include "analysis/Statistics.h"
#include "check.h"

#include <cmath>
#include <vector>

using namespace tf::analysis;

TF_CASE(student_t_quantiles_match_the_published_table)
{
    // Two-sided 95 %: t(0.975, dof), from any statistics table.
    TF_NEAR(studentT(0.95, 1), 12.706204736, 1e-6);
    TF_NEAR(studentT(0.95, 4), 2.776445105, 1e-8);
    TF_NEAR(studentT(0.95, 9), 2.262157163, 1e-8);
    TF_NEAR(studentT(0.95, 29), 2.045229642, 1e-8);
    // Two-sided 99 %, dof 10: t(0.995, 10).
    TF_NEAR(studentT(0.99, 10), 3.169272667, 1e-8);
}

TF_CASE(summary_of_five_strengths)
{
    const std::vector<double> x{412.0, 398.5, 405.2, 420.8, 401.3};
    const auto s = summarise(x);
    TF_REQUIRE(bool(s));
    TF_CHECK(s->n == 5);
    TF_NEAR(s->mean, 407.56, 1e-10);
    TF_NEAR(s->sd, 8.9701170561, 1e-9);     // n - 1 in the denominator
    TF_NEAR(s->median, 405.2, 0);
    // ISO 2602: mean +- t(0.975, n-1) s / sqrt(n).
    const double half = 2.776445105 * 8.9701170561 / std::sqrt(5.0);
    TF_NEAR(s->ciLow, 407.56 - half, 1e-7);
    TF_NEAR(s->ciHigh, 407.56 + half, 1e-7);
}

TF_CASE(median_of_an_even_count_is_the_middle_pair_averaged)
{
    const std::vector<double> x{4, 1, 3, 2};
    const auto s = summarise(x);
    TF_REQUIRE(bool(s));
    TF_NEAR(s->median, 2.5, 0);
}

TF_CASE(one_value_has_no_spread_and_says_so)
{
    const std::vector<double> x{5};
    const auto s = summarise(x);
    TF_CHECK(!s);
    TF_CHECK(s.reason.find("2") != std::string::npos);
}

TF_CASE(missing_values_are_refused_not_skipped)
{
    const std::vector<double> x{1, NAN, 3};
    const auto s = summarise(x);
    TF_CHECK(!s);
}

TF_CASE(weibull_maximum_likelihood_on_five_strengths)
{
    // Independent check (Python, bisection on the ML equation for the
    // shape, values scaled by the maximum to avoid overflow):
    //   shape 50.77774929, scale 411.67088420.
    const std::vector<double> x{412.0, 398.5, 405.2, 420.8, 401.3};
    const auto w = weibullMle(x);
    TF_REQUIRE(bool(w));
    TF_NEAR(w->shape, 50.77774929, 1e-6);
    TF_NEAR(w->scale, 411.67088420, 1e-6);
}

TF_CASE(weibull_refuses_non_positive_values)
{
    const std::vector<double> x{3, 0, 4};
    TF_CHECK(!weibullMle(x));
}

TF_CASE(energy_is_the_area_under_the_curve)
{
    // A triangle rising to 10 at x = 2 and back to 0 at x = 4: area 20.
    const std::vector<double> x{0, 1, 2, 3, 4};
    const std::vector<double> y{0, 5, 10, 5, 0};
    TF_NEAR(energy(x, y, 0, 4), 20.0, 1e-12);
    TF_NEAR(energy(x, y, 0, 2), 10.0, 1e-12);
}

int main() { return tf::test::run(); }
