#include "gui/ChartExport.h"

#include <QAbstractTextDocumentLayout>
#include <QFile>
#include <QFileInfo>
#include <QPageSize>
#include <QPaintDevice>
#include <QPainter>
#include <QPdfWriter>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextObjectInterface>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <cmath>
#include <optional>
#include <vector>

#include <vtkNew.h>
#include <vtkRenderWindow.h>
#include <vtkSVGExporter.h>

namespace tf::gui {

namespace {

// VTK 9.5's SVG exporter draws each chart's plots inside
//   <g transform="matrix(sx,0,0,sy,tx,ty)" clip-path="url(#...)">
// with the clip rectangle in page coordinates and line widths and marker
// sizes already in page pixels. Renderers that follow the SVG spec
// (browsers, Inkscape, librsvg) apply the clip in the transformed space,
// so the curve is clipped away, and they stretch every stroke and marker
// by the (very uneven) scale. Seen 2026-11-13: the exported chart had axes
// and labels but no curve; with the clip moved, the curve showed as a band
// and the markers as long ellipses.
//
// So the scale is applied to the coordinates here and the transform
// dropped: points of polylines and polygons, path coordinates (M, L and Z
// only, which is what VTK writes), line ends, rectangle corners, circle
// and marker centres. Stroke widths and marker sizes stay as written. A
// group whose transform is not a plain scale and translation, or that
// holds anything else, is left transformed, with the clip still moved
// out.
struct Affine {
    double sx = 1, sy = 1, tx = 0, ty = 0;
    double x(double v) const { return sx * v + tx; }
    double y(double v) const { return sy * v + ty; }
};

std::optional<Affine> scaleTranslate(const QString &transform)
{
    static const QRegularExpression re(
        R"(^\s*matrix\(\s*([-+0-9.eE]+)[\s,]+([-+0-9.eE]+)[\s,]+([-+0-9.eE]+)[\s,]+)"
        R"(([-+0-9.eE]+)[\s,]+([-+0-9.eE]+)[\s,]+([-+0-9.eE]+)\s*\)\s*$)");
    const auto m = re.match(transform);
    if (!m.hasMatch() || m.captured(2).toDouble() != 0 || m.captured(3).toDouble() != 0)
        return std::nullopt;
    return Affine{m.captured(1).toDouble(), m.captured(4).toDouble(), m.captured(5).toDouble(),
                  m.captured(6).toDouble()};
}

QString num(double v) { return QString::number(v, 'g', 7); }

// "x,y x,y ..." mapped pair by pair.
std::optional<QString> mapPoints(const QString &pts, const Affine &t)
{
    static const QRegularExpression sep(R"([\s,]+)");
    const QStringList v = pts.trimmed().split(sep, Qt::SkipEmptyParts);
    if (v.size() % 2)
        return std::nullopt;
    QStringList out;
    for (int i = 0; i + 1 < v.size(); i += 2)
        out << num(t.x(v[i].toDouble())) + "," + num(t.y(v[i + 1].toDouble()));
    return out.join(' ');
}

// A path of M, L and Z with absolute coordinates.
std::optional<QString> mapPath(const QString &d, const Affine &t)
{
    static const QRegularExpression tok(R"([MLZmlz]|[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)");
    static const QRegularExpression other(R"([A-DF-KN-Ya-df-kn-y])");  // any other command
    if (other.match(d).hasMatch())
        return std::nullopt;
    QStringList out;
    std::optional<double> x;
    auto it = tok.globalMatch(d);
    while (it.hasNext()) {
        const QString s = it.next().captured(0);
        if (s.front().isLetter()) {
            if (s == "m" || s == "l")
                return std::nullopt;  // relative coordinates: not mapped
            out << s;
            continue;
        }
        if (!x) {
            x = s.toDouble();
        } else {
            out << num(t.x(*x)) + "," + num(t.y(s.toDouble()));
            x.reset();
        }
    }
    if (x)
        return std::nullopt;
    return out.join(' ');
}

// The element's attributes with the transform applied, or nothing if the
// element is not one that can be mapped exactly.
std::optional<QXmlStreamAttributes> mapped(const QStringView name,
                                           const QXmlStreamAttributes &attrs, const Affine &t)
{
    QXmlStreamAttributes out;
    auto d = [&](const char *k) { return attrs.value(QLatin1String(k)).toDouble(); };
    for (const auto &a : attrs) {
        const QString k = a.qualifiedName().toString();
        const QString v = a.value().toString();
        if ((name == u"polyline" || name == u"polygon") && k == "points") {
            const auto p = mapPoints(v, t);
            if (!p)
                return std::nullopt;
            out.append(k, *p);
        } else if (name == u"path" && k == "d") {
            const auto p = mapPath(v, t);
            if (!p)
                return std::nullopt;
            out.append(k, *p);
        } else if (name == u"line" && (k == "x1" || k == "x2")) {
            out.append(k, num(t.x(v.toDouble())));
        } else if (name == u"line" && (k == "y1" || k == "y2")) {
            out.append(k, num(t.y(v.toDouble())));
        } else if ((name == u"circle" || name == u"ellipse") && k == "cx") {
            out.append(k, num(t.x(v.toDouble())));
        } else if ((name == u"circle" || name == u"ellipse") && k == "cy") {
            out.append(k, num(t.y(v.toDouble())));
        } else if (name == u"use" && k == "x") {
            // A marker: its size is in page pixels; only its centre moves.
            const double w = d("width");
            out.append(k, num(t.x(v.toDouble() + w / 2) - w / 2));
        } else if (name == u"use" && k == "y") {
            const double h = d("height");
            out.append(k, num(t.y(v.toDouble() + h / 2) - h / 2));
        } else if (name == u"rect" && (k == "x" || k == "y" || k == "width" || k == "height")) {
            const double x0 = t.x(d("x")), x1 = t.x(d("x") + d("width"));
            const double y0 = t.y(d("y")), y1 = t.y(d("y") + d("height"));
            if (k == "x")
                out.append(k, num(std::min(x0, x1)));
            else if (k == "y")
                out.append(k, num(std::min(y0, y1)));
            else if (k == "width")
                out.append(k, num(std::fabs(x1 - x0)));
            else
                out.append(k, num(std::fabs(y1 - y0)));
        } else if (k == "transform") {
            return std::nullopt;  // nested transforms: left alone
        } else {
            out.append(k, v);
        }
    }
    if (name != u"polyline" && name != u"polygon" && name != u"path" && name != u"line"
        && name != u"circle" && name != u"ellipse" && name != u"use" && name != u"rect"
        && name != u"g")
        return std::nullopt;
    return out;
}

QByteArray flattenPlotTransforms(const QByteArray &svg)
{
    // First pass: which transformed groups hold only mappable elements.
    // Groups are numbered in document order.
    std::vector<bool> flat;
    {
        QXmlStreamReader in(svg);
        in.setNamespaceProcessing(false);
        struct Open {
            int group;  // index into flat, or -1
            std::optional<Affine> t;
        };
        std::vector<Open> stack;
        while (!in.atEnd()) {
            in.readNext();
            if (in.isStartElement()) {
                std::optional<Affine> t = stack.empty() ? std::nullopt : stack.back().t;
                int group = stack.empty() ? -1 : stack.back().group;
                if (in.name() == u"g" && in.attributes().hasAttribute("transform")) {
                    const auto a = scaleTranslate(in.attributes().value("transform").toString());
                    group = int(flat.size());
                    flat.push_back(a.has_value() && !t);
                    t = a;
                } else if (t && group >= 0 && !mapped(in.name(), in.attributes(), *t)) {
                    flat[std::size_t(group)] = false;
                }
                stack.push_back({group, t});
            } else if (in.isEndElement() && !stack.empty()) {
                stack.pop_back();
            }
        }
        if (in.hasError())
            return svg;
    }

    // Second pass: write it out, flattening the groups that can be.
    QXmlStreamReader in(svg);
    in.setNamespaceProcessing(false);
    QByteArray out;
    QXmlStreamWriter w(&out);
    struct Open {
        std::optional<Affine> t;  // the transform applied to coordinates here
        bool extraClose = false;
    };
    std::vector<Open> stack;
    int groups = 0;
    auto writeAttrs = [&](const QXmlStreamAttributes &attrs, bool skipClip, bool skipTransform) {
        // (attrs is the element's own list; the text rules below read it.)
        for (const auto &ns : in.namespaceDeclarations())
            w.writeAttribute(ns.prefix().isEmpty() ? QStringLiteral("xmlns")
                                                   : "xmlns:" + ns.prefix().toString(),
                             ns.namespaceUri().toString());
        for (const auto &a : attrs) {
            if ((skipClip && a.qualifiedName() == u"clip-path")
                || (skipTransform && a.qualifiedName() == u"transform"))
                continue;
            // VTK's exporter names "URW Gothic L" at "12pt", but draws on
            // screen in its own Arial-like face at 12 pixels; text sized for
            // the screen (the legend's box) overflowed. Name what the
            // screen uses.
            if (a.qualifiedName() == u"font-family") {
                w.writeAttribute("font-family", "Arial, Helvetica, 'Liberation Sans', sans-serif");
                continue;
            }
            // VTK writes text-anchor "left"/"right", which SVG does not
            // have (start/end), and its vertical alignment as
            // alignment-baseline, which SVG ignores on <text>; so tick
            // numbers sat on the axis instead of beside it.
            if (a.qualifiedName() == u"text-anchor") {
                const QString v = a.value().toString();
                w.writeAttribute("text-anchor", v == "right" ? "end" : v == "left" ? "start" : v);
                continue;
            }
            if (a.qualifiedName() == u"alignment-baseline")
                continue;  // folded into y below: Qt's SVG renderer has no baseline control
            if (a.qualifiedName() == u"y" && in.name() == u"text") {
                // VTK's vertical alignment as a shift of the baseline, in
                // the text's own frame (before its rotation): top puts the
                // baseline an ascent below y, centre a third of an em,
                // bottom a descent above.
                const QString align = attrs.value("alignment-baseline").toString();
                const double em = attrs.value("font-size").toString().remove("pt").remove("px").toDouble();
                const double shift = align == "top"       ? 0.8 * em
                                     : align == "central" ? 0.35 * em
                                     : align == "bottom"  ? -0.2 * em
                                                          : 0.0;
                w.writeAttribute("y", num(a.value().toDouble() + shift));
                continue;
            }
            // VTK gives text angles counter-clockwise with y up; SVG's y
            // points down, so the angle's sign flips (the rotated axis
            // title read the wrong way and ran into the tick numbers).
            if (a.qualifiedName() == u"transform" && in.name() == u"text") {
                static const QRegularExpression rot(R"(rotate\(\s*([-+0-9.eE]+)\s*\))");
                QString v = a.value().toString();
                const auto m = rot.match(v);
                if (m.hasMatch())
                    v.replace(m.capturedStart(0), m.capturedLength(0),
                              "rotate(" + num(-m.captured(1).toDouble()) + ")");
                w.writeAttribute("transform", v);
                continue;
            }
            if (a.qualifiedName() == u"font-size" && a.value().endsWith(u"pt")) {
                w.writeAttribute("font-size", a.value().chopped(2).toString() + "px");
                continue;
            }
            w.writeAttribute(a.qualifiedName().toString(), a.value().toString());
        }
    };
    while (!in.atEnd()) {
        in.readNext();
        switch (in.tokenType()) {
        case QXmlStreamReader::StartDocument:
            w.writeStartDocument();
            break;
        case QXmlStreamReader::StartElement: {
            const auto attrs = in.attributes();
            std::optional<Affine> t = stack.empty() ? std::nullopt : stack.back().t;
            if (in.name() == u"g" && attrs.hasAttribute("transform")) {
                const bool canFlatten = flat[std::size_t(groups++)];
                const bool clip = attrs.hasAttribute("clip-path");
                if (clip) {
                    // The clip, in page coordinates, on a group of its own.
                    w.writeStartElement(QStringLiteral("g"));
                    w.writeAttribute("clip-path", attrs.value("clip-path").toString());
                }
                w.writeStartElement(QStringLiteral("g"));
                writeAttrs(attrs, true, canFlatten);
                stack.push_back({canFlatten ? scaleTranslate(attrs.value("transform").toString())
                                            : std::nullopt,
                                 clip});
                break;
            }
            QXmlStreamAttributes use = attrs;
            if (t)
                if (const auto m = mapped(in.name(), attrs, *t))
                    use = *m;
            // A circle marker is a <use> of VTK's "Circle" symbol, which
            // Qt's SVG renderer (SVG Tiny) does not draw: written as the
            // circle it stands for.
            if (in.name() == u"use" && use.value("xlink:href") == u"#Circle") {
                const double x = use.value("x").toDouble(), y = use.value("y").toDouble();
                const double wd = use.value("width").toDouble(), ht = use.value("height").toDouble();
                w.writeStartElement(QStringLiteral("circle"));
                w.writeAttribute("cx", num(x + wd / 2));
                w.writeAttribute("cy", num(y + ht / 2));
                w.writeAttribute("r", num(std::min(wd, ht) / 2));
                for (const auto &a : use)
                    if (a.qualifiedName() != u"x" && a.qualifiedName() != u"y"
                        && a.qualifiedName() != u"width" && a.qualifiedName() != u"height"
                        && a.qualifiedName() != u"xlink:href")
                        w.writeAttribute(a.qualifiedName().toString(), a.value().toString());
            } else {
                w.writeStartElement(in.qualifiedName().toString());
                writeAttrs(use, false, false);
            }
            stack.push_back({t, false});
            break;
        }
        case QXmlStreamReader::EndElement:
            w.writeEndElement();
            if (!stack.empty()) {
                if (stack.back().extraClose)
                    w.writeEndElement();
                stack.pop_back();
            }
            break;
        case QXmlStreamReader::Characters:
            w.writeCharacters(in.text().toString());
            break;
        case QXmlStreamReader::Comment:
            w.writeComment(in.text().toString());
            break;
        case QXmlStreamReader::EndDocument:
            w.writeEndDocument();
            break;
        default:
            break;
        }
    }
    return in.hasError() ? svg : out;
}

} // namespace

QByteArray chartSvg(vtkRenderWindow *window, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return QByteArray();
    };
    if (!window)
        return fail("there is no chart");
    const int *size = window->GetSize();
    if (size[0] <= 0 || size[1] <= 0)
        return fail("the chart has not been drawn yet");
    QTemporaryDir dir;
    if (!dir.isValid())
        return fail("no temporary folder to write the chart through");
    const QString path = dir.filePath("chart.svg");
    window->Render();
    vtkNew<vtkSVGExporter> svg;
    svg->SetRenderWindow(window);
    svg->SetFileName(path.toLocal8Bit().constData());
    svg->SetTitle("TenForge chart");
    svg->DrawBackgroundOn();
    svg->TextAsPathOff();  // text stays text: searchable, editable in a drawing program
    svg->Write();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return fail("VTK wrote no SVG");
    const QByteArray bytes = f.readAll();
    if (!bytes.contains("<svg"))
        return fail("VTK's SVG is empty");
    return flattenPlotTransforms(bytes);
}

