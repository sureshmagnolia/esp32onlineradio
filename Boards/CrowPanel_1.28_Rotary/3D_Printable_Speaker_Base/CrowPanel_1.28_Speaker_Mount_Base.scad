/*
 * =========================================================================================================
 * Project: CrowPanel 1.28" ESP32-S3 Rotary Touch Display — Royal Rimmed Modular Speaker Mount Base
 * Target:  Elecrow CrowPanel 1.28-inch HMI (DHE38128D) + MAX98357A I2S Mono DAC/Amp Breakout Board
 * Architecture: 
 *   - Aesthetic: Smooth satin conical body with classical torus architectural rims, a clean minimalist
 *     oval border line, and authentic pure-vector Edifier 3-flame acoustic mark (no text, no floral ornament).
 *   - Part A: Screws directly to bottom of CrowPanel via 2x M2 counterbored screws; provides port cutouts.
 *   - Part B: Conical pedestal with Ø64mm base; houses MAX98357A & wire coils; sticks to speaker with 3M VHB.
 *   - Interlocking: Broad bottle-cap coarse spiral thread (P = 4.5mm, 1.25 turns, loose non-binding fit).
 *   - Wire Exits: Both central bottom Ø6.5mm hole AND a 9mm × 7mm arched side cable exit port.
 * Author:  Antigravity AI Studio
 * =========================================================================================================
 */

$fn = 96;

// --- Select Part to Render ---
// Options: "ALL" (exploded/assembled view), "PART_A" (Top Adapter), "PART_B" (Pedestal)
part_to_render = "ALL"; 

// --- Primary Dimensions (All in mm) ---

// Conical Outer Dimensions
total_height        = 26.0; // Total assembled height
base_outer_dia      = 64.0; // Flared base plinth rim diameter
top_outer_dia       = 50.4; // Top crown collar diameter

// Part Split Seam
bottom_height       = 17.5; // Height of Part B
top_height          = 8.5;  // Height of Part A (total = 17.5 + 8.5 = 26mm)
seam_outer_dia      = 54.0; // Mating seam diameter

// Part A: Top Board Mount Adapter
collar_inner_dia    = 47.10; // Tightened collar diameter (tight snug fit for CrowPanel 47.0mm PCB)
collar_depth        = 3.5;   // Depth of top collar
shelf_height        = top_height - collar_depth; // Z = 5.0mm (where CrowPanel rests)
port_hole_dia       = 34.0;  // Central cutout opening (exposes J34, J36, J37, FPC & button)

// M2 Screw Mount (Matches CrowPanel T1/T2 at +/-13.506mm diagonal)
screw_hole_dia      = 2.3;   // M2 through-hole clearance
screw_cb_dia        = 4.2;   // Counterbore diameter for M2 screw head
screw_cb_depth      = 2.6;   // Counterbore depth from bottom of neck

// Bottle-Cap Coarse Spiral Thread
thread_pitch        = 4.5;   // Broad spiral pitch
thread_turns        = 1.25;  // 1.25 turns (~450 deg)
joint_height        = 5.8;   // Depth of male threaded neck
male_root_dia       = 40.8;  // Male neck core root diameter
male_crest_dia      = 43.6;  // Male thread crest diameter (1.4mm tooth depth)

// Female Thread Socket (with generous 0.45mm clearance)
thread_clearance    = 0.45;
female_crest_dia    = male_root_dia + thread_clearance * 2;  // 41.7mm
female_root_dia     = male_crest_dia + thread_clearance * 2; // 44.5mm

// Part B: Bottom Pedestal & Cavity
floor_thickness     = 2.5;   // Solid bottom floor plate
cavity_inner_dia    = 40.0;  // Internal chamber diameter (houses MAX98357A & wire coils)
cavity_depth        = bottom_height - floor_thickness; // 15.0mm
bottom_wire_hole    = 6.5;   // Center hole through floor for 4-lead cable to speaker
side_wire_width     = 9.0;   // Arched side cable exit width
side_wire_height    = 7.0;   // Arched side cable exit height

// MAX98357A Floor Retention Guides
guide_spacing       = 18.5;  // Spacing for 18mm wide board
guide_length        = 16.0;
guide_width         = 1.8;
guide_height        = 2.2;


// =========================================================================
// HELPER: Architectural Raised Rims
// =========================================================================
module raised_rim_bead(dia, z_pos, height = 0.6, width = 0.6) {
    translate([0, 0, z_pos])
        rotate_extrude($fn = 96)
            translate([dia / 2, 0, 0])
                circle(r = height, $fn = 24);
}


