// The analysis session: what the window shows and does, without the window.
//
// The session holds the file, the column mapping, the specimen and the
// procedure, says in words what to do next, and runs the analysis once
// nothing is missing. The window is a thin view of it, so these cases are
// where the behaviour is pinned; the walkthrough only drives the screen.
//
// Expected answers are those of the committed synthetic fixtures
// (tests/fixtures/synthetic/*.json: Rp0.2 300 MPa and Rm 500 MPa exactly
// for ro-continuous with S0 78.54 mm2).

#include "core/Session.h"
#include "check.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>

#include <cmath>
#include <QTemporaryDir>
#include <cstdio>
#include <QJsonDocument>

using namespace tf::core;

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

namespace {

QString fixture(const char *rel) { return QString(TF_FIXTURES) + "/" + rel; }

QString reported(const Session &s, const char *id)
{
    if (!s.analysis())
        return "no analysis";
    for (const QJsonValue &v : s.analysis()->doc.value("results").toArray())
        if (v.toObject().value("id").toString() == id)
            return v.toObject().value("reported").toObject().value("value").toString();
    return "no " + QString(id);
}

// ro-continuous, mapped and measured as its fixture JSON says.
void readySynthetic(Session &s)
{
    s.openData(fixture("synthetic/ro-continuous.csv"));
    s.specimen.shape = Specimen::Shape::GivenArea;
    s.specimen.area = 78.54e-6;
    s.specimen.gaugeLength = 50e-3;
    s.profile = "iso-6892-1-2019";
}

} // namespace

TF_CASE(a_new_session_asks_for_the_procedure_then_a_file)
{
    // The procedure comes first: its mode decides what else is asked.
    Session s;
    Step n = s.next();
    TF_CHECK(!n.ready);
    TF_CHECK(n.field == "profile");
    TF_CHECK(n.text.contains("bending"));
    s.profile = "iso-6892-1-2019";
    n = s.next();
    TF_CHECK(n.field == "data");
    TF_CHECK(n.text.contains("Open"));
}

TF_CASE(columns_units_and_geometry_are_taken_from_the_file_and_said_so)
{
    Session s;
    TF_REQUIRE(s.openData(fixture("import/quoted-preamble.txt")).isEmpty());
    TF_CHECK(s.mapping.forceColumn == "force");
    TF_CHECK(s.mapping.extensionColumn == "extensometer");
    TF_CHECK(s.mapping.forceUnit == "kN");
    TF_CHECK(s.mapping.extensionUnit == "mm");
    TF_CHECK(s.specimen.shape == Specimen::Shape::GivenArea);
    TF_NEAR(s.specimen.area.value_or(NAN), 78.5e-6, 1e-15);
    TF_NEAR(s.specimen.gaugeLength.value_or(NAN), 50e-3, 1e-15);
    TF_CHECK(!s.notes().filter("So").isEmpty());
    TF_CHECK(!s.notes().filter("Le").isEmpty());
    // Everything but the procedure is known.
    TF_CHECK(s.next().field == "profile");
}

TF_CASE(the_next_step_names_the_missing_dimension_then_the_gauge_length)
{
    Session s;
    TF_REQUIRE(s.openData(fixture("synthetic/ro-continuous.csv")).isEmpty());
    s.profile = "iso-6892-1-2019";
    s.specimen.shape = Specimen::Shape::Rectangular;
    s.specimen.width = 20e-3;
    Step n = s.next();
    TF_CHECK(n.field == "specimen");
    TF_CHECK(n.text.contains("thickness"));
    s.specimen.thickness = 4e-3;
    n = s.next();
    TF_CHECK(n.field == "gaugeLength");
    s.specimen.gaugeLength = 50e-3;
    TF_CHECK(s.next().ready);
}

TF_CASE(two_candidate_force_columns_ask_the_user_to_choose)
{
    Session s;
    s.profile = "iso-6892-1-2019";
    TF_REQUIRE(s.openData(fixture("import/two-forces.csv")).isEmpty());
    TF_CHECK(s.mapping.forceColumn.isEmpty());
    const Step n = s.next();
    TF_CHECK(n.field == "forceColumn");
    TF_CHECK(n.text.contains("force"));
    s.mapping.forceColumn = "load_cell_2_kN";
    TF_CHECK(s.next().field != "forceColumn");
}

