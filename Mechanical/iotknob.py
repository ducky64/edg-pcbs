"""
Enclosure generator for the IoT knob PCB.

Coordinates: origin at the PCB center, X across the PCB width, Y along the PCB length (+Y is "top"),
Z up from the bottom face of the enclosure.

All dimensions in mm, angles in degrees.

Tolerancing applied to receiving (hole / cavity) features, which are enlarged.
Part features are nominal.
"""
from math import atan2, degrees
from pathlib import Path

from build123d import (Align, Box, BuildPart, Color, Compound, Cylinder, Edge, Hole, Location, Locations, Part, Plane,
                       PolarLocations, Pos, Rectangle, RectangleRounded, Rot, Shape, SlotArc, SlotOverall, export_step,
                       export_stl, extrude, insert)
from bd_warehouse.fastener import ClearanceHole, CounterSunkScrew, HexNut, PanHeadScrew

from bd_util.connector import MolexKK254Hole, UsbCReceptacleHole, JstShSideEntryHole
from bd_util.fastener import KeyholeHole, PrintableCaptiveNutHole
from bd_util.light_pipe import CrushRibs, DiffusionCone, Flange, LightPipe, LightPipeHole


# PCB definition
PCB_WIDTH = 24.0  # X
PCB_LENGTH = 70.0  # Y
PCB_RADIUS = 5.0
# (x, y, nut angle), angled so a nut hexagon flat faces the PCB corner
MOUNT_HOLES = [(-9.0, -32.0, 15.0), (-9.0, 32.0, -15.0), (9.0, -32.0, -15.0), (9.0, 32.0, 15.0)]
USB_X = 0.0  # on -Y edge
FAN_HEADER_X = 0.0  # on +Y edge
QWIIC_Y = -17  # on +/-X edge
OPTICAL_SENSOR_HOLES = [(-8, -27), (8, -27)]  # (x, y)
KNOB_Y = -11.0  # knob (encoder shaft) center, on centerline
LED_COUNT = 6  # evenly spaced around the knob
LED_RADIUS = 8.0  # from the knob center
LED_START_ANGLE = 0.0  # rotational offset of first LED

# Enclosure definition
WALL_THICKNESS = 1.5  # side walls, floor, and top face

# these heights in absolute Z coordinates
BOTTOM_BOSS_POS = 3.0  # bottom boss top surface / PCB bottom surface / part split
BOARD_TOP_POS = BOTTOM_BOSS_POS + 1.0  # PCB top surface
TOP_BOSS_POS = BOTTOM_BOSS_POS + 8.0  # top boss bottom surface / top PCB top surface
ENCLOSURE_HEIGHT = BOTTOM_BOSS_POS + 12 + WALL_THICKNESS  # top face outer surface

# Mounting screws, top cover to captive nuts in the bottom
FASTENER_SIZE = "M2.5-0.45"
FASTENER_FIT = "Normal"
BOSS_DIAMETER = 8.0  # clipped to the enclosure outline

# Manufacturability constants
TOLERANCE = 0.2  # clearance gap per side between mating surfaces
NUT_HOLDER_INTERFERENCE = -0.1
OVERHANG_ANGLE = 60.0
_EPS = 0.01  # overlap to avoid coplanar boolean faces


OUTER_OFFSET = TOLERANCE + WALL_THICKNESS  # enclosure outer outline, relative to the PCB outline
INNER_OFFSET = TOLERANCE  # enclosure cavity, relative to the PCB outline


def outline(offset: float) -> RectangleRounded:
  """PCB outline grown outward by offset."""
  return RectangleRounded(PCB_WIDTH + 2 * offset, PCB_LENGTH + 2 * offset, PCB_RADIUS + offset)


def make_light_pipe() -> LightPipe:
  """A single light pipe, in its local frame (origin at the LED on the board top, +X radially outward)."""
  LED_SLOT_LENGTH = 4.0  # circumferential length, including rounds
  LED_SLOT_WIDTH = 2.0
  LIGHT_PIPE_BOARD_LENGTH = 5.0
  LIGHT_PIPE_BOARD_WIDTH = 3.0
  LED_CAVITY = 2.0
  LED_CAVITY_HEIGHT = 1.4
  FLANGE_EXTENSION = 1.0
  DIFFUSION_CONE_DIAMETER = 1.5

  # pipe top: a slot curved along the LED placement circle
  slot_arc_angle = degrees((LED_SLOT_LENGTH - LED_SLOT_WIDTH) / LED_RADIUS)  # centerline arc, excluding end rounds
  slot_arc = Edge.make_circle(LED_RADIUS, Plane((-LED_RADIUS, 0, 0)),
                              start_angle=-slot_arc_angle / 2, end_angle=slot_arc_angle / 2)
  # crush ribs on the long sides of the slot (compressing across its width), on the radial line through the LED
  radial_line = Edge.make_line((-LED_SLOT_WIDTH, 0), (LED_SLOT_WIDTH, 0))
  return LightPipe(
    top_profile=SlotArc(slot_arc, LED_SLOT_WIDTH),
    board_profile=Rot(0, 0, 90) * SlotOverall(LIGHT_PIPE_BOARD_LENGTH, LIGHT_PIPE_BOARD_WIDTH),
    height=ENCLOSURE_HEIGHT - TOP_BOSS_POS,  # pipe top flush with the top face
    wall_thickness=WALL_THICKNESS,
    led_profile=Rectangle(LED_CAVITY, LED_CAVITY),
    led_height=LED_CAVITY_HEIGHT,
    flange=Flange(FLANGE_EXTENSION, draft_angle=OVERHANG_ANGLE),
    crush_ribs=CrushRibs(along=[radial_line]),
    diffusion_cone=DiffusionCone(DIFFUSION_CONE_DIAMETER),
  )


