// The noise floor as a PICTURE rather than a pair of numbers.
//
// The summary says how good the floor is; a reader's next question is WHERE,
// because a weak region along one edge is a layout problem and weakness
// scattered evenly is a speckle-size problem, and the two are fixed by
// different changes. This is the same argument SurView's repair map is built
// on, one tool along.
//
// Negative checks, recorded per the regime -- each rule was removed and the
// named case watched go red:
//
//   absent-is-not-best      painting a not-established pixel at position 0.0
//                           instead of the absent colour: three cases went red,
//                           reporting the blank corner in the SAME colour as
//                           the finest place on the pattern.
//   the-log-ramp            a linear ramp put the geometric mean of a scale
//                           running 0.001 to 1000 px at 0.001 of the way along
//                           rather than at the middle, which is the whole
//                           picture in one colour.
//   the-clamped-top         dropping highIsExceeded let a floor a hundred
//                           thousand times worse than the scale's top read as
//                           merely "the worst colour", label and all.
//   significant-digits      a fixed 3 decimal places printed a floor of four
//                           thousandths of a pixel as "0.004" only by luck and
//                           the good end of a finer scale as "0.000"; the case
//                           went red on the label.
//
// One thing these cases do NOT catch, stated because a green suite must not be
// read as more than it is: they say nothing about whether the ramp's colours
// are distinguishable to a reader. That was checked by looking at the window.

#include <patternfab/NoiseFloorImage.h>

#include <cmath>
#include <limits>
#include <vector>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        ++failures;
    }
}

patternfab::NoiseFloorSummary summaryOf(double best, double typical, double worst,
                                        int established, int total) {
    patternfab::NoiseFloorSummary s;
    s.bestPx = best;
    s.typicalPx = typical;
    s.worstPx = worst;
    s.establishedCount = established;
    s.totalCount = total;
    return s;
}

void a_point_with_no_floor_is_absent_from_the_picture_not_the_best_place_on_it() {
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.01, 1.0, 945000.0, 900, 1000));

    const double absent = patternfab::noiseFloorRampPosition(
        scale, std::numeric_limits<double>::quiet_NaN());
    check(std::isnan(absent),
          "a not-established floor has no position on the ramp");

    patternfab::NoiseFloorMap map;
    map.widthPx = 2;
    map.heightPx = 1;
    map.subsetRadiusPx = 16;
    map.sigmaPx = {0.01, std::numeric_limits<double>::quiet_NaN()};

    const patternfab::NoiseFloorPicture picture = patternfab::drawNoiseFloor(map, scale);
    check(picture.widthPx == 2 && picture.heightPx == 1 && picture.pixels.size() == 2,
          "the picture has one pixel per map pixel");

    const patternfab::Rgb8 best = picture.pixels[0];
    const patternfab::Rgb8 nothing = picture.pixels[1];
    check(!(best.r == nothing.r && best.g == nothing.g && best.b == nothing.b),
          "a place that established nothing is not painted as the finest place on the pattern");
    check(nothing.r == patternfab::kNoFloorColour.r
              && nothing.g == patternfab::kNoFloorColour.g
              && nothing.b == patternfab::kNoFloorColour.b,
          "a place that established nothing takes the absent colour");
}

void the_ramp_is_logarithmic_because_the_floor_spans_decades() {
    // A real pattern's established floors run from about 0.004 px on a good
    // edge to hundreds of thousands on a subset lying over blank ground. On a
    // linear ramp every useful value lands in the first pixel of the scale.
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.001, 1000.0, 945000.0, 900, 1000));

    const double middle = patternfab::noiseFloorRampPosition(scale, std::sqrt(0.001 * 1000.0));
    check(std::abs(middle - 0.5) < 1e-9,
          "the geometric mean of the scale's ends sits at the middle of the ramp");

    const double low = patternfab::noiseFloorRampPosition(scale, 0.001);
    const double high = patternfab::noiseFloorRampPosition(scale, 1000.0);
    check(std::abs(low) < 1e-9 && std::abs(high - 1.0) < 1e-9,
          "the scale's own ends sit at the ends of the ramp");
}

