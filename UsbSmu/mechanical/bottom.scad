include <BOSL/shapes.scad>

$fn = $preview? 12 : 48;
e = 0.1;  // epsilon to avoid z-fighting

// PCB parameters
PCB_X = 155;
PCB_Y = 55;

PCB_CORNER_RADIUS = 3;

// Heatsink parameters
HS_START_X = 35;
HS_END_X = 145;
HS_CEN_Y = 12;
HS_Y = 22 + 2;  // 1mm tolerance each side

// Plate design parameters
PLATE_EXTEND = 2.5;  // how far the plate extends in -X, +X, -Y, +Y
PLATE_THICK = 2;

PLATE_OFFSET = 2;

PLATE_RIM_H = 1;
PLATE_RIM_D = 2;

PIN_INSET = 1;

// Derived parameters
// Note, 0, 0 = top left origin of board
PLATE_X = PCB_X + PLATE_EXTEND * 2;
PLATE_Y = PCB_Y + PLATE_EXTEND * 2;
PLATE_RADIUS = PCB_CORNER_RADIUS + PLATE_EXTEND;
SCREW_CLEAR = 2.9;
SCREW_HEAD_THICK = 1.5;

module screw(center) {    
    translate(center) translate([0, 0, -e]) {
      cyl(l=PLATE_THICK + 20 + 2*e, d=SCREW_CLEAR, align=V_UP);
      cyl(l=SCREW_HEAD_THICK + e, d1=SCREW_CLEAR + 2*(SCREW_HEAD_THICK + e),
        d2=SCREW_CLEAR, align=V_UP);
    }
}

module pin(center, d) {    
    translate(center) translate([0, 0, PLATE_THICK + e]) {
      cyl(l=PIN_INSET + e, d=d, align=V_DOWN,
      chamfer1=0.5);
    }
}

module post(center) {
    translate(center) {
        difference() {
            union() {
                cyl(l=PLATE_OFFSET, r=PCB_CORNER_RADIUS, align=V_UP);
                cyl(l=PLATE_OFFSET, r1=PCB_CORNER_RADIUS + PLATE_EXTEND, r2=PCB_CORNER_RADIUS + PLATE_EXTEND - PLATE_OFFSET, align=V_UP);
            }
            
            translate([0, 0, e]) {
                cyl(l=PLATE_OFFSET + 2*e, d=SCREW_CLEAR, align=V_UP);
            }
        }
    }
}

difference() {
    union() {
        cuboid(p1=[-PLATE_EXTEND, -PLATE_EXTEND, 0],
               p2=[PLATE_X-PLATE_EXTEND, PLATE_Y-PLATE_EXTEND, PLATE_THICK],
               edges=EDGES_Z_ALL, fillet=PCB_CORNER_RADIUS + PLATE_EXTEND);
        
        translate([0, 0, PLATE_THICK]) {
            difference() {
                cuboid(p1=[-PLATE_RIM_D/2, -PLATE_RIM_D/2, 0],
                     p2=[PCB_X + PLATE_RIM_D/2, PCB_Y + PLATE_RIM_D/2, PLATE_RIM_H],
                     edges=EDGES_Z_ALL, fillet=PCB_CORNER_RADIUS + PLATE_RIM_D/2);
                cuboid(p1=[PLATE_RIM_D/2, PLATE_RIM_D/2, -e],
                     p2=[PCB_X - PLATE_RIM_D/2, PCB_Y - PLATE_RIM_D/2, PLATE_RIM_H + e],
                     edges=EDGES_Z_ALL, fillet=PCB_CORNER_RADIUS - PLATE_RIM_D/2);
            }
            post([3, 3]);
            post([3, 52]);
            post([152, 3]);
            post([152, 52]);
        }
    }

    screw([3, 3]);
    screw([3, 52]);
    screw([152, 3]);
    screw([152, 52]);
    cuboid(p1=[HS_START_X, PCB_Y - (HS_CEN_Y - HS_Y/2), -10 -e],
           p2=[HS_END_X, PCB_Y - (HS_CEN_Y + HS_Y/2), PLATE_THICK + 10 + e],
           edges=EDGES_Z_ALL, fillet=1);
    
    pin([146.92, PCB_Y - 9.92], 2.6);
    pin([152, PCB_Y - 9.92], 2.6);
    pin([146.92, PCB_Y - 15], 2.6);
    pin([152, PCB_Y - 15], 2.6);
    
    pin([146.92, PCB_Y - 28.92], 2.6);
    pin([152, PCB_Y - 28.92], 2.6);
    pin([146.92, PCB_Y - 34], 2.6);
    pin([152, PCB_Y - 34], 2.6);

    pin([150.9, PCB_Y - 20.73], 1.7);
    pin([150.9, PCB_Y - 23.27], 1.7);
    
    pin([150.9, PCB_Y - 42.46], 1.7);
    pin([150.9, PCB_Y - 45], 1.7);
    pin([150.9, PCB_Y - 47.54], 1.7);
    
    pin([20, PCB_Y - 43], 1.75);
    pin([20, PCB_Y - 45], 1.75);
}
