#include "gui/MainWindow.h"

#include "core/ApplicationInfo.h"
#include "core/Record.h"
#include "core/Report.h"
#include "core/Annotations.h"
#include "core/Exports.h"
#include "gui/AnnotatedChart.h"
#include "gui/ChartExport.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPageSize>
#include <QPdfWriter>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkAxis.h>
#include <vtkChartLegend.h>
#include <vtkChartXY.h>
#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkDoubleArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>
#include <vtkBrush.h>
#include <vtkPen.h>
#include <vtkPlot.h>
#include <vtkPlotArea.h>
#include <vtkPlotPoints.h>
#include <vtkTable.h>

#include <cmath>

namespace tf::gui {

using tf::core::Specimen;

namespace {

bool s_nativeDialogs = true;

const char *kProfileFile = "\x01profile-file";  // the "A profile file..." entry

std::optional<double> parsed(const QLineEdit *e, double toSi)
{
    QString t = e->text().trimmed();
    if (t.isEmpty())
        return std::nullopt;
    t.replace(',', '.');
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (!ok)
        return std::nullopt;
    return v * toSi;
}

void showValue(QLineEdit *e, const std::optional<double> &si, double fromSi)
{
    e->setText(si ? QString::number(*si * fromSi, 'g', 10) : QString());
}

void select(QComboBox *c, const QString &data)
{
    const int i = c->findData(data);
    c->setCurrentIndex(i >= 0 ? i : 0);
}

QLabel *wrapped(const QString &text, const char *name)
{
    auto *l = new QLabel(text);
    l->setObjectName(QLatin1String(name));
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

} // namespace

void MainWindow::setNativeDialogs(bool on) { s_nativeDialogs = on; }

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("%1 - %2")
                       .arg(tf::core::applicationName(), tf::core::applicationSubtitle()));

    // --- Menus ----------------------------------------------------------------
    QMenu *file = menuBar()->addMenu(tr("&File"));
    QAction *openDataAct = file->addAction(tr("Open &data..."), this, &MainWindow::openData);
    openDataAct->setShortcut(QKeySequence::Open);
    file->addAction(tr("Open &record..."), this, &MainWindow::openRecord);
    file->addSeparator();
    m_saveRecord = file->addAction(tr("&Save record..."), this, &MainWindow::saveRecord);
    m_saveRecord->setShortcut(QKeySequence::Save);
    m_exportReport = file->addAction(tr("&Export report..."), this, &MainWindow::exportReport);
    m_exportReport->setShortcut(QKeySequence(tr("Ctrl+E")));
    m_exportChart = file->addAction(tr("Export &chart..."), this, &MainWindow::exportChart);
    m_exportFreecad = file->addAction(tr("Export for &FreeCAD and CalculiX..."), this,
                                      &MainWindow::exportFreecad);
    m_exportVtp = file->addAction(tr("Export curve for &ParaView..."), this, &MainWindow::exportVtp);
    m_exportPmd = file->addAction(tr("Export for Platform &MaterialDigital (JSON-LD)..."), this,
                                  &MainWindow::exportPmd);
    file->addSeparator();
    m_addReference = file->addAction(tr("Add reference curve (PlotTracer)..."), this,
                                     &MainWindow::addReference);
    m_clearReferences = file->addAction(tr("Remove reference curves"), this, [this] {
        m_session.references.clear();
        refresh();
    });
    file->addSeparator();
    file->addAction(tr("&Quit"), QKeySequence::Quit, qApp, &QApplication::quit);
    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&About TenForge"), this, [this] {
        QMessageBox::about(
            this, tr("About TenForge"),
            QStringLiteral("<h3>%1 %2</h3><p>%3</p><p>Build %4</p>"
                           "<p>Licensed under the GNU LGPL, version 2.1 or later.</p>")
                .arg(tf::core::applicationName(), tf::core::applicationVersion(),
                     tf::core::applicationSubtitle(), tf::core::applicationBuild()));
    });

    // --- The window: the next step on top, inputs left, outcome right -----------
    auto *central = new QWidget(this);
    auto *outer = new QVBoxLayout(central);

    m_next = wrapped({}, "nextStep");
    m_next->setStyleSheet(QStringLiteral(
        "QLabel#nextStep { font-weight: bold; padding: 8px; border: 1px solid palette(mid);"
        " border-radius: 4px; background: palette(alternate-base); }"));
    outer->addWidget(m_next);

    auto *noticeRow = new QHBoxLayout;
    m_notices = wrapped({}, "notices");
    m_notices->setStyleSheet(QStringLiteral("QLabel#notices { color: #a05a00; }"));
    m_addUnlisted = new QPushButton(tr("Add them to the record's manifest"));
    m_addUnlisted->setObjectName(QStringLiteral("addUnlisted"));
    connect(m_addUnlisted, &QPushButton::clicked, this, &MainWindow::addUnlisted);
    noticeRow->addWidget(m_notices, 1);
    noticeRow->addWidget(m_addUnlisted, 0, Qt::AlignTop);
    outer->addLayout(noticeRow);

    auto *split = new QSplitter(Qt::Horizontal);
    split->addWidget(buildInputs());
    split->addWidget(buildOutputs());
    split->setStretchFactor(1, 1);
    split->setSizes({420, 780});
    outer->addWidget(split, 1);
    setCentralWidget(central);
    resize(1200, 800);

    showSession();
    refresh();
}

MainWindow::~MainWindow() = default;

