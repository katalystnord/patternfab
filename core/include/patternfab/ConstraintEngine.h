#pragma once

#include "patternfab/Pattern.h"

#include <string>
#include <vector>

namespace patternfab {

// Both fields are user-supplied, not hardcoded per-process defaults: the
// right minimum feature size and bleed compensation depend on the specific
// printer/laser/process in use, not on a fabrication *method* in the
// abstract. Passing 0.0 disables that check/compensation.
struct ManufacturingConstraints {
    double minFeatureSizeMm = 0.0;
    double bleedCompensationMm = 0.0;
};

struct ConstraintViolation {
    std::size_t primitiveIndex;
    std::string message;
};

struct ConstraintReport {
    // True if the pattern's extent is smaller than the specimen it's meant
    // to cover, meaning it would need to be tiled -- which risks injecting
    // periodicity (see "The periodicity trap" in CLAUDE.md). This checks
    // only whether tiling would be *required*, not whether a given tile is
    // provably non-periodic -- that's a harder problem, out of scope here.
    bool patternRequiresTiling = false;
    std::vector<ConstraintViolation> minimumFeatureSizeViolations;
};

struct BoundingBox {
    double minXMm;
    double minYMm;
    double maxXMm;
    double maxYMm;
};

// Bounding box across all primitives (circle/ellipse: center +/- radius;
// polygon: vertex extents).
BoundingBox computeBoundingBox(const Pattern &pattern);

bool patternRequiresTiling(const Pattern &pattern);

// Approximate for polygons (uses their bounding-box min dimension, not true
// minimum width) -- acceptable here because this only produces a warning,
// it doesn't mutate geometry that gets fabricated. Exact for circle/ellipse.
std::vector<ConstraintViolation> checkMinimumFeatureSize(const Pattern &pattern,
                                                          const ManufacturingConstraints &constraints);

// Returns a copy of pattern with bleedCompensationMm subtracted from each
// circle/ellipse radius (clamped at 0). No-op (including for polygons) when
// bleedCompensationMm <= 0. Otherwise throws std::runtime_error if the
// pattern contains any polygon primitive: true polygon offsetting (Minkowski
// erosion/dilation) isn't implemented, and an approximation here would
// mutate geometry that goes straight to fabrication -- wrong is worse than
// unsupported.
Pattern applyBleedCompensation(const Pattern &pattern, const ManufacturingConstraints &constraints);

ConstraintReport evaluateConstraints(const Pattern &pattern, const ManufacturingConstraints &constraints);

} // namespace patternfab
