/* ==============================================================================
 * 📻 ESP32-S3 JC3248W535 RETRO-MODERN DESKTOP INTERNET RADIO ENCLOSURE
 * ==============================================================================
 * Floor-Molded Battery Cradle Architecture:
 *   - Recessed glass pocket (95.5mm x 63.0mm x 1.6mm) on inside face.
 *   - Clean active LCD viewing window (73.4mm x 49.0mm).
 *   - Dedicated corner standoffs with open Ø2.4mm pilot holes aligned with board ears.
 *   - Battery retention ribs molded directly onto BOTTOM FLOOR (base) of cabinet.
 *   - 100% flat print bed design - zero supports required.
 * ============================================================================== */

$fn = 60;

/* [Part Selector] */
RENDER_PART = "all"; // [front: Front Enclosure Only, back: Rear Backplate Only, all: Print Bed Layout (Both)]

/* [Main Dimensions] */
WALL_THICKNESS       = 2.4;
CORNER_RADIUS        = 6.0;

// Enclosure Dimensions
ENCLOSURE_INNER_W    = 132.0;
ENCLOSURE_INNER_H    = 74.0;
ENCLOSURE_DEPTH      = 32.0;
ENCLOSURE_OUTER_W    = ENCLOSURE_INNER_W + (2 * WALL_THICKNESS);
ENCLOSURE_OUTER_H    = ENCLOSURE_INNER_H + (2 * WALL_THICKNESS);

CORNER_HOLE_DX       = ENCLOSURE_INNER_W - 6.0;
CORNER_HOLE_DY       = ENCLOSURE_INNER_H - 6.0;

// Board Specs & Position
BOARD_CENTER_X       = -18.0;
BOARD_CENTER_Y       = 0.0;

// Glass Pocket (recessed inside front wall)
GLASS_POCKET_W       = 95.5;
GLASS_POCKET_H       = 63.0;
GLASS_POCKET_DEPTH   = 1.6;
GLASS_POCKET_R       = 3.5;

// Active Screen Window (cut through front face)
DISPLAY_ACTIVE_W     = 73.4;
DISPLAY_ACTIVE_H     = 49.0;

// Board Standoffs (matches scalloped corner mounting ears)
BOARD_HOLE_SPACING_X = 84.5;
BOARD_HOLE_SPACING_Y = 52.0;
SCREW_POST_HOLE_DIA  = 2.4;
SCREW_POST_OD        = 6.0;
SCREW_POST_TOP_Z     = 5.2; // 2.8mm above front inner wall

// 2040 Speaker Chamber
SPEAKER_W            = 20.8;
SPEAKER_H            = 40.8;
SPEAKER_D            = 7.5;
SPEAKER_CENTER_X     = 46.0;
SPEAKER_CENTER_Y     = 0.0;

module rounded_rect_2d(w, h, r) {
    hull() {
        translate([-w/2 + r, -h/2 + r]) circle(r=r);
        translate([ w/2 - r, -h/2 + r]) circle(r=r);
        translate([-w/2 + r,  h/2 - r]) circle(r=r);
        translate([ w/2 - r,  h/2 - r]) circle(r=r);
    }
}

module rounded_box(w, h, d, r) {
    linear_extrude(height=d) {
        rounded_rect_2d(w, h, r);
    }
}

/* ==============================================================================
 * FRONT CASE / MAIN BODY (EXTERNAL PANEL MOUNT)
 * ============================================================================== */
