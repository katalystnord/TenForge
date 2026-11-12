#include "analysis/Tension.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tf::analysis {

std::string proofId(double offset)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4f", offset * 100.0);
    std::string s(buf);
    while (!s.empty() && s.back() == '0')
        s.pop_back();
    if (!s.empty() && s.back() == '.')
        s.pop_back();
    return "Rp" + s;
}

namespace {

Determination notDetermined(const std::string &unit, const std::string &why)
{
    Determination d;
    d.unit = unit;
    d.reason = why;
    return d;
}

Determination determined(double value, const std::string &unit, const std::string &primitive)
{
    Determination d;
    d.determined = true;
    d.value = value;
    d.unit = unit;
    d.primitive = primitive;
    return d;
}

struct Elastic {
    LinearFit fit;
    double from = 0, to = 0;  // the window, on the method's axis
    double x0 = 0;            // strain where the elastic line meets zero stress
};

Outcome<Elastic> elasticFit(Samples e, Samples s, double reference, const TensionMethod &m,
                            std::size_t end)
{
    const bool onStrain = m.modulusAxis == TensionMethod::Axis::Strain;
    const double from = onStrain ? m.modulusFrom : m.modulusFrom * reference;
    const double to = onStrain ? m.modulusTo : m.modulusTo * reference;
    const auto w = windowByValue(onStrain ? e : s, from, to, 0, end);
    if (!w)
        return Outcome<Elastic>::fail("modulus window " + w.reason);
    const auto f = fitLine(e, s, w->first, w->last);
    if (!f)
        return Outcome<Elastic>::fail("modulus fit: " + f.reason);
    if (f->slope <= 0)
        return Outcome<Elastic>::fail("the elastic part does not rise");
    return Outcome<Elastic>::ok({*f, from, to, -f->intercept / f->slope});
}

} // namespace

