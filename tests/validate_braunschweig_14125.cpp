// Validation against ZwickRoell testXpert III on real four-point bending
// tests: TU Braunschweig's hemp fibre laminates (Zenodo 17611621, CC BY 4.0).
//
//   validate_braunschweig_14125 <agreed.json> <data dir> [report.md]
//
// Every file is analysed with the shipped ISO 14125 method B profile
// through tf::core::Session, the path the window and the CLI take: the
// width, thickness and span are read from each file's own header; the units
// (N, mm), the loading span (15 mm) and that the crosshead moves the loading
// points come from the archive's test protocols, as a person would enter
// them.
//
// Compared with what testXpert printed for the same specimen (transcribed
// in tests/validation/braunschweig-14125-agreed.json): Ef, the stresses at
// both ends of the modulus window (f', f''), the number of samples in the
// regression (N, exactly), R2, sigma_fM and eps_fM, each within half a unit
// in the last digit printed. This is the Phase 3 exit criterion (ROADMAP).
//
// Two files are excluded, each with its reason in the agreed file: their
// own headers state a maximum stress their data never reach, so the curve
// exported is not the one testXpert evaluated. That is checked here too, so
// an exclusion cannot hide a disagreement: an excluded file whose data DO
// reach its header's maximum fails the run.

#include "analysis/Curve.h"
#include "core/Session.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <cmath>
#include <cstdio>

using namespace tf::core;

