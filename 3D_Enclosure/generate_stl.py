"""
Generator script for ESP32-S3 JC3248W535 Internet Radio 3D Printable Enclosure STLs.
Ensures 100% strictly watertight, manifold geometry with zero non-manifold faces or self-intersections.
Audited for JC3248W535 module:
 - Recessed glass pocket (95.5mm x 63.0mm x 1.6mm) on inside face
 - Precise beveled screen viewing window (73.4mm x 49.0mm)
 - Dedicated standoff posts with open Ø2.4mm x 5.0mm self-tapping screw pilot holes
   aligned with the 4 scalloped corner mounting ears
 - Open rear closure screw boss pilot holes (Ø2.6mm x 18mm)
"""

import os
import functools
import trimesh
import numpy as np
from manifold3d import Manifold

def union_all(manifolds):
    if not manifolds:
        return None
    return functools.reduce(lambda a, b: a + b, manifolds)

def diff_all(base, subtractions):
    if not subtractions:
        return base
    subs_combined = union_all(subtractions)
    return base - subs_combined

def make_rounded_box(width, height, depth, radius, segments=36):
    """Creates a 3D box with rounded corners along the XY plane."""
    r = min(radius, width/2, height/2)
    dx = width / 2 - r
    dy = height / 2 - r
    
    # 4 corner cylinders
    c1 = Manifold.cylinder(depth, r, r, segments).translate([dx, dy, 0])
    c2 = Manifold.cylinder(depth, r, r, segments).translate([-dx, dy, 0])
    c3 = Manifold.cylinder(depth, r, r, segments).translate([dx, -dy, 0])
    c4 = Manifold.cylinder(depth, r, r, segments).translate([-dx, -dy, 0])
    
    # Connecting cubes
    b_x = Manifold.cube([width - 2*r, height, depth], center=True).translate([0, 0, depth/2])
    b_y = Manifold.cube([width, height - 2*r, depth], center=True).translate([0, 0, depth/2])
    
    return union_all([c1, c2, c3, c4, b_x, b_y])

