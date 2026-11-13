// Walkthrough: the real main window, driven as a user would drive it.
//
// A walkthrough test may only do what something on screen tells it to do
// (CLAUDE.md). Before every action these cases read the banner at the top
// of the window, check it asks for exactly that, and then act on a control
// found by its visible label or text, never by an internal name. File
// dialogs are Qt's own here (MainWindow::setNativeDialogs(false)), filled
// in the way a user types a path.
//
// The Phase 2 exit criterion: open a CSV and produce a report using only
// on-screen guidance (ROADMAP). Phase 3 adds the same for a bending test,
// on a real testXpert export whose header holds the specimen's size.

#include "gui/MainWindow.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QSurfaceFormat>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkRenderWindow.h>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <vector>

#ifndef TF_FIXTURES
#error "TF_FIXTURES must point at tests/fixtures"
#endif

namespace {

QString fixture(const char *rel) { return QString(TF_FIXTURES) + "/" + rel; }

QString banner(QWidget *w)
{
    // The banner is the first label in the window; found by being the one
    // that starts with "Next:" or "Analysed", as the user sees it.
    for (QLabel *l : w->findChildren<QLabel *>())
        if (l->isVisible() && (l->text().startsWith("Next:") || l->text().startsWith("Analysed")))
            return l->text();
    return {};
}

// The control a visible label names (QFormLayout sets the label's buddy).
template <class T>
T *labelled(QWidget *w, const QString &label)
{
    for (QLabel *l : w->findChildren<QLabel *>())
        if (l->isVisible() && l->text() == label && l->buddy())
            return qobject_cast<T *>(l->buddy());
    return nullptr;
}

QAction *menuItem(QMainWindow *w, const QString &menu, const QString &item)
{
    for (QAction *m : w->menuBar()->actions())
        if (m->text().remove('&') == menu && m->menu())
            for (QAction *a : m->menu()->actions())
                if (a->text().remove('&') == item)
                    return a;
    return nullptr;
}

QAbstractButton *button(QWidget *w, const QString &text)
{
    for (QAbstractButton *b : w->findChildren<QAbstractButton *>())
        if (b->isVisible() && b->text() == text)
            return b;
    return nullptr;
}

// Types a path into the file dialog that is about to open, and accepts it.
void answerFileDialog(const QString &path)
{
    auto attempt = std::make_shared<std::function<void(int)>>();
    *attempt = [path, attempt](int left) {
        auto *d = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
        if (!d) {
            if (left > 0)
                QTimer::singleShot(50, [attempt, left] { (*attempt)(left - 1); });
            return;
        }
        auto *name = d->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
        if (name) {
            name->setText(path);
            QTest::keyClick(name, Qt::Key_Return);
        } else {
            d->selectFile(path);
            QMetaObject::invokeMethod(d, "accept");
        }
    };
    QTimer::singleShot(0, [attempt] { (*attempt)(60); });
}

// With TF_WALKTHROUGH_SHOTS=<dir>, the window as the user sees it at each
// step is saved there (for a person to look at, and for the manual). It is
// never compared: a screenshot proves nothing a test can name.
void shot(QWidget *w, const char *name)
{
    const QString dir = qEnvironmentVariable("TF_WALKTHROUGH_SHOTS");
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    QTest::qWait(100);
    w->grab().save(dir + "/" + name + ".png");
}

void type(QLineEdit *e, const QString &text)
{
    e->clear();
    QTest::keyClicks(e, text);
}

QString cell(QTableWidget *t, const QString &result, int column)
{
    for (int r = 0; r < t->rowCount(); ++r)
        if (t->item(r, 0) && t->item(r, 0)->text() == result)
            return t->item(r, column) ? t->item(r, column)->text() : QString();
    return "no row " + result;
}

QTableWidget *resultsTable(QWidget *w)
{
    for (QTableWidget *t : w->findChildren<QTableWidget *>())
        if (t->isVisible() && t->horizontalHeaderItem(0)
            && t->horizontalHeaderItem(0)->text() == "Result")
            return t;
    return nullptr;
}

// The input panel's content fits its visible width: nothing is cut off at
// the right edge (it cannot be scrolled to sideways). A real export's long
// names and the bending rows once made it wider than the panel.
QString panelOverflow(QWidget *w)
{
    for (QScrollArea *a : w->findChildren<QScrollArea *>()) {
        if (!a->isVisible() || !a->widget())
            continue;
        const int need = a->widget()->minimumSizeHint().width();
        const int have = a->viewport()->width();
        if (need > have)
            return QString("the input panel needs %1 px and shows %2 px").arg(need).arg(have);
    }
    return {};
}

QByteArray bytesOf(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// The banner asks for the procedure first; picks the one whose name holds
// every one of `words`.
void chooseProcedure(QWidget *w, const QStringList &words)
{
    QVERIFY2(banner(w).contains("Choose the procedure"), qPrintable(banner(w)));
    auto *proc = labelled<QComboBox>(w, "Procedure");
    QVERIFY(proc);
    int found = -1;
    for (int i = 0; i < proc->count(); ++i) {
        bool all = true;
        for (const QString &word : words)
            all = all && proc->itemText(i).contains(word);
        if (all)
            found = i;
    }
    QVERIFY2(found >= 0, qPrintable("no procedure named " + words.join(", ") + " offered"));
    proc->setCurrentIndex(found);
}

// Follows the banner through ISO 6892-1 on the synthetic Ramberg-Osgood
// curve: a rectangular 15.708 mm x 5 mm specimen (S0 78.54 mm2, the
// fixture's), Le 50 mm.
void followToAnalysis(tf::gui::MainWindow &w)
{
    shot(&w, "1-start");
    chooseProcedure(&w, {"ISO 6892-1"});
    if (QTest::currentTestFailed())
        return;
    QVERIFY2(banner(&w).contains("File > Open data"), qPrintable(banner(&w)));
    QAbstractButton *open = button(&w, "Open data...");
    QVERIFY(open);
    answerFileDialog(fixture("synthetic/ro-continuous.csv"));
    QTest::mouseClick(open, Qt::LeftButton);

    // Columns and units come from the file; the banner moves on to the specimen.
    QVERIFY2(banner(&w).contains("width and thickness"), qPrintable(banner(&w)));
    shot(&w, "2-opened");
    auto *width = labelled<QLineEdit>(&w, "Width (mm)");
    auto *thickness = labelled<QLineEdit>(&w, "Thickness (mm)");
    QVERIFY(width && thickness);
    type(width, "15.708");
    QVERIFY2(banner(&w).contains("thickness"), qPrintable(banner(&w)));
    type(thickness, "5");

    QVERIFY2(banner(&w).contains("gauge length Le"), qPrintable(banner(&w)));
    auto *gauge = labelled<QLineEdit>(&w, "Gauge length Le (mm)");
    QVERIFY(gauge);
    type(gauge, "50");

    QVERIFY2(banner(&w).startsWith("Analysed"), qPrintable(banner(&w)));
    shot(&w, "3-analysed");
    QVERIFY2(panelOverflow(&w).isEmpty(), qPrintable(panelOverflow(&w)));
}

} // namespace

class MainWindowWalkthrough : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { tf::gui::MainWindow::setNativeDialogs(false); }

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

