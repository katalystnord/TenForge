// The bending and compliance models, and the curve model for compression
// and bending, against hand-worked numbers and the committed synthetic
// curves.
//
// The curves and their answers come from tools/fixtures/make_modes.py,
// closed-form, never from TenForge. Each file is in machine units with
// dimensions that are not round, so a unit or formula slip cannot pass.

#include "analysis/Curve.h"
#include "analysis/Models.h"
#include "check.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace tf::analysis;

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

namespace {

double jsonNumber(const std::string &text, const std::string &key)
{
    const std::string needle = "\"" + key + "\":";
    const auto at = text.find(needle);
    if (at == std::string::npos) {
        std::printf("  fixture has no key %s\n", key.c_str());
        return std::nan("");
    }
    return std::stod(text.substr(at + needle.size()));
}

// A fixture as the machine wrote it: displacement in m, force in N. The
// file's header says whether force is in N or kN.
struct Fixture {
    std::vector<double> displacement, force;
    std::string meta;
    double answer(const std::string &key) const { return jsonNumber(meta, key); }
    double mm(const std::string &key) const { return jsonNumber(meta, key) * 1e-3; }
};

Fixture load(const std::string &name)
{
    const std::string dir = std::string(TF_FIXTURES) + "/modes/";
    Fixture fx;
    std::ifstream j(dir + name + ".json");
    std::stringstream ss;
    ss << j.rdbuf();
    fx.meta = ss.str();

    std::ifstream c(dir + name + ".csv");
    std::string header, line;
    std::getline(c, header);
    const double forceScale = header.find("force_kN") != std::string::npos ? 1e3 : 1.0;
    while (std::getline(c, line)) {
        double t, d, f;
        if (std::sscanf(line.c_str(), "%lf,%lf,%lf", &t, &d, &f) == 3) {
            fx.displacement.push_back(d * 1e-3);
            fx.force.push_back(f * forceScale);
        }
    }
    return fx;
}

CurveInput bend(const Fixture &fx, const FlexureGeometry &g)
{
    CurveInput in;
    const double ks = *flexureStressFactor(g), ke = *flexureStrainFactor(g);
    for (std::size_t i = 0; i < fx.force.size(); ++i) {
        in.force.push_back(fx.force[i]);
        in.stress.push_back(fx.force[i] * ks);
        in.strain.push_back(fx.displacement[i] * ke);
    }
    return in;
}

// Axial compression. The fixture may be recorded negative; tf_core turns
// the signs, so here the test does it to hand the engine what tf_core
// would.
CurveInput squeeze(const Fixture &fx, double area, double length, double sign)
{
    CurveInput in;
    for (std::size_t i = 0; i < fx.force.size(); ++i) {
        in.force.push_back(sign * fx.force[i]);
        in.stress.push_back(sign * fx.force[i] / area);
        in.strain.push_back(sign * fx.displacement[i] / length);
    }
    return in;
}

const Determination &get(const AnalysisResult &r, const std::string &id)
{
    static const Determination missing;
    const auto it = r.results.find(id);
    if (it == r.results.end()) {
        std::printf("  no result %s\n", id.c_str());
        return missing;
    }
    if (!it->second.determined)
        std::printf("  %s not determined: %s\n", id.c_str(), it->second.reason.c_str());
    return it->second;
}

CurveMethod flexureMethod()
{
    CurveMethod m;
    m.requests = {{"Ef", CurveQuantity::Modulus},
                  {"sigma_fM", CurveQuantity::MaxStress},
                  {"epsilon_fM", CurveQuantity::StrainAtMax},
                  {"Fm", CurveQuantity::MaxForce},
                  {"sigma_fB", CurveQuantity::BreakStress},
                  {"epsilon_fB", CurveQuantity::BreakStrain},
                  {"sigma_fC", CurveQuantity::StressAtStrain, 0.035}};
    return m;
}

} // namespace

// --- The bending model, by hand ---------------------------------------------

TF_CASE(three_point_bending_stress_and_strain_by_hand)
{
    // b = 10 mm, h = 4 mm, L = 64 mm. sigma = 3 F L / (2 b h^2):
    // 3 * 0.064 / (2 * 0.01 * 1.6e-5) = 0.192 / 3.2e-7 = 600000 Pa per N.
    // strain = 6 h s / L^2 = 6 * 0.004 / 0.004096 = 5.859375 per m.
    FlexureGeometry g;
    g.width = 0.010;
    g.thickness = 0.004;
    g.span = 0.064;
    TF_REQUIRE(flexureStressFactor(g).value.has_value());
    TF_NEAR(*flexureStressFactor(g), 600000.0, 1e-6);
    TF_NEAR(*flexureStrainFactor(g), 5.859375, 1e-12);
    // Three-point: where the deflection is taken does not matter.
    g.deflectionAt = FlexureGeometry::DeflectionAt::Midspan;
    TF_NEAR(*flexureStrainFactor(g), 5.859375, 1e-12);
}

