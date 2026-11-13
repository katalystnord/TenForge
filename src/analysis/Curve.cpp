#include "analysis/Curve.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>

namespace tf::analysis {

namespace {

std::string percent(double strain)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g %%", strain * 100.0);
    return buf;
}

Determination refused(const std::string &unit, const std::string &why)
{
    Determination d;
    d.unit = unit;
    d.reason = why;
    return d;
}

Determination found(double value, const std::string &unit, const std::string &primitive,
                    const CurvePoint &at)
{
    Determination d;
    d.determined = true;
    d.value = value;
    d.unit = unit;
    d.primitive = primitive;
    d.point = at;
    return d;
}

const char *unitOf(CurveQuantity q)
{
    switch (q) {
    case CurveQuantity::MaxForce:
        return "N";
    case CurveQuantity::StrainAtMax:
    case CurveQuantity::YieldStrain:
    case CurveQuantity::BreakStrain:
    case CurveQuantity::StrainAtPeak:
        return "1";
    default:
        return "Pa";
    }
}

struct Modulus {
    LinearFit fit;
    double from = 0, to = 0;  // the window, in strain from the origin used
    double x0 = 0;            // recorded strain where the line meets zero stress
};

// The steepest least-squares line over any stretch of `width` in strain,
// among samples 0..end.
Outcome<Modulus> steepest(Samples e, Samples s, double width, std::size_t end)
{
    std::optional<LinearFit> best;
    std::size_t j = 0;
    for (std::size_t i = 0; i <= end; ++i) {
        j = std::max(j, i);
        while (j + 1 <= end && e[j + 1] <= e[i] + width)
            ++j;
        // A stretch must span (nearly) the full width to count, so a few
        // samples at the end of the data cannot win on noise.
        if (j - i + 1 < 5 || e[j] - e[i] < 0.9 * width)
            continue;
        const auto f = fitLine(e, s, i, j);
        if (f && (!best || f->slope > best->slope))
            best = *f;
    }
    if (!best)
        return Outcome<Modulus>::fail("no stretch of " + percent(width)
                                      + " strain with at least 5 samples before the maximum");
    if (best->slope <= 0)
        return Outcome<Modulus>::fail("the curve does not rise before the maximum");
    Modulus m;
    m.fit = *best;
    m.x0 = -best->intercept / best->slope;
    m.from = e[best->first];
    m.to = e[best->last];
    return Outcome<Modulus>::ok(m);
}

// A line between two strains, counted from the origin x0; for the
// elastic-line zero the origin depends on the line, so iterate.
Outcome<Modulus> windowed(Samples e, Samples s, const CurveMethod &m, std::size_t end)
{
    double x0 = 0;
    Outcome<Modulus> out = Outcome<Modulus>::fail("not computed");
    for (int it = 0; it < 50; ++it) {
        std::vector<double> shifted(e.begin(), e.end());
        if (!m.windowOnRecordedStrain)
            for (double &v : shifted)
                v -= x0;
        const auto w = windowByValue(shifted, m.modulusFrom, m.modulusTo, 0, end);
        if (!w)
            return Outcome<Modulus>::fail("modulus window " + w.reason);
        const auto f = fitLine(e, s, w->first, w->last);
        if (!f)
            return Outcome<Modulus>::fail("modulus fit: " + f.reason);
        if (f->slope <= 0)
            return Outcome<Modulus>::fail("the curve does not rise in the modulus window");
        Modulus md;
        md.fit = *f;
        md.from = m.modulusFrom;
        md.to = m.modulusTo;
        md.x0 = -f->intercept / f->slope;
        out = Outcome<Modulus>::ok(md);
        if (m.zero == CurveMethod::Zero::AsRecorded || m.windowOnRecordedStrain)
            break;
        const bool settled = std::fabs(md.x0 - x0) <= 1e-12;
        x0 = md.x0;
        if (settled)
            break;
    }
    return out;
}

} // namespace

