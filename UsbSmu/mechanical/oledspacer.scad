include <BOSL/shapes.scad>

$fn = $preview? 12 : 48;
e = 0.1;  // epsilon to avoid z-fighting

// Plate design parameters
PLATE_EXTEND = 2.5;  // how far the plate extends in -X, +X, -Y, +Y
PLATE_THICK = 2;

PLATE_OFFSET = 7.2;

OLED_X = 85;
OLED_Y = 18;

OLED_INSET_X = 60.5 + 0.2;
OLED_INSET_YP = 32.408/2 + 0.1;
OLED_INSET_YN = 37 - 32.408/2 + 0.1;

OLED_BOSS_X = 70;
OLED_BOSS_Y = 20;
OLED_BOSS_INSET_D = 3;
OLED_BOSS_INSET_H = 1;

OLED_RIM_D = 1;


OPAX171_H = 1.75;


difference() {
    cuboid(p1=[-20, -OLED_BOSS_Y/2 - OLED_BOSS_INSET_D/2, OPAX171_H], 
           p2=[OLED_BOSS_X/2-OLED_INSET_X/2 + OLED_BOSS_INSET_D/2, OLED_BOSS_Y/2 + OLED_BOSS_INSET_D/2, PLATE_OFFSET],
           edges=EDGES_Z_ALL, fillet=OLED_BOSS_INSET_D/2);
    
    cuboid(p1=[-20-e, -OLED_BOSS_Y/2 - OLED_BOSS_INSET_D/2 - e, PLATE_OFFSET + e], 
           p2=[OLED_RIM_D + 0.1, OLED_BOSS_Y/2 + OLED_BOSS_INSET_D/2 + e, PLATE_OFFSET - OLED_BOSS_INSET_H]);
}

translate([OLED_BOSS_X/2-OLED_INSET_X/2, -OLED_BOSS_Y/2, PLATE_OFFSET])
    cyl(d=OLED_BOSS_INSET_D, h=OLED_BOSS_INSET_H, chamfer2=0.5, align=V_UP);
translate([OLED_BOSS_X/2-OLED_INSET_X/2, OLED_BOSS_Y/2, PLATE_OFFSET])
    cyl(d=OLED_BOSS_INSET_D, h=OLED_BOSS_INSET_H, chamfer2=0.5, align=V_UP);
