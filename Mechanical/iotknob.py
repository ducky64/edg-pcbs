"""
Enclosure generator for the IoT knob PCB.

Coordinates: origin at the PCB center, X across the PCB width, Y along the PCB length (+Y is "top"),
Z up from the bottom face of the enclosure.

Tolerance convention: TOLERANCE is the clearance gap between mating surfaces, per side. It is applied to the
receiving (hole / cavity) features, which are enlarged, while the part features (PCB, shaft) are nominal.
Screw holes use the bd_warehouse clearance-hole fits.
"""
from pathlib import Path

from build123d import Align, BuildPart, Cylinder, Locations, Part, Pos, RectangleRounded, Rot, export_stl, extrude, insert
from bd_warehouse.fastener import ClearanceHole, CounterSunkScrew, HexNut, PanHeadScrew

from bd_util.fastener import KeyholeHole, PrintableCaptiveNutHole


# PCB definition
PCB_WIDTH = 24.0  # mm, X
PCB_LENGTH = 70.0  # mm, Y
PCB_RADIUS = 5.0  # mm, corner radius
# (x, y, nut angle): mm, 3mm from the PCB rectangle edges; degrees, so a nut hexagon flat faces the PCB corner
MOUNT_HOLES = [(-9.0, -32.0, 15.0), (-9.0, 32.0, -15.0), (9.0, -32.0, -15.0), (9.0, 32.0, 15.0)]

# Enclosure definition
TOLERANCE = 0.2  # mm, clearance gap per side between mating surfaces
WALL_THICKNESS = 1.0  # mm, side walls, floor, and top face
BOTTOM_BOSS_POS = 3.0  # mm, z of the bottom boss top surface, where the PCB bottom rests; also the part split
TOP_BOSS_POS = BOTTOM_BOSS_POS + 8.0  # mm, z of the top boss bottom surface
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
    # nut pocket and screw hole cut into the bottom face (flipped about X, which keeps the nut plan corners on X)
    with Locations(*[Pos(x, y, 0) * Rot(0, 0, angle) * Rot(180, 0, 0) for x, y, angle in MOUNT_HOLES]):
      PrintableCaptiveNutHole(NUT, fit=FASTENER_FIT, nut_clearance=NUT_CLEARANCE, bridge_depth=NUT_BRIDGE_DEPTH)
      ClearanceHole(NUT, fit=FASTENER_FIT, counter_sunk=False)
    # keyholes cut into the bottom face; slot angles are in the flipped frame (as seen from below)
    with Locations(*[Pos(x, y, 0) * Rot(180, 0, 0) for x, y in KEYHOLES]):
      KeyholeHole(KEYHOLE_SCREW, fit=FASTENER_FIT, angles=KEYHOLE_ANGLES)
  return builder.part


def make_top() -> Part:
  KNOB_HOLE_FROM_BOTTOM = 24.0  # mm, from the PCB bottom edge (-Y), on the X centerline
  KNOB_HOLE_DIAMETER = 6.0  # mm, nominal (shaft) diameter
  SCREW = CounterSunkScrew(size=FASTENER_SIZE, length=16, fastener_type="iso7046")  # length doesn't affect holes

  interior_top = ENCLOSURE_HEIGHT - WALL_THICKNESS  # underside of the top face

  cover_height = ENCLOSURE_HEIGHT - BOTTOM_BOSS_POS
  footprint = Pos(0, 0, BOTTOM_BOSS_POS) * extrude(outline(OUTER_OFFSET), amount=cover_height)
  cover = footprint - Pos(0, 0, BOTTOM_BOSS_POS - _EPS) * extrude(outline(INNER_OFFSET),
                                                                  amount=interior_top - BOTTOM_BOSS_POS + _EPS)

  boss = Pos(0, 0, TOP_BOSS_POS) * Cylinder(BOSS_DIAMETER / 2, interior_top - TOP_BOSS_POS + _EPS,
                                       align=(Align.CENTER, Align.CENTER, Align.MIN))
  for x, y, _ in MOUNT_HOLES:
    cover += (Pos(x, y, 0) * boss) & footprint

  knob_y = -PCB_LENGTH / 2 + KNOB_HOLE_FROM_BOTTOM
  cover -= Pos(0, knob_y, 0) * Cylinder(KNOB_HOLE_DIAMETER / 2 + TOLERANCE, 2 * ENCLOSURE_HEIGHT,
                                        align=(Align.CENTER, Align.CENTER, Align.MIN))

  with BuildPart() as builder:
    insert(cover)
    # countersunk screw hole cut into the top face
    with Locations(*[Pos(x, y, ENCLOSURE_HEIGHT) for x, y, _ in MOUNT_HOLES]):
      ClearanceHole(SCREW, fit=FASTENER_FIT, counter_sunk=True)
  return builder.part


if __name__ == "__main__":
  OUTPUT_DIR.mkdir(exist_ok=True)
  for name, part in [("iotknob_bottom", make_bottom()), ("iotknob_top", make_top())]:
    path = OUTPUT_DIR / f"{name}.stl"
    export_stl(part, str(path))
    bbox = part.bounding_box()
    print(f"{path}: z {bbox.min.Z:.2f} to {bbox.max.Z:.2f} mm, volume {part.volume:.1f} mm^3")
