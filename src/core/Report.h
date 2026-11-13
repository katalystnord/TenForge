#pragma once

// The test report as one HTML document: the saved .html, and the source the
// window prints to PDF. Built only from the results document and the
// specimen and fixture, so a report says nothing the results file and the
// test record do not.

#include "core/Specimen.h"

#include <QJsonObject>
#include <QString>

namespace tf::core {

// chartSrc: the chart image's src (a data: URI, or a resource name the
// printer resolves); empty for no chart. The fixture is shown for the
// procedure's fixture kind (results.json, profile.fixture).
QString reportHtml(const QJsonObject &results, const Specimen &specimen, const QString &chartSrc,
                   const Fixture &fixture = {});

} // namespace tf::core
