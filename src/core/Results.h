#pragma once

// results.json and results.csv (docs/specs/results-v1.md).
//
// Turns the engine's answer, the profile that asked the question and the
// file it was asked of into the results document: every value with how it
// was made, every gap with its reason, and the analysis's conformity
// stated, never implied (tenet 9).

#include "analysis/Tension.h"
#include "core/Profile.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace tf::core {

struct SourceInfo {
    QString path;
    QString sha256;
    QString forceColumn, extensionColumn;
    QStringList notes;
};

// A value in SI as the profile reports it: converted to the report unit and
// rounded to the profile's step, as text so the rounding survives.
// Empty when the unit cannot be converted.
QString reportedText(double siValue, const ProfileResult &r);

QJsonObject resultsJson(const Profile &profile, const tf::analysis::TensionResult &result,
                        const SourceInfo &source, const QDateTime &computedUtc);

// The spreadsheet view: one row per result.
QString resultsCsv(const QJsonObject &results);

} // namespace tf::core
