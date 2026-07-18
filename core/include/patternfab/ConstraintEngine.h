#pragma once

#include "patternfab/Pattern.h"

#include <string>
#include <vector>

namespace patternfab {

// All three are user-supplied, not hardcoded per-process defaults: the
// right minimum feature size, bleed compensation, and bridge width depend on
// the specific printer/laser/material in use, not on a fabrication *method*
// in the abstract. Passing 0.0 disables that check/compensation.
struct ManufacturingConstraints {
    double minFeatureSizeMm = 0.0;
    double bleedCompensationMm = 0.0;
    double minBridgeWidthMm = 0.0;
};

struct ConstraintViolation {
    std::size_t primitiveIndex;
    std::string message;
};

// Gap between two primitives' boundaries (can be negative if they overlap)
// falls below the required minimum bridge width -- the material web between
// two adjacent stencil holes is thin enough to tear.
struct BridgingViolation {
    std::size_t primitiveIndexA;
    std::size_t primitiveIndexB;
    double gapMm;
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
    std::vector<BridgingViolation> bridgingViolations;
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

// Pairwise gap check between every two primitives' boundaries, flagging
// pairs closer than constraints.minBridgeWidthMm (this is the practical,
// checkable form of "stencil bridging" for this primitive set: simple
// filled circles/ellipses/polygons don't create floating islands on their
// own, but two adjacent cut holes can leave a material web thin enough to
// tear). O(n^2) in primitive count -- fine for typical speckle counts, a
// known limitation for very large patterns.
//
// Circle-circle gaps are exact. Ellipse gaps use the axis-aligned polar
// radius toward the other primitive (exact for a single ellipse, an
// approximation for ellipse-to-ellipse minimum distance). Polygon gaps use
// the max centroid-to-vertex distance as the effective radius -- a
// deliberately conservative (over-estimating primitive size, so
// under-estimating gap) approximation, biased toward flagging more rather
// than missing a real too-thin bridge.
std::vector<BridgingViolation> checkStencilBridging(const Pattern &pattern,
                                                     const ManufacturingConstraints &constraints);

ConstraintReport evaluateConstraints(const Pattern &pattern, const ManufacturingConstraints &constraints);

} // namespace patternfab
