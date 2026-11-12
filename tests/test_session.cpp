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

TF_CASE(a_new_session_says_to_open_a_file)
{
    Session s;
    const Step n = s.next();
    TF_CHECK(!n.ready);
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

int main() { return tf::test::run(); }
