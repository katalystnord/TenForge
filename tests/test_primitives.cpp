// The engine's primitives, each against a hand-worked case.
//
// The arrays are small and written out so every expected value can be
// checked on paper; none is produced by the code under test.

#include "analysis/Primitives.h"
#include "check.h"

#include <vector>

using namespace tf::analysis;

TF_CASE(line_fit_recovers_an_exact_line_and_reports_its_statistics)
{
    const std::vector<double> x{0, 1, 2, 3, 4, 5};
    const std::vector<double> y{1, 3, 5, 7, 9, 11};  // y = 2x + 1
    const auto f = fitLine(x, y, 1, 4);
    TF_REQUIRE(bool(f));
    TF_NEAR(f->slope, 2.0, 1e-12);
    TF_NEAR(f->intercept, 1.0, 1e-12);
    TF_NEAR(f->r2, 1.0, 1e-12);
    TF_NEAR(f->sm, 0.0, 1e-12);
    TF_CHECK(f->n == 4 && f->first == 1 && f->last == 4);
}

TF_CASE(line_fit_slope_error_matches_the_textbook_formula)
{
    // Worked by hand. Data: (0,1) (1,0) (2,1) (3,4); mean x 1.5, mean y 1.5.
    // Sxx = 2.25+0.25+0.25+2.25 = 5; Sxy = (-1.5)(-0.5)+(-0.5)(-1.5)+(0.5)(-0.5)+(1.5)(2.5)
    //     = 0.75 + 0.75 - 0.25 + 3.75 = 5  -> slope 1, intercept 0.
    // Residuals: 1, -1, -1, 1 -> SSE 4; s^2 = 4/(n-2) = 2; sm = sqrt(2/5).
    // SST = 0.25+2.25+0.25+6.25 = 9 -> r2 = 1 - 4/9.
    const std::vector<double> x{0, 1, 2, 3};
    const std::vector<double> y{1, 0, 1, 4};
    const auto f = fitLine(x, y, 0, 3);
    TF_REQUIRE(bool(f));
    TF_NEAR(f->slope, 1.0, 1e-12);
    TF_NEAR(f->intercept, 0.0, 1e-12);
    TF_NEAR(f->sm, std::sqrt(2.0 / 5.0), 1e-12);
    TF_NEAR(f->smRel, std::sqrt(2.0 / 5.0), 1e-12);
    TF_NEAR(f->r2, 1.0 - 4.0 / 9.0, 1e-12);
}

TF_CASE(line_fit_refuses_fewer_than_three_points_and_says_why)
{
    const std::vector<double> x{0, 1, 2};
    const std::vector<double> y{0, 1, 2};
    const auto f = fitLine(x, y, 0, 1);
    TF_CHECK(!f);
    TF_CHECK(f.reason.find("3") != std::string::npos);
}

TF_CASE(window_by_value_takes_the_first_rising_run_only)
{
    //                 0  1  2  3  4  5  6  7  8
    const std::vector<double> a{0, 1, 2, 3, 4, 5, 3, 2, 4};
    const auto w = windowByValue(a, 2, 4, 0, 8);
    TF_REQUIRE(bool(w));
    TF_CHECK(w->first == 2);
    TF_CHECK(w->last == 4);  // 5 exceeds 4; the later 2..4 run is not used
}

TF_CASE(value_at_interpolates_between_the_samples_around_it)
{
    const std::vector<double> x{0, 1, 2, 4};
    const std::vector<double> y{0, 10, 20, 30};
    const auto p = valueAt(x, y, 3.0);
    TF_REQUIRE(bool(p));
    TF_NEAR(p->y, 25.0, 1e-12);
    TF_NEAR(p->index, 2.5, 1e-12);
    TF_CHECK(!valueAt(x, y, 5.0));
}

TF_CASE(offset_intersection_is_interpolated_not_the_nearest_sample)
{
    // Elastic line y = 10 x; the curve bends flat at y = 3 from x = 0.3 on.
    // Offset 0.1: line y = 10 (x - 0.1) meets y = 3 at x = 0.4 exactly,
    // which lies between samples at 0.35 and 0.5.
    const std::vector<double> x{0, 0.1, 0.2, 0.3, 0.35, 0.5, 0.7};
    const std::vector<double> y{0, 1, 2, 3, 3, 3, 3};
    const auto p = offsetIntersection(x, y, 10.0, 0.0, 0.1, 0, 6);
    TF_REQUIRE(bool(p));
    TF_NEAR(p->x, 0.4, 1e-12);
    TF_NEAR(p->y, 3.0, 1e-12);
    TF_NEAR(p->index, 4 + (0.4 - 0.35) / 0.15, 1e-12);
}

