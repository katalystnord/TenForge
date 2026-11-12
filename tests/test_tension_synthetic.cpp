// The tension model against the committed synthetic curves.
//
// The curves and their answers come from tools/fixtures/make_synthetic.py,
// closed-form, never from TenForge. Each file is in machine units (kN, mm)
// with a cross-section and gauge length that are not 1, so a unit slip
// cannot pass.

#include "analysis/Tension.h"
#include "check.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

using namespace tf::analysis;

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

namespace {

// The fixture JSON is flat enough to read numbers by key without a parser:
// "key": number. Only what the tests need.
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

struct Fixture {
    TensionInput input;
    std::string meta;
    double answer(const std::string &key) const { return jsonNumber(meta, key); }
};

Fixture load(const std::string &name)
{
    const std::string dir = std::string(TF_FIXTURES) + "/synthetic/";
    Fixture fx;
    std::ifstream j(dir + name + ".json");
    std::stringstream ss;
    ss << j.rdbuf();
    fx.meta = ss.str();
    fx.input.area = jsonNumber(fx.meta, "area_mm2") * 1e-6;
    fx.input.gaugeLength = jsonNumber(fx.meta, "gauge_length_mm") * 1e-3;

    std::ifstream c(dir + name + ".csv");
    std::string line;
    std::getline(c, line);  // header: time_s,extension_mm,force_kN
    while (std::getline(c, line)) {
        double t, ext, f;
        if (std::sscanf(line.c_str(), "%lf,%lf,%lf", &t, &ext, &f) == 3) {
            fx.input.extension.push_back(ext * 1e-3);
            fx.input.force.push_back(f * 1e3);
        }
    }
    return fx;
}

const Determination &get(const TensionResult &r, const std::string &id)
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

} // namespace

TF_CASE(continuous_curve_gives_its_constructed_proof_strength_and_modulus)
{
    const Fixture fx = load("ro-continuous");
    const TensionResult r = analyseTension(fx.input, TensionMethod{});
    TF_CHECK(get(r, "E").determined);
    TF_NEAR(get(r, "E").value, fx.answer("E_Pa"), 0.001 * fx.answer("E_Pa"));
    // 0.05 MPa: far inside the 2 MPa of EN 10002-1 A.5, and wide enough for
    // the fitted (not given) modulus.
    TF_NEAR(get(r, "Rp0.2").value, fx.answer("Rp0.2_Pa"), 0.05e6);
    TF_NEAR(get(r, "Rm").value, fx.answer("Rm_Pa"), 1e3);
    TF_NEAR(get(r, "Fm").value, fx.answer("Fm_N"), 0.01);
    TF_NEAR(get(r, "Agt").value, fx.answer("Agt_1"), 1e-5);
    TF_NEAR(get(r, "Ag").value, fx.answer("Ag_1"), 1e-5);
    TF_NEAR(get(r, "At").value, fx.answer("At_1"), 1e-5);
    TF_NEAR(get(r, "A").value, fx.answer("A_1"), 1e-5);
}

TF_CASE(continuous_curve_has_no_yield_point)
{
    const Fixture fx = load("ro-continuous");
    const TensionResult r = analyseTension(fx.input, TensionMethod{});
    const auto it = r.results.find("ReH");
    TF_CHECK(it != r.results.end());
    TF_CHECK(it != r.results.end() && !it->second.determined);
    TF_CHECK(it != r.results.end() && !it->second.reason.empty());
}

TF_CASE(discontinuous_curve_gives_its_planted_yield_points)
{
    const Fixture fx = load("discontinuous");
    const TensionResult r = analyseTension(fx.input, TensionMethod{});
    TF_NEAR(get(r, "ReH").value, fx.answer("ReH_Pa"), 1e3);
    TF_NEAR(get(r, "ReL").value, fx.answer("ReL_Pa"), 1e3);
    TF_NEAR(get(r, "Rm").value, fx.answer("Rm_Pa"), 1e3);
    TF_NEAR(get(r, "E").value, fx.answer("E_Pa"), 0.001 * fx.answer("E_Pa"));
    TF_NEAR(get(r, "Agt").value, fx.answer("Agt_1"), 1e-5);
    TF_NEAR(get(r, "Ag").value, fx.answer("Ag_1"), 1e-5);
    TF_NEAR(get(r, "At").value, fx.answer("At_1"), 1e-5);
    TF_NEAR(get(r, "A").value, fx.answer("A_1"), 1e-5);
}

