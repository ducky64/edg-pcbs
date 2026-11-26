"""
Enclosure for the IoT knob PCB.

Origin at the PCB center, X across the width, Y along the length (+Y is "top"), Z up from the enclosure bottom.
Dimensions in mm, angles in degrees. Tolerance enlarges receiving features (holes, cavities); parts are nominal.
"""
from math import degrees
from pathlib import Path

from build123d import (Align, BuildPart, Color, Compound, Cylinder, Edge, Face, Hole, Location, Locations, Part, Plane,
                       PolarLocations, Pos, Rectangle, RectangleRounded, Rot, Shape, SlotArc, SlotOverall, Solid,
                       export_step, export_stl, extrude, insert)
from bd_warehouse.fastener import ClearanceHole, CounterSunkScrew, HexNut, PanHeadScrew

from bd_util.connector import MolexKK254Hole, UsbCReceptacleHole, JstShSideEntryHole
from bd_util.fastener import KeyholeHole, PrintableCaptiveNutHole
from bd_util.light_pipe import CrushRibs, DiffusionCone, Flange, LightPipe, LightPipeHole, fuse_light_pipes


# PCB definition
PCB_WIDTH = 24.0  # X
PCB_LENGTH = 70.0  # Y
PCB_RADIUS = 4.0
# (x, y, nut angle), with a nut flat facing the PCB corner
MOUNT_HOLES = [(-9.0, -32.0, 15.0), (-9.0, 32.0, -15.0), (9.0, -32.0, -15.0), (9.0, 32.0, 15.0)]
USB_X = 0.0  # on -Y edge
FAN_HEADER_X = 0.0  # on +Y edge
QWIIC_Y = -17  # on +/-X edge
OPTICAL_SENSOR_HOLES = [(-8, -27), (8, -27)]
KNOB_Y = -7.0  # encoder shaft
LED_COUNT = 6  # around the knob
LED_RADIUS = 8.0  # placement around knob center
LED_START_ANGLE = 30.0

# Enclosure definition
WALL_THICKNESS = 1.5
BOSS_DIAMETER = 8.0  # clipped to the enclosure outline

# absolute Z
BOTTOM_BOSS_Z = 3.0  # also the PCB bottom
BOARD_TOP_Z = BOTTOM_BOSS_Z + 1.0  # bottom PCB
ENCLOSURE_SPLIT_Z = BOARD_TOP_Z
TOP_BOARD_TOP_Z = BOTTOM_BOSS_Z + 8.0  # top PCB
TOP_BOSS_Z = TOP_BOARD_TOP_Z + 2.0  # floats above the board
ENCLOSURE_TOP_Z = BOTTOM_BOSS_Z + 12 + WALL_THICKNESS  # outer surface

# top cover screws into captive nuts in the bottom
FASTENER_SIZE = "M2.5-0.45"
FASTENER_FIT = "Normal"

# Manufacturability constants
TOLERANCE = 0.2  # per side, between mating surfaces
NUT_HOLDER_INTERFERENCE = -0.1
OVERHANG_ANGLE = 60.0
_EPS = 0.01  # avoids coplanar boolean faces


OUTER_OFFSET = TOLERANCE + WALL_THICKNESS
INNER_OFFSET = TOLERANCE


def outline(offset: float) -> RectangleRounded:
  """PCB outline grown outward by offset."""
  return RectangleRounded(PCB_WIDTH + 2 * offset, PCB_LENGTH + 2 * offset, PCB_RADIUS + offset)


