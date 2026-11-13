#pragma once

// What the engine answers, whatever the mode: one Determination per
// result, each either a value with how it was found, or the reason it
// could not be (results-v1.md, principle 2).

#include "analysis/Primitives.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tf::analysis {

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

struct AnalysisResult {
    // Keyed by result id. Strains are fractions (0.05 is 5 %).
    std::map<std::string, Determination> results;
    std::vector<std::string> flags;
    double strainOrigin = 0;     // the zero point used, as strain
};

} // namespace tf::analysis
