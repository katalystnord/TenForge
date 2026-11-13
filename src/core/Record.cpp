#include "core/Record.h"

#include "core/ApplicationInfo.h"
#include "core/Results.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <cmath>

namespace tf::core {

namespace {

const QString kManifest = QStringLiteral("manifest.tfrec");

QByteArray sha256Hex(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

// Shortest text that reads back as the same double; empty for a missing value.
QString number(double v)
{
    return std::isfinite(v) ? QString::number(v, 'g', 17) : QString();
}

struct Writer {
    QString dir;
    QJsonArray files;
    QString error;

    bool write(const QString &rel, const QByteArray &bytes, const QJsonObject &entry)
    {
        if (!error.isEmpty())
            return false;
        const QString path = dir + "/" + rel;
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) {
            error = "cannot write " + path + ": " + f.errorString();
            return false;
        }
        QJsonObject e = entry;
        e["path"] = rel;
        e["sha256"] = QString::fromLatin1(sha256Hex(bytes));
        files.append(e);
        return true;
    }

    QString shaOf(const QString &rel) const
    {
        for (const QJsonValue &v : files)
            if (v.toObject().value("path").toString() == rel)
                return v.toObject().value("sha256").toString();
        return {};
    }
};

QByteArray channelCsv(const QString &siColumn, const std::vector<double> &raw,
                      const std::vector<double> &si)
{
    QByteArray out = "t_device_s,t_host_s,raw," + siColumn.toUtf8() + "\n";
    for (std::size_t i = 0; i < si.size(); ++i) {
        // An imported file has no device or host arrival times: both empty.
        out += ",," + number(i < raw.size() ? raw[i] : NAN).toUtf8() + "," + number(si[i]).toUtf8()
               + "\n";
    }
    return out;
}

QByteArray jsonBytes(const QJsonObject &o)
{
    return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

QJsonObject readJson(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = "cannot read " + path;
        return {};
    }
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(f.readAll(), &pe);
    if (!d.isObject()) {
        *error = path + " is not a JSON object: " + pe.errorString();
        return {};
    }
    return d.object();
}

QString roleFor(const QString &rel)
{
    const QString top = rel.section('/', 0, 0);
    if (top == "analyses")
        return "analysis";
    if (top == "channels")
        return "channel";
    if (top == "source")
        return "source";
    if (top == "results")
        return "results";
    if (top == "frames")
        return "frame";
    if (top == "fields")
        return rel.endsWith(".pvd") ? "field-collection" : "field";
    if (rel == "frames.csv")
        return "frame-index";
    if (rel == "test.json")
        return "metadata";
    return "other";
}

QStringList listedPaths(const QJsonObject &manifest)
{
    QStringList out;
    for (const QJsonValue &v : manifest.value("files").toArray())
        out << v.toObject().value("path").toString();
    return out;
}

QStringList filesUnder(const QString &dir)
{
    QStringList out;
    QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    const QDir base(dir);
    while (it.hasNext()) {
        const QString rel = base.relativeFilePath(it.next());
        if (rel != kManifest)
            out << rel;
    }
    out.sort();
    return out;
}

QStringList unlistedIn(const QString &dir, const QJsonObject &manifest)
{
    const QStringList listed = listedPaths(manifest);
    QStringList out;
    for (const QString &rel : filesUnder(dir))
        if (!listed.contains(rel))
            out << rel;
    return out;
}

// One SI column of a channel file; an empty cell is NaN.
bool readChannel(const QString &path, const QString &column, std::vector<double> *values,
                 QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = "cannot read " + path;
        return false;
    }
    const QStringList lines = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        *error = path + " is empty";
        return false;
    }
    const int col = lines.front().trimmed().split(',').indexOf(column);
    if (col < 0) {
        *error = path + " has no column " + column;
        return false;
    }
    for (int i = 1; i < lines.size(); ++i) {
        const QString cell = lines[i].trimmed().split(',').value(col);
        bool ok = false;
        const double v = cell.toDouble(&ok);
        values->push_back(cell.isEmpty() || !ok ? NAN : v);
    }
    return true;
}

} // namespace

