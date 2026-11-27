Parametric enclosures in `build123d`, plus `bd_util`, a library of reusable features that complements `bd_warehouse`.


## Modeling style

- Use the `build123d` public API (top-level package).
- Algebra mode is the default for primitive operations:
  - build geometry with objects: `Box`, `Cylinder`, `Rectangle`, ...)
  - use operations: (`extrude`, `loft`, `offset`, ...) and operators (`+`, `-`, `&`, `Pos(...) *`).
  - direct topology API (`Solid.extrude`, `.fuse`, `.cut`, `Face(wire.offset_2d(...))`) is not used
- Library generators return `Part` (solids) or `Sketch` (2d profiles).
- Conventions
  - `extrude(profile, amount)` extruding along the face normal (+Z for sketches in XY)
    - `dir=(0, 0, -1)` for cuts into the material
    - `taper=` for chamfers
  - Holes follow bd_warehouse's `ClearanceHole`:
    - Hole surface is at the local origin, cutting towards local -Z.
    - Parameters follow bd_warehouse naming, order, and conventions.
    - `depth=None` is through-all cut.


## Code style

- Shared parameters are at the top of the file.
  - Part-specific parameters are in their generator functions.
- Top-level parts are constructed in global coordinates, and combining them all produces the final assembly. 
- Library generators do not define magic constants, though may provide sensible defaults where widely applicable.
- Optional sub-features with their own parameters are dataclasses passed as `feature: Feature | None = None`
  - Examples: `Flange`, `CrushRibs`, `PrintabilityBridge`
  - These build geometry in a private `_make(...)` relative to its parents' local origin.
  - These may have basic validations in `__post_init__`
