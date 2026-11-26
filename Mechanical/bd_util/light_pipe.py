from __future__ import annotations

import copy
from dataclasses import dataclass
from math import atan, degrees, radians, tan
from typing import Iterable, Union

from build123d.build_common import validate_inputs
from build123d.build_enums import Align, Mode
from build123d.build_part import BuildPart
from build123d.geometry import Location, Plane, Pos, RotationLike, Vector
from build123d.objects_part import BasePartObject
from build123d.topology import Edge, Face, Sketch, Solid, Wire

_EPS = 0.01  # overshoot past the wall surfaces, avoiding coplanar boolean faces
_APEX_TIP_RADIUS = 0.01  # diffusion cone truncation, avoiding a point apex


@dataclass(frozen=True)
class Flange:
  """Retaining flange, directly below the enclosure wall inner surface.

  Args:
      extension: outward growth of the top profile, per side
      draft_angle: of the flange top face; 90 is flat. When printed top down, this is the overhang angle.
      thickness: including draft
  """
  extension: float
  draft_angle: float = 90.0
  thickness: float = 1.0

  def __post_init__(self):
    if self.extension <= 0 or self.thickness <= 0:
      raise ValueError("flange extension and thickness must be positive")
    if not 0 < self.draft_angle <= 90:
      raise ValueError(f"flange draft_angle {self.draft_angle} must be within (0, 90]")

  @property
  def rise(self) -> float:
    """Height of the draft section, to full extension."""
    return 0.0 if self.draft_angle == 90 else self.extension / tan(radians(self.draft_angle))


@dataclass(frozen=True)
class CrushRibs:
  """Half-round press-fit ribs along the through-wall section.

  Ribs are centered where each along curve crosses the top profile outline. The crush is protrusion minus the hole
  clearance. Defaults are for FDM (0.4mm nozzle, 0.2mm layers).

  Args:
      protrusion: beyond the top profile, also the rib radius
      lead_in_angle: taper toward the pipe top, which is inserted first; 90 is no lead-in
      along: curves in local XY, defaulting to the local X axis
  """
  protrusion: float = 0.3
  lead_in_angle: float = 30.0
  along: Iterable[Edge | Wire] | None = None

  def __post_init__(self):
    if self.protrusion <= 0:
      raise ValueError("crush rib protrusion must be positive")
    if not 0 < self.lead_in_angle <= 90:
      raise ValueError(f"crush rib lead_in_angle {self.lead_in_angle} must be within (0, 90]")
    if self.along is not None:
      object.__setattr__(self, "along", tuple(self.along))

  @property
  def lead_in_height(self) -> float:
    return 0.0 if self.lead_in_angle == 90 else self.protrusion / tan(radians(self.lead_in_angle))


@dataclass(frozen=True)
class DiffusionCone:
  """Sealed void above the LED, apex toward the LED, spreading light sideways.

  Args:
      diameter: at the cone base, the top of the void
      angle: apex angle
      gap: solid material between the LED cavity ceiling and the apex
  """
  diameter: float
  angle: float = 90.0
  gap: float = 0.4

  def __post_init__(self):
    if self.diameter <= 0 or self.gap <= 0:
      raise ValueError("diffusion cone diameter and gap must be positive")
    if not 0 < self.angle < 180:
      raise ValueError(f"diffusion cone angle {self.angle} must be within (0, 180)")

  @property
  def depth(self) -> float:
    """Apex to base height."""
    return self.diameter / 2 / tan(radians(self.angle / 2))


def _single_face(profile: Face | Sketch, name: str) -> Face:
  faces = profile.faces()
  if len(faces) != 1:
    raise ValueError(f"{name} must be a single face, got {len(faces)}")
  return faces[0]