LIGHT_PIPE = make_light_pipe()


def make_light_pipe_tree(light_pipe: LightPipe, locations: list[Location]) -> Part:
  """Join light pipes into a single piece with bridging bars between them."""
  BRIDGE_WIDTH = 1.0  # narrow, to limit light crosstalk between pipes
  BRIDGE_THICKNESS = 0.6

  pipes = [loc * light_pipe for loc in locations]
  bars = []
  for start, end in zip(locations, locations[1:] + locations[:1]):
    a, b = start.position, end.position
    mid = (a + b) / 2
    bars.append(Pos(mid.X, mid.Y, a.Z + light_pipe.loft_height) * Rot(0, 0, degrees(atan2(b.Y - a.Y, b.X - a.X)))
                * Box((b - a).length, BRIDGE_WIDTH, BRIDGE_THICKNESS, align=(Align.CENTER, Align.CENTER, Align.MAX)))
  return pipes[0].fuse(*pipes[1:], *bars).clean()


def cut_connectors() -> None:
  """Cut the connector openings through the end walls, in the active BuildPart. Shared by both parts, since
  openings may span the part split."""
  wall_x = PCB_WIDTH / 2  # approx inner wall surface
  wall_y = PCB_LENGTH / 2
  # -Y wall, cutting toward -Y
  with Locations(Plane((USB_X, -wall_y, BOARD_TOP_POS), x_dir=(-1, 0, 0), z_dir=(0, 1, 0))):
    UsbCReceptacleHole(clearance=TOLERANCE)
  # +Y wall, cutting toward +Y
  with Locations(Plane((FAN_HEADER_X, wall_y, BOARD_TOP_POS), x_dir=(1, 0, 0), z_dir=(0, -1, 0))):
    MolexKK254Hole(4, clearance=TOLERANCE)

  # +/-X walls, cutting toward +/-X
  for x, x_dir, z_dir in [(wall_x, (0, -1, 0), (-1, 0, 0)), (-wall_x, (0, 1, 0), (1, 0, 0))]:
    with Locations(Plane((x, QWIIC_Y, TOP_BOSS_POS), x_dir=x_dir, z_dir=z_dir)):
      JstShSideEntryHole(4, clearance=TOLERANCE)


def make_bottom() -> Part:
  NUT = HexNut(size=FASTENER_SIZE, fastener_type="iso4032")
  NUT_BRIDGE_DEPTH = 0.4  # mm, bridging strip depth above the nut pocket roof, for support-free printing

  KEYHOLES = [(0.0, -12.0), (0.0, 12.0)]
  KEYHOLE_ANGLES = [-90, 90]  # degrees from +X as seen from below, slot directions from the head hole
  KEYHOLE_SCREW = PanHeadScrew(size="M2.5-0.45", length=10, fastener_type="iso1580")  # length doesn't affect holes

  footprint = extrude(outline(OUTER_OFFSET), amount=BOTTOM_BOSS_POS)
  tray = footprint - Pos(0, 0, WALL_THICKNESS) * extrude(outline(INNER_OFFSET), amount=BOTTOM_BOSS_POS)

  boss = Cylinder(BOSS_DIAMETER / 2, BOTTOM_BOSS_POS, align=(Align.CENTER, Align.CENTER, Align.MIN))
  for x, y, _ in MOUNT_HOLES:
    tray += (Pos(x, y, 0) * boss) & footprint

  with BuildPart() as builder:
    insert(tray)
    # nut pocket and screw hole cut into the bottom face (normal -Z); the plane is flipped, so the nut pocket
    # rotation about local Z is negated to give the nut angle as seen from above
    for x, y, angle in MOUNT_HOLES:
      with Locations(Plane((x, y, 0), x_dir=(1, 0, 0), z_dir=(0, 0, -1))):
        PrintableCaptiveNutHole(NUT, fit=FASTENER_FIT, nut_clearance=NUT_HOLDER_INTERFERENCE, bridge_depth=NUT_BRIDGE_DEPTH,
                                rotation=(0, 0, -angle))
        ClearanceHole(NUT, fit=FASTENER_FIT, counter_sunk=False)
    # keyholes cut into the bottom face; slot angles are in the flipped frame (as seen from below)
    with Locations(*[Plane((x, y, 0), x_dir=(1, 0, 0), z_dir=(0, 0, -1)) for x, y in KEYHOLES]):
      KeyholeHole(KEYHOLE_SCREW, fit=FASTENER_FIT, angles=KEYHOLE_ANGLES)
    cut_connectors()
  return builder.part