TF_CASE(offset_intersection_respects_the_zero_point)
{
    // Same curve shifted right by 0.05 (a toe): with x0 = 0.05 the answer
    // shifts by exactly the same amount.
    const std::vector<double> x{0.05, 0.15, 0.25, 0.35, 0.40, 0.55, 0.75};
    const std::vector<double> y{0, 1, 2, 3, 3, 3, 3};
    const auto p = offsetIntersection(x, y, 10.0, 0.05, 0.1, 0, 6);
    TF_REQUIRE(bool(p));
    TF_NEAR(p->x, 0.45, 1e-12);
}

TF_CASE(offset_intersection_says_when_the_curve_never_crosses)
{
    const std::vector<double> x{0, 1, 2, 3};
    const std::vector<double> y{0, 10, 20, 30};  // stays on the elastic line
    const auto p = offsetIntersection(x, y, 10.0, 0.0, 0.5, 0, 3);
    TF_CHECK(!p);
    TF_CHECK(!p.reason.empty());
}

TF_CASE(maximum_is_the_first_of_equal_peaks)
{
    const std::vector<double> x{0, 1, 2, 3, 4};
    const std::vector<double> y{1, 5, 2, 5, 0};
    const auto p = maximum(x, y, 0, 4);
    TF_NEAR(p.index, 1.0, 0);
    TF_NEAR(p.y, 5.0, 0);
}

TF_CASE(fracture_is_the_last_point_before_the_collapse)
{
    // Falls of 1, 1, 1, then 20: the 20 is more than 5x the fall before it
    // and more than 10 % of the maximum (30). The fracture point is the
    // sample before the collapse, index 4.
    const std::vector<double> x{0, 1, 2, 3, 4, 5};
    const std::vector<double> y{30, 29, 28, 27, 26, 6};
    const auto f = fracture(x, y, 5.0, 0.10, 30.0, 0);
    TF_CHECK(!f.atEnd);
    TF_NEAR(f.point.index, 4.0, 0);
    TF_NEAR(f.point.x, 4.0, 0);
}

TF_CASE(fracture_ignores_a_sharp_but_small_fall)
{
    // A fall of 2 after falls of 0.1 is 20x, but only 2/30 of the maximum.
    const std::vector<double> x{0, 1, 2, 3, 4};
    const std::vector<double> y{30, 29.9, 29.8, 27.8, 27.7};
    const auto f = fracture(x, y, 5.0, 0.10, 30.0, 0);
    TF_CHECK(f.atEnd);
    TF_NEAR(f.point.index, 4.0, 0);
}

TF_CASE(sustained_peak_finds_an_upper_yield_point)
{
    //                         0  1  2   3   4   5   6   7   8   9
    const std::vector<double> x{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    const std::vector<double> y{0, 4, 8, 10, 9, 8.5, 9, 8.8, 9.5, 11};
    // Peak at 3 (10); y stays <= 10 for x 3..8 (span 5); drop to 8.5 is 15 %.
    const auto p = sustainedPeak(x, y, 0.01, 4.0, 0, 9);
    TF_REQUIRE(bool(p));
    TF_NEAR(p->index, 3.0, 0);
    // With a required span longer than the plateau there is none.
    TF_CHECK(!sustainedPeak(x, y, 0.01, 6.0, 0, 9));
}

TF_CASE(sustained_peak_ignores_a_noise_wiggle)
{
    const std::vector<double> x{0, 1, 2, 3, 4, 5, 6};
    const std::vector<double> y{0, 5, 5.02, 5.01, 6, 7, 8};
    TF_CHECK(!sustainedPeak(x, y, 0.01, 0.5, 0, 6));
}

TF_CASE(first_local_minimum_after_a_point)
{
    const std::vector<double> x{0, 1, 2, 3, 4, 5, 6};
    const std::vector<double> y{10, 9, 7, 7, 8, 6, 9};
    const auto p = firstLocalMinimum(x, y, 0, 6);
    TF_REQUIRE(bool(p));
    TF_NEAR(p->index, 3.0, 0);  // the last of the two equal 7s, then it rises
}

int main() { return tf::test::run(); }