class LightPipe(BasePartObject):
  """Carries light from a board-mounted LED out through an enclosure wall.

  The vertical stackup is:
    z = height
      (optional chamfer)
      top_profile
    z = height - wall_thickness
      (optional flange)
      loft between board_profile and top_profile
      (cutout of led_profile, up to led_height)
    z = 0 (origin)

  Profiles specified in local XY. This generates a positive part.

  Args:
      height: board top surface to the pipe top
      wall_thickness: of the enclosure wall at the pipe top
      led_height: LED cavity height, including any air gap
      chamfer_angle: pipe top chamfer, from the pipe axis
      chamfer_length: along the pipe axis; 0 is no chamfer
  """

  _applies_to = [BuildPart._tag]

  def __init__(
      self,
      top_profile: Face | Sketch,
      board_profile: Face | Sketch,
      height: float,
      wall_thickness: float,
      led_profile: Face | Sketch,
      led_height: float,
      flange: Flange | None = None,
      crush_ribs: CrushRibs | None = None,
      diffusion_cone: DiffusionCone | None = None,
      chamfer_angle: float = 45.0,
      chamfer_length: float = 0.0,
      rotation: RotationLike = (0, 0, 0),
      align: Union[None, Align, tuple[Align, Align, Align]] = None,
      mode: Mode = Mode.ADD,
  ):
    context: BuildPart | None = BuildPart._get_context(self)
    validate_inputs(context, self)

    self.hole_locations: list[Location] = []  #: global, appended by LightPipeHole
    self.top_profile = _single_face(top_profile, "top_profile")
    self.board_profile = _single_face(board_profile, "board_profile")
    self.led_profile = _single_face(led_profile, "led_profile")
    self.pipe_height = height  #: Part.height is the bounding box height
    self.wall_thickness = wall_thickness
    self.led_height = led_height
    if not 0 < wall_thickness < height:
      raise ValueError(f"wall_thickness {wall_thickness} must be within the height {height}")
    self.loft_height = height - wall_thickness  #: board to the wall inner surface
    if not 0 < led_height < self.loft_height:
      raise ValueError(f"led_height {led_height} must be within the loft height {self.loft_height}")
    self.flange = flange
    self.crush_ribs = crush_ribs
    self.diffusion_cone = diffusion_cone
    self.chamfer_angle = chamfer_angle
    self.chamfer_length = chamfer_length
    if not 0 < chamfer_angle < 90:
      raise ValueError(f"chamfer_angle {chamfer_angle} must be within (0, 90)")
    if not 0 <= chamfer_length <= wall_thickness:
      raise ValueError(f"chamfer_length {chamfer_length} must be within [0, wall_thickness {wall_thickness}]")

    #: z of the flange underside, or loft_height without a flange
    self.flange_bottom = self.loft_height if flange is None else self.loft_height - flange.rise - flange.thickness
    if not self.flange_bottom > led_height:
      raise ValueError(f"flange bottom {self.flange_bottom:.3f} must be above the LED cavity {led_height}")

    # copies, since clean() (UnifySameDomain) can modify sub-shapes shared with boolean inputs in place
    top, board, led = (copy.deepcopy(profile) for profile in (self.top_profile, self.board_profile, self.led_profile))
    wall_profile = Pos(0, 0, self.loft_height) * top
    loft = Solid.make_loft([board.outer_wire(), wall_profile.outer_wire()], ruled=True)
    pipe = loft.fuse(Solid.extrude(wall_profile, Vector(0, 0, wall_thickness)))
    if flange is not None:
      pipe = pipe.fuse(self._make_flange(flange, top))
    if crush_ribs is not None:
      pipe = pipe.fuse(*self._make_crush_ribs(crush_ribs, top))
    if chamfer_length > 0:
      pipe = pipe.cut(self._make_chamfer_cutter(top, pipe))
    if led_height > 0:
      pipe = pipe.cut(Solid.extrude(led, Vector(0, 0, led_height)))
    if diffusion_cone is not None:
      pipe = pipe.cut(self._make_diffusion_cone(diffusion_cone))
    pipe = pipe.clean()

    super().__init__(part=pipe, rotation=rotation, align=align, mode=mode)

  def _make_chamfer_cutter(self, top: Face, pipe: Solid) -> Solid:
    """Everything outside the chamfer surface."""
    inset = self.chamfer_length * tan(radians(self.chamfer_angle))
    try:
      chamfer_top = Face(top.outer_wire().offset_2d(-inset))
    except Exception as e:
      raise ValueError(f"chamfer inset {inset:.3f} is too large for the top profile") from e
    if not chamfer_top.area > 0:
      raise ValueError(f"chamfer inset {inset:.3f} is too large for the top profile")
    chamfer_base = self.pipe_height - self.chamfer_length
    chamfered = Solid.make_loft([(Pos(0, 0, chamfer_base) * top).outer_wire(),
                                 (Pos(0, 0, self.pipe_height) * chamfer_top).outer_wire()], ruled=True)
    bbox = pipe.bounding_box()
    slab = Solid.make_box(bbox.size.X + 2, bbox.size.Y + 2, self.chamfer_length + _EPS,
                          Plane((bbox.min.X - 1, bbox.min.Y - 1, chamfer_base)))
    return slab.cut(chamfered)

  def _make_flange(self, flange: Flange, top: Face) -> Solid:
    outer = Face(top.outer_wire().offset_2d(flange.extension))
    plate = Solid.extrude(Pos(0, 0, self.loft_height - flange.rise) * outer, Vector(0, 0, -flange.thickness))
    if flange.rise == 0:
      return plate
    draft = Solid.make_loft([(Pos(0, 0, self.loft_height) * top).outer_wire(),
                             (Pos(0, 0, self.loft_height - flange.rise) * outer).outer_wire()], ruled=True)
    return draft.fuse(plate)

  def _make_crush_ribs(self, ribs: CrushRibs, top: Face) -> list[Solid]:
    engagement = self.wall_thickness - self.chamfer_length  # ribs end where the top chamfer starts
    if ribs.lead_in_height > engagement:
      raise ValueError(f"crush rib lead-in {ribs.lead_in_height:.3f} exceeds the wall thickness below the chamfer {engagement}; "
                       f"lead_in_angle must be at least {degrees(atan(ribs.protrusion / engagement)):.1f}")
    along = ribs.along
    if along is None:  # default to local X axes
      extent = 2 * max(abs(v) for v in [*top.bounding_box().min, *top.bounding_box().max])
      along = (Edge.make_line((-extent, 0), (extent, 0)),)
    outline = top.outer_wire()
    points = []
    for curve in along:
      crossings = outline.intersect(curve)
      points.extend(crossings.vertices() if crossings is not None else [])
    if not points:
      raise ValueError("crush rib along curves don't cross the top profile outline")

    straight = engagement - ribs.lead_in_height
    rib_solids = []
    for point in points:
      base = Plane((point.X, point.Y, self.loft_height))
      if straight > 0:
        rib_solids.append(Solid.make_cylinder(ribs.protrusion, straight, base))
      if ribs.lead_in_height > 0:
        rib_solids.append(Solid.make_cone(ribs.protrusion, 0, ribs.lead_in_height, base.offset(straight)))
    return rib_solids

  def _make_diffusion_cone(self, cone: DiffusionCone) -> Solid:
    apex = self.led_height + cone.gap
    if not apex + cone.depth < self.pipe_height:
      raise ValueError(f"diffusion cone top {apex + cone.depth:.3f} must be within the pipe height {self.pipe_height}")
    # truncated, since OCC solid classification of several sealed voids with point apexes is unreliable
    tip_height = _APEX_TIP_RADIUS / tan(radians(cone.angle / 2))
    return Solid.make_cone(_APEX_TIP_RADIUS, cone.diameter / 2, cone.depth - tip_height,
                           Plane.XY.offset(apex + tip_height))

  def cutout_profile(self, clearance: float = 0.0) -> Face:
    """Top profile grown by clearance per side, rounding convex corners."""
    return Face(self.top_profile.outer_wire().offset_2d(clearance))


class LightPipeHole(BasePartObject):
  """Enclosure wall cutout for a LightPipe, at the wall outer surface, aligned with the pipe in local XY."""

  _applies_to = [BuildPart._tag]

  def __init__(
      self,
      light_pipe: LightPipe,
      clearance: float = 0.0,
      rotation: RotationLike = (0, 0, 0),
      mode: Mode = Mode.SUBTRACT,
  ):
    context: BuildPart | None = BuildPart._get_context(self)
    validate_inputs(context, self)

    self.hole_depth = light_pipe.wall_thickness
    profile = Pos(0, 0, _EPS) * light_pipe.cutout_profile(clearance)
    hole_part = Solid.extrude(profile, Vector(0, 0, -(self.hole_depth + 2 * _EPS)))

    if context is not None:  # pipe origin is on the board, pipe_height below the hole surface
      light_pipe.hole_locations.extend(
        [location * Pos(0, 0, -light_pipe.pipe_height) for location in self._get_object_locations()])

    super().__init__(
      part=hole_part,
      align=None,
      rotation=rotation,
      mode=mode,
    )
