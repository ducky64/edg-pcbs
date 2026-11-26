from __future__ import annotations

import copy
from dataclasses import dataclass
from math import atan, degrees, radians, tan
from typing import Iterable, Union

from build123d.build_common import validate_inputs
from build123d.build_enums import Align, Kind, Mode
from build123d.build_part import BuildPart
from build123d.geometry import Axis, Location, Plane, Pos, RotationLike, Vector
from build123d.objects_part import BasePartObject
from build123d.topology import Edge, Face, Shape, Sketch, Solid, Wire

# overshoot past the wall surfaces and tangencies avoiding coplanar or tangent boolean faces, and cone apex radius,
# since point apexes make OCC booleans and solid classification unreliable
_EPS = 0.01


def _extrude_chamfered(base: Face, length: float, chamfer_length: float = 0.0, chamfer_angle: float = 90.0) -> Solid:
  """Extrudes base (in an XY plane) toward +Z by the total length, the top chamfer_length tapering inward by
  chamfer_angle from the Z axis; 0 is no chamfer."""
  if not 0 <= chamfer_length < length:
    raise ValueError(f"chamfer length {chamfer_length:.3f} must be within [0, length {length:.3f})")
  base_length = length - chamfer_length
  solid = Solid.extrude(base, Vector(0, 0, base_length))
  if chamfer_length > 0:
    inset = chamfer_length * tan(radians(chamfer_angle))
    chamfer_top = Face(base.outer_wire().offset_2d(-inset, kind=Kind.INTERSECTION))
    if not chamfer_top.area > 0:
      raise ValueError(f"chamfer inset {inset:.3f} is too large for the profile")
    chamfer = Solid.extrude_taper(Pos(0, 0, base_length) * base, Vector(0, 0, chamfer_length), chamfer_angle)
    solid = solid.fuse(chamfer)
  return solid


