#pragma once

#include <patternfab/NoiseFloorImage.h>

#include <QImage>
#include <QWidget>

// The noise floor drawn over the specimen: where this pattern can measure, and
// how finely, as a picture rather than as two numbers.
//
// Beside the "What it can measure" panel rather than inside it. The panel gives
// the figure; this answers the question that follows it, which is WHERE -- a
// weak band along one edge is a layout problem and weakness scattered evenly is
// a speckle-size problem, and nothing in a summary distinguishes them.
//
// All of the decisions about what a colour MEANS live in core/NoiseFloorImage.h,
// where tests reach them. This widget only puts them on screen.
class NoiseFloorWidget : public QWidget {
    Q_OBJECT

public:
    explicit NoiseFloorWidget(QWidget *parent = nullptr);

    void showMap(const patternfab::NoiseFloorMap &map, const patternfab::NoiseFloorScale &scale);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage image_;
    patternfab::NoiseFloorScale scale_;
    int subsetRadiusPx_ = 0;
    bool hasContent_ = false;
};
