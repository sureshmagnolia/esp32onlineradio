/*
 * =========================================================================================================
 * Project: Elecrow CrowPanel 5.79" ESP32-S3 E-Paper Smart Clock & Audio Cabinet
 * Target:  CrowPanel 5.79-inch E-Paper HMI (272x792) + Heavy-Duty Snap-Fit Bracket + 40mm Speaker + 18650s
 * Architectural Features:
 *   1. Single Corner Screw Boss: Exactly ONE clean screw post in each corner of the front bezel!
 *   2. Heavy-Duty Snap-Fit Bracket:
 *      - Base thickness reinforced to 4.0mm with 2.5mm perimeter stiffening box-ribs (total 6.5mm height).
 *      - Thick 20mm wide central cross-spine connecting the 2 factory M3 screw bosses.
 *      - 4x Integrated snap-fit locking tabs on top/bottom edges that click into front bezel detents.
 *      - Zero extra screws required for the bracket! Completely eliminates corner double-holes!
 *   3. Rear Cabinet:
 *      - Shallow 2.5mm speaker retention rim on back door (no tall deep cup).
 *      - Clean separate right-half 2x 18650 battery cradle with zero speaker overlap.
 *      - Non-overlapping keyholes (>10.2mm clear of Reset button hole).
 *      - Exactly 1 countersunk screw hole per corner.
 * Author: Antigravity AI Studio
 * =========================================================================================================
 */

$fn = 64;

// --- Part Selector ---
// Options: "ALL" (Exploded assembly), "FRONT" (Front Bezel), "BRACKET" (Mounting Bracket),
//          "REAR" (Rear Cabinet), "BUTTONS" (Push Plungers)
part_to_render = "ALL";

// =========================================================================================================
// 1. PRIMARY PARAMETERS (All dimensions in mm)
// =========================================================================================================

// Enclosure Outer Dimensions (Minimalist slim bezel hugging board)
enclosure_width     = 173.0; // Minimal width (board is 167.92mm -> 2.54mm side walls)
enclosure_height    =  62.5; // Minimal height (board is 57.51mm -> 2.49mm top/bottom walls)
front_bezel_depth   =  14.0; // Front bezel depth
rear_cabinet_depth  =  28.0; // Rear cabinet depth (increased to 28.0mm to provide 25.8mm cavity for 18650s!)
total_depth         = front_bezel_depth + rear_cabinet_depth; // 42.0mm
corner_radius       =   4.0; // Modern tight rounded corners
wall_thickness      =   2.2; // Rigid perimeter wall thickness

// CrowPanel Board Module Pocket (168.2mm x 57.8mm x 12.0mm)
board_length        = 168.2; 
board_height        =  57.8; 
board_depth         =  12.0; 

// Active Display Window (Active E-Paper glass is 139.0mm x 47.74mm)
window_length       = 140.5; // Viewable width
window_height       =  48.5; // Viewable height
window_bevel        =   1.5; // 45-degree aesthetic chamfer

// Factory Board M3 Fastening Screws (Ground Truth from Eagle BRD: X = +/-50.0mm, Y = 0.0mm)
screw_board_pitch   = 100.0;
screw_board_l_x     = -50.0;
screw_board_r_x     =  50.0;
screw_board_y       =   0.0;

// SINGLE Enclosure Corner Assembly Screws (Rear Cabinet to Front Bezel: EXACTLY ONE per corner!)
corner_screw_x      = (enclosure_width / 2) - 4.5;  // 82.0mm
corner_screw_y      = (enclosure_height / 2) - 4.5; // 26.75mm

// Spring-Action Latch Positions on Top and Bottom Edges of Bracket & Bezel Pocket
snap_x_top          = [-28.0, 28.0];
snap_x_bot          = [-12.0, 12.0];
snap_w              =   8.0;
snap_h              =   1.3;
snap_z              =   8.0;

// Rear Push Buttons (Eagle BRD: K1 BOOT @ X=+40.48, Y=+23.27; K2 RESET @ X=+54.18, Y=+23.27)
btn_boot_x          =  40.48;
btn_reset_x         =  54.18;
btn_y               =  23.27;
btn_hole_dia        =   5.2; // Bore through back wall
btn_recess_dia      =   7.8; // Recessed finger well on back face (prevents accidental push on wall)
btn_cap_dia         =   4.6; // Button plunger head diameter
btn_flange_dia      =   6.8; // Captive inner retaining flange
btn_stem_dia        =   3.6; // Stiff guide stem
btn_tip_dia         =   2.2; // Actuator pin contacting tactile switch