QImage svgImage(const QByteArray &svg, int widthPx)
{
    QSvgRenderer r(svg);
    if (!r.isValid())
        return {};
    const QSizeF s = r.defaultSize();
    if (s.width() <= 0)
        return {};
    QImage img(widthPx, int(widthPx * s.height() / s.width() + 0.5), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    r.render(&p);
    return img;
}

QString writeChart(const QByteArray &svg, const QString &path, int pngWidthPx)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "svg") {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(svg) != svg.size())
            return "cannot write " + path;
        return {};
    }
    if (suffix == "png") {
        const QImage img = svgImage(svg, pngWidthPx);
        if (img.isNull())
            return "the chart could not be drawn";
        return img.save(path, "PNG") ? QString() : "cannot write " + path;
    }
    if (suffix == "pdf") {
        QSvgRenderer r(svg);
        if (!r.isValid())
            return "the chart could not be read back";
        // One page the chart's shape, 170 mm wide (a report column).
        const QSizeF s = r.defaultSize();
        const double wMm = 170.0, hMm = wMm * s.height() / s.width();
        QPdfWriter pdf(path);
        pdf.setPageSize(QPageSize(QSizeF(wMm, hMm), QPageSize::Millimeter));
        pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
        pdf.setTitle("TenForge chart");
        QPainter p;
        if (!p.begin(&pdf))
            return "cannot write " + path;
        r.render(&p, QRectF(0, 0, pdf.width(), pdf.height()));
        p.end();
        return QFileInfo(path).size() > 0 ? QString() : "cannot write " + path;
    }
    return "a chart is exported as .svg, .pdf or .png";
}

