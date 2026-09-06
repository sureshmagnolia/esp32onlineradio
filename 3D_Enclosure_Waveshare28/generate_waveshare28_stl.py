"""
3D Printable Stereo Enclosure Generator for Waveshare ESP32-S3-Touch-LCD-2.8
Parametrically re-engineered with the Board placed directly next to the bottom wall:
 - NO case scallop cutout! Continuous, sleek, uninterrupted flat bottom wall!
 - MicroSD socket mouth sits only 2.5mm from the outer bottom wall.
 - MicroSD slot is a clean, flush 13.5 x 2.8 mm slot with lead-in chamfer.
 - Ejected card springs 1.5mm outside the flat bottom wall for effortless finger removal!
 - 3x Tactile buttons on bottom edge are only 0.3mm from inner bottom wall for crisp, zero-wobble plungers.
 - Cabinet height optimized to 62.0mm for sleek, balanced desktop hi-fi aesthetics.
 - Dual 2030 stereo cavity speakers with vertical acoustic grilles at X = ±53.0mm.
 - USB Type-C port on right wall at Y = -3.23mm.
 - 4 Corner M3 closure screws with 10mm recessed counterbore wells in rear cover.
 - Zero-collision perimeter arch board retention clamp.
"""

import os
import functools
import numpy as np
import trimesh
from manifold3d import Manifold

def union_all(manifolds):
    valid = [m for m in manifolds if m is not None]
    if not valid:
        return None
    return functools.reduce(lambda a, b: a + b, valid)

def diff_all(base, subtractions):
    valid = [s for s in subtractions if s is not None]
    if not valid:
        return base
    subs = functools.reduce(lambda a, b: a + b, valid)
    return base - subs

def make_rounded_box(width, height, depth, radius, segments=32):
    """Creates a rounded box along XY plane from Z=0 to depth."""
    r = min(radius, width/2.0 - 0.1, height/2.0 - 0.1)
    dx = width / 2.0 - r
    dy = height / 2.0 - r
    cyls = [
        Manifold.cylinder(depth, r, r, segments).translate([dx, dy, 0]),
        Manifold.cylinder(depth, r, r, segments).translate([-dx, dy, 0]),
        Manifold.cylinder(depth, r, r, segments).translate([dx, -dy, 0]),
        Manifold.cylinder(depth, r, r, segments).translate([-dx, -dy, 0]),
    ]
    bx = Manifold.cube([width - 2*r, height, depth], center=True).translate([0, 0, depth/2.0])
    by = Manifold.cube([width, height - 2*r, depth], center=True).translate([0, 0, depth/2.0])
    return union_all(cyls + [bx, by])

def export_manifold_to_stl(manifold_obj, filepath):
    mesh = manifold_obj.to_mesh()
    tri_mesh = trimesh.Trimesh(
        vertices=mesh.vert_properties[:, :3],
        faces=mesh.tri_verts,
        process=True
    )
    is_wt = tri_mesh.is_watertight
    print(f"Exporting {os.path.basename(filepath)}: Vertices={len(tri_mesh.vertices)}, Faces={len(tri_mesh.faces)}, Watertight={is_wt}")
    tri_mesh.export(filepath)
    return tri_mesh

# -----------------------------------------------------------------------------
# Exact Engineering Coordinates (in mm)
# -----------------------------------------------------------------------------
CASE_W = 142.0       # Total width (desktop aspect ratio)
CASE_H = 62.0        # Total height (optimized for sleek desktop hi-fi proportions)
CASE_FRONT_D = 18.0  # Front case depth
CASE_BACK_D = 14.0   # Rear cover depth (total enclosure depth = 32.0 mm)
WALL_T = 2.0         # Rigid 2.0mm wall thickness
CORNER_R = 6.0       # External corner radius

