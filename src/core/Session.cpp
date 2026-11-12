#include "core/Session.h"

#include "core/Record.h"
#include "core/Results.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>

#include <cmath>

namespace tf::core {

namespace {

QString sentence(QString s)
{
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    if (!s.endsWith('.'))
        s += '.';
    return s;
}

QString mm(double metres) { return QString::number(metres * 1e3, 'g', 8); }

bool sameSamples(const std::vector<double> &a, const std::vector<double> &b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!(a[i] == b[i] || (std::isnan(a[i]) && std::isnan(b[i]))))
            return false;
    return true;
}

QString shownValue(const QJsonObject &r)
{
    if (r.value("status").toString() != "determined")
        return "not determined";
    const QJsonObject rep = r.value("reported").toObject();
    return rep.value("value").toString() + " " + rep.value("unit").toString();
}

} // namespace

QString Session::openData(const QString &path)
{
    auto t = readTable(path);
    if (!t.table)
        return t.error;
    m_table = std::move(t.table);
    m_recordDir.clear();
    m_notes.clear();
    m_notices.clear();
    m_analysis.reset();
    mapping = {};
    specimen = {};  // a new file is a new specimen; the procedure is kept

    QString why;
    mapping.forceColumn = guessForceColumn(*m_table, &why);
    if (!mapping.forceColumn.isEmpty())
        m_notes << "\"" + mapping.forceColumn + "\" taken as force by its name";
    mapping.extensionColumn = guessExtensionColumn(*m_table, &why);
    if (!mapping.extensionColumn.isEmpty())
        m_notes << "\"" + mapping.extensionColumn + "\" taken as extension by its name";
    for (auto [col, unit] : {std::pair{&mapping.forceColumn, &mapping.forceUnit},
                             std::pair{&mapping.extensionColumn, &mapping.extensionUnit}}) {
        if (col->isEmpty())
            continue;
        *unit = columnUnit(*m_table, *col);
        if (!unit->isEmpty())
            m_notes << "unit of \"" + *col + "\" is " + *unit + ", as the file states";
    }

    QString key;
    if (const auto a = preambleQuantity(*m_table, {"So", "S0", "So [mm2]"}, "area", &key)) {
        specimen.shape = Specimen::Shape::GivenArea;
        specimen.area = *a;
        m_notes << "cross-section area " + key + " = " + QString::number(*a * 1e6, 'g', 8)
                       + " mm2, as the file states";
    }
    if (const auto le = preambleQuantity(*m_table, {"Le"}, "length", &key)) {
        specimen.gaugeLength = *le;
        m_notes << "extensometer gauge length " + key + " = " + mm(*le) + " mm, as the file states";
    }
    return {};
}

QString Session::openRecord(const QString &dirOrManifest)
{
    const OpenedRecord o = tf::core::openRecord(dirOrManifest);
    if (!o.error.isEmpty())
        return o.error;
    if (o.sourceFile.isEmpty())
        return "the record has no imported source file; records of live tests arrive with a "
               "later release";
    auto t = readTable(o.dir + "/" + o.sourceFile);
    if (!t.table)
        return "the record's source file cannot be read: " + t.error;

    m_table = std::move(t.table);
    m_recordDir = o.dir;
    m_analysis.reset();
    m_notes = {"opened from the record " + o.dir};
    m_notices.clear();
    mapping = {o.forceColumn, o.extensionColumn, o.forceUnit, o.extensionUnit};
    specimen = o.specimen;
    // Paths begin these sentences and are kept exactly as written.
    for (const QString &p : o.problems)
        m_notices << p + ".";
    for (const QString &u : o.unlisted)
        m_notices << u + " is in the record's folder but not in its manifest, so something "
                         "other than TenForge put it there.";

    profile.clear();
    if (shippedProfiles().contains(o.profileId)) {
        profile = o.profileId;
        const auto p = loadProfile(o.profileId);
        if (p.profile && p.profile->sha256 != o.profileSha256)
            m_notices << "The record was analysed with " + o.profileId + " version "
                             + o.profileVersion + "; the one shipped with this TenForge (version "
                             + p.profile->profileVersion
                             + ") differs, and the results shown are from it.";
    } else {
        m_notices << "The record was analysed with the profile \"" + o.profileId
                         + "\", which is not shipped with this TenForge; choose a procedure.";
    }

    if (!analyse())
        return {};
    const auto &in = m_analysis->imported.input;
    if (!sameSamples(in.force, o.input.force) || !sameSamples(in.extension, o.input.extension))
        m_notices << "The record's channel files no longer match its source file.";

    QJsonArray fresh = m_analysis->doc.value("results").toArray();
    for (const QJsonValue &sv : o.results.value("results").toArray()) {
        const QJsonObject stored = sv.toObject();
        const QString id = stored.value("id").toString();
        for (const QJsonValue &fv : fresh) {
            const QJsonObject now = fv.toObject();
            if (now.value("id").toString() != id)
                continue;
            if (shownValue(stored) != shownValue(now))
                m_notices << id + ": the record holds " + shownValue(stored)
                                 + ", a fresh analysis gives " + shownValue(now) + ".";
        }
    }
    return {};
}

