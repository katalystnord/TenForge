#pragma once

// The stress-strain chart with what TenForge draws over it: a label at
// each stress result, the user's notes, and the modulus window, whose two
// edges can be dragged with the mouse. Everything is painted through
// VTK's 2D context, so the SVG export draws exactly what the screen shows.
//
// Data coordinates throughout: strain in %, stress in MPa, as the axes.

#include <vtkChartXY.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tf::gui {

class AnnotatedChart : public vtkChartXY
{
public:
    static AnnotatedChart *New();
    vtkTypeMacro(AnnotatedChart, vtkChartXY);

    struct Text {
        double x = 0, y = 0;  // the point it is about
        std::string text;
    };
    struct Span {
        double from = 0, to = 0;
        std::string text;
    };

    void setLabels(std::vector<Text> labels) { m_labels = std::move(labels); }
    void setNotes(std::vector<Text> points, std::vector<Span> spans)
    {
        m_points = std::move(points);
        m_spans = std::move(spans);
    }
    // The modulus window's edges; dashed lines with grips. Empty: none.
    void setModulusWindow(std::optional<std::pair<double, double>> w) { m_window = w; }
    const std::optional<std::pair<double, double>> &modulusWindow() const { return m_window; }

    // Called when the user lets go of a dragged edge, with the new window.
    std::function<void(double from, double to)> onWindowMoved;

    // Scene position of data coordinates, and back (for the tests that
    // drag an edge as a user would).
    float sceneX(double x) const;
    float sceneY(double y) const;
    double dataX(float sx) const;

    bool Paint(vtkContext2D *painter) override;
    bool Hit(const vtkContextMouseEvent &mouse) override;
    bool MouseButtonPressEvent(const vtkContextMouseEvent &mouse) override;
    bool MouseMoveEvent(const vtkContextMouseEvent &mouse) override;
    bool MouseButtonReleaseEvent(const vtkContextMouseEvent &mouse) override;

protected:
    AnnotatedChart() = default;
    ~AnnotatedChart() override = default;

private:
    AnnotatedChart(const AnnotatedChart &) = delete;
    void operator=(const AnnotatedChart &) = delete;

    std::vector<Text> m_labels, m_points;
    std::vector<Span> m_spans;
    std::optional<std::pair<double, double>> m_window;
    int m_dragging = 0;  // 0 none, 1 the start edge, 2 the end edge
};

} // namespace tf::gui