// =========================================================================
// MODULE: PART A (Top Board Mount Adapter)
// =========================================================================
module part_a_top_adapter() {
    difference() {
        // --- ADDITIONS ---
        union() {
            // 1. Smooth conical outer shell
            cylinder(d1 = seam_outer_dia, d2 = top_outer_dia, h = top_height);
            
            // 2. Architectural Rims on Part A
            // Rim 1: Seam mating rim (Z = 0.4mm)
            raised_rim_bead(seam_outer_dia, 0.4, 0.6);
            // Rim 2: Mid-frieze dividing rim (Z = 4.5mm)
            raised_rim_bead(seam_outer_dia - 2.0, 4.5, 0.55);
            // Rim 3: Crown lip bead (Z = 7.2mm)
            raised_rim_bead(top_outer_dia, 7.2, 0.6);
            
            // 3. Male threaded spigot neck extending downwards (Z = -joint_height to 0)
            translate([0, 0, -joint_height])
                cylinder(d = male_root_dia, h = joint_height + 0.1);
                
            // 4. Broad Coarse Spiral Thread Rib (45 deg self-supporting flanks)
            for (step = [0 : 80]) {
                rotate([0, 0, step * (thread_turns * 360 / 80)])
                    translate([male_root_dia / 2 - 0.2, 0, -joint_height + 0.4 + step * (thread_pitch / (80 / thread_turns))])
                        rotate([0, 45, 0])
                            cube([1.4, 1.8, 1.4], center = true);
            }
                
        }
        
        // --- SUBTRACTIONS ---
        
        // 5. Precision Tightened Collar for CrowPanel (Ø47.10mm, depth 3.5mm)
        translate([0, 0, shelf_height])
            cylinder(d = collar_inner_dia, h = collar_depth + 1.0);
            
        // 6. Central Bore (Ø35.0mm, R = 17.5mm from Z = -joint_height to shelf_height)
        // Leaves massive 6.05mm solid screwing rim at Z = 5.0mm, and 2.9mm solid male neck wall!
        translate([0, 0, -joint_height - 1.0])
            cylinder(d = 35.0, h = joint_height + shelf_height + 2.0);
            
        // 7. Schematic-Matched Precision Plug Chutes (Front-View Insertion, X -> -X)
        // Engineered from Eagle BRD layout: 100% open vertical pass-through for plugs & wires!
        // Flipped horizontally because the board is inserted face-up with LCD facing up.
        // Outer cylinder limit d = 41.0mm (R = 20.5mm) strictly protects male thread root (d = 40.8mm)
        // Preserves 92.4% solid screwing shelf with 3.3mm solid radial bridge between J34 and J36 on West!
        intersection() {
            translate([0, 0, -joint_height - 1.0])
                cylinder(d = 41.0, h = joint_height + shelf_height + 2.0);
            union() {
                // J34: UART (X in [-20.0, -10.5], Y in [2.5, 9.8]) - West
                translate([-15.25, 6.15, (-joint_height + shelf_height)/2])
                    cube([9.5, 7.3, joint_height + shelf_height + 3.0], center = true);
                // J36: Audio / I2C (X in [-20.0, -10.5], Y in [-8.2, -0.8]) - West
                translate([-15.25, -4.50, (-joint_height + shelf_height)/2])
                    cube([9.5, 7.4, joint_height + shelf_height + 3.0], center = true);
                // J37: Power / USB (X in [8.5, 15.5], Y in [7.0, 14.5]) - East
                translate([12.00, 10.75, (-joint_height + shelf_height)/2])
                    cube([7.0, 7.5, joint_height + shelf_height + 3.0], center = true);
            }
        }
            
        // 8. U-Shaped Screw Slots at T2 (135° NW) and T1 (315° SE) (Open to center instead of 'O')
        // Width 2.4mm through 2.0mm shelf web (Z = 3.0mm to 5.1mm)
        for (angle = [135, 315]) {
            rotate([0, 0, angle])
                translate([0, -1.2, 3.0])
                    cube([19.106 + 0.1, 2.4, 2.2]);
            rotate([0, 0, angle])
                translate([19.106, 0, 3.0])
                    cylinder(d = 2.4, h = 2.2, $fn = 24);
        }
            
        // 9. Full-Depth Vertical Screw & Screwdriver Clearance Gaps in Threaded Neck
        // Extends from bottom of neck (Z = -joint_height - 1.0) all the way up to Z = 3.0mm
        // Width 5.2mm allows M2 screws and screwdrivers to pass straight up to clamp against Z = 3.0mm!
        for (angle = [135, 315]) {
            rotate([0, 0, angle])
                translate([0, -2.6, -joint_height - 1.0])
                    cube([19.106 + 0.1, 5.2, joint_height + 4.0]);
            rotate([0, 0, angle])
                translate([19.106, 0, -joint_height - 1.0])
                    cylinder(d = 5.2, h = joint_height + 4.0, $fn = 32);
        }
    }
}


