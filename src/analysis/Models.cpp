#include "analysis/Models.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace tf::analysis {

namespace {

std::string mm(double metres)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g mm", metres * 1e3);
    return buf;
}

// The distance from a support to the nearest loading point, or why the
// geometry is not a beam.
Outcome<double> loadArm(const FlexureGeometry &g)
{
    if (!(g.width > 0))
        return Outcome<double>::fail("the specimen's width is missing or zero");
    if (!(g.thickness > 0))
        return Outcome<double>::fail("the specimen's thickness is missing or zero");
    if (!(g.span > 0))
        return Outcome<double>::fail("the support span is missing or zero");
    if (g.loadingSpan < 0 || !std::isfinite(g.loadingSpan))
        return Outcome<double>::fail("the loading span must be zero (three-point) or positive");
    if (!(g.loadingSpan < g.span))
        return Outcome<double>::fail("the loading span (" + mm(g.loadingSpan)
                                     + ") must be shorter than the support span ("
                                     + mm(g.span) + ")");
    return Outcome<double>::ok((g.span - g.loadingSpan) / 2.0);
}

} // namespace

Outcome<double> flexureStressFactor(const FlexureGeometry &g)
{
    const auto a = loadArm(g);
    if (!a)
        return Outcome<double>::fail(a.reason);
    return Outcome<double>::ok(3.0 * *a / (g.width * g.thickness * g.thickness));
}

Outcome<double> flexureStrainFactor(const FlexureGeometry &g)
{
    const auto a = loadArm(g);
    if (!a)
        return Outcome<double>::fail(a.reason);
    const double L = g.span, h = g.thickness;
    if (g.deflectionAt == FlexureGeometry::DeflectionAt::Midspan)
        return Outcome<double>::ok(12.0 * h / (3.0 * L * L - 4.0 * *a * *a));
    return Outcome<double>::ok(3.0 * h / (*a * (3.0 * L - 4.0 * *a)));
}

Outcome<std::vector<double>> complianceCorrected(Samples force, Samples displacement,
                                                 Samples tableForce,
                                                 Samples tableDisplacement)
{
    using Out = Outcome<std::vector<double>>;
    if (force.size() != displacement.size())
        return Out::fail("force and displacement have different lengths");
    if (tableForce.size() < 2 || tableForce.size() != tableDisplacement.size())
        return Out::fail("the compliance table needs at least two rows of force and displacement");
    for (std::size_t i = 1; i < tableForce.size(); ++i)
        if (!(tableForce[i] > tableForce[i - 1]))
            return Out::fail("the compliance table's forces must rise from row to row");

    const double lo = tableForce.front(), hi = tableForce.back();
    std::vector<double> out(force.size());
    std::size_t j = 1;
    for (std::size_t i = 0; i < force.size(); ++i) {
        const double f = force[i];
        if (!std::isfinite(f) || !std::isfinite(displacement[i])) {
            out[i] = std::nan("");
            continue;
        }
        if (f < lo || f > hi) {
            char buf[160];
            std::snprintf(buf, sizeof buf,
                          "the compliance table covers %g N to %g N; the test reaches %g N",
                          lo, hi, f);
            return Out::fail(buf);
        }
        // Forces rise and fall over a test; search from where the last one
        // was found, in either direction.
        while (j + 1 < tableForce.size() && f > tableForce[j])
            ++j;
        while (j > 1 && f < tableForce[j - 1])
            --j;
        const double t = (f - tableForce[j - 1]) / (tableForce[j] - tableForce[j - 1]);
        const double machine =
            tableDisplacement[j - 1] + t * (tableDisplacement[j] - tableDisplacement[j - 1]);
        out[i] = displacement[i] - machine;
    }
    return Out::ok(std::move(out));
}

} // namespace tf::analysis
