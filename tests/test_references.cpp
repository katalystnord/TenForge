// Reference curves: a digitised curve from PlotTracer, drawn beside the
// test's own for comparison (ROADMAP Phase 4).
//
// The fixture was written by PlotTracer's own JSON exporter
// (tools/fixtures/make_plottracer_export.test.ts), so the importer is
// tested on the real format: series of points keyed by axis label, nulls
// for unmeasured points, an error-bar series carrying a relation.

#include "core/References.h"
#include "core/Session.h"
#include "check.h"

#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>

#include <cmath>
#include <cstdio>

using namespace tf::core;

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

namespace {

QString fixture(const char *rel) { return QString(TF_FIXTURES) + "/" + rel; }
const char *kSeries = "aluminium-like curve, made up for the fixture";

Session tension()
{
    Session s;
    s.profile = "iso-6892-1-2019";
    s.openData(fixture("synthetic/ro-continuous.csv"));
    s.specimen.shape = Specimen::Shape::GivenArea;
    s.specimen.area = 78.54e-6;
    s.specimen.gaugeLength = 50e-3;
    return s;
}

} // namespace

TF_CASE(a_plottracer_export_lists_its_curves_and_columns)
{
    QString err;
    const auto f = readPlotTracer(fixture("plottracer/curve-export.json"), &err);
    TF_REQUIRE(f.has_value());
    TF_REQUIRE(f->series.size() == 2);
    const PlotTracerSeries &s = f->series[0];
    TF_CHECK(s.name == kSeries);
    TF_CHECK(s.relation.isEmpty());
    TF_CHECK(f->series[1].relation == "upper");  // an error bar, not a curve
    TF_CHECK(s.columns.contains("Strain (%)") && s.columns.contains("Stress (MPa)"));
    // Eleven points, one with no stress: the gap is not a zero.
    TF_CHECK(s.points == 11);
    TF_CHECK(f->sha256.size() == 64);
}

TF_CASE(the_strain_and_stress_columns_are_guessed_from_their_names)
{
    QString err;
    const auto f = readPlotTracer(fixture("plottracer/curve-export.json"), &err);
    TF_REQUIRE(f.has_value());
    const auto g = guessReferenceColumns(f->series[0]);
    TF_CHECK(g.strainColumn == "Strain (%)");
    TF_CHECK(g.stressColumn == "Stress (MPa)");
    TF_CHECK(g.strainUnit == "%");
    TF_CHECK(g.stressUnit == "MPa");
}

TF_CASE(a_reference_curve_is_read_in_si_without_its_gaps)
{
    Session s = tension();
    TF_REQUIRE(s.addReferenceCurve(fixture("plottracer/curve-export.json"), kSeries, "Strain (%)",
                                   "%", "Stress (MPa)", "MPa")
                   .isEmpty());
    TF_REQUIRE(s.references.size() == 1);
    const ReferenceCurve &r = s.references[0];
    TF_CHECK(r.strain.size() == 10);  // the point with no stress is left out
    TF_NEAR(r.strain[6], 0.04, 1e-15);
    TF_NEAR(r.stress[6], 315e6, 1e-6);
    TF_NEAR(r.strain.back(), 0.10, 1e-15);
    TF_NEAR(r.stress.back(), 326e6, 1e-6);
    TF_REQUIRE(s.analyse());
    // The results say what was drawn beside the curve, and from where.
    const QJsonArray refs = s.analysis()->doc.value("reference_curves").toArray();
    TF_REQUIRE(refs.size() == 1);
    TF_CHECK(refs[0].toObject().value("series").toString() == kSeries);
}

TF_CASE(what_cannot_be_a_reference_is_refused_with_why)
{
    Session s = tension();
    const QString file = fixture("plottracer/curve-export.json");
    TF_CHECK(s.addReferenceCurve(file, "no such series", "Strain (%)", "%", "Stress (MPa)", "MPa")
                 .contains("no such series"));
    TF_CHECK(s.addReferenceCurve(file, "upper bound", "Strain (%)", "%", "Stress (MPa)", "MPa")
                 .contains("error bar"));
    TF_CHECK(s.addReferenceCurve(file, kSeries, "Strain (%)", "furlong", "Stress (MPa)", "MPa")
                 .contains("furlong"));
    TF_CHECK(!s.addReferenceCurve(fixture("profiles/user-polymer.json"), kSeries, "x", "%", "y", "MPa")
                  .isEmpty());
    TF_CHECK(s.references.empty());
}

TF_CASE(a_reference_curve_comes_back_with_the_record_byte_for_byte)
{
    Session s = tension();
    TF_REQUIRE(s.addReferenceCurve(fixture("plottracer/curve-export.json"), kSeries, "Strain (%)",
                                   "%", "Stress (MPa)", "MPa")
                   .isEmpty());
    TF_REQUIRE(s.analyse());
    QTemporaryDir dir;
    TF_REQUIRE(s.saveRecord(dir.filePath("r")).isEmpty());

    Session r;
    TF_REQUIRE(r.openRecord(dir.filePath("r")).isEmpty());
    TF_CHECK(r.notices().isEmpty());
    if (!r.notices().isEmpty())
        std::printf("  notices: %s\n", qPrintable(r.notices().join(" | ")));
    TF_REQUIRE(r.references.size() == 1);
    TF_CHECK(r.references[0].sha256 == s.references[0].sha256);
    TF_CHECK(r.references[0].stress == s.references[0].stress);
    TF_CHECK(r.references[0].file.startsWith(dir.filePath("r")));
}

int main() { return tf::test::run(); }
