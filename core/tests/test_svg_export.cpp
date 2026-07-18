#include <patternfab/SvgExport.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

int failures = 0;

void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        ++failures;
    }
}

std::size_t countOccurrences(const std::string &haystack, const std::string &needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

void testSvgExport() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 37.6;
    pattern.params.specimenHeightMm = 82.25;

    patternfab::Primitive circle;
    circle.shape = patternfab::PrimitiveShape::Circle;
    circle.centerXMm = 5.0;
    circle.centerYMm = 5.0;
    circle.radiusXMm = 1.0;
    circle.radiusYMm = 1.0;
    pattern.primitives.push_back(circle);

    patternfab::Primitive ellipse;
    ellipse.shape = patternfab::PrimitiveShape::Ellipse;
    ellipse.centerXMm = 10.0;
    ellipse.centerYMm = 10.0;
    ellipse.radiusXMm = 2.0;
    ellipse.radiusYMm = 1.0;
    pattern.primitives.push_back(ellipse);

    patternfab::Primitive polygon;
    polygon.shape = patternfab::PrimitiveShape::Polygon;
    polygon.verticesMm = {{0, 0}, {1, 0}, {1, 1}};
    pattern.primitives.push_back(polygon);

    const std::string path = "test_pattern.svg";
    patternfab::exportPatternToSvg(pattern, path);

    std::ifstream file(path);
    check(static_cast<bool>(file), "SVG file was written");
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string content = buffer.str();

    check(content.find("width=\"37.6mm\"") != std::string::npos, "declared width is exact mm value");
    check(content.find("height=\"82.25mm\"") != std::string::npos, "declared height is exact mm value");
    check(content.find("viewBox=\"0 0 37.6 82.25\"") != std::string::npos, "viewBox spans exact mm extent");
    check(countOccurrences(content, "<circle") + countOccurrences(content, "<ellipse") == 2,
          "two ellipse/circle elements emitted");
    check(countOccurrences(content, "<path") == 1, "one path element emitted for the polygon");

    // Empty specimen dimensions must throw rather than emit a nonsensical file.
    patternfab::Pattern emptyDims;
    bool threw = false;
    try {
        patternfab::exportPatternToSvg(emptyDims, "should_not_be_written.svg");
    } catch (const std::runtime_error &) {
        threw = true;
    }
    check(threw, "zero specimen dimensions throws");
}

} // namespace

int main() {
    testSvgExport();

    if (failures == 0) {
        std::cout << "OK: all patternfab-core SVG export tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
