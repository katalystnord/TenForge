#include "core/Exports.h"

#include "core/ApplicationInfo.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>
#include <map>
#include <cmath>
#include <optional>

namespace tf::core {

namespace {

const tf::analysis::Determination *found(const Analysis &a, const std::string &key)
{
    const auto it = a.result.results.find(key);
    return it != a.result.results.end() && it->second.determined ? &it->second : nullptr;
}

QString num(double v) { return QString::number(v, 'g', 8); }

// The reported value and unit of a result id, as the results document has
// them ("300", "MPa"); empty if not determined.
std::optional<std::pair<QString, QString>> reported(const Analysis &a, const QString &id)
{
    for (const QJsonValue &v : a.doc.value("results").toArray()) {
        const QJsonObject r = v.toObject();
        if (r.value("id").toString() != id || r.value("status").toString() != "determined")
            continue;
        const QJsonObject rep = r.value("reported").toObject();
        return std::make_pair(rep.value("value").toString(), rep.value("unit").toString());
    }
    return std::nullopt;
}

QString source(const Analysis &a)
{
    const QJsonObject src = a.doc.value("source").toObject();
    return QFileInfo(src.value("path").toString()).fileName() + " (SHA-256 "
           + src.value("sha256").toString() + ")";
}

QString conformityLine(const Analysis &a)
{
    const QString std = a.profile.designation.isEmpty() ? a.profile.title : a.profile.designation;
    return a.doc.value("conformity").toObject().value("status").toString() == "to-standard"
               ? "To " + std
               : "Aligned with " + std + ", not to the standard (see results.json for why)";
}

// The initial yield result for a tension analysis: ReL when the curve has a
// yield point, else the proof strength with the smallest offset.
const ProfileResult *initialYield(const Analysis &a)
{
    for (const ProfileResult &pr : a.profile.results)
        if (pr.quantity == "ReL" && found(a, pr.key.toStdString()))
            return &pr;
    const ProfileResult *best = nullptr;
    double bestOffset = 1e9;
    static const QRegularExpression rp(R"(^Rp(\d+(?:\.\d+)?)$)");
    for (const ProfileResult &pr : a.profile.results) {
        const auto m = rp.match(pr.quantity);
        if (!m.hasMatch() || !found(a, pr.key.toStdString()))
            continue;
        const double o = m.captured(1).toDouble();
        if (o < bestOffset) {
            bestOffset = o;
            best = &pr;
        }
    }
    return best;
}

} // namespace

Hardening hardeningCurve(const Analysis &a, int points)
{
    Hardening h;
    if (a.profile.mode != "tension") {
        h.error = QString("a hardening curve comes from a tension test; ")
                  + (a.profile.mode == "flexure"
                         ? "a bending test's stress is not uniform through the beam"
                         : "a compression curve is not a tensile flow curve")
                  + ", so there is none to export";
        return h;
    }
    if (points < 3) {
        h.error = "a hardening curve needs at least 3 points";
        return h;
    }
    const auto *E = found(a, "E");
    const auto *rm = found(a, "Rm");
    const ProfileResult *y = initialYield(a);
    if (!E || !(E->value > 0)) {
        h.error = "the modulus E was not determined, and true plastic strain needs it";
        return h;
    }
    if (!rm || !rm->point) {
        h.error = "the tensile strength Rm was not determined";
        return h;
    }
    if (!y) {
        h.error = "no initial yield: neither ReL nor a proof strength was determined";
        return h;
    }
    const auto *yd = found(a, y->key.toStdString());
    if (!yd->point) {
        h.error = "the initial yield " + y->id + " has no point on the curve";
        return h;
    }
    h.yieldResult = y->id;

    const double x0 = a.result.strainOrigin;
    const double Ev = E->value;
    auto truePoint = [&](double strainRecorded, double stress) {
        const double e = strainRecorded - x0;
        const double st = stress * (1.0 + e);
        return HardeningPoint{st, std::log1p(e) - st / Ev};
    };

    const std::size_t iy = std::size_t(std::ceil(yd->point->index));
    const std::size_t im = std::size_t(rm->point->index);
    if (im >= a.strain.size() || !std::isfinite(a.strain[im])) {
        h.error = "the strain was not measured up to the maximum force";
        return h;
    }
    if (!(iy < im)) {
        h.error = "the initial yield " + y->id + " is not before the maximum force";
        return h;
    }
    const HardeningPoint yp = truePoint(yd->point->x, yd->point->y);
    std::vector<HardeningPoint> curve{yp};
    for (std::size_t i = iy; i <= im; ++i)
        if (std::isfinite(a.strain[i]) && std::isfinite(a.stress[i]))
            curve.push_back(truePoint(a.strain[i], a.stress[i]));
    const double pEnd = curve.back().plasticStrain;
    if (!(pEnd > yp.plasticStrain)) {
        h.error = "no plastic strain between " + y->id + " and the maximum force";
        return h;
    }

    // Initial yield at plastic strain 0; then the curve's own plastic
    // strains from the yield point to Rm, evenly spaced, each stress at the
    // first place the curve reaches that plastic strain.
    h.points.push_back({yp.trueStress, 0.0});
    std::size_t j = 0;
    for (int k = 0; k < points - 1; ++k) {
        const double target = yp.plasticStrain + (pEnd - yp.plasticStrain) * k / (points - 2);
        if (k == 0) {
            h.points.push_back(yp);
            continue;
        }
        while (j + 1 < curve.size() && curve[j + 1].plasticStrain < target)
            ++j;
        double s = curve.back().trueStress;
        if (j + 1 < curve.size()) {
            const auto &p0 = curve[j], &p1 = curve[j + 1];
            const double span = p1.plasticStrain - p0.plasticStrain;
            const double t = span > 0 ? (target - p0.plasticStrain) / span : 0.0;
            s = p0.trueStress + std::clamp(t, 0.0, 1.0) * (p1.trueStress - p0.trueStress);
        }
        h.points.push_back({s, target});
    }
    h.points.back().trueStress = curve.back().trueStress;  // exactly the maximum

    int raised = 0;
    for (std::size_t i = 1; i < h.points.size(); ++i)
        if (h.points[i].trueStress < h.points[i - 1].trueStress) {
            h.points[i].trueStress = h.points[i - 1].trueStress;
            ++raised;
        }

    h.notes << "Derived by TenForge " + applicationVersion() + " from " + source(a);
    h.notes << conformityLine(a);
    h.notes << "True stress = engineering stress (1 + e); true plastic strain = ln(1 + e) - "
               "true stress / E, with e counted from the zero point and E = "
               + num(Ev * 1e-6) + " MPa";
    h.notes << "Initial yield is " + y->id + " at plastic strain 0; then the curve's own plastic "
               "strains from " + y->id + " to the maximum force (necking excluded)";
    if (raised > 0)
        h.notes << QString("%1 point(s) raised to the stress before them, so the curve never "
                           "falls (a yield drop or noise)")
                       .arg(raised);
    return h;
}

QStringList yieldPoints(const Hardening &h)
{
    QStringList out;
    for (const HardeningPoint &p : h.points)
        out << num(p.trueStress * 1e-6) + ", " + num(p.plasticStrain);
    return out;
}

QString calculixPlastic(const Hardening &h, const Analysis &, bool kinematic)
{
    QString out;
    for (const QString &n : h.notes)
        out += "** " + n + "\n";
    out += "** Units: MPa (N/mm2) and true plastic strain, for a model in mm, N, s.\n";
    out += kinematic ? "*PLASTIC, HARDENING=KINEMATIC\n" : "*PLASTIC\n";
    for (const QString &l : yieldPoints(h))
        out += l + "\n";
    return out;
}

QString freecadCard(const Analysis &a, const Specimen &s, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return QString();
    };
    if (a.profile.mode == "flexure")
        return fail("bending (flexural) properties have no place in FreeCAD's material card, "
                    "whose values are uniaxial");

