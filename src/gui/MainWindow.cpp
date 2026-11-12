#include "gui/MainWindow.h"

#include "core/ApplicationInfo.h"
#include "core/Record.h"
#include "core/Report.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QComboBox>
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
#include <vtkPen.h>
#include <vtkPlot.h>
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
    split->setSizes({380, 820});
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

    // 1. Data
    auto *data = new QGroupBox(tr("1. Data"));
    auto *df = new QFormLayout(data);
    auto *openBtn = new QPushButton(tr("Open data..."));
    openBtn->setObjectName(QStringLiteral("openData"));
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::openData);
    m_file = wrapped(tr("No file open."), "fileName");
    df->addRow(openBtn);
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
    df->addRow(m_readNotes);
    m_forceUnit->addItem(tr("Choose..."), QString());
    for (const QString &u : tf::core::knownUnits("force"))
        m_forceUnit->addItem(u, u);
    m_extUnit->addItem(tr("Choose..."), QString());
    for (const QString &u : tf::core::knownUnits("length") + tf::core::knownUnits("strain"))
        m_extUnit->addItem(u, u);
    v->addWidget(data);

    // 2. Specimen
    auto *spec = new QGroupBox(tr("2. Specimen"));
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

    // One form, so every field lines up; the shape decides which rows show.
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
    showShapeRows();
    m_gauge = new QLineEdit;
    m_gauge->setObjectName(QStringLiteral("gaugeLength"));
    sf->addRow(tr("Gauge length Le (mm)"), m_gauge);
    v->addWidget(spec);

    // 3. Procedure
    auto *proc = new QGroupBox(tr("3. Procedure"));
    auto *pf = new QFormLayout(proc);
    m_profile = new QComboBox;
    m_profile->setObjectName(QStringLiteral("profile"));
    pf->addRow(tr("Procedure"), m_profile);
    v->addWidget(proc);
    v->addStretch(1);

    // Long column names and procedure titles must not widen the panel.
    for (QComboBox *c : {m_forceColumn, m_extColumn, m_profile}) {
        c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->setMinimumContentsLength(12);
    }

    // Every edit goes to the session and the outcome is redrawn.
    for (QComboBox *c : {m_forceColumn, m_forceUnit, m_extColumn, m_extUnit})
        connect(c, &QComboBox::currentIndexChanged, this, [this] { readInputs(); });
    connect(m_shape, &QComboBox::currentIndexChanged, this, [this] {
        showShapeRows();
        readInputs();
    });
    for (QLineEdit *e : {m_id, m_material, m_width, m_thickness, m_diameter, m_outer, m_inner,
                         m_area, m_gauge})
        connect(e, &QLineEdit::textChanged, this, [this] { readInputs(); });
    connect(m_profile, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_profile->itemData(i).toString() != QLatin1String(kProfileFile))
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
    scroll->setMinimumWidth(340);
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
    m_chartXY = vtkSmartPointer<vtkChartXY>::New();
    m_chartXY->GetAxis(vtkAxis::BOTTOM)->SetTitle("Strain (%)");
    m_chartXY->GetAxis(vtkAxis::LEFT)->SetTitle("Stress (MPa)");
    m_chartXY->SetShowLegend(true);
    // Bottom right: a tensile curve rises to the top left and top right,
    // the bottom right stays clear until fracture.
    m_chartXY->GetLegend()->SetHorizontalAlignment(vtkChartLegend::RIGHT);
    m_chartXY->GetLegend()->SetVerticalAlignment(vtkChartLegend::BOTTOM);
    m_view->GetScene()->AddItem(m_chartXY);
    v->addWidget(m_chart, 3);

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
    v->addWidget(m_results, 2);

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
    m_file->setText(t ? QFileInfo(t->path).fileName() + (m_session.recordDir().isEmpty()
                                                             ? QString()
                                                             : tr(" (in the record %1)")
                                                                   .arg(m_session.recordDir()))
                      : tr("No file open."));
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

    m_notices->setText(m_session.notices().join("\n"));
    m_notices->setVisible(!m_session.notices().isEmpty());
    m_addUnlisted->setVisible(!m_session.unlisted().isEmpty());
    m_updating = false;
}

