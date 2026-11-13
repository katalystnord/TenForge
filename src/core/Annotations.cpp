#include "core/Annotations.h"

#include <QJsonObject>

#include <cmath>

namespace tf::core {

namespace {

// A zero point closer to the recorded zero than this is not drawn as a toe:
// a metal's elastic-line zero sits a few microstrain off and would only add
// clutter.
constexpr double kSmallestToe = 1e-4;

const tf::analysis::Determination *found(const Analysis &a, const QString &key)
{
    const auto it = a.result.results.find(key.toStdString());
    return it != a.result.results.end() && it->second.determined ? &it->second : nullptr;
}

} // namespace

std::vector<Region> analysisRegions(const Analysis &a)
{
    std::vector<Region> out;
    const std::size_t n = a.strain.size();
    if (n == 0)
        return out;

    // The toe the zero point leaves out (elastic-line zero).
    const double x0 = a.result.strainOrigin;
    if (x0 > kSmallestToe)
        out.push_back({"toe", "Toe, not counted", 0.0, x0});

    // The samples the modulus was fitted to.
    for (const ProfileResult &pr : a.profile.results) {
        const auto *d = found(a, pr.key);
        if (!d || !d->fit || d->fit->last >= n)
            continue;
        out.push_back({"modulus-fit", "Modulus fit (" + pr.id + ")", a.strain[d->fit->first],
                       a.strain[d->fit->last]});
        break;
    }

    if (a.profile.mode == "tension") {
        const auto *rm = found(a, "Rm");
        const auto *rb = found(a, "Rb");
        if (rm && rm->point && std::isfinite(rm->point->x)) {
            const double from = std::max(0.0, x0);
            if (rm->point->x > from)
                out.push_back({"uniform-elongation", "Uniform elongation", from, rm->point->x});
            if (rb && rb->point && std::isfinite(rb->point->x) && rb->point->x > rm->point->x)
                out.push_back({"necking", "Necking", rm->point->x, rb->point->x});
        }
    } else {
        for (const ProfileResult &pr : a.profile.results) {
            if (pr.primitive != "break")
                continue;
            const auto *d = found(a, pr.key);
            if (!d || !d->point || !std::isfinite(d->point->x))
                continue;
            double last = d->point->x;
            for (double e : a.strain)
                if (std::isfinite(e))
                    last = std::max(last, e);
            if (last > d->point->x)
                out.push_back({"after-break", "After the break", d->point->x, last});
            break;
        }
    }
    return out;
}

std::vector<PointLabel> resultLabels(const Analysis &a)
{
    std::vector<PointLabel> out;
    for (const QJsonValue &v : a.doc.value("results").toArray()) {
        const QJsonObject r = v.toObject();
        if (r.value("status").toString() != "determined" || r.value("unit").toString() != "Pa")
            continue;
        const QString id = r.value("id").toString();
        QString key = id;
        for (const ProfileResult &pr : a.profile.results)
            if (pr.id == id)
                key = pr.key;
        const auto *d = found(a, key);
        if (!d || !d->point || d->fit)  // a modulus is a line, drawn as one
            continue;
        const QJsonObject rep = r.value("reported").toObject();
        out.push_back({id,
                       id + " = " + rep.value("value").toString() + " "
                           + rep.value("unit").toString(),
                       d->point->x, d->point->y});
    }
    return out;
}

std::optional<double> stressAtStrain(const Analysis &a, double strain)
{
    const auto &e = a.strain;
    const auto &s = a.stress;
    for (std::size_t i = 0; i + 1 < e.size() && i + 1 < s.size(); ++i) {
        const double x0 = e[i], x1 = e[i + 1];
        if (!std::isfinite(x0) || !std::isfinite(x1))
            continue;
        if ((x0 <= strain && strain <= x1) || (x1 <= strain && strain <= x0)) {
            const double t = x1 != x0 ? (strain - x0) / (x1 - x0) : 0.0;
            return s[i] + t * (s[i + 1] - s[i]);
        }
    }
    return std::nullopt;
}

QJsonArray curveNotesJson(const std::vector<CurveNote> &notes)
{
    QJsonArray out;
    for (const CurveNote &n : notes) {
        QJsonObject o{{"from", n.from}, {"text", n.text}};
        o["to"] = n.to ? QJsonValue(*n.to) : QJsonValue();
        out.append(o);
    }
    return out;
}

std::optional<std::vector<CurveNote>> curveNotesFromJson(const QJsonArray &a, QString *error)
{
    std::vector<CurveNote> out;
    for (const QJsonValue &v : a) {
        const QJsonObject o = v.toObject();
        if (!o.value("from").isDouble()) {
            if (error)
                *error = "a note on the curve has no strain (\"from\")";
            return std::nullopt;
        }
        CurveNote n;
        n.from = o.value("from").toDouble();
        n.text = o.value("text").toString();
        if (o.value("to").isDouble()) {
            n.to = o.value("to").toDouble();
            if (!(*n.to > n.from)) {
                if (error)
                    *error = "the note \"" + n.text + "\" ends before it starts";
                return std::nullopt;
            }
        }
        out.push_back(n);
    }
    return out;
}

} // namespace tf::core