TF_CASE(lower_yield_follows_the_transient_rule_the_method_names)
{
    const Fixture fx = load("transient-lowest");
    TensionMethod m;
    m.lowerYieldIgnoresTransient = false;
    TF_NEAR(get(analyseTension(fx.input, m), "ReL").value,
            fx.answer("ReL_including_transient_Pa"), 1e3);
    m.lowerYieldIgnoresTransient = true;
    TF_NEAR(get(analyseTension(fx.input, m), "ReL").value,
            fx.answer("ReL_ignoring_transient_Pa"), 1e3);
}

TF_CASE(every_result_says_which_primitive_made_it)
{
    const Fixture fx = load("discontinuous");
    const TensionResult r = analyseTension(fx.input, TensionMethod{});
    for (const auto &[id, d] : r.results) {
        if (d.determined && d.primitive.empty())
            tf::test::fail(__FILE__, __LINE__, id + " has no primitive");
    }
    TF_CHECK(get(r, "E").fit.has_value());
    TF_CHECK(get(r, "E").window.has_value());
}

TF_CASE(missing_geometry_is_refused_not_guessed)
{
    Fixture fx = load("ro-continuous");
    fx.input.area = 0;
    const TensionResult r = analyseTension(fx.input, TensionMethod{});
    TF_CHECK(!get(r, "Rm").determined);
    TF_CHECK(r.results.count("Rm") && r.results.at("Rm").reason.find("area") != std::string::npos);
}

TF_CASE(extensometer_removed_early_keeps_force_results_and_refuses_strain_ones)
{
    // As in BAM's S355 tests: the extensometer comes off at 3 % strain, the
    // force channel runs on to fracture. Stress results stay; strains after
    // the removal are not determined, and say why.
    Fixture fx = load("discontinuous");
    for (double &x : fx.input.extension)
        if (x / fx.input.gaugeLength > 0.03)
            x = std::nan("");
    const TensionResult r = analyseTension(fx.input, TensionMethod{});
    TF_NEAR(get(r, "ReH").value, fx.answer("ReH_Pa"), 1e3);
    TF_NEAR(get(r, "ReL").value, fx.answer("ReL_Pa"), 1e3);
    TF_NEAR(get(r, "Rm").value, fx.answer("Rm_Pa"), 1e3);
    TF_NEAR(get(r, "E").value, fx.answer("E_Pa"), 0.001 * fx.answer("E_Pa"));
    for (const char *id : {"Agt", "Ag", "At", "A"}) {
        const auto &d = r.results.at(id);
        if (d.determined || d.reason.find("extension") == std::string::npos)
            tf::test::fail(__FILE__, __LINE__, std::string(id) + " should be refused for missing extension");
    }
}

TF_CASE(polymer_curve_gives_its_modulus_from_a_strain_window_and_its_break)
{
    // ISO 527 style: the modulus between two strains, strain counted from
    // the recorded zero, maximum and break read off the curve.
    const Fixture fx = load("polymer");
    TensionMethod m;
    m.modulusAxis = TensionMethod::Axis::Strain;
    m.modulusFrom = 0.0005;
    m.modulusTo = 0.0025;
    m.strainFromElasticLine = false;
    m.yieldPoint = false;
    m.proofOffsets.clear();
    const TensionResult r = analyseTension(fx.input, m);
    TF_NEAR(get(r, "E").value, fx.answer("Et_Pa"), 1e-6 * fx.answer("Et_Pa"));
    TF_CHECK(get(r, "E").window.has_value());
    TF_NEAR(get(r, "Rm").value, fx.answer("sigma_m_Pa"), 1e3);
    TF_NEAR(get(r, "Agt").value, fx.answer("epsilon_m_1"), 1e-6);
    TF_NEAR(get(r, "At").value, fx.answer("epsilon_b_1"), 1e-6);
    TF_NEAR(get(r, "Rb").value, fx.answer("sigma_b_Pa"), 1e3);
    TF_CHECK(r.results.count("Rp0.2") == 0);
}

int main() { return tf::test::run(); }
