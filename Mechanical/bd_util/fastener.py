"""
Fastener holes for 3D printed parts, complementing bd_warehouse.fastener.

These follow the bd_warehouse.fastener hole conventions:
  - holes are Part objects, usable in BuildPart (placed at each active location, Mode.SUBTRACT by default)
    or in algebra mode (returned as a cutter at the origin, to be positioned and subtracted)
  - the hole surface is at the local origin, and the hole extends into the material toward local -Z
  - depth=None implies through the part (in BuildPart only), for holes that take a depth
  - clearances follow the bd_warehouse sign convention: added to the fastener dimension, so positive is a
    clearance fit and negative is an interference fit
  - hole locations are appended to fastener.hole_locations in BuildPart mode, for placing fasteners later

Holes are built from topology-level operations only, so they don't interact with an enclosing builder.
"""
from __future__ import annotations

from typing import Iterable, Literal

from build123d.build_common import validate_inputs
from build123d.build_enums import Kind, Mode
from build123d.build_part import BuildPart
from build123d.geometry import Plane, Pos, Rot, RotationLike, Vector
from build123d.objects_part import BasePartObject
from build123d.objects_sketch import Rectangle, SlotCenterToCenter
from build123d.topology import Face, Solid
from bd_warehouse.fastener import Nut, Screw

_BORE_PLANE = Plane((0, 0, 0), z_dir=(0, 0, -1))


def _hole_depth(context: BuildPart | None, depth: float | None) -> float:
  """Hole depth, defaulting to through the part in BuildPart mode, as in bd_warehouse."""
  if depth is not None:
    return depth
  elif context is not None:
    return 2 * context.max_dimension
  else:
    raise ValueError("No depth provided")


class CaptiveNutHole(BasePartObject):
  """Part Object: CaptiveNutHole

  A captive nut hole is a pocket shaped to the nut plan (e.g. a hexagon with sharp corners) and exactly as
  deep as the nut, so the nut is retained without rotating and sits flush with the surface. A negative
  nut_clearance gives an interference fit, so the nut is pressed in and held. Unlike the bd_warehouse
  ClearanceHole captive_nut option (a rounded rectangle, combined with the clearance hole), this is only the
  pocket - combine with a ClearanceHole for the screw.

  Args:
      fastener (Nut): A nut instance with a polygonal nut plan, e.g. HexNut or SquareNut
      nut_clearance (float, optional): clearance added to each side of the nut plan, negative for an
          interference fit. Defaults to 0.0.
      rotation (RotationLike, optional): object rotation. Defaults to (0, 0, 0).
      mode (Mode, optional): combination mode. Defaults to Mode.SUBTRACT.
  """

  _applies_to = [BuildPart._tag]

  def __init__(
      self,
      fastener: Nut,
      nut_clearance: float = 0.0,
      rotation: RotationLike = (0, 0, 0),
      mode: Mode = Mode.SUBTRACT,
  ):
    context: BuildPart | None = BuildPart._get_context(self)
    validate_inputs(context, self)

    self.pocket_depth = fastener.nut_thickness
    hole_part = self._make_hole(fastener, nut_clearance)

    if context is not None:  # nut seated at the bottom of the pocket
      fastener.hole_locations.extend(
        [location * Pos(0, 0, -self.pocket_depth) for location in self._get_object_locations()])

    super().__init__(
      part=hole_part.clean(),
      align=None,
      rotation=rotation,
      mode=mode,
    )

  def _nut_pocket_plan(self, nut: Nut, nut_clearance: float) -> Face:
    """Nut plan grown by nut_clearance per side, keeping sharp corners."""
    with BuildPart(mode=Mode.PRIVATE):  # nut_plan() uses a builder; keep it out of any enclosing context
      nut_plan = nut.nut_plan()
    return Face(nut_plan.outer_wire().offset_2d(nut_clearance, kind=Kind.INTERSECTION))

  def _make_hole(self, fastener: Nut, nut_clearance: float) -> Solid:
    """The hole cutter, in the hole convention."""
    return Solid.extrude(self._nut_pocket_plan(fastener, nut_clearance), Vector(0, 0, -self.pocket_depth))