QWidget *MainWindow::buildInputs()
{
    auto *inner = new QWidget;
    auto *v = new QVBoxLayout(inner);

    // 1. Procedure: first, because its mode (tension, compression, bending)
    // decides which fields below are needed.
    auto *proc = new QGroupBox(tr("1. Procedure"));
    auto *pf = new QFormLayout(proc);
    m_profile = new QComboBox;
    m_profile->setObjectName(QStringLiteral("profile"));
    pf->addRow(tr("Procedure"), m_profile);
    v->addWidget(proc);

    // 2. Data
    auto *data = new QGroupBox(tr("2. Data"));
    auto *df = new QFormLayout(data);
    m_dataForm = df;
    auto *openBtn = new QPushButton(tr("Open data..."));
    openBtn->setObjectName(QStringLiteral("openData"));
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::openData);
    m_file = wrapped(tr("No file open."), "fileName");
    df->addRow(openBtn);
    // Machine software writes long file names without spaces, which a
    // label cannot wrap; they must not widen the panel (seen on a testXpert
    // export: the panel was cut off). Shown elided, in full as a tooltip.
    m_file->setWordWrap(false);
    m_file->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    df->addRow(m_file);
    m_forceColumn = new QComboBox;
    m_forceUnit = new QComboBox;
    m_extColumn = new QComboBox;
    m_extUnit = new QComboBox;
    m_forceColumn->setObjectName(QStringLiteral("forceColumn"));
    m_forceUnit->setObjectName(QStringLiteral("forceUnit"));
    m_extColumn->setObjectName(QStringLiteral("extensionColumn"));
    m_extUnit->setObjectName(QStringLiteral("extensionUnit"));
    df->addRow(tr("Force column"), m_forceColumn);
    df->addRow(tr("Force unit"), m_forceUnit);
    df->addRow(tr("Extension column"), m_extColumn);
    df->addRow(tr("Extension unit"), m_extUnit);
    m_readNotes = wrapped({}, "readNotes");
    m_readNotes->setStyleSheet(QStringLiteral("color: palette(placeholder-text);"));
    m_readNotes->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    df->addRow(m_readNotes);
    m_forceUnit->addItem(tr("Choose..."), QString());
    for (const QString &u : tf::core::knownUnits("force"))
        m_forceUnit->addItem(u, u);
    m_extUnit->addItem(tr("Choose..."), QString());
    for (const QString &u : tf::core::knownUnits("length") + tf::core::knownUnits("strain"))
        m_extUnit->addItem(u, u);
    // Optional: the machine's own compliance, taken out of the displacement.
    m_complianceButton = new QPushButton(tr("Machine compliance..."));
    m_complianceButton->setObjectName(QStringLiteral("complianceButton"));
    m_complianceButton->setToolTip(tr("Optional. A file of force against the machine's own "
                                      "displacement, measured without a specimen."));
    connect(m_complianceButton, &QPushButton::clicked, this, &MainWindow::chooseCompliance);
    m_clearCompliance = new QPushButton(tr("None"));
    m_clearCompliance->setObjectName(QStringLiteral("clearCompliance"));
    connect(m_clearCompliance, &QPushButton::clicked, this, [this] {
        m_session.setCompliance({});
        showSession();
        refresh();
    });
    m_complianceFile = wrapped(tr("Machine compliance: none"), "complianceFile");
    m_complianceFile->setWordWrap(false);
    m_complianceFile->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *complianceRow = new QWidget;
    auto *crow = new QHBoxLayout(complianceRow);
    crow->setContentsMargins(0, 0, 0, 0);
    crow->addWidget(m_complianceButton);
    crow->addWidget(m_clearCompliance);
    df->addRow(complianceRow);
    df->addRow(m_complianceFile);
    v->addWidget(data);

    // 3. Specimen, and the fixture where the mode needs one
    auto *spec = new QGroupBox(tr("3. Specimen"));
    auto *sf = new QFormLayout(spec);
    m_id = new QLineEdit;
    m_id->setObjectName(QStringLiteral("specimenId"));
    m_material = new QLineEdit;
    m_material->setObjectName(QStringLiteral("material"));
    sf->addRow(tr("Specimen id"), m_id);
    sf->addRow(tr("Material"), m_material);
    m_shape = new QComboBox;
    m_shape->setObjectName(QStringLiteral("shape"));
    m_shape->addItem(tr("Rectangular"));
    m_shape->addItem(tr("Round"));
    m_shape->addItem(tr("Tube"));
    m_shape->addItem(tr("Cross-section area given"));
    sf->addRow(tr("Shape"), m_shape);

    // One form, so every field lines up; the shape and mode decide which
    // rows show.
    for (auto [label, edit, name] :
         {std::tuple{tr("Width (mm)"), &m_width, "width"},
          std::tuple{tr("Thickness (mm)"), &m_thickness, "thickness"},
          std::tuple{tr("Diameter (mm)"), &m_diameter, "diameter"},
          std::tuple{tr("Outer diameter (mm)"), &m_outer, "outerDiameter"},
          std::tuple{tr("Inner diameter (mm)"), &m_inner, "innerDiameter"},
          std::tuple{tr("Cross-section S0 (mm2)"), &m_area, "area"}}) {
        *edit = new QLineEdit;
        (*edit)->setObjectName(QLatin1String(name));
        sf->addRow(label, *edit);
    }
    m_specForm = sf;
    m_s0 = wrapped({}, "s0");
    sf->addRow(tr("S0"), m_s0);
    m_gauge = new QLineEdit;
    m_gauge->setObjectName(QStringLiteral("gaugeLength"));
    sf->addRow(tr("Gauge length Le (mm)"), m_gauge);
    m_span = new QLineEdit;
    m_span->setObjectName(QStringLiteral("span"));
    sf->addRow(tr("Support span L (mm)"), m_span);
    m_loadingSpan = new QLineEdit;
    m_loadingSpan->setObjectName(QStringLiteral("loadingSpan"));
    sf->addRow(tr("Loading span (mm)"), m_loadingSpan);
    m_deflectionAt = new QComboBox;
    m_deflectionAt->setObjectName(QStringLiteral("deflectionAt"));
    m_deflectionAt->addItem(tr("Choose..."), QString());
    m_deflectionAt->addItem(tr("Loading points (crosshead)"), QStringLiteral("loading-points"));
    m_deflectionAt->addItem(tr("Mid-span (deflectometer)"), QStringLiteral("mid-span"));
    sf->addRow(tr("Deflection measured at"), m_deflectionAt);
    showShapeRows();
    v->addWidget(spec);
    v->addStretch(1);

    // Long column names and procedure titles must not widen the panel.
    // Nothing in it may make the panel wider than it shows (the walkthrough
    // checks): every list and label sizes down.
    for (QComboBox *c : {m_forceColumn, m_extColumn, m_profile, m_forceUnit, m_extUnit,
                         m_shape, m_deflectionAt}) {
        c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->setMinimumContentsLength(10);
    }

    // Every edit goes to the session and the outcome is redrawn.
    for (QComboBox *c : {m_forceColumn, m_forceUnit, m_extColumn, m_extUnit, m_deflectionAt})
        connect(c, &QComboBox::currentIndexChanged, this, [this] { readInputs(); });
    connect(m_shape, &QComboBox::currentIndexChanged, this, [this] {
        showShapeRows();
        readInputs();
    });
    for (QLineEdit *e : {m_id, m_material, m_width, m_thickness, m_diameter, m_outer, m_inner,
                         m_area, m_gauge, m_span, m_loadingSpan})
        connect(e, &QLineEdit::textChanged, this, [this] { readInputs(); });
    connect(m_profile, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_profile->itemData(i).toString() == QLatin1String(kProfileFile) || m_updating)
            return;
        m_session.profile = m_profile->itemData(i).toString();
        showShapeRows();
        readInputs();
    });
    connect(m_profile, &QComboBox::activated, this, [this](int i) {
        if (m_profile->itemData(i).toString() != QLatin1String(kProfileFile))
            return;
        const QString path = chooseFile(false, tr("Choose a profile file"),
                                        tr("TenForge profiles (*.json)"), {});
        const auto p = path.isEmpty() ? tf::core::ProfileOrError{} : tf::core::loadProfile(path);
        if (p.profile) {
            m_profileFile = path;
            m_session.profile = path;
        } else if (!path.isEmpty()) {
            m_notices->setText(tr("That profile cannot be used: %1").arg(p.error));
            m_notices->setVisible(true);
        }
        showSession();
        refresh();
    });

    auto *scroll = new QScrollArea;
    scroll->setWidget(inner);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(360);
    return scroll;
}

