#pragma once

// Profiles: one standard procedure as data (docs/specs/profile-v1.md).
//
// loadProfile() reads a profile file, checks it names only primitives and
// quantities the engine has, and turns it into the engine's method: a
// TensionMethod for tension, a CurveMethod for compression and flexure.
// Every parameter it reads is listed with its path and whether it is
// confirmed, so results can say exactly what they rest on (tenet 9).

#include "analysis/Curve.h"
#include "analysis/Tension.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace tf::core {

struct ParameterUse {
    QString path;      // e.g. "results.E.method.window.from"
    bool confirmed = false;
    QString document;  // source document
};

struct ProfileResult {
    QString id, symbol, name;
    QString quantity;     // the engine's id: "E", "Rp0.2", "Rm", ...
    QString unit;         // SI
    QString primitive;
    QString reportUnit;
    double rounding = 0;  // step in the report unit; 0 means none given
    // The key of this result in the engine's answer: the quantity for
    // tension ("Rm"), the result's own id for the curve modes.
    QString key;
    // A stress at a deflection given as a multiple of the specimen's
    // thickness (ISO 178's conventional deflection, 1.5 h). The strain it
    // stands for depends on the specimen, so the session fills the
    // request's strain in before each analysis. 0: not such a result.
    double deflectionPerThickness = 0;
};

// A check on the specimen and fixture that decides whether results are
// reportable to the standard: numerator / denominator within [min, max].
// The quantities: width, thickness, span, loading_span, length.
struct ValidityCheck {
    QString id, numerator, denominator;
    double min = 0, max = 0;
};

struct Profile {
    QString id, title, profileVersion;
    QString mode;          // "tension", "compression" or "flexure"
    // How the specimen is held or loaded: "grips", "platens", "three-point"
    // or "four-point" (test-record-v1, 5.3).
    QString fixture;
    QString designation;   // the standard, e.g. "ISO 6892-1:2019"; empty when none named
    // How a person names it: "ISO 6892-1:2019, Metallic materials, ...".
    QString displayName() const
    {
        return designation.isEmpty() ? title : designation + ", " + title;
    }
    QString path;          // where it was read from (":/profiles/..." when shipped)
    bool shipped = false;
    QByteArray sha256;     // of the file's bytes, hex
    tf::analysis::TensionMethod method;   // mode "tension"
    tf::analysis::CurveMethod curve;      // modes "compression" and "flexure"
    std::vector<ValidityCheck> validity;
    std::vector<ProfileResult> results;
    std::vector<ParameterUse> parameters;
};

struct ProfileOrError {
    std::optional<Profile> profile;
    QString error;
};

// A path to a profile file, or the id of a profile shipped with TenForge.
ProfileOrError loadProfile(const QString &pathOrId);

// Ids of the profiles shipped with TenForge.
QStringList shippedProfiles();

} // namespace tf::core
