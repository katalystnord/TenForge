#pragma once

// The curve model: stress and strain in, the results compression and
// bending standards ask for out.
//
// Standard-neutral, like the tension model. The stress and strain arrive
// already computed by a mode's model (Models.h, or force over area); which
// results are wanted and every number that steers how they are found come
// in CurveMethod, filled from a profile by tf_core.

#include "analysis/Primitives.h"
#include "analysis/Result.h"

#include <string>
#include <vector>

namespace tf::analysis {

struct CurveInput {
    std::vector<double> force;   // N, positive in the direction of the test
    std::vector<double> stress;  // Pa, same samples
    std::vector<double> strain;  // 1, same samples; NaN where not measured
};

enum class CurveQuantity {
    Modulus,         // slope of the straight part (Pa)
    MaxStress,       // the largest stress (Pa)
    StrainAtMax,     // strain at the largest stress
    MaxForce,        // the largest force (N)
    YieldStress,     // first maximum the stress then stays below (Pa)
    YieldStrain,
    StressAtStrain,  // stress where the strain first reaches `at`
    BreakStress,     // at the break, by force drop
    BreakStrain,
    PeakStress,      // a maximum before strain `at`, when there is one
    StrainAtPeak,
};

struct CurveRequest {
    std::string id;           // the result's id, as the profile names it
    CurveQuantity quantity = CurveQuantity::MaxStress;
    double at = 0;            // strain, for StressAtStrain and the Peak pair
};

struct CurveMethod {
    // Where strains are counted from.
    //  AsRecorded: the recorded zero.
    //  ElasticLine: where the modulus line meets zero stress (toe
    //   compensation); every strain result is measured from there.
    enum class Zero { AsRecorded, ElasticLine };
    Zero zero = Zero::AsRecorded;

    // The modulus.
    //  StrainWindow: a least-squares line between two strains.
    //  MaxSlope: the steepest least-squares line over any stretch of
    //   maxSlopeWidth in strain, before the maximum force.
    enum class Modulus { StrainWindow, MaxSlope };
    Modulus modulus = Modulus::StrainWindow;
    double modulusFrom = 0.0005;
    double modulusTo = 0.0025;
    double maxSlopeWidth = 0.005;
    // A strain window given on the strain as recorded (as a chart plots
    // it) rather than counted from the zero point: a window a person
    // chose on the chart. The zero point is still found from the line.
    bool windowOnRecordedStrain = false;

    // Yield: the first local maximum after which the stress falls by at
    // least yieldMinDrop (fraction of it) and stays below it over at least
    // yieldMinSpan of strain.
    double yieldMinDrop = 0.005;
    double yieldMinSpan = 0.005;

    // A peak before strain `at` counts only if the stress at `at` is lower
    // than it by at least peakMinDrop (fraction of the peak).
    double peakMinDrop = 0.005;

    // Break by force drop (see fracture() in Primitives.h).
    double breakRatio = 5.0;
    double breakMinDrop = 0.10;

    std::vector<CurveRequest> requests;
};

AnalysisResult analyseCurve(const CurveInput &in, const CurveMethod &m);

} // namespace tf::analysis