QWidget *MainWindow::buildOutputs()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);

    m_emptyState = wrapped(tr("No analysis yet. The stress-strain curve and the results "
                              "appear here once the steps on the left are done."),
                           "emptyState");
    v->addWidget(m_emptyState);

    // The chart area: VTK's own charting inside Qt (never Qt Charts, which is
    // GPL-3.0-only and would force TenForge's licence to GPL).
    m_chart = new QVTKOpenGLNativeWidget;
    m_chart->setObjectName(QStringLiteral("chart"));
    m_chart->setMinimumHeight(320);
    vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
    m_chart->setRenderWindow(renderWindow);
    m_view = vtkSmartPointer<vtkContextView>::New();
    m_view->SetRenderWindow(renderWindow);
    m_chartXY = vtkSmartPointer<AnnotatedChart>::New();
    // Dragging an edge of the modulus window moves it: the session refits.
    m_chartXY->onWindowMoved = [this](double from, double to) {
        m_session.modulusWindow = std::make_pair(from / 100.0, to / 100.0);
        showWindowFields();
        refresh();
    };
    m_chartXY->GetAxis(vtkAxis::BOTTOM)->SetTitle("Strain (%)");
    m_chartXY->GetAxis(vtkAxis::LEFT)->SetTitle("Stress (MPa)");
    m_chartXY->SetShowLegend(true);
    // Bottom right: a tensile curve rises to the top left and top right,
    // the bottom right stays clear until fracture.
    m_chartXY->GetLegend()->SetHorizontalAlignment(vtkChartLegend::RIGHT);
    m_chartXY->GetLegend()->SetVerticalAlignment(vtkChartLegend::BOTTOM);
    m_view->GetScene()->AddItem(m_chartXY);
    v->addWidget(m_chart, 3);

    // The modulus window: drag its dashed edges on the chart, or type it.
    m_windowRow = new QWidget;
    auto *wr = new QHBoxLayout(m_windowRow);
    wr->setContentsMargins(0, 0, 0, 0);
    auto *hint = new QLabel(tr("Modulus window: drag its dashed edges on the chart (the wheel zooms), "
                            "or type it."));
    hint->setObjectName(QStringLiteral("windowHint"));
    wr->addWidget(hint, 1);
    m_windowFrom = new QLineEdit;
    m_windowFrom->setObjectName(QStringLiteral("windowFrom"));
    m_windowTo = new QLineEdit;
    m_windowTo->setObjectName(QStringLiteral("windowTo"));
    for (auto [text, edit] : {std::pair{tr("From (%)"), m_windowFrom},
                              std::pair{tr("To (%)"), m_windowTo}}) {
        auto *l = new QLabel(text);
        l->setBuddy(edit);
        edit->setMaximumWidth(80);
        wr->addWidget(l);
        wr->addWidget(edit);
        connect(edit, &QLineEdit::editingFinished, this, [this] { readWindowFields(); });
    }
    m_windowReset = new QPushButton(tr("Use the procedure's window"));
    m_windowReset->setObjectName(QStringLiteral("windowReset"));
    connect(m_windowReset, &QPushButton::clicked, this, [this] {
        m_session.modulusWindow.reset();
        showWindowFields();
        refresh();
    });
    wr->addWidget(m_windowReset);
    v->addWidget(m_windowRow);

    m_conformity = wrapped({}, "conformity");
    m_conformity->setStyleSheet(QStringLiteral(
        "QLabel#conformity { border: 2px solid #b36b00; padding: 6px; }"));
    v->addWidget(m_conformity);

    m_results = new QTableWidget(0, 4);
    m_results->setObjectName(QStringLiteral("results"));
    m_results->setHorizontalHeaderLabels({tr("Result"), tr("Value"), tr("Unit"), tr("How, or why not")});
    m_results->horizontalHeader()->setStretchLastSection(true);
    m_results->verticalHeader()->setVisible(false);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setWordWrap(true);

    // Notes on the curve: the user's own, drawn on the chart and kept in
    // the record. A note at one strain, or over a span of strain.
    auto *notes = new QWidget;
    auto *nv = new QVBoxLayout(notes);
    m_notesTable = new QTableWidget(0, 3);
    m_notesTable->setObjectName(QStringLiteral("notes"));
    m_notesTable->setHorizontalHeaderLabels({tr("At strain (%)"), tr("To strain (%)"), tr("Note")});
    m_notesTable->horizontalHeader()->setStretchLastSection(true);
    m_notesTable->verticalHeader()->setVisible(false);
    connect(m_notesTable, &QTableWidget::cellChanged, this, [this] { readNotes(); });
    nv->addWidget(m_notesTable, 1);
    auto *nb = new QHBoxLayout;
    for (auto [text, fn] : {std::pair{tr("Add note"), 0}, std::pair{tr("Add span"), 1},
                            std::pair{tr("Remove note"), 2}}) {
        auto *b = new QPushButton(text);
        nb->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, fn = fn] {
            if (fn == 2) {
                const int row = m_notesTable->currentRow();
                if (row >= 0 && row < int(m_session.curveNotes.size()))
                    m_session.curveNotes.erase(m_session.curveNotes.begin() + row);
                showNotes();
                showChart();
            } else {
                addNote(fn == 1);
            }
        });
    }
    nb->addStretch(1);
    nv->addLayout(nb);

    m_tabs = new QTabWidget;
    m_tabs->addTab(m_results, tr("Results"));
    m_tabs->addTab(notes, tr("Notes on the curve"));
    v->addWidget(m_tabs, 2);

    m_flags = wrapped({}, "flags");
    v->addWidget(m_flags);
    return w;
}

