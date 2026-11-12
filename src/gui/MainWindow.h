#pragma once

#include "core/Session.h"

#include <QMainWindow>

#include <vtkSmartPointer.h>

class QAction;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QVTKOpenGLNativeWidget;
class vtkChartXY;
class vtkContextView;

namespace tf::gui {

// The analysis window: open a machine export, say which columns are force
// and extension, enter the specimen, choose a procedure, read the results,
// export a report, save a record.
//
// All state and every decision live in tf::core::Session; the window shows
// it, writes the user's edits into it and asks it what to do next. The
// banner at the top always says the one next step.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // For the walkthrough tests: the chart area, so a test can confirm VTK is
    // really rendering inside Qt rather than merely constructed.
    QVTKOpenGLNativeWidget *chartWidget() const { return m_chart; }

    // File dialogs are the platform's own by default. A walkthrough test
    // turns that off so it can drive Qt's dialog like a user would.
    static void setNativeDialogs(bool on);

    const tf::core::Session &session() const { return m_session; }

public slots:
    void openData();
    void openRecord();
    void saveRecord();
    void exportReport();

private:
    QWidget *buildInputs();
    QWidget *buildOutputs();
    void showSession();      // session -> widgets, after a file or record is opened
    void showShapeRows();    // the dimension rows the chosen shape needs
    void readInputs();       // widgets -> session, after any edit
    void refresh();          // analyse and show the outcome
    void showChart();
    void showResults();
    void addUnlisted();
    QString chooseFile(bool save, const QString &caption, const QString &filter,
                       const QString &suggested);
    QString chartPng() const;  // the chart as a PNG data URI; empty if none

    tf::core::Session m_session;
    bool m_updating = false;

    QLabel *m_next = nullptr;
    QLabel *m_notices = nullptr;
    QPushButton *m_addUnlisted = nullptr;

    QLabel *m_file = nullptr;
    QComboBox *m_forceColumn = nullptr, *m_forceUnit = nullptr;
    QComboBox *m_extColumn = nullptr, *m_extUnit = nullptr;
    QLabel *m_readNotes = nullptr;

    QLineEdit *m_id = nullptr, *m_material = nullptr;
    QComboBox *m_shape = nullptr;
    QFormLayout *m_specForm = nullptr;
    QLineEdit *m_width = nullptr, *m_thickness = nullptr, *m_diameter = nullptr;
    QLineEdit *m_outer = nullptr, *m_inner = nullptr, *m_area = nullptr;
    QLineEdit *m_gauge = nullptr;
    QLabel *m_s0 = nullptr;

    QComboBox *m_profile = nullptr;
    QString m_profileFile;  // a user profile chosen from a file

    QLabel *m_conformity = nullptr;
    QTableWidget *m_results = nullptr;
    QLabel *m_flags = nullptr;
    QLabel *m_emptyState = nullptr;

    QVTKOpenGLNativeWidget *m_chart = nullptr;
    vtkSmartPointer<vtkContextView> m_view;
    vtkSmartPointer<vtkChartXY> m_chartXY;

    QAction *m_saveRecord = nullptr, *m_exportReport = nullptr;
};

} // namespace tf::gui
