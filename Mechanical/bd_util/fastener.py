"""
Fastener holes for 3D printed parts, complementing bd_warehouse.fastener and following its hole conventions:
  - Part objects, usable in BuildPart (at each active location) or algebra mode (as a cutter at the origin)
  - the hole surface is at the local origin, extending into the material toward local -Z
  - depth=None is through the part, in BuildPart only
  - clearance is added to the fastener dimension, so negative is an interference fit
"""

from __future__ import annotations

from dataclasses import dataclass
from math import radians, sqrt, tan
from typing import Iterable, Literal

from build123d import (
    Align,
    BasePartObject,
    Circle,
    Cylinder,
    Kind,
    Mode,
    Part,
    Pos,
    Rectangle,
    Rot,
    RotationLike,
    Sketch,
    SlotCenterToCenter,
    extrude,
    loft,
    offset,
)
from bd_warehouse.fastener import ClearanceHole, Nut, Screw

from bd_util._common import hole_depth


@dataclass(frozen=True)
class PrintabilityBridge:
    """Makes a pocket printable without support when it opens downward, toward the build plate, by stepping from the
    pocket to the clearance hole so that every bridge is straight and anchored at both ends:
      1. a strip the width of the clearance hole across the pocket plan, one layer: the first roof layer bridges across
         the pocket on either side of the strip
      2. a square of the clearance hole width, one layer: this layer bridges across the strip on either side
      3. a ruled loft from the square to the clearance hole: the corners overhang at overhang_angle, without bridging

    Args:
        layer_height: of steps 1 and 2, which should match the print layers
        angle: strip direction from local +X, counterclockwise. The default runs flat-to-flat on bd_warehouse hex nuts,
            which have corners on +X.
        overhang_angle: of the loft corners, from the hole axis
    """

    layer_height: float = 0.2
    angle: float = 90.0
    overhang_angle: float = 45.0

    def __post_init__(self):
        if self.layer_height <= 0:
            raise ValueError(f"bridge layer_height {self.layer_height} must be positive")
        if not 0 < self.overhang_angle < 90:
            raise ValueError(f"bridge overhang_angle {self.overhang_angle} must be within (0, 90)")

    def taper_height(self, width: float) -> float:
        """Of the loft, from the square corners in to the clearance hole of diameter width."""
        return (sqrt(2) - 1) * width / 2 / tan(radians(self.overhang_angle))

    def depth(self, width: float) -> float:
        """Past the pocket bottom, for a clearance hole of diameter width."""
        return 2 * self.layer_height + self.taper_height(width)

    def _make(self, pocket_plan: Sketch, pocket_depth: float, width: float) -> Part:
        """In the hole frame, extending down (toward -Z) from the pocket bottom at -pocket_depth."""
        size = pocket_plan.bounding_box().size
        strip = Rot(0, 0, self.angle) * Rectangle(2 * max(size.X, size.Y), width)
        square = Rot(0, 0, self.angle) * Rectangle(width, width)
        bridge = Pos(0, 0, -pocket_depth) * extrude(strip & pocket_plan, self.layer_height, dir=(0, 0, -1))
        bridge += Pos(0, 0, -pocket_depth - self.layer_height) * extrude(square, self.layer_height, dir=(0, 0, -1))
        taper_top = Pos(0, 0, -pocket_depth - 2 * self.layer_height)
        bridge += loft(
            [
                taper_top * square,
                taper_top * Pos(0, 0, -self.taper_height(width)) * Circle(width / 2),
            ],
            ruled=True,
        )
        return bridge


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
        roof_depth = self.pocket_depth + (0.0 if bridge is None else bridge.depth(clearance_diameter))
        if not self.hole_depth > roof_depth:
            raise ValueError(f"depth {self.hole_depth:.3f} must exceed the pocket and bridge depth {roof_depth:.3f}")

        pocket_plan = offset(fastener.nut_plan(), nut_clearance, kind=Kind.INTERSECTION)
        hole_part = extrude(pocket_plan, self.pocket_depth, dir=(0, 0, -1))
        if bridge is not None:
            hole_part += bridge._make(pocket_plan, self.pocket_depth, clearance_diameter)
        # explicit depth, since a nested object doesn't see the caller's builder
        hole_part += ClearanceHole(
            fastener,
            fit=fit,
            depth=self.hole_depth,
            counter_sunk=False,
            mode=Mode.PRIVATE,
        )

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
        cut_depth = hole_depth(depth)
        # nominal, since Screw.head_diameter is measured from the model and can be smaller (e.g. countersunk)
        head_diameter = fastener.screw_data.get("dk", fastener.head_diameter)
        slot_width = fastener.clearance_hole_diameters[fit]
        slot_length = head_diameter / 2 + slot_width / 2  # origin to slot end center

        head_clearance = (slot_width - fastener.thread_diameter) / 2
        head_hole = Cylinder(
            head_diameter / 2 + head_clearance,
          cut_depth,
            align=(Align.CENTER, Align.CENTER, Align.MAX),
        )
        slot_plan = Pos(slot_length / 2, 0) * SlotCenterToCenter(slot_length, slot_width)
        slot = extrude(slot_plan, cut_depth, dir=(0, 0, -1))
        hole_part = head_hole + [Rot(0, 0, angle) * slot for angle in angles]

        super().__init__(
            part=hole_part.clean(),
            align=None,
            rotation=rotation,
            mode=mode,
        )
