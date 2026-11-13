// What the chart shows besides the curve, decided in tf_core so it can be
// tested without a window: the regions the analysis found, a label at each
// stress result, the user's own notes, and a modulus window the user moved.
//
// Regions and labels come only from what the analysis determined, so a
// figure can never disagree with its results.

#include "core/Annotations.h"
#include "core/Session.h"
#include "check.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <cmath>
#include <cstdio>

using namespace tf::core;

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

namespace {

QString fixture(const char *rel) { return QString(TF_FIXTURES) + "/" + rel; }

const Region *region(const std::vector<Region> &rs, const QString &id)
{
    for (const auto &r : rs)
        if (r.id == id)
            return &r;
    std::printf("  no region %s\n", qPrintable(id));
    return nullptr;
}

double resultValue(const Session &s, const char *id)
{
    for (const QJsonValue &v : s.analysis()->doc.value("results").toArray())
        if (v.toObject().value("id").toString() == id)
            return v.toObject().value("value").toDouble(NAN);
    return NAN;
}

Session tension()
{
    Session s;
    s.profile = "iso-6892-1-2019";
    s.openData(fixture("synthetic/ro-continuous.csv"));
    s.specimen.shape = Specimen::Shape::GivenArea;
    s.specimen.area = 78.54e-6;
    s.specimen.gaugeLength = 50e-3;
    s.analyse();
    return s;
}

Session foam()
{
    Session s;
    s.profile = "iso-844-2021";
    s.openData(fixture("modes/foam.csv"));
    s.specimen.width = 50.3e-3;
    s.specimen.thickness = 49.6e-3;
    s.specimen.gaugeLength = 49.8e-3;
    s.analyse();
    return s;
}

} // namespace

TF_CASE(the_modulus_region_spans_exactly_the_samples_the_fit_used)
{
    const Session s = tension();
    TF_REQUIRE(s.analysis().has_value());
    const auto rs = analysisRegions(*s.analysis());
    const Region *m = region(rs, "modulus-fit");
    TF_REQUIRE(m != nullptr);
    const auto &fit = *s.analysis()->result.results.at("E").fit;
    TF_CHECK(m->from == s.analysis()->strain[fit.first]);
    TF_CHECK(m->to == s.analysis()->strain[fit.last]);
}

TF_CASE(tension_shows_uniform_elongation_then_necking_split_at_the_maximum)
{
    const Session s = tension();
    TF_REQUIRE(s.analysis().has_value());
    const auto rs = analysisRegions(*s.analysis());
    const Region *u = region(rs, "uniform-elongation");
    const Region *n = region(rs, "necking");
    TF_REQUIRE(u && n);
    const auto &R = s.analysis()->result.results;
    TF_CHECK(u->to == R.at("Rm").point->x);
    TF_CHECK(n->from == R.at("Rm").point->x);
    TF_CHECK(n->to == R.at("Rb").point->x);
    TF_CHECK(u->from < u->to && n->from < n->to);
}

TF_CASE(a_foam_shows_the_toe_its_zero_point_leaves_out)
{
    const Session s = foam();
    TF_REQUIRE(s.analysis().has_value());
    const auto rs = analysisRegions(*s.analysis());
    const Region *t = region(rs, "toe");
    TF_REQUIRE(t != nullptr);
    TF_CHECK(t->from == 0.0);
    TF_NEAR(t->to, 0.004, 1e-8);  // the fixture's planted zero point
    // No break in the foam fixture: no region after one.
    for (const auto &r : rs)
        TF_CHECK(r.id != "after-break");
}

TF_CASE(a_bend_that_broke_shows_what_came_after_the_break)
{
    Session s;
    s.profile = "iso-178-2019";
    s.openData(fixture("modes/beam-3pt.csv"));
    s.specimen.width = 10.2e-3;
    s.specimen.thickness = 4.1e-3;
    s.fixture.span = 64e-3;
    TF_REQUIRE(s.analyse());
    const auto regions = analysisRegions(*s.analysis());
    const Region *b = region(regions, "after-break");
    TF_REQUIRE(b != nullptr);
    TF_NEAR(b->from, 0.07, 1e-8);
    TF_CHECK(b->to > b->from);
}

TF_CASE(each_stress_result_is_labelled_as_reported_where_it_was_found)
{
    const Session s = tension();
    TF_REQUIRE(s.analysis().has_value());
    bool rp = false, rm = false;
    for (const PointLabel &l : resultLabels(*s.analysis())) {
        if (l.id == "Rp0.2") {
            rp = true;
            TF_CHECK(l.text == "Rp0.2 = 300 MPa");
            TF_NEAR(l.stress, s.analysis()->result.results.at("Rp0.2").point->y, 1e-6);
        }
        if (l.id == "Rm") {
            rm = true;
            TF_CHECK(l.text == "Rm = 500 MPa");
        }
        TF_CHECK(l.id != "E");  // the modulus is a line, not a point
    }
    TF_CHECK(rp && rm);
}