TF_CASE(four_point_bending_strain_depends_on_where_deflection_is_measured)
{
    // L = 90 mm, loading span 30 mm (a = 30 mm), h = 2.3 mm, b = 15 mm.
    // sigma = 3 F a / (b h^2) = 0.09 / (0.015 * 5.29e-6) = 1134215.5 Pa per N.
    // Mid-span: 12 h / (3 L^2 - 4 a^2) = 0.0276 / (0.0243 - 0.0036) = 4/3.
    // Loading points: 3 h / (a (3 L - 4 a)) = 0.0069 / (0.03 * 0.15) = 23/15.
    FlexureGeometry g;
    g.width = 0.015;
    g.thickness = 0.0023;
    g.span = 0.090;
    g.loadingSpan = 0.030;
    TF_NEAR(*flexureStressFactor(g), 0.09 / (0.015 * 5.29e-6), 1e-6);
    g.deflectionAt = FlexureGeometry::DeflectionAt::Midspan;
    TF_NEAR(*flexureStrainFactor(g), 4.0 / 3.0, 1e-12);
    g.deflectionAt = FlexureGeometry::DeflectionAt::LoadingPoints;
    TF_NEAR(*flexureStrainFactor(g), 23.0 / 15.0, 1e-12);
}

TF_CASE(a_geometry_that_is_not_a_beam_is_refused_with_why)
{
    FlexureGeometry g;
    g.width = 0.010;
    g.thickness = 0.004;
    g.span = 0.064;
    g.loadingSpan = 0.064;  // loading points on the supports
    TF_CHECK(!flexureStressFactor(g));
    TF_CHECK(!flexureStressFactor(g).reason.empty());
    g.loadingSpan = 0;
    g.thickness = 0;
    TF_CHECK(!flexureStrainFactor(g));
    TF_CHECK(!flexureStrainFactor(g).reason.empty());
}

// --- Machine compliance --------------------------------------------------------

TF_CASE(compliance_takes_the_machine_out_and_never_extrapolates)
{
    // Machine: 0.1 mm at 1 kN, 0.3 mm at 2 kN, linear between.
    const std::vector<double> tf{0, 1000, 2000}, td{0, 1e-4, 3e-4};
    const std::vector<double> f{500, 1500}, d{1e-3, 2e-3};
    const auto c = complianceCorrected(f, d, tf, td);
    TF_REQUIRE(c.value.has_value());
    TF_NEAR((*c)[0], 1e-3 - 0.5e-4, 1e-15);
    TF_NEAR((*c)[1], 2e-3 - 2e-4, 1e-15);

    const std::vector<double> beyond{500, 2500}, d2{1e-3, 3e-3};
    const auto out = complianceCorrected(beyond, d2, tf, td);
    TF_CHECK(!out);
    TF_CHECK(out.reason.find("2500") != std::string::npos);

    const std::vector<double> unsorted{0, 2000, 1000};
    TF_CHECK(!complianceCorrected(f, d, unsorted, td));
}

// --- The curve model on the synthetic curves ------------------------------------

TF_CASE(three_point_beam_gives_its_planted_results)
{
    const Fixture fx = load("beam-3pt");
    FlexureGeometry g;
    g.width = fx.mm("width_mm");
    g.thickness = fx.mm("thickness_mm");
    g.span = fx.mm("span_mm");
    const AnalysisResult r = analyseCurve(bend(fx, g), flexureMethod());
    TF_NEAR(get(r, "Ef").value, fx.answer("Ef_Pa"), 1e-6 * fx.answer("Ef_Pa"));
    TF_NEAR(get(r, "sigma_fM").value, fx.answer("sigma_fM_Pa"), 1e-6 * fx.answer("sigma_fM_Pa"));
    TF_NEAR(get(r, "epsilon_fM").value, fx.answer("epsilon_fM_1"), 1e-8);
    TF_NEAR(get(r, "Fm").value, fx.answer("Fm_N"), 1e-6 * fx.answer("Fm_N"));
    TF_NEAR(get(r, "sigma_fB").value, fx.answer("sigma_fB_Pa"), 1e-6 * fx.answer("sigma_fB_Pa"));
    TF_NEAR(get(r, "epsilon_fB").value, fx.answer("epsilon_fB_1"), 1e-8);
    TF_NEAR(get(r, "sigma_fC").value, fx.answer("sigma_fC_Pa"), 1e-6 * fx.answer("sigma_fC_Pa"));
    // The modulus says how it was found: a regression over the window.
    TF_REQUIRE(get(r, "Ef").fit.has_value());
    TF_CHECK(get(r, "Ef").fit->r2 > 0.999999);
    TF_CHECK(get(r, "Ef").windowAxis == "strain");
}

