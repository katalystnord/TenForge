// Reading machine exports: the dialects TenForge meets first.
//
// The fixtures are hand-written, small, and in the layouts of the files the
// validation uses: a quoted preamble with semicolons (NPL TENSTAND's
// layout), a decimal comma with a units row in brackets and an empty cell
// where an extensometer came off (BAM's layout), and a plain CSV with the
// unit in the column name (TenForge's synthetic fixtures).

#include "core/Import.h"
#include "check.h"

#include <QCryptographicHash>
#include <QFile>

#include <cmath>

using namespace tf::core;

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

static QString fixture(const char *rel) { return QString(TF_FIXTURES) + "/" + rel; }

TF_CASE(quoted_preamble_layout_reads_names_units_preamble_and_rows)
{
    const auto r = readTable(fixture("import/quoted-preamble.txt"));
    TF_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TF_CHECK(t.delimiter == ';');
    TF_CHECK(t.decimal == '.');
    TF_CHECK(t.names == QStringList({"time", "crosshead", "extensometer", "force"}));
    TF_CHECK(t.units == QStringList({"s", "mm", "mm", "kN"}));
    TF_REQUIRE(t.columns.size() == 4);
    TF_CHECK(t.columns[3].size() == 4);
    TF_NEAR(t.columns[3][2], 2.9, 1e-12);
    bool so = false;
    for (const auto &p : t.preamble)
        if (p.key == "So" && p.value == "78.5" && p.unit == "mm2")
            so = true;
    TF_CHECK(so);
}

TF_CASE(decimal_comma_layout_reads_bracketed_units_and_keeps_empty_cells_empty)
{
    const auto r = readTable(fixture("import/decimal-comma.csv"));
    TF_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TF_CHECK(t.decimal == ',');
    TF_CHECK(t.units == QStringList({"sec", "mm", "kN", "%"}));
    TF_NEAR(t.columns[2][2], 2.75, 1e-12);
    TF_CHECK(std::isnan(t.columns[3][3]));  // empty, not zero
}

TF_CASE(the_file_hash_is_of_the_bytes_read)
{
    const QString path = fixture("import/decimal-comma.csv");
    const auto r = readTable(path);
    TF_REQUIRE(r.table.has_value());
    QFile f(path);
    TF_REQUIRE(f.open(QIODevice::ReadOnly));
    TF_CHECK(r.table->sha256
             == QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex());
}

TF_CASE(unit_in_the_column_name_is_found)
{
    const auto r = readTable(fixture("synthetic/ro-continuous.csv"));
    TF_REQUIRE(r.table.has_value());
    const auto imp = toTension(*r.table, TensionMapping{{}, {}, {}, {},
                                                        78.54e-6, 50e-3});
    TF_REQUIRE(imp.error.isEmpty());
    TF_CHECK(imp.forceColumn == "force_kN");
    TF_NEAR(imp.input.force[2], 2 * 0.5 * 78.54, 1e-6);  // 1 MPa * 78.54 mm2 = 78.54 N
}

TF_CASE(tension_mapping_takes_geometry_from_the_preamble_in_si)
{
    const auto r = readTable(fixture("import/quoted-preamble.txt"));
    TF_REQUIRE(r.table.has_value());
    const auto imp = toTension(*r.table, TensionMapping{});
    TF_REQUIRE(imp.error.isEmpty());
    TF_CHECK(imp.forceColumn == "force");
    TF_CHECK(imp.extensionColumn == "extensometer");  // not the crosshead
    TF_NEAR(imp.input.area, 78.5e-6, 1e-15);
    TF_NEAR(imp.input.gaugeLength, 0.05, 1e-15);
    TF_NEAR(imp.input.force[1], 1500.0, 1e-9);
    TF_NEAR(imp.input.extension[3], 15e-6, 1e-15);
}

TF_CASE(percent_strain_column_is_taken_as_strain_with_a_note)
{
    const auto r = readTable(fixture("import/decimal-comma.csv"));
    TF_REQUIRE(r.table.has_value());
    TensionMapping m;
    m.area = 120.6e-6;
    const auto imp = toTension(*r.table, m);
    TF_REQUIRE(imp.error.isEmpty());
    TF_CHECK(imp.extensionColumn == "Extensometer elongation");
    // 0.07 % strain: with the gauge length set to 1, extension is the strain.
    TF_NEAR(imp.input.extension[2] / imp.input.gaugeLength, 0.0007, 1e-15);
    TF_CHECK(std::isnan(imp.input.extension[3]));
    TF_CHECK(!imp.notes.filter("strain").isEmpty());
}

TF_CASE(missing_area_is_an_error_that_names_it)
{
    const auto r = readTable(fixture("import/decimal-comma.csv"));
    TF_REQUIRE(r.table.has_value());
    const auto imp = toTension(*r.table, TensionMapping{});
    TF_CHECK(imp.error.contains("area"));
}

TF_CASE(unknown_column_is_an_error_that_lists_the_columns)
{
    const auto r = readTable(fixture("import/decimal-comma.csv"));
    TF_REQUIRE(r.table.has_value());
    TensionMapping m;
    m.force = "Kraft";
    m.area = 1e-4;
    const auto imp = toTension(*r.table, m);
    TF_CHECK(imp.error.contains("Kraft"));
    TF_CHECK(imp.error.contains("Load"));
}

TF_CASE(quantities_parse_with_their_units)
{
    TF_NEAR(parseQuantity("120.6 mm2", "area").value_or(NAN), 120.6e-6, 1e-15);
    TF_NEAR(parseQuantity("50mm", "length").value_or(NAN), 0.05, 1e-15);
    TF_NEAR(parseQuantity("1,5 kN", "force").value_or(NAN), 1500, 1e-9);
    TF_CHECK(!parseQuantity("50", "length"));       // a unit is required
    TF_CHECK(!parseQuantity("50 kN", "length"));    // and must fit the dimension
}

int main() { return tf::test::run(); }
