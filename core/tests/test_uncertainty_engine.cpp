#include <patternfab/UncertaintyEngine.h>

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

patternfab::Pattern makeSingleCirclePattern() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    pattern.params.imagingResolutionPxPerMm = 20.0;

    patternfab::Primitive circle;
    circle.shape = patternfab::PrimitiveShape::Circle;
    circle.centerXMm = 5.0;
    circle.centerYMm = 5.0;
    circle.radiusXMm = 2.0;
    circle.radiusYMm = 2.0;
    pattern.primitives.push_back(circle);
    return pattern;
}

double confidenceAt(const patternfab::UncertaintyMap &map, int x, int y) {
    return map.confidence[static_cast<std::size_t>(y) * map.widthPx + x];
}

void testGradientLocation() {
    const patternfab::Pattern pattern = makeSingleCirclePattern();
    patternfab::SensorNoiseProfile noise;
    noise.S = 0.0;
    noise.O = 0.01; // small constant noise floor

    const auto map = patternfab::computeUncertaintyMap(pattern, noise);
    check(map.widthPx == 200 && map.heightPx == 200, "map sized to specimen * imaging resolution");

    const double pxPerMm = pattern.params.imagingResolutionPxPerMm;
    // Circle edge: at radius 2mm from center (5,5) along +X, i.e. x=7mm, y=5mm.
    const int edgeX = static_cast<int>(7.0 * pxPerMm);
    const int edgeY = static_cast<int>(5.0 * pxPerMm);
    // Circle interior (uniform black) and far background (uniform white).
    const int interiorX = static_cast<int>(5.0 * pxPerMm);
    const int interiorY = static_cast<int>(5.0 * pxPerMm);
    const int backgroundX = static_cast<int>(0.5 * pxPerMm);
    const int backgroundY = static_cast<int>(0.5 * pxPerMm);

    const double edgeConfidence = confidenceAt(map, edgeX, edgeY);
    const double interiorConfidence = confidenceAt(map, interiorX, interiorY);
    const double backgroundConfidence = confidenceAt(map, backgroundX, backgroundY);

    check(edgeConfidence > interiorConfidence,
          "confidence at circle edge exceeds confidence in uniform interior");
    check(edgeConfidence > backgroundConfidence,
          "confidence at circle edge exceeds confidence in uniform background");
    check(interiorConfidence < 0.01 * edgeConfidence, "uniform interior has near-zero confidence relative to edge");
}

void testNoiseReducesConfidence() {
    const patternfab::Pattern pattern = makeSingleCirclePattern();
    const double pxPerMm = pattern.params.imagingResolutionPxPerMm;
    const int edgeX = static_cast<int>(7.0 * pxPerMm);
    const int edgeY = static_cast<int>(5.0 * pxPerMm);

    patternfab::SensorNoiseProfile lowNoise;
    lowNoise.O = 0.01;
    patternfab::SensorNoiseProfile highNoise;
    highNoise.O = 1.0;

    const auto lowNoiseMap = patternfab::computeUncertaintyMap(pattern, lowNoise);
    const auto highNoiseMap = patternfab::computeUncertaintyMap(pattern, highNoise);

    check(confidenceAt(lowNoiseMap, edgeX, edgeY) > confidenceAt(highNoiseMap, edgeX, edgeY),
          "higher sensor noise reduces confidence at the same location");
}

void testLowConfidenceFraction() {
    patternfab::UncertaintyMap map;
    map.widthPx = 2;
    map.heightPx = 2;
    map.confidence = {0.1, 0.5, 0.9, 2.0};

    check(std::abs(patternfab::lowConfidenceFraction(map, 1.0) - 0.75) < 1e-9,
          "three of four pixels below threshold 1.0");
    check(std::abs(patternfab::lowConfidenceFraction(map, 0.0) - 0.0) < 1e-9, "nothing below zero threshold");

    patternfab::UncertaintyMap empty;
    check(patternfab::lowConfidenceFraction(empty, 1.0) == 0.0, "empty map returns zero, not a crash");
}

void testInvalidParameters() {
    patternfab::Pattern pattern = makeSingleCirclePattern();
    pattern.params.imagingResolutionPxPerMm = 0.0;
    patternfab::SensorNoiseProfile noise;

    bool threw = false;
    try {
        patternfab::computeUncertaintyMap(pattern, noise);
    } catch (const std::runtime_error &) {
        threw = true;
    }
    check(threw, "zero imagingResolutionPxPerMm throws");
}

} // namespace

int main() {
    testGradientLocation();
    testNoiseReducesConfidence();
    testLowConfidenceFraction();
    testInvalidParameters();

    if (failures == 0) {
        std::cout << "OK: all patternfab-core uncertainty engine tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
