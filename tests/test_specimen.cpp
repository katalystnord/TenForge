// The specimen: its cross-section from measured dimensions, and how it is
// written into test.json (test-record-v1.md, 5.2).
//
// Expected areas are worked by hand: 20.1 mm x 3.02 mm = 60.702 mm2;
// a round bar of 10 mm is pi x 25 = 78.5398163 mm2; a tube 20/16 mm is
// pi x (400 - 256) / 4 = 113.0973355 mm2.

#include "core/Specimen.h"
#include "check.h"

#include <QJsonDocument>

#include <cmath>

using namespace tf::core;

TF_CASE(rectangular_section_is_width_times_thickness)
{
    Specimen s;
    s.shape = Specimen::Shape::Rectangular;
    s.width = 20.1e-3;
    s.thickness = 3.02e-3;
    const auto a = crossSection(s);
    TF_REQUIRE(a.area.has_value());
    TF_NEAR(*a.area, 60.702e-6, 1e-15);
}

TF_CASE(round_section_from_its_diameter)
{
    Specimen s;
    s.shape = Specimen::Shape::Round;
    s.diameter = 10e-3;
    const auto a = crossSection(s);
    TF_REQUIRE(a.area.has_value());
    TF_NEAR(*a.area, 78.5398163e-6, 1e-13);
}

TF_CASE(tube_section_is_the_ring_between_its_diameters)
{
    Specimen s;
    s.shape = Specimen::Shape::Tube;
    s.outerDiameter = 20e-3;
    s.innerDiameter = 16e-3;
    const auto a = crossSection(s);
    TF_REQUIRE(a.area.has_value());
    TF_NEAR(*a.area, 113.0973355e-6, 1e-13);
}

TF_CASE(a_given_area_is_used_as_given)
{
    Specimen s;
    s.shape = Specimen::Shape::GivenArea;
    s.area = 120.6e-6;
    const auto a = crossSection(s);
    TF_REQUIRE(a.area.has_value());
    TF_NEAR(*a.area, 120.6e-6, 0);
}

TF_CASE(a_missing_dimension_is_named_in_the_reason)
{
    Specimen s;
    s.shape = Specimen::Shape::Rectangular;
    s.width = 20e-3;
    const auto a = crossSection(s);
    TF_CHECK(!a.area);
    TF_CHECK(a.reason.contains("thickness"));
    TF_CHECK(!a.reason.contains("width"));
}

TF_CASE(a_tube_whose_bore_is_not_inside_it_is_refused)
{
    Specimen s;
    s.shape = Specimen::Shape::Tube;
    s.outerDiameter = 16e-3;
    s.innerDiameter = 20e-3;
    const auto a = crossSection(s);
    TF_CHECK(!a.area);
    TF_CHECK(a.reason.contains("inner"));
}

TF_CASE(zero_or_negative_dimensions_are_refused)
{
    Specimen s;
    s.shape = Specimen::Shape::Round;
    s.diameter = 0;
    TF_CHECK(!crossSection(s).area);
    s.diameter = -5e-3;
    TF_CHECK(!crossSection(s).area);
}

TF_CASE(specimen_json_round_trips_and_writes_unmeasured_as_null)
{
    Specimen s;
    s.id = "S355-07";
    s.material = "S355 sheet";
    s.shape = Specimen::Shape::Rectangular;
    s.width = 20.1e-3;
    s.thickness = 3.02e-3;
    s.notes = "edge milled";
    // gauge length not measured
    const QJsonObject j = specimenJson(s);
    const QJsonObject g = j.value("geometry").toObject();
    TF_CHECK(g.value("shape").toString() == "rectangular");
    TF_CHECK(g.value("width").toObject().value("unit").toString() == "m");
    TF_NEAR(g.value("thickness").toObject().value("value").toDouble(), 3.02e-3, 0);
    TF_CHECK(j.value("gauge_length").isNull());  // null, never 0

    QString error;
    const auto back = specimenFromJson(j, &error);
    TF_REQUIRE(back.has_value());
    TF_CHECK(back->id == s.id);
    TF_CHECK(back->material == s.material);
    TF_CHECK(back->notes == s.notes);
    TF_CHECK(back->shape == s.shape);
    TF_CHECK(back->width == s.width);
    TF_CHECK(back->thickness == s.thickness);
    TF_CHECK(!back->gaugeLength);
}

TF_CASE(specimen_json_with_an_unknown_shape_is_refused_with_its_name)
{
    QJsonObject j{{"geometry", QJsonObject{{"shape", "hexagonal"}}}};
    QString error;
    TF_CHECK(!specimenFromJson(j, &error));
    TF_CHECK(error.contains("hexagonal"));
}

int main() { return tf::test::run(); }
