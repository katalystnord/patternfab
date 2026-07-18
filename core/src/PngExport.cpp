#include "patternfab/PngExport.h"

#include "PrimitiveDrawing.h"

#include <QImage>
#include <QPainter>

#include <cmath>
#include <stdexcept>

namespace patternfab {

void exportPatternToPng(const Pattern &pattern, const std::string &path, double dpi) {
    const double widthMm = pattern.params.specimenWidthMm;
    const double heightMm = pattern.params.specimenHeightMm;
    if (widthMm <= 0.0 || heightMm <= 0.0 || dpi <= 0.0) {
        throw std::runtime_error("exportPatternToPng: specimen dimensions and dpi must be positive");
    }

    const double pxPerMm = dpi / 25.4;
    const int widthPx = static_cast<int>(std::llround(widthMm * pxPerMm));
    const int heightPx = static_cast<int>(std::llround(heightMm * pxPerMm));
    if (widthPx <= 0 || heightPx <= 0) {
        throw std::runtime_error("exportPatternToPng: resulting image has zero size: " + path);
    }

    QImage image(widthPx, heightPx, QImage::Format_RGB32);
    image.fill(Qt::white);

    const double dotsPerMeter = dpi / 0.0254;
    image.setDotsPerMeterX(static_cast<int>(std::llround(dotsPerMeter)));
    image.setDotsPerMeterY(static_cast<int>(std::llround(dotsPerMeter)));

    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(pxPerMm, pxPerMm);
        detail::drawPrimitivesMm(painter, pattern.primitives);
        painter.end();
    }

    if (!image.save(QString::fromStdString(path), "PNG")) {
        throw std::runtime_error("exportPatternToPng: failed to write file: " + path);
    }
}

} // namespace patternfab
