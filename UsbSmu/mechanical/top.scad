include <BOSL/shapes.scad>

$fn = $preview? 12 : 48;
e = 0.1;  // epsilon to avoid z-fighting

// PCB parameters
PCB_X = 155;
PCB_Y = 55;

PCB_CORNER_RADIUS = 3;

// OLED parameters

OLED_CUT_X = 57.01 + 1;
OLED_CUT_Y = 29.49 + 1;
OLED_INSET_X = 60.5 + 0.2;
OLED_INSET_YP = 32.408/2 + 0.1;
OLED_INSET_YN = 37 - 32.408/2 + 0.1;
OLED_INSET_H = 1;
OLED_RIBBON_X = 12.5 + 0.2;
OLED_RIBBON_Y = 4;
OLED_RIBBON_H = 1;

OLED_X = 85;
OLED_Y = 18;

OLED_BOSS_X = 70;
OLED_BOSS_Y = 20;
OLED_BOSS_INSET_D = 3.2;
OLED_BOSS_INSET_H = 1;

OLED_RIM_H = 1;
OLED_RIM_D = 1;

// UI parameters
DIRSW_D = 5.2;
DIRSW_TRAVEL = 1.5;  // travel in each direction

// Plate design parameters
PLATE_EXTEND = 2.5;  // how far the plate extends in -X, +X, -Y, +Y
PLATE_THICK = 2;

PLATE_OFFSET = 7.2;

PLATE_RIM_H = 3;
PLATE_RIM_D = 2;

LED1_X = 62;
LED2_X = 66;
LED_Y = 45;
LED_H = PLATE_OFFSET - 0.5;
LED_D = 3.2;
LED_PIPE_D = 6;


// Derived parameters
// Note, 0, 0 = top left origin of board
PLATE_X = PCB_X + PLATE_EXTEND * 2;
PLATE_Y = PCB_Y + PLATE_EXTEND * 2;
PLATE_RADIUS = PCB_CORNER_RADIUS + PLATE_EXTEND;
SCREW_CLEAR = 2.9;
SCREW_HEAD_H = 2;

module screw(center) {    
    translate(center) translate([0, 0, PLATE_THICK + e]) {
      cyl(l=PLATE_THICK + 2*e, d=SCREW_CLEAR, align=V_DOWN);
      cyl(l=SCREW_HEAD_H + e, d2=SCREW_CLEAR + 2*(SCREW_HEAD_H + e),
        d1=SCREW_CLEAR, align=V_DOWN);
    }
}

module output_jack(pin1) {
    translate(pin1) translate([0, 0, -10]) {
        // left edge used to be -1.3 - 0.2 as outer edge of ring + tolerance
        // but now we use -0.8 + 0.1 as the drill edge + interference fit
        cuboid(p1=[-0.8+0.1, -1.3-5.08-0.2, 0],
            p2=[100, 1.3+0.2, PLATE_THICK + 20],
            edges=EDGES_Z_ALL, fillet=1.3+0.2);
        linear_extrude(height=PLATE_THICK + 20, center=false) {
            polygon(points=[
                [5.76-0.2, 0+0.2],
                [5.76-0.2, 2.21+0.2],
                [14.87+0.2, 4.21+0.2],
                [14.87+0.2, -9.29-0.2],
                [5.76-0.2, -7.29-0.2],
                [5.76-0.2, -5.08-0.2]
            ]);
        };
    }
}

module post(center) {
    translate(center) {
        difference() {
            union() {
                cyl(l=PLATE_OFFSET, r=PCB_CORNER_RADIUS, align=V_DOWN);
                cyl(l=PCB_CORNER_RADIUS + PLATE_EXTEND, r2=PCB_CORNER_RADIUS + PLATE_EXTEND, r1=0, align=V_DOWN);
            }
            
            translate([0, 0, e]) {
                cyl(l=PLATE_OFFSET + 2*e, d=SCREW_CLEAR, align=V_DOWN);
            }
        }
    }
}

