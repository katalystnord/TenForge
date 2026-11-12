// Validation against NPL's TENSTAND Premium Quality ASCII dataset.
//
//   validate_tenstand <agreed.json> <directory with 01.txt .. 63.txt> [report.md]
//
// Every file is analysed with the shipped ISO 6892-1 profile, exactly as
// tf-analyse would, and compared with the agreed values NPL's working group
// published (tests/validation/tenstand-agreed.json, transcribed from NPL
// Report DEPC MPE 015, Table 9).
//
// Two kinds of check:
//  - A.5: Rp0.2, ReH, ReL, Rm and A against the "proof of confidence"
//    limits of EN 10002-1 Annex A.5. These decide pass or fail; they are
//    the Phase 1 exit criterion (ROADMAP).
//  - TenForge's own regression bands for what A.5 does not cover (Rp0.1,
//    Fm, At, Ag, Agt, and that a yield point is reported exactly where the
//    agreed table has one, or where some labs reported one, file 38).
//    These also fail the test, so a change that
//    moves them is seen; they are labelled as ours, not the standard's.
// E has no limit anywhere: whether it falls in the agreed range is reported
// and nothing more.

#include "core/Import.h"
#include "core/Profile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <cmath>
#include <cstdio>

using namespace tf::core;

namespace {

struct Row {
    QString file, key, kind;  // kind: "A.5", "TenForge", "info"
    double ours = NAN, lo = NAN, hi = NAN, dev = NAN, limit = NAN;
    QString verdict;
};

double scaleFor(const QString &key)
{
    if (key == "E")
        return 1e-9;    // Pa -> GPa
    if (key == "Fm")
        return 1.0;     // N
    if (key == "A" || key == "At" || key == "Ag" || key == "Agt")
        return 100.0;   // 1 -> %
    return 1e-6;        // Pa -> MPa
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: validate_tenstand <agreed.json> <data dir> [report.md]\n");
        return 2;
    }
    QFile jf(argv[1]);
    if (!jf.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    const QJsonObject agreedDoc = QJsonDocument::fromJson(jf.readAll()).object();
    const QJsonObject tol = agreedDoc.value("tolerances").toObject();
    const QString dir = QString::fromLocal8Bit(argv[2]);

    const auto prof = loadProfile("iso-6892-1-2019");
    if (!prof.profile) {
        std::fprintf(stderr, "%s\n", qPrintable(prof.error));
        return 2;
    }
    tf::analysis::TensionMethod method = prof.profile->method;
    method.proofOffsets.push_back(0.001);  // Rp0.1: in the agreed table, not in the profile

    std::vector<Row> rows;
    int failures = 0;

    for (const QJsonValue &fv : agreedDoc.value("files").toArray()) {
        const QJsonObject fo = fv.toObject();
        const QString file = fo.value("file").toString();
        const QJsonObject agreed = fo.value("agreed").toObject();

        const auto table = readTable(dir + "/" + file);
        if (!table.table) {
            std::printf("FAIL %s: %s\n", qPrintable(file), qPrintable(table.error));
            ++failures;
            continue;
        }
        const auto imp = toTension(*table.table, TensionMapping{});
        if (!imp.error.isEmpty()) {
            std::printf("FAIL %s: %s\n", qPrintable(file), qPrintable(imp.error));
            ++failures;
            continue;
        }
        const auto res = tf::analysis::analyseTension(imp.input, method);

        // A yield point exactly where the agreed table has one, or where
        // some labs reported one (file 38): then within A.5 of theirs.
        const QJsonObject labs = fo.value("reported_by_some_labs").toObject();
        for (const char *k : {"ReH", "ReL"}) {
            const auto &d = res.results.at(k);
            if (agreed.contains(k) || !d.determined)
                continue;
            Row r{file, k, "labs", d.value * 1e-6};
            if (labs.contains(k)) {
                const QJsonArray range = labs.value(k).toArray();
                r.lo = range.at(0).toDouble();
                r.hi = range.at(1).toDouble();
                r.dev = r.ours < r.lo ? r.ours - r.lo : (r.ours > r.hi ? r.ours - r.hi : 0.0);
                const QJsonObject t = tol.value(k).toObject();
                r.limit = std::max(t.value("relative").toDouble() * (r.dev < 0 ? r.lo : r.hi),
                                   t.value("absolute").toDouble());
                r.verdict = std::fabs(r.dev) <= r.limit
                                ? "pass (agreed row blank; compared with the labs that reported one)"
                                : "FAIL against the labs that reported one";
                if (std::fabs(r.dev) > r.limit)
                    ++failures;
            } else {
                r.kind = "TenForge";
                r.verdict = "FAIL: yield point reported where the agreed table has none";
                ++failures;
            }
            rows.push_back(r);
        }

        for (auto it = agreed.begin(); it != agreed.end(); ++it) {
            const QString key = it.key();
            const QJsonArray range = it.value().toArray();
            Row r{file, key};
            r.lo = range.at(0).toDouble();
            r.hi = range.at(1).toDouble();
            const auto found = res.results.find(key.toStdString());
            if (found == res.results.end() || !found->second.determined) {
                r.kind = "A.5";
                r.verdict = "FAIL: not determined"
                            + (found != res.results.end()
                                   ? ": " + QString::fromStdString(found->second.reason)
                                   : QString());
                ++failures;
                rows.push_back(r);
                continue;
            }
            r.ours = found->second.value * scaleFor(key);
            r.dev = r.ours < r.lo ? r.ours - r.lo : (r.ours > r.hi ? r.ours - r.hi : 0.0);
            const double ref = r.dev < 0 ? r.lo : r.hi;

            if (tol.contains(key)) {
                r.kind = "A.5";
                const QJsonObject t = tol.value(key).toObject();
                const double rel = t.value("relative").toDouble(0) * std::fabs(ref);
                const double abs = t.value("absolute").toDouble(0);
                r.limit = std::max(rel, abs);
            } else if (key == "E") {
                r.kind = "info";
            } else {
                // TenForge's regression bands: half the reported precision
                // beyond the agreed value, plus room for the fit.
                r.kind = "TenForge";
                if (key == "Rp0.1")
                    r.limit = std::max(0.005 * std::fabs(ref), 2.0);
                else if (key == "Fm")
                    r.limit = 0.005 * std::fabs(ref);
                else
                    r.limit = 0.5;  // At, Ag, Agt: percentage points
            }
            if (r.kind == "info") {
                r.verdict = r.dev == 0 ? "in range" : "outside the agreed range (no limit)";
            } else if (std::fabs(r.dev) <= r.limit) {
                r.verdict = "pass";
            } else {
                r.verdict = "FAIL";
                ++failures;
            }
            rows.push_back(r);
        }
    }

    // --- Print, and write the report when asked ------------------------------------
    QString md;
    QTextStream ts(&md);
    ts << "| File | Result | TenForge | Agreed | Outside by | Limit | Check | Verdict |\n";
    ts << "|---|---|---|---|---|---|---|---|\n";
    for (const Row &r : rows) {
        const QString agreed = std::isnan(r.lo) ? "none"
                               : r.lo == r.hi   ? QString::number(r.lo)
                                                : QString("%1 to %2").arg(r.lo).arg(r.hi);
        ts << "| " << r.file << " | " << r.key << " | "
           << (std::isnan(r.ours) ? QString("-") : QString::number(r.ours, 'f', 2)) << " | " << agreed
           << " | " << (std::isnan(r.dev) ? QString("-") : QString::number(r.dev, 'f', 2)) << " | "
           << (std::isnan(r.limit) ? QString("-") : QString::number(r.limit, 'f', 2)) << " | " << r.kind
           << " | " << r.verdict << " |\n";
    }
    ts.flush();
    std::fputs(qPrintable(md), stdout);
    std::printf("\n%zu comparisons, %d failed\n", rows.size(), failures);

    if (argc >= 4) {
        QFile out(argv[3]);
        if (out.open(QIODevice::WriteOnly | QIODevice::Truncate))
            out.write(md.toUtf8());
    }
    return failures == 0 ? 0 : 1;
}
