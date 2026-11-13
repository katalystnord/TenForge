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

// --- Real exports from compression and bending machines (Phase 3) ------------
// A ZwickRoell testXpert III export (TU Braunschweig, four-point bending,
// CC BY 4.0): Latin-1, a preamble of "key   value unit" lines with no
// delimiter, no units row, decimal commas in e-notation, and two candidate
// displacement columns, a named deformation and the crosshead.
static const char *kZwick =
    "braunschweig-14125/4PB_DIN14125_NFK_Osten_19122024_Hemp80_Cdry_reference_class3_1.CSV";

TF_CASE(zwick_export_reads_its_whitespace_preamble_and_columns)
{
    const auto r = readTable(fixture(kZwick));
    TF_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TF_CHECK(t.delimiter == ';');
    TF_CHECK(t.decimal == ',');
    TF_CHECK(t.names == QStringList({"Verformung", "Standardkraft", "Zeit", "Traversenweg",
                                     QString::fromUtf8("Prüfzeit")}));
    TF_REQUIRE(t.columns.size() == 5);
    TF_CHECK(t.columns[1].size() == 1549);
    TF_NEAR(t.columns[1][0], 0.1256414204836, 1e-15);
    TF_NEAR(*preambleQuantity(t, {"Probenbreite"}, "length"), 14.85e-3, 1e-15);
    TF_NEAR(*preambleQuantity(t, {"Probendicke"}, "length"), 2.52e-3, 1e-15);
    TF_NEAR(*preambleQuantity(t, {QString::fromUtf8("Stützweite")}, "length"), 45e-3, 1e-15);
}

TF_CASE(a_named_deformation_is_preferred_over_the_crosshead)
{
    const auto r = readTable(fixture(kZwick));
    TF_REQUIRE(r.table.has_value());
    TF_CHECK(guessForceColumn(*r.table) == "Standardkraft");
    TF_CHECK(guessExtensionColumn(*r.table) == "Verformung");
}

TF_CASE(french_compression_export_reads_quoted_names_and_units)
{
    // Universidade de Sao Paulo, PU foam to ASTM D1621 (CC BY 4.0).
    const auto r = readTable(fixture("puf-d1621/1BAT-DIR1-1.TRA"));
    TF_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TF_CHECK(t.units == QStringList({"s", "mm", "N"}));
    TF_CHECK(guessForceColumn(t) == "Force standard");
    TF_CHECK(guessExtensionColumn(t) == QString::fromUtf8("Déformation nominale"));
    TF_CHECK(t.columns[0].size() == 3991);
}

TF_CASE(deflection_and_crosshead_columns_are_recognised)
{
    for (const char *name : {"deflection", "Durchbiegung", "compression", "Stauchung"}) {
        Table t;
        t.names = {"time", name, "force"};
        if (guessExtensionColumn(t) != name)
            tf::test::fail(__FILE__, __LINE__, std::string("not found: ") + name);
    }
    // A crosshead counts only when nothing better is there.
    Table t;
    t.names = {"time", "crosshead", "force"};
    TF_CHECK(guessExtensionColumn(t) == "crosshead");
    t.names = {"time", "crosshead", "deflection", "force"};
    TF_CHECK(guessExtensionColumn(t) == "deflection");
    t.names = {"time", "deflection", "Durchbiegung", "force"};
    QString why;
    TF_CHECK(guessExtensionColumn(t, &why).isEmpty());
    TF_CHECK(why.contains("2"));
}

// --- Instron's exports (tests/fixtures/instron/SOURCE.md) -----------------------
// Real files from Bluehill 2/3, Bluehill Universal and Merlin, one per
// dialect, each with a property checked by hand against its own data.

static QString instron(const char *name) { return fixture((std::string("instron/") + name).c_str()); }

static bool hasPreambleKey(const Table &t, const QString &key)
{
    for (const auto &p : t.preamble)
        if (p.key == key)
            return true;
    return false;
}