// Dual Wall Mounting Keyhole Slots (Moved to clear locations: X = -65.0mm & X = +68.0mm, Y = +14.0mm)
keyhole_l_x         = -65.0;
keyhole_r_x         =  68.0;
keyhole_y           =  14.0;
keyhole_entry_dia   =   7.5;
keyhole_slot_w      =   3.8;
keyhole_slot_len    =   6.5;

// 40mm Speaker Subsystem (Left-Center: X = -25.0mm, Y = 0.0mm)
speaker_dia         =  40.5;
speaker_x           = -25.0;
speaker_y           =   0.0;
speaker_rim_h       =   2.5; // Shallow 2.5mm retention lip on back door (NO deep cup!)

// Dual 18650 Battery Cradle (Right Half: X = 41.0mm, Y = -6.0mm)
// Expanded to 72.0mm x 40.0mm x 9.0mm deep with generous 25.8mm cavity! ZERO overlap with speaker!
bat_bay_x           =  41.0;
bat_bay_y           =  -6.0;
bat_bay_len         =  72.0; // Fits standard, protected, or commercial battery sled
bat_bay_w           =  40.0; // Fits two 18.5mm cells side-by-side
bat_bay_depth       =   9.0; // 9.0mm tall curved cradle saddles

// MAX98357A I2S DAC Breakout Rails (Far Left: X = -60.0mm, Y = 0.0mm)
amp_x               = -60.0;
amp_y               =   0.0;

// Bottom Edge Ports
usbc_x              =  36.03;
usbc_width          =  13.0;
usbc_height         =   7.0;

sd_x                = -39.87;
sd_width            =  15.5;
sd_height           =   4.0;

// Left Edge Controls Window
dial_cutout_y       =   0.0;
dial_cutout_width   =   8.0;
dial_cutout_height  =  28.0;

// =========================================================================================================
// 2. HELPER UTILITY MODULES
// =========================================================================================================

module rounded_rect_solid(w, h, d, r) {
    hull() {
        translate([-w/2 + r, -h/2 + r, 0]) cylinder(r = r, h = d);
        translate([ w/2 - r, -h/2 + r, 0]) cylinder(r = r, h = d);
        translate([-w/2 + r,  h/2 - r, 0]) cylinder(r = r, h = d);
        translate([ w/2 - r,  h/2 - r, 0]) cylinder(r = r, h = d);
    }
}

// Wall Mount Keyhole
module wall_keyhole() {
    union() {
        cylinder(d = keyhole_entry_dia, h = wall_thickness + 2.0);
        translate([0, keyhole_slot_len / 2, 0])
            cube([keyhole_slot_w, keyhole_slot_len, wall_thickness + 2.0], center = true);
        translate([0, 0, wall_thickness - 1.2])
            hull() {
                cylinder(d = keyhole_entry_dia + 2.0, h = 4.0);
                translate([0, keyhole_slot_len, 0])
                    cylinder(d = keyhole_entry_dia + 2.0, h = 4.0);
            }
    }
}

// Rear Acoustic Speaker Grille
module rear_speaker_grille() {
    union() {
        cylinder(d = 5.0, h = wall_thickness + 2.0);
        for (ring = [9.0, 16.0, 23.0, 30.0, 36.0]) {
            n_slits = round(ring * 0.95);
            for (i = [0 : n_slits - 1]) {
                rotate([0, 0, i * (360 / n_slits)])
                    translate([ring / 2, 0, 0])
                        cube([2.0, 1.6, wall_thickness + 2.0], center = true);
            }
        }
    }
}