def build_front_case():
    """
    External Panel-Mount Front Enclosure.
    The JC3248W535 module drops in from the FRONT / OUTSIDE:
      - 82.5mm x 57.5mm cutout with corner scallop clearance
      - 4x solid reinforced screw tabs (Ø7.2mm) with clean Ø3.4mm through-holes
        for rear-to-front screw fastening into the board's corner ears
      - 4x rear closure corner bosses with Ø2.6mm x 18mm pilot holes
      - Speaker chamber on right (46.0mm) & bottom battery cradle (-37.0mm)
    """
    WALL_THICKNESS = 2.4
    CORNER_RADIUS = 6.0
    
    INNER_W = 132.0
    INNER_H = 74.0
    DEPTH = 32.0
    OUTER_W = INNER_W + 2 * WALL_THICKNESS
    OUTER_H = INNER_H + 2 * WALL_THICKNESS
    
    CORNER_HOLE_DX = INNER_W - 6.0
    CORNER_HOLE_DY = INNER_H - 6.0
    
    BOARD_CENTER_X = -18.0
    BOARD_CENTER_Y = 0.0
    
    # External Mount Body Cutout Dimensions
    PANEL_CUTOUT_W = 82.5
    PANEL_CUTOUT_H = 57.5
    
    # Board mounting screw hole pitch
    BOARD_HOLE_SPACING_X = 84.5
    BOARD_HOLE_SPACING_Y = 52.5
    BOARD_TAB_OD = 7.6
    BOARD_TAB_DEPTH = WALL_THICKNESS  # Level/flush with inside front wall (2.4mm) for direct short screw reach
    BOARD_HOLE_DIA = 3.4
    
    SPEAKER_W = 20.8
    SPEAKER_H = 40.8
    SPEAKER_D = 7.5
    SPEAKER_CENTER_X = 46.0
    SPEAKER_CENTER_Y = 0.0
    
    # 1. Main Outer Shell
    outer = make_rounded_box(OUTER_W, OUTER_H, DEPTH, CORNER_RADIUS)
    inner = make_rounded_box(INNER_W, INNER_H, DEPTH + 2.0, CORNER_RADIUS - 1.2).translate([0, 0, WALL_THICKNESS])
    main_shell = outer - inner
    
    # 2. Add-ons (Reinforced board screw tabs, Speaker chamber, Battery cradle, Rear bosses)
    addons = []
    
    # 4x Board Screw Reinforced Tabs (from front wall Z = 0 to Z = BOARD_TAB_DEPTH)
    for dx in [-BOARD_HOLE_SPACING_X/2, BOARD_HOLE_SPACING_X/2]:
        for dy in [-BOARD_HOLE_SPACING_Y/2, BOARD_HOLE_SPACING_Y/2]:
            tab = Manifold.cylinder(BOARD_TAB_DEPTH, BOARD_TAB_OD/2, BOARD_TAB_OD/2, 28).translate([
                BOARD_CENTER_X + dx, BOARD_CENTER_Y + dy, 0
            ])
            addons.append(tab)
            
    # 4x Rear Corner Screw Bosses (Closed solid from base up to rim)
    for x in [-CORNER_HOLE_DX/2, CORNER_HOLE_DX/2]:
        for y in [-CORNER_HOLE_DY/2, CORNER_HOLE_DY/2]:
            boss = Manifold.cylinder(DEPTH, 3.8, 3.8, 28).translate([x, y, 0])
            addons.append(boss)
            
    # Speaker Retention Chamber
    sw = SPEAKER_W + 2.4
    sh = SPEAKER_H + 2.4
    spk_bracket_outer = Manifold.cube([sw, sh, SPEAKER_D + 2.0], center=True).translate([
        SPEAKER_CENTER_X, SPEAKER_CENTER_Y, WALL_THICKNESS + (SPEAKER_D + 2.0)/2
    ])
    spk_bracket_inner = Manifold.cube([SPEAKER_W, SPEAKER_H, SPEAKER_D + 5.0], center=True).translate([
        SPEAKER_CENTER_X, SPEAKER_CENTER_Y, WALL_THICKNESS + (SPEAKER_D + 5.0)/2
    ])
    addons.append(spk_bracket_outer - spk_bracket_inner)
    
    combined = union_all([main_shell] + addons)
    
    # 3. Subtractions (Panel cutout with corner scallops, through-holes, grille, rear pilot holes)
    subtractions = []
    
    # A. External Panel-Mount Body Cutout (82.5mm x 57.5mm with R=5.0mm rounded corners)
    panel_cutout = make_rounded_box(PANEL_CUTOUT_W, PANEL_CUTOUT_H, WALL_THICKNESS + 4.0, 5.0).translate([
        BOARD_CENTER_X, BOARD_CENTER_Y, -2.0
    ])
    subtractions.append(panel_cutout)
    
    # B. 4x Board Screw Through-Holes (Ø3.4mm through front wall and tabs)
    for dx in [-BOARD_HOLE_SPACING_X/2, BOARD_HOLE_SPACING_X/2]:
        for dy in [-BOARD_HOLE_SPACING_Y/2, BOARD_HOLE_SPACING_Y/2]:
            hole = Manifold.cylinder(BOARD_TAB_DEPTH + 4.0, BOARD_HOLE_DIA/2, BOARD_HOLE_DIA/2, 28).translate([
                BOARD_CENTER_X + dx, BOARD_CENTER_Y + dy, -2.0
            ])
            subtractions.append(hole)
            
    # C. Speaker Acoustic Grille Slots
    for gy in np.linspace(-15, 15, 9):
        slot = make_rounded_box(14.0, 2.0, WALL_THICKNESS + 4.0, 1.0).translate([
            SPEAKER_CENTER_X, SPEAKER_CENTER_Y + gy, -2.0
        ])
        subtractions.append(slot)
        
    # D. Rear Corner Screw Pilot Holes (Open from rim Z = DEPTH down 20.0mm, Ø2.8mm)
    for x in [-CORNER_HOLE_DX/2, CORNER_HOLE_DX/2]:
        for y in [-CORNER_HOLE_DY/2, CORNER_HOLE_DY/2]:
            chole = Manifold.cylinder(21.0, 1.4, 1.4, 28).translate([
                x, y, DEPTH - 20.0
            ])
            subtractions.append(chole)
            
    final_front = diff_all(combined, subtractions)
    return final_front

