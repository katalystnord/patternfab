#include "patternfab/UncertaintyEngine.h"

#include "PrimitiveDrawing.h"

#include <QColor>
#include <QImage>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace patternfab {

namespace {

// What the camera would see, and what the sensor would add to it: the two
// gradient-energy fields and the noise variance, per pixel. Shared by both
// entry points so the confidence map and the noise floor cannot come to
// disagree about the same rendering.
struct RenderedFields {
    int widthPx = 0;
    int heightPx = 0;
    std::vector<double> gxSquared;
    std::vector<double> gySquared;
    std::vector<double> noiseVariance;
    std::vector<double> intensity;
};

RenderedFields renderFields(const Pattern &pattern, const SensorNoiseProfile &noiseProfile) {
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

    RenderedFields fields;
    fields.widthPx = widthPx;
    fields.heightPx = heightPx;
    const std::size_t count = static_cast<std::size_t>(widthPx) * heightPx;
    fields.gxSquared.resize(count);
    fields.gySquared.resize(count);
    fields.noiseVariance.resize(count);
    fields.intensity.resize(count);

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
            const double signal = intensityAt(x, y);

            const std::size_t index = static_cast<std::size_t>(y) * widthPx + x;
            fields.gxSquared[index] = gx * gx;
            fields.gySquared[index] = gy * gy;
            fields.noiseVariance[index] =
                std::max(1e-6, noiseProfile.S * signal + noiseProfile.O);
            fields.intensity[index] = signal;
        }
    }

    return fields;
}

// Summed-area table, so a subset's total is four lookups rather than a loop
// over its pixels. Without it the map is O(width * height * radius^2), which on
// a realistic pattern at a realistic subset size is minutes rather than
// milliseconds.
std::vector<double> integralImage(const std::vector<double> &field, int w, int h) {
    std::vector<double> sum(static_cast<std::size_t>(w + 1) * (h + 1), 0.0);
    for (int y = 0; y < h; ++y) {
        double rowRunning = 0.0;
        for (int x = 0; x < w; ++x) {
            rowRunning += field[static_cast<std::size_t>(y) * w + x];
            sum[static_cast<std::size_t>(y + 1) * (w + 1) + (x + 1)] =
                sum[static_cast<std::size_t>(y) * (w + 1) + (x + 1)] + rowRunning;
        }
    }
    return sum;
}

double boxSum(const std::vector<double> &integral, int w, int x0, int y0, int x1, int y1) {
    const auto at = [&](int x, int y) {
        return integral[static_cast<std::size_t>(y) * (w + 1) + x];
    };
    return at(x1 + 1, y1 + 1) - at(x0, y1 + 1) - at(x1 + 1, y0) + at(x0, y0);
}

} // namespace

UncertaintyMap computeUncertaintyMap(const Pattern &pattern, const SensorNoiseProfile &noiseProfile) {
    const RenderedFields fields = renderFields(pattern, noiseProfile);

    UncertaintyMap result;
    result.widthPx = fields.widthPx;
    result.heightPx = fields.heightPx;
    result.confidence.resize(fields.gxSquared.size(), 0.0);

    for (std::size_t i = 0; i < fields.gxSquared.size(); ++i) {
        const double gradientMagnitude =
            std::sqrt(fields.gxSquared[i] + fields.gySquared[i]);
        result.confidence[i] = gradientMagnitude / std::sqrt(fields.noiseVariance[i]);
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

NoiseFloorMap computeNoiseFloorMap(const Pattern &pattern,
                                   const SensorNoiseProfile &noiseProfile,
                                   int subsetRadiusPx) {
    if (subsetRadiusPx <= 0) {
        throw std::runtime_error("computeNoiseFloorMap: subsetRadiusPx must be positive");
    }

    const RenderedFields fields = renderFields(pattern, noiseProfile);
    const int w = fields.widthPx;
    const int h = fields.heightPx;

    const std::vector<double> gxIntegral = integralImage(fields.gxSquared, w, h);
    const std::vector<double> gyIntegral = integralImage(fields.gySquared, w, h);
    const std::vector<double> noiseIntegral = integralImage(fields.noiseVariance, w, h);

    NoiseFloorMap map;
    map.widthPx = w;
    map.heightPx = h;
    map.subsetRadiusPx = subsetRadiusPx;
    map.sigmaPx.assign(static_cast<std::size_t>(w) * h,
                       std::numeric_limits<double>::quiet_NaN());

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // A subset that would run off the image is not a subset the
            // correlation could place, so no floor is established there. Left
            // as not-a-number rather than computed over a clipped window, which
            // would quietly report a different subset's figure.
            if (x - subsetRadiusPx < 0 || y - subsetRadiusPx < 0
                || x + subsetRadiusPx >= w || y + subsetRadiusPx >= h) {
                continue;
            }

            const int x0 = x - subsetRadiusPx;
            const int y0 = y - subsetRadiusPx;
            const int x1 = x + subsetRadiusPx;
            const int y1 = y + subsetRadiusPx;

            const double sumGx = boxSum(gxIntegral, w, x0, y0, x1, y1);
            const double sumGy = boxSum(gyIntegral, w, x0, y0, x1, y1);

            // ⚑ The WORSE axis. A pattern of bars has enormous energy across
            // them and none along them, and it cannot measure the direction it
            // has no structure in. Taking the gradient magnitude instead, as
            // the confidence map does, rates exactly that pattern as excellent.
            const double weakest = std::min(sumGx, sumGy);
            if (!(weakest > 0.0)) {
                continue;   // nothing to measure here, and nothing to report
            }

            // The subset's mean noise variance. The standard formula assumes
            // homogeneous noise; this sensor model makes it depend on
            // intensity, so the subset's mean is what stands in for it.
            const int side = 2 * subsetRadiusPx + 1;
            const double pixels = static_cast<double>(side) * side;
            const double meanVariance = boxSum(noiseIntegral, w, x0, y0, x1, y1) / pixels;

            map.sigmaPx[static_cast<std::size_t>(y) * w + x] =
                std::sqrt(2.0 * meanVariance / weakest);
        }
    }

    return map;
}

NoiseFloorSummary summariseNoiseFloor(const NoiseFloorMap &map) {
    NoiseFloorSummary summary;
    summary.totalCount = static_cast<int>(map.sigmaPx.size());

    std::vector<double> established;
    established.reserve(map.sigmaPx.size());
    for (double sigma : map.sigmaPx) {
        if (!std::isnan(sigma)) {
            established.push_back(sigma);
        }
    }

    summary.establishedCount = static_cast<int>(established.size());
    if (established.empty()) {
        return summary;
    }

    std::sort(established.begin(), established.end());
    summary.bestPx = established.front();
    summary.worstPx = established.back();

    // ⚑ The value 95 per cent of established points beat, not the maximum. One
    // subset over a blank corner is not the pattern's figure, and letting it be
    // one is a mistake this project has already made once, in SurView, on a
    // measured field rather than a designed one.
    const std::size_t index = static_cast<std::size_t>(
        std::llround(0.95 * static_cast<double>(established.size() - 1)));
    summary.typicalPx = established[index];
    return summary;
}

} // namespace patternfab