    auto quote = [](QString v) { return "\"" + v.replace('"', '\'') + "\""; };
    QStringList fields;
    QStringList comments{"# Made by TenForge " + applicationVersion() + " from " + source(a),
                         "# " + conformityLine(a),
                         "# Values as reported. A tensile test does not measure Poisson's "
                         "ratio or density;",
                         "# enter them in FreeCAD before a FEM analysis. Ultimate strain is "
                         "left out: FreeCAD 1.1 gives it a pressure unit."};
    if (a.profile.mode == "tension") {
        if (const auto e = reported(a, "E"))
            fields << "YoungsModulus: " + quote(e->first + " " + e->second);
        if (const ProfileResult *y = initialYield(a))
            if (const auto v = reported(a, y->id)) {
                fields << "YieldStrength: " + quote(v->first + " " + v->second);
                comments << "# YieldStrength is " + y->id + ".";
            }
        if (const auto rm = reported(a, "Rm"))
            fields << "UltimateTensileStrength: " + quote(rm->first + " " + rm->second);
    } else {
        for (const ProfileResult &pr : a.profile.results)
            if (pr.quantity == "max-stress" || pr.quantity == "peak-stress")
                if (const auto v = reported(a, pr.id)) {
                    fields << "CompressiveStrength: " + quote(v->first + " " + v->second);
                    comments << "# CompressiveStrength is " + pr.id + ".";
                    break;
                }
    }
    if (fields.isEmpty())
        return fail("no result the card can hold was determined");

