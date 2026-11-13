#pragma once

#include "core/Session.h"
#include "gui/AnnotatedChart.h"

#include <QMainWindow>

#include <vtkSmartPointer.h>

class QAction;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;
class QVTKOpenGLNativeWidget;
class vtkChartXY;
class vtkContextView;

namespace tf::gui {

// The analysis window: choose a procedure, open a machine export, say which
// columns are force and displacement, enter the specimen and fixture, read
// the results, export a report, save a record. Tension, compression and
// bending: the procedure's mode decides which fields are shown.
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
    // The chart item, so a test can find where an edge is drawn and drag it.
    AnnotatedChart *chart() const { return m_chartXY; }

    // File dialogs are the platform's own by default. A walkthrough test
    // turns that off so it can drive Qt's dialog like a user would.
    static void setNativeDialogs(bool on);

    const tf::core::Session &session() const { return m_session; }

public slots:
    void openData();
    void openRecord();
    void saveRecord();
    void exportReport();
    void exportChart();
    void exportFreecad();
    void exportVtp();
    void addReference();
    void exportPmd();

private:
    QWidget *buildInputs();
    QWidget *buildOutputs();
    void showSession();      // session -> widgets, after a file or record is opened
    void showShapeRows();    // the rows the chosen shape and the procedure's mode need
    void readInputs();       // widgets -> session, after any edit
    void refresh();          // analyse and show the outcome
    void showChart();
    void showResults();
    void addUnlisted();
    void chooseCompliance();
    QString chooseFile(bool save, const QString &caption, const QString &filter,
                       const QString &suggested);
    void showNotes();        // session's notes -> the notes table
    void readNotes();        // the notes table -> session, then redraw
    void addNote(bool span);
    void showWindowFields();
    void readWindowFields();
    QByteArray chartSvg(QString *error = nullptr) const;

    tf::core::Session m_session;
    bool m_updating = false;

    QLabel *m_next = nullptr;
    QLabel *m_notices = nullptr;
    QPushButton *m_addUnlisted = nullptr;

    QLabel *m_file = nullptr;
    QComboBox *m_fileSpecimen = nullptr;  // shown only for a file of several specimens
    QComboBox *m_forceColumn = nullptr, *m_forceUnit = nullptr;
    QComboBox *m_extColumn = nullptr, *m_extUnit = nullptr;
    QLabel *m_readNotes = nullptr;
    QFormLayout *m_dataForm = nullptr;
    QPushButton *m_complianceButton = nullptr, *m_clearCompliance = nullptr;
    QLabel *m_complianceFile = nullptr;

    QLineEdit *m_id = nullptr, *m_material = nullptr;
    QComboBox *m_shape = nullptr;
    QFormLayout *m_specForm = nullptr;
    QLineEdit *m_width = nullptr, *m_thickness = nullptr, *m_diameter = nullptr;
    QLineEdit *m_outer = nullptr, *m_inner = nullptr, *m_area = nullptr;
    QLineEdit *m_gauge = nullptr;
    QLineEdit *m_span = nullptr, *m_loadingSpan = nullptr;
    QComboBox *m_deflectionAt = nullptr;
    QLabel *m_s0 = nullptr;

    QComboBox *m_profile = nullptr;
    QString m_profileFile;  // a user profile chosen from a file

    QLabel *m_conformity = nullptr;
    QTableWidget *m_results = nullptr;
    QTabWidget *m_tabs = nullptr;
    QTableWidget *m_notesTable = nullptr;
    QLineEdit *m_windowFrom = nullptr, *m_windowTo = nullptr;
    QPushButton *m_windowReset = nullptr;
    QWidget *m_windowRow = nullptr;
    QLabel *m_flags = nullptr;
    QLabel *m_emptyState = nullptr;

    QVTKOpenGLNativeWidget *m_chart = nullptr;
    vtkSmartPointer<vtkContextView> m_view;
    vtkSmartPointer<AnnotatedChart> m_chartXY;

    QAction *m_saveRecord = nullptr, *m_exportReport = nullptr, *m_exportChart = nullptr;
    QAction *m_exportFreecad = nullptr, *m_exportVtp = nullptr;
    QAction *m_addReference = nullptr, *m_clearReferences = nullptr, *m_exportPmd = nullptr;
};

} // namespace tf::gui