TF_CASE(a_column_without_a_unit_asks_for_its_unit)
{
    Session s;
    s.profile = "iso-6892-1-2019";
    TF_REQUIRE(s.openData(fixture("import/no-units.csv")).isEmpty());
    TF_CHECK(s.mapping.forceColumn == "Force");
    TF_CHECK(s.mapping.forceUnit.isEmpty());
    TF_CHECK(s.next().field == "forceUnit");
    s.mapping.forceUnit = "kN";
    TF_CHECK(s.next().field == "extensionUnit");
}

TF_CASE(a_file_that_is_not_a_table_is_refused_and_nothing_changes)
{
    Session s;
    s.profile = "iso-6892-1-2019";
    TF_CHECK(!s.openData(fixture("profiles/user-polymer.json")).isEmpty());
    TF_CHECK(!s.table());
    TF_CHECK(s.next().field == "data");
}

TF_CASE(analysis_runs_only_when_ready_and_gives_the_fixtures_answers)
{
    Session s;
    TF_CHECK(!s.analyse());
    readySynthetic(s);
    TF_REQUIRE(s.next().ready);
    TF_REQUIRE(s.analyse());
    TF_CHECK(reported(s, "Rp0.2") == "300");
    TF_CHECK(reported(s, "Rm") == "500");
    TF_CHECK(s.analysis()->doc.value("conformity").toObject().value("status").toString()
             == "aligned");
}

TF_CASE(the_users_units_override_the_files)
{
    Session s;
    readySynthetic(s);
    s.mapping.forceUnit = "N";  // the file says kN: every force 1000 x smaller
    TF_REQUIRE(s.analyse());
    double rm = NAN;
    for (const QJsonValue &v : s.analysis()->doc.value("results").toArray())
        if (v.toObject().value("id").toString() == "Rm")
            rm = v.toObject().value("value").toDouble();
    TF_NEAR(rm, 0.5e6, 1e-3);
}

TF_CASE(a_saved_record_opens_to_the_same_results_with_nothing_to_report)
{
    QTemporaryDir tmp;
    Session s;
    readySynthetic(s);
    s.specimen.id = "RO-1";
    TF_REQUIRE(s.analyse());
    TF_REQUIRE(s.saveRecord(tmp.path() + "/RO-1").isEmpty());

    Session r;
    TF_REQUIRE(r.openRecord(tmp.path() + "/RO-1/manifest.tfrec").isEmpty());
    TF_CHECK(r.specimen.id == "RO-1");
    TF_CHECK(r.profile == "iso-6892-1-2019");
    TF_CHECK(r.mapping.forceColumn == "force_kN");
    TF_REQUIRE(r.analysis().has_value());  // analysed on opening
    TF_CHECK(reported(r, "Rp0.2") == "300");
    TF_CHECK(r.notices().isEmpty());
}

TF_CASE(a_record_whose_files_changed_says_which_on_opening)
{
    QTemporaryDir tmp;
    Session s;
    readySynthetic(s);
    TF_REQUIRE(s.analyse());
    TF_REQUIRE(s.saveRecord(tmp.path() + "/r").isEmpty());
    QFile f(tmp.path() + "/r/results/results.json");
    TF_REQUIRE(f.open(QIODevice::Append));
    f.write(" ");
    f.close();
    Session r;
    TF_REQUIRE(r.openRecord(tmp.path() + "/r").isEmpty());
    TF_CHECK(!r.notices().filter("results/results.json").isEmpty());
}

TF_CASE(stored_results_that_differ_from_a_fresh_analysis_are_named)
{
    QTemporaryDir tmp;
    Session s;
    readySynthetic(s);
    TF_REQUIRE(s.analyse());
    TF_REQUIRE(s.saveRecord(tmp.path() + "/r").isEmpty());
    // Edit the stored Rm, as a hand-edited file or an older version might.
    QFile f(tmp.path() + "/r/results/results.json");
    TF_REQUIRE(f.open(QIODevice::ReadOnly));
    QByteArray b = f.readAll();
    f.close();
    b.replace("\"value\": \"500\"", "\"value\": \"501\"");
    TF_REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(b);
    f.close();
    Session r;
    TF_REQUIRE(r.openRecord(tmp.path() + "/r").isEmpty());
    const QStringList rm = r.notices().filter("Rm");
    TF_REQUIRE(!rm.isEmpty());
    TF_CHECK(rm.front().contains("501") && rm.front().contains("500"));
}