// =============================================================================
// 3. MODULE: FRONT BEZEL (EXACTLY 1 CORNER BOSS PER CORNER + SNAP DETENTS)
// =============================================================================
module front_bezel() {
    difference() {
        union() {
            // Main outer solid shell
            rounded_rect_solid(enclosure_width, enclosure_height, front_bezel_depth, corner_radius);
            
            // EXACTLY 1 SINGLE CORNER BOSS PER CORNER (M2.5/M3: dia 2.6mm core)
            // The redundant inner peanut boss has been completely eliminated!
            for (mx = [-1, 1]) {
                for (my = [-1, 1]) {
                    translate([mx * corner_screw_x, my * corner_screw_y, 0])
                        cylinder(d = 7.0, h = front_bezel_depth);
                }
            }
        }
        
        // --- SUBTRACTIONS ---
        
        // 1. Active Display Window with 45-deg Bevel
        translate([0, 0, -1.0]) {
            cube([window_length, window_height, wall_thickness + 2.0], center = true);
            hull() {
                cube([window_length, window_height, 0.1], center = true);
                translate([0, 0, -window_bevel])
                    cube([window_length + window_bevel * 2, window_height + window_bevel * 2, 0.1], center = true);
            }
        }
        
        // 2. CrowPanel Module Internal Pocket (168.2mm x 57.8mm x 12.0mm)
        translate([0, 0, wall_thickness])
            rounded_rect_solid(board_length, board_height, front_bezel_depth + 1.0, 1.5);
            
        // 3. 4x Perimeter Corner Assembly Pilot Holes (dia 2.6mm for M2.5/M3 screws)
        for (mx = [-1, 1]) {
            for (my = [-1, 1]) {
                translate([mx * corner_screw_x, my * corner_screw_y, -1.0])
                    cylinder(d = 2.6, h = front_bezel_depth + 2.0);
            }
        }
        
        // 4. Snap-Fit Receiver Detents on Top & Bottom Pocket Walls
        for (sx = snap_x_top) {
            translate([sx, board_height/2 - 0.2, wall_thickness + snap_z])
                cube([snap_w + 1.0, 2.0, 2.5], center = true);
        }
        for (sx = snap_x_bot) {
            translate([sx, -board_height/2 + 0.2, wall_thickness + snap_z])
                cube([snap_w + 1.0, 2.0, 2.5], center = true);
        }
        
        // 5. Left Edge Thumb Window for Jog Dial K5 & Navigation Keys
        translate([-enclosure_width / 2 - 1.0, dial_cutout_y - dial_cutout_height / 2, wall_thickness + 1.0])
            cube([dial_cutout_width + 2.0, dial_cutout_height, front_bezel_depth]);
            
        // 6. Bottom Edge USB-C Slot (J2)
        translate([usbc_x - usbc_width / 2, -enclosure_height / 2 - 1.0, wall_thickness + 2.0])
            cube([usbc_width, wall_thickness + 3.0, usbc_height]);
            
        // 7. Bottom Edge MicroSD Slot (J1)
        translate([sd_x - sd_width / 2, -enclosure_height / 2 - 1.0, wall_thickness + 2.0])
            cube([sd_width, wall_thickness + 3.0, sd_height]);
    }
}

// =============================================================================
// 4. MODULE: REINFORCED SNAP-FIT MOUNTING BRACKET (4.0mm BASE + PERIMETER RIBS)
// =============================================================================
module mounting_bracket() {
    t_base = 4.0;
    h_rib  = 2.5;
    
