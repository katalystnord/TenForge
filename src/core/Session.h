#pragma once

// One analysis as the window presents it, without the window.
//
// A session holds the file, which columns are force and extension and in
// which units, the specimen, and the procedure (profile). next() says in
// words what is still needed, one thing at a time, so the screen can show
// the user exactly one next step; analyse() runs once nothing is missing.
// The window binds its fields to this and holds no logic of its own
// (engineering principle 5: headless core, thin GUI).

#include "analysis/Tension.h"
#include "core/Import.h"
#include "core/Profile.h"
#include "core/Specimen.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace tf::core {

struct Step {
    bool ready = false;
    QString field;  // which input is wanted: data, forceColumn, forceUnit,
                    // extensionColumn, extensionUnit, specimen, gaugeLength, profile
    QString text;   // the next step, as the user reads it
};

struct ColumnMapping {
    QString forceColumn, extensionColumn;
    QString forceUnit, extensionUnit;  // as the user confirmed them
};

struct Analysis {
    Profile profile;
    TensionImport imported;
    tf::analysis::TensionResult result;
    QJsonObject doc;  // results.json
};

class Session
{
public:
    // A machine export. On success the mapping, units and any geometry the
    // file states are filled in, and notes() says where each came from.
    QString openData(const QString &path);
    // A test record (its folder or its manifest.tfrec). Analysed at once;
    // notices() lists what changed in it and any stored result that a
    // fresh analysis does not reproduce.
    QString openRecord(const QString &dirOrManifest);

    ColumnMapping mapping;
    Specimen specimen;
    QString profile;  // a shipped profile's id or a profile file's path

    const Table *table() const { return m_table ? &*m_table : nullptr; }
    QString recordDir() const { return m_recordDir; }
    QStringList notes() const { return m_notes; }
    QStringList notices() const { return m_notices; }
    // Files in the opened record's folder its manifest does not list.
    QStringList unlisted() const { return m_unlisted; }

    Step next() const;
    bool analyse();  // false, and analysis() empty, unless next().ready
    const std::optional<Analysis> &analysis() const { return m_analysis; }

    // A new record from the current analysis, into a new or empty folder.
    QString saveRecord(const QString &dir) const;

private:
    TensionMapping tensionMapping() const;

    std::optional<Table> m_table;
    QString m_recordDir;
    QStringList m_notes, m_notices, m_unlisted;
    std::optional<Analysis> m_analysis;
};

} // namespace tf::core