TF_CASE(known_units_cover_the_importers_table)
{
    TF_CHECK(knownUnits("force").contains("kN"));
    TF_CHECK(knownUnits("force").contains("lbf"));
    TF_CHECK(knownUnits("length").contains("mm"));
    TF_CHECK(knownUnits("strain").contains("%"));
}

// --- Compression and bending (Phase 3) -------------------------------------------

namespace {

double fixtureNumber(const char *json, const QString &key)
{
    QFile f(fixture(json));
    if (!f.open(QIODevice::ReadOnly))
        return NAN;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.contains(key))
        return o.value(key).toDouble(NAN);
    return o.value("answers").toObject().value(key).toDouble(NAN);
}

double value(const Session &s, const char *id)
{
    if (!s.analysis())
        return NAN;
    for (const QJsonValue &v : s.analysis()->doc.value("results").toArray())
        if (v.toObject().value("id").toString() == id)
            return v.toObject().value("value").toDouble(NAN);
    return NAN;
}

QStringList reasonCodes(const Session &s)
{
    QStringList out;
    if (s.analysis())
        for (const QJsonValue &v :
             s.analysis()->doc.value("conformity").toObject().value("reasons").toArray())
            out << v.toObject().value("code").toString();
    return out;
}

} // namespace

TF_CASE(four_point_bending_asks_for_its_own_steps_and_gives_the_beams_answers)
{
    const char *meta = "modes/beam-4pt.json";
    Session s;
    s.profile = "iso-14125-1998-four-point";
    TF_REQUIRE(s.openData(fixture("modes/beam-4pt.csv")).isEmpty());
    TF_CHECK(s.mapping.extensionColumn == "deflection_mm");
    TF_CHECK(s.displacementWord() == "deflection");
    Step n = s.next();
    TF_CHECK(n.field == "specimen");
    s.specimen.width = fixtureNumber(meta, "width_mm") * 1e-3;
    s.specimen.thickness = fixtureNumber(meta, "thickness_mm") * 1e-3;
    n = s.next();
    TF_CHECK(n.field == "span");
    s.fixture.span = fixtureNumber(meta, "span_mm") * 1e-3;
    n = s.next();
    TF_CHECK(n.field == "loadingSpan");
    s.fixture.loadingSpan = fixtureNumber(meta, "loading_span_mm") * 1e-3;
    n = s.next();
    TF_CHECK(n.field == "deflectionAt");
    TF_CHECK(n.text.contains("loading points"));
    s.fixture.deflectionAt = Fixture::DeflectionAt::LoadingPoints;
    TF_REQUIRE(s.next().ready);
    TF_REQUIRE(s.analyse());
    TF_NEAR(value(s, "Ef"), fixtureNumber(meta, "Ef_Pa"), 1e-6 * fixtureNumber(meta, "Ef_Pa"));
    TF_NEAR(value(s, "sigma_fM"), fixtureNumber(meta, "sigma_fM_Pa"), 1e-6 * fixtureNumber(meta, "sigma_fM_Pa"));
    TF_CHECK(reported(s, "Ef") == "25.00");
    TF_CHECK(reported(s, "sigma_fM") == "420.0");
    // Loading span 27 of 81 mm is a third: the check is met, so it is no reason.
    TF_CHECK(!reasonCodes(s).contains("validity-not-met"));
    TF_CHECK(s.analysis()->doc.value("validity").toArray().first().toObject().value("status").toString() == "met");
    // The curve the window draws is the one analysed.
    TF_CHECK(s.analysis()->stress.size() == s.analysis()->imported.input.force.size());
}

