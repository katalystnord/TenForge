#pragma once

// The test record on disk (docs/specs/test-record-v1.md): one test, one
// folder, every file listed in manifest.tfrec with its SHA-256.
//
// writeRecord() keeps the imported file verbatim under source/, writes one
// channel file per mapped column with the file's raw value beside the SI
// value, and puts the results inside. openRecord() reads it back exactly,
// and says which files changed, went missing, or were added by someone
// else since the manifest was written; it never changes the record.

#include "analysis/Tension.h"
#include "core/Import.h"
#include "core/Profile.h"
#include "core/Session.h"
#include "core/Specimen.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace tf::core {

// Writes a new record into dir, which must not exist or be empty. The
// results document is the one resultsJson() made from the same import;
// its source path and record inputs are rewritten to point inside the
// record. Returns an error, empty on success.
// The fixture is written for the profile's fixture kind; a compliance table
// used in the analysis is kept byte for byte under source/.
QString writeRecord(const QString &dir, const Table &source, const TensionImport &imported,
                    const Specimen &specimen, const Profile &profile, QJsonObject results,
                    const Fixture &fixture = {}, const QString &compliancePath = {},
                    const std::optional<std::pair<double, double>> &modulusWindow = {},
                    const std::vector<CurveNote> &curveNotes = {});

struct OpenedRecord {
    QString error;                 // non-empty: the record could not be read
    QString dir;
    QString mode;                  // "tension", "compression", "flexure"
    Specimen specimen;
    Fixture fixture;
    QString complianceFile;        // relative to dir; empty when none was used
    std::optional<std::pair<double, double>> modulusWindow;  // the user's, if any
    std::vector<CurveNote> curveNotes;
    tf::analysis::TensionInput input;
    QString profileId, profileVersion;
    QByteArray profileSha256;
    QString sourceFile;            // relative to dir
    QString forceColumn, extensionColumn, forceUnit, extensionUnit;
    QJsonObject results;           // as stored
    QStringList problems;          // listed files that changed or are missing
    QStringList unlisted;          // files present that the manifest does not list
};

OpenedRecord openRecord(const QString &dir);

// Adds the files openRecord() found unlisted to the manifest, with their
// hashes, after the user agreed. Changes nothing else. Returns an error.
QString addUnlistedFiles(const QString &dir);

} // namespace tf::core
