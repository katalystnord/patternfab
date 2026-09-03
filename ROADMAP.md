# PatternFab - roadmap

Where the work goes next. This is the one place open items live, so that nothing
has to be carried around as a caveat: a gap found anywhere gets written down
here and then fixed. Nothing in this file belongs in outward-facing text.

Ordered within each section by what unblocks the most.

## Working today

The whole chain, end to end: read a pattern as vector primitives or as a raster,
apply manufacturing constraints (minimum feature size, bleed, bridging, and a
tiling check that refuses to inject periodicity), and write every export -
SVG and DXF for a laser-cut stencil, PNG at a caller-supplied DPI for
direct-write or decal transfer, and STL relief for a 3D-printed stamp or mould.
Plus a design-time sensor-physics uncertainty map, and a GUI with 2D stencil and
VTK 3D relief previews.

## Next

- **Report the design-time uncertainty as a DISPLACEMENT NOISE FLOOR, in
  pixels.** `UncertaintyEngine` computes local intensity gradient over sensor
  noise per pixel: sound physics, but a dimensionless confidence whose threshold
  is caller-supplied, so it has no absolute meaning and cannot be compared with
  anything. SurView's measured figure is DIC's sigma,
  `sqrt(2 * noise^2 / min(sum gx^2, sum gy^2))`, aggregated over the subset and
  carried in pixels of displacement. Same physics, one extra aggregation, and
  the number acquires a unit and a meaning.

  ⚑ **Why this is worth doing even on its own: it closes a loop nobody in the
  field closes.** PatternFab would then state, before anything is fabricated,
  the noise floor a pattern could achieve under a named sensor; SurView states,
  after the run, the noise floor it actually achieved. Both in pixels, both by
  the same definition. The literature is explicit that this is missing -- design
  today rests on "approximated uncertainty models or upon image quality metrics
  that are linked loosely with the actual value of uncertainty", and a 2025
  assessment found none of ten published metrics agreeing substantially with
  measured error. Closed-loop work that does exist closes it in SIMULATION, by
  rendering deformed images; simulation cannot see ink bleed, substrate texture,
  focus, lighting or the printer.

  ⚑ **State it as a BOUND, never as a prediction.** PatternFab renders an ideal
  pattern; SurView photographs a fabricated one. The two will not agree, and
  that is the point: the difference IS the fabrication and imaging penalty,
  which is the quantity nobody currently measures. Claiming to predict what
  SurView will measure would be wrong on the first specimen and would deserve to
  be.

  Not scope drift, though it sits near the line the README draws. PatternFab
  still does not score a pattern in the abstract; this is what a FABRICATED
  artefact can achieve under a real sensor, which is a fabrication question.

- **A cut layer, for stickers whose islands move independently.** A speckle
  applied as a continuous film carries load, and on a soft substrate - elastomer,
  tissue, thin film - it stiffens the specimen and the measurement is partly of
  the coating. Dividing the sticker into islands removes the stiffness coupling.
  The principle is established practice by other means: polymer granules and
  loose powder are already used on ultra-soft hydrated materials, and they work
  because they are naturally discrete. What is not established is the ENGINEERED
  version - a designed, print-then-cut sticker whose island layout is chosen
  rather than accidental.

  This is squarely PatternFab's existing machinery pointed at a second output:
  print-and-cut is how sticker and vinyl cutters already work, and minimum
  feature size, bridging and bleed all apply to a cut path as they do to a
  stencil.

  Three constraints the layout has to respect:

  - ⚑ **The cut layout must be non-periodic too.** A regular grid of cuts
    injects periodicity into a pattern whose whole value is being non-periodic,
    and the existing tiling check is exactly the guard for it. Cuts should
    follow the speckle rather than a lattice.
  - **Islands need enough adhered area not to peel or shear.** The failure this
    invites is an island that reports its own motion rather than the substrate's,
    and nothing downstream can detect that: it correlates beautifully and is
    wrong. Small enough not to reinforce, large enough to stay faithful, and the
    optimum is a real trade rather than a free win.
  - **The measurement side should track islands as MARKERS**, not correlate
    subsets across them. That is SurView's item, recorded in its own roadmap.

- **Fiducials, scale and identity as pattern furniture.** Things printed
  alongside the speckle, outside the region of interest, that the measurement
  needs and that only a true-scale exporter can guarantee:

  - **A scale bar or known-length fiducial pair**, which is what turns pixels
    into millimetres.
  - **ArUco or ChArUco markers**, which give pose, scale and datum together.
    SurView's multi-view work needs exactly this, and OpenCV's detector for them
    is already a dependency there.
  - **An identity code** (a small datamatrix) encoding which pattern this is, so
    a photograph carries its own provenance and SurView can record the pattern a
    field was measured on. That closes the provenance chain one step further
    upstream, into the specimen itself.

- **Calibration targets as a first-class export.** Checkerboards, dot targets
  and ChArUco boards, which SurView's `CameraCalibrator` already detects.

  ⚑ Scoped honestly, because the obvious version is not worth building: drawing
  a checkerboard is trivial and anyone can print one. The value PatternFab adds
  is the part that silently ruins calibrations - **a printer that scales the
  target by two per cent corrupts every intrinsic that follows, and nobody
  checks.** True-scale export and a stated substrate flatness requirement are
  the feature; the geometry is incidental.

## Directions, not yet features

- **A multi-modal sticker**: speckle for deformation, plus layers that report
  something else from the same photograph - pressure-sensitive film in the
  manner of Fujifilm Prescale, a temperature indicator, a chemical or humidity
  colour shift. The attraction is that co-registration is free, because the
  speckle IS the registration target, so traction and displacement would land on
  one map in one coordinate frame.

  Recorded as a direction rather than a feature because the film itself is a
  materials venture rather than a software one, and because three properties
  have to be designed around rather than wished away: an interface film must be
  buried and so cannot be photographed while loaded, which makes it a
  before-and-after workflow; a pressure film is IRREVERSIBLE and INTEGRATING, so
  it records a peak rather than a series, and pairing it with a DIC time series
  risks implying they are contemporaneous; and colour readout fights the
  monochrome cameras DIC rigs prefer. The way through the last of those is
  SurView's second-modality registration item: read the colour layers afterwards
  on a scanner and register that image through the speckle.

  What PatternFab would contribute is the layout: speckle layer, cut layer,
  indicator windows, fiducials, all at true scale on one artefact.

## Test and tooling debt

- **The GUI's uncertainty map** is in `patternfab-core` and `patternfab-cli` and
  is not surfaced in the window.
- **Mutation testing and coverage** are neither run nor tracked, as in SurView.
