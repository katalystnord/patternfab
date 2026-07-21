#pragma once

#include <patternfab/ConstraintEngine.h>
#include <patternfab/Pattern.h>

#include <QMainWindow>

#include <functional>
#include <optional>
#include <string>

class PreviewWidget;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

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
    // Regenerate the preview SVG (raw design geometry) and hand it to the view.
    void refreshPreview();
    void setExportsEnabled(bool enabled);

    patternfab::ManufacturingConstraints currentConstraints() const;
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

    PreviewWidget *preview_ = nullptr;

    QLabel *fileLabel_ = nullptr;
    QLabel *specimenLabel_ = nullptr;
    QLabel *resolutionLabel_ = nullptr;
    QLabel *speckleLabel_ = nullptr;
    QLabel *countLabel_ = nullptr;

    QDoubleSpinBox *minFeatureSpin_ = nullptr;
    QDoubleSpinBox *bleedSpin_ = nullptr;
    QDoubleSpinBox *bridgeSpin_ = nullptr;

    QPlainTextEdit *reportView_ = nullptr;

    QDoubleSpinBox *dpiSpin_ = nullptr;
    QDoubleSpinBox *baseThicknessSpin_ = nullptr;
    QDoubleSpinBox *bumpHeightSpin_ = nullptr;
    QDoubleSpinBox *meshResSpin_ = nullptr;

    QPushButton *svgButton_ = nullptr;
    QPushButton *dxfButton_ = nullptr;
    QPushButton *pngButton_ = nullptr;
    QPushButton *stlButton_ = nullptr;
};
