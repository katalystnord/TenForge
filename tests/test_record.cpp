// The test record (docs/specs/test-record-v1.md): written from an imported
// file, opened again, and checked for what changed since.
//
// The inputs are committed fixtures read by the importer; what is checked
// is that nothing is lost, converted or invented between writing and
// reading, and that every file is accounted for by its hash.

#include "core/Import.h"
#include "core/Profile.h"
#include "core/Record.h"
#include "core/Results.h"
#include "core/Specimen.h"
#include "check.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <cmath>

using namespace tf::core;

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

namespace {

QString fixture(const char *rel) { return QString(TF_FIXTURES) + "/" + rel; }

QByteArray bytesOf(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QByteArray sha(const QByteArray &b)
{
    return QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex();
}

QJsonObject jsonOf(const QString &path)
{
    return QJsonDocument::fromJson(bytesOf(path)).object();
}

struct Written {
    QTemporaryDir tmp;
    QString dir;
    Table table;
    TensionImport imp;
    Specimen specimen;
    Profile profile;
    QString error;
};

// The synthetic Ramberg-Osgood curve, a round bar of 10 mm and Le 50 mm.
// Area pi x 25 mm2, not the fixture's 78.54, so the record's area visibly
// comes from the specimen and not from anything the file says.
std::unique_ptr<Written> writeSynthetic(const char *file = "synthetic/ro-continuous.csv",
                                        bool percent = false)
{
    auto w = std::make_unique<Written>();
    w->dir = w->tmp.path() + "/S-01";
    w->table = *readTable(fixture(file)).table;
    w->specimen.id = "S-01";
    w->specimen.material = "synthetic";
    if (percent) {
        w->specimen.shape = Specimen::Shape::GivenArea;
        w->specimen.area = 120.6e-6;
    } else {
        w->specimen.shape = Specimen::Shape::Round;
        w->specimen.diameter = 10e-3;
        w->specimen.gaugeLength = 50e-3;
    }
    TensionMapping m;
    m.area = crossSection(w->specimen).area;
    m.gaugeLength = w->specimen.gaugeLength;
    w->imp = toTension(w->table, m);
    w->profile = *loadProfile("iso-6892-1-2019").profile;
    const auto result = tf::analysis::analyseTension(w->imp.input, w->profile.method);
    const QJsonObject doc = resultsJson(w->profile, result,
                                        {w->table.path, QString::fromLatin1(w->table.sha256),
                                         w->imp.forceColumn, w->imp.extensionColumn, w->imp.notes},
                                        QDateTime::currentDateTimeUtc());
    w->error = writeRecord(w->dir, w->table, w->imp, w->specimen, w->profile, doc);
    return w;
}

} // namespace

TF_CASE(a_written_record_has_the_layout_of_the_spec)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    for (const char *f : {"manifest.tfrec", "test.json", "channels/force.csv",
                          "channels/extension.csv", "source/ro-continuous.csv",
                          "results/results.json", "results/results.csv"})
        if (!QFile::exists(w->dir + "/" + f))
            tf::test::fail(__FILE__, __LINE__, std::string("missing ") + f);
}

TF_CASE(the_source_file_is_kept_byte_for_byte)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    TF_CHECK(bytesOf(w->dir + "/source/ro-continuous.csv")
             == bytesOf(fixture("synthetic/ro-continuous.csv")));
}

TF_CASE(the_manifest_lists_every_file_with_its_hash_and_role)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    const QJsonObject man = jsonOf(w->dir + "/manifest.tfrec");
    TF_CHECK(man.value("format").toString() == "tenforge-record");
    TF_CHECK(man.value("version").toString().startsWith("1."));
    TF_CHECK(man.value("application").toObject().value("name").toString() == "TenForge");
    QStringList listed;
    for (const QJsonValue &v : man.value("files").toArray()) {
        const QJsonObject f = v.toObject();
        const QString path = f.value("path").toString();
        listed << path;
        TF_CHECK(f.value("sha256").toString().toLatin1() == sha(bytesOf(w->dir + "/" + path)));
        TF_CHECK(!f.value("role").toString().isEmpty());
    }
    listed.sort();
    TF_CHECK(listed == QStringList({"channels/extension.csv", "channels/force.csv",
                                    "results/results.csv", "results/results.json",
                                    "source/ro-continuous.csv", "test.json"}));
}

