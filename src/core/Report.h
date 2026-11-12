#pragma once

// The test report as one HTML document: the saved .html, and the source the
// window prints to PDF. Built only from the results document and the
// specimen, so a report says nothing the results file does not.

#include "core/Specimen.h"

#include <QJsonObject>
#include <QString>

namespace tf::core {

// chartSrc: the chart image's src (a data: URI, or a resource name the
// printer resolves); empty for no chart.
QString reportHtml(const QJsonObject &results, const Specimen &specimen, const QString &chartSrc);

} // namespace tf::core
