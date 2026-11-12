#pragma once

// The specimen as measured: its shape and dimensions, from which the
// cross-section S0 follows, and how it is written into test.json
// (docs/specs/test-record-v1.md, 5.2). Dimensions are in metres; a
// dimension not measured is empty, never zero.

#include <QJsonObject>
#include <QString>

#include <optional>

namespace tf::core {

struct Specimen {
    enum class Shape { Rectangular, Round, Tube, GivenArea };

    QString id, material, notes;
    Shape shape = Shape::Rectangular;
    std::optional<double> width, thickness;                 // rectangular
    std::optional<double> diameter;                         // round
    std::optional<double> outerDiameter, innerDiameter;     // tube
    std::optional<double> area;                             // given area, m^2
    std::optional<double> gaugeLength;                      // extensometer Le
};

struct AreaOrReason {
    std::optional<double> area;  // m^2
    QString reason;              // says what to enter when there is no area
};

AreaOrReason crossSection(const Specimen &s);

// The shape's name in test.json: "rectangular", "round", "tube", "given-area".
QString shapeName(Specimen::Shape shape);

QJsonObject specimenJson(const Specimen &s);
std::optional<Specimen> specimenFromJson(const QJsonObject &o, QString *error);

} // namespace tf::core
