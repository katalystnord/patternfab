#pragma once

#include <patternfab/ConstraintEngine.h>
#include <patternfab/Pattern.h>
#include <patternfab/StlExport.h>

#include <QMainWindow>

#include <functional>
#include <optional>
#include <string>

class PreviewWidget;
class ReliefPreviewWidget;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Load a pattern from a path, reporting any failure in a dialog. Public
    // because main() calls it for a path given on the command line.
    void loadPattern(const QString &path);

private slots:
    void openPattern();
    void exportSvg();
    void exportDxf();
    void exportPng();
    void exportStl();

private:
    void buildUi();
    // Recompute + display the constraint report for the loaded pattern under
    // the current manufacturing constraints. Called on load and whenever a
    // constraint spin box changes.
    void refreshReport();

    // What the pattern can MEASURE, as opposed to whether it can be made.
    // Its own panel rather than more text in the constraint report: they
    // answer different questions, and a reader after one should not have to
    // scroll past the other to reach it.
    void refreshMeasurement();
    // Regenerate the 2D preview SVG (raw design geometry) and hand it to the
    // view; marks the 3D relief preview stale (rebuilt lazily when shown).
    void refreshPreview();
    // Rebuild the 3D relief preview STL from the current relief parameters.
    // Meshing is non-trivial, so this runs only when the 3D tab is actually
    // visible (or on demand), guarded by relief3dDirty_.
    void refreshReliefPreview();
    void onPreviewTabChanged();
    void setExportsEnabled(bool enabled);

    patternfab::ManufacturingConstraints currentConstraints() const;
    patternfab::ReliefParameters currentRelief() const;
    // Bleed-compensated fabrication geometry for export. May throw (e.g. a
    // polygon under nonzero bleed, which core refuses rather than approximate).
    patternfab::Pattern fabricationGeometry() const;
    // Shared export wrapper: prompts for a path, applies bleed compensation,
    // runs `write`, and reports success/failure to the user. `write` receives
    // the geometry and the chosen output path.
    void runExport(const QString &title, const QString &filter, const QString &suffix,
                   const std::function<void(const patternfab::Pattern &, const std::string &)> &write);

    std::optional<patternfab::Pattern> pattern_;
    QString previewSvgPath_;
    QString previewStlPath_;
    bool relief3dDirty_ = true;

    QTabWidget *previewTabs_ = nullptr;
    PreviewWidget *preview_ = nullptr;
    ReliefPreviewWidget *relief3d_ = nullptr;

    QLabel *fileLabel_ = nullptr;
    QLabel *specimenLabel_ = nullptr;
    QLabel *resolutionLabel_ = nullptr;
    QLabel *speckleLabel_ = nullptr;
    QLabel *countLabel_ = nullptr;

    QDoubleSpinBox *minFeatureSpin_ = nullptr;
    QDoubleSpinBox *bleedSpin_ = nullptr;
    QDoubleSpinBox *bridgeSpin_ = nullptr;

    QPlainTextEdit *reportView_ = nullptr;
    QPlainTextEdit *measurementView_ = nullptr;

    QDoubleSpinBox *dpiSpin_ = nullptr;
    QDoubleSpinBox *baseThicknessSpin_ = nullptr;
    QDoubleSpinBox *bumpHeightSpin_ = nullptr;
    QDoubleSpinBox *meshResSpin_ = nullptr;

    QPushButton *svgButton_ = nullptr;
    QPushButton *dxfButton_ = nullptr;
    QPushButton *pngButton_ = nullptr;
    QPushButton *stlButton_ = nullptr;
};
