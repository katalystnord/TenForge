#include "core/References.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <map>

namespace tf::core {

namespace {

const std::map<QString, double> &strainUnits()
{
    static const std::map<QString, double> u{{"%", 1e-2}, {"1", 1.0}};
    return u;
}

const std::map<QString, double> &stressUnits()
{
    static const std::map<QString, double> u{
        {"MPa", 1e6}, {"N/mm2", 1e6}, {"GPa", 1e9}, {"kPa", 1e3}, {"Pa", 1.0}};
    return u;
}

struct Parsed {
    QJsonArray series;
    QByteArray sha256;
};

std::optional<Parsed> parse(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = "cannot read " + path;
        return std::nullopt;
    }
    const QByteArray bytes = f.readAll();
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.object().value("series").isArray()) {
        if (error)
            *error = path + " is not a PlotTracer JSON export (no list of \"series\")";
        return std::nullopt;
    }
    return Parsed{doc.object().value("series").toArray(),
                  QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()};
}

QString unitIn(const QString &label)
{
    static const QRegularExpression br(R"([\(\[]\s*([^\)\]]+?)\s*[\)\]])");
    const auto m = br.match(label);
    if (!m.hasMatch())
        return {};
    const QString u = m.captured(1);
    if (u == "mm/mm" || u == "m/m" || u == "-")
        return "1";
    if (u == "N/mm²")
        return "N/mm2";
    return u;
}

} // namespace

QStringList referenceStrainUnits() { return {"%", "1"}; }
QStringList referenceStressUnits() { return {"MPa", "N/mm2", "GPa", "kPa", "Pa"}; }

std::optional<PlotTracerFile> readPlotTracer(const QString &path, QString *error)
{
    const auto p = parse(path, error);
    if (!p)
        return std::nullopt;
    PlotTracerFile out;
    out.path = path;
    out.sha256 = p->sha256;
    for (const QJsonValue &v : p->series) {
        const QJsonObject o = v.toObject();
        PlotTracerSeries s;
        s.name = o.value("name").toString();
        s.relation = o.value("relation").toObject().value("role").toString();
        const QJsonArray pts = o.value("points").toArray();
        s.points = int(pts.size());
        for (const QJsonValue &pv : pts)
            for (const QString &k : pv.toObject().keys())
                if (!s.columns.contains(k))
                    s.columns << k;
        out.series.push_back(s);
    }
    return out;
}

ReferenceColumns guessReferenceColumns(const PlotTracerSeries &s)
{
    static const QRegularExpression strainRe(
        R"(strain|extension|elongation|deformation|deflection|dehnung|ε|%)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression stressRe(R"(stress|spannung|σ|Pa\b|N/mm)",
                                             QRegularExpression::CaseInsensitiveOption);
    ReferenceColumns g;
    for (const QString &c : s.columns) {
        if (g.strainColumn.isEmpty() && strainRe.match(c).hasMatch() && !stressRe.match(c).hasMatch())
            g.strainColumn = c;
        else if (g.stressColumn.isEmpty() && stressRe.match(c).hasMatch())
            g.stressColumn = c;
    }
    const QString su = unitIn(g.strainColumn), tu = unitIn(g.stressColumn);
    g.strainUnit = strainUnits().count(su) ? su : QString();
    g.stressUnit = stressUnits().count(tu) ? tu : QString();
    return g;
}

std::optional<ReferenceCurve> readReferenceCurve(const QString &path, const QString &series,
                                                 const QString &strainColumn,
                                                 const QString &strainUnit,
                                                 const QString &stressColumn,
                                                 const QString &stressUnit, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return std::optional<ReferenceCurve>();
    };
    const auto p = parse(path, error);
    if (!p)
        return std::nullopt;
    const auto su = strainUnits().find(strainUnit);
    if (su == strainUnits().end())
        return fail("\"" + strainUnit + "\" is not a strain unit TenForge reads ("
                    + referenceStrainUnits().join(", ") + ")");
    const auto tu = stressUnits().find(stressUnit);
    if (tu == stressUnits().end())
        return fail("\"" + stressUnit + "\" is not a stress unit TenForge reads ("
                    + referenceStressUnits().join(", ") + ")");

    QStringList names;
    for (const QJsonValue &v : p->series) {
        const QJsonObject o = v.toObject();
        names << o.value("name").toString();
        if (o.value("name").toString() != series)
            continue;
        if (o.contains("relation")) {
            const QJsonObject rel = o.value("relation").toObject();
            return fail("\"" + series + "\" is an error bar (" + rel.value("role").toString()
                        + ") of \"" + rel.value("of").toString() + "\", not a curve");
        }
        ReferenceCurve r;
        r.file = path;
        r.sha256 = p->sha256;
        r.series = series;
        r.strainColumn = strainColumn;
        r.strainUnit = strainUnit;
        r.stressColumn = stressColumn;
        r.stressUnit = stressUnit;
        bool hasColumns = false;
        for (const QJsonValue &pv : o.value("points").toArray()) {
            const QJsonObject pt = pv.toObject();
            hasColumns = hasColumns || (pt.contains(strainColumn) && pt.contains(stressColumn));
            const QJsonValue x = pt.value(strainColumn), y = pt.value(stressColumn);
            if (!x.isDouble() || !y.isDouble())
                continue;  // a gap stays a gap
            r.strain.push_back(x.toDouble() * su->second);
            r.stress.push_back(y.toDouble() * tu->second);
        }
        if (!hasColumns)
            return fail("\"" + series + "\" has no columns \"" + strainColumn + "\" and \""
                        + stressColumn + "\"");
        if (r.strain.size() < 2)
            return fail("\"" + series + "\" has fewer than two points with both values");
        return r;
    }
    return fail("the file has no series named \"" + series + "\"; it has: " + names.join(", "));
}

} // namespace tf::core
