#include "patternfab/ConstraintEngine.h"

#include <algorithm>
#include <cmath>
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

// Centroid for polygon, center point otherwise -- the reference point used
// for gap-distance and direction calculations in checkStencilBridging.
std::pair<double, double> effectiveCenter(const Primitive &primitive) {
    if (primitive.shape == PrimitiveShape::Polygon) {
        double cx = 0.0;
        double cy = 0.0;
        for (const auto &[x, y] : primitive.verticesMm) {
            cx += x;
            cy += y;
        }
        const double n = static_cast<double>(primitive.verticesMm.size());
        return {cx / n, cy / n};
    }
    return {primitive.centerXMm, primitive.centerYMm};
}

// Effective radius of primitive in the direction (dirX, dirY) (a unit
// vector, pointing away from the primitive's effective center). See
// ConstraintEngine.h for the per-shape approximation rationale.
double effectiveRadiusToward(const Primitive &primitive, double dirX, double dirY) {
    if (primitive.shape == PrimitiveShape::Polygon) {
        const auto [cx, cy] = effectiveCenter(primitive);
        double maxDist = 0.0;
        for (const auto &[x, y] : primitive.verticesMm) {
            maxDist = std::max(maxDist, std::hypot(x - cx, y - cy));
        }
        return maxDist;
    }
    if (primitive.radiusXMm == primitive.radiusYMm) {
        return primitive.radiusXMm;
    }
    // Axis-aligned ellipse polar radius formula.
    const double rx = primitive.radiusXMm;
    const double ry = primitive.radiusYMm;
    const double denom = std::hypot(ry * dirX, rx * dirY);
    return (rx * ry) / denom;
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
    // A real speckle field is discrete dots, so its bounding box is always
    // inset from the specimen edge by up to the speckle spacing -- there can
    // be no ink beyond the outermost dot. Requiring the extent to reach the
    // specimen edge to within a float epsilon therefore false-positives on
    // every physically-applied field, firing the periodicity warning on
    // patterns that in fact cover the specimen in a single application. Use a
    // physical margin instead: the field only genuinely needs tiling if it
    // leaves an uncovered strip wider than one target speckle on either axis.
    // Fall back to the float epsilon when targetSpeckleSizeMm is unset (0), so
    // a degenerate pattern still isn't flagged on floating-point noise alone.
    const double margin = std::max(pattern.params.targetSpeckleSizeMm, kExtentEpsilonMm);
    return width < pattern.params.specimenWidthMm - margin ||
           height < pattern.params.specimenHeightMm - margin;
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

std::vector<BridgingViolation> checkStencilBridging(const Pattern &pattern,
                                                     const ManufacturingConstraints &constraints) {
    std::vector<BridgingViolation> violations;
    if (constraints.minBridgeWidthMm <= 0.0) {
        return violations;
    }

    const auto &primitives = pattern.primitives;
    for (std::size_t i = 0; i < primitives.size(); ++i) {
        const auto [cxi, cyi] = effectiveCenter(primitives[i]);
        for (std::size_t j = i + 1; j < primitives.size(); ++j) {
            const auto [cxj, cyj] = effectiveCenter(primitives[j]);
            const double dx = cxj - cxi;
            const double dy = cyj - cyi;
            const double centerDist = std::hypot(dx, dy);

            double gap;
            if (centerDist < 1e-12) {
                // Coincident centers: treat as fully overlapping.
                gap = -std::max(effectiveRadiusToward(primitives[i], 1.0, 0.0),
                                 effectiveRadiusToward(primitives[j], 1.0, 0.0));
            } else {
                const double ux = dx / centerDist;
                const double uy = dy / centerDist;
                const double ri = effectiveRadiusToward(primitives[i], ux, uy);
                const double rj = effectiveRadiusToward(primitives[j], -ux, -uy);
                gap = centerDist - ri - rj;
            }

            if (gap < constraints.minBridgeWidthMm) {
                violations.push_back({i, j, gap,
                                       "gap " + std::to_string(gap) + "mm between primitives " + std::to_string(i) +
                                           " and " + std::to_string(j) + " is below minimum bridge width " +
                                           std::to_string(constraints.minBridgeWidthMm) + "mm"});
            }
        }
    }
    return violations;
}

ConstraintReport evaluateConstraints(const Pattern &pattern, const ManufacturingConstraints &constraints) {
    ConstraintReport report;
    report.patternRequiresTiling = patternRequiresTiling(pattern);
    report.minimumFeatureSizeViolations = checkMinimumFeatureSize(pattern, constraints);
    report.bridgingViolations = checkStencilBridging(pattern, constraints);
    return report;
}

} // namespace patternfab
