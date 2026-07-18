#include <patternfab/PngExport.h>

#include <QColor>
#include <QImage>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        ++failures;
    }
}

void testPngExport() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 5.0;

    patternfab::Primitive circle;
    circle.shape = patternfab::PrimitiveShape::Circle;
    circle.centerXMm = 5.0;
    circle.centerYMm = 2.5;
    circle.radiusXMm = 1.0;
    circle.radiusYMm = 1.0;
    pattern.primitives.push_back(circle);

    const double dpi = 300.0;
    const std::string path = "test_pattern.png";
    patternfab::exportPatternToPng(pattern, path, dpi);

    QImage image(QString::fromStdString(path));
    check(!image.isNull(), "PNG file was written and is readable");

    const double pxPerMm = dpi / 25.4;
    const int expectedWidthPx = static_cast<int>(std::llround(pattern.params.specimenWidthMm * pxPerMm));
    const int expectedHeightPx = static_cast<int>(std::llround(pattern.params.specimenHeightMm * pxPerMm));
    check(image.width() == expectedWidthPx,
          "image width matches specimen size at given dpi (" + std::to_string(image.width()) + " vs " +
              std::to_string(expectedWidthPx) + ")");
    check(image.height() == expectedHeightPx, "image height matches specimen size at given dpi");

    const double expectedDotsPerMeter = dpi / 0.0254;
    check(std::abs(image.dotsPerMeterX() - expectedDotsPerMeter) < 2,
          "embedded DPI metadata (dotsPerMeterX) is correct");

    // Center of the drawn circle should be black (ink); a corner far from
    // any primitive should stay white (background).
    const int centerPx = static_cast<int>(circle.centerXMm * pxPerMm);
    const int centerPy = static_cast<int>(circle.centerYMm * pxPerMm);
    check(QColor(image.pixel(centerPx, centerPy)).red() < 128, "circle center pixel is dark");
    check(QColor(image.pixel(2, 2)).red() > 128, "corner pixel (no primitive) stays light");

    // Non-positive dpi/dimensions must throw.
    bool threw = false;
    try {
        patternfab::exportPatternToPng(pattern, "should_not_be_written.png", 0.0);
    } catch (const std::runtime_error &) {
        threw = true;
    }
    check(threw, "zero dpi throws");
}

} // namespace

int main() {
    testPngExport();

    if (failures == 0) {
        std::cout << "OK: all patternfab-core PNG export tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