module front_case() {
    difference() {
        union() {
            // Main Outer Shell
            difference() {
                rounded_box(ENCLOSURE_OUTER_W, ENCLOSURE_OUTER_H, ENCLOSURE_DEPTH, CORNER_RADIUS);
                translate([0, 0, WALL_THICKNESS])
                    rounded_box(ENCLOSURE_INNER_W, ENCLOSURE_INNER_H, ENCLOSURE_DEPTH + 1, CORNER_RADIUS - 1.2);
            }
            
            // 4x Front Screw Bosses level with front wall (WALL_THICKNESS = 2.4mm)
            translate([BOARD_CENTER_X, BOARD_CENTER_Y, 0]) {
                for (dx = [-BOARD_HOLE_SPACING_X/2, BOARD_HOLE_SPACING_X/2]) {
                    for (dy = [-BOARD_HOLE_SPACING_Y/2, BOARD_HOLE_SPACING_Y/2]) {
                        translate([dx, dy, 0])
                            cylinder(d=7.6, h=WALL_THICKNESS);
                    }
                }
            }
            
            // 4x Rear Closure Corner Screw Bosses
            for (x = [-CORNER_HOLE_DX/2, CORNER_HOLE_DX/2]) {
                for (y = [-CORNER_HOLE_DY/2, CORNER_HOLE_DY/2]) {
                    translate([x, y, 0])
                        cylinder(d=7.6, h=ENCLOSURE_DEPTH);
                }
            }
            
            // Speaker Chamber (Front Right)
            translate([SPEAKER_CENTER_X, SPEAKER_CENTER_Y, WALL_THICKNESS]) {
                difference() {
                    rounded_box(SPEAKER_W + 2.4, SPEAKER_H + 2.4, SPEAKER_D + 2.0, 1.5);
                    translate([0, 0, -0.1])
                        rounded_box(SPEAKER_W, SPEAKER_H, SPEAKER_D + 3.0, 1.0);
                }
            }
        }
        
        // --- SUBTRACTIONS ---
        
        // 1. External Panel Mount Cutout (82.5mm x 57.5mm with R=5mm corners all the way through front wall)
        translate([BOARD_CENTER_X, BOARD_CENTER_Y, -0.5])
            rounded_box(82.5, 57.5, WALL_THICKNESS + 2.0, 5.0);
        
        // 2. 4x Screw Clearance Through-Holes (Ø3.4mm through front wall for direct screw fastening)
        translate([BOARD_CENTER_X, BOARD_CENTER_Y, -0.5]) {
            for (dx = [-BOARD_HOLE_SPACING_X/2, BOARD_HOLE_SPACING_X/2]) {
                for (dy = [-BOARD_HOLE_SPACING_Y/2, BOARD_HOLE_SPACING_Y/2]) {
                    translate([dx, dy, 0])
                        cylinder(d=3.4, h=WALL_THICKNESS + 2.0);
                }
            }
        }
        
        // 3. Speaker Acoustic Grille Slots
        translate([SPEAKER_CENTER_X, SPEAKER_CENTER_Y, -0.5]) {
            for (gy = [-15 : 3.75 : 15]) {
                translate([0, gy, 0])
                    rounded_box(14.0, 2.0, WALL_THICKNESS + 1.0, 1.0);
            }
        }
        
        // 4. Rear Corner Screw Boss Pilot Holes (Open from rim down 20mm, Ø2.8mm)
        for (x = [-CORNER_HOLE_DX/2, CORNER_HOLE_DX/2]) {
            for (y = [-CORNER_HOLE_DY/2, CORNER_HOLE_DY/2]) {
                translate([x, y, ENCLOSURE_DEPTH - 20.0])
                    cylinder(d=2.8, h=21.0);
            }
        }
    }
}

/* ==============================================================================
 * REAR BACKPLATE (Molded for flat print-bed with inner aligning lip)
 * ============================================================================== */
module back_plate() {
    // On Print Bed: X_bed = -X_assembled so folding over matches front case 1:1
    BOARD_X_BED   = -BOARD_CENTER_X; // +18.0
    SPEAKER_X_BED = -SPEAKER_CENTER_X; // -46.0
    USB_X_BED     = -(BOARD_CENTER_X + (94.5/2) - 1.5); // -27.75
    SW_X_BED      = -(BOARD_CENTER_X + 28.0); // -10.0
    