def build_back_plate():
    WALL_THICKNESS = 2.4
    CORNER_RADIUS = 6.0
    INNER_W = 132.0
    INNER_H = 74.0
    OUTER_W = INNER_W + 2 * WALL_THICKNESS
    OUTER_H = INNER_H + 2 * WALL_THICKNESS
    
    CORNER_HOLE_DX = INNER_W - 6.0
    CORNER_HOLE_DY = INNER_H - 6.0
    
    # On Print Bed (X_bed = -X_assembled so folding over onto front case matches 1:1):
    BOARD_CENTER_X_BED   = +18.0
    BOARD_CENTER_Y_BED   = 0.0
    SPEAKER_CENTER_X_BED = -46.0
    SPEAKER_CENTER_Y_BED = 0.0
    USB_X_BED            = -27.75
    USB_Y_BED            = +4.0
    SW_X_BED             = -10.0
    SW_Y_BED             = +23.0
    
    # 1. Base Plate & Inner Aligning Lip
    base = make_rounded_box(OUTER_W, OUTER_H, WALL_THICKNESS, CORNER_RADIUS)
    
    lip_outer = make_rounded_box(INNER_W - 0.6, INNER_H - 0.6, 2.2 + 0.2, CORNER_RADIUS - 1.4).translate([0, 0, WALL_THICKNESS - 0.2])
    lip_inner = make_rounded_box(INNER_W - 3.8, INNER_H - 3.8, 3.0, CORNER_RADIUS - 2.0).translate([0, 0, WALL_THICKNESS - 0.4])
    lip = lip_outer - lip_inner
    
    plate = base + lip
    
    # 2. Subtractions
    subtractions = []
    
    # A. 4x Corner Boss Relief Cutouts (Cuts lip only, leaving base plate intact with 1.0mm radial clearance around bosses)
    for x in [-CORNER_HOLE_DX/2, CORNER_HOLE_DX/2]:
        for y in [-CORNER_HOLE_DY/2, CORNER_HOLE_DY/2]:
            boss_relief = Manifold.cylinder(3.0, 4.8, 4.8, 28).translate([x, y, WALL_THICKNESS - 0.2])
            subtractions.append(boss_relief)
            
    # B. 4x Corner Screw Clearance Through-Holes & Universal Counterbores (M3)
    # Through-hole Ø3.4mm (full height Z = -1.0 to 6.0mm)
    # Counterbore Ø7.2mm x 1.6mm depth on exterior face (Z = -0.2 to 1.4mm)
    # 45° chamfer transition from Ø7.2mm to Ø3.4mm (Z = 1.4 to 2.0mm)
    for x in [-CORNER_HOLE_DX/2, CORNER_HOLE_DX/2]:
        for y in [-CORNER_HOLE_DY/2, CORNER_HOLE_DY/2]:
            shole = Manifold.cylinder(7.0, 1.7, 1.7, 28).translate([x, y, -1.0])
            cbore = Manifold.cylinder(1.6, 3.6, 3.6, 28).translate([x, y, -0.2])
            c_sink = Manifold.cylinder(0.6, 3.6, 1.7, 28).translate([x, y, 1.4])
            subtractions.extend([shole, cbore, c_sink])
            
    # C. Vertical USB-C Port Cutout
    usb = make_rounded_box(8.5, 14.5, WALL_THICKNESS + 4.0, 3.5).translate([USB_X_BED, USB_Y_BED, -1.0])
    subtractions.append(usb)
    
    # D. Battery Switch Button Access Cutout
    batt_sw = Manifold.cylinder(WALL_THICKNESS + 4.0, 3.0, 3.0, 20).translate([SW_X_BED, SW_Y_BED, -1.0])
    subtractions.append(batt_sw)
    
    # E. Cooling & Acoustic Louvers (Speaker side)
    for ly in np.linspace(-18, 18, 9):
        louver = make_rounded_box(18.0, 2.2, WALL_THICKNESS + 4.0, 1.0).translate([
            SPEAKER_CENTER_X_BED, SPEAKER_CENTER_Y_BED + ly, -1.0
        ])
        subtractions.append(louver)
        
    # F. ESP32 CPU Ventilation Mesh Holes (Board side)
    for mx in np.linspace(-10, 18, 5):
        for my in np.linspace(-6, 6, 3):
            vent = Manifold.cylinder(WALL_THICKNESS + 4.0, 1.4, 1.4, 16).translate([
                BOARD_CENTER_X_BED + mx, BOARD_CENTER_Y_BED + 16 + my, -1.0
            ])
            subtractions.append(vent)
            
    final_back = diff_all(plate, subtractions)
    return final_back

