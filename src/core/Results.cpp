#include "core/Results.h"

#include "core/ApplicationInfo.h"

#include <QJsonArray>

#include <cmath>
#include <map>

namespace tf::core {

namespace {

// Factor from the SI unit to the report unit, by SI unit.
std::optional<double> reportFactor(const QString &si, const QString &to)
{
    static const std::map<QString, std::map<QString, double>> t{
        {"Pa", {{"Pa", 1}, {"kPa", 1e-3}, {"MPa", 1e-6}, {"GPa", 1e-9}}},
        {"N", {{"N", 1}, {"kN", 1e-3}, {"MN", 1e-6}}},
        {"1", {{"1", 1}, {"%", 100}}},
    };
    const auto a = t.find(si);
    if (a == t.end())
        return std::nullopt;
    const auto b = a->second.find(to);
    if (b == a->second.end())
        return std::nullopt;
    return b->second;
}

int decimalsOf(double step)
{
    for (int d = 0; d <= 9; ++d) {
        const double scaled = step * std::pow(10.0, d);
        if (std::fabs(scaled - std::round(scaled)) < 1e-9 * std::max(1.0, scaled))
            return d;
    }
    return 9;
}

QJsonValue numberOrNull(double v) { return std::isfinite(v) ? QJsonValue(v) : QJsonValue(); }

} // namespace

QString reportedText(double siValue, const ProfileResult &r)
{
    const auto f = reportFactor(r.unit, r.reportUnit);
    if (!f || !std::isfinite(siValue))
        return {};
    const double v = siValue * *f;
    if (r.rounding <= 0)
        return QString::number(v, 'g', 10);
    // Round half away from zero to the profile's step.
    const double rounded = std::round(v / r.rounding) * r.rounding;
    return QString::number(rounded, 'f', decimalsOf(r.rounding));
}

QJsonObject resultsJson(const Profile &profile, const tf::analysis::TensionResult &result,
                        const SourceInfo &source, const QDateTime &computedUtc)
{
    QJsonObject root;
    root["format"] = "tenforge-results";
    root["version"] = "1.0";
    root["computed"] = computedUtc.toUTC().toString(Qt::ISODate);
    root["application"] = QJsonObject{{"name", applicationName()},
                                      {"version", applicationVersion()},
                                      {"build", applicationBuild()}};
    // Analysed straight from a file, not a test record: the record field is
    // null and the source is described instead.
    root["record"] = QJsonValue();
    root["source"] = QJsonObject{
        {"path", source.path},
        {"sha256", source.sha256},
        {"columns", QJsonObject{{"force", source.forceColumn}, {"extension", source.extensionColumn}}},
        {"notes", QJsonArray::fromStringList(source.notes)}};
    root["profile"] = QJsonObject{{"id", profile.id},
                                  {"profile_version", profile.profileVersion},
                                  {"sha256", QString::fromLatin1(profile.sha256)},
                                  {"shipped", profile.shipped}};

    // --- Conformity ------------------------------------------------------------
    QJsonArray reasons;
    QStringList unconfirmed;
    for (const auto &u : profile.parameters)
        if (!u.confirmed)
            unconfirmed << u.path;
    if (!unconfirmed.isEmpty())
        reasons.append(QJsonObject{
            {"code", "unconfirmed-parameter"},
            {"detail", QString("%1 of %2 profile parameters are not yet confirmed from the standard's text: %3")
                           .arg(unconfirmed.size())
                           .arg(profile.parameters.size())
                           .arg(unconfirmed.join(", "))}});
    reasons.append(QJsonObject{{"code", "no-force-calibration"},
                               {"detail", "No force calibration certificate (ISO 7500-1) is recorded with this analysis."}});
    reasons.append(QJsonObject{{"code", "no-strain-calibration"},
                               {"detail", "No extensometer calibration certificate (ISO 9513) is recorded with this analysis."}});
    if (!profile.shipped)
        reasons.append(QJsonObject{{"code", "user-profile"},
                                   {"detail", "The profile is not one shipped with TenForge: " + profile.path}});
    root["conformity"] = QJsonObject{{"status", "aligned"}, {"reasons", reasons}};

    // --- Results ------------------------------------------------------------------
    QJsonArray results;
    for (const ProfileResult &pr : profile.results) {
        QJsonObject o;
        o["id"] = pr.id;
        o["symbol"] = pr.symbol;
        o["name"] = pr.name;
        o["unit"] = pr.unit;
        const auto it = result.results.find(pr.quantity.toStdString());
        const bool have = it != result.results.end();
        const tf::analysis::Determination *d = have ? &it->second : nullptr;
        if (d && d->determined) {
            o["status"] = "determined";
            o["value"] = numberOrNull(d->value);
            o["reported"] = QJsonObject{{"value", reportedText(d->value, pr)}, {"unit", pr.reportUnit}};
            o["reason"] = QJsonValue();
        } else {
            o["status"] = "not-determined";
            o["value"] = QJsonValue();
            o["reported"] = QJsonValue();
            o["reason"] = d ? QString::fromStdString(d->reason)
                            : QString("the engine produced no " + pr.quantity);
        }

        QJsonObject how{{"primitive", pr.primitive}, {"quantity", pr.quantity}};
        if (d && d->fit) {
            const auto &f = *d->fit;
            how["fit"] = "regression";
            how["statistics"] = QJsonObject{{"slope", f.slope}, {"intercept", f.intercept},
                                            {"r2", f.r2}, {"sm", f.sm}, {"sm_rel", f.smRel}};
            how["samples"] = QJsonObject{{"channel", source.extensionColumn},
                                         {"first", qint64(f.first)},
                                         {"last", qint64(f.last)},
                                         {"n", qint64(f.n)}};
        }
        if (d && d->window)
            how["window"] = QJsonObject{{"axis", QString::fromStdString(d->windowAxis)},
                                        {"from", d->window->first},
                                        {"to", d->window->second}};
        if (d && d->point)
            how["point"] = QJsonObject{{"sample", d->point->index},
                                       {"strain", numberOrNull(d->point->x)},
                                       {"value", numberOrNull(d->point->y)}};
        QJsonArray params;
        const QString prefix = "results." + pr.id + ".";
        for (const auto &u : profile.parameters)
            if (u.path.startsWith(prefix))
                params.append(QJsonObject{{"path", u.path}, {"confirmed", u.confirmed}});
        how["parameters"] = params;
        o["how"] = how;
        results.append(o);
    }
    root["results"] = results;

    QJsonArray flags;
    for (const auto &f : result.flags)
        flags.append(QString::fromStdString(f));
    root["flags"] = flags;
    root["strain_origin"] = result.strainOrigin;
    return root;
}

QString resultsCsv(const QJsonObject &root)
{
    auto cell = [](QString s) {
        if (s.contains(',') || s.contains('"') || s.contains('\n'))
            s = '"' + s.replace("\"", "\"\"") + '"';
        return s;
    };
    const QString conformity = root.value("conformity").toObject().value("status").toString();
    QString out = "id,symbol,status,value,unit,reported_value,reported_unit,u,k,conformity,reason\n";
    for (const QJsonValue &v : root.value("results").toArray()) {
        const QJsonObject r = v.toObject();
        const QJsonObject rep = r.value("reported").toObject();
        QStringList row{
            cell(r.value("id").toString()),
            cell(r.value("symbol").toString()),
            r.value("status").toString(),
            r.value("value").isDouble() ? QString::number(r.value("value").toDouble(), 'g', 12) : QString(),
            cell(r.value("unit").toString()),
            cell(rep.value("value").toString()),
            cell(rep.value("unit").toString()),
            QString(),  // u: uncertainty arrives with Phase 7
            QString(),  // k
            conformity,
            cell(r.value("reason").toString()),
        };
        out += row.join(',') + '\n';
    }
    return out;
}

} // namespace tf::core