QString MainWindow::chooseFile(bool save, const QString &caption, const QString &filter,
                               const QString &suggested)
{
    static QString lastDir;
    QFileDialog dlg(this, caption, lastDir, filter);
    dlg.setAcceptMode(save ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
    dlg.setFileMode(save ? QFileDialog::AnyFile : QFileDialog::ExistingFile);
    if (!s_nativeDialogs)
        dlg.setOption(QFileDialog::DontUseNativeDialog);
    if (!suggested.isEmpty())
        dlg.selectFile(suggested);
    if (dlg.exec() != QDialog::Accepted || dlg.selectedFiles().isEmpty())
        return {};
    const QString path = dlg.selectedFiles().front();
    lastDir = QFileInfo(path).absolutePath();
    return path;
}

// --- Session to widgets and back ------------------------------------------------

void MainWindow::showSession()
{
    m_updating = true;
    const auto *t = m_session.table();
    const QString fileText = t ? QFileInfo(t->path).fileName()
                                     + (m_session.recordDir().isEmpty()
                                            ? QString()
                                            : tr(" (in the record %1)").arg(m_session.recordDir()))
                               : tr("No file open.");
    m_file->setToolTip(t ? t->path : QString());
    m_file->setText(m_file->fontMetrics().elidedText(fileText, Qt::ElideMiddle, 320));
    for (QComboBox *c : {m_forceColumn, m_extColumn}) {
        c->clear();
        c->addItem(tr("Choose..."), QString());
        if (t)
            for (int i = 0; i < t->names.size(); ++i)
                c->addItem(t->names[i] + (t->units.value(i).isEmpty() ? QString()
                                                                      : " (" + t->units[i] + ")"),
                           t->names[i]);
    }
    select(m_forceColumn, m_session.mapping.forceColumn);
    select(m_extColumn, m_session.mapping.extensionColumn);
    select(m_forceUnit, m_session.mapping.forceUnit);
    select(m_extUnit, m_session.mapping.extensionUnit);
    m_readNotes->setText(m_session.notes().join("\n"));

    const Specimen &s = m_session.specimen;
    m_id->setText(s.id);
    m_material->setText(s.material);
    m_shape->setCurrentIndex(int(s.shape));
    showShapeRows();
    showValue(m_width, s.width, 1e3);
    showValue(m_thickness, s.thickness, 1e3);
    showValue(m_diameter, s.diameter, 1e3);
    showValue(m_outer, s.outerDiameter, 1e3);
    showValue(m_inner, s.innerDiameter, 1e3);
    showValue(m_area, s.area, 1e6);
    showValue(m_gauge, s.gaugeLength, 1e3);
    showValue(m_span, m_session.fixture.span, 1e3);
    showValue(m_loadingSpan, m_session.fixture.loadingSpan, 1e3);
    select(m_deflectionAt, !m_session.fixture.deflectionAt ? QString()
                           : *m_session.fixture.deflectionAt == tf::core::Fixture::DeflectionAt::Midspan
                               ? QStringLiteral("mid-span")
                               : QStringLiteral("loading-points"));
    m_complianceFile->setText(tr("Machine compliance: %1")
                                  .arg(m_session.compliance().isEmpty()
                                           ? tr("none")
                                           : QFileInfo(m_session.compliance()).fileName()));

    m_profile->clear();
    m_profile->addItem(tr("Choose a procedure..."), QString());
    for (const QString &id : tf::core::shippedProfiles()) {
        const auto p = tf::core::loadProfile(id);
        m_profile->addItem(p.profile ? p.profile->displayName() : id, id);
    }
    if (!m_profileFile.isEmpty())
        m_profile->addItem(QFileInfo(m_profileFile).fileName() + tr(" (your profile)"),
                           m_profileFile);
    m_profile->addItem(tr("A profile file..."), QString::fromLatin1(kProfileFile));
    select(m_profile, m_session.profile);
    showShapeRows();

    m_notices->setText(m_session.notices().join("\n"));
    m_notices->setVisible(!m_session.notices().isEmpty());
    m_addUnlisted->setVisible(!m_session.unlisted().isEmpty());
    showNotes();
    showWindowFields();
    m_updating = false;
}

void MainWindow::showShapeRows()
{
    const QString mode = m_session.mode();
    const bool bend = mode == "flexure";
    const bool four = m_session.fixtureKind() == "four-point";
    // A bend specimen is a rectangular beam; the shape is not a choice.
    if (bend && m_shape->currentIndex() != int(Specimen::Shape::Rectangular)) {
        const bool was = m_updating;
        m_updating = true;
        m_shape->setCurrentIndex(int(Specimen::Shape::Rectangular));
        m_updating = was;
    }
    const auto shape = Specimen::Shape(m_shape->currentIndex());
    const std::map<QWidget *, bool> rows{
        {m_shape, !bend},
        {m_width, shape == Specimen::Shape::Rectangular},
        {m_thickness, shape == Specimen::Shape::Rectangular},
        {m_diameter, !bend && shape == Specimen::Shape::Round},
        {m_outer, !bend && shape == Specimen::Shape::Tube},
        {m_inner, !bend && shape == Specimen::Shape::Tube},
        {m_area, !bend && shape == Specimen::Shape::GivenArea},
        {m_s0, !bend},
        {m_gauge, !bend},
        {m_span, bend},
        {m_loadingSpan, bend && four},
        {m_deflectionAt, bend && four}};
    for (const auto &[w, on] : rows)
        m_specForm->setRowVisible(w, on);

    // The words follow the mode: a bend test has a deflection, a
    // compression test a deformation measured over a length L0.
    const QString word = m_session.displacementWord();
    const QString Word = word.left(1).toUpper() + word.mid(1);
    if (auto *l = qobject_cast<QLabel *>(m_dataForm->labelForField(m_extColumn)))
        l->setText(tr("%1 column").arg(Word));
    if (auto *l = qobject_cast<QLabel *>(m_dataForm->labelForField(m_extUnit)))
        l->setText(tr("%1 unit").arg(Word));
    if (auto *l = qobject_cast<QLabel *>(m_specForm->labelForField(m_gauge)))
        l->setText(mode == "compression" ? tr("Height or gauge length L0 (mm)")
                                         : tr("Gauge length Le (mm)"));
    // Compliance applies to a displacement in compression and bending.
    const bool curveMode = mode == "compression" || bend;
    m_dataForm->setRowVisible(m_complianceButton->parentWidget(), curveMode);
    m_dataForm->setRowVisible(m_complianceFile, curveMode);
}

void MainWindow::readInputs()
{
    if (m_updating)
        return;
    auto &m = m_session.mapping;
    m.forceColumn = m_forceColumn->currentData().toString();
    m.forceUnit = m_forceUnit->currentData().toString();
    m.extensionColumn = m_extColumn->currentData().toString();
    m.extensionUnit = m_extUnit->currentData().toString();

    Specimen &s = m_session.specimen;
    s.id = m_id->text().trimmed();
    s.material = m_material->text().trimmed();
    s.shape = Specimen::Shape(m_shape->currentIndex());
    s.width = parsed(m_width, 1e-3);
    s.thickness = parsed(m_thickness, 1e-3);
    s.diameter = parsed(m_diameter, 1e-3);
    s.outerDiameter = parsed(m_outer, 1e-3);
    s.innerDiameter = parsed(m_inner, 1e-3);
    s.area = parsed(m_area, 1e-6);
    s.gaugeLength = parsed(m_gauge, 1e-3);
    auto &f = m_session.fixture;
    f.span = parsed(m_span, 1e-3);
    f.loadingSpan = parsed(m_loadingSpan, 1e-3);
    const QString at = m_deflectionAt->currentData().toString();
    f.deflectionAt = at == "mid-span"         ? std::optional(tf::core::Fixture::DeflectionAt::Midspan)
                     : at == "loading-points" ? std::optional(tf::core::Fixture::DeflectionAt::LoadingPoints)
                                              : std::nullopt;

    const QString p = m_profile->currentData().toString();
    if (p != QLatin1String(kProfileFile))
        m_session.profile = p;
    refresh();
}

void MainWindow::refresh()
{
    m_session.analyse();
    const tf::core::Step step = m_session.next();
    m_next->setText(step.ready ? step.text : tr("Next: %1").arg(step.text));

    // The one field the next step is about is outlined.
    const std::map<QString, QWidget *> fields{
        {"forceColumn", m_forceColumn}, {"forceUnit", m_forceUnit},
        {"extensionColumn", m_extColumn}, {"extensionUnit", m_extUnit},
        {"gaugeLength", m_gauge}, {"profile", m_profile}, {"span", m_span},
        {"loadingSpan", m_loadingSpan}, {"deflectionAt", m_deflectionAt},
        {"compliance", m_complianceButton}};
    const QString wanted = QStringLiteral("border: 2px solid #d08000;");
    for (const auto &[name, w] : fields)
        w->setStyleSheet(name == step.field ? wanted : QString());
    // A missing dimension: every empty one of the shape's fields.
    for (QLineEdit *e : {m_width, m_thickness, m_diameter, m_outer, m_inner, m_area})
        e->setStyleSheet(step.field == "specimen" && e->text().trimmed().isEmpty() ? wanted
                                                                                  : QString());

    const auto area = tf::core::crossSection(m_session.specimen);
    m_s0->setText(area.area ? QString::number(*area.area * 1e6, 'g', 8) + " mm2" : tr("-"));

    m_saveRecord->setEnabled(m_session.analysis().has_value());
    m_exportReport->setEnabled(m_session.analysis().has_value());
    m_exportChart->setEnabled(m_session.analysis().has_value());
    m_exportFreecad->setEnabled(m_session.analysis().has_value());
    m_exportVtp->setEnabled(m_session.analysis().has_value());
    m_exportPmd->setEnabled(m_session.analysis().has_value());
    m_clearReferences->setEnabled(!m_session.references.empty());
    m_windowRow->setVisible(m_session.analysis().has_value());
    m_tabs->setVisible(m_session.analysis().has_value());
    showResults();
    showChart();
}

// --- The outcome --------------------------------------------------------------

void MainWindow::showResults()
{
    const auto &a = m_session.analysis();
    m_emptyState->setVisible(!a);
    m_conformity->setVisible(bool(a));
    m_results->setRowCount(0);
    m_flags->clear();
    if (!a)
        return;

    const QJsonObject conf = a->doc.value("conformity").toObject();
    const QString title = a->profile.designation.isEmpty() ? a->profile.title
                                                           : a->profile.designation;
    QStringList reasons;
    for (const QJsonValue &r : conf.value("reasons").toArray()) {
        QString d = r.toObject().value("detail").toString();
        const QString marker = "standard's text: ";
        const int at = d.indexOf(marker);
        reasons << (at > 0 ? d.left(at + int(marker.size()) - 2) + "." : d);
    }
    m_conformity->setText(
        conf.value("status").toString() == "to-standard"
            ? tr("To the standard: %1.").arg(title)
            : tr("Aligned with %1, not to the standard. The results follow the procedure's "
                 "method, but:\n- %2")
                  .arg(title, reasons.join("\n- ")));

    for (const QJsonValue &v : a->doc.value("results").toArray()) {
        const QJsonObject r = v.toObject();
        const int row = m_results->rowCount();
        m_results->insertRow(row);
        const bool det = r.value("status").toString() == "determined";
        const QJsonObject rep = r.value("reported").toObject();
        const QString name = r.value("id").toString();
        auto *item = new QTableWidgetItem(name);
        item->setToolTip(r.value("name").toString());
        m_results->setItem(row, 0, item);
        m_results->setItem(row, 1, new QTableWidgetItem(det ? rep.value("value").toString() : "-"));
        m_results->setItem(row, 2, new QTableWidgetItem(det ? rep.value("unit").toString() : QString()));
        m_results->setItem(row, 3,
                           new QTableWidgetItem(det ? r.value("name").toString()
                                                    : tr("not determined: %1")
                                                          .arg(r.value("reason").toString())));
    }
    m_results->resizeColumnsToContents();
    m_results->resizeRowsToContents();

    QStringList flags;
    for (const QJsonValue &f : a->doc.value("flags").toArray())
        flags << "- " + f.toString();
    m_flags->setText(flags.isEmpty() ? QString() : tr("Flags:\n%1").arg(flags.join("\n")));
}

void MainWindow::showChart()
{
    m_chartXY->ClearPlots();
    const QString mode = m_session.mode();
    m_chartXY->GetAxis(vtkAxis::BOTTOM)->SetTitle(mode == "flexure"       ? "Flexural strain (%)"
                                                  : mode == "compression" ? "Compressive strain (%)"
                                                                          : "Strain (%)");
    m_chartXY->GetAxis(vtkAxis::LEFT)->SetTitle(mode == "flexure"       ? "Flexural stress (MPa)"
                                                : mode == "compression" ? "Compressive stress (MPa)"
                                                                        : "Stress (MPa)");
    const auto &a = m_session.analysis();
    if (a) {
        vtkNew<vtkTable> curve;
        vtkNew<vtkDoubleArray> x, y;
        x->SetName("Strain (%)");
        y->SetName("Stress (MPa)");
        for (std::size_t i = 0; i < a->stress.size(); ++i) {
            const double e = a->strain[i] * 100.0;
            const double s = a->stress[i] * 1e-6;
            if (std::isfinite(e) && std::isfinite(s)) {
                x->InsertNextValue(e);
                y->InsertNextValue(s);
            }
        }
        // The regions the analysis found, shaded under the curve (drawn
        // first, so the curve lies on top); each in the legend.
        double top = 0;
        for (vtkIdType i = 0; i < y->GetNumberOfTuples(); ++i)
            top = std::max(top, y->GetValue(i));
        const std::map<QString, std::array<unsigned char, 3>> shade{
            {"toe", {150, 150, 150}},
            {"modulus-fit", {60, 150, 60}},
            {"uniform-elongation", {80, 130, 200}},
            {"necking", {210, 120, 40}},
            {"after-break", {120, 120, 120}}};
        for (const tf::core::Region &r : tf::core::analysisRegions(*a)) {
            vtkNew<vtkTable> t;
            vtkNew<vtkDoubleArray> rx, lo, hi;
            rx->SetName("x");
            lo->SetName("lo");
            hi->SetName(r.label.toUtf8().constData());
            // vtkPlotArea fills nothing between only two x values (it
            // draws an outline; seen with VTK 9.5): five points across.
            for (int k = 0; k <= 4; ++k) {
                rx->InsertNextValue((r.from + (r.to - r.from) * k / 4.0) * 100.0);
                lo->InsertNextValue(0.0);
                hi->InsertNextValue(top * 1.04);
            }
            t->AddColumn(rx);
            t->AddColumn(lo);
            t->AddColumn(hi);
            auto *area = vtkPlotArea::SafeDownCast(m_chartXY->AddPlot(vtkChart::AREA));
            area->SetInputData(t);
            area->SetInputArray(0, "x");
            area->SetInputArray(1, "lo");
            area->SetInputArray(2, r.label.toUtf8().constData());
            const auto c = shade.count(r.id) ? shade.at(r.id) : std::array<unsigned char, 3>{120, 120, 120};
            area->GetBrush()->SetColor(c[0], c[1], c[2], 50);
            area->GetPen()->SetColor(c[0], c[1], c[2], 0);
            area->SetLabel(r.label.toStdString());
        }

        curve->AddColumn(x);
        curve->AddColumn(y);
        vtkPlot *line = m_chartXY->AddPlot(vtkChart::LINE);
        line->SetInputData(curve, 0, 1);
        line->SetColor(40, 90, 160, 255);
        line->SetWidth(1.5);
        line->SetLabel("Curve");

        // Reference curves (from PlotTracer): dashed, named, never analysed.
        const unsigned char refColour[3][3] = {{120, 60, 160}, {30, 130, 110}, {160, 110, 20}};
        for (std::size_t k = 0; k < m_session.references.size(); ++k) {
            const tf::core::ReferenceCurve &r = m_session.references[k];
            vtkNew<vtkTable> rt;
            vtkNew<vtkDoubleArray> rx, ry;
            rx->SetName("x");
            const std::string label = (r.series + tr(" (reference)")).toStdString();
            ry->SetName(label.c_str());
            for (std::size_t i = 0; i < r.strain.size(); ++i) {
                rx->InsertNextValue(r.strain[i] * 100.0);
                ry->InsertNextValue(r.stress[i] * 1e-6);
            }
            rt->AddColumn(rx);
            rt->AddColumn(ry);
            vtkPlot *rp = m_chartXY->AddPlot(vtkChart::LINE);
            rp->SetInputData(rt, 0, 1);
            const auto &c = refColour[k % 3];
            rp->SetColor(c[0], c[1], c[2], 255);
            rp->SetWidth(1.5);
            rp->GetPen()->SetLineType(vtkPen::DASH_DOT_LINE);
            rp->SetLabel(label);
        }

        // The modulus line, from zero stress to near the top of the curve.
        for (const auto &pr : a->profile.results) {
            if (pr.primitive != "linear-fit" && pr.primitive != "max-slope")
                continue;
            const auto it = a->result.results.find(pr.key.toStdString());
            if (it == a->result.results.end() || !it->second.determined || !it->second.fit)
                continue;
            const auto &f = *it->second.fit;
            double top = 0;
            for (vtkIdType i = 0; i < y->GetNumberOfTuples(); ++i)
                top = std::max(top, y->GetValue(i));
            vtkNew<vtkTable> el;
            vtkNew<vtkDoubleArray> ex, ey;
            ex->SetName("x");
            ey->SetName(pr.id.toUtf8().constData());
            for (double s : {0.0, 0.9 * top}) {
                ex->InsertNextValue((s * 1e6 - f.intercept) / f.slope * 100.0);
                ey->InsertNextValue(s);
            }
            el->AddColumn(ex);
            el->AddColumn(ey);
            vtkPlot *p = m_chartXY->AddPlot(vtkChart::LINE);
            p->SetInputData(el, 0, 1);
            p->SetColor(150, 150, 150, 255);
            p->GetPen()->SetLineType(vtkPen::DASH_LINE);
            p->SetLabel(QString("Elastic line (%1)").arg(pr.id).toStdString());
            break;
        }

        // Each stress result where the engine found it on the curve.
        int markerIndex = 0;
        for (const auto &pr : a->profile.results) {
            const auto it = a->result.results.find(pr.key.toStdString());
            if (it == a->result.results.end() || !it->second.determined || it->second.unit != "Pa"
                || !it->second.point || it->second.fit)
                continue;
            vtkNew<vtkTable> pt;
            vtkNew<vtkDoubleArray> px, py;
            px->SetName("x");
            py->SetName(pr.id.toUtf8().constData());
            px->InsertNextValue(it->second.point->x * 100.0);  // already a strain
            py->InsertNextValue(it->second.point->y * 1e-6);
            pt->AddColumn(px);
            pt->AddColumn(py);
            auto *p = vtkPlotPoints::SafeDownCast(m_chartXY->AddPlot(vtkChart::POINTS));
            p->SetInputData(pt, 0, 1);
            // Fixed colours, in result order: a figure's colours must not
            // depend on what else is drawn (a reference curve shifted VTK's
            // automatic cycle).
            static const unsigned char marker[6][3] = {{230, 120, 0},   {160, 70, 40},
                                                       {200, 40, 40},   {40, 120, 200},
                                                       {110, 70, 170},  {60, 140, 60}};
            const auto &mc = marker[markerIndex++ % 6];
            p->SetColor(mc[0], mc[1], mc[2], 255);
            p->SetMarkerStyle(vtkPlotPoints::CIRCLE);
            p->SetMarkerSize(9);
            p->SetLabel(pr.id.toStdString());
        }
    }
    // Labels at the results, the user's notes, and the modulus window's
    // edges, drawn over the plots by the chart item.
    std::vector<AnnotatedChart::Text> labels, points;
    std::vector<AnnotatedChart::Span> spans;
    std::optional<std::pair<double, double>> window;
    if (a) {
        for (const tf::core::PointLabel &l : tf::core::resultLabels(*a))
            labels.push_back({l.strain * 100.0, l.stress * 1e-6, l.text.toStdString()});
        for (const tf::core::CurveNote &n : m_session.curveNotes) {
            if (n.to) {
                spans.push_back({n.from * 100.0, *n.to * 100.0, n.text.toStdString()});
            } else if (const auto y = tf::core::stressAtStrain(*a, n.from)) {
                points.push_back({n.from * 100.0, *y * 1e-6, n.text.toStdString()});
            }
        }
        for (const tf::core::Region &r : tf::core::analysisRegions(*a))
            if (r.id == "modulus-fit")
                window = std::make_pair(r.from * 100.0, r.to * 100.0);
        // A window the user set is shown as set, not as the samples in it.
        if (m_session.modulusWindow)
            window = std::make_pair(m_session.modulusWindow->first * 100.0,
                                    m_session.modulusWindow->second * 100.0);
    }
    m_chartXY->setLabels(labels);
    m_chartXY->setNotes(points, spans);
    m_chartXY->setModulusWindow(window);
    m_chartXY->RecalculateBounds();
    if (m_chart->isVisible())
        m_chart->renderWindow()->Render();
}

QByteArray MainWindow::chartSvg(QString *error) const
{
    return tf::gui::chartSvg(m_chart->renderWindow(), error);
}

// --- Notes on the curve and the modulus window ----------------------------------

void MainWindow::showNotes()
{
    const QSignalBlocker block(m_notesTable);
    const auto &notes = m_session.curveNotes;
    m_notesTable->setRowCount(int(notes.size()));
    for (int i = 0; i < int(notes.size()); ++i) {
        m_notesTable->setItem(i, 0, new QTableWidgetItem(QString::number(notes[i].from * 100.0, 'g', 6)));
        m_notesTable->setItem(i, 1, new QTableWidgetItem(
                                        notes[i].to ? QString::number(*notes[i].to * 100.0, 'g', 6)
                                                    : QString()));
        m_notesTable->setItem(i, 2, new QTableWidgetItem(notes[i].text));
    }
}

void MainWindow::readNotes()
{
    std::vector<tf::core::CurveNote> notes;
    for (int i = 0; i < m_notesTable->rowCount(); ++i) {
        auto text = [&](int c) { return m_notesTable->item(i, c) ? m_notesTable->item(i, c)->text().trimmed() : QString(); };
        bool ok = false;
        const double from = QString(text(0)).replace(',', '.').toDouble(&ok) / 100.0;
        if (!ok)
            continue;  // a row being typed; kept in the table, not yet drawn
        tf::core::CurveNote n{from, std::nullopt, text(2)};
        bool ok2 = false;
        const double to = QString(text(1)).replace(',', '.').toDouble(&ok2) / 100.0;
        if (ok2 && to > from)
            n.to = to;
        notes.push_back(n);
    }
    m_session.curveNotes = notes;
    showChart();
}

void MainWindow::addNote(bool span)
{
    const auto &a = m_session.analysis();
    if (!a)
        return;
    // Start the note in the middle of the curve; the user moves it by
    // typing the strain.
    double lo = 0, hi = 0;
    for (double e : a->strain)
        if (std::isfinite(e))
            hi = std::max(hi, e);
    const double mid = (lo + hi) / 2.0;
    m_session.curveNotes.push_back(span ? tf::core::CurveNote{mid - hi / 10.0, mid + hi / 10.0, tr("span")}
                                        : tf::core::CurveNote{mid, std::nullopt, tr("note")});
    showNotes();
    showChart();
    m_tabs->setCurrentIndex(1);
    m_notesTable->setCurrentCell(int(m_session.curveNotes.size()) - 1, 2);
    m_notesTable->editItem(m_notesTable->currentItem());
}

void MainWindow::showWindowFields()
{
    const auto &w = m_session.modulusWindow;
    m_windowFrom->setText(w ? QString::number(w->first * 100.0, 'g', 6) : QString());
    m_windowTo->setText(w ? QString::number(w->second * 100.0, 'g', 6) : QString());
    // Empty fields: the procedure's own window is used.
    m_windowReset->setEnabled(w.has_value());
}

void MainWindow::readWindowFields()
{
    const auto from = parsed(m_windowFrom, 0.01), to = parsed(m_windowTo, 0.01);
    if (from && to)
        m_session.modulusWindow = std::make_pair(*from, *to);
    else if (m_windowFrom->text().trimmed().isEmpty() && m_windowTo->text().trimmed().isEmpty())
        m_session.modulusWindow.reset();
    else
        return;  // half typed
    m_windowReset->setEnabled(m_session.modulusWindow.has_value());
    refresh();
}

// --- Actions --------------------------------------------------------------------

void MainWindow::openData()
{
    const QString path = chooseFile(false, tr("Open a machine export"),
                                    tr("Machine exports (*.csv *.txt *.tsv *.dat);;All files (*)"), {});
    if (path.isEmpty())
        return;
    const QString err = m_session.openData(path);
    if (!err.isEmpty()) {
        m_notices->setText(tr("%1 could not be opened: %2").arg(QFileInfo(path).fileName(), err));
        m_notices->setVisible(true);
        return;
    }
    showSession();
    refresh();
}

void MainWindow::openRecord()
{
    const QString path = chooseFile(false, tr("Open a TenForge record"),
                                    tr("TenForge records (manifest.tfrec)"), {});
    if (path.isEmpty())
        return;
    const QString err = m_session.openRecord(path);
    if (!err.isEmpty()) {
        m_notices->setText(tr("The record could not be opened: %1").arg(err));
        m_notices->setVisible(true);
        return;
    }
    showSession();
    refresh();
}

void MainWindow::saveRecord()
{
    if (!m_session.analysis())
        return;
    const QString suggested = m_session.specimen.id.isEmpty() ? QStringLiteral("record")
                                                              : m_session.specimen.id;
    const QString dir = chooseFile(true, tr("Save the test record as a new folder"),
                                   tr("TenForge record folder (*)"), suggested);
    if (dir.isEmpty())
        return;
    const QString err = m_session.saveRecord(dir);
    if (!err.isEmpty()) {
        m_notices->setText(tr("The record was not saved: %1").arg(err));
        m_notices->setVisible(true);
        return;
    }
    statusBar()->showMessage(tr("Record saved in %1").arg(dir));
}

void MainWindow::exportReport()
{
    const auto &a = m_session.analysis();
    if (!a)
        return;
    const QString suggested = (m_session.specimen.id.isEmpty() ? QStringLiteral("report")
                                                               : m_session.specimen.id)
                              + ".pdf";
    QString path = chooseFile(true, tr("Export the report"), tr("PDF (*.pdf);;HTML (*.html)"),
                              suggested);
    if (path.isEmpty())
        return;
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix != "pdf" && suffix != "html" && suffix != "htm")
        path += ".pdf";

    QString error;
    if (path.endsWith(".pdf", Qt::CaseInsensitive)) {
        // The chart is drawn from its SVG, so it stays vector in the PDF.
        QTextDocument doc;
        const QByteArray svg = chartSvg();
        if (!svg.isEmpty())
            tf::gui::drawImageAsSvg(&doc, QStringLiteral("chart.svg"), svg);
        doc.setHtml(tf::core::reportHtml(a->doc, m_session.specimen,
                                         svg.isEmpty() ? QString() : QStringLiteral("chart.svg"),
                                         m_session.fixture));
        QPdfWriter pdf(path);
        pdf.setPageSize(QPageSize(QPageSize::A4));
        pdf.setTitle(tr("%1 test %2")
                         .arg(a->profile.mode == "flexure"       ? tr("Flexural")
                              : a->profile.mode == "compression" ? tr("Compression")
                                                                 : tr("Tensile"),
                              m_session.specimen.id));
        pdf.setCreator(tf::core::applicationName() + " " + tf::core::applicationVersion());
        doc.print(&pdf);
        if (!QFileInfo(path).exists() || QFileInfo(path).size() == 0)
            error = tr("the PDF could not be written");
    } else {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            const QByteArray svg = chartSvg();
            const QString src = svg.isEmpty() ? QString()
                                              : "data:image/svg+xml;base64,"
                                                    + QString::fromLatin1(svg.toBase64());
            f.write(tf::core::reportHtml(a->doc, m_session.specimen, src, m_session.fixture)
                        .toUtf8());
        }
        else
            error = f.errorString();
    }
    if (!error.isEmpty()) {
        m_notices->setText(tr("The report was not written: %1").arg(error));
        m_notices->setVisible(true);
        return;
    }
    statusBar()->showMessage(tr("Report written to %1").arg(path));
}