Step Session::next() const
{
    if (!m_table)
        return {false, "data",
                "Open a machine export: a CSV or text file from your testing machine "
                "(File > Open data)."};

    const QStringList &names = m_table->names;
    QString why;
    if (!names.contains(mapping.forceColumn)) {
        guessForceColumn(*m_table, &why);
        return {false, "forceColumn",
                "Choose which column is the force (" + why + ")."};
    }
    if (!siFactor(mapping.forceUnit, "force"))
        return {false, "forceUnit",
                "Choose the unit of the force column \"" + mapping.forceColumn
                    + "\"; the file does not say."};
    if (!names.contains(mapping.extensionColumn)) {
        guessExtensionColumn(*m_table, &why);
        return {false, "extensionColumn",
                "Choose which column is the extension or strain (" + why + ")."};
    }
    const bool strain = bool(siFactor(mapping.extensionUnit, "strain"));
    if (!strain && !siFactor(mapping.extensionUnit, "length"))
        return {false, "extensionUnit",
                "Choose the unit of the extension column \"" + mapping.extensionColumn
                    + "\" (a length such as mm, or a strain in %)."};

    const auto area = crossSection(specimen);
    if (!area.area)
        return {false, "specimen", sentence(area.reason)};
    if (!strain && !specimen.gaugeLength)
        return {false, "gaugeLength",
                "Enter the extensometer gauge length Le, which turns extension into strain."};
    if (!strain && !(*specimen.gaugeLength > 0))
        return {false, "gaugeLength", "The gauge length must be greater than zero."};

    if (profile.isEmpty())
        return {false, "profile", "Choose the procedure to analyse with."};
    const auto p = loadProfile(profile);
    if (!p.profile)
        return {false, "profile", sentence(p.error)};

    return {true, {},
            "Analysed. The results update as you change anything above; save a report with "
            "File > Export report."};
}

TensionMapping Session::tensionMapping() const
{
    TensionMapping m;
    m.force = mapping.forceColumn;
    m.extension = mapping.extensionColumn;
    m.forceUnit = mapping.forceUnit;
    m.extensionUnit = mapping.extensionUnit;
    m.area = crossSection(specimen).area;
    m.gaugeLength = specimen.gaugeLength;
    return m;
}

bool Session::analyse()
{
    m_analysis.reset();
    if (!next().ready)
        return false;
    auto p = loadProfile(profile);
    if (!p.profile)
        return false;
    Analysis a;
    a.profile = std::move(*p.profile);
    a.imported = toTension(*m_table, tensionMapping());
    if (!a.imported.error.isEmpty())
        return false;
    a.result = tf::analysis::analyseTension(a.imported.input, a.profile.method);
    a.doc = resultsJson(a.profile, a.result,
                        {m_table->path, QString::fromLatin1(m_table->sha256),
                         a.imported.forceColumn, a.imported.extensionColumn,
                         m_notes + a.imported.notes},
                        QDateTime::currentDateTimeUtc());
    m_analysis = std::move(a);
    return true;
}

QString Session::saveRecord(const QString &dir) const
{
    if (!m_analysis)
        return "there is no analysis to save yet: " + next().text;
    return writeRecord(dir, *m_table, m_analysis->imported, specimen, m_analysis->profile,
                       m_analysis->doc);
}

} // namespace tf::core
