#pragma once

// Exports for other tools (ROADMAP Phase 4; docs/specs/exports-v1.md).
// Every export is made from an analysis and says, inside the file, where
// it came from and that it is derived: the test record stays the record.
//
//  - The hardening curve: true stress against true plastic strain, from
//    initial yield to the maximum force, for FreeCAD FEM's YieldPoints and
//    CalculiX's *PLASTIC. Tension only.
//  - A FreeCAD material card (.FCMat): the measured elastic and strength
//    values, nothing that was not measured.
//  - The analysed curve as VTK XML PolyData (.vtp), for ParaView and PyVista.

#include "core/Session.h"

#include <QString>
#include <QStringList>

#include <vector>

namespace tf::core {

struct HardeningPoint {
    double trueStress = 0;     // Pa
    double plasticStrain = 0;  // 1, true
};

struct Hardening {
    std::vector<HardeningPoint> points;
    QString yieldResult;       // the result taken as initial yield: "ReL", "Rp0.2", ...
    QStringList notes;         // how it was made, for the files' comments
    QString error;             // non-empty: no curve, and why
};

// Initial yield (ReL, or else the profile's smallest proof strength) at
// plastic strain 0, then the curve's own true plastic strains from that
// point to the maximum force, `points` in all, evenly spaced in plastic
// strain; stress kept from falling (a yield drop or noise), with a note.
Hardening hardeningCurve(const Analysis &a, int points = 20);

// "stress, plastic strain" per line, MPa and 1: FreeCAD FEM's YieldPoints.
QStringList yieldPoints(const Hardening &h);

// A CalculiX *PLASTIC block (isotropic hardening unless kinematic), MPa,
// with comment lines saying where it came from.
QString calculixPlastic(const Hardening &h, const Analysis &a, bool kinematic = false);

// A FreeCAD material card (YAML), or empty with why.
QString freecadCard(const Analysis &a, const Specimen &s, QString *error);

// The analysed curve as VTK XML PolyData, ASCII: one polyline, point
// coordinates (strain %, stress MPa, 0) for a sensible view, and point
// arrays in SI: strain, stress_Pa, force_N; the results as field data.
QString curveVtp(const Analysis &a);

} // namespace tf::core
