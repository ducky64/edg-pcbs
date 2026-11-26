"""
Fastener holes for 3D printed parts, complementing bd_warehouse.fastener and following its hole conventions:
  - Part objects, usable in BuildPart (at each active location) or algebra mode (as a cutter at the origin)
  - the hole surface is at the local origin, extending into the material toward local -Z
  - depth=None is through the part, in BuildPart only
  - clearance is added to the fastener dimension, so negative is an interference fit
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Literal

from build123d import (Align, BasePartObject, Cylinder, Kind, Mode, Part, Pos, Rectangle, Rot, RotationLike, Sketch,
                       SlotCenterToCenter, extrude, offset)
from bd_warehouse.fastener import ClearanceHole, Nut, Screw

from bd_util._common import hole_depth


@dataclass(frozen=True)
class PrintabilityBridge:
  """Makes a pocket printable without support when it opens downward, toward the build plate.

  A strip the width of the clearance hole is cut across the pocket plan, depth past the pocket bottom. The first roof
  layers bridge across the pocket on either side of the strip, then the next layer bridges across the strip.

  Args:
      depth: past the pocket bottom, typically two layer heights
      angle: strip direction from local +X, counterclockwise. The default runs flat-to-flat on bd_warehouse hex nuts,
          which have corners on +X.
  """
  depth: float = 0.4
  angle: float = 90.0

  def __post_init__(self):
    if self.depth <= 0:
      raise ValueError(f"sacrificial bridge depth {self.depth} must be positive")

  def _make(self, pocket_plan: Sketch, width: float) -> Part:
    """With the pocket bottom at the local origin, extending toward -Z."""
    size = pocket_plan.bounding_box().size
    strip = Rot(0, 0, self.angle) * Rectangle(2 * max(size.X, size.Y), width)
    return extrude(strip & pocket_plan, self.depth, dir=(0, 0, -1))


class CaptiveNutHole(BasePartObject):
  """Clearance hole with a nut pocket at its surface, like the bd_warehouse ClearanceHole captive_nut option.

  Unlike that option, the pocket is the nut plan (rather than a filleted rectangle) and as deep as the nut,
  retaining it flush.

  Args:
      fit: of the shank clearance hole and any sacrificial bridge strip
      depth: of the whole hole including the pocket, None is through the part
      nut_clearance: per side, added to the nut plan. None follows bd_warehouse captive_nut, half the difference
          between the clearance hole and thread diameters.
      bridge: for printing with the pocket opening downward
  """

  def __init__(
      self,
      fastener: Nut,
      fit: Literal["Close", "Normal", "Loose"] = "Normal",
      depth: float | None = None,
      nut_clearance: float | None = None,
      bridge: PrintabilityBridge | None = None,
      rotation: RotationLike = (0, 0, 0),
      mode: Mode = Mode.SUBTRACT,
  ):
    self.fit = fit
    self.bridge = bridge
    self.hole_depth = hole_depth(depth)
    self.pocket_depth = fastener.nut_thickness
    clearance_diameter = fastener.clearance_hole_diameters[fit]
    if nut_clearance is None:
      nut_clearance = (clearance_diameter - fastener.thread_diameter) / 2
    self.nut_clearance = nut_clearance
    roof_depth = self.pocket_depth + (0.0 if bridge is None else bridge.depth)
    if not self.hole_depth > roof_depth:
      raise ValueError(f"depth {self.hole_depth:.3f} must exceed the pocket and bridge depth {roof_depth:.3f}")

    pocket_plan = offset(fastener.nut_plan(), nut_clearance, kind=Kind.INTERSECTION)
    hole_part = extrude(pocket_plan, self.pocket_depth, dir=(0, 0, -1))
    if bridge is not None:
      hole_part += Pos(0, 0, -self.pocket_depth) * bridge._make(pocket_plan, clearance_diameter)
    # explicit depth, since a nested object doesn't see the caller's builder
    hole_part += ClearanceHole(fastener, fit=fit, depth=self.hole_depth, counter_sunk=False, mode=Mode.PRIVATE)

    super().__init__(
      part=hole_part.clean(),
      align=None,
      rotation=rotation,
      mode=mode,
    )


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
