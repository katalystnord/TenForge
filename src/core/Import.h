#pragma once

// Reading a testing machine's export: a delimited text file of columns.
//
// Machines write these in many dialects: comma or semicolon or tab, decimal
// point or comma, a preamble of key;value lines, a units row under the
// names or units inside the names. readTable() finds all of that by itself
// and keeps the file's own names; nothing is renamed or resampled
// (tenet 10). toTension() then maps two of its columns to force and
// extension, in SI, as the analysis expects.

#include "analysis/Tension.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace tf::core {

struct PreambleEntry {
    QString key;
    QString value;
    QString unit;
};

struct Table {
    QString path;
    QByteArray sha256;                         // of the file as read, hex
    QStringList names;                         // one per column, as written
    QStringList units;                         // one per column; empty when none given
    std::vector<std::vector<double>> columns;  // NaN for an empty cell
    std::vector<PreambleEntry> preamble;       // key;value[;unit] lines above the header
    QChar delimiter;
    QChar decimal;
};

struct TableOrError {
    std::optional<Table> table;
    QString error;
};

TableOrError readTable(const QString &path);

// A value with its unit, e.g. "120.6 mm2" or "50mm". The unit is required.
std::optional<double> parseQuantity(const QString &text, const QString &dimension);

// The SI factor for a unit of a dimension ("force", "length", "area"), or
// nothing when the unit is not known for that dimension.
std::optional<double> siFactor(const QString &unit, const QString &dimension);

struct TensionMapping {
    QString force;            // column name; empty: found by name
    QString extension;
    QString forceUnit;        // overrides the file's; empty: from the file
    QString extensionUnit;
    std::optional<double> area;         // m^2; empty: from the preamble (So)
    std::optional<double> gaugeLength;  // m; empty: from the preamble (Le)
    // False for the curve modes, whose geometry is not an area and a gauge
    // length: the columns and units are mapped and the geometry left at 0
    // (the gauge length at 1 for a strain column).
    bool needsGeometry = true;
};

struct TensionImport {
    tf::analysis::TensionInput input;
    QString forceColumn, extensionColumn;
    QString forceUnit, extensionUnit;  // as used, from the file or the mapping
    bool extensionIsStrain = false;    // the column was a strain (%): gauge length 1
    QStringList notes;   // how each choice was made, for the report
    QString error;       // non-empty: nothing usable was produced
};

TensionImport toTension(const Table &table, const TensionMapping &mapping);

// The pieces of toTension() a person confirms one by one in the window.
//
// The column whose name says it is force (or extension), by the same rule
// toTension() uses; empty when no column or several fit, with why.
QString guessForceColumn(const Table &table, QString *why = nullptr);
QString guessExtensionColumn(const Table &table, QString *why = nullptr);
// The unit the file states for a column (units row, or a known unit after
// the last underscore of its name); empty when it states none.
QString columnUnit(const Table &table, const QString &column);
// A quantity the file's preamble states under one of keys, in SI; key is
// set to the key found.
std::optional<double> preambleQuantity(const Table &table, const QStringList &keys,
                                       const QString &dimension, QString *key = nullptr);
// The units TenForge reads for a dimension ("force", "length", "strain"),
// most common first.
QStringList knownUnits(const QString &dimension);

} // namespace tf::core
