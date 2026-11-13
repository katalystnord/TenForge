#include "core/ApplicationInfo.h"
#include "core/Import.h"
#include "core/Profile.h"
#include "core/Exports.h"
#include "core/Results.h"
#include "core/Session.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QTextStream>

// tf-analyse: TenForge's analysis without a window, for scripts and tests.
//
//   tf-analyse --profile iso-6892-1-2019 machine-export.csv
//
// Reads a machine's export, analyses it with a profile, prints the results
// with their conformity, and writes results.json / results.csv on request.
// It drives the same tf::core::Session the window does, so the two cannot
// analyse a file differently; a missing input is refused with the
// session's own words and the option that supplies it.
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
    const QCommandLineOption extOpt("extension", "Name of the extension, deformation or deflection column (found by name if not given).", "column");
    const QCommandLineOption forceUnitOpt("force-unit", "Unit of the force column, if the file does not say (N, kN).", "unit");
    const QCommandLineOption extUnitOpt("extension-unit", "Unit of the extension column, if the file does not say (mm, %).", "unit");
    const QCommandLineOption areaOpt("area", "Original cross-section S0 with its unit, e.g. \"120.6 mm2\".", "quantity");
    const QCommandLineOption gaugeOpt("gauge-length", "Length the extension or compression is measured over, with its unit: the extensometer's gauge length, or a compression specimen's height between the platens, e.g. \"50 mm\".", "quantity");
    const QCommandLineOption widthOpt("width", "Specimen width, e.g. \"10 mm\" (rectangular specimens, and every bend specimen).", "quantity");
    const QCommandLineOption thicknessOpt("thickness", "Specimen thickness, e.g. \"4 mm\".", "quantity");
    const QCommandLineOption spanOpt("span", "Bending: the support span L, e.g. \"64 mm\".", "quantity");
    const QCommandLineOption loadingSpanOpt("loading-span", "Four-point bending: the distance between the loading points.", "quantity");
    const QCommandLineOption deflectionAtOpt("deflection-at", "Four-point bending: where the deflection was measured, loading-points (the crosshead) or mid-span.", "where");
    const QCommandLineOption complianceOpt("compliance", "A table of force against the machine's own displacement, taken out of the displacement (compression and bending).", "file");
    const QCommandLineOption jsonOpt("json", "Write results.json here (\"-\" for standard output).", "path");
    const QCommandLineOption csvOpt("csv", "Write results.csv here (\"-\" for standard output).", "path");
    const QCommandLineOption freecadOpt("freecad", "Tension: write a FreeCAD material card <name>.FCMat, FEM yield points <name>-yieldpoints.txt and a CalculiX block <name>-plastic.inp (true stress against true plastic strain, yield to Rm).", "name");
    const QCommandLineOption vtpOpt("vtp", "Write the analysed curve as VTK XML PolyData (.vtp), for ParaView and PyVista.", "path");
    parser.addOptions({profileOpt, listOpt, forceOpt, extOpt, forceUnitOpt, extUnitOpt, areaOpt,
                       gaugeOpt, widthOpt, thicknessOpt, spanOpt, loadingSpanOpt, deflectionAtOpt,
                       complianceOpt, jsonOpt, csvOpt, freecadOpt, vtpOpt});
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

    tf::core::Session session;
    session.profile = parser.value(profileOpt);
    if (const QString e = session.openData(files.front()); !e.isEmpty())
        return refuse(e);
    const tf::core::Table &table = *session.table();

    if (parser.isSet(forceOpt))
        session.mapping.forceColumn = parser.value(forceOpt);
    if (parser.isSet(extOpt))
        session.mapping.extensionColumn = parser.value(extOpt);
    if (parser.isSet(forceUnitOpt))
        session.mapping.forceUnit = parser.value(forceUnitOpt);
    if (parser.isSet(extUnitOpt))
        session.mapping.extensionUnit = parser.value(extUnitOpt);

    auto length = [&](const QCommandLineOption &o, std::optional<double> &into) -> QString {
        if (!parser.isSet(o))
            return {};
        into = tf::core::parseQuantity(parser.value(o), "length");
        return into ? QString() : "--" + o.names().front() + " needs a number and a length unit, e.g. \"50 mm\"";
    };
    if (parser.isSet(areaOpt)) {
        session.specimen.shape = tf::core::Specimen::Shape::GivenArea;
        session.specimen.area = tf::core::parseQuantity(parser.value(areaOpt), "area");
        if (!session.specimen.area)
            return refuse("--area needs a number and an area unit, e.g. \"120.6 mm2\"");
    }
    if (parser.isSet(widthOpt) || parser.isSet(thicknessOpt)) {
        if (parser.isSet(areaOpt))
            return refuse("give --area, or --width and --thickness, not both");
        session.specimen.shape = tf::core::Specimen::Shape::Rectangular;
    }
    for (const QString &e : {length(widthOpt, session.specimen.width),
                             length(thicknessOpt, session.specimen.thickness),
                             length(gaugeOpt, session.specimen.gaugeLength),
                             length(spanOpt, session.fixture.span),
                             length(loadingSpanOpt, session.fixture.loadingSpan)})
        if (!e.isEmpty())
            return refuse(e);
    if (parser.isSet(deflectionAtOpt)) {
        const QString at = parser.value(deflectionAtOpt);
        if (at == "loading-points")
            session.fixture.deflectionAt = tf::core::Fixture::DeflectionAt::LoadingPoints;
        else if (at == "mid-span")
            session.fixture.deflectionAt = tf::core::Fixture::DeflectionAt::Midspan;
        else
            return refuse("--deflection-at is loading-points or mid-span");
    }
    if (parser.isSet(complianceOpt))
        if (const QString e = session.setCompliance(parser.value(complianceOpt)); !e.isEmpty())
            return refuse(e);

    const tf::core::Step step = session.next();
    if (!step.ready) {
        static const QMap<QString, QString> option{
            {"forceColumn", "--force"},
            {"forceUnit", "--force-unit"},
            {"extensionColumn", "--extension"},
            {"extensionUnit", "--extension-unit"},
            {"specimen", "--area, or --width and --thickness"},
            {"gaugeLength", "--gauge-length"},
            {"span", "--span"},
            {"loadingSpan", "--loading-span"},
            {"deflectionAt", "--deflection-at loading-points or mid-span"},
            {"compliance", "--compliance"},
        };
        return refuse(QString("give %1: %2").arg(option.value(step.field, "the missing input"), step.text));
    }
    if (!session.analyse())
        return refuse("the analysis did not run: " + session.next().text);
    const QJsonObject doc = session.analysis()->doc;
    QStringList notes;
    for (const QJsonValue &v : doc.value("source").toObject().value("notes").toArray())
        notes << v.toString();

    if (parser.isSet(jsonOpt)
        && !writeFile(parser.value(jsonOpt), QJsonDocument(doc).toJson(QJsonDocument::Indented)))
        return refuse("cannot write " + parser.value(jsonOpt));
    if (parser.isSet(csvOpt) && !writeFile(parser.value(csvOpt), tf::core::resultsCsv(doc).toUtf8()))
        return refuse("cannot write " + parser.value(csvOpt));
    if (parser.isSet(freecadOpt)) {
        const QString base = parser.value(freecadOpt);
        QString error;
        const QString card = tf::core::freecadCard(*session.analysis(), session.specimen, &error);
        if (card.isEmpty())
            return refuse("no FreeCAD card: " + error);
        if (!writeFile(base + ".FCMat", card.toUtf8()))
            return refuse("cannot write " + base + ".FCMat");
        const tf::core::Hardening h = tf::core::hardeningCurve(*session.analysis());
        if (h.error.isEmpty()) {
            if (!writeFile(base + "-yieldpoints.txt", (tf::core::yieldPoints(h).join('\n') + '\n').toUtf8())
                || !writeFile(base + "-plastic.inp",
                              tf::core::calculixPlastic(h, *session.analysis()).toUtf8()))
                return refuse("cannot write the yield points beside " + base + ".FCMat");
        } else {
            err() << "tf-analyse: card written; no hardening curve: " << h.error << "\n";
        }
    }
    if (parser.isSet(vtpOpt) && !writeFile(parser.value(vtpOpt), tf::core::curveVtp(*session.analysis()).toUtf8()))
        return refuse("cannot write " + parser.value(vtpOpt));
    if (parser.value(jsonOpt) == "-" || parser.value(csvOpt) == "-")
        return 0;  // standard output carries the document; no table around it

    // --- The human view ----------------------------------------------------------
    QTextStream &o = out();
    o << tf::core::applicationName() << " " << tf::core::applicationVersion() << " - "
      << tf::core::applicationSubtitle() << "\n";
    o << "Profile:  " << profile.id << " " << profile.profileVersion << " (" << profile.title << ")"
      << (profile.shipped ? "" : ", NOT shipped with TenForge") << "\n";
    o << "File:     " << table.path << "\n";
    o << "          sha256 " << table.sha256 << "\n";
    for (const QString &n : notes)
        o << "          " << n << "\n";

    const QJsonObject conf = doc.value("conformity").toObject();
    o << "\nConformity: " << conf.value("status").toString().toUpper()
      << " (results follow the profile's method but are not to the standard):\n";
    for (const QJsonValue &v : conf.value("reasons").toArray()) {
        QString d = v.toObject().value("detail").toString();
        const int colon = d.indexOf("standard's text: ");
        if (colon > 0)
            d = d.left(colon + int(QString("standard's text").size())) + " (listed in results.json)";
        o << "  - " << d << "\n";
    }

    o << "\nResults\n";
    for (const QJsonValue &v : doc.value("results").toArray()) {
        const QJsonObject r = v.toObject();
        o << "  " << r.value("id").toString().leftJustified(12);
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
