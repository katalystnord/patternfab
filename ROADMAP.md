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
A GUI with 2D stencil and VTK 3D relief previews.

And two design-time uncertainty figures from the same rendering: a per-pixel
confidence map, and **the displacement noise floor in pixels** - DIC's sigma,
by the definition SurView measures after a run, so a designed figure and a
measured one can be set side by side. It is an upper bound rather than a
prediction: the difference between the two is the fabrication and imaging
penalty.

## Next

- **Surface the noise floor in the GUI.** `computeNoiseFloorMap()` and
  `summariseNoiseFloor()` are in `patternfab-core` and reported by
  `patternfab-cli`; the window still shows only the dimensionless confidence
  map. Same gap the confidence map itself has.

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

- **A foil carrier, with the pattern EMBOSSED rather than printed.** For a
  free-standing strip that is itself the measurement medium, rather than a
  coating on somebody's specimen, thin annealed aluminium foil is a strong
  carrier: it yields at a few tenths of a per cent with essentially no in-plane
  springback, which is the property the whole idea rests on, since a strip that
  sprang back would read as a clean null result rather than a failure.

  Two advantages that are easy to miss. Foil is dimensionally stable, where
  paper and polymer absorb moisture and change size between two photographs
  taken days apart -- and a uniform size change is exactly what strain looks
  like. And at 15 to 30 um it is roughly an order of magnitude thinner than
  pressure film, whose two-sheet type runs about 90 um per film; when the thing
  under investigation IS a clearance, the sensor's own thickness is a
  measurement error.

  ⚑ **Embossing collapses the cut-layer problem for this artefact.** A pattern
  pressed into the foil IS the carrier: no coating, so nothing to stiffen,
  delaminate or shear, and no need to divide it into islands at all. The cut
  layer above still matters for a sticker applied to somebody else's specimen;
  it does not apply here. Embossing also survives handling, heat and solvent,
  and it sidesteps ink adhering poorly to bare aluminium.

  **The existing STL relief export is already the tool**, since an embossing die
  is the same object as the 3D-printed stamp it was built for.

  Four things to design around: bright foil is a mirror and specular highlights
  wreck correlation, so the matte side or etched stock; embossed relief reads by
  SHADING and so depends on lighting direction, where etching gives a tonal
  pattern instead; thickness is a real trade, since too thin tears and too thick
  will not conform; and rolled foil is anisotropic with a rolling direction once
  the strain is quantitative.

  A form factor worth borrowing whole: a plaster. Grip the wings, never touch
  the pad -- which is the handling-damage problem solved by shape rather than by
  discipline, and those same wings, having seen no load, ARE the control region
  whose apparent strain is pure handling and reading error. The sealed wrapper
  protects an indicator layer from premature exposure and is the natural place
  for the batch identity and calibration to live.

## Test and tooling debt

- **The GUI's uncertainty map** is in `patternfab-core` and `patternfab-cli` and
  is not surfaced in the window.
- **Mutation testing and coverage** are neither run nor tracked, as in SurView.