void MainWindow::exportChart()
{
    if (!m_session.analysis())
        return;
    const QString base = m_session.specimen.id.isEmpty() ? QStringLiteral("chart")
                                                         : m_session.specimen.id;
    QString path = chooseFile(true, tr("Export the chart"),
                              tr("SVG, vector (*.svg);;PDF, vector (*.pdf);;PNG (*.png)"),
                              base + ".svg");
    if (path.isEmpty())
        return;
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix != "svg" && suffix != "pdf" && suffix != "png")
        path += ".svg";
    QString error;
    const QByteArray svg = chartSvg(&error);
    if (error.isEmpty())
        error = tf::gui::writeChart(svg, path);
    if (!error.isEmpty()) {
        m_notices->setText(tr("The chart was not exported: %1").arg(error));
        m_notices->setVisible(true);
        return;
    }
    statusBar()->showMessage(tr("Chart written to %1").arg(path));
}

void MainWindow::exportFreecad()
{
    const auto &a = m_session.analysis();
    if (!a)
        return;
    // Bending and the rest are refused with the reason, on screen.
    QString error;
    const QString card = tf::core::freecadCard(*a, m_session.specimen, &error);
    const tf::core::Hardening h = tf::core::hardeningCurve(*a);
    if (card.isEmpty()) {
        m_notices->setText(tr("Nothing to export for FreeCAD: %1.").arg(error));
        m_notices->setVisible(true);
        return;
    }
    const QString base = m_session.specimen.material.isEmpty()
                             ? (m_session.specimen.id.isEmpty() ? QStringLiteral("material")
                                                                : m_session.specimen.id)
                             : m_session.specimen.material;
    QString path = chooseFile(true, tr("Export for FreeCAD and CalculiX"),
                              tr("FreeCAD material card (*.FCMat)"), base + ".FCMat");
    if (path.isEmpty())
        return;
    if (path.endsWith(".FCMat", Qt::CaseInsensitive))
        path.chop(6);
    QStringList written, problems;
    auto write = [&](const QString &p, const QByteArray &bytes) {
        QFile f(p);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(bytes) == bytes.size())
            written << QFileInfo(p).fileName();
        else
            problems << tr("cannot write %1").arg(p);
    };
    write(path + ".FCMat", card.toUtf8());
    if (h.error.isEmpty()) {
        write(path + "-yieldpoints.txt", (tf::core::yieldPoints(h).join('\n') + '\n').toUtf8());
        write(path + "-plastic.inp", tf::core::calculixPlastic(h, *a).toUtf8());
    } else {
        problems << tr("no hardening curve: %1").arg(h.error);
    }
    if (!problems.isEmpty()) {
        m_notices->setText(tr("Written: %1. Not written: %2.")
                               .arg(written.isEmpty() ? tr("nothing") : written.join(", "),
                                    problems.join("; ")));
        m_notices->setVisible(true);
    }
    statusBar()->showMessage(tr("Written for FreeCAD and CalculiX: %1").arg(written.join(", ")));
}