difference() {
    union() {
        cuboid(p1=[-PLATE_EXTEND, -PLATE_EXTEND, 0],
               p2=[PLATE_X-PLATE_EXTEND, PLATE_Y-PLATE_EXTEND, PLATE_THICK],
               edges=EDGES_Z_ALL, fillet=PCB_CORNER_RADIUS + PLATE_EXTEND);
        
        difference() {
            cuboid(p1=[-PLATE_RIM_D/2, -PLATE_RIM_D/2, 0],
                 p2=[PCB_X + PLATE_RIM_D/2, PCB_Y + PLATE_RIM_D/2, -PLATE_RIM_H],
                 edges=EDGES_Z_ALL, fillet=PCB_CORNER_RADIUS + PLATE_RIM_D/2);
            cuboid(p1=[PLATE_RIM_D/2, PLATE_RIM_D/2, e],
                 p2=[PCB_X - PLATE_RIM_D/2, PCB_Y - PLATE_RIM_D/2, -PLATE_RIM_H - e],
                 edges=EDGES_Z_ALL, fillet=PCB_CORNER_RADIUS - PLATE_RIM_D/2);
        }
        
        translate([OLED_X, PCB_Y - OLED_Y]) {
            cuboid(p1=[-OLED_INSET_X/2 - OLED_RIM_D, -OLED_INSET_YN-OLED_RIM_D, e],
                   p2=[OLED_INSET_X/2 + OLED_RIM_D, -OLED_INSET_YN+OLED_RIM_D*4, -OLED_RIM_H]);
            cuboid(p1=[-OLED_INSET_X/2 - OLED_RIM_D, OLED_INSET_YP-OLED_RIM_D*2, e],
                   p2=[OLED_INSET_X/2 + OLED_RIM_D, OLED_INSET_YP+OLED_RIM_D, -OLED_RIM_H]);
        }
        
        translate([LED1_X, PCB_Y-45, 0]) {  // led lightpipe
            cuboid(p1=[-LED_PIPE_D/2, -LED_PIPE_D/2, 0],
                   p2=[(LED2_X - LED1_X) + LED_PIPE_D/2, LED_PIPE_D/2, -LED_H],
                   edges=EDGES_Z_ALL, fillet=LED_PIPE_D/2);
        }
        
        post([3, 3]);
        post([3, 52]);
        post([152, 3]);
        post([152, 52]);
    }

    screw([3, 3]);
    screw([3, 52]);
    screw([152, 3]);
    screw([152, 52]);

    translate([7, PCB_Y-27, -e])  // capacitor
        cyl(l=PLATE_THICK + 2*e, d=8+0.4, align=V_UP);
    
    // OLED boss
    translate([OLED_X, PCB_Y - OLED_Y]) {
        translate([-OLED_BOSS_X/2, -OLED_BOSS_Y/2, -e])
            cyl(l=OLED_BOSS_INSET_H + e, d=OLED_BOSS_INSET_D, chamfer2=OLED_BOSS_INSET_H/2, align=V_UP);
        translate([OLED_BOSS_X/2, -OLED_BOSS_Y/2, -e])
            cyl(l=OLED_BOSS_INSET_H + e, d=OLED_BOSS_INSET_D, chamfer2=OLED_BOSS_INSET_H/2, align=V_UP);
        translate([-OLED_BOSS_X/2, OLED_BOSS_Y/2, -e])
            cyl(l=OLED_BOSS_INSET_H + e, d=OLED_BOSS_INSET_D, chamfer2=OLED_BOSS_INSET_H/2, align=V_UP);
        translate([OLED_BOSS_X/2, OLED_BOSS_Y/2, -e])
            cyl(l=OLED_BOSS_INSET_H + e, d=OLED_BOSS_INSET_D, chamfer2=OLED_BOSS_INSET_H/2, align=V_UP);
        
        cuboid(p1=[-OLED_CUT_X/2, -OLED_CUT_Y/2, -10-e],
               p2=[OLED_CUT_X/2, OLED_CUT_Y/2, PLATE_THICK + 10],
               edges=EDGES_Z_ALL, fillet=1);
        cuboid(p1=[-OLED_INSET_X/2, -OLED_INSET_YN, -10-e],
               p2=[OLED_INSET_X/2, OLED_INSET_YP, OLED_INSET_H]);
        cuboid(p1=[-OLED_RIBBON_X/2, -OLED_INSET_YN + e, -10-e],
               p2=[OLED_RIBBON_X/2, -OLED_INSET_YN - OLED_RIBBON_Y, OLED_RIBBON_H]);
        
        translate([-OLED_INSET_X/2, -OLED_INSET_YN, -10-e])
            cyl(l=OLED_RIBBON_H + 10 + e, d=1, align=V_UP);
        translate([OLED_INSET_X/2, -OLED_INSET_YN, -10-e])
            cyl(l=OLED_RIBBON_H + 10 + e, d=1, align=V_UP);
        translate([-OLED_INSET_X/2, OLED_INSET_YP, -10-e])
            cyl(l=OLED_RIBBON_H + 10 + e, d=1, align=V_UP);
        translate([OLED_INSET_X/2, OLED_INSET_YP, -10-e])
            cyl(l=OLED_RIBBON_H + 10 + e, d=1, align=V_UP);
    }
    
    output_jack([146.92, PCB_Y-9.92]);
    output_jack([146.92, PCB_Y-28.92]);
    
    translate([117, PCB_Y-45, -10-e])  // rotary encoder
        cyl(l=PLATE_THICK + 10 + 2*e, d=7+0.4, align=V_UP);
    
    translate([53, PCB_Y-45, -e]) {  // directional switch
        cuboid(p1=[-DIRSW_D/2-DIRSW_TRAVEL, -DIRSW_D/2, -10],
               p2=[DIRSW_D/2+DIRSW_TRAVEL, DIRSW_D/2, PLATE_THICK + 2*e],
               edges=EDGES_Z_ALL, fillet=DIRSW_D/2);
        cuboid(p1=[-DIRSW_D/2, -DIRSW_D/2-DIRSW_TRAVEL, -10],
               p2=[DIRSW_D/2, DIRSW_D/2+DIRSW_TRAVEL, PLATE_THICK + 2*e],
               edges=EDGES_Z_ALL, fillet=DIRSW_D/2);
    }

    translate([LED1_X, PCB_Y-LED_Y, -LED_H-e])  // RGB LED
        cyl(l=PLATE_THICK + LED_H + 2*e, d=LED_D, align=V_UP);
    translate([LED2_X, PCB_Y-LED_Y, -LED_H-e])  // debug LED
        cyl(l=PLATE_THICK + LED_H + 2*e, d=LED_D, align=V_UP);
    
    translate([0, PCB_Y-12, PLATE_THICK - 0.4]) {
        linear_extrude(1 + e)
            text("USB", size=5, halign="left", valign="center");
    }    
    translate([143, PCB_Y-12.460, PLATE_THICK - 0.4]) {
        linear_extrude(1 + e)
            text("GND", size=5, halign="right", valign="center");
    }
    translate([143, PCB_Y-31.459, PLATE_THICK - 0.4]) {
        linear_extrude(1 + e)
            text("OUT", size=5, halign="right", valign="center");
    }
}
