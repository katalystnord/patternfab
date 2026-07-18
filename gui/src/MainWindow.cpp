#include "MainWindow.h"

#include <patternfab/Core.h>

#include <QString>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QString("PatternFab %1").arg(QString::fromStdString(patternfab::coreVersion())));
    resize(800, 600);
}
