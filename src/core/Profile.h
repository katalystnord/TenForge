#pragma once

// Profiles: one standard procedure as data (docs/specs/profile-v1.md).
//
// loadProfile() reads a profile file, checks it names only primitives and
// quantities the engine has, and turns it into the engine's TensionMethod.
// Every parameter it reads is listed with its path and whether it is
// confirmed, so results can say exactly what they rest on (tenet 9).

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
};

struct Profile {
    QString id, title, profileVersion, mode;
    QString path;          // where it was read from (":/profiles/..." when shipped)
    bool shipped = false;
    QByteArray sha256;     // of the file's bytes, hex
    tf::analysis::TensionMethod method;
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
