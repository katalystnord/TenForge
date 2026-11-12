// Validation against BAM's S355 reference tensile tests (CC BY 4.0).
//
// Ten specimens tested to DIN EN ISO 6892-1 by BAM, with the raw machine
// exports and BAM's own evaluated results (tests/fixtures/bam-s355/,
// SOURCE.md there). Each export is analysed exactly as tf-analyse would,
// with the shipped ISO 6892-1 profile and BAM's measured S0, and compared
// with what BAM reported:
//  - ReH and Rm within EN 10002-1 Annex A.5 (relative or absolute limit,
//    TenForge's reading of the table, as for TENSTAND);
//  - Fm within 0.5 % (TenForge's band; A.5 has none for force);
//  - E printed beside BAM's, without a limit (no standard sets one here).

#include "core/Import.h"
#include "core/Profile.h"
#include "check.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

using namespace tf::core;

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

namespace {

struct Specimen {
    QString id;
    QJsonObject reported;
    tf::analysis::TensionResult result;
    QString error;
};

std::vector<Specimen> &specimens()
{
    static std::vector<Specimen> all = [] {
        std::vector<Specimen> out;
        const QString dir = QString(TF_FIXTURES) + "/bam-s355/";
        QFile jf(dir + "reported.json");
        if (!jf.open(QIODevice::ReadOnly))
            return out;
        const QJsonObject specs = QJsonDocument::fromJson(jf.readAll()).object().value("specimens").toObject();
        const auto prof = loadProfile("iso-6892-1-2019");
        for (auto it = specs.begin(); it != specs.end(); ++it) {
            Specimen s{it.key(), it.value().toObject(), {}, {}};
            const auto t = readTable(dir + it.key() + ".csv");
            TensionMapping m;
            m.area = s.reported.value("S0_mm2").toDouble() * 1e-6;
            const auto imp = t.table ? toTension(*t.table, m) : TensionImport{};
            if (!t.table)
                s.error = t.error;
            else if (!imp.error.isEmpty())
                s.error = imp.error;
            else if (!prof.profile)
                s.error = prof.error;
            else
                s.result = tf::analysis::analyseTension(imp.input, prof.profile->method);
            out.push_back(s);
        }
        return out;
    }();
    return all;
}

const tf::analysis::Determination *find(const Specimen &s, const char *id)
{
    const auto it = s.result.results.find(id);
    return it == s.result.results.end() ? nullptr : &it->second;
}

void within(const Specimen &s, const char *id, double ours, double theirs, double rel, double abs,
            const char *unit)
{
    const double limit = std::max(rel * std::fabs(theirs), abs);
    const double dev = ours - theirs;
    std::printf("  %s %-3s TenForge %10.3f  BAM %10.3f %-3s  diff %+7.3f  limit %.3f\n",
                qPrintable(s.id), id, ours, theirs, unit, dev, limit);
    if (!(std::fabs(dev) <= limit))
        tf::test::fail(__FILE__, __LINE__, (s.id + " " + id + " outside its limit").toStdString());
}

} // namespace

TF_CASE(all_ten_bam_specimens_are_read_and_analysed)
{
    TF_CHECK(specimens().size() == 10);
    for (const auto &s : specimens())
        if (!s.error.isEmpty())
            tf::test::fail(__FILE__, __LINE__, (s.id + ": " + s.error).toStdString());
}

TF_CASE(upper_yield_strength_matches_bam_within_annex_a5)
{
    for (const auto &s : specimens()) {
        const auto *d = find(s, "ReH");
        if (!d || !d->determined) {
            tf::test::fail(__FILE__, __LINE__,
                           (s.id + " ReH not determined: " + (d ? QString::fromStdString(d->reason) : ""))
                               .toStdString());
            continue;
        }
        const double bam = s.reported.value("FeH_kN").toDouble() * 1e3 / s.reported.value("S0_mm2").toDouble();
        within(s, "ReH", d->value * 1e-6, bam, 0.01, 4.0, "MPa");
    }
}

TF_CASE(tensile_strength_and_maximum_force_match_bam)
{
    for (const auto &s : specimens()) {
        const auto *rm = find(s, "Rm");
        const auto *fm = find(s, "Fm");
        if (!rm || !rm->determined || !fm || !fm->determined) {
            tf::test::fail(__FILE__, __LINE__, (s.id + " Rm or Fm not determined").toStdString());
            continue;
        }
        const double fmBam = s.reported.value("Fm_kN").toDouble();
        within(s, "Rm", rm->value * 1e-6, fmBam * 1e3 / s.reported.value("S0_mm2").toDouble(), 0.005,
               2.0, "MPa");
        within(s, "Fm", fm->value * 1e-3, fmBam, 0.005, 0.0, "kN");
    }
}

TF_CASE(elongations_past_the_extensometer_are_refused_with_the_reason)
{
    for (const auto &s : specimens())
        for (const char *id : {"Agt", "Ag", "At", "A"}) {
            const auto *d = find(s, id);
            if (!d || d->determined || d->reason.find("extension") == std::string::npos)
                tf::test::fail(__FILE__, __LINE__,
                               (s.id + " " + id + " should be refused: the extensometer was removed").toStdString());
        }
}

TF_CASE(modulus_beside_bam_for_the_record_without_a_limit)
{
    for (const auto &s : specimens()) {
        const auto *e = find(s, "E");
        std::printf("  %s E   TenForge %7.1f  BAM %5.0f GPa (no limit)\n", qPrintable(s.id),
                    e && e->determined ? e->value * 1e-9 : NAN, s.reported.value("E_GPa").toDouble());
    }
}

int main() { return tf::test::run(); }