def make_light_pipe() -> LightPipe:
  """Origin at the LED on the board top, +X radially outward."""
  LED_SLOT_LENGTH = 4.0  # circumferential length, including rounds
  LED_SLOT_WIDTH = 2.0
  LIGHT_PIPE_BOARD_LENGTH = 4.0
  LIGHT_PIPE_BOARD_WIDTH = 2.0
  LED_CAVITY = 2.2
  LED_CAVITY_HEIGHT = 1.4
  FLANGE_EXTENSION = 1.0
  DIFFUSION_CONE_DIAMETER = 1.5
  CHAMFER_LENGTH = 0.5

  # pipe top profile curved along the LED circle
  slot_arc_angle = degrees((LED_SLOT_LENGTH - LED_SLOT_WIDTH) / LED_RADIUS)  # centerline arc, excluding end rounds
  slot_arc = Edge.make_circle(LED_RADIUS, Plane((-LED_RADIUS, 0, 0)),
                              start_angle=-slot_arc_angle / 2, end_angle=slot_arc_angle / 2)
  # crush ribs at the slot ends
  led_circle = Edge.make_circle(LED_RADIUS, Plane((-LED_RADIUS, 0, 0)))
  return LightPipe(
    top_profile=SlotArc(slot_arc, LED_SLOT_WIDTH),
    board_profile=Rot(0, 0, 90) * SlotOverall(LIGHT_PIPE_BOARD_LENGTH, LIGHT_PIPE_BOARD_WIDTH),
    height=ENCLOSURE_TOP_Z - TOP_BOARD_TOP_Z,  # pipe top flush with the top face
    wall_thickness=WALL_THICKNESS,
    led_profile=Rectangle(LED_CAVITY, LED_CAVITY),
    led_height=LED_CAVITY_HEIGHT,
    flange=Flange(FLANGE_EXTENSION, draft_angle=OVERHANG_ANGLE),
    crush_ribs=CrushRibs(along=[led_circle]),
    diffusion_cone=DiffusionCone(DIFFUSION_CONE_DIAMETER),
    chamfer_length=CHAMFER_LENGTH,
  )


LIGHT_PIPE = make_light_pipe()


def cut_connectors() -> None:
  """Shared by both parts, since openings may span the split."""
  wall_x = PCB_WIDTH / 2  # approx inner wall surface
  wall_y = PCB_LENGTH / 2
  # -Y wall
  with Locations(Plane((USB_X, -wall_y, BOARD_TOP_Z), x_dir=(-1, 0, 0), z_dir=(0, 1, 0))):
    UsbCReceptacleHole(clearance=TOLERANCE)
  # +Y wall
  with Locations(Plane((FAN_HEADER_X, wall_y, BOARD_TOP_Z), x_dir=(1, 0, 0), z_dir=(0, -1, 0))):
    MolexKK254Hole(4, clearance=TOLERANCE)
  # +/-X walls
  for x, x_dir, z_dir in [(wall_x, (0, -1, 0), (-1, 0, 0)), (-wall_x, (0, 1, 0), (1, 0, 0))]:
    with Locations(Plane((x, QWIIC_Y, TOP_BOARD_TOP_Z), x_dir=x_dir, z_dir=z_dir)):
      JstShSideEntryHole(4, clearance=TOLERANCE)


def make_bottom() -> Part:
  NUT = HexNut(size=FASTENER_SIZE, fastener_type="iso4032")
  NUT_BRIDGE_DEPTH = 0.4

  KEYHOLES = [(0.0, -12.0), (0.0, 12.0)]
  KEYHOLE_ANGLES = [-90, 90]  # as seen from below
  KEYHOLE_SCREW = PanHeadScrew(size="M2.5-0.45", length=10, fastener_type="iso1580")  # length doesn't affect holes

  footprint = extrude(outline(OUTER_OFFSET), amount=ENCLOSURE_SPLIT_Z)
  tray = footprint - Pos(0, 0, WALL_THICKNESS) * extrude(outline(INNER_OFFSET), amount=ENCLOSURE_SPLIT_Z)

  boss = Cylinder(BOSS_DIAMETER / 2, BOTTOM_BOSS_Z, align=(Align.CENTER, Align.CENTER, Align.MIN))
  for x, y, _ in MOUNT_HOLES:
    tray += (Pos(x, y, 0) * boss) & footprint

  with BuildPart() as builder:
    insert(tray)
    # the bottom face plane is flipped, so negate the rotation to give the nut angle as seen from above
    for x, y, angle in MOUNT_HOLES:
      with Locations(Plane((x, y, 0), x_dir=(1, 0, 0), z_dir=(0, 0, -1))):
        PrintableCaptiveNutHole(NUT, fit=FASTENER_FIT, nut_clearance=NUT_HOLDER_INTERFERENCE, bridge_depth=NUT_BRIDGE_DEPTH,
                                rotation=(0, 0, -angle))
        ClearanceHole(NUT, fit=FASTENER_FIT, counter_sunk=False)
    with Locations(*[Plane((x, y, 0), x_dir=(1, 0, 0), z_dir=(0, 0, -1)) for x, y in KEYHOLES]):
      KeyholeHole(KEYHOLE_SCREW, fit=FASTENER_FIT, angles=KEYHOLE_ANGLES)
    cut_connectors()
  return builder.part