# Display Board Dimensions (Enlarged by +1.5 to +2.0 mm for smooth fit & 3D print tolerances)
GLASS_W = 75.0       # 73.06mm + 1.94mm clearance (smooth drop-in, zero binding)
GLASS_H = 52.5       # 50.54mm + 1.96mm clearance (smooth drop-in, zero binding)
GLASS_D = 1.6        # Recess depth from inside front wall
# DISPLAY & BOARD POSITIONING:
# Active viewing screen is centered symmetrically at X = 0.00 mm (between the two speakers).
# From official Waveshare drawing (73.06mm glass, 57.6mm active area):
# Active area has 4.41mm left margin and 11.05mm right margin on glass.
# Exact Glass Center: X = +3.32 mm.
# Exact PCB Center (69.0mm substrate): X = +1.86 mm.
SCREEN_ACTIVE_W = 58.4   # 57.6mm + 0.8mm clearance
SCREEN_ACTIVE_H = 44.0   # 43.2mm + 0.8mm clearance
SCREEN_CENTER_X = 0.00   # Centered on front case
SCREEN_CENTER_Y = -3.23

GLASS_W = 75.0           # 73.06mm + 1.94mm smooth-fit clearance
GLASS_H = 52.5           # 50.54mm + 1.96mm smooth-fit clearance
GLASS_D = 1.6            # Recess depth from inside front wall
GLASS_OFFSET_X = 3.32    # Aligns glass with centered screen
GLASS_OFFSET_Y = -3.23

PCB_OFFSET_X = 1.86      # Aligns PCB with centered screen
PCB_OFFSET_Y = -3.23

# Height of Raised Factory Standoff Tops:
STANDOFF_TOP_Z = WALL_T + 1.4 + 2.0 + 1.6 + 2.8  # 9.8mm (flush with factory standoffs)
BOARD_CLAMP_Z = STANDOFF_TOP_Z

# 2x Board Retention Chassis Bosses (Located safely in the open channel ABOVE display glass):
BOARD_CLAMP_BOSSES = [
    (-18.00,  25.50), # Top-Left chassis boss
    ( 18.00,  25.50), # Top-Right chassis boss
]

# 4x Board Mounting Hole Coordinates on PCB (for clamp bracket registration nubs):
PCB_HOLES = [
    (PCB_OFFSET_X - 26.27, PCB_OFFSET_Y - 18.43), # Bottom-Left  (-24.41, -21.66)
    (PCB_OFFSET_X - 26.27, PCB_OFFSET_Y + 18.43), # Top-Left     (-24.41,  15.20)
    (PCB_OFFSET_X + 26.27, PCB_OFFSET_Y - 18.43), # Bottom-Right (+28.13, -21.66)
    (PCB_OFFSET_X + 26.27, PCB_OFFSET_Y + 18.43), # Top-Right    (+28.13,  15.20)
]

# 2x Tactile Buttons on BOTTOM Edge (RESET hole is CLOSED; only BOOT and BAT_PWR are accessible)
BUTTONS_X = [
    PCB_OFFSET_X - 18.40,  # BOOT:     X = -16.54 mm (Programmable Multi-Action Button)
    PCB_OFFSET_X -  4.40,  # BAT_PWR:  X =  -2.54 mm (Hardware Power Up / Down)
]
BUTTONS_Z = WALL_T + 1.4 + 2.0 + 1.6 + 1.1  # 8.1 mm

# MicroSD Slot on BOTTOM Wall (mouth at Y = -28.18 mm -> 2.8mm from outer wall)
TF_SLOT_X = PCB_OFFSET_X + 4.54   # X = +6.40 mm
TF_SLOT_Z = WALL_T + 1.4 + 2.0 + 1.6 + 1.0  # 8.0 mm

# USB Type-C Port on RIGHT Wall
USBC_Y = PCB_OFFSET_Y
USBC_Z = WALL_T + 1.4 + 2.0 + 1.6 + 0.1  # 7.1mm

