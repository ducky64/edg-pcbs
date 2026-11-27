"""
Connector panel cutouts, following the bd_util.fastener hole conventions.

Profile origin is on the board top surface, centered on the connector width along local X, with height along
local +Y. To cut through a wall, place on the wall outer surface at the board top height with local -Z into the
enclosure, e.g. Rot(90, 0, 0) for a -Y wall.
"""

from __future__ import annotations

from build123d import (
    BasePartObject,
    Mode,
    Pos,
    Rectangle,
    RectangleRounded,
    RotationLike,
    Sketch,
    extrude,
)

from bd_util._common import hole_depth


class _ProfileHole(BasePartObject):
    def __init__(self, profile: Sketch, depth: float | None, rotation: RotationLike, mode: Mode):
        self.hole_depth = hole_depth(depth)
        super().__init__(
            part=extrude(profile, self.hole_depth, dir=(0, 0, -1)),
            align=None,
            rotation=rotation,
            mode=mode,
        )


class UsbCReceptacleHole(_ProfileHole):
    """USB Type-C receptacle shell, sitting directly on the board."""

    _WIDTH = 8.94
    _HEIGHT = 3.15
    _RADIUS = 1.25

    def __init__(
        self,
        clearance: float = 0.0,
        depth: float | None = None,
        rotation: RotationLike = (0, 0, 0),
        mode: Mode = Mode.SUBTRACT,
    ):
        profile = Pos(0, self._HEIGHT / 2) * RectangleRounded(
            self._WIDTH + 2 * clearance,
            self._HEIGHT + 2 * clearance,
            self._RADIUS + clearance,
        )
        super().__init__(profile, depth, rotation, mode)


class MolexKK254Hole(_ProfileHole):
    """Mating crimp housing for a Molex KK 254 header (e.g. PC fan), pins parallel to and resting on the board."""

    _PITCH = 2.54
    _PIN_SIZE = 0.64  # square
    _HOUSING_WIDTH = 6.35  # perpendicular to the pin row
    _HOUSING_LENGTH_2PIN = 4.93  # along the pin row

    def __init__(
        self,
        pins: int,
        clearance: float = 0.0,
        depth: float | None = None,
        rotation: RotationLike = (0, 0, 0),
        mode: Mode = Mode.SUBTRACT,
    ):
        if pins < 2:
            raise ValueError(f"pins {pins} must be at least 2")
        housing_length = self._HOUSING_LENGTH_2PIN + (pins - 2) * self._PITCH
        profile = Pos(0, self._PIN_SIZE / 2) * Rectangle(
            housing_length + 2 * clearance, self._HOUSING_WIDTH + 2 * clearance
        )
        super().__init__(profile, depth, rotation, mode)


class JstShSideEntryHole(_ProfileHole):
    """JST SH side-entry SMD connector, SMxxB-SRSS-TB. 4-pin is used for qwiic.

    protrusions defines whether the cut includes protrusions on the side of the connectors.
    """

    _PITCH = 1.00
    _HOUSING_HEIGHT = 2.9
    _HOUSING_LENGTH_2PIN = 4.00
    _HOUSING_PROTRUSION_LENGTH = 2.0  # both sides

    def __init__(
        self,
        pins: int,
        protrusions: bool = True,
        clearance: float = 0.0,
        depth: float | None = None,
        rotation: RotationLike = (0, 0, 0),
        mode: Mode = Mode.SUBTRACT,
    ):
        if pins < 2:
            raise ValueError(f"pins {pins} must be at least 2")
        housing_length = self._HOUSING_LENGTH_2PIN + (pins - 2) * self._PITCH
        if protrusions:
            housing_length += self._HOUSING_PROTRUSION_LENGTH
        profile = Pos(0, self._HOUSING_HEIGHT / 2) * Rectangle(
            housing_length + 2 * clearance, self._HOUSING_HEIGHT + 2 * clearance
        )
        super().__init__(profile, depth, rotation, mode)
