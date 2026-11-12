#pragma once

// Statistics over a series of tests, and the energy under a curve.
//
// The series primitives a profile's "statistics" section names
// (profile-v1.md, 4.6): mean, sample standard deviation, median, the
// confidence interval of the mean as ISO 2602 sets it out (Student's t),
// and a two-parameter Weibull fit by maximum likelihood.

#include "analysis/Primitives.h"

#include <cstddef>

namespace tf::analysis {

struct Summary {
    std::size_t n = 0;
    double mean = 0;
    double sd = 0;        // sample standard deviation, n - 1
    double median = 0;
    double ciLow = 0;     // confidence interval of the mean
    double ciHigh = 0;
    double confidence = 0;
};

// Refuses fewer than 2 values and any value that is not finite: a missing
// result is not silently left out of a mean.
Outcome<Summary> summarise(Samples values, double confidence = 0.95);

// Two-sided quantile of Student's t: the t with P(|T| <= t) = confidence.
double studentT(double confidence, int dof);

struct Weibull {
    double shape = 0;  // m
    double scale = 0;  // sigma_0
};

// Maximum-likelihood estimates; refuses fewer than 2 values, any value
// that is not positive, and all-equal values.
Outcome<Weibull> weibullMle(Samples values);

// The area under y(x) from sample first to last, by the trapezoid rule:
// work, when y is force in N and x extension in m.
double energy(Samples x, Samples y, std::size_t first, std::size_t last);

} // namespace tf::analysis