AnalysisResult analyseCurve(const CurveInput &in, const CurveMethod &m)
{
    AnalysisResult out;
    auto &R = out.results;
    auto refuseAll = [&](const std::string &why) {
        for (const auto &q : m.requests)
            R[q.id] = refused(unitOf(q.quantity), why);
    };

    const std::size_t n = in.force.size();
    if (n < 10 || in.stress.size() != n || in.strain.size() != n) {
        refuseAll("force, stress and strain must have the same length, at least 10 samples");
        return out;
    }
    const auto &F = in.force;
    const auto &s = in.stress;
    const auto &e = in.strain;

    std::vector<double> idx(n);
    for (std::size_t i = 0; i < n; ++i)
        idx[i] = double(i);
    const CurvePoint fmax = maximum(idx, F, 0, n - 1);
    const auto iFm = std::size_t(fmax.index);

    // Strain may stop before force does (a deflectometer or extensometer
    // taken off); everything needing strain stays where it was measured.
    std::size_t nE = 0;
    while (nE < n && std::isfinite(e[nE]))
        ++nE;
    if (nE < 10) {
        refuseAll("the strain has fewer than 10 measured samples");
        for (const auto &q : m.requests)
            if (q.quantity == CurveQuantity::MaxForce)
                R[q.id] = found(fmax.y, "N", "maximum", fmax);
        return out;
    }
    const std::size_t lastE = nE - 1;
    if (nE < n)
        out.flags.push_back("strain was measured for the first " + std::to_string(nE) + " of "
                            + std::to_string(n) + " samples");
    const std::string strainEnds = "the strain stops at " + percent(e[lastE]) + ", before this point";

    // --- The break ---------------------------------------------------------------
    const Fracture brk = fracture(e, F, m.breakRatio, m.breakMinDrop, F[iFm], iFm);
    const bool broke = !brk.atEnd;
    // After a break the samples are the broken specimen, not the curve.
    const std::size_t curveEnd = broke ? std::size_t(brk.point.index) : n - 1;
    const std::size_t strainEnd = std::min(curveEnd, lastE);

    // --- The modulus, and the zero point ------------------------------------------
    bool wantModulus = m.zero == CurveMethod::Zero::ElasticLine;
    for (const auto &q : m.requests)
        wantModulus = wantModulus || q.quantity == CurveQuantity::Modulus;
    Outcome<Modulus> mod = Outcome<Modulus>::fail("not asked for");
    if (wantModulus) {
        const std::size_t end = std::min(iFm, lastE);
        mod = m.modulus == CurveMethod::Modulus::MaxSlope ? steepest(e, s, m.maxSlopeWidth, end)
                                                          : windowed(e, s, m, end);
    }
    double x0 = 0;
    if (m.zero == CurveMethod::Zero::ElasticLine) {
        if (!mod) {
            refuseAll("no zero point: " + mod.reason);
            for (const auto &q : m.requests)
                if (q.quantity == CurveQuantity::MaxForce)
                    R[q.id] = found(fmax.y, "N", "maximum", fmax);
            return out;
        }
        x0 = mod->x0;
    }
    out.strainOrigin = x0;

    // --- The maxima ----------------------------------------------------------------
    const CurvePoint smax = maximum(e, s, 0, curveEnd);
    std::optional<CurvePoint> yield;
    std::string noYield;
    {
        const auto p = sustainedPeak(e, s, m.yieldMinDrop, m.yieldMinSpan, 0, strainEnd);
        if (p)
            yield = *p;
        else
            noYield = "no yield: the stress has no maximum it then falls " + percent(m.yieldMinDrop)
                      + " below and stays below for " + percent(m.yieldMinSpan) + " strain";
    }
    std::vector<double> fromZero(e.begin(), e.end());
    for (double &v : fromZero)
        v -= x0;

    for (const CurveRequest &q : m.requests) {
        const std::string unit = unitOf(q.quantity);
        switch (q.quantity) {
        case CurveQuantity::Modulus: {
            if (!mod) {
                R[q.id] = refused(unit, mod.reason);
                break;
            }
            Determination d;
            d.determined = true;
            d.value = mod->fit.slope;
            d.unit = unit;
            d.primitive = m.modulus == CurveMethod::Modulus::MaxSlope ? "max-slope" : "linear-fit";
            d.fit = mod->fit;
            // The window as strain from the zero point, as results report
            // every strain.
            const bool recorded =
                m.modulus == CurveMethod::Modulus::MaxSlope || m.windowOnRecordedStrain;
            d.window = std::make_pair(mod->from - (recorded ? x0 : 0.0),
                                      mod->to - (recorded ? x0 : 0.0));
            d.windowAxis = "strain";
            R[q.id] = d;
            break;
        }
        case CurveQuantity::MaxForce:
            R[q.id] = found(F[iFm], unit, "maximum", CurvePoint{e[iFm], s[iFm], double(iFm)});
            break;
        case CurveQuantity::MaxStress:
            R[q.id] = found(smax.y, unit, "maximum", smax);
            break;
        case CurveQuantity::StrainAtMax:
            if (std::size_t(smax.index) > lastE)
                R[q.id] = refused(unit, strainEnds + " (maximum)");
            else
                R[q.id] = found(smax.x - x0, unit, "maximum", smax);
            break;
        case CurveQuantity::YieldStress:
            R[q.id] = yield ? found(yield->y, unit, "zero-slope", *yield) : refused(unit, noYield);
            break;
        case CurveQuantity::YieldStrain:
            R[q.id] = yield ? found(yield->x - x0, unit, "zero-slope", *yield) : refused(unit, noYield);
            break;
        case CurveQuantity::StressAtStrain: {
            const auto p = valueAt(std::span<const double>(fromZero).first(strainEnd + 1),
                                   std::span<const double>(s).first(strainEnd + 1), q.at);
            if (!p) {
                R[q.id] = refused(unit, "the curve never reaches a strain of " + percent(q.at)
                                            + (broke ? " before the break" : ""));
                break;
            }
            CurvePoint at = *p;
            at.x += x0;
            R[q.id] = found(at.y, unit, "value-at", at);
            break;
        }
        case CurveQuantity::BreakStress:
        case CurveQuantity::BreakStrain: {
            if (!broke) {
                R[q.id] = refused(unit, "no break: the force never falls suddenly after the maximum");
                break;
            }
            const auto ib = std::size_t(brk.point.index);
            const CurvePoint at{e[ib], s[ib], double(ib)};
            if (q.quantity == CurveQuantity::BreakStress)
                R[q.id] = found(s[ib], unit, "break", at);
            else if (ib > lastE)
                R[q.id] = refused(unit, strainEnds + " (break)");
            else
                R[q.id] = found(e[ib] - x0, unit, "break", at);
            break;
        }
        case CurveQuantity::PeakStress:
        case CurveQuantity::StrainAtPeak: {
            // The window ends where the strain first reaches `at`, or at
            // the end of the curve if it never does.
            const auto reach = valueAt(std::span<const double>(fromZero).first(strainEnd + 1),
                                       std::span<const double>(s).first(strainEnd + 1), q.at);
            const std::size_t iEnd = reach ? std::size_t(std::floor(reach->index)) : strainEnd;
            const double sEnd = reach ? reach->y : s[strainEnd];
            const CurvePoint pk = maximum(e, s, 0, iEnd);
            const bool isPeak = std::size_t(pk.index) < iEnd && sEnd <= pk.y * (1.0 - m.peakMinDrop);
            if (!isPeak) {
                R[q.id] = refused(unit, "no maximum before " + percent(q.at)
                                            + " strain: the stress is still rising there");
                break;
            }
            if (q.quantity == CurveQuantity::PeakStress)
                R[q.id] = found(pk.y, unit, "max-within-window", pk);
            else
                R[q.id] = found(pk.x - x0, unit, "max-within-window", pk);
            break;
        }
        }
    }
    return out;
}

} // namespace tf::analysis