TF_CASE(channels_hold_the_raw_value_and_the_si_value_per_sample)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    const QStringList lines = QString::fromUtf8(bytesOf(w->dir + "/channels/force.csv"))
                                  .split('\n', Qt::SkipEmptyParts);
    TF_CHECK(lines.value(0) == "t_device_s,t_host_s,raw,force_N");
    TF_CHECK(lines.size() == int(w->table.columns[0].size()) + 1);
    // Row 3 of the fixture: 0.117810000 kN.
    const QStringList cells = lines.value(4).split(',');
    TF_REQUIRE(cells.size() == 4);
    TF_NEAR(cells[2].toDouble(), 0.11781, 1e-12);
    TF_NEAR(cells[3].toDouble(), 117.81, 1e-9);
    // An imported file has no host arrival times: empty, not invented.
    TF_CHECK(cells[0].isEmpty());
    TF_CHECK(cells[1].isEmpty());
}

TF_CASE(test_json_describes_specimen_profile_and_where_each_channel_came_from)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    const QJsonObject t = jsonOf(w->dir + "/test.json");
    TF_CHECK(t.value("mode").toString() == "tension");
    TF_CHECK(t.value("specimen").toObject().value("id").toString() == "S-01");
    const QJsonObject p = t.value("profile").toObject();
    TF_CHECK(p.value("id").toString() == "iso-6892-1-2019");
    TF_CHECK(p.value("sha256").toString().toLatin1() == w->profile.sha256);
    const QJsonObject force = t.value("channels").toObject().value("force").toObject();
    TF_CHECK(force.value("quantity").toString() == "force");
    TF_CHECK(force.value("kind").toString() == "measured");
    const QJsonObject src = force.value("source").toObject();
    TF_CHECK(src.value("file").toString() == "source/ro-continuous.csv");
    TF_CHECK(src.value("column").toString() == "force_kN");
    TF_CHECK(src.value("unit").toString() == "kN");
}

TF_CASE(results_in_a_record_name_the_record_files_they_used)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    const QJsonObject r = jsonOf(w->dir + "/results/results.json");
    TF_CHECK(r.value("source").toObject().value("path").toString() == "source/ro-continuous.csv");
    QStringList used;
    for (const QJsonValue &v : r.value("record").toObject().value("inputs").toArray()) {
        const QJsonObject f = v.toObject();
        used << f.value("path").toString();
        TF_CHECK(f.value("sha256").toString().toLatin1()
                 == sha(bytesOf(w->dir + "/" + f.value("path").toString())));
    }
    used.sort();
    TF_CHECK(used == QStringList({"channels/extension.csv", "channels/force.csv", "test.json"}));
}

TF_CASE(an_opened_record_gives_back_exactly_what_was_written)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    const OpenedRecord o = openRecord(w->dir);
    TF_REQUIRE(o.error.isEmpty());
    TF_CHECK(o.problems.isEmpty());
    TF_CHECK(o.unlisted.isEmpty());
    TF_CHECK(o.specimen.id == "S-01");
    TF_CHECK(o.specimen.diameter == w->specimen.diameter);
    TF_CHECK(o.profileId == "iso-6892-1-2019");
    TF_CHECK(o.profileSha256 == w->profile.sha256);
    TF_CHECK(o.forceColumn == "force_kN");
    TF_CHECK(o.input.area == w->imp.input.area);
    TF_CHECK(o.input.gaugeLength == w->imp.input.gaugeLength);
    TF_REQUIRE(o.input.force.size() == w->imp.input.force.size());
    bool same = true;
    for (std::size_t i = 0; i < o.input.force.size(); ++i)
        same = same && o.input.force[i] == w->imp.input.force[i]
               && o.input.extension[i] == w->imp.input.extension[i];
    TF_CHECK(same);  // bit for bit, not "close"
    TF_CHECK(o.results.value("format").toString() == "tenforge-results");
}

