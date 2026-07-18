#include <patternfab/ConstraintEngine.h>

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

patternfab::Primitive makeCircle(double x, double y, double radius) {
    patternfab::Primitive p;
    p.shape = patternfab::PrimitiveShape::Circle;
    p.centerXMm = x;
    p.centerYMm = y;
    p.radiusXMm = radius;
    p.radiusYMm = radius;
    return p;
}

void testBoundingBoxAndTiling() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    pattern.primitives.push_back(makeCircle(1.0, 1.0, 0.5));
    pattern.primitives.push_back(makeCircle(4.0, 4.0, 0.5));

    const auto bbox = patternfab::computeBoundingBox(pattern);
    check(bbox.minXMm == 0.5, "bbox minX");
    check(bbox.maxXMm == 4.5, "bbox maxX");

    // Pattern only spans 0.5..4.5 (4mm), specimen is 10mm -> requires tiling.
    check(patternfab::patternRequiresTiling(pattern), "small pattern requires tiling");

    // Enlarge the specimen requirement to match the pattern extent exactly.
    pattern.params.specimenWidthMm = 4.0;
    pattern.params.specimenHeightMm = 4.0;
    check(!patternfab::patternRequiresTiling(pattern), "pattern covering specimen does not require tiling");
}

void testMinimumFeatureSize() {
    patternfab::Pattern pattern;
    pattern.primitives.push_back(makeCircle(0, 0, 0.1));  // diameter 0.2mm
    pattern.primitives.push_back(makeCircle(1, 1, 0.5));  // diameter 1.0mm

    patternfab::ManufacturingConstraints constraints;
    constraints.minFeatureSizeMm = 0.5;

    const auto violations = patternfab::checkMinimumFeatureSize(pattern, constraints);
    check(violations.size() == 1, "exactly one violation (the 0.2mm circle)");
    if (!violations.empty()) {
        check(violations[0].primitiveIndex == 0, "violation flags primitive 0");
    }

    constraints.minFeatureSizeMm = 0.0;
    check(patternfab::checkMinimumFeatureSize(pattern, constraints).empty(), "zero threshold disables check");
}

void testBleedCompensation() {
    patternfab::Pattern pattern;
    pattern.primitives.push_back(makeCircle(0, 0, 1.0));

    patternfab::ManufacturingConstraints constraints;
    constraints.bleedCompensationMm = 0.3;

    const auto compensated = patternfab::applyBleedCompensation(pattern, constraints);
    check(compensated.primitives[0].radiusXMm == 0.7, "radius reduced by bleed compensation");

    // Clamped at zero, not negative.
    constraints.bleedCompensationMm = 5.0;
    const auto clamped = patternfab::applyBleedCompensation(pattern, constraints);
    check(clamped.primitives[0].radiusXMm == 0.0, "radius clamped at zero");

    // Polygon + nonzero compensation must throw.
    patternfab::Pattern polygonPattern;
    patternfab::Primitive polygon;
    polygon.shape = patternfab::PrimitiveShape::Polygon;
    polygon.verticesMm = {{0, 0}, {1, 0}, {1, 1}};
    polygonPattern.primitives.push_back(polygon);

    bool threw = false;
    try {
        patternfab::applyBleedCompensation(polygonPattern, constraints);
    } catch (const std::runtime_error &) {
        threw = true;
    }
    check(threw, "polygon + nonzero compensation throws");

    // Zero compensation must NOT throw even with a polygon present.
    patternfab::ManufacturingConstraints noCompensation;
    bool didNotThrow = true;
    try {
        patternfab::applyBleedCompensation(polygonPattern, noCompensation);
    } catch (const std::runtime_error &) {
        didNotThrow = false;
    }
    check(didNotThrow, "polygon + zero compensation is a no-op, does not throw");
}

} // namespace

int main() {
    testBoundingBoxAndTiling();
    testMinimumFeatureSize();
    testBleedCompensation();

    if (failures == 0) {
        std::cout << "OK: all patternfab-core constraint engine tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
