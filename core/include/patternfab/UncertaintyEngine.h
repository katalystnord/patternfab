#pragma once

#include "patternfab/Pattern.h"

#include <string>
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

// --- the displacement noise floor, in pixels --------------------------------
//
// The same physics as the confidence map above, reported as the quantity DIC
// itself uses: sigma, a displacement noise floor in PIXELS.
//
//     sigma = sqrt(2 * noiseVariance / min(sum gx^2, sum gy^2))
//
// summed over the subset a correlation will actually use. This is the figure
// SurView measures after a run, by the same definition, so a design-time number
// and a measured one can be set side by side for the first time. The confidence
// map's dimensionless ratio could not be: its threshold is caller-supplied, so
// it has no absolute meaning and nothing to compare against.
//
// ⚑ IT TAKES THE WORSE AXIS, and that is the whole difference. The confidence
// map takes the gradient MAGNITUDE, so a pattern of bars in one direction --
// enormous gradient across them, none along them -- is rated excellent, when it
// cannot measure displacement along the bars at all. A measurement is as good
// as its weakest direction, which is what min() says.
//
// ⚑ IT IS A BOUND, NOT A PREDICTION. This renders an ideal pattern. A real one
// is printed with real ink on a real substrate and photographed slightly out of
// focus, so what is measured afterwards will be worse -- and the difference
// between the two is the fabrication and imaging penalty, which is the quantity
// nobody currently measures. Reporting this as what a run WILL achieve would be
// wrong on the first specimen.

// The subset radius the tools report at when nobody chooses one. A default,
// not a recommendation: the right radius is the one the measurement will
// actually be run at, which is why every figure carries the radius it used.
inline constexpr int kSubsetRadiusPx = 16;

struct NoiseFloorMap {
    int widthPx = 0;
    int heightPx = 0;

    // The subset this was computed for. Carried with the map because a noise
    // floor quoted without it is not a number anyone can use: the same pattern
    // gives a finer floor to a larger subset.
    int subsetRadiusPx = 0;

    // Row-major, size widthPx*heightPx, in pixels of displacement. Lower is
    // better, which is the opposite reading from the confidence map above.
    //
    // ⚑ NOT-A-NUMBER where no floor could be established, never zero. Zero is
    // the flattering reading -- it claims a perfect measurement -- and the
    // places it would appear are blank regions, which is where a pattern is at
    // its worst.
    std::vector<double> sigmaPx;
};

// `subsetRadiusPx` is the correlation's subset radius, the same setting a
// measurement will be run at. Throws for a non-positive radius, and for the
// same invalid pattern parameters computeUncertaintyMap() rejects.
NoiseFloorMap computeNoiseFloorMap(const Pattern &pattern,
                                   const SensorNoiseProfile &noiseProfile,
                                   int subsetRadiusPx);

struct NoiseFloorSummary {
    double bestPx = 0.0;
    // ⚑ A PERCENTILE, not the maximum. Paid for once already in SurView, where
    // one subset sitting on blank background set the headline figure for an
    // otherwise excellent field. The worst value is reported separately for
    // anyone who wants it, rather than being allowed to speak for the whole.
    double typicalPx = 0.0;
    // Kept for a caller that wants it, and deliberately not a headline. A
    // subset lying almost entirely on blank background still has a trace of
    // gradient, so it is established and its floor is enormous: on the sample
    // pattern the worst is about 945000 px. That is arithmetic, not
    // information, and quoting it would describe a good pattern as a disaster.
    double worstPx = 0.0;
    int establishedCount = 0;
    int totalCount = 0;
};

NoiseFloorSummary summariseNoiseFloor(const NoiseFloorMap &map);

// The summary as a reader should meet it, qualifications included.
//
// In core rather than in a widget for the reason every other rule here is: it
// is where a test can reach it. Three qualifications are load-bearing and none
// of them is optional. The subset radius, because the same pattern gives a
// different figure at another one. That it is a BOUND. And which direction is
// better -- because the confidence map sits beside it on the same screen and
// reads the OTHER way, and a reader carrying the habit across reads a poor
// pattern as a good one.
std::string describeNoiseFloor(const NoiseFloorMap &map,
                               const NoiseFloorSummary &summary);

} // namespace patternfab
