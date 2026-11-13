#pragma once

// Stress and strain models: how force and a displacement become stress and
// strain for a mode (ROADMAP, section 3.1, item 2), and the machine
// compliance correction that may come before them (item 1).
//
// Axial models (tension, compression) are a division by the area and by the
// length the displacement is measured over; tf_core does those inline. The
// bending model has geometry worth a function of its own.

#include "analysis/Primitives.h"

#include <vector>

namespace tf::analysis {

// A beam on two supports, loaded at mid-span (three-point) or at two points
// symmetric about mid-span (four-point). Lengths in metres.
struct FlexureGeometry {
    double width = 0;        // b
    double thickness = 0;    // h
    double span = 0;         // L, between the supports
    double loadingSpan = 0;  // between the two loading points; 0 for three-point
    // Where the deflection was measured. A crosshead moves the loading
    // points; a deflectometer under the beam's middle measures mid-span.
    // For three-point bending the two are the same place.
    enum class DeflectionAt { LoadingPoints, Midspan };
    DeflectionAt deflectionAt = DeflectionAt::LoadingPoints;
};

// Engineering beam theory, linear elastic, small deflection. With a the
// distance from a support to the nearest loading point (L/2 for
// three-point), the surface stress between the loading points is
//   sigma = 3 F a / (b h^2)
// and the surface strain follows from the deflection s:
//   at mid-span:           strain = 12 h s / (3 L^2 - 4 a^2)
//   at the loading points: strain = 3 h s / (a (3 L - 4 a))
// For three-point (a = L/2) both are 6 h s / L^2.
//
// Factors to multiply force (N) by for stress (Pa), and deflection (m) by
// for strain (1). Refused, with why, for a geometry that is not a beam.
Outcome<double> flexureStressFactor(const FlexureGeometry &g);
Outcome<double> flexureStrainFactor(const FlexureGeometry &g);

// The displacement with the machine's own deflection taken out: each
// sample's displacement minus the machine's deflection at that sample's
// force, linearly interpolated in a table measured on the machine (forces
// ascending, both columns in SI). Refused for forces outside the table:
// a compliance is never extrapolated.
Outcome<std::vector<double>> complianceCorrected(Samples force, Samples displacement,
                                                 Samples tableForce,
                                                 Samples tableDisplacement);

} // namespace tf::analysis
