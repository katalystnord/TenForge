#include "core/Specimen.h"

#include <QJsonValue>
#include <QStringList>

#include <cmath>
#include <numbers>

namespace tf::core {

namespace {

struct Dim {
    const char *key;   // in test.json
    const char *label; // in a reason a person reads
    std::optional<double> Specimen::*field;
};

const std::vector<Dim> &dimsOf(Specimen::Shape shape)
{
    static const std::vector<Dim> rect{{"width", "width", &Specimen::width},
                                       {"thickness", "thickness", &Specimen::thickness}};
    static const std::vector<Dim> round{{"diameter", "diameter", &Specimen::diameter}};
    static const std::vector<Dim> tube{
        {"outer_diameter", "outer diameter", &Specimen::outerDiameter},
        {"inner_diameter", "inner diameter", &Specimen::innerDiameter}};
    static const std::vector<Dim> given{{"area", "cross-section area", &Specimen::area}};
    switch (shape) {
    case Specimen::Shape::Rectangular: return rect;
    case Specimen::Shape::Round: return round;
    case Specimen::Shape::Tube: return tube;
    case Specimen::Shape::GivenArea: return given;
    }
    return rect;
}

QJsonValue quantity(const std::optional<double> &v, const char *unit)
{
    if (!v || !std::isfinite(*v))
        return QJsonValue();  // unmeasured: null, never 0
    return QJsonObject{{"value", *v}, {"unit", unit}};
}

std::optional<double> fromQuantity(const QJsonValue &v)
{
    if (!v.isObject())
        return std::nullopt;
    const QJsonValue x = v.toObject().value("value");
    if (!x.isDouble())
        return std::nullopt;
    return x.toDouble();
}

} // namespace

QString shapeName(Specimen::Shape shape)
{
    switch (shape) {
    case Specimen::Shape::Rectangular: return QStringLiteral("rectangular");
    case Specimen::Shape::Round: return QStringLiteral("round");
    case Specimen::Shape::Tube: return QStringLiteral("tube");
    case Specimen::Shape::GivenArea: return QStringLiteral("given-area");
    }
    return {};
}

AreaOrReason crossSection(const Specimen &s)
{
    QStringList missing, bad;
    for (const Dim &d : dimsOf(s.shape)) {
        const auto &v = s.*(d.field);
        if (!v || !std::isfinite(*v))
            missing << d.label;
        else if (!(*v > 0))
            bad << d.label;
    }
    if (!missing.isEmpty())
        return {std::nullopt, "enter the specimen's " + missing.join(" and ")};
    if (!bad.isEmpty())
        return {std::nullopt, "the specimen's " + bad.join(" and ") + " must be greater than zero"};

    constexpr double pi = std::numbers::pi;
    switch (s.shape) {
    case Specimen::Shape::Rectangular:
        return {*s.width * *s.thickness, {}};
    case Specimen::Shape::Round:
        return {pi * *s.diameter * *s.diameter / 4.0, {}};
    case Specimen::Shape::Tube:
        if (!(*s.innerDiameter < *s.outerDiameter))
            return {std::nullopt, "the tube's inner diameter must be smaller than its outer diameter"};
        return {pi * (*s.outerDiameter * *s.outerDiameter - *s.innerDiameter * *s.innerDiameter) / 4.0,
                {}};
    case Specimen::Shape::GivenArea:
        return {*s.area, {}};
    }
    return {std::nullopt, "unknown shape"};
}

QJsonObject specimenJson(const Specimen &s)
{
    QJsonObject g{{"shape", shapeName(s.shape)}};
    for (const Dim &d : dimsOf(s.shape))
        g[d.key] = quantity(s.*(d.field), s.shape == Specimen::Shape::GivenArea ? "m2" : "m");
    return QJsonObject{{"id", s.id},
                       {"material", s.material},
                       {"geometry", g},
                       {"gauge_length", quantity(s.gaugeLength, "m")},
                       {"notes", s.notes}};
}

std::optional<Specimen> specimenFromJson(const QJsonObject &o, QString *error)
{
    Specimen s;
    s.id = o.value("id").toString();
    s.material = o.value("material").toString();
    s.notes = o.value("notes").toString();
    const QJsonObject g = o.value("geometry").toObject();
    const QString shape = g.value("shape").toString();
    bool known = false;
    for (auto sh : {Specimen::Shape::Rectangular, Specimen::Shape::Round, Specimen::Shape::Tube,
                    Specimen::Shape::GivenArea})
        if (shapeName(sh) == shape) {
            s.shape = sh;
            known = true;
        }
    if (!known) {
        if (error)
            *error = "the specimen's shape \"" + shape + "\" is not one TenForge knows";
        return std::nullopt;
    }
    for (const Dim &d : dimsOf(s.shape))
        s.*(d.field) = fromQuantity(g.value(d.key));
    s.gaugeLength = fromQuantity(o.value("gauge_length"));
    return s;
}

} // namespace tf::core
