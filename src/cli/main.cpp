#include "core/ApplicationInfo.h"
#include "core/Import.h"
#include "core/Profile.h"
#include "core/Results.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

// tf-analyse: TenForge's analysis without a window, for scripts and tests.
//
//   tf-analyse --profile iso-6892-1-2019 machine-export.csv
//
// Reads a machine's export, analyses it with a profile, prints the results
// with their conformity, and writes results.json / results.csv on request.
// Exit status: 0 when the analysis ran (results that could not be
// determined are reported, not errors), 2 for a usage or input error.

namespace {

QTextStream &out()
{
    static QTextStream s(stdout);
    return s;
}

QTextStream &err()
{
    static QTextStream s(stderr);
    return s;
}

int refuse(const QString &why)
{
    err() << "tf-analyse: " << why << "\n";
    err().flush();
    return 2;
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    if (path == "-") {
        out() << QString::fromUtf8(bytes);
        out().flush();
        return true;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(bytes) == bytes.size();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("tf-analyse"));
    QCoreApplication::setApplicationVersion(tf::core::applicationVersion());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("%1 - %2, headless analysis.")
            .arg(tf::core::applicationName(), tf::core::applicationSubtitle()));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"),
                                 QStringLiteral("A delimited text export from a testing machine."));
    const QCommandLineOption profileOpt("profile", "Profile id (see --list-profiles) or a profile file.", "id-or-path");
    const QCommandLineOption listOpt("list-profiles", "List the profiles shipped with TenForge.");
    const QCommandLineOption forceOpt("force", "Name of the force column (found by name if not given).", "column");
    const QCommandLineOption extOpt("extension", "Name of the extension column (found by name if not given).", "column");
    const QCommandLineOption forceUnitOpt("force-unit", "Unit of the force column, if the file does not say (N, kN).", "unit");
    const QCommandLineOption extUnitOpt("extension-unit", "Unit of the extension column, if the file does not say (mm, %).", "unit");
    const QCommandLineOption areaOpt("area", "Original cross-section S0 with its unit, e.g. \"120.6 mm2\".", "quantity");
    const QCommandLineOption gaugeOpt("gauge-length", "Extensometer gauge length with its unit, e.g. \"50 mm\".", "quantity");
    const QCommandLineOption jsonOpt("json", "Write results.json here (\"-\" for standard output).", "path");
    const QCommandLineOption csvOpt("csv", "Write results.csv here (\"-\" for standard output).", "path");
    parser.addOptions({profileOpt, listOpt, forceOpt, extOpt, forceUnitOpt, extUnitOpt, areaOpt,
                       gaugeOpt, jsonOpt, csvOpt});
    parser.process(app);

    if (parser.isSet(listOpt)) {
        for (const QString &id : tf::core::shippedProfiles()) {
            const auto p = tf::core::loadProfile(id);
            out() << id << "  " << (p.profile ? p.profile->title : p.error) << "\n";
        }
        return 0;
    }

    const QStringList files = parser.positionalArguments();
    if (files.size() != 1)
        return refuse("give exactly one file to analyse; see --help");
    if (!parser.isSet(profileOpt))
        return refuse("say which profile to analyse with, --profile <id>; shipped: "
                      + tf::core::shippedProfiles().join(", "));

    const auto prof = tf::core::loadProfile(parser.value(profileOpt));
    if (!prof.profile)
        return refuse(prof.error);
    const tf::core::Profile &profile = *prof.profile;

    const auto table = tf::core::readTable(files.front());
    if (!table.table)
        return refuse(table.error);

    tf::core::TensionMapping map;
    map.force = parser.value(forceOpt);
    map.extension = parser.value(extOpt);
    map.forceUnit = parser.value(forceUnitOpt);
    map.extensionUnit = parser.value(extUnitOpt);
    if (parser.isSet(areaOpt)) {
        map.area = tf::core::parseQuantity(parser.value(areaOpt), "area");
        if (!map.area)
            return refuse("--area needs a number and an area unit, e.g. \"120.6 mm2\"");
    }
    if (parser.isSet(gaugeOpt)) {
        map.gaugeLength = tf::core::parseQuantity(parser.value(gaugeOpt), "length");
        if (!map.gaugeLength)
            return refuse("--gauge-length needs a number and a length unit, e.g. \"50 mm\"");
    }
    const auto imp = tf::core::toTension(*table.table, map);
    if (!imp.error.isEmpty())
        return refuse(imp.error);

    const auto result = tf::analysis::analyseTension(imp.input, profile.method);
    const QJsonObject doc = tf::core::resultsJson(
        profile, result,
        {table.table->path, QString::fromLatin1(table.table->sha256), imp.forceColumn,
         imp.extensionColumn, imp.notes},
        QDateTime::currentDateTimeUtc());

    if (parser.isSet(jsonOpt)
        && !writeFile(parser.value(jsonOpt), QJsonDocument(doc).toJson(QJsonDocument::Indented)))
        return refuse("cannot write " + parser.value(jsonOpt));
    if (parser.isSet(csvOpt) && !writeFile(parser.value(csvOpt), tf::core::resultsCsv(doc).toUtf8()))
        return refuse("cannot write " + parser.value(csvOpt));
    if (parser.value(jsonOpt) == "-" || parser.value(csvOpt) == "-")
        return 0;  // standard output carries the document; no table around it

    // --- The human view ----------------------------------------------------------
    QTextStream &o = out();
    o << tf::core::applicationName() << " " << tf::core::applicationVersion() << " - "
      << tf::core::applicationSubtitle() << "\n";
    o << "Profile:  " << profile.id << " " << profile.profileVersion << " (" << profile.title << ")"
      << (profile.shipped ? "" : ", NOT shipped with TenForge") << "\n";
    o << "File:     " << table.table->path << "\n";
    o << "          sha256 " << table.table->sha256 << "\n";
    for (const QString &n : imp.notes)
        o << "          " << n << "\n";

    const QJsonObject conf = doc.value("conformity").toObject();
    o << "\nConformity: " << conf.value("status").toString().toUpper()
      << " (results follow the profile's method but are not to the standard):\n";
    for (const QJsonValue &v : conf.value("reasons").toArray()) {
        QString d = v.toObject().value("detail").toString();
        const int colon = d.indexOf(": results.");
        if (colon > 0)
            d = d.left(colon) + " (listed in results.json)";
        o << "  - " << d << "\n";
    }

    o << "\nResults\n";
    for (const QJsonValue &v : doc.value("results").toArray()) {
        const QJsonObject r = v.toObject();
        o << "  " << r.value("id").toString().leftJustified(10);
        if (r.value("status").toString() == "determined") {
            const QJsonObject rep = r.value("reported").toObject();
            o << (rep.value("value").toString() + " " + rep.value("unit").toString()).leftJustified(14)
              << r.value("how").toObject().value("primitive").toString();
        } else {
            o << "not determined: " << r.value("reason").toString();
        }
        o << "\n";
    }
    const QJsonArray flags = doc.value("flags").toArray();
    if (!flags.isEmpty()) {
        o << "\nFlags\n";
        for (const QJsonValue &f : flags)
            o << "  - " << f.toString() << "\n";
    }
    o.flush();
    return 0;
}