QString writeRecord(const QString &dir, const Table &source, const TensionImport &imported,
                    const Specimen &specimen, const Profile &profile, QJsonObject results,
                    const Fixture &fixture, const QString &compliancePath)
{
    if (QDir(dir).exists() && !QDir(dir).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden))
        return dir + " already holds files; a record is only written into a new or empty folder";

    // The specimen in test.json must be the one the results were computed
    // with, or the record would contradict itself.
    // (Tension carries its geometry in the import; the curve modes compute
    // stress and strain from the specimen and fixture written here.)
    const auto area = crossSection(specimen);
    if (!area.area)
        return "the specimen has no cross-section area: " + area.reason;
    if (profile.mode == "tension") {
        if (std::fabs(*area.area - imported.input.area) > 1e-12 * std::fabs(*area.area))
            return QString("the specimen's cross-section area (%1 mm2) is not the area the "
                           "analysis used (%2 mm2)")
                .arg(*area.area * 1e6, 0, 'g', 10)
                .arg(imported.input.area * 1e6, 0, 'g', 10);
        if (!imported.extensionIsStrain
            && (!specimen.gaugeLength || *specimen.gaugeLength != imported.input.gaugeLength))
            return "the specimen's gauge length is not the gauge length the analysis used";
    }
    if (profile.mode == "flexure" && !fixture.span)
        return "a bending record needs the support span";
    QByteArray complianceBytes;
    if (!compliancePath.isEmpty()) {
        QFile cf(compliancePath);
        if (!cf.open(QIODevice::ReadOnly))
            return "cannot read the compliance table " + compliancePath + " to keep it in the record";
        complianceBytes = cf.readAll();
    }

    QFile in(source.path);
    if (!in.open(QIODevice::ReadOnly))
        return "cannot read " + source.path + " again to keep it in the record";
    const QByteArray sourceBytes = in.readAll();
    if (sha256Hex(sourceBytes) != source.sha256)
        return source.path + " has changed since it was read; open it again before saving";

    if (!QDir().mkpath(dir))
        return "cannot create " + dir;

    Writer w{dir, {}, {}};
    const QString sourceRel = "source/" + QFileInfo(source.path).fileName();
    w.write(sourceRel, sourceBytes, {{"role", "source"}});

    const int fc = source.names.indexOf(imported.forceColumn);
    const int ec = source.names.indexOf(imported.extensionColumn);
    if (fc < 0 || ec < 0)
        return "the imported columns are not in the source table";
    const QString disp = profile.mode == "flexure"       ? "deflection"
                         : profile.mode == "compression" ? "deformation"
                                                         : "extension";
    const QString extId = imported.extensionIsStrain ? "strain" : disp;
    const QString extSi = imported.extensionIsStrain ? "strain_1" : disp + "_m";
    std::vector<double> extSiValues = imported.input.extension;
    if (imported.extensionIsStrain)
        for (double &v : extSiValues)
            v /= imported.input.gaugeLength;  // 1 by construction: stored as strain
    w.write("channels/force.csv", channelCsv("force_N", source.columns[fc], imported.input.force),
            {{"role", "channel"}, {"channel", "force"}});
    w.write("channels/" + extId + ".csv", channelCsv(extSi, source.columns[ec], extSiValues),
            {{"role", "channel"}, {"channel", extId}});

    auto channel = [&](const QString &quantity, const QString &unit, const QString &column,
                       const QString &fileUnit) {
        return QJsonObject{{"quantity", quantity},
                           {"unit", unit},
                           {"kind", "measured"},
                           {"timestamps", "none"},
                           {"calibration", QJsonValue()},
                           {"source", QJsonObject{{"file", sourceRel},
                                                  {"column", column},
                                                  {"unit", fileUnit}}}};
    };
    QString complianceRel;
    if (!compliancePath.isEmpty()) {
        complianceRel = "source/compliance-" + QFileInfo(compliancePath).fileName();
        w.write(complianceRel, complianceBytes, {{"role", "compliance"}});
    }
    QJsonObject test{
        {"mode", profile.mode},
        {"specimen", specimenJson(specimen)},
        {"fixture", fixtureJson(profile.fixture, fixture)},
        {"profile", QJsonObject{{"id", profile.id},
                                {"version", profile.profileVersion},
                                {"sha256", QString::fromLatin1(profile.sha256)},
                                {"shipped", profile.shipped}}},
        {"machine", QJsonObject{{"name", ""}, {"driver", "import"}, {"driver_version", ""}}},
        {"channels", QJsonObject{{"force", channel("force", "N", imported.forceColumn, imported.forceUnit)},
                                 {extId, channel(imported.extensionIsStrain ? "strain" : disp,
                                                 imported.extensionIsStrain ? "1" : "m",
                                                 imported.extensionColumn, imported.extensionUnit)}}},
        {"calibration", QJsonArray()},
        {"operator", ""}};
    if (!complianceRel.isEmpty())
        test["compliance"] = QJsonObject{{"file", complianceRel}, {"sha256", w.shaOf(complianceRel)}};
    w.write("test.json", jsonBytes(test), {{"role", "metadata"}});

    // Results inside a record point into it. The manifest lists the results
    // and so cannot also be hashed by them: they name the record files they
    // were computed from instead (results-v1.md, 2).
    QJsonObject src = results.value("source").toObject();
    src["path"] = sourceRel;
    results["source"] = src;
    QJsonArray inputs;
    for (const QString &rel : {QString("test.json"), QString("channels/force.csv"),
                               "channels/" + extId + ".csv"})
        inputs.append(QJsonObject{{"path", rel}, {"sha256", w.shaOf(rel)}});
    results["record"] = QJsonObject{{"manifest_sha256", QJsonValue()}, {"inputs", inputs}};
    w.write("results/results.json", jsonBytes(results), {{"role", "results"}});
    w.write("results/results.csv", resultsCsv(results).toUtf8(), {{"role", "results"}});

    const QJsonObject manifest{
        {"format", "tenforge-record"},
        {"version", "1.0"},
        {"created", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"start", QJsonObject{{"host_utc", QJsonValue()}}},
        {"application", QJsonObject{{"name", applicationName()},
                                    {"version", applicationVersion()},
                                    {"build", applicationBuild()}}},
        {"files", w.files}};
    if (!w.error.isEmpty())
        return w.error;
    QSaveFile mf(dir + "/" + kManifest);
    const QByteArray mb = jsonBytes(manifest);
    if (!mf.open(QIODevice::WriteOnly) || mf.write(mb) != mb.size() || !mf.commit())
        return "cannot write the manifest in " + dir;
    return {};
}