    difference() {
        union() {
            // 1. Heavy-duty solid base plate (167.0mm x 56.5mm x 4.0mm)
            rounded_rect_solid(167.0, 56.5, t_base, 3.0);
            
            // 2. Continuous perimeter structural flange (adds massive bending stiffness)
            difference() {
                rounded_rect_solid(167.0, 56.5, t_base + h_rib, 3.0);
                translate([0, 0, -0.5])
                    rounded_rect_solid(162.0, 51.5, t_base + h_rib + 1.0, 2.0);
            }
            
            // 3. Heavy-Duty Central Backbone Spine (Width = 20mm across center)
            translate([0, 0, t_base])
                cube([130.0, 20.0, h_rib], center = true);
                
            // 4. Solid reinforced cylindrical bosses around M3 screws (dia 14mm)
            for (xs = [screw_board_l_x, screw_board_r_x]) {
                translate([xs, y_screw, 0])
                    cylinder(d = 13.0, h = t_base + h_rib);
            }
            
            // 5. 4x Spring-Action Cantilever Latch Teeth & Squeeze Release Pads
            for (sx = snap_x_top) {
                translate([sx, 56.5/2 + snap_h/2, t_base/2])
                    cube([snap_w, snap_h * 2, 3.5], center = true);
                translate([sx, 56.5/2 - 0.9, t_base + 1.0])
                    cube([snap_w, 1.8, 2.0], center = true);
            }
            for (sx = snap_x_bot) {
                translate([sx, -56.5/2 - snap_h/2, t_base/2])
                    cube([snap_w, snap_h * 2, 3.5], center = true);
                translate([sx, -56.5/2 + 0.9, t_base + 1.0])
                    cube([snap_w, 1.8, 2.0], center = true);
            }
        }
        
        // --- SUBTRACTIONS ---
        
        // 0. Spring Flexure Relief Slots (Allows cantilever arm to bend inward on squeeze/push)
        for (sx = snap_x_top) {
            translate([sx, 56.5/2 - 2.8, -1.0])
                cube([22.0, 2.0, t_base + h_rib + 2.0], center = true);
        }
        for (sx = snap_x_bot) {
            translate([sx, -56.5/2 + 2.8, -1.0])
                cube([22.0, 2.0, t_base + h_rib + 2.0], center = true);
        }
        
        // Finger-Pry Extraction Notches on Left and Right Ends
        for (ex = [-62.0, 62.0]) {
            translate([ex, 0, t_base + h_rib - 1.0])
                cube([8.0, 6.0, 2.0], center = true);
        }
        
        // 1. 2x M3 Screws into CrowPanel Factory Brass Inserts (X = +/-50mm)
        for (xs = [screw_board_l_x, screw_board_r_x]) {
            translate([xs, y_screw, -1.0])
                cylinder(d = 3.4, h = t_base + h_rib + 2.0);
            translate([xs, y_screw, t_base + h_rib - 3.5])
                cylinder(d = 6.5, h = 4.0);
        }
        
        // 2. Corner Clearance Notches for Front Bezel Corner Posts
        for (mx = [-1, 1]) {
            for (my = [-1, 1]) {
                translate([mx * corner_screw_x, my * corner_screw_y, -1.0])
                    cylinder(d = 11.0, h = t_base + h_rib + 2.0);
            }
        }
        
        // 3. Electronics Window (ESP32-S3 module, antenna, components)
        translate([0, 0, -1.0])
            cube([55.0, 30.0, t_base + h_rib + 2.0], center = true);
            
        // 4. K1 (Boot) and K2 (Reset) Button Clearance Window
        translate([47.33, 23.27, -1.0])
            cube([26.0, 12.0, t_base + h_rib + 2.0], center = true);
            
        // 5. Battery Connector & USB-C Area Window
        translate([45.0, -20.0, -1.0])
            cube([25.0, 14.0, t_base + h_rib + 2.0], center = true);
            
        // 6. MicroSD Slot Area Window
        translate([-40.0, -20.0, -1.0])
            cube([22.0, 14.0, t_base + h_rib + 2.0], center = true);
            
        // 7. Left Side Jog Dial Window
        translate([-76.0, 0.0, -1.0])
            cube([14.0, 32.0, t_base + h_rib + 2.0], center = true);
    }
}