TF_CASE(bluehill_flexure_export_reads_imperial_units_and_its_header_result)
{
    // Bluehill's header: "Maximum Load : Maximum Load","131.18347",lbf and
    // the extension there, 1.50294 in. Its curve's maximum is that row.
    const auto r = readTable(instron("bluehill2-flexure.csv"));
    TF_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TF_CHECK(t.names == QStringList({"Time", "Extension", "Load"}));
    TF_CHECK(t.units == QStringList({"s", "in", "lbf"}));
    TF_CHECK(guessForceColumn(t) == "Load");
    TF_CHECK(guessExtensionColumn(t) == "Extension");
    // A key is found behind Bluehill's "Section : " prefix.
    TF_NEAR(preambleQuantity(t, {"Maximum Load"}, "force").value_or(NAN), 131.18347 * 4.4482216152605,
            1e-9);

    TensionMapping m;
    m.needsGeometry = false;
    const auto imp = toTension(t, m);
    TF_REQUIRE(imp.error.isEmpty());
    std::size_t k = 0;
    for (std::size_t i = 0; i < imp.input.force.size(); ++i)
        if (imp.input.force[i] > imp.input.force[k])
            k = i;
    TF_NEAR(imp.input.force[k], 131.18347 * 4.4482216152605, 1e-9);
    TF_NEAR(imp.input.extension[k], 1.50294 * 0.0254, 1e-12);
}

TF_CASE(bluehill_universal_results_table_is_kept_as_the_files_own_results)
{
    const auto r = readTable(instron("bluehill-universal-results.csv"));
    TF_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    auto find = [&](const QString &name) -> const FileResult * {
        for (const FileResult &f : t.results)
            if (f.name == name)
                return &f;
        return nullptr;
    };
    const FileResult *fm = find("Maximum Force");
    TF_REQUIRE(fm != nullptr);
    TF_CHECK(fm->table == "Results Table 1");
    TF_CHECK(fm->row == "1");
    TF_CHECK(fm->unit == "N");
    TF_CHECK(fm->value == "285.30");
    const FileResult *e = find("Modulus (Automatic Young's)");
    TF_REQUIRE(e != nullptr);
    TF_CHECK(e->unit == "MPa" && e->value == "143.72");
    const FileResult *nu = find("Poisson's ratio (Chord)");
    TF_REQUIRE(nu != nullptr);
    TF_CHECK(nu->value == "-----");  // kept as written: not a number, not a zero
    TF_NEAR(fileResultQuantity(t, {"Maximum Force"}, "force").value_or(NAN), 285.30, 1e-9);
    TF_CHECK(!fileResultQuantity(t, {"Poisson's ratio (Chord)"}, "strain"));

    // The results row is not header data, and the curve below is read whole.
    TF_CHECK(!hasPreambleKey(t, "1"));
    TF_CHECK(t.names == QStringList({"Time", "Displacement", "Force", "Tensile displacement",
                                     "Tensile strain (Displacement)", "Tensile stress"}));
    TF_CHECK(t.units == QStringList({"s", "mm", "kN", "mm", "%", "MPa"}));
    // Bluehill's leading column is empty in every row: left out, and said.
    TF_CHECK(!t.notes.filter("empty").isEmpty());
    TF_CHECK(t.columns[0].size() == 340);
    // Bluehill's maximum force is on the curve: 0.2853 kN at 6.74 s.
    std::size_t k = 0;
    for (std::size_t i = 0; i < t.columns[2].size(); ++i)
        if (t.columns[2][i] > t.columns[2][k])
            k = i;
    TF_NEAR(t.columns[2][k], 0.2853, 1e-12);
    TF_NEAR(t.columns[0][k], 6.74, 1e-12);
}