void MainWindow::showShapeRows()
{
    const auto shape = Specimen::Shape(m_shape->currentIndex());
    const std::map<QLineEdit *, bool> rows{
        {m_width, shape == Specimen::Shape::Rectangular},
        {m_thickness, shape == Specimen::Shape::Rectangular},
        {m_diameter, shape == Specimen::Shape::Round},
        {m_outer, shape == Specimen::Shape::Tube},
        {m_inner, shape == Specimen::Shape::Tube},
        {m_area, shape == Specimen::Shape::GivenArea}};
    for (const auto &[edit, on] : rows)
        m_specForm->setRowVisible(edit, on);
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
        {"gaugeLength", m_gauge}, {"profile", m_profile}};
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
        const int colon = d.indexOf(": results.");
        reasons << (colon > 0 ? d.left(colon) + "." : d);
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
    const auto &a = m_session.analysis();
    if (a) {
        const auto &in = a->imported.input;
        vtkNew<vtkTable> curve;
        vtkNew<vtkDoubleArray> x, y;
        x->SetName("Strain (%)");
        y->SetName("Stress (MPa)");
        for (std::size_t i = 0; i < in.force.size(); ++i) {
            const double e = in.extension[i] / in.gaugeLength * 100.0;
            const double s = in.force[i] / in.area * 1e-6;
            if (std::isfinite(e) && std::isfinite(s)) {
                x->InsertNextValue(e);
                y->InsertNextValue(s);
            }
        }
        curve->AddColumn(x);
        curve->AddColumn(y);
        vtkPlot *line = m_chartXY->AddPlot(vtkChart::LINE);
        line->SetInputData(curve, 0, 1);
        line->SetColor(40, 90, 160, 255);
        line->SetWidth(1.5);
        line->SetLabel("Curve");

        // The elastic line of E, from zero stress to the top of its window.
        const auto eIt = a->result.results.find("E");
        if (eIt != a->result.results.end() && eIt->second.determined && eIt->second.fit) {
            const auto &f = *eIt->second.fit;
            double top = 0;
            for (vtkIdType i = 0; i < y->GetNumberOfTuples(); ++i)
                top = std::max(top, y->GetValue(i));
            vtkNew<vtkTable> el;
            vtkNew<vtkDoubleArray> ex, ey;
            ex->SetName("x");
            ey->SetName("E");
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
            p->SetLabel("Elastic line (E)");
        }

        // Each stress result where the engine found it on the curve.
        for (const auto &pr : a->profile.results) {
            const auto it = a->result.results.find(pr.quantity.toStdString());
            if (it == a->result.results.end() || !it->second.determined || it->second.unit != "Pa"
                || !it->second.point || pr.quantity == "E")
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
            p->SetMarkerStyle(vtkPlotPoints::CIRCLE);
            p->SetMarkerSize(9);
            p->SetLabel(pr.id.toStdString());
        }
    }
    m_chartXY->RecalculateBounds();
    if (m_chart->isVisible())
        m_chart->renderWindow()->Render();
}

QString MainWindow::chartPng() const
{
    const QImage img = m_chart->grabFramebuffer();
    if (img.isNull())
        return {};
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return "data:image/png;base64," + QString::fromLatin1(png.toBase64());
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
        QTextDocument doc;
        const QImage img = m_chart->grabFramebuffer();
        if (!img.isNull())
            doc.addResource(QTextDocument::ImageResource, QUrl("chart.png"), img);
        doc.setHtml(tf::core::reportHtml(a->doc, m_session.specimen,
                                         img.isNull() ? QString() : QStringLiteral("chart.png")));
        QPdfWriter pdf(path);
        pdf.setPageSize(QPageSize(QPageSize::A4));
        pdf.setTitle(tr("Tensile test %1").arg(m_session.specimen.id));
        pdf.setCreator(tf::core::applicationName() + " " + tf::core::applicationVersion());
        doc.print(&pdf);
        if (!QFileInfo(path).exists() || QFileInfo(path).size() == 0)
            error = tr("the PDF could not be written");
    } else {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(tf::core::reportHtml(a->doc, m_session.specimen, chartPng()).toUtf8());
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
