#pragma once

// The tension model: force and extension in, the tensile properties out.
//
// Standard-neutral. Which properties are wanted, and every number that
// steers how they are found, arrive in TensionMethod, filled from a profile
// by tf_core. The defaults below are only there so a test can state what it
// changes; a profile always sets them, and says where each came from.

#include "analysis/Primitives.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tf::analysis {

struct TensionInput {
    std::vector<double> force;      // N, one per sample
    std::vector<double> extension;  // m, of the gauge length, same samples
    double area = 0;                // m^2, original cross-section S0
    double gaugeLength = 0;         // m, extensometer gauge length Le
};

struct TensionMethod {
    // Slope of the elastic part (E): a least-squares line over a window.
    //  Axis::Stress: the window is given as fractions of a reference stress,
    //   the proof stress (or the upper yield strength when there is one).
    //   Found by iteration, since the window depends on the proof stress and
    //   the proof stress on the slope.
    //  Axis::Strain: the window is two strains, as given (e.g. 0.0005 and
    //   0.0025 for plastics).
    enum class Axis { Stress, Strain };
    Axis modulusAxis = Axis::Stress;
    double modulusFrom = 0.10;
    double modulusTo = 0.40;
    double modulusReferenceOffset = 0.002;  // proof stress used as reference
    int modulusIterations = 30;

    // Zero point: where the elastic line meets zero stress. Strains of the
    // results are measured from it when true, from the recorded zero if not.
    bool strainFromElasticLine = true;

    // Proof strengths wanted, as plastic strain offsets (0.002 is Rp0.2).
    std::vector<double> proofOffsets{0.002};

    // Discontinuous yield (ReH, ReL).
    bool yieldPoint = true;
    double yieldMinDrop = 0.01;        // fraction of the peak stress
    double yieldMinSpan = 0.0025;      // strain the stress stays below the peak
    double yieldMaxPlastic = 0.005;    // non-proportional strain at the peak, at most
    bool lowerYieldIgnoresTransient = false;

    // Fracture by force drop (see fracture() in Primitives.h).
    double fractureRatio = 5.0;
    double fractureMinDrop = 0.10;     // fraction of the maximum force
};

// What a single result is and how it was found.
struct Determination {
    bool determined = false;
    double value = 0;            // SI
    std::string unit;            // SI unit symbol: "Pa", "N", "1"
    std::string reason;          // when not determined
    std::string primitive;       // the engine primitive that produced it
    std::optional<LinearFit> fit;
    std::optional<std::pair<double, double>> window;  // fit window, on windowAxis
    std::string windowAxis;                           // "stress" (Pa) or "strain" (1)
    std::optional<CurvePoint> point;                  // on the stress-strain curve
};

struct TensionResult {
    // Keyed by result id: "E", "Rp0.2", "ReH", "ReL", "Fm", "Rm", "Agt", "Ag",
    // "At", "A", "Rb" (stress at fracture). Strains are fractions (0.05 is
    // 5 %).
    std::map<std::string, Determination> results;
    std::vector<std::string> flags;
    double strainOrigin = 0;     // the zero point used, as strain
};

// The proof-strength id for an offset: 0.002 -> "Rp0.2", 0.001 -> "Rp0.1".
std::string proofId(double offset);

TensionResult analyseTension(const TensionInput &in, const TensionMethod &m);

} // namespace tf::analysis
