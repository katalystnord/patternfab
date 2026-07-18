#pragma once

#include "patternfab/Pattern.h"

#include <string>

namespace patternfab {

// Renders pattern.primitives filled black on white, sized to
// pattern.params.specimenWidthMm/specimenHeightMm. The SVG's viewBox spans
// exactly that many mm (1 user unit = 1mm), and the root <svg> width/height
// attributes carry an explicit "mm" suffix so laser-cutting/CAM software
// gets the physically correct size regardless of any DPI convention.
//
// Throws std::runtime_error if the specimen dimensions aren't positive, or
// if the file can't be written.
void exportPatternToSvg(const Pattern &pattern, const std::string &path);

} // namespace patternfab
