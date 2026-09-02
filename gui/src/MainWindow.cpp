#include "MainWindow.h"

#include "PreviewWidget.h"
#include "ReliefPreviewWidget.h"

#include <patternfab/ConstraintEngine.h>
#include <patternfab/Core.h>
#include <patternfab/DxfExport.h>
#include <patternfab/PngExport.h>
#include <patternfab/StlExport.h>
#include <patternfab/SvgExport.h>
#include <patternfab/VectorInput.h>

#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include <exception>

namespace {

QDoubleSpinBox *makeSpin(double min, double max, double step, int decimals, double value,
                         const QString &suffix) {
    auto *spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    spin->setValue(value);
    if (!suffix.isEmpty()) {
        spin->setSuffix(suffix);
    }
    return spin;
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QString("PatternFab %1").arg(QString::fromStdString(patternfab::coreVersion())));
    previewSvgPath_ = QDir(QDir::tempPath()).filePath("patternfab_preview.svg");
    previewStlPath_ = QDir(QDir::tempPath()).filePath("patternfab_preview.stl");
    buildUi();
    setExportsEnabled(false);
    resize(1120, 740);
}

void MainWindow::buildUi() {
    // --- Menu ---------------------------------------------------------
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Open pattern…"), QKeySequence::Open, this, &MainWindow::openPattern);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    // --- Left control column -----------------------------------------
    auto *controls = new QWidget;
    auto *col = new QVBoxLayout(controls);
    col->setContentsMargins(12, 12, 12, 12);
    col->setSpacing(8);

    // Keep the control stack compact so all export actions sit above the fold
    // rather than behind a scrollbar.
    const auto tighten = [](QFormLayout *form) {
        form->setContentsMargins(10, 8, 10, 8);
        form->setVerticalSpacing(6);
        form->setHorizontalSpacing(10);
    };

    auto *openButton = new QPushButton(tr("Open pattern…"));
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openPattern);
    col->addWidget(openButton);

    // Loaded pattern summary.
    auto *patternBox = new QGroupBox(tr("Pattern"));
    auto *patternForm = new QFormLayout(patternBox);
    tighten(patternForm);
    fileLabel_ = new QLabel(tr("(none loaded)"));
    fileLabel_->setWordWrap(true);
    specimenLabel_ = new QLabel("—");
    resolutionLabel_ = new QLabel("—");
    speckleLabel_ = new QLabel("—");
    countLabel_ = new QLabel("—");
    patternForm->addRow(tr("File:"), fileLabel_);
    patternForm->addRow(tr("Specimen:"), specimenLabel_);
    patternForm->addRow(tr("Imaging res:"), resolutionLabel_);
    patternForm->addRow(tr("Target speckle:"), speckleLabel_);
    patternForm->addRow(tr("Speckles:"), countLabel_);
    col->addWidget(patternBox);

    // Manufacturing constraints (drive the report and export bleed comp).
    auto *constraintBox = new QGroupBox(tr("Manufacturing constraints"));
    auto *constraintForm = new QFormLayout(constraintBox);
    tighten(constraintForm);
    minFeatureSpin_ = makeSpin(0.0, 10.0, 0.01, 3, 0.15, tr(" mm"));
    bleedSpin_ = makeSpin(0.0, 5.0, 0.01, 3, 0.03, tr(" mm"));
    bridgeSpin_ = makeSpin(0.0, 10.0, 0.01, 3, 0.20, tr(" mm"));
    constraintForm->addRow(tr("Min feature size:"), minFeatureSpin_);
    constraintForm->addRow(tr("Bleed compensation:"), bleedSpin_);
    constraintForm->addRow(tr("Min bridge width:"), bridgeSpin_);
    col->addWidget(constraintBox);
    for (auto *spin : {minFeatureSpin_, bleedSpin_, bridgeSpin_}) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, &MainWindow::refreshReport);
    }

    // Constraint report.
    auto *reportBox = new QGroupBox(tr("Constraint report"));
    auto *reportLayout = new QVBoxLayout(reportBox);
    reportView_ = new QPlainTextEdit;
    reportView_->setReadOnly(true);
    // Cap the height so the empty report doesn't devour vertical space and
    // push the export actions below the fold; it scrolls internally if the
    // violation list runs long.
    reportView_->setMinimumHeight(84);
    reportView_->setMaximumHeight(124);
    reportLayout->addWidget(reportView_);
    col->addWidget(reportBox);

    // Export.
    auto *exportBox = new QGroupBox(tr("Export"));
    auto *exportLayout = new QVBoxLayout(exportBox);

    svgButton_ = new QPushButton(tr("Export SVG… (stencil)"));
    dxfButton_ = new QPushButton(tr("Export DXF… (stencil)"));
    connect(svgButton_, &QPushButton::clicked, this, &MainWindow::exportSvg);
    connect(dxfButton_, &QPushButton::clicked, this, &MainWindow::exportDxf);
    exportLayout->addWidget(svgButton_);
    exportLayout->addWidget(dxfButton_);

    auto *pngRow = new QHBoxLayout;
    pngButton_ = new QPushButton(tr("Export PNG… (direct-write)"));
    connect(pngButton_, &QPushButton::clicked, this, &MainWindow::exportPng);
    dpiSpin_ = makeSpin(50.0, 4800.0, 50.0, 0, 600.0, tr(" dpi"));
    pngRow->addWidget(pngButton_, 1);
    pngRow->addWidget(dpiSpin_);
    exportLayout->addLayout(pngRow);

    stlButton_ = new QPushButton(tr("Export STL… (stamp/mold)"));
    connect(stlButton_, &QPushButton::clicked, this, &MainWindow::exportStl);
    exportLayout->addWidget(stlButton_);
    baseThicknessSpin_ = makeSpin(0.1, 50.0, 0.1, 2, 2.0, tr(" mm"));
    baseThicknessSpin_->setToolTip(tr("Relief base-plate thickness"));
    bumpHeightSpin_ = makeSpin(0.05, 20.0, 0.05, 2, 0.8, tr(" mm"));
    bumpHeightSpin_->setToolTip(tr("Raised bump height at each speckle"));
    meshResSpin_ = makeSpin(1.0, 50.0, 1.0, 1, 8.0, tr(" /mm"));
    meshResSpin_->setToolTip(tr("Heightfield mesh resolution (samples per mm)"));
    auto *reliefForm = new QFormLayout;
    tighten(reliefForm);
    reliefForm->addRow(tr("Base thickness:"), baseThicknessSpin_);
    reliefForm->addRow(tr("Bump height:"), bumpHeightSpin_);
    reliefForm->addRow(tr("Mesh resolution:"), meshResSpin_);
    exportLayout->addLayout(reliefForm);
    col->addWidget(exportBox);

    col->addStretch(1);

    auto *scroll = new QScrollArea;
    scroll->setWidget(controls);
    scroll->setWidgetResizable(true);
    scroll->setMinimumWidth(380);
    scroll->setMaximumWidth(460);
    // The control column may scroll vertically on a short window (a visible,
    // expected affordance); it must never scroll horizontally (sideways-hidden
    // content is easy to miss entirely).
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // --- Right preview (tabbed 2D / 3D) ------------------------------
    preview_ = new PreviewWidget;
    relief3d_ = new ReliefPreviewWidget;
    previewTabs_ = new QTabWidget;
    previewTabs_->addTab(preview_, tr("2D (stencil / direct-write)"));
    previewTabs_->addTab(relief3d_, tr("3D relief (stamp / mold)"));
    connect(previewTabs_, &QTabWidget::currentChanged, this, &MainWindow::onPreviewTabChanged);

    // The relief preview depends on the STL relief parameters; rebuild it when
    // they change, but only while its tab is visible.
    for (auto *spin : {baseThicknessSpin_, bumpHeightSpin_, meshResSpin_}) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this]() {
            relief3dDirty_ = true;
            if (previewTabs_->currentWidget() == relief3d_) {
                refreshReliefPreview();
            }
        });
    }

    auto *central = new QWidget;
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);
    root->addWidget(previewTabs_, 1);
    setCentralWidget(central);

    statusBar()->showMessage(tr("Open a PatternFab pattern to begin."));
}