TF_CASE(iso_178_takes_its_conventional_deflection_from_the_specimen)
{
    const char *meta = "modes/beam-3pt.json";
    Session s;
    s.profile = "iso-178-2019";
    TF_REQUIRE(s.openData(fixture("modes/beam-3pt.csv")).isEmpty());
    s.specimen.width = fixtureNumber(meta, "width_mm") * 1e-3;
    s.specimen.thickness = fixtureNumber(meta, "thickness_mm") * 1e-3;
    s.fixture.span = fixtureNumber(meta, "span_mm") * 1e-3;
    TF_REQUIRE(s.next().ready);
    TF_REQUIRE(s.analyse());
    // L/h = 15.6, so 1.5 h is at 3.69 % strain, not 3.5 %.
    TF_NEAR(value(s, "sigma_fC"), fixtureNumber(meta, "sigma_at_1.5h_Pa"),
            1e-6 * fixtureNumber(meta, "sigma_at_1.5h_Pa"));
    TF_NEAR(value(s, "sigma_fB"), fixtureNumber(meta, "sigma_fB_Pa"), 1e-6 * fixtureNumber(meta, "sigma_fB_Pa"));
    TF_CHECK(!reasonCodes(s).contains("validity-not-met"));

    // A span outside 16 h +/- h is reported, and is a reason the results
    // are not to the standard.
    s.fixture.span = 80e-3;
    TF_REQUIRE(s.analyse());
    TF_CHECK(reasonCodes(s).contains("validity-not-met"));
}

TF_CASE(compression_recorded_negative_is_analysed_positive_and_says_so)
{
    const char *meta = "modes/compression.json";
    Session s;
    s.profile = "iso-604-2002";
    TF_REQUIRE(s.openData(fixture("modes/compression.csv")).isEmpty());
    TF_CHECK(s.mapping.extensionColumn == "compression_mm");
    Step n = s.next();
    TF_CHECK(n.field == "specimen");
    s.specimen.width = fixtureNumber(meta, "width_mm") * 1e-3;
    s.specimen.thickness = fixtureNumber(meta, "thickness_mm") * 1e-3;
    n = s.next();
    TF_CHECK(n.field == "gaugeLength");
    TF_CHECK(n.text.contains("platens"));
    s.specimen.gaugeLength = fixtureNumber(meta, "length_mm") * 1e-3;
    TF_REQUIRE(s.analyse());
    for (const char *k : {"Ec", "sigma_y", "sigma_M"})
        TF_NEAR(value(s, k), fixtureNumber(meta, QString(k) + "_Pa"),
                1e-6 * fixtureNumber(meta, QString(k) + "_Pa"));
    TF_NEAR(value(s, "epsilon_y"), fixtureNumber(meta, "epsilon_y_1"), 1e-8);
    TF_CHECK(reported(s, "sigma_M") == "150.0");
    const QJsonArray notes =
        s.analysis()->doc.value("source").toObject().value("notes").toArray();
    bool said = false;
    for (const QJsonValue &v : notes)
        said = said || v.toString().contains("force is negative");
    TF_CHECK(said);
}

TF_CASE(foam_strength_and_modulus_are_reported_in_kilopascals)
{
    const char *meta = "modes/foam.json";
    Session s;
    s.profile = "iso-844-2021";
    TF_REQUIRE(s.openData(fixture("modes/foam.csv")).isEmpty());
    s.specimen.width = fixtureNumber(meta, "width_mm") * 1e-3;
    s.specimen.thickness = fixtureNumber(meta, "thickness_mm") * 1e-3;
    s.specimen.gaugeLength = fixtureNumber(meta, "height_mm") * 1e-3;
    TF_REQUIRE(s.analyse());
    TF_CHECK(reported(s, "E") == "11500");
    TF_CHECK(reported(s, "sigma_m") == "290.0");
    TF_NEAR(value(s, "sigma_10"), fixtureNumber(meta, "sigma_10_Pa"), 1e-6 * fixtureNumber(meta, "sigma_10_Pa"));
    TF_NEAR(s.analysis()->result.strainOrigin, fixtureNumber(meta, "zero_point_1"), 1e-8);
}

