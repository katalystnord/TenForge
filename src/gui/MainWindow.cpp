#include "gui/MainWindow.h"

#include "core/ApplicationInfo.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkAxis.h>
#include <vtkChartXY.h>
#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>

namespace tf::gui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("%1 - %2")
                       .arg(tf::core::applicationName(),
                            tf::core::applicationSubtitle()));

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    // The first screen says what TenForge can do today and nothing more. A
    // button that does nothing would be a UX defect (tenet 7), so there is
    // none until the CSV import it would open exists.
    m_emptyState = new QLabel(central);
    m_emptyState->setObjectName(QStringLiteral("emptyState"));
    m_emptyState->setWordWrap(true);
    m_emptyState->setText(QStringLiteral(
        "TenForge is in its first phase of development. Analysing a CSV "
        "file from any testing machine arrives in release 0.1."));
    layout->addWidget(m_emptyState);

    // The chart area: VTK's own charting inside Qt, which is where the
    // stress-strain curve will be drawn. Present from the start so the
    // Qt-and-VTK integration is exercised by the tests from the start.
    m_chart = new QVTKOpenGLNativeWidget(central);
    m_chart->setObjectName(QStringLiteral("chart"));
    vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
    m_chart->setRenderWindow(renderWindow);

    vtkNew<vtkContextView> view;
    view->SetRenderWindow(renderWindow);
    vtkNew<vtkChartXY> chart;
    chart->GetAxis(vtkAxis::BOTTOM)->SetTitle("Strain");
    chart->GetAxis(vtkAxis::LEFT)->SetTitle("Stress");
    view->GetScene()->AddItem(chart);
    layout->addWidget(m_chart, 1);

    setCentralWidget(central);
    resize(1000, 700);
}

} // namespace tf::gui