TF_CASE(bluehill_universal_header_with_the_unit_before_the_value)
{
    // "Specimen properties : Width,(in),"0.39"": unit, then value.
    const auto r = readTable(instron("bluehill-universal-composite.csv"));
    TF_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TF_NEAR(preambleQuantity(t, {"Width"}, "length").value_or(NAN), 0.39 * 0.0254, 1e-12);
    TF_NEAR(preambleQuantity(t, {"Thickness"}, "length").value_or(NAN), 0.04 * 0.0254, 1e-12);
    TF_NEAR(fileResultQuantity(t, {"Area"}, "area").value_or(NAN), 30.42e-6, 1e-15);
    TF_CHECK(t.names == QStringList({"Time", "Displacement", "Force", "Bonded Strain Gage",
                                     "Tensile stress"}));
    TF_CHECK(t.columns[0].size() == 3682);
}

TF_CASE(italian_bluehill_export_is_read_by_its_column_names)
{
    const auto r = readTable(instron("bluehill2-italian.csv"));
    TF_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TF_CHECK(t.delimiter == ';');
    TF_CHECK(t.decimal == '.');
    TF_CHECK(t.names == QStringList({"Tempo", "Corsa", "Carico"}));
    TF_CHECK(guessForceColumn(t) == "Carico");
    TF_CHECK(guessExtensionColumn(t) == "Corsa");
}

TF_CASE(merlin_export_states_units_inside_its_column_names)
{
    const auto r = readTable(instron("merlin-compression.raw"));
    TF_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TF_CHECK(columnUnit(t, "Time sec") == "sec");
    TF_CHECK(columnUnit(t, "Extension mm") == "mm");
    TF_CHECK(columnUnit(t, "Load N") == "N");
    TF_CHECK(columnUnit(t, "Strain [Exten.] %") == "%");
    TF_CHECK(columnUnit(t, "Cycle Count").isEmpty());
    TF_CHECK(guessForceColumn(t) == "Load N");
    // "Area:",9.677400,"mm*mm"
    TF_NEAR(preambleQuantity(t, {"Area"}, "area").value_or(NAN), 9.6774e-6, 1e-15);
    TensionMapping m;
    m.force = "Load N";
    m.extension = "Extension mm";
    m.needsGeometry = false;
    const auto imp = toTension(t, m);
    TF_REQUIRE(imp.error.isEmpty());
    TF_CHECK(imp.forceUnit == "N" && imp.extensionUnit == "mm");
    TF_NEAR(imp.input.force[0], -2.130933, 1e-12);
}

TF_CASE(bluehill_dimension_header_gives_the_area)
{
    const auto r = readTable(instron("bluehill2-dimensions.csv"));
    TF_REQUIRE(r.table.has_value());
    TF_NEAR(preambleQuantity(*r.table, {"Area"}, "area").value_or(NAN), 181.1716e-6, 1e-15);
}

TF_CASE(force_and_displacement_names_in_other_languages_are_recognised)
{
    const std::vector<std::pair<const char *, const char *>> pairs{
        {"Carico", "Allungamento"}, {"Forza", "Deformazione"}, {"Fuerza", "Alargamiento"},
        {"Carga", "Deformación"},   {"Força", "Deformação"}};
    for (const auto &[f, e] : pairs) {
        Table t;
        t.names = {"time", QString::fromUtf8(f), QString::fromUtf8(e)};
        if (guessForceColumn(t) != QString::fromUtf8(f))
            tf::test::fail(__FILE__, __LINE__, std::string("force not found: ") + f);
        if (guessExtensionColumn(t) != QString::fromUtf8(e))
            tf::test::fail(__FILE__, __LINE__, std::string("extension not found: ") + e);
    }
    // A crosshead in Italian, Spanish and Portuguese counts only when nothing
    // better is there.
    for (const char *c : {"Corsa", "Desplazamiento", "Deslocamento"}) {
        Table t;
        t.names = {"time", c, "Carico"};
        if (guessExtensionColumn(t) != c)
            tf::test::fail(__FILE__, __LINE__, std::string("crosshead not found: ") + c);
    }
}

int main() { return tf::test::run(); }