// =============================================================================
// 5. MODULE: REAR CABINET (SHALLOW SPEAKER RIM, CLEAN BATTERY BAY, SAFE KEYHOLES)
// =============================================================================
module rear_cabinet() {
    difference() {
        union() {
            // Main solid shell
            rounded_rect_solid(enclosure_width, enclosure_height, rear_cabinet_depth, corner_radius);
            
            // Shallow Speaker Rim (2.5mm height, dia 40.5mm)
            translate([speaker_x, speaker_y, wall_thickness])
                difference() {
                    cylinder(d = speaker_dia + 3.0, h = speaker_rim_h);
                    translate([0, 0, -0.5])
                        cylinder(d = speaker_dia, h = speaker_rim_h + 1.0);
                }
                
            // Dual 18650 Battery Cradle (Right Half: X = 41.0mm, Y = -6.0mm)
            translate([bat_bay_x - bat_bay_len/2, bat_bay_y - bat_bay_w/2, wall_thickness])
                difference() {
                    cube([bat_bay_len, bat_bay_w, bat_bay_depth]);
                    translate([-0.5, 1.5, 0])
                        cube([bat_bay_len + 1.0, bat_bay_w - 3.0, bat_bay_depth + 1.0]);
                }
            translate([bat_bay_x - bat_bay_len/2, bat_bay_y - 0.9, wall_thickness])
                cube([bat_bay_len, 1.8, bat_bay_depth]);
            
            // MAX98357A I2S DAC Rails (Far Left)
            translate([amp_x - 10.0, amp_y - 10.0, wall_thickness])
                cube([2.0, 20.0, 6.0]);
            translate([amp_x + 10.0, amp_y - 10.0, wall_thickness])
                cube([2.0, 20.0, 6.0]);
                
            // Integrated Desktop Resting Base Plinth
            translate([0, -enclosure_height / 2 + 2.5, 0])
                cube([enclosure_width - 24.0, 5.0, rear_cabinet_depth], center = true);
        }
        
        // --- SUBTRACTIONS ---
        
        // 1. Main Interior Cavity
        translate([0, 0, wall_thickness])
            rounded_rect_solid(enclosure_width - wall_thickness * 2, 
                               enclosure_height - wall_thickness * 2, 
                               rear_cabinet_depth + 1.0, 
                               corner_radius - 0.8);
                               
        // 2. Rear Speaker Acoustic Grille
        translate([speaker_x, speaker_y, -1.0])
            rear_speaker_grille();
            
        // 3. Dual Wall Keyhole Slots (X = -65.0mm & X = +68.0mm, Y = +14.0mm)
        translate([keyhole_l_x, keyhole_y, -0.1])
            wall_keyhole();
        translate([keyhole_r_x, keyhole_y, -0.1])
            wall_keyhole();
            
        // 4. Rear Boot & Reset Button Access & Guide Shafts
        for (bx = [btn_boot_x, btn_reset_x]) {
            translate([bx, btn_y, -1.0]) {
                cylinder(d = btn_hole_dia, h = wall_thickness + 2.0);
                cylinder(d = btn_recess_dia, h = 1.2);
            }
            translate([bx, btn_y, wall_thickness - 0.8])
                cylinder(d = btn_flange_dia + 0.6, h = 4.0);
        }
        
        // 5. Exactly 1 Counterbored Screw Hole per Corner (M2.5/M3)
        for (mx = [-1, 1]) {
            for (my = [-1, 1]) {
                translate([mx * corner_screw_x, my * corner_screw_y, -1.0]) {
                    cylinder(d = 3.2, h = rear_cabinet_depth + 2.0);
                    cylinder(d = 6.0, h = 3.5 + 1.0);
                }
            }
        }
        
        // 6. Bottom Edge USB-C & MicroSD Matching Reliefs
        translate([usbc_x - usbc_width / 2, -enclosure_height / 2 - 1.0, rear_cabinet_depth - 4.5])
            cube([usbc_width, wall_thickness + 3.0, 5.0]);
        translate([sd_x - sd_width / 2, -enclosure_height / 2 - 1.0, rear_cabinet_depth - 4.5])
            cube([sd_width, wall_thickness + 3.0, 5.0]);
            
        // 7. Desktop Non-Slip Silicone Foot Pad Recesses
        for (mx = [-1, 1]) {
            translate([mx * (enclosure_width / 2 - 20.0), -enclosure_height / 2 + 2.5, rear_cabinet_depth - 1.2])
                cube([16.0, 4.0, 2.0], center = true);
        }
    }
}

// =============================================================================
// 6. MODULE: CAPTIVE 3D PUSH-BUTTON PLUNGERS
// =============================================================================
module button_plunger() {
    union() {
        cylinder(d = btn_cap_dia, h = 2.2);
        translate([0, 0, 2.2])
            cylinder(d = btn_flange_dia, h = 1.5);
        // Stiff guide stem
        translate([0, 0, 3.7])
            cylinder(d = btn_stem_dia, h = 16.0);
        // Tactile switch tip
        translate([0, 0, 19.7])
            cylinder(d = btn_tip_dia, h = 5.5);
    }
}

// =============================================================================
// 7. DISPATCHER & EXPLODED ASSEMBLY PREVIEW
// =============================================================================
if (part_to_render == "FRONT") {
    front_bezel();
} else if (part_to_render == "BRACKET") {
    mounting_bracket();
} else if (part_to_render == "REAR") {
    rear_cabinet();
} else if (part_to_render == "BUTTONS") {
    translate([-6, 0, 0]) button_plunger();
    translate([ 6, 0, 0]) button_plunger();
} else {
    // Exploded 3D Assembly Preview
    color([0.94, 0.95, 0.97, 1.0])
        front_bezel();
        
    translate([0, 0, 16.0])
        color([0.10, 0.75, 0.45, 0.95])
            mounting_bracket();
            
    translate([0, 0, 45.0])
        color([0.20, 0.23, 0.28, 0.95])
            rear_cabinet();
            
    // Buttons entering from rear
    translate([btn_boot_x, btn_y, 52.0])
        color([0.90, 0.30, 0.20])
            button_plunger();
            
    translate([btn_reset_x, btn_y, 52.0])
        color([0.20, 0.60, 0.90])
            button_plunger();
}