void MainWindow::setExportsEnabled(bool enabled) {
    svgButton_->setEnabled(enabled);
    dxfButton_->setEnabled(enabled);
    pngButton_->setEnabled(enabled);
    stlButton_->setEnabled(enabled);
}

patternfab::ManufacturingConstraints MainWindow::currentConstraints() const {
    patternfab::ManufacturingConstraints c;
    c.minFeatureSizeMm = minFeatureSpin_->value();
    c.bleedCompensationMm = bleedSpin_->value();
    c.minBridgeWidthMm = bridgeSpin_->value();
    return c;
}

void MainWindow::openPattern() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open pattern"), QString(), tr("PatternFab pattern (*.json)"));
    if (path.isEmpty()) {
        return;
    }
    loadPattern(path);
}

// Separated from the dialog so that a path can also arrive from the command
// line, which is how a file manager, a terminal and a screenshot script all
// open a pattern. The dialog is one caller of this, not the only door in.
void MainWindow::loadPattern(const QString &path) {
    try {
        patternfab::Pattern loaded = patternfab::loadPatternFromVectorFile(path.toStdString());
        pattern_ = std::move(loaded);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Could not open pattern"), QString::fromUtf8(e.what()));
        return;
    }

    const auto &p = pattern_->params;
    fileLabel_->setText(QFileInfo(path).fileName());
    specimenLabel_->setText(tr("%1 × %2 mm").arg(p.specimenWidthMm).arg(p.specimenHeightMm));
    resolutionLabel_->setText(tr("%1 px/mm").arg(p.imagingResolutionPxPerMm));
    speckleLabel_->setText(tr("%1 mm").arg(p.targetSpeckleSizeMm));
    countLabel_->setText(QString::number(pattern_->primitives.size()));

    setExportsEnabled(true);
    refreshReport();
    refreshPreview();
    statusBar()->showMessage(tr("Loaded %1").arg(QFileInfo(path).fileName()), 5000);
}

