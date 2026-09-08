#include "patternfab/NoiseFloorImage.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace patternfab {

namespace {

unsigned char mix(unsigned char a, unsigned char b, double t) {
    return static_cast<unsigned char>(std::lround(a + (b - a) * t));
}

std::string significant(double value, int digits) {
    std::ostringstream out;
    out << std::setprecision(digits) << value;
    return out.str();
}

} // namespace

NoiseFloorScale scaleNoiseFloor(const NoiseFloorSummary &summary) {
    NoiseFloorScale scale;
    if (summary.establishedCount <= 0 || !(summary.bestPx > 0.0)) {
        return scale;
    }

    scale.usable = true;
    scale.lowPx = summary.bestPx;
    // A pattern uniform enough that its best and typical figures coincide has
    // one value, not a range. Left equal rather than widened by an invented
    // factor: every point then sits at the good end, which is what is true.
    scale.highPx = std::max(summary.typicalPx, summary.bestPx);
    scale.highIsExceeded = summary.worstPx > scale.highPx;
    return scale;
}

double noiseFloorRampPosition(const NoiseFloorScale &scale, double sigmaPx) {
    if (!scale.usable || std::isnan(sigmaPx) || !(sigmaPx > 0.0)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (!(scale.highPx > scale.lowPx)) {
        return 0.0;
    }
    const double span = std::log(scale.highPx) - std::log(scale.lowPx);
    const double position = (std::log(sigmaPx) - std::log(scale.lowPx)) / span;
    return std::clamp(position, 0.0, 1.0);
}

Rgb8 noiseFloorColour(double position) {
    const double t = std::clamp(position, 0.0, 1.0) * 2.0;
    // The last stop is reached by the upper segment at t == 2, so the segment
    // index stops at 1: reading kNoiseFloorRampStops[lower + 1] with lower == 2
    // would be off the end of the array.
    const int lower = t < 1.0 ? 0 : 1;
    const double local = t - lower;
    const Rgb8 &a = kNoiseFloorRampStops[lower];
    const Rgb8 &b = kNoiseFloorRampStops[lower + 1];
    return Rgb8{mix(a.r, b.r, local), mix(a.g, b.g, local), mix(a.b, b.b, local)};
}

NoiseFloorPicture drawNoiseFloor(const NoiseFloorMap &map, const NoiseFloorScale &scale) {
    NoiseFloorPicture picture;
    picture.widthPx = map.widthPx;
    picture.heightPx = map.heightPx;
    picture.pixels.resize(map.sigmaPx.size(), kNoFloorColour);

    for (std::size_t i = 0; i < map.sigmaPx.size(); ++i) {
        const double position = noiseFloorRampPosition(scale, map.sigmaPx[i]);
        // ⚑ Absent, not zero. The alternative -- painting an unestablished
        // pixel at position 0 -- puts every hole in the pattern in the colour
        // of its finest ground, so the worst places would read as the best.
        picture.pixels[i] = std::isnan(position) ? kNoFloorColour : noiseFloorColour(position);
    }
    return picture;
}

std::vector<std::string> noiseFloorScaleLabels(const NoiseFloorScale &scale, int count) {
    std::vector<std::string> labels;
    if (!scale.usable || count < 1) {
        return labels;
    }
    if (!(scale.highPx > scale.lowPx)) {
        labels.push_back(significant(scale.lowPx, 3) + " px");
        return labels;
    }

    const double lowLog = std::log(scale.lowPx);
    const double span = std::log(scale.highPx) - lowLog;
    for (int i = 0; i < count; ++i) {
        const double fraction = count == 1 ? 0.0
                                           : static_cast<double>(i) / (count - 1);
        const double value = std::exp(lowLog + span * fraction);
        std::string label = significant(value, 3) + " px";
        if (i == count - 1 && scale.highIsExceeded) {
            label = "> " + label;
        }
        labels.push_back(label);
    }
    return labels;
}

std::string noiseFloorScaleCaption(int subsetRadiusPx, const NoiseFloorScale &scale) {
    std::ostringstream text;
    if (!scale.usable) {
        // ⚑ A verdict, never a blank picture. Nothing established anywhere is a
        // statement about the pattern, and unexplained it reads as a broken
        // tool instead of a pattern that cannot measure.
        text << "None established anywhere at a " << subsetRadiusPx
             << " px subset radius: this pattern has no intensity gradient in "
                "one of the two directions, so it cannot resolve displacement "
                "in that direction at all.";
        return text.str();
    }

    text << "Displacement noise floor at a " << subsetRadiusPx
         << " px subset radius, in pixels; lower is better, unlike the "
            "confidence figure. Grey is where no whole subset fits or where "
            "there is no gradient to correlate, which is an absence rather "
            "than a good reading. Every figure counts gradient across a whole "
            "subset, so a speckle's influence spreads to the width of one and "
            "the picture is blockier than the pattern that made it.";
    if (scale.highIsExceeded) {
        text << " The scale stops at the figure 95% of established points beat; "
                "worse points are painted at its top.";
    }
    return text.str();
}

} // namespace patternfab
