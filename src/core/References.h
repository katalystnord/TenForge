#pragma once

// Reference curves: curves from elsewhere (a figure in a paper, digitised
// with PlotTracer) drawn beside the test's own for comparison. Never used
// by the analysis; kept in the test record with the file they came from.
//
// PlotTracer's JSON export: {"series": [{"name", "points": [{<axis label>:
// value, ...}], "relation"?}]}. It records axis labels but no units, so
// which column is strain and which is stress, and their units, are the
// user's to say; guessReferenceColumns() proposes them from the labels.

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace tf::core {

struct PlotTracerSeries {
    QString name;
    QStringList columns;   // the axis labels the points are keyed by
    QString relation;      // non-empty for an error-bar series ("upper", ...)
    int points = 0;
};

struct PlotTracerFile {
    QString path;
    QByteArray sha256;     // hex
    std::vector<PlotTracerSeries> series;
};

std::optional<PlotTracerFile> readPlotTracer(const QString &path, QString *error);

struct ReferenceColumns {
    QString strainColumn, strainUnit;  // unit "%" or "1"
    QString stressColumn, stressUnit;  // "Pa", "kPa", "MPa", "GPa", "N/mm2"
};
ReferenceColumns guessReferenceColumns(const PlotTracerSeries &s);

// The units a reference curve's columns may be in.
QStringList referenceStrainUnits();
QStringList referenceStressUnits();

struct ReferenceCurve {
    QString file;          // where it was read from
    QByteArray sha256;
    QString series;
    QString strainColumn, strainUnit, stressColumn, stressUnit;
    std::vector<double> strain, stress;  // 1 and Pa; points with a gap left out
};

// The named series as a curve, in SI, or nothing with why.
std::optional<ReferenceCurve> readReferenceCurve(const QString &path, const QString &series,
                                                 const QString &strainColumn,
                                                 const QString &strainUnit,
                                                 const QString &stressColumn,
                                                 const QString &stressUnit, QString *error);

} // namespace tf::core
