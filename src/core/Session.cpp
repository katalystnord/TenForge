#include "core/Session.h"

#include "core/Record.h"
#include "core/Results.h"

#include "analysis/Models.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>

#include <algorithm>
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
    m_unlisted.clear();
    m_analysis.reset();
    mapping = {};
    specimen = {};  // a new file is a new specimen; the procedure is kept
    modulusWindow.reset();
    curveNotes.clear();

    QString why;
    mapping.forceColumn = guessForceColumn(*m_table, &why);
    if (!mapping.forceColumn.isEmpty())
        m_notes << "\"" + mapping.forceColumn + "\" taken as force by its name";
    mapping.extensionColumn = guessExtensionColumn(*m_table, &why);
    if (!mapping.extensionColumn.isEmpty())
        m_notes << "\"" + mapping.extensionColumn + "\" taken as the "
                       + (profile.isEmpty() ? QString("displacement") : displacementWord())
                       + " by its name";
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
    // Width, thickness and span as machine software states them (ZwickRoell
    // testXpert writes German keys). A cross-section S0 stated outright
    // wins over the dimensions.
    if (!specimen.area) {
        const auto w = preambleQuantity(*m_table, {"Probenbreite", "Width", "width", "Specimen width"},
                                        "length", &key);
        const QString wKey = key;
        const auto h = preambleQuantity(*m_table, {"Probendicke", "Thickness", "thickness",
                                                   "Specimen thickness"},
                                        "length", &key);
        if (w && h) {
            specimen.shape = Specimen::Shape::Rectangular;
            specimen.width = *w;
            specimen.thickness = *h;
            m_notes << "width " + wKey + " = " + mm(*w) + " mm and thickness " + key + " = " + mm(*h)
                           + " mm, as the file states";
        }
    }
    if (const auto L = preambleQuantity(*m_table, {QString::fromUtf8("Stützweite"), "Span",
                                                   "span", "Support span"},
                                        "length", &key)) {
        fixture.span = *L;
        m_notes << "support span " + key + " = " + mm(*L) + " mm, as the file states";
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
    m_unlisted = o.unlisted;
    mapping = {o.forceColumn, o.extensionColumn, o.forceUnit, o.extensionUnit};
    specimen = o.specimen;
    fixture = o.fixture;
    modulusWindow = o.modulusWindow;
    curveNotes = o.curveNotes;
    setCompliance({});
    if (!o.complianceFile.isEmpty()) {
        const QString err = setCompliance(o.dir + "/" + o.complianceFile);
        if (!err.isEmpty())
            m_notices << "The record's compliance table cannot be used: " + err + ".";
    }
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

QString Session::mode() const
{
    if (profile.isEmpty())
        return {};
    const auto p = loadProfile(profile);
    return p.profile ? p.profile->mode : QString();
}

QString Session::fixtureKind() const
{
    if (profile.isEmpty())
        return {};
    const auto p = loadProfile(profile);
    return p.profile ? p.profile->fixture : QString();
}

QString Session::displacementWord() const
{
    const QString m = mode();
    return m == "flexure" ? "deflection" : m == "compression" ? "deformation" : "extension";
}

QString Session::setCompliance(const QString &path)
{
    if (path.isEmpty()) {
        m_compliancePath.clear();
        m_complianceForce.clear();
        m_complianceDisplacement.clear();
        m_analysis.reset();
        return {};
    }
    const auto t = readTable(path);
    if (!t.table)
        return t.error;
    TensionMapping m;
    m.needsGeometry = false;
    const TensionImport imp = toTension(*t.table, m);
    if (!imp.error.isEmpty())
        return "the compliance table: " + imp.error;
    if (imp.extensionIsStrain)
        return "the compliance table's displacement must be a length, not a strain";
    m_compliancePath = path;
    m_complianceForce = imp.input.force;
    m_complianceDisplacement = imp.input.extension;
    m_analysis.reset();
    return {};
}

Step Session::next() const
{
    if (profile.isEmpty())
        return {false, "profile",
                "Choose the procedure: the standard your test follows, for tension, compression "
                "or bending."};
    const auto p = loadProfile(profile);
    if (!p.profile)
        return {false, "profile", sentence(p.error)};
    const QString md = p.profile->mode;

    if (!m_table)
        return {false, "data",
                "Open a machine export: a CSV or text file from your testing machine "
                "(File > Open data)."};

    const QStringList &names = m_table->names;
    const QString word = displacementWord();
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
                "Choose which column is the " + word + " or strain ("
                    + QString(why).replace("extension", word) + ")."};
    }
    const bool strain = bool(siFactor(mapping.extensionUnit, "strain"));
    if (!strain && !siFactor(mapping.extensionUnit, "length"))
        return {false, "extensionUnit",
                "Choose the unit of the " + word + " column \"" + mapping.extensionColumn
                    + "\" (a length such as mm, or a strain in %)."};

    if (md == "flexure") {
        if (specimen.shape != Specimen::Shape::Rectangular)
            return {false, "specimen",
                    "Bending needs a rectangular specimen: choose Rectangular and enter its width "
                    "and thickness."};
        const auto area = crossSection(specimen);
        if (!area.area)
            return {false, "specimen", sentence(area.reason)};
        if (!fixture.span)
            return {false, "span", "Enter the support span L, the distance between the supports."};
        if (!(*fixture.span > 0))
            return {false, "span", "The support span must be greater than zero."};
        if (p.profile->fixture == "four-point") {
            if (!fixture.loadingSpan)
                return {false, "loadingSpan",
                        "Enter the loading span, the distance between the two loading points."};
            if (!(*fixture.loadingSpan > 0) || !(*fixture.loadingSpan < *fixture.span))
                return {false, "loadingSpan",
                        "The loading span must be greater than zero and shorter than the support "
                        "span."};
            if (!strain && !fixture.deflectionAt)
                return {false, "deflectionAt",
                        "Say where the deflection was measured: at the loading points (the "
                        "crosshead, as most machines record it) or at mid-span (a deflectometer "
                        "under the beam)."};
        }
    } else {
        const auto area = crossSection(specimen);
        if (!area.area)
            return {false, "specimen", sentence(area.reason)};
        const QString lengthText =
            md == "compression"
                ? "Enter L0, the length the deformation is measured over: the specimen's height "
                  "between the platens, or the extensometer's gauge length."
                : "Enter the extensometer gauge length Le, which turns extension into strain.";
        if (!strain && !specimen.gaugeLength)
            return {false, "gaugeLength", lengthText};
        if (!strain && !(*specimen.gaugeLength > 0))
            return {false, "gaugeLength",
                    md == "compression" ? QString("L0 must be greater than zero.")
                                        : QString("The gauge length must be greater than zero.")};
    }

    if (modulusWindow
        && !(std::isfinite(modulusWindow->first) && std::isfinite(modulusWindow->second)
             && modulusWindow->first < modulusWindow->second))
        return {false, "modulusWindow",
                "The modulus window must start at a lower strain than it ends."};

    if (md != "tension" && !m_compliancePath.isEmpty()) {
        const Prepared pr = prepareCurve(*p.profile);
        if (!pr.error.isEmpty())
            return {false, "compliance", sentence(pr.error)};
    }

    return {true, {},
            "Analysed. The results update as you change anything on the left; save a report "
            "with File > Export report."};
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