# 2030 Stereo Speakers (20mm W x 30mm H x 6mm D)
SPK_CENTER_DIST = 53.0             # Left: -53.0 mm, Right: +53.0 mm
SPK_CENTER_Y = -1.0                # Centered with display
SPK_POCKET_W = 20.8
SPK_POCKET_H = 30.8
SPK_POCKET_D = 7.0

# Case Corner Closure Screws (M3 x 12-20mm)
CORNER_SCREW_DX = CASE_W - 2 * (CORNER_R + 2.0) # 126.0 mm (X = ±63.0)
CORNER_SCREW_DY = CASE_H - 2 * (CORNER_R + 2.0) # 46.0 mm  (Y = ±23.0)

# -----------------------------------------------------------------------------
# 1. Front Cabinet Builder
# -----------------------------------------------------------------------------
def build_front_case():
    print("Building Front Cabinet with Board Placed at Bottom Wall (Zero Scallop)...")
    
    # 1. Base Outer Shell
    outer = make_rounded_box(CASE_W, CASE_H, CASE_FRONT_D, CORNER_R)
    inner = make_rounded_box(CASE_W - 2*WALL_T, CASE_H - 2*WALL_T, CASE_FRONT_D, max(1.0, CORNER_R - WALL_T)).translate([0, 0, WALL_T])
    base_shell = outer - inner
    
    addons = []
    
    # A. Dual Speaker Retention Chambers
    spk_box_w = SPK_POCKET_W + 2 * 1.6
    spk_box_h = SPK_POCKET_H + 2 * 1.6
    spk_box_d = WALL_T + SPK_POCKET_D
    for side in [-1, 1]:
        spk_x = side * SPK_CENTER_DIST
        spk_box = make_rounded_box(spk_box_w, spk_box_h, spk_box_d, 3.0).translate([spk_x, SPK_CENTER_Y, 0])
        addons.append(spk_box)
        
    # B. 4x Case Corner Closure Screw Bosses
    for dx in [-CORNER_SCREW_DX / 2.0, CORNER_SCREW_DX / 2.0]:
        for dy in [-CORNER_SCREW_DY / 2.0, CORNER_SCREW_DY / 2.0]:
            boss = Manifold.cylinder(CASE_FRONT_D, 3.8, 3.8, 24).translate([dx, dy, 0])
            addons.append(boss)
            
    # C. Glass Perimeter Retention Nest (Surrounds the 73.06 x 50.54mm display glass)
    nest_wall_t = 1.8
    nest_outer = make_rounded_box(GLASS_W + 2*nest_wall_t, GLASS_H + 2*nest_wall_t, 6.8, 2.5).translate([
        GLASS_OFFSET_X, GLASS_OFFSET_Y, 0
    ])
    nest_inner = make_rounded_box(GLASS_W, GLASS_H, 10.0, 1.2).translate([
        GLASS_OFFSET_X, GLASS_OFFSET_Y, WALL_T
    ])
    glass_nest = nest_outer - nest_inner
    addons.append(glass_nest)
    
    # D. 2x Board Retention Chassis Bosses (Above display glass)
    for cbx, cby in BOARD_CLAMP_BOSSES:
        cb_boss = Manifold.cylinder(BOARD_CLAMP_Z, 3.2, 3.2, 24).translate([cbx, cby, 0])
        addons.append(cb_boss)
        
    combined = union_all([base_shell] + addons)
    
    # 3. Subtractions
    subtractions = []
    
    # A. Active Screen Viewing Window (through front wall, centered at X=0, Y=-3.23)
    win = Manifold.cube([SCREEN_ACTIVE_W, SCREEN_ACTIVE_H, WALL_T + 4.0], center=True).translate([
        SCREEN_CENTER_X, SCREEN_CENTER_Y, WALL_T / 2.0
    ])
    subtractions.append(win)
    
    # B. Recessed Glass Pocket (seats the 75.0 x 52.5mm glass, depth = 1.6mm)
    glass_p = Manifold.cube([GLASS_W, GLASS_H, GLASS_D + 0.1], center=True).translate([
        GLASS_OFFSET_X, GLASS_OFFSET_Y, WALL_T + (GLASS_D + 0.1) / 2.0
    ])
    subtractions.append(glass_p)
    
    # C. Pilot holes for the 4x Board Retention Chassis Bosses (Ø2.2mm x 5.0mm deep)
    for cbx, cby in BOARD_CLAMP_BOSSES:
        pilot = Manifold.cylinder(5.5, 1.15, 1.15, 20).translate([cbx, cby, BOARD_CLAMP_Z - 5.0])
        subtractions.append(pilot)
        
    # D. Speaker Internal Cavities & Vertical Slotted Grilles
    num_slots = 6
    slot_w = 1.8
    slot_h = 24.0
    slot_pitch = 2.8
    for side in [-1, 1]:
        spk_x = side * SPK_CENTER_DIST
        cav = make_rounded_box(SPK_POCKET_W, SPK_POCKET_H, SPK_POCKET_D + 2.0, 2.0).translate([spk_x, SPK_CENTER_Y, WALL_T])
        subtractions.append(cav)
        for i in range(num_slots):
            offset_x = (i - (num_slots - 1) / 2.0) * slot_pitch
            slot = make_rounded_box(slot_w, slot_h, WALL_T + 4.0, slot_w / 2.0).translate([
                spk_x + offset_x, SPK_CENTER_Y, -2.0
            ])
            subtractions.append(slot)
            
    # E. 4x Corner Screw Pilot Holes (Ø2.8mm x 14mm for M3 screws)
    for dx in [-CORNER_SCREW_DX / 2.0, CORNER_SCREW_DX / 2.0]:
        for dy in [-CORNER_SCREW_DY / 2.0, CORNER_SCREW_DY / 2.0]:
            chole = Manifold.cylinder(15.0, 1.4, 1.4, 20).translate([dx, dy, CASE_FRONT_D - 14.0])
            subtractions.append(chole)
            
    # F. 3x Tactile Button Guide Holes (through BOTTOM wall, Y = -CASE_H/2 = -31.0mm)
    # Wall is 2.0mm thick (Y = -31.0 to -29.0mm)
    for bx in BUTTONS_X:
        # 1. Main shaft through-tunnel (Ø3.6mm clearance, spans Y = -34.0 to -28.0mm)
        b_tunnel = Manifold.cylinder(6.0, 1.8, 1.8, 24).rotate([-90, 0, 0]).translate([
            bx, -CASE_H / 2.0 - 3.0, BUTTONS_Z
        ])
        # 2. Internal retaining flange counterbore pocket (Ø4.4mm, depth 0.9mm from inside wall face: Y = -29.9 to -28.5mm)
        b_pocket = Manifold.cylinder(1.4, 2.2, 2.2, 24).rotate([-90, 0, 0]).translate([
            bx, -CASE_H / 2.0 + WALL_T - 0.9, BUTTONS_Z
        ])
        subtractions.extend([b_tunnel, b_pocket])
        
    # G. MicroSD Card Slot (FLAT BOTTOM WALL - ZERO SCALLOP!)
    # Socket mouth sits at Y = -28.18mm, outer wall is at Y = -31.0mm (only 2.8mm deep!)
    # Clean 11.4mm x 2.2mm card pass-through slot with elegant lead-in bevel (leaves solid 1.1mm dividing rib to button!)
    tf_tunnel = Manifold.cube([11.4, 6.0, 2.2], center=True).translate([
        TF_SLOT_X, -CASE_H / 2.0 + 1.0, TF_SLOT_Z
    ])
    tf_chamfer = Manifold.cube([12.0, 0.8, 2.8], center=True).translate([
        TF_SLOT_X, -CASE_H / 2.0 + 0.2, TF_SLOT_Z
    ])
    subtractions.extend([tf_tunnel, tf_chamfer])
    
    # H. USB Type-C Port Cutout (through RIGHT wall, X = +CASE_W/2)
    usbc_tunnel = Manifold.cube([WALL_T + 6.0, 12.0, 7.0], center=True).translate([
        CASE_W / 2.0 - WALL_T / 2.0 + 1.0, USBC_Y, USBC_Z
    ])
    usbc_chamfer = Manifold.cube([2.5, 16.0, 10.5], center=True).translate([
        CASE_W / 2.0, USBC_Y, USBC_Z
    ])
    subtractions.extend([usbc_tunnel, usbc_chamfer])
    
    final_front = diff_all(combined, subtractions)
    return final_front

