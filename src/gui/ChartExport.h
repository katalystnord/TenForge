#pragma once

// The chart as a figure: one SVG, written by VTK from the chart exactly as
// it is drawn (curve, regions, labels, notes, modulus window), and every
// other form made from that SVG, so they cannot differ:
//  - .svg as is (vector);
//  - .pdf as one page the chart's shape, drawn by Qt SVG (vector);
//  - .png drawn from the SVG at a chosen width (raster);
//  - the PDF report, whose chart is drawn from the SVG as vectors too.

#include <QByteArray>
#include <QImage>
#include <QString>

class QTextDocument;
class vtkRenderWindow;

namespace tf::gui {

// The chart in the render window as SVG. Empty, with why, if it cannot be
// written (e.g. the window was never drawn).
QByteArray chartSvg(vtkRenderWindow *window, QString *error);

QImage svgImage(const QByteArray &svg, int widthPx);

// Writes the SVG as .svg, .pdf or .png by the path's suffix. Returns an
// error, empty on success.
QString writeChart(const QByteArray &svg, const QString &path, int pngWidthPx = 2400);

// Makes the document draw the image named `name` from the SVG, as vectors,
// whatever it prints to.
void drawImageAsSvg(QTextDocument *doc, const QString &name, const QByteArray &svg);

} // namespace tf::gui