TF_CASE(a_note_on_the_curve_sits_on_it)
{
    const Session s = tension();
    TF_REQUIRE(s.analysis().has_value());
    // Ramberg-Osgood: at 300 MPa the strain is 300/200000 + 0.002.
    const auto y = stressAtStrain(*s.analysis(), 0.0035);
    TF_REQUIRE(y.has_value());
    TF_NEAR(*y, 300e6, 0.2e6);
    TF_CHECK(!stressAtStrain(*s.analysis(), 5.0).has_value());  // past the curve
}

TF_CASE(notes_survive_json_and_refuse_nonsense)
{
    const std::vector<CurveNote> notes{{0.002, std::nullopt, "first slip lines"},
                                       {0.01, 0.05, "Lueders band crossing"}};
    QString err;
    const auto back = curveNotesFromJson(curveNotesJson(notes), &err);
    TF_REQUIRE(back.has_value());
    TF_REQUIRE(back->size() == 2);
    TF_CHECK((*back)[0].text == "first slip lines" && !(*back)[0].to);
    TF_CHECK((*back)[1].to == 0.05);
    QJsonArray bad{QJsonObject{{"from", 0.05}, {"to", 0.01}, {"text", "backwards"}}};
    TF_CHECK(!curveNotesFromJson(bad, &err).has_value());
    TF_CHECK(err.contains("before"));
}

TF_CASE(a_moved_modulus_window_changes_the_modulus_and_says_so)
{
    Session s = tension();
    TF_REQUIRE(s.analysis().has_value());
    const double procedure = resultValue(s, "E");
    // 0.2 % to 0.4 % strain lies past the proportional limit: a lower slope.
    s.modulusWindow = std::make_pair(0.002, 0.004);
    TF_REQUIRE(s.analyse());
    const double moved = resultValue(s, "E");
    TF_CHECK(moved < 0.8 * procedure);
    QStringList codes;
    for (const QJsonValue &v :
         s.analysis()->doc.value("conformity").toObject().value("reasons").toArray())
        codes << v.toObject().value("code").toString();
    TF_CHECK(codes.contains("user-modulus-window"));
    const QJsonObject w =
        s.analysis()->doc.value("analysis_choices").toObject().value("modulus_window").toObject();
    TF_CHECK(w.value("from").toDouble() == 0.002 && w.value("to").toDouble() == 0.004);
    // The region drawn is the window moved to.
    const auto regions = analysisRegions(*s.analysis());
    const Region *m = region(regions, "modulus-fit");
    TF_REQUIRE(m != nullptr);
    TF_CHECK(m->from >= 0.002 && m->to <= 0.004);

    s.modulusWindow = std::make_pair(0.004, 0.002);
    TF_CHECK(s.next().field == "modulusWindow");
    s.modulusWindow.reset();
    TF_REQUIRE(s.analyse());
    TF_NEAR(resultValue(s, "E"), procedure, 1e-6 * procedure);
}

TF_CASE(a_moved_window_on_a_foam_is_on_the_strain_as_plotted)
{
    // The foam's zero point is 0.4 % recorded strain. A window given as
    // 1.0 % to 1.5 % of the plotted (recorded) strain lies on its straight
    // part, so the slope is the fixture's 11.5 MPa exactly.
    Session s = foam();
    s.modulusWindow = std::make_pair(0.010, 0.015);
    TF_REQUIRE(s.analyse());
    TF_NEAR(resultValue(s, "E"), 11.5e6, 1e-6 * 11.5e6);
    const auto regions = analysisRegions(*s.analysis());
    const Region *m = region(regions, "modulus-fit");
    TF_REQUIRE(m != nullptr);
    TF_CHECK(m->from >= 0.010 && m->to <= 0.015);
}

TF_CASE(notes_and_the_moved_window_come_back_with_the_record)
{
    Session s = tension();
    s.modulusWindow = std::make_pair(0.0005, 0.0012);
    s.curveNotes = {{0.003, std::nullopt, "here"}, {0.05, 0.08, "necking starts"}};
    TF_REQUIRE(s.analyse());
    QTemporaryDir dir;
    TF_REQUIRE(s.saveRecord(dir.filePath("r")).isEmpty());

    Session r;
    TF_REQUIRE(r.openRecord(dir.filePath("r")).isEmpty());
    TF_CHECK(r.notices().isEmpty());
    if (!r.notices().isEmpty())
        std::printf("  notices: %s\n", qPrintable(r.notices().join(" | ")));
    TF_REQUIRE(r.modulusWindow.has_value());
    TF_CHECK(r.modulusWindow->first == 0.0005 && r.modulusWindow->second == 0.0012);
    TF_REQUIRE(r.curveNotes.size() == 2);
    TF_CHECK(r.curveNotes[1].text == "necking starts");
    TF_NEAR(resultValue(r, "E"), resultValue(s, "E"), 1e-9 * resultValue(s, "E"));
}

int main() { return tf::test::run(); }
