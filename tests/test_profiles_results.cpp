// Profiles in, results documents out (profile-v1.md, results-v1.md).

#include "core/Import.h"
#include "core/Profile.h"
#include "core/Results.h"
#include "check.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <cmath>
#include <functional>
#include <tuple>

using namespace tf::core;

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

static QString fixture(const char *rel) { return QString(TF_FIXTURES) + "/" + rel; }

static QJsonObject resultById(const QJsonObject &doc, const QString &id)
{
    for (const QJsonValue &v : doc.value("results").toArray())
        if (v.toObject().value("id").toString() == id)
            return v.toObject();
    return {};
}

TF_CASE(both_shipped_profiles_are_compiled_in)
{
    const QStringList ids = shippedProfiles();
    TF_CHECK(ids.contains("iso-6892-1-2019"));
    TF_CHECK(ids.contains("iso-527-1-2019"));
}

TF_CASE(metals_profile_fills_the_engine_method)
{
    const auto r = loadProfile("iso-6892-1-2019");
    TF_REQUIRE(r.profile.has_value());
    const auto &m = r.profile->method;
    TF_CHECK(r.profile->shipped);
    TF_CHECK(m.modulusAxis == tf::analysis::TensionMethod::Axis::Stress);
    TF_NEAR(m.modulusFrom, 0.10, 0);
    TF_NEAR(m.modulusTo, 0.40, 0);
    TF_NEAR(m.modulusReferenceOffset, 0.002, 0);
    TF_CHECK(m.proofOffsets.size() == 1);
    TF_CHECK(m.yieldPoint);
    TF_NEAR(m.yieldMaxPlastic, 0.01, 0);
    TF_CHECK(!m.lowerYieldIgnoresTransient);
    TF_NEAR(m.fractureRatio, 5.0, 0);
    TF_NEAR(m.fractureMinDrop, 0.10, 0);
    TF_CHECK(m.strainFromElasticLine);
}

TF_CASE(plastics_profile_uses_a_strain_window_and_the_recorded_zero)
{
    const auto r = loadProfile("iso-527-1-2019");
    TF_REQUIRE(r.profile.has_value());
    const auto &m = r.profile->method;
    TF_CHECK(m.modulusAxis == tf::analysis::TensionMethod::Axis::Strain);
    TF_NEAR(m.modulusFrom, 0.0005, 0);
    TF_NEAR(m.modulusTo, 0.0025, 0);
    TF_CHECK(!m.yieldPoint);
    TF_CHECK(!m.strainFromElasticLine);
    TF_CHECK(m.proofOffsets.empty());
}

TF_CASE(a_profile_naming_the_wrong_primitive_is_refused_and_says_which)
{
    const auto r = loadProfile(fixture("profiles/bad-primitive.json"));
    TF_CHECK(!r.profile.has_value());
    TF_CHECK(r.error.contains("first-local-maximum"));
    TF_CHECK(r.error.contains("sigma_m"));
}

TF_CASE(an_unknown_profile_lists_the_shipped_ones)
{
    const auto r = loadProfile("iso-0000");
    TF_CHECK(!r.profile.has_value());
    TF_CHECK(r.error.contains("iso-6892-1-2019"));
}

TF_CASE(every_parameter_is_listed_with_whether_it_is_confirmed)
{
    const auto r = loadProfile("iso-6892-1-2019");
    TF_REQUIRE(r.profile.has_value());
    bool window = false, offsetConfirmed = false;
    for (const auto &u : r.profile->parameters) {
        if (u.path == "results.E.method.window.from" && !u.confirmed)
            window = true;
        if (u.path == "results.Rp0.2.method.offset" && u.confirmed)
            offsetConfirmed = true;
    }
    TF_CHECK(window);
    TF_CHECK(offsetConfirmed);
}

TF_CASE(reported_values_are_converted_rounded_and_kept_as_text)
{
    ProfileResult mpa{"Rp0.2", "", "", "Rp0.2", "Pa", "offset-intersection", "MPa", 1};
    TF_CHECK(reportedText(309.73e6, mpa) == "310");
    ProfileResult pct{"A", "", "", "A", "1", "break", "%", 0.5};
    TF_CHECK(reportedText(0.4123, pct) == "41.0");
    TF_CHECK(reportedText(0.4126, pct) == "41.5");
    ProfileResult gpa{"E", "", "", "E", "Pa", "linear-fit", "GPa", 0.1};
    TF_CHECK(reportedText(213.24e9, gpa) == "213.2");
    ProfileResult bad{"E", "", "", "E", "Pa", "linear-fit", "furlong", 0.1};
    TF_CHECK(reportedText(1.0, bad).isEmpty());
}