def make_top() -> Part:
  KNOB_HOLE_DIAMETER = 10.0
  SCREW = CounterSunkScrew(size=FASTENER_SIZE, length=16, fastener_type="iso7046")  # length doesn't affect holes
  OPTICAL_HOLE_DIAMETER = 4.0

  interior_top = ENCLOSURE_TOP_Z - WALL_THICKNESS

  cover_height = ENCLOSURE_TOP_Z - ENCLOSURE_SPLIT_Z
  footprint = Pos(0, 0, ENCLOSURE_SPLIT_Z) * extrude(outline(OUTER_OFFSET), amount=cover_height)
  cover = footprint - Pos(0, 0, ENCLOSURE_SPLIT_Z - _EPS) * extrude(outline(INNER_OFFSET),
                                                                    amount=interior_top - ENCLOSURE_SPLIT_Z + _EPS)

  boss = Pos(0, 0, TOP_BOSS_Z) * Cylinder(BOSS_DIAMETER / 2, interior_top - TOP_BOSS_Z + _EPS,
                                          align=(Align.CENTER, Align.CENTER, Align.MIN))
  for x, y, _ in MOUNT_HOLES:
    cover += (Pos(x, y, 0) * boss) & footprint

  with BuildPart() as builder:
    insert(cover)

    with Locations(Pos(0, KNOB_Y, ENCLOSURE_TOP_Z + _EPS)):
      Hole(KNOB_HOLE_DIAMETER / 2 + TOLERANCE)

    with Locations(*[Pos(x, y, ENCLOSURE_TOP_Z) for x, y, _ in MOUNT_HOLES]):
      ClearanceHole(SCREW, fit=FASTENER_FIT, counter_sunk=True)
    # at the LEDs on the board, local +X radially outward
    with Locations(Pos(0, KNOB_Y, TOP_BOARD_TOP_Z)):
      with PolarLocations(LED_RADIUS, LED_COUNT, start_angle=LED_START_ANGLE, rotate=True):
        LightPipeHole(LIGHT_PIPE, clearance=TOLERANCE)
    for x, y in OPTICAL_SENSOR_HOLES:
      with Locations(Pos(x, y, ENCLOSURE_TOP_Z + _EPS)):
        Hole(OPTICAL_HOLE_DIAMETER / 2)
    cut_connectors()
  return builder.part


def make_light_pipe_tree() -> Part:
  """Light pipes joined into one piece by bridging bars."""
  BRIDGE_WIDTH = 0.8  # narrow, limiting crosstalk
  BRIDGE_THICKNESS = 1.0

  led_circle = Edge.make_circle(LED_RADIUS, Plane((-LED_RADIUS, 0, 0)))  # local frame
  # local +Y is counterclockwise around the knob
  cw_tip, ccw_tip = sorted(LIGHT_PIPE.top_profile.outer_wire().intersect(led_circle).vertices(), key=lambda v: v.Y)
  locations = LIGHT_PIPE.hole_locations
  bars = []
  for start, end in zip(locations, locations[1:] + locations[:1]):
    a = (start * Pos(ccw_tip.center())).position
    b = (end * Pos(cw_tip.center())).position
    z = start.position.Z + LIGHT_PIPE.loft_height  # bar top, at the wall inner surface
    centerline = Edge.make_line((a.X, a.Y, z), (b.X, b.Y, z))
    bars.append(Solid.extrude(Face(centerline.offset_2d(BRIDGE_WIDTH / 2)), (0, 0, -BRIDGE_THICKNESS)))
  return fuse_light_pipes(LIGHT_PIPE, locations, *bars)


if __name__ == "__main__":
  OUTPUT_DIR = Path(__file__).parent / "generated"
  OUTPUT_DIR.mkdir(exist_ok=True)
  bottom = make_bottom()
  top = make_top()  # also records the global light pipe locations
  light_pipes = make_light_pipe_tree()

  # (name, part in global coordinates, print rotation, assembly color)
  parts = [
    ("iotknob_bottom", bottom, Rot(0, 0, 0), Color(0.3, 0.3, 0.3)),
    ("iotknob_top", top, Rot(180, 0, 0), Color(0.7, 0.7, 0.7)),
    ("iotknob_lightpipes", light_pipes, Rot(180, 0, 0), Color(0.1, 0.4, 1.0, 0.5)),
  ]

  # STLs in printing orientation
  for name, part, rotation, _ in parts:
    path = OUTPUT_DIR / f"{name}.stl"
    printable = rotation * part
    export_stl(printable, str(path))
    bbox = printable.bounding_box()
    print(f"{path}: {bbox.size.X:.2f} x {bbox.size.Y:.2f} x {bbox.size.Z:.2f} mm, volume {part.volume:.1f} mm^3")

  # assembly in global coordinates
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
