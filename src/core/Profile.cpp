#include "core/Profile.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <map>

namespace tf::core {

namespace {

// The engine's quantities and the primitive that produces each. A profile
// naming anything else is refused: the engine has no hidden fallbacks.
const std::map<QString, QString> &knownQuantities()
{
    static const std::map<QString, QString> q{
        {"E", "linear-fit"},        {"ReH", "sustained-peak"}, {"ReL", "minimum"},
        {"Fm", "maximum"},          {"Rm", "maximum"},         {"Agt", "value-at"},
        {"Ag", "value-at"},         {"At", "break"},           {"A", "break"},
        {"Rb", "break"},
    };
    return q;
}

bool isProof(const QString &q) { return QRegularExpression(R"(^Rp\d+(\.\d+)?$)").match(q).hasMatch(); }

QString expectedPrimitive(const QString &quantity)
{
    if (isProof(quantity))
        return "offset-intersection";
    const auto it = knownQuantities().find(quantity);
    return it == knownQuantities().end() ? QString() : it->second;
}

struct Reader {
    std::vector<ParameterUse> *uses;
    QString error;

    // A parameter object: {"value": ..., "confirmed": ..., "source": {...}}.
    std::optional<QJsonValue> param(const QJsonObject &parent, const QString &key,
                                    const QString &path)
    {
        if (!parent.contains(key)) {
            error = "missing parameter " + path;
            return std::nullopt;
        }
        const QJsonObject p = parent.value(key).toObject();
        if (!p.contains("value") || !p.contains("confirmed")) {
            error = path + " is not a parameter (needs \"value\" and \"confirmed\")";
            return std::nullopt;
        }
        uses->push_back({path, p.value("confirmed").toBool(),
                         p.value("source").toObject().value("document").toString()});
        return p.value("value");
    }

