#include "MainWindow.h"

#include <QVTKOpenGLNativeWidget.h>

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char *argv[]) {
    // Required before any QVTKOpenGLNativeWidget is constructed: it fixes the
    // OpenGL context format the embedded VTK render window shares.
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    // patternfab-gui <pattern.json> opens that pattern straight away. Loaded
    // after show() so that a failure reports itself in a dialog over a real
    // window rather than against nothing.
    const QStringList args = QApplication::arguments();
    if (args.size() > 1) {
        window.loadPattern(args.at(1));
    }

    return app.exec();
}
