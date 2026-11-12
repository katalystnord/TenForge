#include "analysis/Statistics.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace tf::analysis {

namespace {

// Continued fraction for the regularised incomplete beta function, by the
// modified Lentz method.
double betaContinuedFraction(double a, double b, double x)
{
    const double tiny = 1e-300;
    const double eps = 1e-15;
    const double qab = a + b, qap = a + 1, qam = a - 1;
    double c = 1.0;
    double d = 1.0 - qab * x / qap;
    if (std::fabs(d) < tiny)
        d = tiny;
    d = 1.0 / d;
    double h = d;
    for (int m = 1; m <= 1000; ++m) {
        const int m2 = 2 * m;
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (std::fabs(d) < tiny)
            d = tiny;
        c = 1.0 + aa / c;
        if (std::fabs(c) < tiny)
            c = tiny;
        d = 1.0 / d;
        h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (std::fabs(d) < tiny)
            d = tiny;
        c = 1.0 + aa / c;
        if (std::fabs(c) < tiny)
            c = tiny;
        d = 1.0 / d;
        const double del = d * c;
        h *= del;
        if (std::fabs(del - 1.0) < eps)
            break;
    }
    return h;
}

// I_x(a, b).
double incompleteBeta(double a, double b, double x)
{
    if (x <= 0)
        return 0;
    if (x >= 1)
        return 1;
    const double front = std::exp(std::lgamma(a + b) - std::lgamma(a) - std::lgamma(b)
                                  + a * std::log(x) + b * std::log1p(-x));
    if (x < (a + 1) / (a + b + 2))
        return front * betaContinuedFraction(a, b, x) / a;
    return 1.0 - front * betaContinuedFraction(b, a, 1 - x) / b;
}

// P(|T| <= t) for Student's t with dof degrees of freedom.
double centralProbability(double t, int dof)
{
    const double v = dof;
    return 1.0 - incompleteBeta(v / 2.0, 0.5, v / (v + t * t));
}

} // namespace

double studentT(double confidence, int dof)
{
    if (dof < 1 || !(confidence > 0) || !(confidence < 1))
        return std::numeric_limits<double>::quiet_NaN();
    double lo = 0, hi = 1;
    while (centralProbability(hi, dof) < confidence)
        hi *= 2;
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        (centralProbability(mid, dof) < confidence ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

Outcome<Summary> summarise(Samples values, double confidence)
{
    const std::size_t n = values.size();
    if (n < 2)
        return Outcome<Summary>::fail("a spread needs at least 2 values, there are "
                                      + std::to_string(n));
    for (double v : values)
        if (!std::isfinite(v))
            return Outcome<Summary>::fail("a value is missing or not finite; a series with a "
                                          "missing result is not summarised");
    Summary s;
    s.n = n;
    s.confidence = confidence;
    double sum = 0;
    for (double v : values)
        sum += v;
    s.mean = sum / double(n);
    double ss = 0;
    for (double v : values)
        ss += (v - s.mean) * (v - s.mean);
    s.sd = std::sqrt(ss / double(n - 1));
    std::vector<double> sorted(values.begin(), values.end());
    std::sort(sorted.begin(), sorted.end());
    s.median = n % 2 ? sorted[n / 2] : 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);
    const double half = studentT(confidence, int(n) - 1) * s.sd / std::sqrt(double(n));
    s.ciLow = s.mean - half;
    s.ciHigh = s.mean + half;
    return Outcome<Summary>::ok(s);
}

Outcome<Weibull> weibullMle(Samples values)
{
    const std::size_t n = values.size();
    if (n < 2)
        return Outcome<Weibull>::fail("a Weibull fit needs at least 2 values");
    double xmax = 0, xmin = std::numeric_limits<double>::infinity(), meanLog = 0;
    for (double v : values) {
        if (!(v > 0) || !std::isfinite(v))
            return Outcome<Weibull>::fail("a Weibull fit needs positive, finite values");
        xmax = std::max(xmax, v);
        xmin = std::min(xmin, v);
        meanLog += std::log(v);
    }
    meanLog /= double(n);
    if (xmax == xmin)
        return Outcome<Weibull>::fail("all values are equal; the Weibull shape is unbounded");

    // The shape m solves  sum(x^m ln x) / sum(x^m) - 1/m - mean(ln x) = 0,
    // which rises monotonically in m. Values are scaled by the largest so
    // that x^m cannot overflow (the ratio does not change).
    auto g = [&](double m) {
        double a = 0, b = 0;
        for (double v : values) {
            const double w = std::pow(v / xmax, m);
            a += w * std::log(v);
            b += w;
        }
        return a / b - 1.0 / m - meanLog;
    };
    double lo = 1e-3, hi = 1.0;
    while (g(hi) < 0 && hi < 1e7)
        hi *= 2;
    if (g(hi) < 0)
        return Outcome<Weibull>::fail("the Weibull shape did not converge");
    for (int i = 0; i < 300; ++i) {
        const double mid = 0.5 * (lo + hi);
        (g(mid) < 0 ? lo : hi) = mid;
    }
    Weibull w;
    w.shape = 0.5 * (lo + hi);
    double b = 0;
    for (double v : values)
        b += std::pow(v / xmax, w.shape);
    w.scale = xmax * std::pow(b / double(n), 1.0 / w.shape);
    return Outcome<Weibull>::ok(w);
}

double energy(Samples x, Samples y, std::size_t first, std::size_t last)
{
    double area = 0;
    for (std::size_t i = first; i < last && i + 1 < x.size(); ++i)
        area += 0.5 * (y[i] + y[i + 1]) * (x[i + 1] - x[i]);
    return area;
}

} // namespace tf::analysis
