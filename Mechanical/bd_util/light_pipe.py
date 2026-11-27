from __future__ import annotations

import copy
from dataclasses import dataclass
from math import atan, degrees, radians, tan
from typing import Iterable, Union

from build123d import (
    Align,
    Axis,
    BasePartObject,
    Cone,
    Cylinder,
    Edge,
    Face,
    Kind,
    Location,
    Mode,
    Part,
    Pos,
    RotationLike,
    Sketch,
    Wire,
    extrude,
    loft,
    offset,
)

# overshoot past the wall surfaces and tangencies avoiding coplanar or tangent boolean faces, and cone apex radius,
# since point apexes make OCC booleans and solid classification unreliable
_EPS = 0.01


_BASE_ALIGN = (Align.CENTER, Align.CENTER, Align.MIN)


def _extrude_chamfered(
    base: Face | Sketch,
    length: float,
    chamfer_length: float = 0.0,
    chamfer_angle: float = 90.0,
) -> Part:
    """Extrudes base (in an XY plane) toward +Z by the total length, the top chamfer_length tapering inward by
    chamfer_angle from the Z axis; 0 is no chamfer."""
    if not 0 <= chamfer_length < length:
        raise ValueError(
            f"chamfer length {chamfer_length:.3f} must be within [0, length {length:.3f})"
        )
    base_length = length - chamfer_length
    part = extrude(base, base_length)
    if chamfer_length > 0:
        inset = chamfer_length * tan(radians(chamfer_angle))
        if not offset(base, -inset, kind=Kind.INTERSECTION).area > 0:
            raise ValueError(f"chamfer inset {inset:.3f} is too large for the profile")
        part += extrude(
            Pos(0, 0, base_length) * base, chamfer_length, taper=chamfer_angle
        )
    return part


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
            raise ValueError(
                f"flange draft_angle {self.draft_angle} must be within (0, 90]"
            )

    @property
    def draft_height(self) -> float:
        """Height of the draft section, to full extension."""
        return (
            0.0
            if self.draft_angle == 90
            else self.extension / tan(radians(self.draft_angle))
        )

    def _make(self, top: Face, wall_inner_z: float) -> Part:
        """In the light pipe frame, extending down from the wall inner surface at wall_inner_z."""
        plate = offset(top, self.extension, kind=Kind.INTERSECTION)
        return Pos(0, 0, wall_inner_z - self.thickness) * _extrude_chamfered(
            plate, self.thickness, self.draft_height, self.draft_angle
        )


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
            raise ValueError(
                f"crush rib lead_in_angle {self.lead_in_angle} must be within (0, 90]"
            )
        object.__setattr__(self, "along", tuple(self.along))

    @property
    def lead_in_height(self) -> float:
        return (
            0.0
            if self.lead_in_angle == 90
            else self.protrusion / tan(radians(self.lead_in_angle))
        )

    def _make(
        self, top: Face, wall_inner_z: float, engagement: float, flange: Flange | None
    ) -> list[Part]:
        """In the light pipe frame, extending engagement up from the wall inner surface at wall_inner_z, and into the
        flange below."""
        if self.lead_in_height > engagement:
            raise ValueError(
                f"crush rib lead-in {self.lead_in_height:.3f} exceeds the engagement {engagement:.3f}; "
                f"lead_in_angle must be at least {degrees(atan(self.protrusion / engagement)):.1f}"
            )
        outline = top.outer_wire()
        points = [
            vertex for curve in self.along for vertex in outline.intersect(curve) or []
        ]
        if not points:
            raise ValueError(
                "crush rib along curves don't cross the top profile outline"
            )

        # if there is a flange draft, extend the ribs into it
        buried = (0.0 if flange is None else flange.draft_height) + _EPS
        straight = buried + engagement - self.lead_in_height
        if not straight > 0:
            raise ValueError(
                f"crush rib straight length {straight:.3f} must be positive"
            )
        ribs = []
        for point in points:
            base = Pos(point.X, point.Y, wall_inner_z - buried)
            ribs.append(base * Cylinder(self.protrusion, straight, align=_BASE_ALIGN))
            if (
                self.lead_in_height > 0
            ):  # truncated at the same slope, rather than an apex on the top profile outline
                ribs.append(
                    base
                    * Pos(0, 0, straight)
                    * Cone(
                        self.protrusion, _EPS, self.lead_in_height, align=_BASE_ALIGN
                    )
                )
        return ribs


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
            raise ValueError(
                f"diffusion cone angle {self.angle} must be within (0, 180)"
            )

    @property
    def depth(self) -> float:
        """Apex to base height."""
        return self.diameter / 2 / tan(radians(self.angle / 2))

    def _make(self, led_height: float) -> Part:
        """In the light pipe frame, above the LED cavity ceiling at led_height."""
        return Pos(0, 0, led_height + self.gap) * Cone(
            _EPS, self.diameter / 2, self.depth, align=_BASE_ALIGN
        )


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
            raise ValueError(
                f"wall_thickness {wall_thickness} must be within the height {height}"
            )
        self.loft_height = loft_height = (
            height - wall_thickness
        )  #: board to the wall inner surface
        if not 0 < led_height < loft_height:
            raise ValueError(
                f"led_height {led_height} must be within the loft height {loft_height}"
            )
        if not 0 < chamfer_angle < 90:
            raise ValueError(f"chamfer_angle {chamfer_angle} must be within (0, 90)")
        flange_thickness = 0.0 if flange is None else flange.thickness
        if not loft_height - flange_thickness > led_height:
            raise ValueError(
                f"flange bottom {loft_height - flange_thickness:.3f} above the board must be above the LED "
                f"cavity {led_height}"
            )

        # copies, since clean() (UnifySameDomain) can modify sub-shapes shared with boolean inputs in place
        top, board, led = (
            copy.deepcopy(profile)
            for profile in (self.top_profile, self.board_profile, self.led_profile)
        )
        wall_inner = Pos(0, 0, loft_height)
        pipe = loft([board, wall_inner * top], ruled=True)
        pipe += wall_inner * _extrude_chamfered(
            top, wall_thickness, chamfer_length, chamfer_angle
        )
        if flange is not None:
            pipe += flange._make(top, loft_height)
        if crush_ribs is not None:
            pipe += crush_ribs._make(
                top, loft_height, wall_thickness - chamfer_length, flange
            )
        pipe -= extrude(led, led_height)
        voids: list[Part] = []  # sealed, cut last; see fused()
        if diffusion_cone is not None:
            if not led_height + diffusion_cone.gap + diffusion_cone.depth < height:
                raise ValueError(
                    f"diffusion cone top {led_height + diffusion_cone.gap + diffusion_cone.depth:.3f} above the "
                    f"board must be within the pipe height {height}"
                )
            voids.append(diffusion_cone._make(led_height))
        body = copy.deepcopy(pipe)  # without voids
        if voids:
            pipe -= voids
        pipe = pipe.clean()

        unaligned = pipe.solids()[
            0
        ].location  # super() aligns and rotates pipe; apply the same to body and voids
        super().__init__(part=pipe, rotation=rotation, align=align, mode=mode)
        transform = self.solids()[0].location * unaligned.inverse()
        self._body = transform * body
        self._voids = [transform * void for void in voids]

    def array(self, locations: Iterable[Location], *others: Part) -> Part:
        """This light pipe at each location, fused with others (e.g. bridging bars) into one part. See fused()."""
        return LightPipe.fused([(self, location) for location in locations], *others)

    @staticmethod
    def fused(placements: Iterable[tuple[LightPipe, Location]], *others: Part) -> Part:
        """Light pipes (possibly of different designs) at locations, fused with others (e.g. bridging bars) into one part.

        Use instead of fusing light pipes directly: OCC can produce an invalid solid when fusing solids that already
        contain sealed voids (e.g. diffusion cones), so this fuses the void-free bodies and cuts the voids last.

        Locations are relative to each light pipe's own origin, as constructed: any placement of the light pipe object
        itself (location * light_pipe, or by a builder) is ignored.
        """
        placements = list(placements)
        if not placements:
            raise ValueError("fused requires at least one light pipe")
        # copies, since clean() can modify sub-shapes shared with boolean inputs in place
        bodies = [location * copy.deepcopy(pipe._body) for pipe, location in placements]
        voids = [
            location * void for pipe, location in placements for void in pipe._voids
        ]
        fused = bodies[0] + [*bodies[1:], *others]
        if voids:
            fused -= voids
        return Part(fused.clean().wrapped)  # disjoint fuses return a Compound

    def cutout_profile(self, clearance: float = 0.0) -> Sketch:
        """Top profile grown by clearance per side, rounding convex corners."""
        return offset(self.top_profile, clearance)


class LightPipeHole(BasePartObject):
    """Enclosure wall pocket for a LightPipe, sharing its board origin, from the wall inner surface to the pipe top.

    This adds a _EPS to the cut height on both sides to allow a cut even when coplanar with the wall faces.
    """

    def __init__(
        self,
        light_pipe: LightPipe,
        clearance: float = 0.0,
        rotation: RotationLike = (0, 0, 0),
        mode: Mode = Mode.SUBTRACT,
    ):
        profile = Pos(0, 0, light_pipe.loft_height - _EPS) * light_pipe.cutout_profile(
            clearance
        )
        hole_part = extrude(profile, light_pipe.wall_thickness + 2 * _EPS)

        super().__init__(
            part=hole_part,
            align=None,
            rotation=rotation,
            mode=mode,
        )