def make_top() -> Part:
  KNOB_HOLE_DIAMETER = 6.0
  SCREW = CounterSunkScrew(size=FASTENER_SIZE, length=16, fastener_type="iso7046")  # length doesn't affect holes
  OPTICAL_HOLE_DIAMETER = 2.0

  interior_top = ENCLOSURE_HEIGHT - WALL_THICKNESS  # underside of the top face

  cover_height = ENCLOSURE_HEIGHT - BOTTOM_BOSS_POS
  footprint = Pos(0, 0, BOTTOM_BOSS_POS) * extrude(outline(OUTER_OFFSET), amount=cover_height)
  cover = footprint - Pos(0, 0, BOTTOM_BOSS_POS - _EPS) * extrude(outline(INNER_OFFSET),
                                                                  amount=interior_top - BOTTOM_BOSS_POS + _EPS)

  boss = Pos(0, 0, TOP_BOSS_POS) * Cylinder(BOSS_DIAMETER / 2, interior_top - TOP_BOSS_POS + _EPS,
                                       align=(Align.CENTER, Align.CENTER, Align.MIN))
  for x, y, _ in MOUNT_HOLES:
    cover += (Pos(x, y, 0) * boss) & footprint

  with BuildPart() as builder:
    insert(cover)

    with Locations(Pos(0, KNOB_Y, ENCLOSURE_HEIGHT + _EPS)):
      Hole(KNOB_HOLE_DIAMETER / 2 + TOLERANCE)

    # countersunk screw hole cut into the top face
    with Locations(*[Pos(x, y, ENCLOSURE_HEIGHT) for x, y, _ in MOUNT_HOLES]):
      ClearanceHole(SCREW, fit=FASTENER_FIT, counter_sunk=True)
    # light pipe cutouts through the top face, local +X radially outward from the knob
    with Locations(Pos(0, KNOB_Y, ENCLOSURE_HEIGHT)):
      with PolarLocations(LED_RADIUS, LED_COUNT, start_angle=LED_START_ANGLE, rotate=True):
        LightPipeHole(LIGHT_PIPE, clearance=TOLERANCE)
    # optical sensor holes through the top face
    for x, y in OPTICAL_SENSOR_HOLES:
      with Locations(Pos(x, y, ENCLOSURE_HEIGHT + _EPS)):
        Hole(OPTICAL_HOLE_DIAMETER / 2)
    cut_connectors()
  return builder.part


if __name__ == "__main__":
  OUTPUT_DIR = Path(__file__).parent / "generated"
  OUTPUT_DIR.mkdir(exist_ok=True)
  bottom = make_bottom()
  top = make_top()  # also records the global light pipe locations
  light_pipes = make_light_pipe_tree(LIGHT_PIPE, LIGHT_PIPE.hole_locations)

  # (name, part in global coordinates, print rotation, assembly color)
  parts = [
    ("iotknob_bottom", bottom, Rot(0, 0, 0), Color(0.3, 0.3, 0.3)),  # dark grey
    ("iotknob_top", top, Rot(180, 0, 0), Color(0.7, 0.7, 0.7)),  # light grey
    ("iotknob_lightpipes", light_pipes, Rot(180, 0, 0), Color(0.1, 0.4, 1.0, 0.5)),  # blue
  ]

  # individual STLs, in printing orientation
  for name, part, rotation, _ in parts:
    path = OUTPUT_DIR / f"{name}.stl"
    printable = rotation * part
    export_stl(printable, str(path))
    bbox = printable.bounding_box()
    print(f"{path}: {bbox.size.X:.2f} x {bbox.size.Y:.2f} x {bbox.size.Z:.2f} mm, volume {part.volume:.1f} mm^3")

  # composite assembly, all parts in global coordinates, colored per part
  assembly_children = []
  for name, part, _, color in parts:
    for i, solid in enumerate(part.solids()):
      child = Compound([solid])
      child.label = name if len(part.solids()) == 1 else f"{name}_{i}"
      child.color = color
      assembly_children.append(child)
  assembly = Compound(label="iotknob_assembly", children=assembly_children)
  path = OUTPUT_DIR / "iotknob_assembly.step"
  export_step(assembly, str(path))
  print(f"{path}: {len(assembly_children)} parts")