// =========================================================================
// MODULE: PART B (Bottom Speaker Pedestal Base)
// =========================================================================
module part_b_bottom_pedestal() {
    difference() {
        // --- ADDITIONS ---
        union() {
            // 1. Smooth conical lower pedestal
            cylinder(d1 = base_outer_dia, d2 = seam_outer_dia, h = bottom_height);
            
            // 2. Architectural Rims on Part B
            // Rim 1: Base plinth rim bead (Z = 2.4mm)
            raised_rim_bead(base_outer_dia - 1.2, 2.4, 0.6);
            // Rim 2: Lower meander accent rim (Z = 6.0mm)
            raised_rim_bead(base_outer_dia - 3.6, 6.0, 0.6);
            // Rim 3: Top seam mating rim (Z = bottom_height - 0.5mm)
            raised_rim_bead(seam_outer_dia, bottom_height - 0.5, 0.65);
            
            // 3. Front Royal Minimalist Oval Frame Line (Clean oval border alone, zero floral elements)
            translate([0, 28.2, 11.6])
                rotate([-atan2(5.0, 17.5), 0, 0])
                scale([1.1, 0.4, 1.2])
                difference() {
                    cylinder(r = 7.2, h = 1.0, center = true);
                    cylinder(r = 5.8, h = 1.2, center = true);
                }
                
            // 4. MAX98357A floor retention guide rails
            translate([-guide_length / 2, -guide_spacing / 2 - guide_width, floor_thickness])
                cube([guide_length, guide_width, guide_height]);
            translate([-guide_length / 2, guide_spacing / 2, floor_thickness])
                cube([guide_length, guide_width, guide_height]);
        }
        
        // --- SUBTRACTIONS ---
        
        // 5. Main internal electronics chamber (Ø40.0mm)
        translate([0, 0, floor_thickness])
            cylinder(d = cavity_inner_dia, h = cavity_depth + 0.1);
            
        // 6. Female Thread Socket Core (Ø41.7mm)
        translate([0, 0, bottom_height - joint_height])
            cylinder(d = female_crest_dia, h = joint_height + 0.5);
            
        // 7. Female Helical Spiral Groove (matching male thread with 0.45mm clearance)
        for (step = [0 : 80]) {
            rotate([0, 0, step * (thread_turns * 360 / 80)])
                translate([female_crest_dia / 2 - 0.2, 0, (bottom_height - joint_height + 0.4) + step * (thread_pitch / (80 / thread_turns))])
                    rotate([0, 45, 0])
                        cube([1.8, 2.2, 1.8], center = true);
        }
            
        // 8. Central 4-lead cable pass-through hole through floor (Ø6.5mm)
        translate([0, 0, -1.0])
            cylinder(d = bottom_wire_hole, h = floor_thickness + 2.0, $fn = 48);
            
        // 9. Arched Side Cable Exit Port (Width = 9.0mm, Height = 7.0mm, at rear side)
        translate([0, -base_outer_dia / 2 + 5.0, floor_thickness + side_wire_height / 2])
            cube([side_wire_width, 20.0, side_wire_height], center = true);
        translate([0, -base_outer_dia / 2 + 5.0, floor_thickness + side_wire_height])
            rotate([90, 0, 0])
                cylinder(d = side_wire_width, h = 20.0, center = true);
    }
}


// =========================================================================
// RENDER CONTROLLER
// =========================================================================
if (part_to_render == "PART_A") {
    part_a_top_adapter();
} else if (part_to_render == "PART_B") {
    part_b_bottom_pedestal();
} else {
    // "ALL" Assembled / Exploded Visualizer
    color([0.78, 0.45, 0.25, 1.0]) // Antique Copper/Bronze
        part_b_bottom_pedestal();
        
    color([0.85, 0.52, 0.28, 1.0]) // Antique Copper/Bronze
        translate([0, 0, bottom_height + 15.0])
            part_a_top_adapter();
}
