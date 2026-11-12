// The test report: one HTML document per analysis, the source of both the
// saved .html and the PDF (the window prints this same document).
//
// What a report must carry is what results-v1 says a result is: the value
// as reported, or why there is none; the conformity and every reason; and
// enough provenance (file hash, profile version, build) to find the data
// again. Built from the synthetic fixture with known answers.

#include "core/Report.h"
#include "core/Session.h"
#include "check.h"

#include <QJsonArray>
#include <QJsonObject>

using namespace tf::core;

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

namespace {

Session analysed()
{
    Session s;
    s.openData(QString(TF_FIXTURES) + "/synthetic/ro-continuous.csv");
    s.specimen.id = "RO-1";
    s.specimen.material = "synthetic steel";
    s.specimen.shape = Specimen::Shape::GivenArea;
    s.specimen.area = 78.54e-6;
    s.specimen.gaugeLength = 50e-3;
    s.profile = "iso-6892-1-2019";
    s.analyse();
    return s;
}

} // namespace

TF_CASE(the_report_states_conformity_and_every_reason)
{
    const Session s = analysed();
    TF_REQUIRE(s.analysis().has_value());
    const QString html = reportHtml(s.analysis()->doc, s.specimen, {});
    TF_CHECK(html.contains("Aligned with ISO 6892-1:2019"));
    TF_CHECK(html.contains("not to the standard"));
    const QJsonArray reasons =
        s.analysis()->doc.value("conformity").toObject().value("reasons").toArray();
    TF_REQUIRE(!reasons.isEmpty());
    for (const QJsonValue &r : reasons)
        if (!html.contains(r.toObject().value("code").toString()))
            tf::test::fail(__FILE__, __LINE__,
                           "reason missing: " + r.toObject().value("code").toString().toStdString());
}

TF_CASE(the_report_gives_each_result_as_reported_or_why_not)
{
    const Session s = analysed();
    TF_REQUIRE(s.analysis().has_value());
    const QString html = reportHtml(s.analysis()->doc, s.specimen, {});
    TF_CHECK(html.contains("<td>300</td>"));  // Rp0.2, the fixture's answer
    TF_CHECK(html.contains("<td>500</td>"));  // Rm
    for (const QJsonValue &v : s.analysis()->doc.value("results").toArray()) {
        const QJsonObject r = v.toObject();
        if (r.value("status").toString() != "determined"
            && !html.contains(r.value("reason").toString().toHtmlEscaped()))
            tf::test::fail(__FILE__, __LINE__,
                           "no reason for " + r.value("id").toString().toStdString());
    }
}

TF_CASE(the_report_carries_specimen_source_profile_and_build)
{
    const Session s = analysed();
    TF_REQUIRE(s.analysis().has_value());
    const QString html = reportHtml(s.analysis()->doc, s.specimen, {});
    TF_CHECK(html.contains("RO-1"));
    TF_CHECK(html.contains("synthetic steel"));
    TF_CHECK(html.contains("78.54 mm2"));
    TF_CHECK(html.contains("50 mm"));
    TF_CHECK(html.contains("ro-continuous.csv"));
    TF_CHECK(html.contains(QString::fromLatin1(s.table()->sha256)));
    TF_CHECK(html.contains("iso-6892-1-2019"));
    TF_CHECK(html.contains(s.analysis()->profile.profileVersion));
    TF_CHECK(html.contains(QString::fromLatin1(s.analysis()->profile.sha256)));
    TF_CHECK(html.contains("TenForge"));
}

TF_CASE(text_from_files_and_users_is_escaped)
{
    Session s = analysed();
    TF_REQUIRE(s.analysis().has_value());
    Specimen sp = s.specimen;
    sp.id = "<script>x</script>";
    const QString html = reportHtml(s.analysis()->doc, sp, {});
    TF_CHECK(!html.contains("<script>"));
    TF_CHECK(html.contains("&lt;script&gt;"));
}

TF_CASE(the_chart_appears_only_when_given)
{
    const Session s = analysed();
    TF_REQUIRE(s.analysis().has_value());
    TF_CHECK(!reportHtml(s.analysis()->doc, s.specimen, {}).contains("<img"));
    TF_CHECK(reportHtml(s.analysis()->doc, s.specimen, "chart.png").contains("src=\"chart.png\""));
}

TF_CASE(the_report_has_no_em_or_en_dashes)
{
    const Session s = analysed();
    TF_REQUIRE(s.analysis().has_value());
    const QString html = reportHtml(s.analysis()->doc, s.specimen, {});
    TF_CHECK(!html.contains(QChar(0x2014)));
    TF_CHECK(!html.contains(QChar(0x2013)));
}

int main() { return tf::test::run(); }