Session::Prepared Session::prepareCurve(const Profile &p) const
{
    Prepared out;
    TensionMapping tm = tensionMapping();
    tm.needsGeometry = false;
    out.imported = toTension(*m_table, tm);
    if (!out.imported.error.isEmpty()) {
        out.error = out.imported.error;
        return out;
    }
    const auto &F = out.imported.input.force;
    const auto &D = out.imported.input.extension;  // m, or strain
    const std::size_t n = F.size();
    if (n == 0) {
        out.error = "the file has no samples";
        return out;
    }

    // Many machines record compression and bending as negative. The test's
    // own direction is taken as positive, and the note says so; the record
    // keeps the file's signs.
    double fmin = 0, fmax = 0;
    std::size_t iAbs = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isfinite(F[i]))
            continue;
        fmin = std::min(fmin, F[i]);
        fmax = std::max(fmax, F[i]);
        if (std::fabs(F[i]) > std::fabs(F[iAbs]) || !std::isfinite(F[iAbs]))
            iAbs = i;
    }
    const double fs = -fmin > fmax ? -1.0 : 1.0;
    const double ds = std::isfinite(D[iAbs]) && D[iAbs] < 0 ? -1.0 : 1.0;
    if (fs < 0)
        out.notes << "the force is negative in the file; the test's direction is taken as positive";
    if (ds < 0)
        out.notes << "the " + displacementWord()
                         + " is negative in the file; the test's direction is taken as positive";

    std::vector<double> force(n), disp(n);
    for (std::size_t i = 0; i < n; ++i) {
        force[i] = fs * F[i];
        disp[i] = ds * D[i];
    }
    if (!m_compliancePath.isEmpty()) {
        if (out.imported.extensionIsStrain) {
            out.error = "a compliance table corrects a displacement; the " + displacementWord()
                        + " column is a strain";
            return out;
        }
        const auto c = tf::analysis::complianceCorrected(force, disp, m_complianceForce,
                                                         m_complianceDisplacement);
        if (!c) {
            out.error = QString::fromStdString(c.reason);
            return out;
        }
        disp = *c;
        out.notes << "the machine's compliance from " + QFileInfo(m_compliancePath).fileName()
                         + " is taken out of the " + displacementWord();
    }

    double ks = 0, ke = 0;
    if (p.mode == "flexure") {
        tf::analysis::FlexureGeometry g;
        g.width = specimen.width.value_or(0);
        g.thickness = specimen.thickness.value_or(0);
        g.span = fixture.span.value_or(0);
        g.loadingSpan = p.fixture == "four-point" ? fixture.loadingSpan.value_or(0) : 0.0;
        g.deflectionAt = fixture.deflectionAt == Fixture::DeflectionAt::Midspan
                             ? tf::analysis::FlexureGeometry::DeflectionAt::Midspan
                             : tf::analysis::FlexureGeometry::DeflectionAt::LoadingPoints;
        const auto a = tf::analysis::flexureStressFactor(g);
        const auto b = tf::analysis::flexureStrainFactor(g);
        if (!a || !b) {
            out.error = QString::fromStdString(!a ? a.reason : b.reason);
            return out;
        }
        ks = *a;
        ke = out.imported.extensionIsStrain ? 1.0 : *b;
        if (p.fixture == "four-point" && !out.imported.extensionIsStrain)
            out.notes << QString("deflection taken as measured at the ")
                             + (g.deflectionAt == tf::analysis::FlexureGeometry::DeflectionAt::Midspan
                                    ? "mid-span"
                                    : "loading points");
    } else {
        const auto area = crossSection(specimen);
        if (!area.area) {
            out.error = area.reason;
            return out;
        }
        ks = 1.0 / *area.area;
        ke = out.imported.extensionIsStrain ? 1.0 : 1.0 / specimen.gaugeLength.value_or(NAN);
    }
    out.input.force = force;
    out.input.stress.resize(n);
    out.input.strain.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.input.stress[i] = force[i] * ks;
        out.input.strain[i] = disp[i] * ke;
    }
    return out;
}