TensionResult analyseTension(const TensionInput &in, const TensionMethod &m)
{
    TensionResult out;
    auto &R = out.results;
    const std::vector<std::string> stressIds{"E", "ReH", "ReL", "Rm"};
    const std::vector<std::string> strainIds{"Agt", "Ag", "At", "A"};
    // Rb (stress at fracture) needs no strain but does need the area; it is
    // refused with the stress results.

    auto refuseAll = [&](const std::string &why) {
        for (const auto &id : stressIds)
            R[id] = notDetermined("Pa", why);
        R["Rb"] = notDetermined("Pa", why);
        for (double o : m.proofOffsets)
            R[proofId(o)] = notDetermined("Pa", why);
        for (const auto &id : strainIds)
            R[id] = notDetermined("1", why);
    };

    const std::size_t n = in.force.size();
    if (n < 10 || in.extension.size() != n) {
        refuseAll("the force and extension channels must have the same length, at least 10 samples");
        R["Fm"] = notDetermined("N", R["E"].reason);
        return out;
    }

    // Maximum force needs no geometry.
    std::vector<double> idx(n);
    for (std::size_t i = 0; i < n; ++i)
        idx[i] = double(i);
    const CurvePoint fmax = maximum(idx, in.force, 0, n - 1);

    if (!(in.area > 0) || !(in.gaugeLength > 0)) {
        std::string why = !(in.area > 0) ? "the cross-section area S0 is missing or zero"
                                         : "the gauge length is missing or zero";
        if (!(in.area > 0) && !(in.gaugeLength > 0))
            why = "the cross-section area S0 and the gauge length are missing or zero";
        refuseAll(why);
        R["Fm"] = determined(fmax.y, "N", "maximum");
        R["Fm"].point = fmax;
        return out;
    }

    std::vector<double> s(n), e(n);
    for (std::size_t i = 0; i < n; ++i) {
        s[i] = in.force[i] / in.area;
        e[i] = in.extension[i] / in.gaugeLength;
    }
    const auto iFm0 = std::size_t(fmax.index);

    // The extension channel may end before the force channel does (an
    // extensometer taken off mid-test leaves empty cells). Everything that
    // needs a strain is confined to the part where it was measured.
    std::size_t nE = 0;
    while (nE < n && std::isfinite(e[nE]))
        ++nE;
    if (nE < 10) {
        refuseAll("the extension channel has fewer than 10 measured samples");
        R["Fm"] = determined(fmax.y, "N", "maximum");
        R["Fm"].point = fmax;
        return out;
    }
    const std::size_t lastE = nE - 1;
    const std::size_t strainEnd = std::min(iFm0, lastE);
    const std::string extensionEnds = "the extension channel ends at a strain of "
                                      + std::to_string(e[lastE]) + ", before this point";
    if (nE < n)
        out.flags.push_back("extension was measured for the first " + std::to_string(nE) + " of "
                            + std::to_string(n) + " samples");

    // --- Upper yield candidate, and the elastic line --------------------------
    std::optional<CurvePoint> peak;
    if (m.yieldPoint) {
        const auto p = sustainedPeak(e, s, m.yieldMinDrop, m.yieldMinSpan, 0, strainEnd);
        if (p)
            peak = *p;
    }

    Outcome<Elastic> el = Outcome<Elastic>::fail("not computed");
    double reference = 0;
    if (peak) {
        reference = peak->y;
        el = elasticFit(e, s, reference, m, std::size_t(peak->index));
        if (el) {
            const double plastic = peak->x - el->x0 - peak->y / el->fit.slope;
            if (plastic > m.yieldMaxPlastic) {
                out.flags.push_back("a sustained peak was found at a non-proportional strain of "
                                    + std::to_string(plastic)
                                    + ", beyond the yield-point limit; treated as no yield point");
                peak.reset();
            }
        } else {
            peak.reset();
        }
    }
    if (!peak && m.modulusAxis == TensionMethod::Axis::Strain) {
        el = elasticFit(e, s, 0, m, strainEnd);
    } else if (!peak) {
        // Iterate: the window depends on the proof stress, the proof stress
        // on the slope. Start from the maximum stress.
        reference = s[iFm0];
        for (int it = 0; it < m.modulusIterations; ++it) {
            el = elasticFit(e, s, reference, m, strainEnd);
            if (!el)
                break;
            const auto rp = offsetIntersection(e, s, el->fit.slope, el->x0,
                                               m.modulusReferenceOffset, el->fit.first, strainEnd);
            if (!rp) {
                el = Outcome<Elastic>::fail("the reference proof stress for the modulus window: "
                                            + rp.reason);
                break;
            }
            const bool converged = std::fabs(rp->y - reference) <= 1e-9 * std::fabs(reference);
            reference = rp->y;
            if (converged)
                break;
            if (it + 1 == m.modulusIterations)
                out.flags.push_back("the modulus window did not converge in "
                                    + std::to_string(m.modulusIterations) + " iterations");
        }
        // The window must sit on the final reference, so refit once more.
        if (el)
            el = elasticFit(e, s, reference, m, strainEnd);
    }

    if (!el) {
        refuseAll(el.reason);
        R["Fm"] = determined(fmax.y, "N", "maximum");
        R["Fm"].point = fmax;
        return out;
    }

    const double E = el->fit.slope;
    const double x0 = m.strainFromElasticLine ? el->x0 : 0.0;
    out.strainOrigin = x0;
    R["E"] = determined(E, "Pa", "linear-fit");
    R["E"].fit = el->fit;
    R["E"].window = std::make_pair(el->from, el->to);
    R["E"].windowAxis = m.modulusAxis == TensionMethod::Axis::Strain ? "strain" : "stress";

    // --- Maximum force and tensile strength ------------------------------------
    CurvePoint mx = fmax;
    std::size_t iFm = iFm0;
    if (peak) {
        // After a yield point, the maximum is sought after it (the peak
        // itself is not the tensile strength).
        mx = maximum(idx, in.force, std::size_t(peak->index) + 1, n - 1);
        iFm = std::size_t(mx.index);
        if (in.force[iFm] < peak->y * in.area)
            out.flags.push_back("the maximum force after yielding is below the upper yield force");
    }
    R["Fm"] = determined(in.force[iFm], "N", "maximum");
    R["Fm"].point = CurvePoint{e[iFm], in.force[iFm], double(iFm)};
    R["Rm"] = determined(s[iFm], "Pa", "maximum");
    R["Rm"].point = CurvePoint{e[iFm], s[iFm], double(iFm)};
    const std::size_t proofEnd = std::min(iFm, lastE);

    // --- Yield points -------------------------------------------------------------
    if (peak) {
        const auto ip = std::size_t(peak->index);
        R["ReH"] = determined(peak->y, "Pa", "sustained-peak");
        R["ReH"].point = *peak;
        std::size_t from = ip + 1;
        if (m.lowerYieldIgnoresTransient) {
            const auto t = firstLocalMinimum(e, s, ip + 1, iFm);
            if (t)
                from = std::size_t(t->index) + 1;
        }
        if (from <= iFm) {
            const CurvePoint lo = minimum(e, s, from, iFm);
            R["ReL"] = determined(lo.y, "Pa", "minimum");
            R["ReL"].point = lo;
        } else {
            R["ReL"] = notDetermined("Pa", "no samples between the upper yield point and the maximum");
        }
    } else {
        const std::string why = m.yieldPoint ? "no yield point: no sustained fall in stress before the maximum"
                                             : "yield points were not asked for";
        R["ReH"] = notDetermined("Pa", why);
        R["ReL"] = notDetermined("Pa", why);
    }

    // --- Proof strengths --------------------------------------------------------------
    for (double o : m.proofOffsets) {
        const auto p = offsetIntersection(e, s, E, el->x0, o, el->fit.first, proofEnd);
        if (p) {
            R[proofId(o)] = determined(p->y, "Pa", "offset-intersection");
            R[proofId(o)].point = *p;
        } else {
            R[proofId(o)] = notDetermined("Pa", p.reason);
        }
    }

    // --- Elongations ------------------------------------------------------------------
    if (iFm <= lastE) {
        const double agt = e[iFm] - x0;
        R["Agt"] = determined(agt, "1", "value-at");
        R["Agt"].point = R["Rm"].point;
        R["Ag"] = determined(agt - s[iFm] / E, "1", "value-at");
        R["Ag"].point = R["Rm"].point;
    } else {
        R["Agt"] = notDetermined("1", extensionEnds + " (maximum force)");
        R["Ag"] = notDetermined("1", extensionEnds + " (maximum force)");
    }

    const Fracture f = fracture(e, in.force, m.fractureRatio, m.fractureMinDrop, in.force[iFm], iFm);
    if (f.atEnd)
        out.flags.push_back("no fracture drop found; the last sample is taken as fracture");
    const auto iF = std::size_t(f.point.index);
    R["Rb"] = determined(s[iF], "Pa", "break");
    R["Rb"].point = CurvePoint{e[iF], s[iF], double(iF)};
    if (iF <= lastE) {
        R["At"] = determined(e[iF] - x0, "1", "break");
        R["At"].point = CurvePoint{e[iF], s[iF], double(iF)};
        R["A"] = determined(e[iF] - x0 - s[iF] / E, "1", "break");
        R["A"].point = R["At"].point;
    } else {
        R["At"] = notDetermined("1", extensionEnds + " (fracture)");
        R["A"] = notDetermined("1", extensionEnds + " (fracture)");
    }

    return out;
}

} // namespace tf::analysis
