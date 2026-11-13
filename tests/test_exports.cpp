// Exports for other tools: the hardening curve for FreeCAD and CalculiX,
// the FreeCAD material card, and the curve as VTK XML for ParaView.
//
// The hardening curve's expected values come from
// tools/fixtures/make_exports.py (closed form, not TenForge's code). The
// .vtp is read back with VTK's own XML reader, the one ParaView and
// PyVista use.

#include "core/Exports.h"
#include "core/Session.h"
#include "check.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <vtkDataArray.h>
#include <vtkFieldData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkXMLPolyDataReader.h>

#include <cmath>
#include <cstdio>

using namespace tf::core;

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

namespace {

QString fixture(const char *rel) { return QString(TF_FIXTURES) + "/" + rel; }

Session tension(const char *file = "synthetic/ro-continuous.csv", double area = 78.54e-6)
{
    Session s;
    s.profile = "iso-6892-1-2019";
    s.openData(fixture(file));
    s.specimen.shape = Specimen::Shape::GivenArea;
    s.specimen.area = area;
    s.specimen.gaugeLength = 50e-3;
    s.specimen.material = "RO test steel";
    s.analyse();
    return s;
}

QJsonArray expectedPoints()
{
    QFile f(fixture("exports/ro-hardening.json"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object().value("points").toArray();
}

} // namespace

TF_CASE(the_hardening_curve_matches_the_closed_form)
{
    const Session s = tension();
    TF_REQUIRE(s.analysis().has_value());
    const Hardening h = hardeningCurve(*s.analysis(), 20);
    TF_REQUIRE(h.error.isEmpty());
    if (!h.error.isEmpty())
        std::printf("  %s\n", qPrintable(h.error));
    const QJsonArray want = expectedPoints();
    TF_REQUIRE(want.size() == 20);
    TF_REQUIRE(h.points.size() == 20);
    TF_CHECK(h.yieldResult == "Rp0.2");
    for (int i = 0; i < 20; ++i) {
        const QJsonObject w = want[i].toObject();
        // 0.2 MPa: the modulus is fitted, not given, and Rp0.2 is found
        // within 0.05 MPa (tension_on_synthetic_curves).
        TF_NEAR(h.points[i].trueStress * 1e-6, w.value("true_stress_MPa").toDouble(), 0.2);
        TF_NEAR(h.points[i].plasticStrain, w.value("plastic_strain").toDouble(), 2e-5);
    }
    TF_CHECK(h.points.front().plasticStrain == 0.0);
}

TF_CASE(a_yield_drop_is_kept_from_lowering_the_hardening_curve_and_said)
{
    // The discontinuous fixture: ReH, a plateau with a planted lowest point,
    // then hardening. Initial yield is ReL; stress never falls along the
    // curve, and the notes say points were raised.
    Session s;
    s.profile = "iso-6892-1-2019";
    s.openData(fixture("synthetic/discontinuous.csv"));
    s.specimen.shape = Specimen::Shape::GivenArea;
    s.specimen.area = 120.64e-6;
    s.specimen.gaugeLength = 50e-3;
    TF_REQUIRE(s.analyse());
    // 400 points: about two per wave of the fixture's plateau, so its dips
    // are sampled (at 40 the plateau gets one or two points and no dip).
    const Hardening h = hardeningCurve(*s.analysis(), 400);
    TF_REQUIRE(h.error.isEmpty());
    TF_CHECK(h.yieldResult == "ReL");
    for (std::size_t i = 1; i < h.points.size(); ++i) {
        TF_CHECK(h.points[i].trueStress >= h.points[i - 1].trueStress);
        TF_CHECK(h.points[i].plasticStrain > h.points[i - 1].plasticStrain
                 || (i == 1 && h.points[i].plasticStrain >= 0));
    }
    TF_CHECK(!h.notes.filter("raised").isEmpty());
}

TF_CASE(bending_has_no_hardening_curve_and_says_why)
{
    Session s;
    s.profile = "iso-178-2019";
    s.openData(fixture("modes/beam-3pt.csv"));
    s.specimen.width = 10.2e-3;
    s.specimen.thickness = 4.1e-3;
    s.fixture.span = 64e-3;
    TF_REQUIRE(s.analyse());
    const Hardening h = hardeningCurve(*s.analysis());
    TF_CHECK(h.points.empty());
    TF_CHECK(h.error.contains("tension"));
    QString err;
    TF_CHECK(freecadCard(*s.analysis(), s.specimen, &err).isEmpty());
    TF_CHECK(err.contains("bending"));
}

TF_CASE(yield_points_and_the_plastic_block_are_in_mpa)
{
    const Session s = tension();
    const Hardening h = hardeningCurve(*s.analysis(), 20);
    const QStringList y = yieldPoints(h);
    TF_REQUIRE(y.size() == 20);
    TF_CHECK(y.front().startsWith("301.0") || y.front().startsWith("301.1"));
    TF_CHECK(y.front().endsWith(", 0"));
    const QString inp = calculixPlastic(h, *s.analysis());
    TF_CHECK(inp.contains("*PLASTIC\n"));
    TF_CHECK(!inp.contains("HARDENING=KINEMATIC"));
    TF_CHECK(inp.contains("** "));               // comments say where it came from
    TF_CHECK(inp.contains("ro-continuous.csv"));
    TF_CHECK(calculixPlastic(h, *s.analysis(), true).contains("*PLASTIC, HARDENING=KINEMATIC"));
    // Every data line of the block is "stress, strain".
    int data = 0;
    for (const QString &l : inp.split('\n'))
        if (!l.startsWith("*") && !l.trimmed().isEmpty())
            ++data;
    TF_CHECK(data == 20);
}

TF_CASE(the_freecad_card_holds_what_was_measured_and_nothing_else)
{
    const Session s = tension();
    QString err;
    const QString card = freecadCard(*s.analysis(), s.specimen, &err);
    TF_REQUIRE(err.isEmpty());
    TF_CHECK(card.startsWith("---\n"));
    TF_CHECK(card.contains("Name: \"RO test steel\""));
    TF_CHECK(card.contains("UUID: '7b561d1d-fb9b-44f6-9da9-56a4f74d7536'"));  // LinearElastic
    TF_CHECK(card.contains("YoungsModulus: \"199.9 GPa\""));  // as reported
    TF_CHECK(card.contains("YieldStrength: \"300 MPa\""));
    TF_CHECK(card.contains("UltimateTensileStrength: \"500 MPa\""));
    // Not measured by a tensile test: never written.
    TF_CHECK(!card.contains("PoissonRatio:"));
    TF_CHECK(!card.contains("Density:"));
    TF_CHECK(card.contains("# Aligned with ISO 6892-1:2019, not to the standard"));
}

TF_CASE(the_curve_reads_back_in_vtk_with_its_arrays_and_results)
{
    const Session s = tension();
    QTemporaryDir dir;
    const QString path = dir.filePath("curve.vtp");
    QFile f(path);
    TF_REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(curveVtp(*s.analysis()).toUtf8());
    f.close();

    vtkNew<vtkXMLPolyDataReader> r;
    r->SetFileName(path.toLocal8Bit().constData());
    r->Update();
    vtkPolyData *pd = r->GetOutput();
    TF_REQUIRE(pd != nullptr);
    const vtkIdType n = vtkIdType(s.analysis()->stress.size());
    TF_CHECK(pd->GetNumberOfPoints() == n);
    TF_CHECK(pd->GetNumberOfLines() == 1);
    vtkDataArray *stress = pd->GetPointData()->GetArray("stress_Pa");
    vtkDataArray *force = pd->GetPointData()->GetArray("force_N");
    TF_REQUIRE(stress && force);
    const vtkIdType k = n / 2;
    // The SI data exactly as computed (written with 17 digits).
    TF_CHECK(stress->GetTuple1(k) == s.analysis()->stress[std::size_t(k)]);
    // Ready for a line chart, in the units a reader expects.
    vtkDataArray *pct = pd->GetPointData()->GetArray("strain_percent");
    vtkDataArray *mpa = pd->GetPointData()->GetArray("stress_MPa");
    TF_REQUIRE(pct && mpa);
    // Plotting arrays to 8 significant digits.
    TF_NEAR(mpa->GetTuple1(k), s.analysis()->stress[std::size_t(k)] * 1e-6,
            1e-7 * std::fabs(s.analysis()->stress[std::size_t(k)] * 1e-6));
    TF_NEAR(pct->GetTuple1(k), s.analysis()->strain[std::size_t(k)] * 100.0,
            1e-7 * std::fabs(s.analysis()->strain[std::size_t(k)] * 100.0));
    vtkDataArray *rm = pd->GetFieldData()->GetArray("Rm_Pa");
    TF_REQUIRE(rm != nullptr);
    TF_NEAR(rm->GetTuple1(0), 500e6, 1e3);
}

TF_CASE(the_pmd_json_ld_carries_each_result_with_its_class_and_unit)
{
    const Session s = tension();
    QString err;
    const QString text = pmdJsonLd(*s.analysis(), s.specimen, &err);
    TF_REQUIRE(err.isEmpty());
    const QJsonObject doc = QJsonDocument::fromJson(text.toUtf8()).object();
    TF_CHECK(doc.value("@context").toObject().value("tto").toString() == "https://w3id.org/pmd/tto/");
    // Index the graph by type.
    QJsonObject rm, rmDatum, process;
    for (const QJsonValue &v : doc.value("@graph").toArray()) {
        const QJsonObject o = v.toObject();
        if (o.value("@type").toString() == "pmd:PMD_0000974")
            process = o;
        if (o.value("@type").toString() == "tto:TTO_0000053")
            rm = o;
    }
    TF_REQUIRE(!process.isEmpty() && !rm.isEmpty());
    for (const QJsonValue &v : doc.value("@graph").toArray()) {
        const QJsonObject o = v.toObject();
        if (o.value("obo:IAO_0000221").toObject().value("@id") == rm.value("@id"))
            rmDatum = o;
    }
    TF_REQUIRE(!rmDatum.isEmpty());
    const QJsonObject spec = rmDatum.value("obo:OBI_0001938").toObject();
    TF_NEAR(spec.value("obo:OBI_0001937").toObject().value("@value").toDouble(), 500.0, 1e-3);
    TF_CHECK(spec.value("obo:IAO_0000039").toObject().value("@id").toString() == "unit:MegaPA");
    TF_CHECK(process.value("rdfs:comment").toString().contains("not to the standard"));
    // The QUDT IRIs PMD's own example gets wrong (they do not resolve).
    TF_CHECK(!text.contains("unit:MegaPa\"") && !text.contains("unit:GigaPa\""));
    // Rp0.1 is not in the profile; At has no class: named as left out.
    TF_CHECK(process.value("rdfs:comment").toString().contains("At"));
}

TF_CASE(compression_and_bending_are_not_exported_to_pmd_and_say_why)
{
    Session s;
    s.profile = "iso-178-2019";
    s.openData(fixture("modes/beam-3pt.csv"));
    s.specimen.width = 10.2e-3;
    s.specimen.thickness = 4.1e-3;
    s.fixture.span = 64e-3;
    TF_REQUIRE(s.analyse());
    QString err;
    TF_CHECK(pmdJsonLd(*s.analysis(), s.specimen, &err).isEmpty());
    TF_CHECK(err.contains("tensile"));
}

TF_CASE(a_saved_record_carries_its_curve_for_paraview)
{
    Session s = tension();
    QTemporaryDir dir;
    TF_REQUIRE(s.saveRecord(dir.filePath("r")).isEmpty());
    const QString vtp = dir.filePath("r/results/curve.vtp");
    TF_REQUIRE(QFile::exists(vtp));
    QFile m(dir.filePath("r/manifest.tfrec"));
    TF_REQUIRE(m.open(QIODevice::ReadOnly));
    const QByteArray manifest = m.readAll();
    TF_CHECK(manifest.contains("results/curve.vtp"));
    vtkNew<vtkXMLPolyDataReader> r;
    r->SetFileName(vtp.toLocal8Bit().constData());
    r->Update();
    TF_CHECK(r->GetOutput()->GetNumberOfPoints() == vtkIdType(s.analysis()->stress.size()));
    // Reopened, the record reports nothing changed or added.
    Session o;
    TF_REQUIRE(o.openRecord(dir.filePath("r")).isEmpty());
    TF_CHECK(o.notices().isEmpty());
}

int main() { return tf::test::run(); }
