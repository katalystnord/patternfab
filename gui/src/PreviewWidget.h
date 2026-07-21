#pragma once

#include <QSvgRenderer>
#include <QString>
#include <QWidget>

// A dependency-light 2D preview: renders a pattern's exported SVG scaled to
// fit, on a white "specimen" ground. Deliberately uses the real SVG export
// path (via QSvgRenderer over the file exportPatternToSvg writes) rather than
// a separate draw routine, so what's previewed is exactly what's exported.
// Needs only Qt6::Svg -- no VTK-in-Qt embedding (that arrives with the 3D
// relief preview in a later increment).
class PreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget *parent = nullptr);

    // Load and display the SVG at the given path. Clears on failure.
    void showSvg(const QString &path);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QSvgRenderer renderer_;
    bool hasContent_ = false;
};