    void the_first_screen_asks_for_the_procedure_and_says_where_results_will_appear()
    {
        tf::gui::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY2(banner(&window).contains("Choose the procedure"), qPrintable(banner(&window)));
        QVERIFY2(banner(&window).contains("bending"), qPrintable(banner(&window)));
        QVERIFY(button(&window, "Open data..."));
        bool says = false;
        for (QLabel *l : window.findChildren<QLabel *>())
            says = says || (l->isVisible() && l->text().contains("No analysis yet"));
        QVERIFY(says);
        QVERIFY(!menuItem(&window, "File", "Export report...")->isEnabled());
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

    // The Phase 2 exit criterion.
    void open_a_csv_and_export_a_report_following_the_screen()
    {
        QTemporaryDir tmp;
        tf::gui::MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        followToAnalysis(w);
        if (QTest::currentTestFailed())
            return;

        QTableWidget *t = resultsTable(&w);
        QVERIFY(t);
        QCOMPARE(cell(t, "Rp0.2", 1), QStringLiteral("300"));
        QCOMPARE(cell(t, "Rm", 1), QStringLiteral("500"));
        bool aligned = false;
        for (QLabel *l : w.findChildren<QLabel *>())
            aligned = aligned || (l->isVisible() && l->text().contains("not to the standard"));
        QVERIFY2(aligned, "the window does not say the results are not to the standard");

        // The banner says where the report is: File > Export report.
        QVERIFY(banner(&w).contains("File > Export report"));
        QAction *report = menuItem(&w, "File", "Export report...");
        QVERIFY(report && report->isEnabled());
        const QString pdf = tmp.path() + "/RO-1.pdf";
        answerFileDialog(pdf);
        report->trigger();
        QVERIFY2(bytesOf(pdf).startsWith("%PDF"), "no PDF written");
        if (!qEnvironmentVariable("TF_WALKTHROUGH_SHOTS").isEmpty())
            QFile::copy(pdf, qEnvironmentVariable("TF_WALKTHROUGH_SHOTS") + "/report.pdf");

        const QString html = tmp.path() + "/RO-1.html";
        answerFileDialog(html);
        report->trigger();
        const QString page = QString::fromUtf8(bytesOf(html));
        QVERIFY(page.contains("<td>300</td>"));
        QVERIFY(page.contains("not to the standard"));
        QVERIFY2(page.contains("data:image/png;base64,"), "the chart is not in the report");
    }

    void a_saved_record_opens_again_with_the_same_results_and_nothing_to_report()
    {
        QTemporaryDir tmp;
        const QString dir = tmp.path() + "/RO-1";
        {
            tf::gui::MainWindow w;
            w.show();
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            followToAnalysis(w);
            if (QTest::currentTestFailed())
                return;
            QAction *save = menuItem(&w, "File", "Save record...");
            QVERIFY(save && save->isEnabled());
            answerFileDialog(dir);
            save->trigger();
            QVERIFY(QFile::exists(dir + "/manifest.tfrec"));
        }
        tf::gui::MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        answerFileDialog(dir + "/manifest.tfrec");
        menuItem(&w, "File", "Open record...")->trigger();
        QVERIFY2(banner(&w).startsWith("Analysed"), qPrintable(banner(&w)));
        QCOMPARE(cell(resultsTable(&w), "Rp0.2", 1), QStringLiteral("300"));
        QCOMPARE(labelled<QLineEdit>(&w, "Width (mm)")->text(), QStringLiteral("15.708"));
        QVERIFY(!button(&w, "Add them to the record's manifest"));
    }

    void a_file_another_tool_left_in_a_record_is_offered_not_adopted()
    {
        QTemporaryDir tmp;
        const QString dir = tmp.path() + "/RO-1";
        {
            tf::gui::MainWindow w;
            w.show();
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            followToAnalysis(w);
            if (QTest::currentTestFailed())
                return;
            answerFileDialog(dir);
            menuItem(&w, "File", "Save record...")->trigger();
        }
        QDir().mkpath(dir + "/analyses/fit");
        QFile f(dir + "/analyses/fit/fit.py");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("print(1)\n");
        f.close();

        tf::gui::MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        answerFileDialog(dir + "/manifest.tfrec");
        menuItem(&w, "File", "Open record...")->trigger();
        bool said = false;
        for (QLabel *l : w.findChildren<QLabel *>())
            said = said || (l->isVisible() && l->text().contains("analyses/fit/fit.py"));
        QVERIFY2(said, "the unlisted file is not named on screen");
        QAbstractButton *add = button(&w, "Add them to the record's manifest");
        QVERIFY(add);
        QTest::mouseClick(add, Qt::LeftButton);
        QVERIFY(bytesOf(dir + "/manifest.tfrec").contains("analyses/fit/fit.py"));
        QVERIFY(!button(&w, "Add them to the record's manifest"));
    }

    void a_file_that_is_not_a_table_is_refused_on_screen_without_a_dialog()
    {
        tf::gui::MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        answerFileDialog(fixture("profiles/user-polymer.json"));
        QTest::mouseClick(button(&w, "Open data..."), Qt::LeftButton);
        bool said = false;
        for (QLabel *l : w.findChildren<QLabel *>())
            said = said || (l->isVisible() && l->text().contains("could not be opened"));
        QVERIFY(said);
        QVERIFY(!QApplication::activeModalWidget());
        // Nothing was taken in: the banner still asks for the first step.
        QVERIFY2(banner(&w).contains("Choose the procedure"), qPrintable(banner(&w)));
    }

    // The Phase 3 walkthrough: a real four-point bending export from
    // testXpert III (TU Braunschweig, CC BY 4.0), followed by the banner
    // to testXpert's own modulus, and a report.
    void bend_a_real_beam_and_export_a_report_following_the_screen()
    {
        QTemporaryDir tmp;
        tf::gui::MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        chooseProcedure(&w, {"ISO 14125", "four-point"});
        if (QTest::currentTestFailed())
            return;
        // A bend specimen is a rectangular beam: no shape to choose.
        QVERIFY(!labelled<QComboBox>(&w, "Shape"));
        QVERIFY(labelled<QLineEdit>(&w, "Support span L (mm)"));
        QVERIFY(!labelled<QLineEdit>(&w, "Gauge length Le (mm)"));

        QVERIFY2(banner(&w).contains("File > Open data"), qPrintable(banner(&w)));
        answerFileDialog(fixture(
            "braunschweig-14125/4PB_DIN14125_NFK_Osten_19122024_Hemp80_Cdry_reference_class3_1.CSV"));
        QTest::mouseClick(button(&w, "Open data..."), Qt::LeftButton);

        // testXpert writes no units; the banner asks for each.
        QVERIFY2(banner(&w).contains("unit of the force column"), qPrintable(banner(&w)));
        auto *forceUnit = labelled<QComboBox>(&w, "Force unit");
        QVERIFY(forceUnit);
        forceUnit->setCurrentIndex(forceUnit->findText("N"));
        QVERIFY2(banner(&w).contains("unit of the deflection column"), qPrintable(banner(&w)));
        auto *defUnit = labelled<QComboBox>(&w, "Deflection unit");
        QVERIFY2(defUnit, "the displacement is not called the deflection in a bend test");
        defUnit->setCurrentIndex(defUnit->findText("mm"));

        // Width, thickness and span come from the file's header, and say so.
        QCOMPARE(labelled<QLineEdit>(&w, "Width (mm)")->text(), QStringLiteral("14.85"));
        QCOMPARE(labelled<QLineEdit>(&w, "Thickness (mm)")->text(), QStringLiteral("2.52"));
        QCOMPARE(labelled<QLineEdit>(&w, "Support span L (mm)")->text(), QStringLiteral("45"));

        QVERIFY2(banner(&w).contains("loading span"), qPrintable(banner(&w)));
        type(labelled<QLineEdit>(&w, "Loading span (mm)"), "15");
        QVERIFY2(banner(&w).contains("where the deflection was measured"), qPrintable(banner(&w)));
        shot(&w, "bend-1-where");
        auto *at = labelled<QComboBox>(&w, "Deflection measured at");
        QVERIFY(at);
        at->setCurrentIndex(at->findText("Loading points (crosshead)"));

        QVERIFY2(banner(&w).startsWith("Analysed"), qPrintable(banner(&w)));
        shot(&w, "bend-2-analysed");
        QVERIFY2(panelOverflow(&w).isEmpty(), qPrintable(panelOverflow(&w)));
        QTableWidget *t = resultsTable(&w);
        QVERIFY(t);
        // testXpert III printed 8.54 GPa and 174.162 MPa for this specimen.
        QCOMPARE(cell(t, "Ef", 1), QStringLiteral("8.54"));
        QCOMPARE(cell(t, "sigma_fM", 1), QStringLiteral("174.2"));

        const QString html = tmp.path() + "/Ref_32.html";
        answerFileDialog(html);
        menuItem(&w, "File", "Export report...")->trigger();
        const QString page = QString::fromUtf8(bytesOf(html));
        QVERIFY(page.contains("Flexural test report"));
        QVERIFY(page.contains("Support span L"));
        QVERIFY(page.contains("the loading points"));
        QVERIFY(page.contains("<td>8.54</td>"));
        QVERIFY2(page.contains("loading-span-to-span"), "the fixture check is not in the report");
    }
};

// Headless however it is started. Run straight from a shell on a Wayland
// desktop, Qt would take WAYLAND_DISPLAY and open every window on the
// user's screen (it did, 2026-11-13, during this file's own negative
// checks). So unless it is already on its private X server, the test
// restarts itself under xvfb-run with Wayland cleared and software GL.
// TF_WALKTHROUGH_ON_DESKTOP=1 is the one way to watch it on purpose.
static void ensureHeadless(char *argv[])
{
    if (qEnvironmentVariableIsSet("TF_WALKTHROUGH_XVFB")
        || qEnvironmentVariable("TF_WALKTHROUGH_ON_DESKTOP") == "1")
        return;
    qputenv("TF_WALKTHROUGH_XVFB", "1");
    qputenv("QT_QPA_PLATFORM", "xcb");
    qputenv("LIBGL_ALWAYS_SOFTWARE", "1");
    qunsetenv("WAYLAND_DISPLAY");
    std::vector<char *> args{const_cast<char *>("xvfb-run"), const_cast<char *>("-a"),
                             const_cast<char *>("-s"),
                             const_cast<char *>("-screen 0 1600x1000x24")};
    for (char **a = argv; *a; ++a)
        args.push_back(*a);
    args.push_back(nullptr);
    execvp("xvfb-run", args.data());
    // Only reached when xvfb-run cannot be started: refuse rather than
    // fall back to the desktop.
    std::perror("walkthrough: cannot start xvfb-run (install xvfb, or set "
                "TF_WALKTHROUGH_ON_DESKTOP=1 to run on your screen)");
    std::exit(2);
}

int main(int argc, char *argv[])
{
    ensureHeadless(argv);
    // As in the application: VTK's widget needs its surface format set
    // before the QApplication exists.
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);
    MainWindowWalkthrough test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_main_window_walkthrough.moc"
