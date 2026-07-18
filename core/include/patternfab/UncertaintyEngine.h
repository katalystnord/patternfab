#pragma once

#include "patternfab/Pattern.h"

#include <vector>

namespace patternfab {

// A single-channel affine sensor noise model: variance(signal) = S*signal +
// O, where signal is normalized pixel intensity in [0,1]. Caller-supplied
// from a real device's Camera2 CameraCharacteristics.SENSOR_NOISE_PROFILE
// query (per capture, per gain) -- not hardcoded, since it's specific to
// the actual sensor/ISO in use, same reasoning as ManufacturingConstraints
// and ReliefParameters. Real Bayer sensors expose separate (S,O) per
// color-filter channel; this v1 models a single representative channel
// (e.g. green/luminance) rather than full per-channel Bayer handling.
struct SensorNoiseProfile {
    double S = 0.0;
    double O = 0.0;
};

struct UncertaintyMap {
    int widthPx = 0;
    int heightPx = 0;
    // Row-major, size widthPx*heightPx. Higher = more confident (higher
    // local intensity-gradient magnitude relative to sensor noise at that
    // pixel's intensity level).
    std::vector<double> confidence;
};

// Renders the pattern at pattern.params.imagingResolutionPxPerMm (the
// camera's imaging resolution -- not a printer DPI, since this predicts
// what the camera would actually see), computes local intensity-gradient
// magnitude per pixel via central differences, and divides by the sensor
// noise standard deviation at that pixel's intensity level under
// noiseProfile. This is a physically-grounded per-pixel DIC correlation
// confidence prediction, in place of pattern-only heuristics (e.g.
// autocorrelation/coverage metrics that ignore the actual capturing
// sensor) -- a design-time check, run before fabrication.
//
// Throws std::runtime_error if imagingResolutionPxPerMm or the specimen
// dimensions aren't positive.
UncertaintyMap computeUncertaintyMap(const Pattern &pattern, const SensorNoiseProfile &noiseProfile);

// Fraction of pixels with confidence below minConfidence -- a summary
// number for flagging patterns likely to correlate poorly under this
// sensor's real noise characteristics. minConfidence is caller-supplied:
// what counts as "acceptable" confidence depends on the correlation
// algorithm and required strain accuracy, not a fixed constant.
double lowConfidenceFraction(const UncertaintyMap &map, double minConfidence);

} // namespace patternfab
