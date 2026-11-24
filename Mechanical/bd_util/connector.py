"""
Connector panel cutouts, following the bd_warehouse.fastener hole conventions (see bd_util.fastener):
  - Part objects, usable in BuildPart (placed at each active location, Mode.SUBTRACT by default) or in algebra mode
  - the cutout surface is at the local origin, and the cutout extends into the material toward local -Z
  - depth=None implies through the part (in BuildPart only)
  - clearance is added to each side of the connector profile: positive is a clearance fit, negative interference

Cutout profiles have their origin on the board top surface, centered on the connector width: the connector
width is along local X, and its height above the board along local +Y. To cut through a wall, place the cutout on
the wall outer surface at the board top height, with local -Z pointing into the enclosure, e.g. Rot(90, 0, 0)
for a -Y wall (local Y up).
"""
from __future__ import annotations

from build123d.build_common import validate_inputs
from build123d.build_enums import Mode
from build123d.build_part import BuildPart
from build123d.geometry import Pos, RotationLike, Vector
from build123d.objects_part import BasePartObject
from build123d.objects_sketch import Rectangle, RectangleRounded
from build123d.topology import Face, Solid

from bd_util._common import hole_depth


class _ProfileHole(BasePartObject):
  """Base class for a cutout extruded from a planar profile, in the hole convention."""

  _applies_to = [BuildPart._tag]

  def __init__(self, profile: Face, depth: float | None, rotation: RotationLike, mode: Mode):
    context: BuildPart | None = BuildPart._get_context(self)
    validate_inputs(context, self)

    self.hole_depth = hole_depth(context, depth)
    super().__init__(
      part=Solid.extrude(profile, Vector(0, 0, -self.hole_depth)),
      align=None,
      rotation=rotation,
      mode=mode,
    )


class UsbCReceptacleHole(_ProfileHole):
  """Part Object: UsbCReceptacleHole

  A panel cutout for a USB Type-C receptacle shell sitting directly on the board: a rounded rectangle, with
  the origin on the board top surface, centered on the receptacle width.

  Args:
      clearance (float, optional): clearance added to each side of the shell profile. Defaults to 0.0.
      depth (float, optional): cutout depth - None implies through part. Defaults to None.
      rotation (RotationLike, optional): object rotation. Defaults to (0, 0, 0).
      mode (Mode, optional): combination mode. Defaults to Mode.SUBTRACT.

  Raises:
      ValueError: No depth provided
  """

  _WIDTH = 8.94  # mm, shell width
  _HEIGHT = 3.15  # mm, shell height
  _RADIUS = 1.25  # mm, shell corner radius

  def __init__(
      self,
      clearance: float = 0.0,
      depth: float | None = None,
      rotation: RotationLike = (0, 0, 0),
      mode: Mode = Mode.SUBTRACT,
  ):
    profile = Pos(0, self._HEIGHT / 2) * RectangleRounded(self._WIDTH + 2 * clearance, self._HEIGHT + 2 * clearance,
                                                         self._RADIUS + clearance).face()
    super().__init__(profile, depth, rotation, mode)


class MolexKK254Hole(_ProfileHole):
  """Part Object: MolexKK254Hole

  A panel cutout for the mating crimp housing of a Molex KK 254 (2.54mm pitch, e.g. PC fan) header mounted
  with its pins parallel to and resting on the board, inserted along the pin axis: a rectangle centered on the
  pin row, with the row along local X. The origin is on the board top surface, centered on the pin row.

  Args:
      pins (int): number of circuits
      clearance (float, optional): clearance added to each side of the housing profile. Defaults to 0.0.
      depth (float, optional): cutout depth - None implies through part. Defaults to None.
      rotation (RotationLike, optional): object rotation. Defaults to (0, 0, 0).
      mode (Mode, optional): combination mode. Defaults to Mode.SUBTRACT.

  Raises:
      ValueError: No depth provided
      ValueError: fewer than 2 pins
  """

  _PITCH = 2.54  # mm
  _PIN_SIZE = 0.64  # mm, square pins
  _HOUSING_WIDTH = 6.35  # mm, housing size perpendicular to the pin row
  _HOUSING_LENGTH_2PIN = 4.93  # mm, housing size along the pin row, for 2 pins

  def __init__(
      self,
      pins: int,
      clearance: float = 0.0,
      depth: float | None = None,
      rotation: RotationLike = (0, 0, 0),
      mode: Mode = Mode.SUBTRACT,
  ):
    assert pins >= 2
    housing_length = self._HOUSING_LENGTH_2PIN + (pins - 2) * self._PITCH
    profile = Pos(0, self._PIN_SIZE / 2) * Rectangle(housing_length + 2 * clearance,
                                                     self._HOUSING_WIDTH + 2 * clearance).face()
    super().__init__(profile, depth, rotation, mode)


class JstShSideEntry(_ProfileHole):
  """
  A panel cutout for JST SH side-entry surface-mounted connector, SMxxB-SRSS-TB.
  4-pin variation used for qwiic.

  Args:
      pins (int): number of circuits
      clearance (float, optional): clearance added to each side of the housing profile. Defaults to 0.0.
      depth (float, optional): cutout depth - None implies through part. Defaults to None.
      rotation (RotationLike, optional): object rotation. Defaults to (0, 0, 0).
      mode (Mode, optional): combination mode. Defaults to Mode.SUBTRACT.

  Raises:
      ValueError: No depth provided
      ValueError: fewer than 2 pins
  """

  _PITCH = 1.00  # mm
  _HOUSING_HEIGHT = 2.9  # mm
  _HOUSING_LENGTH_2PIN = 4.00  # mm

  def __init__(
      self,
      pins: int,
      clearance: float = 0.0,
      depth: float | None = None,
      rotation: RotationLike = (0, 0, 0),
      mode: Mode = Mode.SUBTRACT,
  ):
    assert pins >= 2
    housing_length = self._HOUSING_LENGTH_2PIN + (pins - 2) * self._PITCH
    profile = Pos(0, self._HOUSING_HEIGHT / 2) * Rectangle(housing_length + 2 * clearance,
                                                     self._HOUSING_HEIGHT + 2 * clearance).face()
    super().__init__(profile, depth, rotation, mode)