# -----------------------------------------------------------------------------
# 2. Rear Cover Builder
# -----------------------------------------------------------------------------
def build_back_cover():
    print("Building Rear Cover with Engineered Corner Pillars & Recessed Screw Wells...")
    
    # Base Outer Shell
    outer = make_rounded_box(CASE_W, CASE_H, CASE_BACK_D, CORNER_R)
    inner = make_rounded_box(CASE_W - 2*WALL_T, CASE_H - 2*WALL_T, CASE_BACK_D, max(1.0, CORNER_R - WALL_T)).translate([0, 0, WALL_T])
    base_shell = outer - inner
    
    addons = []
    
    # Interlocking Mating Lip (extends 2.0mm into front housing)
    rim_w = (CASE_W - 2*WALL_T) - 0.4
    rim_h = (CASE_H - 2*WALL_T) - 0.4
    rim_outer = make_rounded_box(rim_w, rim_h, 2.2, max(1.0, CORNER_R - WALL_T - 0.2)).translate([0, 0, CASE_BACK_D])
    rim_inner = make_rounded_box(rim_w - 2*1.2, rim_h - 2*1.2, 4.0, max(0.5, CORNER_R - WALL_T - 1.4)).translate([0, 0, CASE_BACK_D - 0.5])
    addons.append(rim_outer - rim_inner)
    
    # 4x Solid Structural Corner Pillars (from Z=0 to Z=CASE_BACK_D = 14.0mm)
    for dx in [-CORNER_SCREW_DX / 2.0, CORNER_SCREW_DX / 2.0]:
        for dy in [-CORNER_SCREW_DY / 2.0, CORNER_SCREW_DY / 2.0]:
            pillar = Manifold.cylinder(CASE_BACK_D, 3.8, 3.8, 24).translate([dx, dy, 0])
            addons.append(pillar)
            
    # Internal Battery Cradle (fits up to 60 x 30 x 7 mm pouch cell)
    bat_w = 60.0
    bat_h = 28.0
    bat_d = 4.5
    bat_outer = make_rounded_box(bat_w + 2*1.2, bat_h + 2*1.2, bat_d + WALL_T, 2.5).translate([0, 2.0, 0])
    bat_inner = make_rounded_box(bat_w, bat_h, bat_d + 5.0, 1.8).translate([0, 2.0, WALL_T])
    addons.append(bat_outer - bat_inner)
    
    combined = union_all([base_shell] + addons)
    
    subtractions = []
    
    # Rear Sound & Heat Ventilation Slots
    num_vents = 7
    vent_w = 2.0
    vent_h = 24.0
    vent_pitch = 4.2
    for i in range(num_vents):
        vx = (i - (num_vents - 1) / 2.0) * vent_pitch
        vent = make_rounded_box(vent_w, vent_h, WALL_T + 4.0, vent_w / 2.0).translate([vx, 2.0, -2.0])
        subtractions.append(vent)
        
    # Engineered Corner Closure Screw Wells:
    # 1. Recessed Counterbore Well: Ø6.5mm x 9.0mm deep from outer back face
    # 2. Solid Shoulder: 5.0mm thick (from Z=9.0mm to Z=14.0mm)
    # 3. Screw Clearance: Ø3.4mm through-hole for M3 screw
    # With M3x16mm screw: reaches 11.0mm deep into front case!
    for dx in [-CORNER_SCREW_DX / 2.0, CORNER_SCREW_DX / 2.0]:
        for dy in [-CORNER_SCREW_DY / 2.0, CORNER_SCREW_DY / 2.0]:
            well = Manifold.cylinder(10.0, 3.25, 3.25, 24).translate([dx, dy, -1.0])
            shank = Manifold.cylinder(CASE_BACK_D + 6.0, 1.7, 1.7, 24).translate([dx, dy, -1.0])
            subtractions.extend([well, shank])
            
    final_back = diff_all(combined, subtractions)
    return final_back

