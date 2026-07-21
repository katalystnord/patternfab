# PatternFab

Takes an optimized, non-periodic DIC speckle pattern and exports
method-aware physical fabrication files — STL/relief for a 3D-printed stamp
or mold, SVG/DXF for a laser-cut stencil, PNG at correct DPI for
direct-write or decal transfer — with manufacturing constraints (minimum
feature size, bridging, bleed compensation, non-periodicity) baked in.

Built on Qt (GUI, SVG export) and VTK (mesh/relief pipeline, STL export),
as a headless `patternfab-core` library wrapped by a thin `patternfab-gui`
application.

See [CLAUDE.md](CLAUDE.md) for scope, foundation rationale, and the
development path.

## Status

Phase 4 complete — sensor-physics uncertainty: a design-time per-pixel
DIC correlation confidence prediction combining local intensity gradient
with a real camera sensor noise model, on top of Phase 3's export
pipelines. All core functionality now built and tested.

A `patternfab-cli` driver and an end-to-end integration test run a real
sample field (`examples/sample_pattern.json`, a 175-speckle Poisson-disc
pattern) through the whole ingest → constrain → export → uncertainty chain
— the same call sequence the GUI will wrap. GUI (Phase 5) is still an empty
shell.

## License

LGPL-2.1-or-later. See [LICENSE](LICENSE).