    const QString name = !s.material.isEmpty() ? s.material
                         : !s.id.isEmpty()     ? s.id
                                               : QStringLiteral("TenForge material");
    QString out = "---\n" + comments.join('\n') + "\n";
    out += "General:\n";
    out += "  UUID: \"" + QUuid::createUuid().toString(QUuid::WithoutBraces) + "\"\n";
    out += "  Name: " + quote(name) + "\n";
    out += "  Author: " + quote("TenForge " + applicationVersion()) + "\n";
    out += "  Description: " + quote("Measured; " + conformityLine(a)) + "\n";
    out += "Models:\n";
    out += "  LinearElastic:\n";
    out += "    UUID: '7b561d1d-fb9b-44f6-9da9-56a4f74d7536'\n";
    for (const QString &f : fields)
        out += "    " + f + "\n";
    return out;
}

QString curveVtp(const Analysis &a)
{
    std::vector<std::size_t> keep;
    for (std::size_t i = 0; i < a.stress.size() && i < a.strain.size(); ++i)
        if (std::isfinite(a.stress[i]) && std::isfinite(a.strain[i]))
            keep.push_back(i);
    const auto &F = a.imported.input.force;

    // The SI data at full precision (17 digits round-trip a double); the
    // plotting arrays at 8, which is more than a chart can show.
    auto array = [&](const QString &name, auto value, int digits = 17) {
        QString s = "        <DataArray type=\"Float64\" Name=\"" + name + "\" format=\"ascii\">\n          ";
        for (std::size_t i : keep)
            s += QString::number(value(i), 'g', digits) + " ";
        return s + "\n        </DataArray>\n";
    };
    QString x = "<?xml version=\"1.0\"?>\n";
    x += "<!-- TenForge " + applicationVersion() + ": the analysed curve of " + source(a).toHtmlEscaped()
         + ", analysed with " + a.profile.id.toHtmlEscaped() + " " + a.profile.profileVersion.toHtmlEscaped()
         + ". Plot it in a Line Chart View, X array strain_percent, Y stress_MPa (the 3D "
           "view scales both axes alike and squeezes the curve). Points are (strain %, "
           "stress MPa, 0); strain, stress_Pa and force_N are the SI data. "
           "Derived; the test record is the record. -->\n";
    x += "<VTKFile type=\"PolyData\" version=\"1.0\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n";
    x += "  <PolyData>\n    <FieldData>\n";
    for (const QJsonValue &v : a.doc.value("results").toArray()) {
        const QJsonObject r = v.toObject();
        if (r.value("status").toString() != "determined")
            continue;
        QString id = r.value("id").toString();
        id.replace(QRegularExpression("[^A-Za-z0-9_.]"), "_");
        x += "      <DataArray type=\"Float64\" Name=\"" + id + "_" + r.value("unit").toString()
             + "\" NumberOfTuples=\"1\" format=\"ascii\">" + num(r.value("value").toDouble())
             + "</DataArray>\n";
    }
    x += "    </FieldData>\n";
    x += QString("    <Piece NumberOfPoints=\"%1\" NumberOfVerts=\"0\" NumberOfLines=\"1\" "
                 "NumberOfStrips=\"0\" NumberOfPolys=\"0\">\n")
             .arg(keep.size());
    x += "      <Points>\n        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n          ";
    for (std::size_t i : keep)
        x += num(a.strain[i] * 100.0) + " " + num(a.stress[i] * 1e-6) + " 0 ";
    x += "\n        </DataArray>\n      </Points>\n";
    x += "      <PointData Scalars=\"stress_Pa\">\n";
    x += array("strain", [&](std::size_t i) { return a.strain[i]; });
    x += array("stress_Pa", [&](std::size_t i) { return a.stress[i]; });
    x += array("force_N", [&](std::size_t i) { return i < F.size() ? F[i] : NAN; });
    // Ready to plot in ParaView's Line Chart View (X array strain_percent),
    // without calculator steps; the SI arrays above are the data.
    x += array("strain_percent", [&](std::size_t i) { return a.strain[i] * 100.0; }, 8);
    x += array("stress_MPa", [&](std::size_t i) { return a.stress[i] * 1e-6; }, 8);
    x += "      </PointData>\n";
    x += "      <Lines>\n        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">\n          ";
    for (std::size_t k = 0; k < keep.size(); ++k)
        x += QString::number(k) + " ";
    x += "\n        </DataArray>\n        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">"
         + QString::number(keep.size()) + "</DataArray>\n      </Lines>\n";
    x += "    </Piece>\n  </PolyData>\n</VTKFile>\n";
    return x;
}