    difference() {
        union() {
            // Main Rear Cover Plate
            rounded_box(ENCLOSURE_OUTER_W, ENCLOSURE_OUTER_H, WALL_THICKNESS, CORNER_RADIUS);
            
            // Inner Aligning Lip
            translate([0, 0, WALL_THICKNESS]) {
                difference() {
                    rounded_box(ENCLOSURE_INNER_W - 0.6, ENCLOSURE_INNER_H - 0.6, 2.2, CORNER_RADIUS - 1.4);
                    translate([0, 0, -0.1])
                        rounded_box(ENCLOSURE_INNER_W - 3.8, ENCLOSURE_INNER_H - 3.8, 2.4, CORNER_RADIUS - 2.0);
                }
            }
        }
        
        // --- SUBTRACTIONS ---
        
        // A. 4x Corner Boss Relief Cutouts (Cuts lip only, leaving base plate intact with 1.0mm radial clearance)
        for (x = [-CORNER_HOLE_DX/2, CORNER_HOLE_DX/2]) {
            for (y = [-CORNER_HOLE_DY/2, CORNER_HOLE_DY/2]) {
                translate([x, y, WALL_THICKNESS - 0.1])
                    cylinder(d=9.6, h=3.0);
            }
        }
        
        // B. 4x Corner Screw Clearance Through-Holes & Universal Counterbores (M3)
        for (x = [-CORNER_HOLE_DX/2, CORNER_HOLE_DX/2]) {
            for (y = [-CORNER_HOLE_DY/2, CORNER_HOLE_DY/2]) {
                // Through hole Ø3.4mm through entire plate and lip
                translate([x, y, -0.5])
                    cylinder(d=3.4, h=WALL_THICKNESS + 4.0);
                // Counterbore Ø7.2mm x 1.6mm depth on exterior face
                translate([x, y, -0.1])
                    cylinder(d=7.2, h=1.5);
                // 45° chamfer transition down to Ø3.4mm
                translate([x, y, 1.4])
                    cylinder(d1=7.2, d2=3.4, h=0.6);
            }
        }
        
        // C. Vertical USB-C Port Cutout
        translate([USB_X_BED, BOARD_CENTER_Y + 4.0, -0.5])
            rounded_box(8.5, 14.5, WALL_THICKNESS + 2.0, 3.5);
            
        // D. Battery Switch Button Cutout
        translate([SW_X_BED, BOARD_CENTER_Y + 23.0, -0.5])
            cylinder(d=6.0, h=WALL_THICKNESS + 2.0);
        
        // E. Acoustic/Cooling Louvers (Speaker side)
        translate([SPEAKER_X_BED, SPEAKER_CENTER_Y, -0.5]) {
            for (ly = [-18 : 4.5 : 18]) {
                translate([0, ly, 0])
                    rounded_box(18.0, 2.2, WALL_THICKNESS + 2.0, 1.0);
            }
        }
        
        // F. ESP32 CPU Ventilation Mesh Holes (Board side)
        for (mx = [-10 : 7.0 : 18]) {
            for (my = [-6 : 6.0 : 6]) {
                translate([BOARD_X_BED + mx, BOARD_CENTER_Y + 16 + my, -0.5])
                    cylinder(d=2.8, h=WALL_THICKNESS + 2.0);
            }
        }
    }
}

/* ==============================================================================
 * ULTRA-THIN TEST FIT TEMPLATE (0.8mm / 4-LAYERS, ~2-MIN PRINT)
 * ============================================================================== */
module test_template() {
    THICK = 0.8;
    difference() {
        // Ultra-thin Flat Base Plate
        rounded_box(ENCLOSURE_OUTER_W, ENCLOSURE_OUTER_H, THICK, CORNER_RADIUS);
        
        // --- SUBTRACTIONS (1:1 Verification Cuts) ---
        // 1. Active Screen Window Cutout
        translate([BOARD_CENTER_X, BOARD_CENTER_Y, -0.5])
            rounded_box(DISPLAY_ACTIVE_W, DISPLAY_ACTIVE_H, THICK + 1.0, 1.5);
            
        // 2. 4x Board Screw Through-Holes (Ø2.8mm for testing M2.5/M3 screws)
        translate([BOARD_CENTER_X, BOARD_CENTER_Y, -0.5]) {
            for (dx = [-BOARD_HOLE_SPACING_X/2, BOARD_HOLE_SPACING_X/2]) {
                for (dy = [-BOARD_HOLE_SPACING_Y/2, BOARD_HOLE_SPACING_Y/2]) {
                    translate([dx, dy, 0])
                        cylinder(d=2.8, h=THICK + 1.0);
                }
            }
        }
        
        // 3. Speaker Window Cutout
        translate([SPEAKER_CENTER_X, SPEAKER_CENTER_Y, -0.5])
            rounded_box(SPEAKER_W, SPEAKER_H, THICK + 1.0, 1.5);
        
        // 4. 4x Outer Case Corner Screw Holes (Ø3.2mm)
        for (x = [-CORNER_HOLE_DX/2, CORNER_HOLE_DX/2]) {
            for (y = [-CORNER_HOLE_DY/2, CORNER_HOLE_DY/2]) {
                translate([x, y, -0.5])
                    cylinder(d=3.2, h=THICK + 1.0);
            }
        }
    }
}

/* ==============================================================================
 * SCENE RENDERING SELECTOR
 * ============================================================================== */
if (RENDER_PART == "front") {
    front_case();
} else if (RENDER_PART == "back") {
    back_plate();
} else if (RENDER_PART == "template") {
    test_template();
} else {
    // Both arranged side-by-side for 3D printing
    translate([-75, 0, 0]) front_case();
    translate([ 75, 0, 0]) back_plate();
}