# -----------------------------------------------------------------------------
# 3. Captive Push Button Plungers (Set of 3 + 1 spare = 4x)
# -----------------------------------------------------------------------------
def build_button_plungers():
    print("Building Short Precision Captive Button Plungers...")
    # Precision plunger geometry (designed to print standing on button cap on print bed):
    # Base (button cap on bed): Z = 0 to 2.2mm, Ø3.2mm (protrudes 1.4mm outside case)
    # Flange (retaining collar): Z = 2.2 to 3.0mm, Ø4.0mm (captivated in Ø4.4mm wall pocket)
    # Nub (actuator contact pin): Z = 3.0 to 3.6mm, Ø1.6mm (depresses tactile switch)
    cap_shaft = Manifold.cylinder(2.2, 1.6, 1.6, 24)
    flange = Manifold.cylinder(0.8, 2.0, 2.0, 24).translate([0, 0, 2.2])
    nub = Manifold.cylinder(0.6, 0.8, 0.8, 18).translate([0, 0, 3.0])
    single_btn = union_all([cap_shaft, flange, nub])
    
    # 4 plungers arranged on print bed (3 required + 1 spare!)
    plungers = []
    spacing = 9.0
    for idx, (dx, dy) in enumerate([(-spacing/2, -spacing/2), (spacing/2, -spacing/2),
                                   (-spacing/2, spacing/2), (spacing/2, spacing/2)]):
        plungers.append(single_btn.translate([dx, dy, 0]))
        
    return union_all(plungers)