TF_CASE(results_document_states_conformity_and_how_each_value_was_made)
{
    const auto prof = loadProfile("iso-6892-1-2019");
    TF_REQUIRE(prof.profile.has_value());
    const auto table = readTable(fixture("synthetic/ro-continuous.csv"));
    TF_REQUIRE(table.table.has_value());
    TensionMapping map;
    map.area = 78.54e-6;
    map.gaugeLength = 0.05;
    const auto imp = toTension(*table.table, map);
    TF_REQUIRE(imp.error.isEmpty());
    const auto res = tf::analysis::analyseTension(imp.input, prof.profile->method);
    const QJsonObject doc = resultsJson(*prof.profile, res,
                                        {table.table->path, QString::fromLatin1(table.table->sha256),
                                         imp.forceColumn, imp.extensionColumn, imp.notes},
                                        QDateTime::currentDateTimeUtc());

    TF_CHECK(doc.value("format").toString() == "tenforge-results");
    const QJsonObject conf = doc.value("conformity").toObject();
    TF_CHECK(conf.value("status").toString() == "aligned");
    QStringList codes;
    for (const QJsonValue &v : conf.value("reasons").toArray())
        codes << v.toObject().value("code").toString();
    TF_CHECK(codes.contains("unconfirmed-parameter"));
    TF_CHECK(codes.contains("no-force-calibration"));
    TF_CHECK(!codes.contains("user-profile"));

    const QJsonObject rp = resultById(doc, "Rp0.2");
    TF_CHECK(rp.value("status").toString() == "determined");
    TF_NEAR(rp.value("value").toDouble(), 300e6, 0.05e6);
    TF_CHECK(rp.value("reported").toObject().value("value").toString() == "300");
    TF_CHECK(rp.value("how").toObject().value("primitive").toString() == "offset-intersection");

    const QJsonObject e = resultById(doc, "E");
    TF_CHECK(e.value("how").toObject().value("samples").toObject().value("n").toInt() > 3);
    TF_CHECK(e.value("how").toObject().value("statistics").toObject().contains("r2"));

    // No yield point on a continuous curve: null with a reason, never 0.
    const QJsonObject reh = resultById(doc, "ReH");
    TF_CHECK(reh.value("status").toString() == "not-determined");
    TF_CHECK(reh.value("value").isNull());
    TF_CHECK(!reh.value("reason").toString().isEmpty());

    const QString csv = resultsCsv(doc);
    TF_CHECK(csv.startsWith("id,symbol,status,value,unit,reported_value"));
    TF_CHECK(csv.contains("Rp0.2,R_p0.2,determined,"));
    TF_CHECK(csv.contains("ReH,R_eH,not-determined,,Pa,,,,,aligned,"));
}

TF_CASE(a_user_profile_is_labelled_as_not_shipped)
{
    const auto prof = loadProfile(fixture("profiles/user-polymer.json"));
    TF_REQUIRE(prof.profile.has_value());
    TF_CHECK(!prof.profile->shipped);
    const auto table = readTable(fixture("synthetic/polymer.csv"));
    TF_REQUIRE(table.table.has_value());
    TensionMapping map;
    map.area = 40e-6;
    map.gaugeLength = 0.075;
    const auto imp = toTension(*table.table, map);
    const auto res = tf::analysis::analyseTension(imp.input, prof.profile->method);
    const QJsonObject doc = resultsJson(*prof.profile, res, {}, QDateTime::currentDateTimeUtc());
    bool user = false;
    for (const QJsonValue &v : doc.value("conformity").toObject().value("reasons").toArray())
        user = user || v.toObject().value("code").toString() == "user-profile";
    TF_CHECK(user);
    TF_NEAR(resultById(doc, "Et").value("value").toDouble(), 2.5e9, 1e3);
}

// --- Compression and bending (Phase 3) -----------------------------------------

namespace {

// A profile file made from a shipped one with one change, to show what the
// loader refuses.
QString variant(const QTemporaryDir &dir, const QString &shipped,
                const std::function<void(QJsonObject &)> &change)
{
    QFile in(":/profiles/" + shipped + ".json");
    if (!in.open(QIODevice::ReadOnly))
        return {};
    QJsonObject root = QJsonDocument::fromJson(in.readAll()).object();
    change(root);
    const QString path = dir.filePath("variant.json");
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    out.write(QJsonDocument(root).toJson());
    return path;
}

const tf::analysis::CurveRequest *request(const Profile &p, const std::string &id)
{
    for (const auto &q : p.curve.requests)
        if (q.id == id)
            return &q;
    return nullptr;
}

} // namespace

