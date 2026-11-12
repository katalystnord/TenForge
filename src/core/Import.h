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
};

struct TensionImport {
    tf::analysis::TensionInput input;
    QString forceColumn, extensionColumn;
    QStringList notes;   // how each choice was made, for the report
    QString error;       // non-empty: nothing usable was produced
};

TensionImport toTension(const Table &table, const TensionMapping &mapping);

} // namespace tf::core