QString pmdJsonLd(const Analysis &a, const Specimen &s, QString *error)
{
    if (a.profile.mode != "tension") {
        if (error)
            *error = "Platform MaterialDigital's ontologies model the results of tensile tests "
                     "(TTO); for compression and bending they have process classes only";
        return {};
    }
    // TenForge's quantity -> the class of the quality or characteristic,
    // the QUDT unit, and the factor from SI. TTO 3.0.0 terms that TTO's
    // main branch has since deprecated are given as their PMDco 3.1.2
    // replacements (ReH, ReL, A, the gauge length), so the document stays
    // valid when TTO next releases.
    struct Term {
        const char *cls, *unit;
        double fromSi;
    };
    static const std::map<QString, Term> terms{
        {"E", {"pmd:PMD_0000618", "unit:GigaPA", 1e-9}},
        {"Rp0.2", {"tto:TTO_0000047", "unit:MegaPA", 1e-6}},
        {"ReH", {"pmd:PMD_0080173", "unit:MegaPA", 1e-6}},
        {"ReL", {"pmd:PMD_0080172", "unit:MegaPA", 1e-6}},
        {"Rm", {"tto:TTO_0000053", "unit:MegaPA", 1e-6}},
        {"Fm", {"tto:TTO_0000023", "unit:N", 1.0}},
        {"A", {"pmd:PMD_0060004", "unit:PERCENT", 100.0}},
        {"Agt", {"tto:TTO_0000040", "unit:PERCENT", 100.0}},
        {"Ag", {"tto:TTO_0000037", "unit:PERCENT", 100.0}},
    };
    const QString base = "urn:tenforge:"
                         + a.doc.value("source").toObject().value("sha256").toString().left(32) + ":";
    auto id = [&](const QString &local) { return QJsonObject{{"@id", base + local}}; };
    QJsonArray graph, outputs, characteristics, qualities;
    QStringList skipped;

    auto datum = [&](const QString &key, const char *cls, const char *unit, double value,
                     bool quality) {
        const QString q = key;
        graph.append(QJsonObject{{"@id", base + q}, {"@type", cls}});
        graph.append(QJsonObject{
            {"@id", base + q + "_datum"},
            {"@type", "obo:IAO_0000109"},
            {"obo:IAO_0000221", id(q)},
            {"obo:OBI_0001938",
             QJsonObject{{"@type", "obo:OBI_0001931"},
                         {"obo:OBI_0001927", id(q)},
                         {"obo:OBI_0001937", QJsonObject{{"@value", value}, {"@type", "xsd:double"}}},
                         {"obo:IAO_0000039", QJsonObject{{"@id", unit}}}}}});
        outputs.append(id(q + "_datum"));
        (quality ? qualities : characteristics).append(id(q));
    };

    if (const auto area = crossSection(s).area)
        datum("S0", "tto:TTO_0000026", "unit:MilliM2", *area * 1e6, true);
    if (s.gaugeLength && !a.imported.extensionIsStrain)
        datum("L0", "pmd:PMD_0060016", "unit:MilliM", *s.gaugeLength * 1e3, true);
    for (const ProfileResult &pr : a.profile.results) {
        const auto *d = found(a, pr.key.toStdString());
        if (!d)
            continue;
        const auto t = terms.find(pr.quantity);
        if (t == terms.end()) {
            skipped << pr.id;
            continue;
        }
        QString local = pr.id;
        local.replace('.', '_');
        datum(local, t->second.cls, t->second.unit, d->value * t->second.fromSi, false);
    }

    const QString standard = a.profile.designation.isEmpty() ? a.profile.title : a.profile.designation;
    QJsonObject process{{"@id", base + "test"},
                        {"@type", "pmd:PMD_0000974"},
                        {"obo:OBI_0000293", id("testpiece")},
                        {"obo:BFO_0000055", id("plan")},
                        {"obo:OBI_0000299", outputs}};
    QString comment = "Made by TenForge " + applicationVersion() + " from " + source(a) + ". "
                      + conformityLine(a) + ".";
    if (!skipped.isEmpty())
        comment += " Results with no class in PMDco or TTO, left out: " + skipped.join(", ") + ".";
    process["rdfs:comment"] = comment;
    graph.prepend(process);
    graph.append(QJsonObject{{"@id", base + "plan"},
                             {"@type", "obo:OBI_0000260"},
                             {"obo:RO_0000059", id("method")}});
    graph.append(QJsonObject{{"@id", base + "method"},
                             {"@type", QJsonArray{"tto:TTO_0000054", "obo:IAO_0000033"}},
                             {"rdfs:label", standard}});
    QJsonObject piece{{"@id", base + "testpiece"},
                      {"@type", "pmd:PMD_0020193"},
                      {"obo:RO_0000087",
                       QJsonObject{{"@id", base + "testpiece_role"}, {"@type", "obo:OBI_0000067"}}},
                      {"obo:RO_0000086", qualities},
                      {"obo:RO_0000053", characteristics}};
    if (!s.id.isEmpty())
        piece["rdfs:label"] = s.id;
    graph.append(piece);

    const QJsonObject doc{
        {"@context",
         QJsonObject{{"obo", "http://purl.obolibrary.org/obo/"},
                     {"pmd", "https://w3id.org/pmd/co/"},
                     {"tto", "https://w3id.org/pmd/tto/"},
                     {"unit", "http://qudt.org/vocab/unit/"},
                     {"rdfs", "http://www.w3.org/2000/01/rdf-schema#"},
                     {"xsd", "http://www.w3.org/2001/XMLSchema#"}}},
        {"@graph", graph}};
    return QString::fromUtf8(QJsonDocument(doc).toJson(QJsonDocument::Indented));
}

} // namespace tf::core