void the_scale_ends_at_the_typical_figure_and_says_the_worst_is_beyond_it() {
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.01, 1.0, 945000.0, 900, 1000));

    check(scale.usable, "a pattern with established points has a usable scale");
    check(scale.lowPx == 0.01 && scale.highPx == 1.0,
          "the scale runs from the best point to the typical one, never to the worst");
    check(scale.highIsExceeded,
          "a floor worse than the scale's top is declared, not silently clamped into it");

    // Clamped rather than off the ramp: a subset over blank ground is a real
    // place on the pattern and must still be painted, at the bad end.
    const double beyond = patternfab::noiseFloorRampPosition(scale, 945000.0);
    check(std::abs(beyond - 1.0) < 1e-9,
          "a floor beyond the scale's top is painted at the top, not left blank");

    const std::string caption = patternfab::noiseFloorScaleCaption(16, scale);
    check(caption.find("16") != std::string::npos,
          "the caption names the subset radius the figure belongs to");
    check(caption.find("lower is better") != std::string::npos,
          "the caption says which direction is better");
    // ⚑ Because the picture does not look like the pattern, and the first
    // question a reader has is why: a round speckle prints as a rounded square
    // the width of the subset, since every figure counts gradient across one.
    // Seen the moment the window was first opened on a real pattern.
    check(caption.find("subset") != std::string::npos
              && caption.find("blockier") != std::string::npos,
          "the caption explains why the picture is blockier than the pattern");
}

void the_scale_labels_carry_significant_digits_not_decimal_places() {
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.0041, 1.0, 945000.0, 900, 1000));
    const std::vector<std::string> labels = patternfab::noiseFloorScaleLabels(scale, 4);

    check(labels.size() == 4, "the scale draws the number of labels asked for");
    check(labels.front().find("0.0041") != std::string::npos,
          "a floor of four thousandths of a pixel is not rounded away to 0.000");
    check(labels.back().find(">") != std::string::npos,
          "the top label says the worst point lies beyond it");
}

void a_pattern_that_established_nothing_has_no_picture_and_says_why() {
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.0, 0.0, 0.0, 0, 1000));
    check(!scale.usable, "a pattern that established nothing has no usable scale");

    patternfab::NoiseFloorMap map;
    map.widthPx = 1;
    map.heightPx = 1;
    map.subsetRadiusPx = 16;
    map.sigmaPx = {std::numeric_limits<double>::quiet_NaN()};

    const patternfab::NoiseFloorPicture picture = patternfab::drawNoiseFloor(map, scale);
    check(picture.pixels.size() == 1
              && picture.pixels[0].r == patternfab::kNoFloorColour.r,
          "every pixel of a pattern that measured nothing is absent, never a colour");

    const std::string caption = patternfab::noiseFloorScaleCaption(16, scale);
    check(caption.find("none") != std::string::npos || caption.find("None") != std::string::npos,
          "the caption states the verdict rather than leaving the picture blank and unexplained");
}

void the_two_ends_of_the_ramp_are_different_colours() {
    const patternfab::Rgb8 good = patternfab::noiseFloorColour(0.0);
    const patternfab::Rgb8 bad = patternfab::noiseFloorColour(1.0);
    check(!(good.r == bad.r && good.g == bad.g && good.b == bad.b),
          "the finest and the coarsest place on the pattern are not the same colour");
    check(!(good.r == patternfab::kNoFloorColour.r && good.g == patternfab::kNoFloorColour.g
            && good.b == patternfab::kNoFloorColour.b)
              && !(bad.r == patternfab::kNoFloorColour.r
                   && bad.g == patternfab::kNoFloorColour.g
                   && bad.b == patternfab::kNoFloorColour.b),
          "neither end of the ramp collides with the colour meaning nothing was established");
}

} // namespace

int main() {
    a_point_with_no_floor_is_absent_from_the_picture_not_the_best_place_on_it();
    the_ramp_is_logarithmic_because_the_floor_spans_decades();
    the_scale_ends_at_the_typical_figure_and_says_the_worst_is_beyond_it();
    the_scale_labels_carry_significant_digits_not_decimal_places();
    a_pattern_that_established_nothing_has_no_picture_and_says_why();
    the_two_ends_of_the_ramp_are_different_colours();

    if (failures == 0) {
        std::cout << "test_noise_floor_image: all cases passed" << std::endl;
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