OpenedRecord openRecord(const QString &dirIn)
{
    OpenedRecord r;
    QString dir = dirIn;
    if (QFileInfo(dir).isFile())
        dir = QFileInfo(dir).absolutePath();  // the manifest itself was named
    r.dir = dir;
    if (!QFile::exists(dir + "/" + kManifest)) {
        r.error = "no " + kManifest + " in " + dir + "; it is not a TenForge record";
        return r;
    }
    QString err;
    const QJsonObject manifest = readJson(dir + "/" + kManifest, &err);
    if (!err.isEmpty()) {
        r.error = err;
        return r;
    }
    if (manifest.value("format").toString() != "tenforge-record") {
        r.error = dir + "/" + kManifest + " is not a TenForge record manifest";
        return r;
    }
    const QString version = manifest.value("version").toString();
    if (version.section('.', 0, 0) != "1") {
        r.error = "the record is version " + version
                  + "; this TenForge reads version 1 records. A newer TenForge is needed.";
        return r;
    }

    for (const QJsonValue &v : manifest.value("files").toArray()) {
        const QJsonObject f = v.toObject();
        const QString rel = f.value("path").toString();
        QFile file(dir + "/" + rel);
        if (!file.open(QIODevice::ReadOnly)) {
            r.problems << rel + " is listed in the manifest but missing";
            continue;
        }
        if (sha256Hex(file.readAll()) != f.value("sha256").toString().toLatin1())
            r.problems << rel + " has changed since the record was written (its SHA-256 no "
                                "longer matches the manifest)";
    }
    r.unlisted = unlistedIn(dir, manifest);

    const QJsonObject test = readJson(dir + "/test.json", &err);
    if (!err.isEmpty()) {
        r.error = err;
        return r;
    }
    r.mode = test.value("mode").toString();
    if (r.mode != "tension" && r.mode != "compression" && r.mode != "flexure") {
        r.error = "the record is a " + r.mode
                  + " test; this TenForge analyses tension, compression and flexure tests";
        return r;
    }
    r.fixture = fixtureFromJson(test.value("fixture").toObject());
    r.complianceFile = test.value("compliance").toObject().value("file").toString();
    const auto spec = specimenFromJson(test.value("specimen").toObject(), &err);
    if (!spec) {
        r.error = err;
        return r;
    }
    r.specimen = *spec;
    const QJsonObject prof = test.value("profile").toObject();
    r.profileId = prof.value("id").toString();
    r.profileVersion = prof.value("version").toString();
    r.profileSha256 = prof.value("sha256").toString().toLatin1();

    const QJsonObject channels = test.value("channels").toObject();
    const bool strain = channels.contains("strain");
    const QString disp = r.mode == "flexure"       ? "deflection"
                         : r.mode == "compression" ? "deformation"
                                                   : "extension";
    const QString extId = strain ? "strain" : disp;
    if (!channels.contains("force") || !channels.contains(extId)) {
        r.error = "the record has no force and " + disp + " channels to analyse";
        return r;
    }
    const QJsonObject fsrc = channels.value("force").toObject().value("source").toObject();
    const QJsonObject esrc = channels.value(extId).toObject().value("source").toObject();
    r.sourceFile = fsrc.value("file").toString();
    r.forceColumn = fsrc.value("column").toString();
    r.forceUnit = fsrc.value("unit").toString();
    r.extensionColumn = esrc.value("column").toString();
    r.extensionUnit = esrc.value("unit").toString();

    if (!readChannel(dir + "/channels/force.csv", "force_N", &r.input.force, &err)
        || !readChannel(dir + "/channels/" + extId + ".csv", strain ? "strain_1" : disp + "_m",
                        &r.input.extension, &err)) {
        r.error = err;
        return r;
    }
    if (r.input.force.size() != r.input.extension.size()) {
        r.error = "the force and extension channels have different numbers of samples";
        return r;
    }
    const auto area = crossSection(r.specimen);
    if (!area.area) {
        r.error = "the record's specimen has no cross-section area: " + area.reason;
        return r;
    }
    r.input.area = *area.area;
    if (r.mode == "flexure") {
        r.input.area = 0;
        r.input.gaugeLength = strain ? 1.0 : 0.0;
    } else if (strain) {
        r.input.gaugeLength = 1.0;
    } else if (r.specimen.gaugeLength) {
        r.input.gaugeLength = *r.specimen.gaugeLength;
    } else {
        r.error = "the record's specimen has no gauge length to turn extension into strain";
        return r;
    }

    if (QFile::exists(dir + "/results/results.json")) {
        QString rerr;
        r.results = readJson(dir + "/results/results.json", &rerr);
    }
    return r;
}

QString addUnlistedFiles(const QString &dir)
{
    QString err;
    QJsonObject manifest = readJson(dir + "/" + kManifest, &err);
    if (!err.isEmpty())
        return err;
    QJsonArray files = manifest.value("files").toArray();
    for (const QString &rel : unlistedIn(dir, manifest)) {
        QFile f(dir + "/" + rel);
        if (!f.open(QIODevice::ReadOnly))
            return "cannot read " + rel;
        files.append(QJsonObject{{"path", rel},
                                 {"role", roleFor(rel)},
                                 {"sha256", QString::fromLatin1(sha256Hex(f.readAll()))}});
    }
    manifest["files"] = files;
    QSaveFile mf(dir + "/" + kManifest);
    const QByteArray mb = jsonBytes(manifest);
    if (!mf.open(QIODevice::WriteOnly) || mf.write(mb) != mb.size() || !mf.commit())
        return "cannot write the manifest in " + dir;
    return {};
}

} // namespace tf::core