TF_CASE(four_point_beam_without_a_break_refuses_the_break_results)
{
    const Fixture fx = load("beam-4pt");
    FlexureGeometry g;
    g.width = fx.mm("width_mm");
    g.thickness = fx.mm("thickness_mm");
    g.span = fx.mm("span_mm");
    g.loadingSpan = fx.mm("loading_span_mm");
    g.deflectionAt = FlexureGeometry::DeflectionAt::LoadingPoints;
    const AnalysisResult r = analyseCurve(bend(fx, g), flexureMethod());
    TF_NEAR(get(r, "Ef").value, fx.answer("Ef_Pa"), 1e-6 * fx.answer("Ef_Pa"));
    TF_NEAR(get(r, "sigma_fM").value, fx.answer("sigma_fM_Pa"), 1e-6 * fx.answer("sigma_fM_Pa"));
    TF_NEAR(get(r, "epsilon_fM").value, fx.answer("epsilon_fM_1"), 1e-8);
    TF_NEAR(get(r, "Fm").value, fx.answer("Fm_N"), 1e-6 * fx.answer("Fm_N"));
    for (const char *id : {"sigma_fB", "epsilon_fB"}) {
        const auto it = r.results.find(id);
        TF_REQUIRE(it != r.results.end());
        TF_CHECK(!it->second.determined);
        TF_CHECK(it->second.reason.find("break") != std::string::npos);
    }
    // The curve stops at 2 % strain, before 3.5 %: refused, not guessed.
    const auto c = r.results.find("sigma_fC");
    TF_REQUIRE(c != r.results.end());
    TF_CHECK(!c->second.determined);
    TF_CHECK(!c->second.reason.empty());
}

TF_CASE(the_same_beam_read_at_mid_span_gives_a_different_modulus)
{
    // Guards the model's choice of deflection position: the fixture was
    // written with deflection at the loading points, so reading it as
    // mid-span must NOT reproduce its answer.
    const Fixture fx = load("beam-4pt");
    FlexureGeometry g;
    g.width = fx.mm("width_mm");
    g.thickness = fx.mm("thickness_mm");
    g.span = fx.mm("span_mm");
    g.loadingSpan = fx.mm("loading_span_mm");
    g.deflectionAt = FlexureGeometry::DeflectionAt::Midspan;
    const AnalysisResult r = analyseCurve(bend(fx, g), flexureMethod());
    TF_CHECK(std::fabs(get(r, "Ef").value - fx.answer("Ef_Pa")) > 0.05 * fx.answer("Ef_Pa"));
}

TF_CASE(foam_counts_strain_from_the_steepest_line_and_finds_its_strength)
{
    const Fixture fx = load("foam");
    const double area = fx.mm("width_mm") * fx.mm("thickness_mm");
    CurveMethod m;
    m.zero = CurveMethod::Zero::ElasticLine;
    m.modulus = CurveMethod::Modulus::MaxSlope;
    m.maxSlopeWidth = 0.005;
    m.requests = {{"E", CurveQuantity::Modulus},
                  {"sigma_m", CurveQuantity::PeakStress, 0.10},
                  {"epsilon_m", CurveQuantity::StrainAtPeak, 0.10},
                  {"sigma_10", CurveQuantity::StressAtStrain, 0.10}};
    const AnalysisResult r = analyseCurve(squeeze(fx, area, fx.mm("height_mm"), 1.0), m);
    TF_NEAR(get(r, "E").value, fx.answer("E_Pa"), 1e-6 * fx.answer("E_Pa"));
    TF_NEAR(r.strainOrigin, fx.answer("zero_point_1"), 1e-8);
    TF_NEAR(get(r, "sigma_m").value, fx.answer("sigma_m_Pa"), 1e-6 * fx.answer("sigma_m_Pa"));
    TF_NEAR(get(r, "epsilon_m").value, fx.answer("epsilon_m_1"), 1e-8);
    TF_NEAR(get(r, "sigma_10").value, fx.answer("sigma_10_Pa"), 1e-6 * fx.answer("sigma_10_Pa"));
}