class PrintableCaptiveNutHole(CaptiveNutHole):
  """Part Object: PrintableCaptiveNutHole

  A CaptiveNutHole that prints without support when the pocket opens downward (pocket roof printed as a
  bridge). A strip as wide as the clearance hole is cut across the entire nut plan, bridge_depth past the
  pocket bottom, so the first roof layers bridge across the pocket on either side of the strip, uninterrupted
  by the clearance hole. The layer above the strip then bridges perpendicularly across the strip, leaving the
  clearance hole. Combine with a ClearanceHole of the same fit for the screw.

  Args:
      fastener (Nut): A nut instance with a polygonal nut plan, e.g. HexNut or SquareNut
      fit (Literal["Close", "Normal", "Loose"], optional): Control strip (clearance hole) width.
          Defaults to "Normal".
      nut_clearance (float, optional): clearance added to each side of the nut plan, negative for an
          interference fit. Defaults to 0.0.
      bridge_depth (float, optional): depth of the strip past the pocket bottom, typically two layer heights.
          Defaults to 0.4.
      bridge_angle (float, optional): strip direction from the nut plan local +X axis, counterclockwise, in
          degrees. The default runs the strip flat-to-flat on bd_warehouse hex nuts, which have corners on +X.
          Defaults to 90.0.
      rotation (RotationLike, optional): object rotation. Defaults to (0, 0, 0).
      mode (Mode, optional): combination mode. Defaults to Mode.SUBTRACT.
  """

  def __init__(
      self,
      fastener: Nut,
      fit: Literal["Close", "Normal", "Loose"] = "Normal",
      nut_clearance: float = 0.0,
      bridge_depth: float = 0.4,
      bridge_angle: float = 90.0,
      rotation: RotationLike = (0, 0, 0),
      mode: Mode = Mode.SUBTRACT,
  ):
    self.fit = fit
    self.bridge_depth = bridge_depth
    self.bridge_angle = bridge_angle
    super().__init__(fastener, nut_clearance=nut_clearance, rotation=rotation, mode=mode)

  def _make_hole(self, fastener: Nut, nut_clearance: float) -> Solid:
    pocket_plan = self._nut_pocket_plan(fastener, nut_clearance)
    strip_length = 2 * max(pocket_plan.bounding_box().size.X, pocket_plan.bounding_box().size.Y)
    strip = Rot(0, 0, self.bridge_angle) * Rectangle(strip_length, fastener.clearance_hole_diameters[self.fit]).face()
    bridge_plan = strip.intersect(pocket_plan).faces()[0]
    bridge = Pos(0, 0, -self.pocket_depth) * Solid.extrude(bridge_plan, Vector(0, 0, -self.bridge_depth))
    return super()._make_hole(fastener, nut_clearance).fuse(bridge)


class KeyholeHole(BasePartObject):
  """Part Object: KeyholeHole

  A keyhole hole hangs a part on screws protruding from a surface: the screw head passes through a hole
  at the origin the size of the screw head, then the part slides so the shank moves along a slot narrower
  than the head, capturing it. The slot ends with the shank just clear of the head hole, so the head bears
  on the slot edges. Multiple slot angles allow hanging the part in different orientations.

  Args:
      fastener (Screw): A screw instance, for the head and clearance hole diameters
      fit (Literal["Close", "Normal", "Loose"], optional): Control slot (clearance hole) width.
          Defaults to "Normal".
      depth (float, optional): hole depth - None implies through part. Defaults to None.
      angles (Iterable[float], optional): slot directions from the local +X axis, counterclockwise,
          in degrees. Defaults to (0,).
      rotation (RotationLike, optional): object rotation. Defaults to (0, 0, 0).
      mode (Mode, optional): combination mode. Defaults to Mode.SUBTRACT.

  Raises:
      ValueError: No depth provided
  """

  _applies_to = [BuildPart._tag]

  def __init__(
      self,
      fastener: Screw,
      fit: Literal["Close", "Normal", "Loose"] = "Normal",
      depth: float | None = None,
      angles: Iterable[float] = (0,),
      rotation: RotationLike = (0, 0, 0),
      mode: Mode = Mode.SUBTRACT,
  ):
    context: BuildPart | None = BuildPart._get_context(self)
    validate_inputs(context, self)

    self.hole_depth = _hole_depth(context, depth)
    # nominal head diameter; Screw.head_diameter is measured from the model and can be smaller (e.g. countersunk)
    head_diameter = fastener.screw_data.get("dk", fastener.head_diameter)
    slot_width = fastener.clearance_hole_diameters[fit]
    self.slot_length = head_diameter / 2 + slot_width / 2  # origin to slot end center

    hole_part = Solid.make_cylinder(head_diameter / 2, self.hole_depth, plane=_BORE_PLANE)
    slot_plan = Pos(self.slot_length / 2, 0) * SlotCenterToCenter(self.slot_length, slot_width).face()
    slot = Solid.extrude(slot_plan, Vector(0, 0, -self.hole_depth))
    for angle in angles:
      hole_part = hole_part.fuse(Rot(0, 0, angle) * slot)

    if context is not None:
      fastener.hole_locations.extend(self._get_object_locations())

    super().__init__(
      part=hole_part.clean(),
      align=None,
      rotation=rotation,
      mode=mode,
    )
