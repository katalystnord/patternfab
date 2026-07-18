#pragma once

#include "patternfab/Pattern.h"

#include <string>

namespace patternfab {

// Renders pattern.primitives filled black on white as a PNG raster image,
// sized to pattern.params.specimenWidthMm/specimenHeightMm at the given dpi
// (dots per inch). dpi is caller-supplied, not hardcoded per-process -- it
// depends on the specific printer/direct-write setup in use (same reasoning
// as ManufacturingConstraints in ConstraintEngine.h). The saved PNG embeds
// physical DPI metadata (dots-per-meter) so consuming software sizes it
// correctly.
//
// Throws std::runtime_error if dpi or the specimen dimensions aren't
// positive, or if the file can't be written.
void exportPatternToPng(const Pattern &pattern, const std::string &path, double dpi);

} // namespace patternfab
