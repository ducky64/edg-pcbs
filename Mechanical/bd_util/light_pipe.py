from __future__ import annotations

from typing import Union

from build123d.build_common import validate_inputs
from build123d.build_enums import Align, Mode
from build123d.build_part import BuildPart
from build123d.geometry import Location, Pos, RotationLike, Vector
from build123d.objects_part import BasePartObject
from build123d.topology import Face, Sketch, Solid

_EPS = 0.01  # mm, overshoot past the enclosure wall surfaces, to avoid coplanar boolean faces


def _single_face(profile: Face | Sketch, name: str) -> Face:
  faces = profile.faces()
  if len(faces) != 1:
    raise ValueError(f"{name} must be a single face, got {len(faces)}")
  return faces[0]


class LightPipe(BasePartObject):
  """Part Object: LightPipe

  A light pipe carrying light from a board-mounted LED out through an enclosure wall. The pipe lofts from the
  board profile at the board surface to the top profile at the inner surface of the enclosure wall, then
  continues as the top profile through the wall to the pipe top. A cavity for the LED (and an air gap above it)
  is cut up into the pipe from the board face.

  The origin is at the LED center on the board top surface, with +Z away from the board. Profiles are in the
  local XY plane, relative to the origin.

  This is the authoritative positive part. Create it outside any builder (like a bd_warehouse Screw), as
  reference geometry.

  Args:
      top_profile (Face | Sketch): pipe profile through the enclosure wall, at the pipe top
      board_profile (Face | Sketch): pipe profile at the board surface
      height (float): board top surface to the pipe top
      wall_thickness (float): enclosure wall thickness the top profile passes through, ending at the pipe top
      led_profile (Face | Sketch): LED cavity profile, at the board surface
      led_height (float): LED cavity height from the board surface, including any air gap
      rotation (RotationLike, optional): object rotation. Defaults to (0, 0, 0).
      align (Align | tuple[Align, Align, Align], optional): object alignment. Defaults to None.
      mode (Mode, optional): combination mode. Defaults to Mode.ADD.

  Raises:
      ValueError: a profile is not a single face
      ValueError: invalid heights
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
      rotation: RotationLike = (0, 0, 0),
      align: Union[None, Align, tuple[Align, Align, Align]] = None,
      mode: Mode = Mode.ADD,
  ):
    context: BuildPart | None = BuildPart._get_context(self)
    validate_inputs(context, self)

    self.hole_locations: list[Location] = []  #: global pipe locations, appended by LightPipeHole
    self.top_profile = _single_face(top_profile, "top_profile")
    self.board_profile = _single_face(board_profile, "board_profile")
    self.led_profile = _single_face(led_profile, "led_profile")
    self.pipe_height = height  #: board top surface to the pipe top (Part.height is the bounding box height)
    self.wall_thickness = wall_thickness
    self.led_height = led_height
    if not 0 < wall_thickness < height:
      raise ValueError(f"wall_thickness {wall_thickness} must be within the height {height}")
    self.loft_height = height - wall_thickness  #: board surface to the inner surface of the enclosure wall
    if not 0 < led_height < self.loft_height:
      raise ValueError(f"led_height {led_height} must be within the loft height {self.loft_height}")

    wall_profile = Pos(0, 0, self.loft_height) * self.top_profile
    loft = Solid.make_loft([self.board_profile.outer_wire(), wall_profile.outer_wire()], ruled=True)
    through = Solid.extrude(wall_profile, Vector(0, 0, wall_thickness))
    led_cavity = Solid.extrude(self.led_profile, Vector(0, 0, led_height))
    pipe = loft.fuse(through).cut(led_cavity).clean()

    super().__init__(part=pipe, rotation=rotation, align=align, mode=mode)

  def cutout_profile(self, clearance: float = 0.0) -> Face:
    """Top profile grown by clearance per side (rounded at convex corners), at the local origin."""
    if clearance == 0:
      return self.top_profile
    return Face(self.top_profile.outer_wire().offset_2d(clearance))


class LightPipeHole(BasePartObject):
  """Part Object: LightPipeHole

  The enclosure wall cutout for a LightPipe: its top profile grown by clearance, through the wall thickness.
  The origin is on the enclosure wall outer surface, cutting toward local -Z, aligned with the pipe in local XY.

  Args:
      light_pipe (LightPipe): the light pipe passing through the cutout
      clearance (float, optional): clearance added to each side of the pipe top profile. Defaults to 0.0.
      rotation (RotationLike, optional): object rotation. Defaults to (0, 0, 0).
      mode (Mode, optional): combination mode. Defaults to Mode.SUBTRACT.
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

    self.hole_depth = light_pipe.wall_thickness
    profile = Pos(0, 0, _EPS) * light_pipe.cutout_profile(clearance)
    hole_part = Solid.extrude(profile, Vector(0, 0, -(self.hole_depth + 2 * _EPS)))

    if context is not None:  # pipe origin on the board surface, below the pipe top at the hole surface
      light_pipe.hole_locations.extend(
        [location * Pos(0, 0, -light_pipe.pipe_height) for location in self._get_object_locations()])

    super().__init__(
      part=hole_part,
      align=None,
      rotation=rotation,
      mode=mode,
    )
