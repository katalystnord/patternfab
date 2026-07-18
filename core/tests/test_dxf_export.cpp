#include <patternfab/DxfExport.h>

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

void testDxfExport() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 50.0;
    pattern.params.specimenHeightMm = 50.0;

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

    const std::string path = "test_pattern.dxf";
    patternfab::exportPatternToDxf(pattern, path);

    std::ifstream file(path);
    check(static_cast<bool>(file), "DXF file was written");
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string content = buffer.str();

    check(!content.empty(), "DXF file is non-empty");
    check(countOccurrences(content, "CIRCLE") >= 1, "CIRCLE entity present");
    check(countOccurrences(content, "ELLIPSE") >= 1, "ELLIPSE entity present");
    check(countOccurrences(content, "POLYLINE") >= 1, "POLYLINE entity present");
    check(content.find("PatternFab") != std::string::npos, "layer name present");
}

} // namespace

int main() {
    testDxfExport();

    if (failures == 0) {
        std::cout << "OK: all patternfab-core DXF export tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