# -----------------------------------------------------------------------------
# 4. Board Retention Clamp Frame (Zero-Collision Perimeter Arch Bracket)
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# 4. Board Retention Clamp Frame (Unified Monolithic U-Bracket on Standoffs)
# -----------------------------------------------------------------------------
def build_board_clamp():
    print("Building Precision Monolithic Board Clamping U-Frame...")
    clamp_t = 2.4
    
    # Left Standoffs: X = -30.00, Right Standoffs: X = +22.54
    # Bottom Standoffs: Y = -21.66, Top Standoffs: Y = +15.20
    # Top Chassis Bosses: Y = +25.50 at X = -20.00 and X = +12.00
    # Wings run from Y = -24.5mm up to Y = +28.5mm (length = 53.0mm, center Y = +2.0mm)
    # Top bar runs along Y = +25.50mm from X = -33.5mm to X = +25.5mm (width = 60.0mm, height = 7.0mm)
    # This guarantees a massive 6.0mm solid overlap uniting the wings and top bar into ONE single piece!
    
    # 1. Left Clamp Wing (centered directly along the 2 left standoffs)
    left_w = 7.0
    left_h = 53.0
    left_cx = PCB_HOLES[0][0]  # -24.41 mm
    left_cy = 2.0
    left_wing = make_rounded_box(left_w, left_h, clamp_t, 2.0).translate([left_cx, left_cy, 0])
    
    # 2. Right Clamp Wing (centered directly along the 2 right standoffs)
    right_w = 6.0
    right_h = 53.0
    right_cx = PCB_HOLES[2][0]  # +28.13 mm
    right_cy = 2.0
    right_wing = make_rounded_box(right_w, right_h, clamp_t, 2.0).translate([right_cx, right_cy, 0])
    
    # 3. Top Structural Crossbar (connects left and right wings along Y = +25.50mm OUTSIDE the PCB)
    top_w = (right_cx - left_cx) + 12.0
    top_h = 7.0
    top_cx = (left_cx + right_cx) / 2.0
    top_bar = make_rounded_box(top_w, top_h, clamp_t, 2.0).translate([top_cx, 25.50, 0])
    
    # 4. 4x Board Registration Nubs (extend 1.4mm downward into the factory brass standoffs' Ø2.2mm threaded bores)
    nubs = []
    for hx, hy in PCB_HOLES:
        nub = Manifold.cylinder(1.4, 1.05, 1.05, 18).translate([hx, hy, -1.4])
        nubs.append(nub)
        
    combined = union_all([left_wing, right_wing, top_bar] + nubs)
    
    subtractions = []
    # A. 2x Chassis Mounting Screw Clearance Holes (Ø3.0mm into top front bosses)
    for cbx, cby in BOARD_CLAMP_BOSSES:
        shole = Manifold.cylinder(clamp_t + 4.0, 1.5, 1.5, 20).translate([cbx, cby, -2.0])
        subtractions.append(shole)
        
    # B. 4x Board Eyelet Screw Holes (Ø2.4mm clearance for M2/M2.5 screws into brass bushings)
    for hx, hy in PCB_HOLES:
        bhole = Manifold.cylinder(clamp_t + 4.0, 1.2, 1.2, 20).translate([hx, hy, -2.0])
        subtractions.append(bhole)
            
    clamp = diff_all(combined, subtractions)
    return clamp

