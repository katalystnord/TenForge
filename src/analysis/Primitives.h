#pragma once

// The engine's primitives: small, pure functions over sampled curves.
//
// Every standard in TenForge is built from these (ROADMAP, section 3.1).
// They know nothing about a standard, a unit system for display, Qt or a
// file. Inputs are plain arrays of doubles in SI; outputs say how they were
// found, and a result that cannot be found says why instead of returning a
// fallback value (results-v1.md, principle 2).

#include <cstddef>
#include <optional>
#include <span>
#include <string>

namespace tf::analysis {

using Samples = std::span<const double>;

// A value, or the reason it could not be determined. Never both.
template <class T>
struct Outcome {
    std::optional<T> value;
    std::string reason;

    static Outcome ok(T v) { return Outcome{std::move(v), {}}; }
    static Outcome fail(std::string why) { return Outcome{std::nullopt, std::move(why)}; }
    explicit operator bool() const { return value.has_value(); }
    const T &operator*() const { return *value; }
    const T *operator->() const { return &*value; }
};

// A least-squares line y = slope * x + intercept over samples first..last
// (inclusive), with the statistics results-v1.md asks every fit to carry.
struct LinearFit {
    double slope = 0;
    double intercept = 0;
    double r2 = 0;         // coefficient of determination
    double sm = 0;         // standard error of the slope
    double smRel = 0;      // sm / |slope|
    std::size_t first = 0;
    std::size_t last = 0;
    std::size_t n = 0;
};

Outcome<LinearFit> fitLine(Samples x, Samples y, std::size_t first, std::size_t last);

// The contiguous run of samples, starting at or after `start` and ending at
// or before `end`, from the first sample with axis >= from to the last one
// before axis first exceeds to. For a window on a rising part of a curve.
struct IndexRange {
    std::size_t first = 0;
    std::size_t last = 0;
};
Outcome<IndexRange> windowByValue(Samples axis, double from, double to,
                                  std::size_t start, std::size_t end);

// A point on a curve, possibly between two samples. `index` is fractional:
// 41.25 lies a quarter of the way from sample 41 to sample 42.
struct CurvePoint {
    double x = 0;
    double y = 0;
    double index = 0;
};

// y at a given x on the first stretch where x reaches it (linear
// interpolation between the two samples around it), searching from start.
Outcome<CurvePoint> valueAt(Samples x, Samples y, double at, std::size_t start = 0);

// The largest y and where it is; the first sample if several are equal.
// Searches start..end inclusive.
CurvePoint maximum(Samples x, Samples y, std::size_t start, std::size_t end);
CurvePoint minimum(Samples x, Samples y, std::size_t start, std::size_t end);

// The first crossing, after `start`, of the curve with the line
// y = slope * (x - x0 - offset): the offset (proof) method. Interpolated
// between the two samples either side of the crossing, never the nearest
// sample.
Outcome<CurvePoint> offsetIntersection(Samples x, Samples y, double slope, double x0,
                                       double offset, std::size_t start, std::size_t end);

// Fracture by force drop. Scanning from `start`, fracture is effective at
// the first sample i where the fall y[i-1] - y[i] exceeds `ratio` times the
// fall before it AND is at least `minDropFraction` of `reference`. The
// fracture point returned is sample i-1, the last one on the curve. With no
// such drop the curve is taken to end at its last sample, and `atEnd` says
// so.
struct Fracture {
    CurvePoint point;
    bool atEnd = false;
};
Fracture fracture(Samples x, Samples y, double ratio, double minDropFraction,
                  double reference, std::size_t start);

// Upper yield by first sustained peak: the first local maximum p in
// start..end after which y falls by at least `minDropFraction` of y[p]
// and stays at or below y[p] for at least `minSpan` in x. Returns sample p.
Outcome<CurvePoint> sustainedPeak(Samples x, Samples y, double minDropFraction,
                                  double minSpan, std::size_t start, std::size_t end);

// The first local minimum after sample `from`: the first sample whose
// successor is higher, searching to `end`.
Outcome<CurvePoint> firstLocalMinimum(Samples x, Samples y, std::size_t from,
                                      std::size_t end);

} // namespace tf::analysis