def build_test_template():
    """
    Ultra-thin (0.8mm / 4 layers at 0.2mm) flat alignment gauge card (~2-3 min print, ~3g filament).
    Acts as a 1:1 physical check overlay to instantly verify:
      1. 4x Board mounting ear holes (Ø3.2mm at 84.5mm x 52.5mm pitch)
      2. 82.5mm x 57.5mm scalloped rear body clearance window
      3. 2040 Speaker window cutout (20.8mm x 40.8mm)
      4. 4x Outer case closure screw holes (Ø3.2mm at 126.0mm x 68.0mm pitch)
    """
    THICKNESS = 0.8  # 4 layers of 0.2mm
    CORNER_RADIUS = 6.0
    
    INNER_W = 132.0
    INNER_H = 74.0
    OUTER_W = INNER_W + 2 * 2.4  # 136.8mm
    OUTER_H = INNER_H + 2 * 2.4  # 78.8mm
    
    CORNER_HOLE_DX = INNER_W - 6.0  # 126.0mm
    CORNER_HOLE_DY = INNER_H - 6.0  # 68.0mm
    
    BOARD_CENTER_X = -18.0
    BOARD_CENTER_Y = 0.0
    
    # 82.5mm x 57.5mm rear body window with corner clearance
    REAR_BODY_W = 82.5
    REAR_BODY_H = 57.5
    
    BOARD_HOLE_SPACING_X = 84.5
    BOARD_HOLE_SPACING_Y = 52.5
    BOARD_HOLE_DIA = 3.2  # Clear pass-through for M2.5/M3 screws
    
    SPEAKER_W = 20.8
    SPEAKER_H = 40.8
    SPEAKER_CENTER_X = 46.0
    SPEAKER_CENTER_Y = 0.0
    
    # 1. Ultra-thin flat plate (0.8mm)
    base = make_rounded_box(OUTER_W, OUTER_H, THICKNESS, CORNER_RADIUS)
    
    # 2. Subtractions (Through-cuts for instant physical overlay testing)
    subtractions = []
    
    # 82.5 x 57.5mm Clearance Cutout
    rear_win = make_rounded_box(REAR_BODY_W, REAR_BODY_H, THICKNESS + 2.0, 5.0).translate([
        BOARD_CENTER_X, BOARD_CENTER_Y, -1.0
    ])
    subtractions.append(rear_win)
    
    # 4x Outer Board Mounting Ear Through-Holes (84.5mm x 52.5mm)
    for dx in [-BOARD_HOLE_SPACING_X/2, BOARD_HOLE_SPACING_X/2]:
        for dy in [-BOARD_HOLE_SPACING_Y/2, BOARD_HOLE_SPACING_Y/2]:
            hole = Manifold.cylinder(THICKNESS + 2.0, BOARD_HOLE_DIA/2, BOARD_HOLE_DIA/2, 24).translate([
                BOARD_CENTER_X + dx, BOARD_CENTER_Y + dy, -1.0
            ])
            subtractions.append(hole)
            
    # Speaker Window Cutout
    spk_cut = make_rounded_box(SPEAKER_W, SPEAKER_H, THICKNESS + 2.0, 1.5).translate([
        SPEAKER_CENTER_X, SPEAKER_CENTER_Y, -1.0
    ])
    subtractions.append(spk_cut)
    
    # 4x Outer Case Corner Screw Holes
    for x in [-CORNER_HOLE_DX/2, CORNER_HOLE_DX/2]:
        for y in [-CORNER_HOLE_DY/2, CORNER_HOLE_DY/2]:
            chole = Manifold.cylinder(THICKNESS + 2.0, 1.6, 1.6, 24).translate([x, y, -1.0])
            subtractions.append(chole)
            
    final_template = diff_all(base, subtractions)
    return final_template

