#pragma once

#include "patternfab/Pattern.h"

#include <string>

namespace patternfab {

// Recovers approximate circular primitives from a grayscale PNG/TIFF speckle
// image via connected-component blob detection (darker pixels = speckle
// ink). Each blob becomes a Circle primitive: centroid -> center, pixel
// area -> equivalent radius (assumes roughly round speckles, matching
// typical DIC dot patterns).
//
// physicalParams supplies imagingResolutionPxPerMm for the pixel->mm
// conversion (its other fields are copied through unchanged into the
// returned Pattern).
//
// This is the fallback path for source patterns that don't already carry
// vector geometry (see loadPatternFromVectorFile) — a naive fixed threshold,
// not adaptive/Otsu, and single-channel grayscale input only. Throws
// std::runtime_error on an unreadable file or non-grayscale image.
Pattern loadPatternFromRasterFile(const std::string &path, const PhysicalParameters &physicalParams);

} // namespace patternfab
