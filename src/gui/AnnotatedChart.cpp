#include "gui/AnnotatedChart.h"

#include <vtkAxis.h>
#include <vtkBrush.h>
#include <vtkContext2D.h>
#include <vtkContextMouseEvent.h>
#include <vtkObjectFactory.h>
#include <vtkPen.h>
#include <vtkTextProperty.h>

#include <algorithm>
#include <cmath>

namespace tf::gui {

vtkStandardNewMacro(AnnotatedChart);

namespace {

// How near (scene pixels) a press must be to an edge to grab it.
constexpr float kGrab = 6.0f;

} // namespace

float AnnotatedChart::sceneX(double x) const
{
    auto *a = const_cast<AnnotatedChart *>(this)->GetAxis(vtkAxis::BOTTOM);
    const double lo = a->GetMinimum(), hi = a->GetMaximum();
    const float p1 = a->GetPoint1()[0], p2 = a->GetPoint2()[0];
    return hi > lo ? float(p1 + (x - lo) / (hi - lo) * (p2 - p1)) : p1;
}

float AnnotatedChart::sceneY(double y) const
{
    auto *a = const_cast<AnnotatedChart *>(this)->GetAxis(vtkAxis::LEFT);
    const double lo = a->GetMinimum(), hi = a->GetMaximum();
    const float p1 = a->GetPoint1()[1], p2 = a->GetPoint2()[1];
    return hi > lo ? float(p1 + (y - lo) / (hi - lo) * (p2 - p1)) : p1;
}

double AnnotatedChart::dataX(float sx) const
{
    auto *a = const_cast<AnnotatedChart *>(this)->GetAxis(vtkAxis::BOTTOM);
    const double lo = a->GetMinimum(), hi = a->GetMaximum();
    const float p1 = a->GetPoint1()[0], p2 = a->GetPoint2()[0];
    return p2 != p1 ? lo + (sx - p1) / double(p2 - p1) * (hi - lo) : lo;
}

bool AnnotatedChart::Paint(vtkContext2D *painter)
{
    const bool ok = vtkChartXY::Paint(painter);
    if (GetNumberOfPlots() == 0)
        return ok;

    auto *bottom = GetAxis(vtkAxis::BOTTOM);
    auto *left = GetAxis(vtkAxis::LEFT);
    const float xMin = bottom->GetPoint1()[0], xMax = bottom->GetPoint2()[0];
    const float yMin = left->GetPoint1()[1], yMax = left->GetPoint2()[1];
    auto inside = [&](float x) { return x >= xMin - 0.5f && x <= xMax + 0.5f; };

    vtkTextProperty *tp = painter->GetTextProp();
    tp->SetFontSize(12);
    tp->SetBold(false);
    tp->SetItalic(false);
    tp->SetVerticalJustificationToBottom();

    // The modulus window: dashed edges, a grip at the top of each.
    if (m_window) {
        painter->GetPen()->SetColor(110, 110, 110, 255);
        painter->GetPen()->SetWidth(1.5f);
        painter->GetPen()->SetLineType(vtkPen::DASH_LINE);
        painter->GetBrush()->SetColor(110, 110, 110, 255);
        for (double x : {m_window->first, m_window->second}) {
            const float sx = sceneX(x);
            if (!inside(sx))
                continue;
            painter->DrawLine(sx, yMin, sx, yMax);
            painter->GetPen()->SetLineType(vtkPen::SOLID_LINE);
            painter->DrawRect(sx - 3.0f, yMax - 10.0f, 6.0f, 10.0f);
            painter->GetPen()->SetLineType(vtkPen::DASH_LINE);
        }
        painter->GetPen()->SetLineType(vtkPen::SOLID_LINE);
    }

    // Labels at the results: beside the point, kept inside the plot.
    tp->SetColor(0.10, 0.10, 0.10);
    for (std::size_t i = 0; i < m_labels.size(); ++i) {
        const Text &l = m_labels[i];
        const float sx = sceneX(l.x), sy = sceneY(l.y);
        if (!inside(sx))
            continue;
        float b[4];
        painter->ComputeStringBounds(l.text.c_str(), b);
        const bool right = sx + 8.0f + b[2] > xMax;
        tp->SetJustification(right ? VTK_TEXT_RIGHT : VTK_TEXT_LEFT);
        // Alternate below and above the point so neighbours do not overlap.
        const float dy = (i % 2 == 0) ? -16.0f : 8.0f;
        painter->DrawString(right ? sx - 8.0f : sx + 8.0f, std::clamp(sy + dy, yMin, yMax - 14.0f),
                            l.text.c_str());
    }

    // The user's notes: a marker on the curve with its text, and spans as a
    // bracket along the top with the text above.
    tp->SetColor(0.55, 0.25, 0.0);
    painter->GetPen()->SetColor(140, 64, 0, 255);
    painter->GetPen()->SetWidth(1.5f);
    painter->GetBrush()->SetColor(140, 64, 0, 255);
    for (const Text &n : m_points) {
        const float sx = sceneX(n.x), sy = sceneY(n.y);
        if (!inside(sx))
            continue;
        painter->DrawLine(sx, sy, sx, std::min(sy + 30.0f, yMax - 4.0f));
        painter->DrawEllipse(sx, sy, 3.5f, 3.5f);
        tp->SetJustification(sx + 120.0f > xMax ? VTK_TEXT_RIGHT : VTK_TEXT_LEFT);
        painter->DrawString(sx, std::min(sy + 32.0f, yMax - 2.0f), n.text.c_str());
    }
    float row = yMax - 6.0f;
    for (const Span &s : m_spans) {
        const float a = std::max(sceneX(s.from), xMin), b = std::min(sceneX(s.to), xMax);
        if (!(b > a))
            continue;
        painter->DrawLine(a, row, b, row);
        painter->DrawLine(a, row, a, row - 6.0f);
        painter->DrawLine(b, row, b, row - 6.0f);
        tp->SetJustification(VTK_TEXT_CENTERED);
        painter->DrawString((a + b) / 2.0f, row + 2.0f, s.text.c_str());
        row -= 22.0f;
    }
    return ok;
}

bool AnnotatedChart::Hit(const vtkContextMouseEvent &mouse)
{
    return m_dragging != 0 || vtkChartXY::Hit(mouse);
}

bool AnnotatedChart::MouseButtonPressEvent(const vtkContextMouseEvent &mouse)
{
    if (m_window && mouse.GetButton() == vtkContextMouseEvent::LEFT_BUTTON) {
        const float x = mouse.GetScenePos().GetX();
        const float d1 = std::fabs(x - sceneX(m_window->first));
        const float d2 = std::fabs(x - sceneX(m_window->second));
        if (std::min(d1, d2) <= kGrab) {
            m_dragging = d1 <= d2 ? 1 : 2;
            return true;
        }
    }
    return vtkChartXY::MouseButtonPressEvent(mouse);
}

bool AnnotatedChart::MouseMoveEvent(const vtkContextMouseEvent &mouse)
{
    if (m_dragging == 0)
        return vtkChartXY::MouseMoveEvent(mouse);
    const double x = dataX(mouse.GetScenePos().GetX());
    if (m_dragging == 1)
        m_window->first = std::min(x, m_window->second - 1e-6);
    else
        m_window->second = std::max(x, m_window->first + 1e-6);
    GetScene()->SetDirty(true);
    return true;
}

bool AnnotatedChart::MouseButtonReleaseEvent(const vtkContextMouseEvent &mouse)
{
    if (m_dragging == 0)
        return vtkChartXY::MouseButtonReleaseEvent(mouse);
    m_dragging = 0;
    if (onWindowMoved)
        onWindowMoved(m_window->first, m_window->second);
    return true;
}

} // namespace tf::gui