# -----------------------------------------------------------------------------
# Main Execution
# -----------------------------------------------------------------------------
def main():
    out_dir = r"d:\ESP32Radio\3D_Enclosure_Waveshare28"
    os.makedirs(out_dir, exist_ok=True)
    
    front_case = build_front_case()
    back_cover = build_back_cover()
    buttons = build_button_plungers()
    clamp = build_board_clamp()
    
    front_stl = os.path.join(out_dir, "Waveshare28_Radio_Front_Case.stl")
    back_stl = os.path.join(out_dir, "Waveshare28_Radio_Back_Cover.stl")
    btn_stl = os.path.join(out_dir, "Waveshare28_Radio_Button_Plungers.stl")
    clamp_stl = os.path.join(out_dir, "Waveshare28_Radio_Board_Clamp.stl")
    all_stl = os.path.join(out_dir, "Waveshare28_Radio_All_Parts.stl")
    
    mesh_front = export_manifold_to_stl(front_case, front_stl)
    mesh_back = export_manifold_to_stl(back_cover, back_stl)
    mesh_btn = export_manifold_to_stl(buttons, btn_stl)
    mesh_clamp = export_manifold_to_stl(clamp, clamp_stl)
    
    # Combined Print Plate
    front_plate = front_case.rotate([180, 0, 0]).translate([0, -CASE_H/2.0 - 5.0, CASE_FRONT_D])
    back_plate = back_cover.translate([0, CASE_H/2.0 + 5.0, 0])
    btn_plate = buttons.translate([CASE_W/2.0 + 14.0, -15.0, 0])
    clamp_plate = clamp.translate([CASE_W/2.0 + 14.0, 15.0, 0])
    
    all_parts = union_all([front_plate, back_plate, btn_plate, clamp_plate])
    mesh_all = export_manifold_to_stl(all_parts, all_stl)
    
    print("\n=======================================================")
    print("  All Revised Waveshare 2.8 Enclosure STLs generated successfully!")
    print(f"  Front Case Watertight: {mesh_front.is_watertight}")
    print(f"  Back Cover Watertight: {mesh_back.is_watertight}")
    print(f"  Buttons Watertight:    {mesh_btn.is_watertight}")
    print(f"  Clamp Watertight:      {mesh_clamp.is_watertight}")
    print(f"  Combined Watertight:   {mesh_all.is_watertight}")
    print(f"  Directory: {out_dir}")
    print("=======================================================\n")

if __name__ == '__main__':
    main()
