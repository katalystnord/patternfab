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

Phase 2 complete — constraint engine (non-periodicity/tiling check,
minimum-feature-size flagging, bleed compensation) on top of Phase 1's
pattern representation and input paths, tested. GUI is still an empty
shell.

## License

LGPL-2.1-or-later. See [LICENSE](LICENSE).