TF_CASE(a_curve_still_rising_at_ten_percent_has_no_strength_before_it)
{
    // Straight, then flattening, still rising through 10 % and beyond.
    CurveInput in;
    for (int k = 0; k <= 300; ++k) {
        const double e = 0.001 * k;
        const double s = e < 0.02 ? 1e7 * e : 2e5 + 1e6 * (e - 0.02);
        in.strain.push_back(e);
        in.stress.push_back(s);
        in.force.push_back(s * 0.0025);
    }
    CurveMethod m;
    m.requests = {{"sigma_m", CurveQuantity::PeakStress, 0.10},
                  {"epsilon_m", CurveQuantity::StrainAtPeak, 0.10},
                  {"sigma_10", CurveQuantity::StressAtStrain, 0.10}};
    const AnalysisResult r = analyseCurve(in, m);
    for (const char *id : {"sigma_m", "epsilon_m"}) {
        const auto it = r.results.find(id);
        TF_REQUIRE(it != r.results.end());
        TF_CHECK(!it->second.determined);
        TF_CHECK(it->second.reason.find("10 %") != std::string::npos);
    }
    // 2e5 + 1e6 * 0.08 = 280000 Pa, on a straight stretch.
    TF_NEAR(get(r, "sigma_10").value, 280000.0, 1e-6);
}

TF_CASE(compression_finds_yield_strength_and_break)
{
    const Fixture fx = load("compression");
    const double area = fx.mm("width_mm") * fx.mm("thickness_mm");
    CurveMethod m;
    m.requests = {{"Ec", CurveQuantity::Modulus},
                  {"sigma_y", CurveQuantity::YieldStress},
                  {"epsilon_y", CurveQuantity::YieldStrain},
                  {"sigma_M", CurveQuantity::MaxStress},
                  {"epsilon_M", CurveQuantity::StrainAtMax},
                  {"sigma_B", CurveQuantity::BreakStress},
                  {"epsilon_B", CurveQuantity::BreakStrain}};
    const AnalysisResult r = analyseCurve(squeeze(fx, area, fx.mm("length_mm"), -1.0), m);
    for (const char *k : {"Ec", "sigma_y", "sigma_M", "sigma_B"})
        TF_NEAR(get(r, k).value, fx.answer(std::string(k) + "_Pa"),
                1e-6 * fx.answer(std::string(k) + "_Pa"));
    for (const char *k : {"epsilon_y", "epsilon_M", "epsilon_B"})
        TF_NEAR(get(r, k).value, fx.answer(std::string(k) + "_1"), 1e-8);
}

TF_CASE(a_curve_without_a_sustained_fall_has_no_yield)
{
    // The four-point beam falls 2.4 % after its maximum for only 0.2 %
    // strain: no yield under a 0.5 % span rule.
    const Fixture fx = load("beam-4pt");
    FlexureGeometry g;
    g.width = fx.mm("width_mm");
    g.thickness = fx.mm("thickness_mm");
    g.span = fx.mm("span_mm");
    g.loadingSpan = fx.mm("loading_span_mm");
    CurveMethod m;
    m.requests = {{"sigma_y", CurveQuantity::YieldStress}, {"epsilon_y", CurveQuantity::YieldStrain}};
    const AnalysisResult r = analyseCurve(bend(fx, g), m);
    for (const char *id : {"sigma_y", "epsilon_y"}) {
        const auto it = r.results.find(id);
        TF_REQUIRE(it != r.results.end());
        TF_CHECK(!it->second.determined);
        TF_CHECK(!it->second.reason.empty());
    }
}

TF_CASE(too_few_samples_refuse_every_result)
{
    CurveInput in;
    for (int k = 0; k < 5; ++k) {
        in.force.push_back(k);
        in.stress.push_back(k * 1e6);
        in.strain.push_back(k * 1e-3);
    }
    const AnalysisResult r = analyseCurve(in, flexureMethod());
    TF_CHECK(r.results.size() == flexureMethod().requests.size());
    for (const auto &[id, d] : r.results) {
        TF_CHECK(!d.determined);
        TF_CHECK(!d.reason.empty());
    }
}

int main() { return tf::test::run(); }