TF_CASE(a_compliance_table_is_taken_out_and_must_cover_the_test)
{
    const char *meta = "modes/beam-3pt.json";
    Session s;
    s.profile = "iso-178-2019";
    TF_REQUIRE(s.openData(fixture("modes/beam-3pt.csv")).isEmpty());
    s.specimen.width = fixtureNumber(meta, "width_mm") * 1e-3;
    s.specimen.thickness = fixtureNumber(meta, "thickness_mm") * 1e-3;
    s.fixture.span = fixtureNumber(meta, "span_mm") * 1e-3;
    TF_REQUIRE(s.analyse());
    const double plain = value(s, "Ef");
    TF_REQUIRE(s.setCompliance(fixture("modes/compliance.csv")).isEmpty());
    TF_REQUIRE(s.analyse());
    // The machine's share of the deflection is gone: the beam is stiffer.
    TF_CHECK(value(s, "Ef") > 1.01 * plain);
    bool said = false;
    for (const QJsonValue &v : s.analysis()->doc.value("source").toObject().value("notes").toArray())
        said = said || v.toString().contains("compliance");
    TF_CHECK(said);

    // The beam reaches 164 N; a table that stops at 100 N cannot correct it.
    QTemporaryDir dir;
    QFile f(dir.filePath("short.csv"));
    TF_REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("force_N,displacement_mm\n0,0\n100,0.02\n");
    f.close();
    TF_REQUIRE(s.setCompliance(dir.filePath("short.csv")).isEmpty());
    const Step n = s.next();
    TF_CHECK(n.field == "compliance");
    TF_CHECK(n.text.contains("100 N"));
    TF_CHECK(!s.analyse());
}

TF_CASE(a_bending_record_opens_to_the_same_results_and_fixture)
{
    const char *meta = "modes/beam-4pt.json";
    Session s;
    s.profile = "iso-14125-1998-four-point";
    TF_REQUIRE(s.openData(fixture("modes/beam-4pt.csv")).isEmpty());
    s.specimen.width = fixtureNumber(meta, "width_mm") * 1e-3;
    s.specimen.thickness = fixtureNumber(meta, "thickness_mm") * 1e-3;
    s.fixture.span = fixtureNumber(meta, "span_mm") * 1e-3;
    s.fixture.loadingSpan = fixtureNumber(meta, "loading_span_mm") * 1e-3;
    s.fixture.deflectionAt = Fixture::DeflectionAt::LoadingPoints;
    TF_REQUIRE(s.setCompliance(fixture("modes/compliance.csv")).isEmpty());
    TF_REQUIRE(s.analyse());
    QTemporaryDir dir;
    const QString rec = dir.filePath("beam");
    TF_REQUIRE(s.saveRecord(rec).isEmpty());
    TF_CHECK(QFile::exists(rec + "/channels/deflection.csv"));

    Session r;
    TF_REQUIRE(r.openRecord(rec).isEmpty());
    TF_CHECK(r.notices().isEmpty());
    if (!r.notices().isEmpty())
        std::printf("  notices: %s\n", qPrintable(r.notices().join(" | ")));
    TF_CHECK(r.mode() == "flexure");
    TF_NEAR(r.fixture.span.value_or(NAN), s.fixture.span.value_or(0), 1e-15);
    TF_CHECK(r.fixture.deflectionAt == Fixture::DeflectionAt::LoadingPoints);
    TF_CHECK(!r.compliance().isEmpty());
    TF_CHECK(reported(r, "Ef") == reported(s, "Ef"));
}

TF_CASE(a_real_foam_export_is_analysed_from_its_own_columns)
{
    // Universidade de Sao Paulo's PU foam to ASTM D1621 (CC BY 4.0): a
    // French machine export, no reference results published. This shows
    // the importer and the ISO 844 profile run on real, noisy data with a
    // toe: every result found or refused with a reason, and the zero point
    // after the recorded zero.
    Session s;
    s.profile = "iso-844-2021";
    TF_REQUIRE(s.openData(fixture("puf-d1621/1BAT-DIR1-1.TRA")).isEmpty());
    TF_CHECK(s.mapping.forceUnit == "N");
    TF_CHECK(s.mapping.extensionUnit == "mm");
    // 1BAT-DIM.txt, first row; the archive does not say which of its three
    // columns is the height, so all three are about 50 mm here.
    s.specimen.width = 50.4e-3;
    s.specimen.thickness = 50.8e-3;
    s.specimen.gaugeLength = 50.0e-3;
    TF_REQUIRE(s.analyse());
    for (const QJsonValue &v : s.analysis()->doc.value("results").toArray()) {
        const QJsonObject r = v.toObject();
        TF_CHECK(r.value("status").toString() == "determined"
                 || !r.value("reason").toString().isEmpty());
    }
    TF_CHECK(std::isfinite(value(s, "E")));
    TF_CHECK(std::isfinite(value(s, "sigma_10")));
    TF_CHECK(s.analysis()->result.strainOrigin >= 0);
}

int main() { return tf::test::run(); }
