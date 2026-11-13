#pragma once

// One analysis as the window presents it, without the window.
//
// A session holds the procedure (profile), the file, which columns are
// force and displacement and in which units, the specimen and fixture, and
// an optional machine compliance table. The procedure comes first because
// its mode (tension, compression, flexure) decides what else is needed.
// next() says in words what is still needed, one thing at a time, so the
// screen can show the user exactly one next step; analyse() runs once
// nothing is missing.
// The window binds its fields to this and holds no logic of its own
// (engineering principle 5: headless core, thin GUI).

#include "analysis/Curve.h"
#include "analysis/Tension.h"
#include "core/Import.h"
#include "core/Profile.h"
#include "core/Specimen.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace tf::core {

struct Step {
    bool ready = false;
    QString field;  // which input is wanted: profile, data, forceColumn,
                    // forceUnit, extensionColumn, extensionUnit, specimen,
                    // gaugeLength, span, loadingSpan, deflectionAt, compliance
    QString text;   // the next step, as the user reads it
};

struct ColumnMapping {
    QString forceColumn, extensionColumn;
    QString forceUnit, extensionUnit;  // as the user confirmed them
};

struct Analysis {
    Profile profile;
    TensionImport imported;            // the file's columns in SI, signs as recorded
    tf::analysis::AnalysisResult result;
    // The curve the results were found on, as analysed: stress (Pa) and
    // strain (1) per sample, compression and bending taken as positive.
    std::vector<double> stress, strain;
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
    Fixture fixture;
    QString profile;  // a shipped profile's id or a profile file's path

    // A machine compliance table: a file of force against the machine's own
    // displacement, taken out of the displacement before strain is
    // computed. Empty path: none. Returns an error, and keeps the previous
    // table, if the file is not a usable table.
    QString setCompliance(const QString &path);
    QString compliance() const { return m_compliancePath; }

    // The chosen procedure's mode, or empty while none is chosen or it
    // cannot be read: "tension", "compression", "flexure".
    QString mode() const;
    // The chosen procedure's fixture kind: "grips", "platens", "three-point",
    // "four-point"; empty as for mode().
    QString fixtureKind() const;
    // The word for the displacement column in this mode: "extension",
    // "deformation" or "deflection".
    QString displacementWord() const;

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
    // The curve for the compression and flexure modes, or why not.
    struct Prepared {
        QString error;
        TensionImport imported;
        tf::analysis::CurveInput input;
        QStringList notes;
    };
    Prepared prepareCurve(const Profile &p) const;
    QJsonArray validity(const Profile &p) const;

    std::optional<Table> m_table;
    QString m_recordDir;
    QStringList m_notes, m_notices, m_unlisted;
    std::optional<Analysis> m_analysis;
    QString m_compliancePath;
    std::vector<double> m_complianceForce, m_complianceDisplacement;  // N, m
};

} // namespace tf::core
