// Walkthrough: the real main window, as a user first meets it.
//
// A walkthrough test may only do what something on screen tells it to do
// (CLAUDE.md). In Phase 0 the screen offers nothing to do yet, so these
// cases check what it SAYS, and that the chart area really renders.

#include "gui/MainWindow.h"

#include <QApplication>
#include <QLabel>
#include <QSurfaceFormat>
#include <QTest>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkRenderWindow.h>

class MainWindowWalkthrough : public QObject
{
    Q_OBJECT

private slots:
    void the_window_names_the_application_and_what_it_is()
    {
        tf::gui::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY2(window.windowTitle().contains(QStringLiteral("TenForge")),
                 qPrintable(window.windowTitle()));
        QVERIFY2(window.windowTitle().contains(
                     QStringLiteral("open universal testing software")),
                 qPrintable(window.windowTitle()));
    }

    void the_first_screen_says_what_is_available_and_when()
    {
        tf::gui::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *label = window.findChild<QLabel *>(QStringLiteral("emptyState"));
        QVERIFY(label);
        QVERIFY(label->isVisible());
        QVERIFY2(label->text().contains(QStringLiteral("release 0.1")),
                 qPrintable(label->text()));
    }

    void the_chart_area_renders_vtk_inside_qt()
    {
        tf::gui::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVTKOpenGLNativeWidget *chart = window.chartWidget();
        QVERIFY(chart);
        QVERIFY(chart->isVisible());
        vtkRenderWindow *renderWindow = chart->renderWindow();
        QVERIFY(renderWindow);
        renderWindow->Render();
        // A render window that never got a GL context reports a zero size;
        // one that rendered reports the widget's.
        const int *size = renderWindow->GetSize();
        QVERIFY2(size[0] > 0 && size[1] > 0,
                 qPrintable(QStringLiteral("%1 x %2").arg(size[0]).arg(size[1])));
    }
};

int main(int argc, char *argv[])
{
    // As in the application: VTK's widget needs its surface format set
    // before the QApplication exists.
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);
    MainWindowWalkthrough test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_main_window_walkthrough.moc"
