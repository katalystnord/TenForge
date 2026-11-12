#include "analysis/Primitives.h"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>

namespace tf::analysis {

namespace {

bool inRange(Samples s, std::size_t i) { return i < s.size(); }

CurvePoint at(Samples x, Samples y, std::size_t i)
{
    return {x[i], y[i], static_cast<double>(i)};
}

// The point a fraction t of the way from sample i to sample i + 1.
CurvePoint between(Samples x, Samples y, std::size_t i, double t)
{
    return {x[i] + t * (x[i + 1] - x[i]), y[i] + t * (y[i + 1] - y[i]),
            static_cast<double>(i) + t};
}

} // namespace

Outcome<LinearFit> fitLine(Samples x, Samples y, std::size_t first, std::size_t last)
{
    if (x.size() != y.size())
        return Outcome<LinearFit>::fail("x and y have different lengths");
    if (last >= x.size() || first > last)
        return Outcome<LinearFit>::fail("fit window lies outside the data");
    const std::size_t n = last - first + 1;
    if (n < 3)
        return Outcome<LinearFit>::fail("a line fit needs at least 3 points, the window has "
                                        + std::to_string(n));

    const Eigen::Map<const Eigen::ArrayXd> xs(x.data() + first, Eigen::Index(n));
    const Eigen::Map<const Eigen::ArrayXd> ys(y.data() + first, Eigen::Index(n));
    const double mx = xs.mean();
    const double my = ys.mean();
    const Eigen::ArrayXd dx = xs - mx;
    const Eigen::ArrayXd dy = ys - my;
    const double sxx = (dx * dx).sum();
    if (sxx <= 0)
        return Outcome<LinearFit>::fail("all x values in the fit window are equal");

    LinearFit f;
    f.slope = (dx * dy).sum() / sxx;
    f.intercept = my - f.slope * mx;
    const Eigen::ArrayXd res = ys - (f.slope * xs + f.intercept);
    const double sse = (res * res).sum();
    const double sst = (dy * dy).sum();
    f.r2 = sst > 0 ? 1.0 - sse / sst : 1.0;
    f.sm = std::sqrt(sse / double(n - 2) / sxx);
    f.smRel = f.slope != 0 ? f.sm / std::fabs(f.slope) : INFINITY;
    f.first = first;
    f.last = last;
    f.n = n;
    return Outcome<LinearFit>::ok(f);
}

Outcome<IndexRange> windowByValue(Samples axis, double from, double to, std::size_t start,
                                  std::size_t end)
{
    end = std::min(end, axis.empty() ? 0 : axis.size() - 1);
    std::size_t i = start;
    while (i <= end && inRange(axis, i) && axis[i] < from)
        ++i;
    if (i > end || !inRange(axis, i))
        return Outcome<IndexRange>::fail("the data never reaches the window's lower limit");
    if (axis[i] > to)
        return Outcome<IndexRange>::fail("no sample lies inside the window; sampling is too coarse");
    std::size_t j = i;
    while (j + 1 <= end && axis[j + 1] <= to)
        ++j;
    return Outcome<IndexRange>::ok({i, j});
}

Outcome<CurvePoint> valueAt(Samples x, Samples y, double target, std::size_t start)
{
    for (std::size_t i = start; i + 1 < x.size(); ++i) {
        const double a = x[i], b = x[i + 1];
        if ((a <= target && target <= b) || (b <= target && target <= a)) {
            const double t = b != a ? (target - a) / (b - a) : 0.0;
            return Outcome<CurvePoint>::ok(between(x, y, i, t));
        }
    }
    return Outcome<CurvePoint>::fail("the curve never reaches that value");
}

CurvePoint maximum(Samples x, Samples y, std::size_t start, std::size_t end)
{
    end = std::min(end, y.size() - 1);
    std::size_t best = start;
    for (std::size_t i = start; i <= end; ++i)
        if (y[i] > y[best])
            best = i;
    return at(x, y, best);
}

CurvePoint minimum(Samples x, Samples y, std::size_t start, std::size_t end)
{
    end = std::min(end, y.size() - 1);
    std::size_t best = start;
    for (std::size_t i = start; i <= end; ++i)
        if (y[i] < y[best])
            best = i;
    return at(x, y, best);
}

Outcome<CurvePoint> offsetIntersection(Samples x, Samples y, double slope, double x0,
                                       double offset, std::size_t start, std::size_t end)
{
    end = std::min(end, y.size() - 1);
    // d > 0 while the curve is above the offset line, d <= 0 once it has
    // crossed it. The crossing is interpolated linearly on d, which is
    // exactly linear interpolation along the curve between the two samples.
    auto d = [&](std::size_t i) { return y[i] - slope * (x[i] - x0 - offset); };
    std::size_t i = start;
    while (i <= end && d(i) <= 0)  // begin where the curve is above the line
        ++i;
    for (; i + 1 <= end; ++i) {
        const double d0 = d(i), d1 = d(i + 1);
        if (d0 > 0 && d1 <= 0) {
            const double t = d0 / (d0 - d1);
            return Outcome<CurvePoint>::ok(between(x, y, i, t));
        }
    }
    return Outcome<CurvePoint>::fail("the curve does not cross the offset line within the data");
}

Fracture fracture(Samples x, Samples y, double ratio, double minDropFraction, double reference,
                  std::size_t start)
{
    const std::size_t n = y.size();
    for (std::size_t i = std::max<std::size_t>(start, 2); i < n; ++i) {
        const double fall = y[i - 1] - y[i];
        const double before = std::max(0.0, y[i - 2] - y[i - 1]);
        if (fall > ratio * before && fall >= minDropFraction * reference)
            return {at(x, y, i - 1), false};
    }
    return {at(x, y, n - 1), true};
}

Outcome<CurvePoint> sustainedPeak(Samples x, Samples y, double minDropFraction, double minSpan,
                                  std::size_t start, std::size_t end)
{
    end = std::min(end, y.size() - 1);
    for (std::size_t p = std::max<std::size_t>(start, 1); p + 1 <= end; ++p) {
        if (!(y[p] >= y[p - 1] && y[p] > y[p + 1]))
            continue;
        // Follow the curve while it stays at or below the peak.
        double lowest = y[p];
        std::size_t q = p;
        while (q + 1 <= end && y[q + 1] <= y[p]) {
            ++q;
            lowest = std::min(lowest, y[q]);
        }
        if (y[p] - lowest >= minDropFraction * std::fabs(y[p]) && x[q] - x[p] >= minSpan)
            return Outcome<CurvePoint>::ok(at(x, y, p));
    }
    return Outcome<CurvePoint>::fail("no peak with a sustained fall after it");
}

Outcome<CurvePoint> firstLocalMinimum(Samples x, Samples y, std::size_t from, std::size_t end)
{
    end = std::min(end, y.size() - 1);
    for (std::size_t i = from; i + 1 <= end; ++i)
        if (y[i + 1] > y[i])
            return Outcome<CurvePoint>::ok(at(x, y, i));
    return Outcome<CurvePoint>::fail("the curve does not turn upwards again");
}

} // namespace tf::analysis