    std::optional<double> number(const QJsonObject &parent, const QString &key, const QString &path)
    {
        const auto v = param(parent, key, path);
        if (!v)
            return std::nullopt;
        if (!v->isDouble()) {
            error = path + " must be a number";
            return std::nullopt;
        }
        return v->toDouble();
    }
};

} // namespace

QStringList shippedProfiles()
{
    QStringList ids;
    for (const QString &f : QDir(":/profiles").entryList({"*.json"}, QDir::Files, QDir::Name))
        ids << QFileInfo(f).completeBaseName();
    return ids;
}

ProfileOrError loadProfile(const QString &pathOrId)
{
    auto fail = [](const QString &why) { return ProfileOrError{std::nullopt, why}; };

    QString path = pathOrId;
    bool shipped = false;
    if (!QFileInfo::exists(path) && QFileInfo::exists(":/profiles/" + pathOrId + ".json")) {
        path = ":/profiles/" + pathOrId + ".json";
        shipped = true;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return fail("no profile file or shipped profile named \"" + pathOrId + "\"; shipped: "
                    + shippedProfiles().join(", "));
    const QByteArray bytes = f.readAll();
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError)
        return fail(path + " is not valid JSON: " + pe.errorString() + " at offset "
                    + QString::number(pe.offset));
    const QJsonObject root = doc.object();
    if (root.value("format").toString() != "tenforge-profile")
        return fail(path + " is not a TenForge profile (format is not \"tenforge-profile\")");
    const QString version = root.value("version").toString();
    if (!version.startsWith("1."))
        return fail(path + " is profile format version " + version
                    + "; this TenForge reads version 1.x");

    Profile p;
    p.id = root.value("id").toString();
    p.title = root.value("title").toString();
    {
        const QJsonObject st = root.value("standard").toObject();
        const QString body = st.value("body").toString(), number = st.value("number").toString(),
                      edition = st.value("edition").toString();
        if (!number.isEmpty())
            p.designation = (body + " " + number).trimmed() + (edition.isEmpty() ? "" : ":" + edition);
    }
    p.profileVersion = root.value("profile_version").toString();
    p.mode = root.value("mode").toString();
    p.path = path;
    p.shipped = shipped;
    p.sha256 = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
    if (p.mode != "tension")
        return fail("profile " + p.id + " is for mode \"" + p.mode
                    + "\"; this TenForge analyses tension only");

    tf::analysis::TensionMethod &m = p.method;
    m.proofOffsets.clear();
    m.yieldPoint = false;
    Reader rd{&p.parameters, {}};

    for (const QJsonValue &c : root.value("conditioning").toArray()) {
        const QJsonObject step = c.toObject();
        if (step.value("step").toString() == "zero-point") {
            const QString how = step.value("method").toString();
            if (how == "elastic-line")
                m.strainFromElasticLine = true;
            else if (how == "as-recorded")
                m.strainFromElasticLine = false;
            else
                return fail("conditioning zero-point method \"" + how + "\" is not one the engine has");
        } else {
            return fail("conditioning step \"" + step.value("step").toString()
                        + "\" is not one the engine has");
        }
    }

    std::map<QString, double> proofById;      // result id -> offset
    QString modulusReference;
    std::optional<double> fractureRatio, fractureMinDrop;

    for (const QJsonValue &rv : root.value("results").toArray()) {
        const QJsonObject r = rv.toObject();
        ProfileResult pr;
        pr.id = r.value("id").toString();
        pr.symbol = r.value("symbol").toString();
        pr.name = r.value("name").toString();
        pr.quantity = r.value("quantity").toString(pr.id);
        pr.unit = r.value("unit").toString();
        const QJsonObject method = r.value("method").toObject();
        pr.primitive = method.value("primitive").toString();
        const QString base = "results." + pr.id + ".method";

        const QString expected = expectedPrimitive(pr.quantity);
        if (expected.isEmpty())
            return fail("result " + pr.id + " asks for quantity \"" + pr.quantity
                        + "\", which the engine does not compute");
        if (pr.primitive != expected)
            return fail("result " + pr.id + " names primitive \"" + pr.primitive
                        + "\"; the engine computes " + pr.quantity + " with \"" + expected + "\"");

        if (pr.quantity == "E") {
            const QJsonObject w = method.value("window").toObject();
            const QString axis = w.value("axis").toString();
            if (axis == "stress") {
                m.modulusAxis = tf::analysis::TensionMethod::Axis::Stress;
                modulusReference = w.value("reference").toString();
            } else if (axis == "strain") {
                m.modulusAxis = tf::analysis::TensionMethod::Axis::Strain;
            } else {
                return fail("result " + pr.id + ": modulus window axis \"" + axis
                            + "\" is not stress or strain");
            }
            const auto from = rd.number(w, "from", base + ".window.from");
            const auto to = rd.number(w, "to", base + ".window.to");
            if (!from || !to)
                return fail(rd.error);
            m.modulusFrom = *from;
            m.modulusTo = *to;
        } else if (isProof(pr.quantity)) {
            const auto o = rd.number(method, "offset", base + ".offset");
            if (!o)
                return fail(rd.error);
            m.proofOffsets.push_back(*o);
            proofById[pr.id] = *o;
            if (tf::analysis::proofId(*o) != pr.quantity)
                return fail("result " + pr.id + ": offset " + QString::number(*o)
                            + " does not match quantity " + pr.quantity);
        } else if (pr.quantity == "ReH") {
            m.yieldPoint = true;
            const auto d = rd.number(method, "min_drop", base + ".min_drop");
            const auto s = rd.number(method, "min_span", base + ".min_span");
            const auto x = rd.number(method, "max_plastic", base + ".max_plastic");
            if (!d || !s || !x)
                return fail(rd.error);
            m.yieldMinDrop = *d;
            m.yieldMinSpan = *s;
            m.yieldMaxPlastic = *x;
        } else if (pr.quantity == "ReL") {
            const auto t = rd.param(method, "ignore_initial_transient",
                                    base + ".ignore_initial_transient");
            if (!t)
                return fail(rd.error);
            m.lowerYieldIgnoresTransient = t->toBool();
        } else if (pr.primitive == "break" && method.contains("ratio")) {
            const auto ratio = rd.number(method, "ratio", base + ".ratio");
            const auto drop = rd.number(method, "min_drop", base + ".min_drop");
            if (!ratio || !drop)
                return fail(rd.error);
            if ((fractureRatio && *fractureRatio != *ratio) || (fractureMinDrop && *fractureMinDrop != *drop))
                return fail("result " + pr.id + " gives a different fracture rule from another result; "
                            "a test has one fracture");
            fractureRatio = ratio;
            fractureMinDrop = drop;
        }

        const QJsonObject report = r.value("report").toObject();
        pr.reportUnit = report.value("unit").toString(pr.unit);
        if (report.contains("rounding")) {
            const auto step = rd.number(report, "rounding", "results." + pr.id + ".report.rounding");
            if (!step)
                return fail(rd.error);
            pr.rounding = *step;
        }
        p.results.push_back(pr);
    }

    if (p.results.empty())
        return fail("profile " + p.id + " asks for no results");
    if (m.modulusAxis == tf::analysis::TensionMethod::Axis::Stress) {
        const auto it = proofById.find(modulusReference);
        if (it == proofById.end())
            return fail("the modulus window refers to \"" + modulusReference
                        + "\", which is not a proof strength in this profile");
        m.modulusReferenceOffset = it->second;
    }
    // No silent defaults: what the engine would otherwise fill in must come
    // from the profile.
    bool wantsBreak = false, wantsReL = false;
    for (const auto &r : p.results) {
        wantsBreak = wantsBreak || r.primitive == "break";
        wantsReL = wantsReL || r.quantity == "ReL";
    }
    if (wantsBreak && !fractureRatio)
        return fail("profile " + p.id + " asks for a result at fracture but no result gives the "
                    "fracture rule (ratio and min_drop)");
    if (wantsReL && !m.yieldPoint)
        return fail("profile " + p.id + " asks for ReL without ReH, which sets how a yield point is found");
    if (fractureRatio) {
        m.fractureRatio = *fractureRatio;
        m.fractureMinDrop = *fractureMinDrop;
    }
    return {p, {}};
}

} // namespace tf::core