@dataclass(frozen=True)
class Flange:
  """Retaining flange, directly below the enclosure wall inner surface.

  Args:
      extension: outward growth of the top profile, per side
      draft_angle: of the flange top face; 90 is flat. When printed top down, this is the overhang angle.
      thickness: from the wall inner surface to the flange underside, including the draft section
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
  def draft_height(self) -> float:
    """Height of the draft section, to full extension."""
    return 0.0 if self.draft_angle == 90 else self.extension / tan(radians(self.draft_angle))

  def _make(self, top: Face) -> Solid:
    """With the wall inner surface at the local origin, extending toward -Z."""
    plate = Face(top.outer_wire().offset_2d(self.extension, kind=Kind.INTERSECTION))
    return Pos(0, 0, -self.thickness) * _extrude_chamfered(plate, self.thickness, self.draft_height, self.draft_angle)


@dataclass(frozen=True)
class CrushRibs:
  """Half-round press-fit ribs along the through-wall section.

  Ribs are centered where each along curve crosses the top profile outline.
  If a flange is also present, the ribs extend down into the flange draft.

  Args:
      protrusion: beyond the top profile (not accounting for any hole tolerancing), also the rib radius
      lead_in_angle: taper toward the pipe top, which is inserted first, from the pipe axis; 90 is no lead-in
      along: ribs are positioned at where these lines intersect the top profile
  """
  protrusion: float = 0.3
  lead_in_angle: float = 30.0
  along: Iterable[Edge | Wire | Axis] = (Axis.X,)

  def __post_init__(self):
    if self.protrusion <= 0:
      raise ValueError("crush rib protrusion must be positive")
    if not 0 < self.lead_in_angle <= 90:
      raise ValueError(f"crush rib lead_in_angle {self.lead_in_angle} must be within (0, 90]")
    object.__setattr__(self, "along", tuple(self.along))

  @property
  def lead_in_height(self) -> float:
    return 0.0 if self.lead_in_angle == 90 else self.protrusion / tan(radians(self.lead_in_angle))

  def _make(self, top: Face, engagement: float, flange: Flange | None) -> list[Solid]:
    """With the wall inner surface at the local origin, extending engagement toward +Z, and into the flange below."""
    if self.lead_in_height > engagement:
      raise ValueError(f"crush rib lead-in {self.lead_in_height:.3f} exceeds the engagement {engagement:.3f}; "
                       f"lead_in_angle must be at least {degrees(atan(self.protrusion / engagement)):.1f}")
    outline = top.outer_wire()
    points = [vertex for curve in self.along for vertex in outline.intersect(curve) or []]
    if not points:
      raise ValueError("crush rib along curves don't cross the top profile outline")

    # if there is a flange draft, extend the ribs into it
    buried = (0.0 if flange is None else flange.draft_height) + _EPS
    straight = buried + engagement - self.lead_in_height
    assert straight > 0, f"rib straight length {straight} must be positive"
    rib_solids = []
    for point in points:
      base = Plane((point.X, point.Y, -buried))
      rib_solids.append(Solid.make_cylinder(self.protrusion, straight, base))
      if self.lead_in_height > 0:  # truncated at the same slope, rather than an apex on the top profile outline
        rib_solids.append(Solid.make_cone(self.protrusion, _EPS, self.lead_in_height, base.offset(straight)))
    return rib_solids


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

  def _make(self) -> Solid:
    """With the LED cavity ceiling at the local origin, extending toward +Z."""
    return Solid.make_cone(_EPS, self.diameter / 2, self.depth,
                           Plane.XY.offset(self.gap))


def _single_face(profile: Face | Sketch, name: str) -> Face:
  faces = profile.faces()
  if len(faces) != 1:
    raise ValueError(f"{name} must be a single face, got {len(faces)}")
  return faces[0]


class LightPipe(BasePartObject):
  """Carries light from a board-mounted LED out through an enclosure wall.

  The vertical stackup is:
    z = height (pipe top)
      (optional chamfer)
      top_profile
    z = loft_height (wall inner surface)
      (optional flange)
      loft between board_profile and top_profile
      (cutout of led_profile, up to led_height)
    z = 0 (origin, board top surface)

  Profiles specified in local XY. This generates a positive part.

  Args:
      height: board top surface to the pipe top
      wall_thickness: wall inner surface to the pipe top, the depth the pipe engages the wall
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
    self.flange = flange
    self.crush_ribs = crush_ribs
    self.diffusion_cone = diffusion_cone
    self.chamfer_angle = chamfer_angle
    self.chamfer_length = chamfer_length

    if not 0 < wall_thickness < height:
      raise ValueError(f"wall_thickness {wall_thickness} must be within the height {height}")
    self.loft_height = loft_height = height - wall_thickness  #: board to the wall inner surface
    if not 0 < led_height < loft_height:
      raise ValueError(f"led_height {led_height} must be within the loft height {loft_height}")
    if not 0 < chamfer_angle < 90:
      raise ValueError(f"chamfer_angle {chamfer_angle} must be within (0, 90)")
    flange_thickness = 0.0 if flange is None else flange.thickness
    if not loft_height - flange_thickness > led_height:
      raise ValueError(f"flange bottom {loft_height - flange_thickness:.3f} above the board must be above the LED "
                       f"cavity {led_height}")

    # copies, since clean() (UnifySameDomain) can modify sub-shapes shared with boolean inputs in place
    top, board, led = (copy.deepcopy(profile) for profile in (self.top_profile, self.board_profile, self.led_profile))
    wall_inner = Pos(0, 0, loft_height)
    loft = Solid.make_loft([board.outer_wire(), (wall_inner * top).outer_wire()], ruled=True)
    through_wall = _extrude_chamfered(top, wall_thickness, chamfer_length, chamfer_angle)
    pipe = loft.fuse(wall_inner * through_wall)
    if flange is not None:
      pipe = pipe.fuse(wall_inner * flange._make(top))
    if crush_ribs is not None:
      pipe = pipe.fuse(*[wall_inner * rib for rib in crush_ribs._make(top, wall_thickness - chamfer_length, flange)])
    pipe = pipe.cut(Solid.extrude(led, Vector(0, 0, led_height)))
    self._voids: list[Solid] = []  # sealed, cut last; see fuse_light_pipes
    if diffusion_cone is not None:
      if not led_height + diffusion_cone.gap + diffusion_cone.depth < height:
        raise ValueError(f"diffusion cone top {led_height + diffusion_cone.gap + diffusion_cone.depth:.3f} above the "
                         f"board must be within the pipe height {height}")
      self._voids.append(Pos(0, 0, led_height) * diffusion_cone._make())
    self._body = copy.deepcopy(pipe)  # without voids
    if self._voids:
      pipe = pipe.cut(*self._voids)
    pipe = pipe.clean()

    super().__init__(part=pipe, rotation=rotation, align=align, mode=mode)

  def cutout_profile(self, clearance: float = 0.0) -> Face:
    """Top profile grown by clearance per side, rounding convex corners."""
    return Face(self.top_profile.outer_wire().offset_2d(clearance))


def fuse_light_pipes(light_pipe: LightPipe, locations: Iterable[Location], *others: Shape) -> Shape:
  """Copies of light_pipe at each location, fused with others (e.g. bridging bars) into one part.

  Use instead of fusing placed LightPipes directly: OCC can produce an invalid solid when fusing solids that already
  contain sealed voids (e.g. diffusion cones), so this fuses the void-free bodies and cuts the voids last.
  """
  locations = list(locations)
  if not locations:
    raise ValueError("fuse_light_pipes requires at least one location")
  body = copy.deepcopy(light_pipe._body)  # clean() can modify shared sub-shapes in place
  bodies = [location * body for location in locations]
  fused = bodies[0].fuse(*bodies[1:], *others)
  voids = [location * void for location in locations for void in light_pipe._voids]
  if voids:
    fused = fused.cut(*voids)
  return fused.clean()


class LightPipeHole(BasePartObject):
  """Enclosure wall pocket for a LightPipe, sharing its board origin, from the wall inner surface to the pipe top.

  This adds a _EPS to the cut height on both sides to allow a cut even when coplanar with the wall faces.
  """

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

    profile = Pos(0, 0, light_pipe.loft_height - _EPS) * light_pipe.cutout_profile(clearance)
    hole_part = Solid.extrude(profile, Vector(0, 0, light_pipe.wall_thickness + 2 * _EPS))

    if context is not None:  # pipe and hole share the board origin
      light_pipe.hole_locations.extend(self._get_object_locations())

    super().__init__(
      part=hole_part,
      align=None,
      rotation=rotation,
      mode=mode,
    )