void MainWindow::refreshReport() {
    if (!pattern_) {
        return;
    }
    const patternfab::ConstraintReport report =
        patternfab::evaluateConstraints(*pattern_, currentConstraints());

    QString text;
    if (report.patternRequiresTiling) {
        text += tr("⚠ Requires tiling — periodicity risk. The pattern does not "
                   "cover the specimen in one application; repeating it injects "
                   "periodicity that aliases DIC correlation.\n\n");
    } else {
        text += tr("✓ Covers the specimen in a single application (no tiling "
                   "needed).\n\n");
    }

    text += tr("Minimum-feature violations: %1\n").arg(report.minimumFeatureSizeViolations.size());
    for (const auto &v : report.minimumFeatureSizeViolations) {
        text += tr("  • #%1: %2\n").arg(v.primitiveIndex).arg(QString::fromStdString(v.message));
    }
    text += tr("\nBridging violations: %1\n").arg(report.bridgingViolations.size());
    for (const auto &v : report.bridgingViolations) {
        text += tr("  • #%1–#%2: %3\n")
                    .arg(v.primitiveIndexA)
                    .arg(v.primitiveIndexB)
                    .arg(QString::fromStdString(v.message));
    }
    reportView_->setPlainText(text);
}

void MainWindow::refreshPreview() {
    if (!pattern_) {
        preview_->clear();
        relief3d_->clear();
        return;
    }
    // Preview the raw design geometry (bleed compensation is a sub-visible
    // fabrication offset applied at export time, not a design change).
    try {
        patternfab::exportPatternToSvg(*pattern_, previewSvgPath_.toStdString());
        preview_->showSvg(previewSvgPath_);
    } catch (const std::exception &e) {
        preview_->clear();
        statusBar()->showMessage(tr("Preview failed: %1").arg(QString::fromUtf8(e.what())), 5000);
    }

    // The 3D relief is comparatively expensive to mesh; rebuild it now only if
    // its tab is showing, otherwise defer until the user switches to it.
    relief3dDirty_ = true;
    if (previewTabs_->currentWidget() == relief3d_) {
        refreshReliefPreview();
    }
}

void MainWindow::refreshReliefPreview() {
    if (!pattern_) {
        relief3d_->clear();
        return;
    }
    try {
        patternfab::exportPatternToStl(*pattern_, previewStlPath_.toStdString(), currentRelief());
        relief3d_->showStl(previewStlPath_);
        relief3dDirty_ = false;
    } catch (const std::exception &e) {
        relief3d_->clear();
        statusBar()->showMessage(tr("3D preview failed: %1").arg(QString::fromUtf8(e.what())), 5000);
    }
}

void MainWindow::onPreviewTabChanged() {
    if (pattern_ && relief3dDirty_ && previewTabs_->currentWidget() == relief3d_) {
        refreshReliefPreview();
    }
}

patternfab::ReliefParameters MainWindow::currentRelief() const {
    patternfab::ReliefParameters relief;
    relief.baseThicknessMm = baseThicknessSpin_->value();
    relief.bumpHeightMm = bumpHeightSpin_->value();
    relief.meshResolutionPerMm = meshResSpin_->value();
    return relief;
}

patternfab::Pattern MainWindow::fabricationGeometry() const {
    return patternfab::applyBleedCompensation(*pattern_, currentConstraints());
}

void MainWindow::runExport(const QString &title, const QString &filter, const QString &suffix,
                           const std::function<void(const patternfab::Pattern &, const std::string &)> &write) {
    if (!pattern_) {
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, title, QString(), filter);
    if (path.isEmpty()) {
        return;
    }
    if (!suffix.isEmpty() && !path.endsWith(suffix, Qt::CaseInsensitive)) {
        path += suffix;
    }
    try {
        const patternfab::Pattern geo = fabricationGeometry();
        write(geo, path.toStdString());
        statusBar()->showMessage(tr("Exported %1").arg(QFileInfo(path).fileName()), 5000);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Export failed"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::exportSvg() {
    runExport(tr("Export SVG"), tr("SVG (*.svg)"), ".svg",
              [](const patternfab::Pattern &p, const std::string &path) {
                  patternfab::exportPatternToSvg(p, path);
              });
}

void MainWindow::exportDxf() {
    runExport(tr("Export DXF"), tr("DXF (*.dxf)"), ".dxf",
              [](const patternfab::Pattern &p, const std::string &path) {
                  patternfab::exportPatternToDxf(p, path);
              });
}

void MainWindow::exportPng() {
    const double dpi = dpiSpin_->value();
    runExport(tr("Export PNG"), tr("PNG (*.png)"), ".png",
              [dpi](const patternfab::Pattern &p, const std::string &path) {
                  patternfab::exportPatternToPng(p, path, dpi);
              });
}

void MainWindow::exportStl() {
    const patternfab::ReliefParameters relief = currentRelief();
    runExport(tr("Export STL"), tr("STL (*.stl)"), ".stl",
              [relief](const patternfab::Pattern &p, const std::string &path) {
                  patternfab::exportPatternToStl(p, path, relief);
              });
}
