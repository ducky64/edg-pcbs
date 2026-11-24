"""
Enclosure generator for the IoT knob PCB.

Coordinates: origin at the PCB center, X across the PCB width, Y along the PCB length (+Y is "top"),
Z up from the bottom face of the enclosure.

Tolerance convention: TOLERANCE is the clearance gap between mating surfaces, per side. It is applied to the
receiving (hole / cavity) features, which are enlarged, while the part features (PCB, shaft) are nominal.
Screw holes use the bd_warehouse clearance-hole fits.
"""
from pathlib import Path

from build123d import (Align, BuildPart, Cylinder, Hole, Locations, Part, Plane, PolarLocations, Pos, RectangleRounded,
                       export_stl, extrude, insert)
from bd_warehouse.fastener import ClearanceHole, CounterSunkScrew, HexNut, PanHeadScrew

from bd_util.connector import MolexKK254Hole, UsbCReceptacleHole, JstShSideEntryHole
from bd_util.fastener import KeyholeHole, PrintableCaptiveNutHole


# PCB definition
PCB_WIDTH = 24.0  # mm, X
PCB_LENGTH = 70.0  # mm, Y
PCB_RADIUS = 5.0  # mm, corner radius
# (x, y, nut angle): mm, 3mm from the PCB rectangle edges; degrees, so a nut hexagon flat faces the PCB corner
MOUNT_HOLES = [(-9.0, -32.0, 15.0), (-9.0, 32.0, -15.0), (9.0, -32.0, -15.0), (9.0, 32.0, 15.0)]
USB_X = 0.0  # mm, on -Y edge
FAN_HEADER_X = 0.0  # mm, on +Y edge
QWIIC_Y = -17  # mm, on +/-X edge
OPTICAL_SENSOR_HOLES = [(-8, -27), (8, -27)]  # mm, (x, y)

# Enclosure definition
TOLERANCE = 0.2  # mm, clearance gap per side between mating surfaces
WALL_THICKNESS = 1.0  # mm, side walls, floor, and top face
BOTTOM_BOSS_POS = 3.0  # mm, z of the bottom boss top surface, where the PCB bottom rests; also the part split
BOARD_TOP_POS = BOTTOM_BOSS_POS + 1.0  # mm, z of the PCB top surface (1mm board)
TOP_BOSS_POS = BOTTOM_BOSS_POS + 8.0  # mm, z of the top boss bottom surface and top PCB top surface
ENCLOSURE_HEIGHT = BOTTOM_BOSS_POS + 12 + WALL_THICKNESS  # mm, z of the top face outer surface

# Mounting screws, top cover to captive nuts in the bottom
FASTENER_SIZE = "M2.5-0.45"
FASTENER_FIT = "Normal"  # bd_warehouse clearance fit, for screw holes: Close, Normal, Loose
BOSS_DIAMETER = 8.0  # mm, nut shelves (bottom) and screw bosses (top); >1mm wall around the nut pocket corners
                     # (5.5mm) and countersunk head (4.7mm), clipped to the enclosure outline

OUTPUT_DIR = Path(__file__).parent / "generated"

_EPS = 0.01  # mm, overlap to avoid coplanar boolean faces

OUTER_OFFSET = TOLERANCE + WALL_THICKNESS  # enclosure outer outline, relative to the PCB outline
INNER_OFFSET = TOLERANCE  # enclosure cavity, relative to the PCB outline


def outline(offset: float) -> RectangleRounded:
  """PCB outline grown outward by offset."""
  return RectangleRounded(PCB_WIDTH + 2 * offset, PCB_LENGTH + 2 * offset, PCB_RADIUS + offset)


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
  NUT_CLEARANCE = -0.1  # mm per flat, positive = clearance, negative = interference (press fit)
  NUT_BRIDGE_DEPTH = 0.4  # mm, bridging strip depth above the nut pocket roof, for support-free printing

  KEYHOLES = [(0.0, -12.0), (0.0, 12.0)]  # mm
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
        PrintableCaptiveNutHole(NUT, fit=FASTENER_FIT, nut_clearance=NUT_CLEARANCE, bridge_depth=NUT_BRIDGE_DEPTH,
                                rotation=(0, 0, -angle))
        ClearanceHole(NUT, fit=FASTENER_FIT, counter_sunk=False)
    # keyholes cut into the bottom face; slot angles are in the flipped frame (as seen from below)
    with Locations(*[Plane((x, y, 0), x_dir=(1, 0, 0), z_dir=(0, 0, -1)) for x, y in KEYHOLES]):
      KeyholeHole(KEYHOLE_SCREW, fit=FASTENER_FIT, angles=KEYHOLE_ANGLES)
    cut_connectors()
  return builder.part


def make_top() -> Part:
  KNOB_Y = -11.0  # mm
  KNOB_HOLE_DIAMETER = 6.0  # mm, nominal (shaft) diameter
  SCREW = CounterSunkScrew(size=FASTENER_SIZE, length=16, fastener_type="iso7046")  # length doesn't affect holes
  LED_COUNT = 6  # evenly spaced around the knob
  LED_RADIUS = 8.0  # mm, from the knob center
  LED_START_ANGLE = 0.0  # degrees from +X, first LED
  LED_HOLE_DIAMETER = 2.0  # mm

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
    # LED light holes through the top face
    with Locations(Pos(0, KNOB_Y, ENCLOSURE_HEIGHT + _EPS)):
      with PolarLocations(LED_RADIUS, LED_COUNT, start_angle=LED_START_ANGLE, rotate=False):
        Hole(LED_HOLE_DIAMETER / 2)
    # optical sensor holes through the top face
    for x, y in OPTICAL_SENSOR_HOLES:
      with Locations(Pos(x, y, ENCLOSURE_HEIGHT + _EPS)):
        Hole(LED_HOLE_DIAMETER / 2)
    cut_connectors()
  return builder.part


if __name__ == "__main__":
  OUTPUT_DIR.mkdir(exist_ok=True)
  for name, part in [("iotknob_bottom", make_bottom()), ("iotknob_top", make_top())]:
    path = OUTPUT_DIR / f"{name}.stl"
    export_stl(part, str(path))
    bbox = part.bounding_box()
    print(f"{path}: z {bbox.min.Z:.2f} to {bbox.max.Z:.2f} mm, volume {part.volume:.1f} mm^3")
