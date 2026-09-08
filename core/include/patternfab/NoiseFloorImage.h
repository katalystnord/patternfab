#pragma once

#include "patternfab/UncertaintyEngine.h"

#include <string>
#include <vector>

namespace patternfab {

// The noise floor drawn as a picture of the specimen, so that WHERE a pattern
// is weak can be seen rather than inferred from a summary figure.
//
// In core rather than in the widget for the reason everything else here is: a
// test can reach it. The rules below are the load-bearing part, and every one
// of them is a rule about not flattering the pattern.

struct Rgb8 {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
};

// ⚑ The colour of a place where NO floor was established, and deliberately not
// a colour on the ramp. A blank corner painted at the good end reads as the
// finest place on the pattern, which is the exact reverse of the truth --
// nothing was measurable there at all. Desaturated on purpose, so that it
// cannot be mistaken for a value on a ramp that is saturated end to end.
inline constexpr Rgb8 kNoFloorColour{0x9a, 0x96, 0x92};

struct NoiseFloorScale {
    // False when the pattern established no floor anywhere. There is then
    // nothing to draw, and the caption carries the verdict instead.
    bool usable = false;

    // The ends of the ramp, in pixels of displacement.
    double lowPx = 0.0;
    double highPx = 0.0;

    // ⚑ True when real points lie beyond the top of the scale. They are painted
    // AT the top, because a subset over blank ground is still a place on the
    // specimen, and the top label says so with a ">" rather than letting a
    // floor a hundred thousand times worse read as merely "the worst colour".
    bool highIsExceeded = false;
};

// ⚑ The scale runs to the TYPICAL figure, not the worst. Ranged to the worst,
// a single subset lying over blank ground -- about 945000 px on the sample
// pattern -- takes the whole ramp and every real value collapses into the first
// colour. Paid for once already in SurView, where one such point made an
// excellent run report "at worst one part in 3".
NoiseFloorScale scaleNoiseFloor(const NoiseFloorSummary &summary);

// Position on the ramp in [0, 1], or not-a-number where no floor was
// established. Clamped at both ends.
//
// ⚑ LOGARITHMIC, because the floor spans decades: on a real pattern the finest
// place is thousandths of a pixel and a subset over blank ground is hundreds of
// thousands. On a linear ramp every usable value lands in the first pixel of
// the scale and the picture answers nothing.
double noiseFloorRampPosition(const NoiseFloorScale &scale, double sigmaPx);

// The ramp itself, good (low sigma) to bad. Position is clamped to [0, 1].
Rgb8 noiseFloorColour(double position);

struct NoiseFloorPicture {
    int widthPx = 0;
    int heightPx = 0;
    // Row-major, one pixel per map pixel, in the map's own row order.
    std::vector<Rgb8> pixels;
};

NoiseFloorPicture drawNoiseFloor(const NoiseFloorMap &map, const NoiseFloorScale &scale);

// Labels for a scale bar drawn with `count` ticks, low end first. Sized in
// SIGNIFICANT digits rather than decimal places: the same rule SurView's field
// scales carry, and for the same reason -- a fixed decimal count prints a floor
// of four thousandths of a pixel as "0.000".
std::vector<std::string> noiseFloorScaleLabels(const NoiseFloorScale &scale, int count);

// One sentence beside the bar. Names the subset radius, because the same
// pattern gives a different floor at another one, and says which direction is
// better, because the confidence figure on the same screen reads the other way.
std::string noiseFloorScaleCaption(int subsetRadiusPx, const NoiseFloorScale &scale);

} // namespace patternfab