namespace {

// Draws one named image from an SVG, as vectors, in place of QTextDocument's
// own image handling (which rasterises).
class SvgImageHandler : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    SvgImageHandler(QObject *parent, QString name, QByteArray svg)
        : QObject(parent), m_name(std::move(name)), m_renderer(svg)
    {
    }

    QSizeF intrinsicSize(QTextDocument *doc, int, const QTextFormat &format) override
    {
        const QTextImageFormat f = format.toImageFormat();
        const QSizeF s = m_renderer.defaultSize();
        double w = f.width() > 0 ? f.width() : s.width();
        double h = f.height() > 0 ? f.height() : (s.width() > 0 ? w * s.height() / s.width() : 0);
        // Sizes are in pixels at 96 dpi; a printer lays out in its own
        // units, as Qt's own image handler allows for (without this the
        // chart printed as a thumbnail).
        double scale = 1.0;
        if (QPaintDevice *pd = doc->documentLayout()->paintDevice())
            scale = pd->logicalDpiY() / 96.0;
        return {w * scale, h * scale};
    }

    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *, int,
                    const QTextFormat &format) override
    {
        if (format.toImageFormat().name() == m_name && m_renderer.isValid())
            m_renderer.render(painter, rect);
    }

private:
    QString m_name;
    QSvgRenderer m_renderer;
};

} // namespace

void drawImageAsSvg(QTextDocument *doc, const QString &name, const QByteArray &svg)
{
    doc->documentLayout()->registerHandler(QTextFormat::ImageObject,
                                           new SvgImageHandler(doc, name, svg));
}

} // namespace tf::gui

#include "ChartExport.moc"
