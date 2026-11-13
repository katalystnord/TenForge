#include "core/Report.h"

#include "core/ApplicationInfo.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QStringList>

namespace tf::core {

namespace {

QString esc(const QString &s) { return s.toHtmlEscaped(); }

QString mm(const std::optional<double> &m, double scale, const char *unit)
{
    if (!m)
        return QStringLiteral("not measured");
    return QString::number(*m * scale, 'g', 8) + " " + unit;
}

QString row(const QString &k, const QString &v)
{
    return "<tr><th align=\"left\">" + esc(k) + "</th><td>" + esc(v) + "</td></tr>\n";
}

// The unconfirmed-parameter detail lists every parameter path; the report
// gives the count and leaves the list to results.json.
QString shortDetail(const QJsonObject &reason)
{
    const QString d = reason.value("detail").toString();
    const QString marker = "standard's text: ";
    const int at = d.indexOf(marker);
    return at > 0 ? d.left(at + int(marker.size()) - 2) + " (each listed in results.json)." : d;
}

} // namespace

QString reportHtml(const QJsonObject &doc, const Specimen &sp, const QString &chartSrc,
                   const Fixture &fx)
{
    const QJsonObject profile = doc.value("profile").toObject();
    const QJsonObject source = doc.value("source").toObject();
    const QJsonObject app = doc.value("application").toObject();
    const QJsonObject conf = doc.value("conformity").toObject();
    // The standard's designation where the profile names one, as a reader
    // looks for it first.
    QString title = profile.value("standard").toString();
    if (title.isEmpty())
        title = profile.value("title").toString();
    if (title.isEmpty())
        title = profile.value("id").toString();
    const bool toStandard = conf.value("status").toString() == "to-standard";
    const QString mode = profile.value("mode").toString();
    const QString kind = profile.value("fixture").toString();
    const QString test = mode == "flexure"       ? QStringLiteral("Flexural test")
                         : mode == "compression" ? QStringLiteral("Compression test")
                                                 : QStringLiteral("Tensile test");

    QString h;
    h += "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n";
    h += "<title>" + test + " " + esc(sp.id) + "</title>\n";
    h += "<style>body{font-family:sans-serif;font-size:10pt}"
         "table{border-collapse:collapse;margin-bottom:12px}"
         "th,td{border:1px solid #999;padding:3px 6px;vertical-align:top}"
         ".conf{border:2px solid #b36b00;padding:6px;margin:8px 0}</style>\n";
    h += "</head><body>\n";
    h += "<h1>" + test + " report</h1>\n";
    h += "<p>Specimen <b>" + esc(sp.id.isEmpty() ? QStringLiteral("(no id)") : sp.id) + "</b>, "
         + esc(title) + "</p>\n";

    // Conformity first: a reader must not mistake an aligned result for one
    // made to the standard (tenet 9).
    h += "<div class=\"conf\"><p><b>"
         + QString(toStandard ? "To the standard: " : "Aligned with ") + esc(title)
         + (toStandard ? "" : ", not to the standard.") + "</b> "
         + (toStandard ? "" : "The results follow the profile's method, but:") + "</p><ul>\n";
    for (const QJsonValue &v : conf.value("reasons").toArray()) {
        const QJsonObject r = v.toObject();
        h += "<li>" + esc(shortDetail(r)) + " <i>(" + esc(r.value("code").toString()) + ")</i></li>\n";
    }
    h += "</ul></div>\n";

    h += "<h2>Specimen</h2>\n<table>\n";
    h += row("Id", sp.id);
    h += row("Material", sp.material);
    h += row("Shape", shapeName(sp.shape));
    switch (sp.shape) {
    case Specimen::Shape::Rectangular:
        h += row("Width", mm(sp.width, 1e3, "mm"));
        h += row("Thickness", mm(sp.thickness, 1e3, "mm"));
        break;
    case Specimen::Shape::Round:
        h += row("Diameter", mm(sp.diameter, 1e3, "mm"));
        break;
    case Specimen::Shape::Tube:
        h += row("Outer diameter", mm(sp.outerDiameter, 1e3, "mm"));
        h += row("Inner diameter", mm(sp.innerDiameter, 1e3, "mm"));
        break;
    case Specimen::Shape::GivenArea:
        break;
    }
    if (mode == "flexure") {
        h += row("Loading", kind);
        h += row("Support span L", mm(fx.span, 1e3, "mm"));
        if (kind == "four-point") {
            h += row("Loading span", mm(fx.loadingSpan, 1e3, "mm"));
            h += row("Deflection measured at",
                     !fx.deflectionAt ? QStringLiteral("not stated (the column was a strain)")
                     : *fx.deflectionAt == Fixture::DeflectionAt::Midspan
                         ? QStringLiteral("mid-span")
                         : QStringLiteral("the loading points"));
        }
    } else {
        h += row("Cross-section S0", mm(crossSection(sp).area, 1e6, "mm2"));
        h += row(mode == "compression" ? "Height or gauge length L0" : "Gauge length Le",
                 mm(sp.gaugeLength, 1e3, "mm"));
    }
    if (!sp.notes.isEmpty())
        h += row("Notes", sp.notes);
    h += "</table>\n";

    if (!chartSrc.isEmpty())
        h += "<h2>Stress and strain</h2>\n<p><img src=\"" + esc(chartSrc)
             + "\" width=\"640\"></p>\n";

    h += "<h2>Results</h2>\n<table width=\"100%\">\n<tr><th width=\"34%\">Result</th>"
         "<th>Symbol</th><th>Value</th><th>Unit</th><th>How, or why not</th></tr>\n";
    for (const QJsonValue &v : doc.value("results").toArray()) {
        const QJsonObject r = v.toObject();
        const bool det = r.value("status").toString() == "determined";
        const QJsonObject rep = r.value("reported").toObject();
        h += "<tr><td>" + esc(r.value("name").toString().isEmpty() ? r.value("id").toString()
                                                                  : r.value("name").toString())
             + "</td><td>" + esc(r.value("id").toString()) + "</td>";
        if (det)
            h += "<td>" + esc(rep.value("value").toString()) + "</td><td>"
                 + esc(rep.value("unit").toString()) + "</td><td>"
                 + esc(r.value("how").toObject().value("primitive").toString()) + "</td>";
        else
            h += "<td>-</td><td></td><td>not determined: " + esc(r.value("reason").toString())
                 + "</td>";
        h += "</tr>\n";
    }
    h += "</table>\n";

    const QJsonArray flags = doc.value("flags").toArray();
    if (!flags.isEmpty()) {
        h += "<h2>Flags</h2>\n<ul>\n";
        for (const QJsonValue &f : flags)
            h += "<li>" + esc(f.toString()) + "</li>\n";
        h += "</ul>\n";
    }

    const QJsonArray validity = doc.value("validity").toArray();
    if (!validity.isEmpty()) {
        h += "<h2>Specimen and fixture checks</h2>\n<table>\n";
        for (const QJsonValue &v : validity) {
            const QJsonObject c = v.toObject();
            h += row(c.value("status").toString(), c.value("detail").toString());
        }
        h += "</table>\n";
    }

    h += "<h2>Provenance</h2>\n<table>\n";
    h += row("Data file", QFileInfo(source.value("path").toString()).fileName());
    h += row("Data file SHA-256", source.value("sha256").toString());
    const QJsonObject cols = source.value("columns").toObject();
    h += row("Force column", cols.value("force").toString());
    h += row(mode == "flexure"       ? "Deflection column"
             : mode == "compression" ? "Deformation column"
                                     : "Extension column",
             cols.value("extension").toString());
    QStringList notes;
    for (const QJsonValue &n : source.value("notes").toArray())
        notes << n.toString();
    if (!notes.isEmpty())
        h += row("How the file was read", notes.join("; "));
    h += row("Profile", profile.value("id").toString() + " version "
                            + profile.value("profile_version").toString()
                            + (profile.value("shipped").toBool() ? ", shipped with TenForge"
                                                                 : ", a user profile"));
    h += row("Profile SHA-256", profile.value("sha256").toString());
    h += row("Analysed", doc.value("computed").toString());
    h += row("Software", app.value("name").toString() + " " + app.value("version").toString()
                             + " (build " + app.value("build").toString() + ")");
    h += "</table>\n";
    h += "<p>Values are rounded as the profile states; the unrounded values, fit windows and "
         "statistics are in results.json.</p>\n";
    h += "<p><small>" + esc(applicationName() + " - " + applicationSubtitle()) + "</small></p>\n";
    h += "</body></html>\n";
    return h;
}

} // namespace tf::core