namespace {

// Half a unit in the last digit of a number as printed: "7.18" -> 0.005,
// "12.2" -> 0.05, "288" -> 0.5. A hair added for binary rounding.
double halfUnit(const QString &printed)
{
    const int dot = printed.indexOf('.');
    const int decimals = dot < 0 ? 0 : int(printed.size()) - dot - 1;
    return 0.5 * std::pow(10.0, -decimals) + 1e-9;
}

struct Row {
    QString specimen, quantity, printed;
    double ours = NAN, tolerance = 0;
    bool pass = false;
};

double resultValue(const Analysis &a, const char *id)
{
    for (const QJsonValue &v : a.doc.value("results").toArray())
        if (v.toObject().value("id").toString() == id)
            return v.toObject().value("value").toDouble(NAN);
    return NAN;
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: validate_braunschweig_14125 <agreed.json> <data dir> [report.md]\n");
        return 2;
    }
    QFile af(argv[1]);
    if (!af.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    const QJsonObject agreed = QJsonDocument::fromJson(af.readAll()).object();
    const QString dataDir = QString::fromLocal8Bit(argv[2]);

    std::vector<Row> rows;
    QStringList excludedNotes, problems;
    int analysed = 0;

    for (const QJsonValue &sv : agreed.value("specimens").toArray()) {
        const QJsonObject sp = sv.toObject();
        const QString name = sp.value("specimen").toString();
        const QJsonObject tx = sp.value("testxpert").toObject();

        Session s;
        s.profile = "iso-14125-1998-four-point";
        const QString err = s.openData(dataDir + "/" + sp.value("file").toString());
        if (!err.isEmpty()) {
            problems << name + ": " + err;
            continue;
        }
        if (!s.specimen.width || !s.specimen.thickness || !s.fixture.span) {
            problems << name + ": width, thickness or span was not read from the file's header";
            continue;
        }
        s.mapping.forceUnit = "N";
        s.mapping.extensionUnit = "mm";
        s.fixture.loadingSpan = sp.value("loading_span_mm").toDouble() * 1e-3;
        s.fixture.deflectionAt = Fixture::DeflectionAt::LoadingPoints;
        if (!s.analyse()) {
            problems << name + ": not analysed: " + s.next().text;
            continue;
        }
        const Analysis &a = *s.analysis();
        const double sigmaFM = resultValue(a, "sigma_fM") * 1e-6;
        const double headerFM = tx.value("sigma_fM_MPa").toString().toDouble();

        if (sp.contains("excluded")) {
            // The reason must still hold: the data do not reach the header's
            // maximum stress.
            if (std::fabs(sigmaFM - headerFM) <= halfUnit(tx.value("sigma_fM_MPa").toString()))
                problems << name + " is excluded because its data do not reach the header's "
                                   "maximum stress, but they do: include it";
            excludedNotes << QString("%1: header %2 MPa, data %3 MPa. %4")
                                 .arg(name, tx.value("sigma_fM_MPa").toString())
                                 .arg(sigmaFM, 0, 'f', 3)
                                 .arg(sp.value("excluded").toString());
            continue;
        }
        ++analysed;

        // The stresses at the modulus window's ends, from the same curve.
        tf::analysis::CurveMethod m;
        m.requests = {{"f1", tf::analysis::CurveQuantity::StressAtStrain, 0.0005},
                      {"f2", tf::analysis::CurveQuantity::StressAtStrain, 0.0025}};
        // (Only the break rule reads the force, and stress is proportional
        // to it, so stress stands in for force here.)
        const auto ends = tf::analysis::analyseCurve({a.stress, a.stress, a.strain}, m);
        const auto &ef = a.result.results.at("Ef");

        auto add = [&](const QString &q, const QString &printed, double ours) {
            Row r{name, q, printed, ours, halfUnit(printed), false};
            r.pass = std::isfinite(ours) && std::fabs(ours - printed.toDouble()) <= r.tolerance;
            rows.push_back(r);
        };
        add("Ef (GPa)", tx.value("Ef_GPa").toString(), resultValue(a, "Ef") * 1e-9);
        add("f' (MPa)", tx.value("f1_MPa").toString(),
            ends.results.at("f1").determined ? ends.results.at("f1").value * 1e-6 : NAN);
        add("f'' (MPa)", tx.value("f2_MPa").toString(),
            ends.results.at("f2").determined ? ends.results.at("f2").value * 1e-6 : NAN);
        add("N", QString::number(tx.value("N").toInt()), ef.fit ? double(ef.fit->n) : NAN);
        add("R2", tx.value("R2").toString(), ef.fit ? ef.fit->r2 : NAN);
        add("sigma_fM (MPa)", tx.value("sigma_fM_MPa").toString(), sigmaFM);
        add("eps_fM (%)", tx.value("epsilon_fM_percent").toString(),
            resultValue(a, "epsilon_fM") * 100.0);
    }

    int failed = 0;
    for (const Row &r : rows)
        failed += r.pass ? 0 : 1;

    QString report;
    QTextStream o(&report);
    o << "# TenForge against testXpert III: TU Braunschweig ISO 14125 four-point bending\n\n"
      << agreed.value("dataset").toString() << "\n\n"
      << "Method: " << agreed.value("method").toString() << "\n\n"
      << "Tolerance: " << agreed.value("tolerance").toString() << "\n\n"
      << "Specimens analysed: " << analysed << "; comparisons: " << rows.size()
      << "; failed: " << failed << "; excluded: " << excludedNotes.size() << ".\n\n"
      << "| Specimen | Quantity | testXpert | TenForge | Tolerance | |\n|---|---|---|---|---|---|\n";
    for (const Row &r : rows)
        o << "| " << r.specimen << " | " << r.quantity << " | " << r.printed << " | "
          << QString::number(r.ours, 'f', r.quantity == "N" ? 0 : 5) << " | "
          << QString::number(r.tolerance, 'g', 2) << " | " << (r.pass ? "pass" : "FAIL") << " |\n";
    o << "\n## Excluded\n\n";
    for (const QString &e : excludedNotes)
        o << "- " << e << "\n";
    if (!problems.isEmpty()) {
        o << "\n## Problems\n\n";
        for (const QString &p : problems)
            o << "- " << p << "\n";
    }
    o.flush();

    std::fputs(report.toUtf8().constData(), stdout);
    if (argc > 3) {
        QFile rf(QString::fromLocal8Bit(argv[3]));
        if (rf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            rf.write(report.toUtf8());
    }
    if (analysed == 0)
        return 1;
    return failed == 0 && problems.isEmpty() ? 0 : 1;
}