QJsonArray Session::validity(const Profile &p) const
{
    auto value = [&](const QString &q) -> std::optional<double> {
        if (q == "width")
            return specimen.width;
        if (q == "thickness")
            return specimen.thickness;
        if (q == "span")
            return fixture.span;
        if (q == "loading_span")
            return fixture.loadingSpan;
        if (q == "length")
            return specimen.gaugeLength;
        return std::nullopt;
    };
    QJsonArray out;
    for (const ValidityCheck &c : p.validity) {
        const auto a = value(c.numerator), b = value(c.denominator);
        QJsonObject o{{"id", c.id}};
        if (!a || !b || !(*b > 0)) {
            o["status"] = "not-checked";
            o["detail"] = c.id + ": the " + (!a ? c.numerator : c.denominator)
                          + " is not known, so " + c.numerator + " / " + c.denominator
                          + " was not checked.";
        } else {
            const double r = *a / *b;
            const bool met = r >= c.min && r <= c.max;
            o["status"] = met ? "met" : "not-met";
            o["value"] = r;
            o["detail"] = QString("%1: %2 / %3 is %4; the procedure asks for %5 to %6.")
                              .arg(c.id, c.numerator, c.denominator)
                              .arg(r, 0, 'g', 4)
                              .arg(c.min, 0, 'g', 4)
                              .arg(c.max, 0, 'g', 4);
        }
        out.append(o);
    }
    return out;
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

    QStringList notes = m_notes;
    if (modulusWindow) {
        // The user's window replaces the procedure's, on the strain as
        // plotted.
        a.profile.method.modulusAxis = tf::analysis::TensionMethod::Axis::Strain;
        a.profile.method.modulusFrom = modulusWindow->first;
        a.profile.method.modulusTo = modulusWindow->second;
        a.profile.curve.modulus = tf::analysis::CurveMethod::Modulus::StrainWindow;
        a.profile.curve.modulusFrom = modulusWindow->first;
        a.profile.curve.modulusTo = modulusWindow->second;
        a.profile.curve.windowOnRecordedStrain = true;
    }
    if (a.profile.mode == "tension") {
        a.imported = toTension(*m_table, tensionMapping());
        if (!a.imported.error.isEmpty())
            return false;
        a.result = tf::analysis::analyseTension(a.imported.input, a.profile.method);
        const auto &in = a.imported.input;
        for (std::size_t i = 0; i < in.force.size(); ++i) {
            a.stress.push_back(in.force[i] / in.area);
            a.strain.push_back(in.extension[i] / in.gaugeLength);
        }
        notes += a.imported.notes;
    } else {
        Prepared pr = prepareCurve(a.profile);
        if (!pr.error.isEmpty())
            return false;
        a.imported = std::move(pr.imported);
        tf::analysis::CurveMethod m = a.profile.curve;
        // A stress at a deflection (ISO 178's 1.5 h) becomes a strain for
        // this specimen: the model's strain per deflection times the
        // deflection.
        for (const ProfileResult &r : a.profile.results) {
            if (r.deflectionPerThickness <= 0)
                continue;
            tf::analysis::FlexureGeometry g;
            g.width = specimen.width.value_or(0);
            g.thickness = specimen.thickness.value_or(0);
            g.span = fixture.span.value_or(0);
            g.loadingSpan = a.profile.fixture == "four-point" ? fixture.loadingSpan.value_or(0) : 0.0;
            if (fixture.deflectionAt == Fixture::DeflectionAt::Midspan)
                g.deflectionAt = tf::analysis::FlexureGeometry::DeflectionAt::Midspan;
            const auto k = tf::analysis::flexureStrainFactor(g);
            for (auto &q : m.requests)
                if (q.id == r.id.toStdString() && k)
                    q.at = *k * r.deflectionPerThickness * g.thickness;
        }
        a.result = tf::analysis::analyseCurve(pr.input, m);
        a.stress = pr.input.stress;
        a.strain = pr.input.strain;
        notes += a.imported.notes + pr.notes;
    }
    a.doc = resultsJson(a.profile, a.result,
                        {m_table->path, QString::fromLatin1(m_table->sha256),
                         a.imported.forceColumn, a.imported.extensionColumn, notes},
                        QDateTime::currentDateTimeUtc(), validity(a.profile));
    if (modulusWindow) {
        const auto pct = [](double v) { return QString::number(v * 100.0, 'g', 6) + " %"; };
        QJsonObject conf = a.doc.value("conformity").toObject();
        QJsonArray reasons = conf.value("reasons").toArray();
        reasons.append(QJsonObject{
            {"code", "user-modulus-window"},
            {"detail", "The modulus was fitted between " + pct(modulusWindow->first) + " and "
                           + pct(modulusWindow->second)
                           + " strain (as plotted), a window chosen by the user instead of "
                             "the procedure's."}});
        conf["reasons"] = reasons;
        a.doc["conformity"] = conf;
        a.doc["analysis_choices"] = QJsonObject{
            {"modulus_window", QJsonObject{{"from", modulusWindow->first},
                                           {"to", modulusWindow->second},
                                           {"axis", "strain"},
                                           {"on", "strain as plotted"}}}};
    }
    m_analysis = std::move(a);
    return true;
}

QString Session::saveRecord(const QString &dir) const
{
    if (!m_analysis)
        return "there is no analysis to save yet: " + next().text;
    return writeRecord(dir, *m_table, m_analysis->imported, specimen, m_analysis->profile,
                       m_analysis->doc, fixture, m_compliancePath, modulusWindow, curveNotes);
}

} // namespace tf::core
