#pragma once

#include "patternfab/Pattern.h"

#include <string>

namespace patternfab {

// All caller-supplied, not hardcoded -- same reasoning as
// ManufacturingConstraints (ConstraintEngine.h): the right base thickness,
// bump height, and mesh density depend on the specific stamp/mold design
// and printer in use, not on "stamp" as a fabrication method in the
// abstract.
struct ReliefParameters {
    double baseThicknessMm = 0.0;
    double bumpHeightMm = 0.0;
    // Grid samples per mm (in both X and Y) used to rasterize the
    // heightfield before meshing. Higher = smoother bumps, larger file,
    // slower export.
    double meshResolutionPerMm = 0.0;
};

// Builds a solid relief: a flat base plate of baseThicknessMm with a raised
// dome over each circle/ellipse primitive (hemispherical-cap profile,
// tapering smoothly to zero at the primitive's edge) rising bumpHeightMm at
// its highest point, and a flat-topped raised region for each polygon
// primitive (no dome -- true distance-based falloff for arbitrary polygons
// isn't implemented, a v1 simplification). Overlapping primitives take the
// tallest bump at each point, not a sum.
//
// Before calling this for stamp/mold fabrication, run
// ConstraintEngine::checkStencilBridging with a minBridgeWidthMm reflecting
// the mold-wall/cavity minimum for your process -- the same adjacent-gap
// concern that applies to stencils (a too-thin material web) applies here
// to mold cavity walls between adjacent bumps.
//
// The heightfield is rasterized on a regular grid at meshResolutionPerMm
// samples/mm, then swept into a closed solid (top surface + side skirt +
// bottom cap) via linear extrusion, and written as a binary STL.
// O(gridPoints * primitiveCount) -- a known limitation for very dense
// grids or very large primitive counts.
//
// Throws std::runtime_error if any of the specimen dimensions,
// baseThicknessMm, bumpHeightMm, or meshResolutionPerMm aren't positive, or
// if the file can't be written.
void exportPatternToStl(const Pattern &pattern, const std::string &path, const ReliefParameters &relief);

} // namespace patternfab