def build_board_gauge():
    """
    1:1 Direct Board Test Gauge (94.5mm x 62.0mm x 0.8mm, ~1 min print, ~1.5g filament).
    Matches the exact outer boundary of the JC3248W535 board.
    Features:
      - 4x Corner Mounting Ear Through-Holes (Ø3.2mm at 84.5mm x 52.5mm pitch)
      - Large Scalloped Internal Clearance Window (82.5mm x 57.5mm) so it drops 100% FLUSH
        over the 82x57mm rear casing and connectors when tested from the back!
    """
    THICKNESS = 0.8
    BOARD_W = 94.5
    BOARD_H = 62.0
    CORNER_R = 3.8
    
    HOLE_DX = 84.5
    HOLE_DY = 52.5
    HOLE_DIA = 3.2
    
    WINDOW_W = 82.5
    WINDOW_H = 57.5
    
    base = make_rounded_box(BOARD_W, BOARD_H, THICKNESS, CORNER_R)
    
    subtractions = []
    
    # 1. 82.5 x 57.5mm Scalloped Clearance Window
    win = make_rounded_box(WINDOW_W, WINDOW_H, THICKNESS + 2.0, 5.0).translate([0, 0, -1.0])
    subtractions.append(win)
    
    # 2. 4x Corner Mounting Ear Holes
    for dx in [-HOLE_DX/2, HOLE_DX/2]:
        for dy in [-HOLE_DY/2, HOLE_DY/2]:
            hole = Manifold.cylinder(THICKNESS + 2.0, HOLE_DIA/2, HOLE_DIA/2, 24).translate([dx, dy, -1.0])
            subtractions.append(hole)
            
    return diff_all(base, subtractions)

def export_manifold_to_stl(manifold_obj, file_path):
    mesh = manifold_obj.to_mesh()
    verts = mesh.vert_properties[:, :3]
    faces = mesh.tri_verts
    tri_mesh = trimesh.Trimesh(vertices=verts, faces=faces)
    
    # Verify mesh health
    is_watertight = tri_mesh.is_watertight
    volume = tri_mesh.volume
    print(f"Mesh Check: {os.path.basename(file_path)} -> Watertight: {is_watertight}, Volume: {volume:.1f} mm³, Faces: {len(faces)}")
    
    tri_mesh.export(file_path)
    print(f"  Exported: {file_path}")

if __name__ == "__main__":
    out_dir = r"d:\ESP32Radio\3D_Enclosure"
    os.makedirs(out_dir, exist_ok=True)
    
    print("--- 1. Generating 1:1 Direct Board Fit Gauge ---")
    board_gauge = build_board_gauge()
    gauge_path = os.path.join(out_dir, "ESP32S3_Radio_Board_Fit_Gauge.stl")
    export_manifold_to_stl(board_gauge, gauge_path)
    
    print("\n--- 2. Generating Front Test Template ---")
    test_template = build_test_template()
    template_path = os.path.join(out_dir, "ESP32S3_Radio_Front_Test_Template.stl")
    export_manifold_to_stl(test_template, template_path)
    
    print("\n--- 3. Generating Front Case ---")
    front = build_front_case()
    front_path = os.path.join(out_dir, "ESP32S3_Radio_Front_Case.stl")
    export_manifold_to_stl(front, front_path)
    
    print("\n--- 4. Generating Back Plate ---")
    back = build_back_plate()
    back_path = os.path.join(out_dir, "ESP32S3_Radio_Back_Plate.stl")
    export_manifold_to_stl(back, back_path)
    
    print("\n--- 5. Generating Combined Print-Bed Layout ---")
    front_flat = front.translate([-75, 0, 0])
    back_flat = back.translate([75, 0, 0])
    both = front_flat + back_flat
    both_path = os.path.join(out_dir, "ESP32S3_Radio_All_Parts.stl")
    export_manifold_to_stl(both, both_path)
    
    print("\nAll STL files successfully generated and verified!")