TF_CASE(a_percent_strain_channel_is_a_strain_and_an_empty_cell_stays_empty)
{
    const auto w = writeSynthetic("import/decimal-comma.csv", true);
    TF_REQUIRE(w->error.isEmpty());
    const QStringList lines = QString::fromUtf8(bytesOf(w->dir + "/channels/strain.csv"))
                                  .split('\n', Qt::SkipEmptyParts);
    TF_CHECK(lines.value(0) == "t_device_s,t_host_s,raw,strain_1");
    TF_CHECK(lines.value(4) == ",,,");  // the extensometer's empty cell
    const OpenedRecord o = openRecord(w->dir);
    TF_REQUIRE(o.error.isEmpty());
    TF_CHECK(o.input.gaugeLength == 1.0);
    TF_NEAR(o.input.extension[2], 0.0007, 1e-15);
    TF_CHECK(std::isnan(o.input.extension[3]));
}

TF_CASE(a_changed_file_is_reported_by_name)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    QFile f(w->dir + "/channels/force.csv");
    TF_REQUIRE(f.open(QIODevice::Append));
    f.write("1,2,3,4\n");
    f.close();
    const OpenedRecord o = openRecord(w->dir);
    TF_CHECK(!o.problems.filter("channels/force.csv").isEmpty());
}

TF_CASE(a_missing_file_is_reported_by_name)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    QFile::remove(w->dir + "/results/results.csv");
    const OpenedRecord o = openRecord(w->dir);
    TF_CHECK(!o.problems.filter("results/results.csv").isEmpty());
}

TF_CASE(another_tools_files_are_listed_never_adopted_silently)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    QDir().mkpath(w->dir + "/analyses/fit-r");
    QFile f(w->dir + "/analyses/fit-r/fit.R");
    TF_REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("fit <- lm(y ~ x)\n");
    f.close();
    const QByteArray manifestBefore = bytesOf(w->dir + "/manifest.tfrec");

    OpenedRecord o = openRecord(w->dir);
    TF_CHECK(o.unlisted == QStringList({"analyses/fit-r/fit.R"}));
    TF_CHECK(bytesOf(w->dir + "/manifest.tfrec") == manifestBefore);  // opening changes nothing

    TF_CHECK(addUnlistedFiles(w->dir).isEmpty());
    o = openRecord(w->dir);
    TF_CHECK(o.unlisted.isEmpty());
    TF_CHECK(o.problems.isEmpty());
    bool found = false;
    for (const QJsonValue &v : jsonOf(w->dir + "/manifest.tfrec").value("files").toArray())
        if (v.toObject().value("path").toString() == "analyses/fit-r/fit.R")
            found = v.toObject().value("role").toString() == "analysis";
    TF_CHECK(found);
}

TF_CASE(a_record_of_an_unknown_major_version_is_refused_with_the_version)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    QJsonObject man = jsonOf(w->dir + "/manifest.tfrec");
    man["version"] = "2.0";
    QFile f(w->dir + "/manifest.tfrec");
    TF_REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(QJsonDocument(man).toJson());
    f.close();
    const OpenedRecord o = openRecord(w->dir);
    TF_CHECK(o.error.contains("2.0"));
}

TF_CASE(a_record_is_never_written_over_an_existing_folder_with_files)
{
    const auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    const QJsonObject doc = jsonOf(w->dir + "/results/results.json");
    const QString again = writeRecord(w->dir, w->table, w->imp, w->specimen, w->profile, doc);
    TF_CHECK(!again.isEmpty());
}

TF_CASE(a_specimen_that_disagrees_with_the_analysed_area_is_refused)
{
    auto w = writeSynthetic();
    TF_REQUIRE(w->error.isEmpty());
    Specimen other = w->specimen;
    other.diameter = 12e-3;
    const QString err = writeRecord(w->tmp.path() + "/other", w->table, w->imp, other, w->profile,
                                    jsonOf(w->dir + "/results/results.json"));
    TF_CHECK(err.contains("area"));
}

int main() { return tf::test::run(); }
