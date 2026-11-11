#include "core/ApplicationInfo.h"
#include "gui/MainWindow.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>

int main(int argc, char *argv[])
{
    // Must precede QApplication: QVTKOpenGLNativeWidget needs its own default
    // surface format to get a usable OpenGL context.
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication app(argc, argv);
    QApplication::setApplicationName(tf::core::applicationName());
    QApplication::setOrganizationName(QStringLiteral("Katalyst Nord"));
    QApplication::setApplicationVersion(tf::core::applicationVersion());

    tf::gui::MainWindow window;
    window.show();
    return app.exec();
}
