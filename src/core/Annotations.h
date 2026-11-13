#pragma once

// What a chart shows besides the curve, decided here so it is the same in
// the window, the exported figure and the report, and can be tested
// without a window.
//
//  - Regions: stretches of the curve the analysis identified (the samples
//    the modulus was fitted to, the toe a zero point leaves out, uniform
//    elongation and necking, what came after a break). Only from what the
//    analysis determined, so a figure cannot disagree with its results.
//  - Labels: each stress result, as reported, where it was found.
//  - Notes: the user's own, at a strain or over a span of strain, kept in
//    the test record.
//
// Strains here are as plotted: the analysed curve's strain (Analysis::
// strain), before any zero-point shift.

#include "core/Session.h"

#include <QJsonArray>
#include <QString>

#include <optional>
#include <vector>

namespace tf::core {

struct Region {
    QString id;      // "modulus-fit", "toe", "uniform-elongation", "necking", "after-break"
    QString label;   // as the legend shows it
    double from = 0, to = 0;
};

struct PointLabel {
    QString id;      // the result's id
    QString text;    // "Rp0.2 = 300 MPa"
    double strain = 0, stress = 0;  // where it was found; Pa
};

std::vector<Region> analysisRegions(const Analysis &a);
std::vector<PointLabel> resultLabels(const Analysis &a);

// The stress where the analysed curve first reaches a strain, or nothing
// beyond the curve. Puts a note at a strain onto the curve.
std::optional<double> stressAtStrain(const Analysis &a, double strain);

QJsonArray curveNotesJson(const std::vector<CurveNote> &notes);
std::optional<std::vector<CurveNote>> curveNotesFromJson(const QJsonArray &a, QString *error);

} // namespace tf::core
