include <BOSL/shapes.scad>

$fn = $preview? 12 : 48;
e = 0.1;  // epsilon to avoid z-fighting

// PCB / device parameters
PLATE_THICK = 2;

PLATE_OFFSET = 7.2;

// Switch parameters
STUB_L = 1.95 + 0.2;
STUB_R = 0.5 + 0.1;  // radius
STUB_H_TOP = 5 + 0.1;
STUB_H_BOT = 5-2;  // offset from 0, bottom of adapter
STUB_ROT_CEN_H = 0.49;  // offset from 0

// Knob parameters
KNOB_D = 5;
KNOB_EXT = 2;

KNOB_BASE_D = 9;
STUB_ENTRY_CHAMFER_H = 0.5;

difference() {
    union() {
        intersection() {
            translate([0, 0, STUB_H_BOT])
                cyl(l=PLATE_OFFSET - STUB_H_BOT, d=KNOB_BASE_D, align=V_UP);
            translate([0, 0, STUB_ROT_CEN_H])
                sphere(r=PLATE_OFFSET - STUB_ROT_CEN_H);
        }
        translate([0, 0, STUB_H_BOT])
            cyl(l=PLATE_OFFSET - STUB_H_BOT + PLATE_THICK + KNOB_EXT, d=KNOB_D, align=V_UP,
                fillet=0.5);
    }
    cuboid(p1=[-STUB_L/2, -STUB_L/2, STUB_H_BOT-e], p2=[STUB_L/2, STUB_L/2, STUB_H_TOP],
        edges=EDGES_Z_ALL, fillet=STUB_R);
    translate([0, 0, STUB_H_BOT-e])
        cyl(l=STUB_ENTRY_CHAMFER_H+e, d1=STUB_L+STUB_ENTRY_CHAMFER_H*2, d2=STUB_L, align=V_UP);   
}
