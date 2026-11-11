#pragma once

#include <QMainWindow>

class QLabel;
class QVTKOpenGLNativeWidget;

namespace tf::gui {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // For the walkthrough tests: the chart area, so a test can confirm VTK is
    // really rendering inside Qt rather than merely constructed.
    QVTKOpenGLNativeWidget *chartWidget() const { return m_chart; }

private:
    QLabel *m_emptyState = nullptr;
    QVTKOpenGLNativeWidget *m_chart = nullptr;
};

} // namespace tf::gui