void MainWindow::exportVtp()
{
    const auto &a = m_session.analysis();
    if (!a)
        return;
    const QString base = m_session.specimen.id.isEmpty() ? QStringLiteral("curve")
                                                         : m_session.specimen.id;
    QString path = chooseFile(true, tr("Export the curve for ParaView"),
                              tr("VTK PolyData (*.vtp)"), base + ".vtp");
    if (path.isEmpty())
        return;
    if (!path.endsWith(".vtp", Qt::CaseInsensitive))
        path += ".vtp";
    QFile f(path);
    const QByteArray bytes = tf::core::curveVtp(*a).toUtf8();
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(bytes) != bytes.size()) {
        m_notices->setText(tr("The curve was not exported: cannot write %1").arg(path));
        m_notices->setVisible(true);
        return;
    }
    statusBar()->showMessage(tr("Curve written to %1").arg(path));
}

void MainWindow::exportPmd()
{
    const auto &a = m_session.analysis();
    if (!a)
        return;
    QString error;
    const QString ld = tf::core::pmdJsonLd(*a, m_session.specimen, &error);
    if (ld.isEmpty()) {
        m_notices->setText(tr("Nothing to export for Platform MaterialDigital: %1.").arg(error));
        m_notices->setVisible(true);
        return;
    }
    const QString base = m_session.specimen.id.isEmpty() ? QStringLiteral("test")
                                                         : m_session.specimen.id;
    QString path = chooseFile(true, tr("Export for Platform MaterialDigital"),
                              tr("JSON-LD (*.jsonld)"), base + ".jsonld");
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(ld.toUtf8()) != ld.toUtf8().size()) {
        m_notices->setText(tr("The JSON-LD was not written: cannot write %1").arg(path));
        m_notices->setVisible(true);
        return;
    }
    statusBar()->showMessage(tr("JSON-LD written to %1").arg(path));
}

