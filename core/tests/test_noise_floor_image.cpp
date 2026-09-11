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
//
// Two mutants in this file survive on purpose, recorded so that nobody hunts
// them twice (measured 2026-09-09: 36 of 38 viable mutants killed):
//
//   noiseFloorColour, `t < 1.0` -> `t <= 1.0`   EQUIVALENT. At t exactly 1 the
//     two readings are the upper stop either way -- mixed from below at
//     local 1, or from above at local 0. There is no input that tells them
//     apart, so there is no case to write.
//   drawNoiseFloor, `i < size()` -> `i <= size()`  A read one past the end of
//     both vectors. It is a real defect and no assertion can see it, because
//     the value read is whatever happens to sit there and the picture is
//     unchanged. It wants the suite built under a sanitizer, which is on the
//     roadmap rather than pretended at here.

// ⚑ TWO SURVIVORS HERE ARE CLOSED BY ARGUMENT (2026-09-11). The ramp's segment
// index (`t < 1.0` widened to "<=") picks the lower segment at exactly the
// junction and interpolates all the way to its far stop - which IS the upper
// segment's near stop, the same colour, because a ramp is continuous where its
// segments meet. And the pixel loop widened to "<=" reads one element past the
// end of the map, which is undefined behaviour rather than an assertion.

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


bool sameColour(const patternfab::Rgb8 &a, const patternfab::Rgb8 &b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

// Everything below this line was written to kill mutants that survived the
// cases above: the suite ran this code and checked almost nothing about it.
// 26 mutants in this file survived the first sweep, and the two worth naming
// are an out-of-bounds read (kNoiseFloorRampStops[lower + 1] with lower left
// free to reach 2) and an interpolation that could run backwards, both of
// which the end-point checks above are blind to by construction.

void the_ramp_passes_through_its_stops_and_not_past_them() {
    check(sameColour(patternfab::noiseFloorColour(0.0), patternfab::kNoiseFloorRampStops[0]),
          "the good end of the ramp is the first stop");
    check(sameColour(patternfab::noiseFloorColour(0.5), patternfab::kNoiseFloorRampStops[1]),
          "the middle of the ramp is the middle stop");
    check(sameColour(patternfab::noiseFloorColour(1.0), patternfab::kNoiseFloorRampStops[2]),
          "the bad end of the ramp is the last stop, and reading one further would be off the end");

    // Interpolated, not stepped: a quarter of the way along is between the
    // first two stops and equal to neither.
    const patternfab::Rgb8 quarter = patternfab::noiseFloorColour(0.25);
    check(!sameColour(quarter, patternfab::kNoiseFloorRampStops[0])
              && !sameColour(quarter, patternfab::kNoiseFloorRampStops[1]),
          "a position between two stops takes a colour between them");
    check(quarter.g > patternfab::kNoiseFloorRampStops[0].g
              && quarter.g < patternfab::kNoiseFloorRampStops[1].g,
          "the interpolation runs from the lower stop towards the upper one, not away from it");

    // Clamped, so a position outside [0, 1] is still a colour on the ramp.
    check(sameColour(patternfab::noiseFloorColour(-3.0), patternfab::kNoiseFloorRampStops[0])
              && sameColour(patternfab::noiseFloorColour(4.0),
                            patternfab::kNoiseFloorRampStops[2]),
          "a position outside the ramp is clamped to its ends");
}

void nothing_beyond_the_scale_means_no_greater_than_on_its_top_label() {
    // ⚑ The worst point sitting exactly ON the scale's top is not beyond it.
    // Declared anyway, every scale would carry a ">" and the mark would stop
    // meaning anything.
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.01, 1.0, 1.0, 900, 1000));
    check(!scale.highIsExceeded,
          "a worst point equal to the scale's top does not exceed it");

    const std::vector<std::string> labels = patternfab::noiseFloorScaleLabels(scale, 4);
    for (const std::string &label : labels) {
        check(label.find(">") == std::string::npos,
              "no label claims values beyond a scale nothing exceeds");
    }
}

void only_the_top_label_says_the_worst_lies_beyond_it() {
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.01, 1.0, 945000.0, 900, 1000));
    const std::vector<std::string> labels = patternfab::noiseFloorScaleLabels(scale, 4);
    for (std::size_t i = 0; i + 1 < labels.size(); ++i) {
        check(labels[i].find(">") == std::string::npos,
              "a label below the top of the scale is an exact value, not a bound");
    }
    check(labels.back().find(">") != std::string::npos,
          "the top label is the one that carries the bound");
}