TF_CASE(compression_and_bending_profiles_are_shipped_with_their_modes)
{
    const std::vector<std::tuple<QString, QString, QString>> want{
        {"iso-178-2019", "flexure", "three-point"},
        {"iso-14125-1998-three-point", "flexure", "three-point"},
        {"iso-14125-1998-four-point", "flexure", "four-point"},
        {"iso-604-2002", "compression", "platens"},
        {"iso-844-2021", "compression", "platens"}};
    for (const auto &[id, mode, fixture] : want) {
        const auto p = loadProfile(id);
        if (!p.profile) {
            tf::test::fail(__FILE__, __LINE__, (id + ": " + p.error).toStdString());
            continue;
        }
        TF_CHECK(p.profile->mode == mode);
        TF_CHECK(p.profile->fixture == fixture);
        TF_CHECK(!p.profile->designation.isEmpty());
    }
}

TF_CASE(flexure_profile_fills_the_curve_method)
{
    const auto p = loadProfile("iso-178-2019");
    TF_REQUIRE(p.profile.has_value());
    const auto &c = p.profile->curve;
    TF_CHECK(c.zero == tf::analysis::CurveMethod::Zero::AsRecorded);
    TF_CHECK(c.modulus == tf::analysis::CurveMethod::Modulus::StrainWindow);
    TF_NEAR(c.modulusFrom, 0.0005, 1e-15);
    TF_NEAR(c.modulusTo, 0.0025, 1e-15);
    TF_REQUIRE(request(*p.profile, "Ef") != nullptr);
    TF_CHECK(request(*p.profile, "Ef")->quantity == tf::analysis::CurveQuantity::Modulus);
    TF_REQUIRE(request(*p.profile, "sigma_fC") != nullptr);
    TF_CHECK(request(*p.profile, "sigma_fC")->quantity == tf::analysis::CurveQuantity::StressAtStrain);
    bool fc = false;
    for (const auto &r : p.profile->results)
        if (r.id == "sigma_fC") {
            fc = true;
            TF_NEAR(r.deflectionPerThickness, 1.5, 1e-15);
            TF_CHECK(r.key == "sigma_fC");
        }
    TF_CHECK(fc);
    TF_REQUIRE(p.profile->validity.size() == 1);
    TF_CHECK(p.profile->validity[0].numerator == "span");
    TF_NEAR(p.profile->validity[0].min, 15.0, 1e-15);
    TF_NEAR(p.profile->validity[0].max, 17.0, 1e-15);
}

TF_CASE(foam_profile_counts_from_the_steepest_line)
{
    const auto p = loadProfile("iso-844-2021");
    TF_REQUIRE(p.profile.has_value());
    const auto &c = p.profile->curve;
    TF_CHECK(c.zero == tf::analysis::CurveMethod::Zero::ElasticLine);
    TF_CHECK(c.modulus == tf::analysis::CurveMethod::Modulus::MaxSlope);
    TF_NEAR(c.maxSlopeWidth, 0.01, 1e-15);
    TF_REQUIRE(request(*p.profile, "sigma_m") != nullptr);
    TF_NEAR(request(*p.profile, "sigma_m")->at, 0.10, 1e-15);
    TF_NEAR(request(*p.profile, "sigma_10")->at, 0.10, 1e-15);
}

TF_CASE(a_curve_profile_that_cannot_run_is_refused_and_says_why)
{
    QTemporaryDir dir;
    // A tension quantity in a flexure profile.
    auto a = loadProfile(variant(dir, "iso-178-2019", [](QJsonObject &r) {
        QJsonArray rs = r["results"].toArray();
        QJsonObject x = rs[0].toObject();
        x["quantity"] = "Rm";
        rs[0] = x;
        r["results"] = rs;
    }));
    TF_CHECK(!a.profile);
    TF_CHECK(a.error.contains("Rm"));
    // Counting strain from the elastic line needs a modulus.
    auto b = loadProfile(variant(dir, "iso-844-2021", [](QJsonObject &r) {
        QJsonArray rs = r["results"].toArray();
        rs.removeAt(0);
        r["results"] = rs;
    }));
    TF_CHECK(!b.profile);
    TF_CHECK(b.error.contains("modulus"));
    // A bending profile must say how the beam is loaded.
    auto c = loadProfile(variant(dir, "iso-178-2019", [](QJsonObject &r) {
        QJsonObject in = r["inputs"].toObject();
        in["fixture"] = "five-point";
        r["inputs"] = in;
    }));
    TF_CHECK(!c.profile);
    TF_CHECK(c.error.contains("five-point"));
    // A validity check on a quantity TenForge does not know.
    auto d = loadProfile(variant(dir, "iso-178-2019", [](QJsonObject &r) {
        QJsonArray v = r["validity"].toArray();
        QJsonObject x = v[0].toObject();
        x["numerator"] = "colour";
        v[0] = x;
        r["validity"] = v;
    }));
    TF_CHECK(!d.profile);
    TF_CHECK(d.error.contains("colour"));
}

int main() { return tf::test::run(); }
