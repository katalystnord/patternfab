#include "patternfab/UncertaintyEngine.h"

#include "PrimitiveDrawing.h"

#include <QColor>
#include <QImage>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace patternfab {

UncertaintyMap computeUncertaintyMap(const Pattern &pattern, const SensorNoiseProfile &noiseProfile) {
    const double widthMm = pattern.params.specimenWidthMm;
    const double heightMm = pattern.params.specimenHeightMm;
    const double pxPerMm = pattern.params.imagingResolutionPxPerMm;
    if (widthMm <= 0.0 || heightMm <= 0.0 || pxPerMm <= 0.0) {
        throw std::runtime_error(
            "computeUncertaintyMap: specimen dimensions and imagingResolutionPxPerMm must be positive");
    }

    const int widthPx = static_cast<int>(std::llround(widthMm * pxPerMm));
    const int heightPx = static_cast<int>(std::llround(heightMm * pxPerMm));
    if (widthPx < 2 || heightPx < 2) {
        throw std::runtime_error("computeUncertaintyMap: imagingResolutionPxPerMm too low for specimen size");
    }

    QImage image(widthPx, heightPx, QImage::Format_RGB32);
    image.fill(Qt::white); // matches PNG export's convention: black ink on white
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(pxPerMm, pxPerMm);
        detail::drawPrimitivesMm(painter, pattern.primitives);
        painter.end();
    }

    UncertaintyMap result;
    result.widthPx = widthPx;
    result.heightPx = heightPx;
    result.confidence.resize(static_cast<std::size_t>(widthPx) * heightPx, 0.0);

    const auto intensityAt = [&](int x, int y) -> double {
        x = std::clamp(x, 0, widthPx - 1);
        y = std::clamp(y, 0, heightPx - 1);
        return qGray(image.pixel(x, y)) / 255.0;
    };

    for (int y = 0; y < heightPx; ++y) {
        for (int x = 0; x < widthPx; ++x) {
            // Central-difference gradient -- a reasonable v1; Sobel would
            // be smoother but adds complexity not needed yet.
            const double gx = (intensityAt(x + 1, y) - intensityAt(x - 1, y)) / 2.0;
            const double gy = (intensityAt(x, y + 1) - intensityAt(x, y - 1)) / 2.0;
            const double gradientMagnitude = std::sqrt(gx * gx + gy * gy);

            const double signal = intensityAt(x, y);
            const double variance = std::max(1e-6, noiseProfile.S * signal + noiseProfile.O);
            const double noiseStd = std::sqrt(variance);

            result.confidence[static_cast<std::size_t>(y) * widthPx + x] = gradientMagnitude / noiseStd;
        }
    }

    return result;
}

double lowConfidenceFraction(const UncertaintyMap &map, double minConfidence) {
    if (map.confidence.empty()) {
        return 0.0;
    }
    const auto count = std::count_if(map.confidence.begin(), map.confidence.end(),
                                      [minConfidence](double c) { return c < minConfidence; });
    return static_cast<double>(count) / static_cast<double>(map.confidence.size());
}

} // namespace patternfab