void the_labels_span_the_scale_they_belong_to() {
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.01, 100.0, 945000.0, 900, 1000));
    const std::vector<std::string> labels = patternfab::noiseFloorScaleLabels(scale, 5);
    check(labels.size() == 5, "five ticks give five labels");
    check(labels.front().find("0.01") != std::string::npos,
          "the first label is the scale's own low end");
    check(labels.back().find("100") != std::string::npos,
          "the last label is the scale's own high end, not somewhere short of it");
    // Logarithmic, so the middle of five ticks over 0.01 to 100 is 1.
    check(labels[2].find("1 px") != std::string::npos,
          "the ticks are spaced on the ramp the colours use, not on a linear axis");

    const std::vector<std::string> one = patternfab::noiseFloorScaleLabels(scale, 1);
    check(one.size() == 1, "a single tick is a single label, not none and not a division by zero");
}

void a_floor_of_exactly_zero_is_an_absence_not_the_finest_reading() {
    // ⚑ Strictly positive, not merely non-negative. A noise floor of zero
    // claims a perfect measurement, which is not reachable, so a zero is a
    // value that was never established -- the same rule SurView applies to
    // sigma and beta, and it reads as the FLATTERING answer if it slips
    // through, since zero paints at the good end of the ramp.
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.01, 1.0, 945000.0, 900, 1000));
    check(std::isnan(patternfab::noiseFloorRampPosition(scale, 0.0)),
          "a floor of exactly zero has no position on the ramp");
    check(std::isnan(patternfab::noiseFloorRampPosition(scale, -1.0)),
          "a negative floor has no position on the ramp");
}

void a_summary_that_established_nothing_is_unusable_whatever_else_it_says() {
    // The count is what decides it. A summary carrying a plausible best figure
    // with no established points behind it is not a scale with one good pixel
    // on it; it is nothing to draw.
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.5, 2.0, 3.0, 0, 1000));
    check(!scale.usable,
          "no established points means no scale, whatever figures accompany them");
}

void an_unusable_scale_answers_nothing_rather_than_something() {
    // Every guard here is an OR of independent refusals, and each has to be
    // able to refuse on its own: turned into an AND, an unusable scale with a
    // perfectly ordinary sigma in hand starts answering questions about it.
    const patternfab::NoiseFloorScale nothing =
        patternfab::scaleNoiseFloor(summaryOf(0.0, 0.0, 0.0, 0, 1000));

    check(std::isnan(patternfab::noiseFloorRampPosition(nothing, 0.5)),
          "a scale with nothing on it gives no position to a perfectly good figure");
    check(patternfab::noiseFloorScaleLabels(nothing, 4).empty(),
          "a scale with nothing on it draws no labels");
}

void a_best_figure_of_exactly_zero_is_not_a_scale_to_draw() {
    // ⚑ The same strictly-positive rule as the ramp position, one level up. A
    // best of exactly zero is a floor that was never established, so a summary
    // reporting one has nothing to anchor the good end of its scale to.
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.0, 1.0, 2.0, 900, 1000));
    check(!scale.usable, "a best figure of exactly zero does not make a usable scale");
}

void a_pattern_of_one_single_value_is_all_at_the_good_end() {
    // Best and typical coincide on a pattern uniform enough to have one figure.
    // That is a scale with no extent, not a scale to divide by: every point is
    // the best point, and there is one label rather than a row of identical
    // ones.
    const patternfab::NoiseFloorScale scale =
        patternfab::scaleNoiseFloor(summaryOf(0.02, 0.02, 0.02, 900, 1000));
    check(scale.usable, "one repeated figure is still a figure");

    const double position = patternfab::noiseFloorRampPosition(scale, 0.02);
    check(!std::isnan(position) && std::abs(position) < 1e-9,
          "with no extent to the scale every point sits at its good end");

    const std::vector<std::string> labels = patternfab::noiseFloorScaleLabels(scale, 4);
    check(labels.size() == 1 && labels.front().find("0.02") != std::string::npos,
          "a scale of one value carries that value once, not four ticks of the same number");
}
} // namespace

int main() {
    a_point_with_no_floor_is_absent_from_the_picture_not_the_best_place_on_it();
    the_ramp_is_logarithmic_because_the_floor_spans_decades();
    the_scale_ends_at_the_typical_figure_and_says_the_worst_is_beyond_it();
    the_scale_labels_carry_significant_digits_not_decimal_places();
    a_pattern_that_established_nothing_has_no_picture_and_says_why();
    the_two_ends_of_the_ramp_are_different_colours();
    the_ramp_passes_through_its_stops_and_not_past_them();
    nothing_beyond_the_scale_means_no_greater_than_on_its_top_label();
    only_the_top_label_says_the_worst_lies_beyond_it();
    the_labels_span_the_scale_they_belong_to();
    a_floor_of_exactly_zero_is_an_absence_not_the_finest_reading();
    a_summary_that_established_nothing_is_unusable_whatever_else_it_says();
    an_unusable_scale_answers_nothing_rather_than_something();
    a_best_figure_of_exactly_zero_is_not_a_scale_to_draw();
    a_pattern_of_one_single_value_is_all_at_the_good_end();

    if (failures == 0) {
        std::cout << "test_noise_floor_image: all cases passed" << std::endl;
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
