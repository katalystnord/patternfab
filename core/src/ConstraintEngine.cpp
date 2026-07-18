#include "patternfab/ConstraintEngine.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace patternfab {

namespace {

constexpr double kExtentEpsilonMm = 1e-9;

void expandForPrimitive(const Primitive &primitive, double &minX, double &minY, double &maxX, double &maxY) {
    if (primitive.shape == PrimitiveShape::Polygon) {
        for (const auto &[x, y] : primitive.verticesMm) {
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    } else {
        minX = std::min(minX, primitive.centerXMm - primitive.radiusXMm);
        minY = std::min(minY, primitive.centerYMm - primitive.radiusYMm);
        maxX = std::max(maxX, primitive.centerXMm + primitive.radiusXMm);
        maxY = std::max(maxY, primitive.centerYMm + primitive.radiusYMm);
    }
}

// Approximate feature size for a single primitive: exact smallest dimension
// for circle/ellipse, bounding-box-based approximation for polygon (see
// ConstraintEngine.h for why the approximation is acceptable here).
double featureSizeMm(const Primitive &primitive) {
    if (primitive.shape == PrimitiveShape::Polygon) {
        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double maxY = std::numeric_limits<double>::lowest();
        expandForPrimitive(primitive, minX, minY, maxX, maxY);
        return std::min(maxX - minX, maxY - minY);
    }
    return 2.0 * std::min(primitive.radiusXMm, primitive.radiusYMm);
}

} // namespace

BoundingBox computeBoundingBox(const Pattern &pattern) {
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    for (const auto &primitive : pattern.primitives) {
        expandForPrimitive(primitive, minX, minY, maxX, maxY);
    }

    if (pattern.primitives.empty()) {
        return {0.0, 0.0, 0.0, 0.0};
    }
    return {minX, minY, maxX, maxY};
}

bool patternRequiresTiling(const Pattern &pattern) {
    const BoundingBox bbox = computeBoundingBox(pattern);
    const double width = bbox.maxXMm - bbox.minXMm;
    const double height = bbox.maxYMm - bbox.minYMm;
    return width < pattern.params.specimenWidthMm - kExtentEpsilonMm ||
           height < pattern.params.specimenHeightMm - kExtentEpsilonMm;
}

std::vector<ConstraintViolation> checkMinimumFeatureSize(const Pattern &pattern,
                                                          const ManufacturingConstraints &constraints) {
    std::vector<ConstraintViolation> violations;
    if (constraints.minFeatureSizeMm <= 0.0) {
        return violations;
    }

    for (std::size_t i = 0; i < pattern.primitives.size(); ++i) {
        const double size = featureSizeMm(pattern.primitives[i]);
        if (size < constraints.minFeatureSizeMm) {
            violations.push_back(
                {i, "feature size " + std::to_string(size) + "mm is below minimum " +
                        std::to_string(constraints.minFeatureSizeMm) + "mm"});
        }
    }
    return violations;
}

Pattern applyBleedCompensation(const Pattern &pattern, const ManufacturingConstraints &constraints) {
    if (constraints.bleedCompensationMm <= 0.0) {
        return pattern;
    }

    const bool hasPolygon = std::any_of(pattern.primitives.begin(), pattern.primitives.end(),
                                         [](const Primitive &p) { return p.shape == PrimitiveShape::Polygon; });
    if (hasPolygon) {
        throw std::runtime_error(
            "applyBleedCompensation: pattern contains polygon primitive(s); true polygon "
            "offsetting isn't implemented, refusing to approximate on fabrication geometry");
    }

    Pattern result = pattern;
    for (auto &primitive : result.primitives) {
        primitive.radiusXMm = std::max(0.0, primitive.radiusXMm - constraints.bleedCompensationMm);
        primitive.radiusYMm = std::max(0.0, primitive.radiusYMm - constraints.bleedCompensationMm);
    }
    return result;
}

ConstraintReport evaluateConstraints(const Pattern &pattern, const ManufacturingConstraints &constraints) {
    ConstraintReport report;
    report.patternRequiresTiling = patternRequiresTiling(pattern);
    report.minimumFeatureSizeViolations = checkMinimumFeatureSize(pattern, constraints);
    return report;
}

} // namespace patternfab
