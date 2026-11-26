"""
Fastener holes for 3D printed parts, complementing bd_warehouse.fastener and following its hole conventions:
  - Part objects, usable in BuildPart (at each active location) or algebra mode (as a cutter at the origin)
  - the hole surface is at the local origin, extending into the material toward local -Z
  - depth=None is through the part, in BuildPart only
  - clearance is added to the fastener dimension, so negative is an interference fit
"""
from __future__ import annotations

from typing import Iterable, Literal

from build123d import (Align, BasePartObject, Cylinder, Kind, Mode, Part, Pos, Rectangle, Rot, RotationLike, Sketch,
                       SlotCenterToCenter, extrude, offset)
from bd_warehouse.fastener import Nut, Screw

from bd_util._common import hole_depth


class CaptiveNutHole(BasePartObject):
  """Pocket shaped to the nut plan with sharp corners and as deep as the nut, retaining it flush and unrotating.

  Unlike the bd_warehouse ClearanceHole captive_nut option, this is only the pocket: combine with a ClearanceHole.
  """

  def __init__(
      self,
      fastener: Nut,
      nut_clearance: float = 0.0,
      rotation: RotationLike = (0, 0, 0),
      mode: Mode = Mode.SUBTRACT,
  ):
    self.pocket_depth = fastener.nut_thickness
    super().__init__(
      part=self._make_hole(fastener, nut_clearance).clean(),
      align=None,
      rotation=rotation,
      mode=mode,
    )

  def _nut_pocket_plan(self, nut: Nut, nut_clearance: float) -> Sketch:
    return offset(nut.nut_plan(), nut_clearance, kind=Kind.INTERSECTION)

  def _make_hole(self, fastener: Nut, nut_clearance: float) -> Part:
    return extrude(self._nut_pocket_plan(fastener, nut_clearance), self.pocket_depth, dir=(0, 0, -1))


class PrintableCaptiveNutHole(CaptiveNutHole):
  """CaptiveNutHole printable without support when the pocket opens downward.

  A strip the width of the clearance hole is cut across the nut plan, bridge_depth past the pocket bottom. The first
  roof layers bridge across the pocket on either side of the strip, then the next layer bridges across the strip.
  Combine with a ClearanceHole of the same fit.

  Args:
      bridge_depth: typically two layer heights
      bridge_angle: strip direction from local +X, counterclockwise. The default runs flat-to-flat on bd_warehouse
          hex nuts, which have corners on +X.
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

  def _make_hole(self, fastener: Nut, nut_clearance: float) -> Part:
    pocket_plan = self._nut_pocket_plan(fastener, nut_clearance)
    strip_length = 2 * max(pocket_plan.bounding_box().size.X, pocket_plan.bounding_box().size.Y)
    strip = Rot(0, 0, self.bridge_angle) * Rectangle(strip_length, fastener.clearance_hole_diameters[self.fit])
    bridge = Pos(0, 0, -self.pocket_depth) * extrude(strip & pocket_plan, self.bridge_depth, dir=(0, 0, -1))
    return super()._make_hole(fastener, nut_clearance) + bridge


class KeyholeHole(BasePartObject):
  """Hangs a part on protruding screws: the head passes through a hole at the origin, then the shank slides along a
  narrower slot, ending just clear of the head hole so the head bears on the slot edges.

  Args:
      angles: slot directions from local +X, counterclockwise
  """

  def __init__(
      self,
      fastener: Screw,
      fit: Literal["Close", "Normal", "Loose"] = "Normal",
      depth: float | None = None,
      angles: Iterable[float] = (0,),
      rotation: RotationLike = (0, 0, 0),
      mode: Mode = Mode.SUBTRACT,
  ):
    self.hole_depth = hole_depth(depth)
    # nominal, since Screw.head_diameter is measured from the model and can be smaller (e.g. countersunk)
    head_diameter = fastener.screw_data.get("dk", fastener.head_diameter)
    slot_width = fastener.clearance_hole_diameters[fit]
    self.slot_length = head_diameter / 2 + slot_width / 2  # origin to slot end center

    head_hole = Cylinder(head_diameter / 2, self.hole_depth, align=(Align.CENTER, Align.CENTER, Align.MAX))
    slot_plan = Pos(self.slot_length / 2, 0) * SlotCenterToCenter(self.slot_length, slot_width)
    slot = extrude(slot_plan, self.hole_depth, dir=(0, 0, -1))
    hole_part = head_hole + [Rot(0, 0, angle) * slot for angle in angles]

    super().__init__(
      part=hole_part.clean(),
      align=None,
      rotation=rotation,
      mode=mode,
    )