void MainWindow::addReference()
{
    const QString path = chooseFile(false, tr("Add a reference curve"),
                                    tr("PlotTracer JSON export (*.json);;All files (*)"), {});
    if (path.isEmpty())
        return;
    QString error;
    const auto file = tf::core::readPlotTracer(path, &error);
    if (!file) {
        m_notices->setText(tr("%1 cannot be used: %2").arg(QFileInfo(path).fileName(), error));
        m_notices->setVisible(true);
        return;
    }
    // Which curve, which columns are strain and stress, in which units:
    // PlotTracer records labels but no units, so the user says; TenForge's
    // guess from the labels is filled in.
    QDialog dlg(this);
    dlg.setObjectName(QStringLiteral("referenceDialog"));
    dlg.setWindowTitle(tr("Reference curve from %1").arg(QFileInfo(path).fileName()));
    auto *form = new QFormLayout(&dlg);
    auto *series = new QComboBox;
    auto *strainCol = new QComboBox, *stressCol = new QComboBox;
    auto *strainUnit = new QComboBox, *stressUnit = new QComboBox;
    for (const auto &s : file->series)
        if (s.relation.isEmpty())
            series->addItem(s.name);
    strainUnit->addItems(tf::core::referenceStrainUnits());
    stressUnit->addItems(tf::core::referenceStressUnits());
    form->addRow(tr("Curve"), series);
    form->addRow(tr("Strain column"), strainCol);
    form->addRow(tr("Strain unit"), strainUnit);
    form->addRow(tr("Stress column"), stressCol);
    form->addRow(tr("Stress unit"), stressUnit);
    auto fill = [&] {
        for (const auto &s : file->series) {
            if (s.name != series->currentText())
                continue;
            for (QComboBox *c : {strainCol, stressCol}) {
                c->clear();
                c->addItems(s.columns);
            }
            const auto g = tf::core::guessReferenceColumns(s);
            if (!g.strainColumn.isEmpty())
                strainCol->setCurrentText(g.strainColumn);
            if (!g.stressColumn.isEmpty())
                stressCol->setCurrentText(g.stressColumn);
            if (!g.strainUnit.isEmpty())
                strainUnit->setCurrentText(g.strainUnit);
            if (!g.stressUnit.isEmpty())
                stressUnit->setCurrentText(g.stressUnit);
        }
    };
    fill();
    connect(series, &QComboBox::currentTextChanged, &dlg, fill);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);
    if (series->count() == 0) {
        m_notices->setText(tr("%1 holds no curve, only error bars.").arg(QFileInfo(path).fileName()));
        m_notices->setVisible(true);
        return;
    }
    if (dlg.exec() != QDialog::Accepted)
        return;
    error = m_session.addReferenceCurve(path, series->currentText(), strainCol->currentText(),
                                        strainUnit->currentText(), stressCol->currentText(),
                                        stressUnit->currentText());
    if (!error.isEmpty()) {
        m_notices->setText(tr("The reference curve was not added: %1").arg(error));
        m_notices->setVisible(true);
        return;
    }
    refresh();
}

void MainWindow::chooseCompliance()
{
    const QString path = chooseFile(false, tr("Choose the machine's compliance table"),
                                    tr("Tables (*.csv *.txt *.tsv *.dat);;All files (*)"), {});
    if (path.isEmpty())
        return;
    const QString err = m_session.setCompliance(path);
    if (!err.isEmpty()) {
        m_notices->setText(tr("%1 cannot be used: %2").arg(QFileInfo(path).fileName(), err));
        m_notices->setVisible(true);
        return;
    }
    showSession();
    refresh();
}

void MainWindow::addUnlisted()
{
    const QString dir = m_session.recordDir();
    const QString err = tf::core::addUnlistedFiles(dir);
    if (err.isEmpty())
        m_session.openRecord(dir);
    showSession();
    refresh();
    if (!err.isEmpty()) {
        m_notices->setText(tr("The manifest was not changed: %1").arg(err));
        m_notices->setVisible(true);
    }
}

} // namespace tf::gui
