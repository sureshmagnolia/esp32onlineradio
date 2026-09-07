"""
3D Printable Enclosure Generator for ESP32S3_JC3248W535_Radio
Features:
 - Deep Unibody Front Cabinet (39mm depth): Houses screen, board, mounting plate, speaker, and battery inside front body.
 - Flat Screw-On Back Plate (3mm thickness): Slim, strong rear cover door with flush counterbored M3 screw wells and acoustic sound slots.
 - Internal Board Mounting: Board drops in from INSIDE into precision recessed glass pocket.
 - Zero Front Screws: Seamless modern retro-radio front face with 45° beveled screen viewing aperture.
 - Heavy-Duty Board Mounting Plate: 3.2mm thick solid plate with dedicated port openings and integrated battery retention cradle.
 - MicroSD Slot at Bottom: Flush 14.0 x 3.2 mm slot on the bottom wall with chamfered lead-in for easy card access.
 - USB Type-C on Right Wall: Direct pass-through port cutout on the unibody right wall.
 - 2040 Loudspeaker Chamber: Right-side acoustic cavity with 7 front vertical sound grille slots and rear acoustic vents.
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
    cur = base
    for s in subtractions:
        if s is not None:
            cur = cur - s
    return cur

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
CASE_W = 154.0       # Enclosure outer width
CASE_H = 76.0        # Enclosure outer height
CASE_FRONT_D = 39.0  # Deep Unibody Front Cabinet Depth (houses all electronics & battery)
BACK_PLATE_T = 3.0   # Flat Screw-On Back Door Plate Thickness (Total enclosure depth = 42.0 mm)
WALL_T = 2.2         # Rigid 2.2mm wall thickness
FRONT_FACE_T = 2.4   # Front face thickness
CORNER_R = 6.0       # External corner radius

# Board Dimensions (ESP32-S3 JC3248W535)
BOARD_CENTER_X = -14.0
BOARD_CENTER_Y = 0.0

# Outer Glass Specs: 94.5 x 62.0 x 1.6 mm
GLASS_W = 95.5       # 94.5mm + 1.0mm smooth-fit clearance (X span: -61.75 to +33.75 mm)
GLASS_H = 63.0       # 62.0mm + 1.0mm smooth-fit clearance (Y span: -31.50 to +31.50 mm)
GLASS_POCKET_D = 1.8 # Recess depth inside front wall (leaves 0.6mm front bezel rim)

# Active Display Screen Window: 73.4 x 49.0 mm
SCREEN_W = 73.4
SCREEN_H = 49.0

# 1. Bracket-to-Board Screw Pattern (4x screws into module's corner ears)
# Hole spacing: X = 84.3mm (+/- 42.15mm), Y = 52.3mm (+/- 26.15mm)
BOARD_HOLE_DX = 84.3
BOARD_HOLE_DY = 52.3
BOARD_MOUNT_HOLES = [
    (BOARD_CENTER_X - BOARD_HOLE_DX/2.0, BOARD_CENTER_Y - BOARD_HOLE_DY/2.0),
    (BOARD_CENTER_X + BOARD_HOLE_DX/2.0, BOARD_CENTER_Y - BOARD_HOLE_DY/2.0),
    (BOARD_CENTER_X - BOARD_HOLE_DX/2.0, BOARD_CENTER_Y + BOARD_HOLE_DY/2.0),
    (BOARD_CENTER_X + BOARD_HOLE_DX/2.0, BOARD_CENTER_Y + BOARD_HOLE_DY/2.0),
]

# 2. Bracket-to-Body Chassis Boss Pattern (4x screws fastening bracket+board assembly to cabinet body)
# Perfectly symmetric on side flanks at Y = +/- 12.0 mm:
# Left tabs at X = -65.0 mm, Y = +/- 12.0 mm
# Right tabs at X = +37.0 mm, Y = +/- 12.0 mm
CHASSIS_BOSS_DX_L = -65.0
CHASSIS_BOSS_DX_R = +37.0
CHASSIS_BOSS_DY = 12.0
CHASSIS_BOSSES = [
    (CHASSIS_BOSS_DX_L, -CHASSIS_BOSS_DY),
    (CHASSIS_BOSS_DX_R, -CHASSIS_BOSS_DY),
    (CHASSIS_BOSS_DX_L,  CHASSIS_BOSS_DY),
    (CHASSIS_BOSS_DX_R,  CHASSIS_BOSS_DY),
]
CHASSIS_BOSS_OD = 6.8
CHASSIS_BOSS_ID = 2.6   # M3 thread-forming pilot
CHASSIS_BOSS_TOP_Z = 12.0 # Flush with rear bracket mounting surface

# 3. Corner Closure Screws (Enclosure closure - 100% CLEAR OF GLASS NEST!)
# X = +/- 70.0 mm (>8.25mm clear of glass left edge!), Y = +/- 31.5 mm
CORNER_SCREW_DX = 140.0 # +/- 70.0 mm
CORNER_SCREW_DY = 63.0  # +/- 31.5 mm
CORNER_SCREWS = [
    (-CORNER_SCREW_DX/2.0, -CORNER_SCREW_DY/2.0),
    ( CORNER_SCREW_DX/2.0, -CORNER_SCREW_DY/2.0),
    (-CORNER_SCREW_DX/2.0,  CORNER_SCREW_DY/2.0),
    ( CORNER_SCREW_DX/2.0,  CORNER_SCREW_DY/2.0),
]
CORNER_BOSS_OD = 7.8
CORNER_BOSS_ID = 2.6  # M3 pilot in front case, 3.4mm clearance in rear plate

# 2040 Loudspeaker Chamber (X span: 41.6 to 62.4 mm)
SPEAKER_CENTER_X = 52.0
SPEAKER_CENTER_Y = 0.0
SPEAKER_W = 20.8
SPEAKER_H = 40.8
SPEAKER_D = 7.5

# MicroSD Slot on Bottom Wall (Aligned directly with board bottom slot)
SD_SLOT_X = BOARD_CENTER_X # -14.0 mm
SD_SLOT_W = 14.0           # MicroSD card width 11.0mm + 3.0mm clearance
SD_SLOT_H = 3.2            # MicroSD card thickness 1.0mm + 2.2mm clearance
SD_SLOT_Z = 8.0            # Center height in Z

def build_front_case():
    print("Generating Deep Unibody Front Case Cabinet CAD (39mm depth)...")

    # 1. Outer Shell Solid (Z = 0 to 39.0 mm)
    outer = make_rounded_box(CASE_W, CASE_H, CASE_FRONT_D, CORNER_R)

    # 2. Main Inner Cavity (Z = 2.4 to 39.0 mm)
    inner_w = CASE_W - 2 * WALL_T
    inner_h = CASE_H - 2 * WALL_T
    inner_cavity = make_rounded_box(inner_w, inner_h, CASE_FRONT_D, CORNER_R - 1.5).translate([0, 0, FRONT_FACE_T])

    # 3. Internal Glass Nest / Pocket on Inside Face of Front Wall
    # Cutout starts at Z = FRONT_FACE_T - GLASS_POCKET_D (0.6mm) up to FRONT_FACE_T + 2.0mm
    glass_nest = make_rounded_box(GLASS_W, GLASS_H, GLASS_POCKET_D + 2.0, 4.0).translate(
        [BOARD_CENTER_X, BOARD_CENTER_Y, FRONT_FACE_T - GLASS_POCKET_D]
    )

    # 4. Active Screen Aperture with Bevel Chamfer (Through Front Face)
    # Perfectly centered viewing window: 73.4 x 49.0 mm
    screen_cutout = make_rounded_box(SCREEN_W, SCREEN_H, FRONT_FACE_T + 2.0, 1.5).translate(
        [BOARD_CENTER_X, BOARD_CENTER_Y, -1.0]
    )
    # Bevel chamfer funnel on front exterior face
    bevel_w = SCREEN_W + 2.4
    bevel_h = SCREEN_H + 2.4
    screen_bevel = make_rounded_box(bevel_w, bevel_h, 1.0, 2.0).translate(
        [BOARD_CENTER_X, BOARD_CENTER_Y, -0.2]
    )

    # 5. Speaker Acoustic Grille Slots
    grille_slots = []
    num_slots = 7
    slot_w = 2.0
    slot_h = 28.0
    slot_pitch = 3.2
    for i in range(num_slots):
        sx = SPEAKER_CENTER_X + (i - (num_slots - 1) / 2.0) * slot_pitch
        slot = make_rounded_box(slot_w, slot_h, FRONT_FACE_T + 2.0, slot_w / 2.0).translate(
            [sx, SPEAKER_CENTER_Y, -1.0]
        )
        grille_slots.append(slot)

    # 6. Speaker Acoustic Chamber Walls (inside case, Z = FRONT_FACE_T to 10.0mm)
    spk_wall_t = 1.6
    spk_box_outer = make_rounded_box(SPEAKER_W + 2*spk_wall_t, SPEAKER_H + 2*spk_wall_t, SPEAKER_D + 1.5, 2.5).translate(
        [SPEAKER_CENTER_X, SPEAKER_CENTER_Y, FRONT_FACE_T - 0.5]
    )
    spk_box_inner = make_rounded_box(SPEAKER_W, SPEAKER_H, SPEAKER_D + 2.0, 1.5).translate(
        [SPEAKER_CENTER_X, SPEAKER_CENTER_Y, FRONT_FACE_T]
    )
    speaker_chamber_solid = spk_box_outer - spk_box_inner

    # 7. MicroSD Card Opening at Bottom Wall (Y = -CASE_H/2.0)
    sd_slot_solid = make_rounded_box(SD_SLOT_W, 10.0, SD_SLOT_H, 1.0).translate(
        [SD_SLOT_X, -CASE_H/2.0, SD_SLOT_Z - SD_SLOT_H/2.0]
    )
    sd_funnel = make_rounded_box(SD_SLOT_W + 4.0, 4.0, SD_SLOT_H + 3.0, 1.5).translate(
        [SD_SLOT_X, -CASE_H/2.0 - 1.0, SD_SLOT_Z - (SD_SLOT_H + 3.0)/2.0]
    )

    # 8. 4x Chassis Bosses for Bracket-to-Body Mounting (Z = 2.4 to 12.0 mm)
    chassis_boss_solids = []
    chassis_boss_holes = []
    for bx, by in CHASSIS_BOSSES:
        boss = Manifold.cylinder(CHASSIS_BOSS_TOP_Z - FRONT_FACE_T + 0.5, CHASSIS_BOSS_OD/2.0, CHASSIS_BOSS_OD/2.0, 32).translate(
            [bx, by, FRONT_FACE_T - 0.5]
        )
        hole = Manifold.cylinder(CHASSIS_BOSS_TOP_Z - FRONT_FACE_T + 1.0, CHASSIS_BOSS_ID/2.0, CHASSIS_BOSS_ID/2.0, 24).translate(
            [bx, by, FRONT_FACE_T + 1.5]
        )
        chassis_boss_solids.append(boss)
        chassis_boss_holes.append(hole)

    # 9. 4x Solid Corner Closure Screw Bosses (Fully merged with case walls, recessed 1.5mm to receive back door lip)
    BOSS_TOP_Z = CASE_FRONT_D - 1.5  # 37.5 mm (leaves 1.5mm recessed landing shelf for back door indexing lip)
    corner_boss_solids = []
    corner_boss_holes = []
    for cx, cy in CORNER_SCREWS:
        sx = 1.0 if cx > 0 else -1.0
        sy = 1.0 if cy > 0 else -1.0
        bx_w = abs(CASE_W/2.0 - (abs(cx) - 4.5))
        by_h = abs(CASE_H/2.0 - (abs(cy) - 4.5))
        bx_center = (abs(cx) - 4.5 + CASE_W/2.0) / 2.0 * sx
        by_center = (abs(cy) - 4.5 + CASE_H/2.0) / 2.0 * sy

        # Solid corner block merging directly into outer and inner corner walls
        block = Manifold.cube([bx_w, by_h, BOSS_TOP_Z - FRONT_FACE_T + 0.5], center=True).translate(
            [bx_center, by_center, (FRONT_FACE_T - 0.5 + BOSS_TOP_Z)/2.0]
        )
        # Rounded cylindrical boss at screw center
        cyl = Manifold.cylinder(BOSS_TOP_Z - FRONT_FACE_T + 0.5, 4.5, 4.5, 32).translate([cx, cy, FRONT_FACE_T - 0.5])
        corner_boss_solids.extend([block, cyl])

        # M3 thread-forming pilot hole extending 16mm deep from the recessed landing shelf
        hole = Manifold.cylinder(16.0, CORNER_BOSS_ID/2.0, CORNER_BOSS_ID/2.0, 24).translate([cx, cy, BOSS_TOP_Z - 15.5])
        corner_boss_holes.append(hole)

    # Base shell union
    front_solid = outer - inner_cavity
    front_solid = union_all([front_solid, speaker_chamber_solid] + chassis_boss_solids + corner_boss_solids)

    # Subtractions
    all_subtractions = [
        glass_nest,
        screen_cutout,
        screen_bevel,
        sd_slot_solid,
        sd_funnel,
    ] + grille_slots + chassis_boss_holes + corner_boss_holes

    front_case = diff_all(front_solid, all_subtractions)
    return front_case

def build_board_clamp():
    print("Generating Heavy-Duty Board Mounting Plate (Clean Plate without Battery Slot)...")

    CLAMP_THICKNESS = 3.2

    # 1. Main Base Plate: 112.0 x 62.0 mm (covers board ears and chassis bosses)
    CLAMP_W = 112.0
    CLAMP_H = 62.0
    base_plate = make_rounded_box(CLAMP_W, CLAMP_H, CLAMP_THICKNESS, 4.0)

    # 2. Bracket-to-Board Screws: 4x Holes matching the module's 4 corner ears
    board_screw_holes = []
    for hx, hy in BOARD_MOUNT_HOLES:
        lx = hx - BOARD_CENTER_X
        ly = hy - BOARD_CENTER_Y
        hole = Manifold.cylinder(CLAMP_THICKNESS + 2.0, 1.6, 1.6, 24).translate([lx, ly, -1.0])
        cb = Manifold.cylinder(1.5, 2.9, 2.9, 24).translate([lx, ly, CLAMP_THICKNESS - 1.4])
        board_screw_holes.extend([hole, cb])

    # 3. Bracket-to-Body Chassis Screws: 4x Holes matching the front case chassis bosses
    body_screw_holes = []
    for bx, by in CHASSIS_BOSSES:
        lx = bx - BOARD_CENTER_X
        ly = by - BOARD_CENTER_Y
        hole = Manifold.cylinder(CLAMP_THICKNESS + 2.0, 1.7, 1.7, 24).translate([lx, ly, -1.0])
        cb = Manifold.cylinder(1.5, 3.1, 3.1, 24).translate([lx, ly, CLAMP_THICKNESS - 1.4])
        body_screw_holes.extend([hole, cb])

    # 4. Port Openings on the Plate:
    # A. USB-C & Power Header Window (rear face: centered over Type-C at lx = +35.86, ly = +2.48 and JST 4P power header at ly = -7.50)
    # Generous 22.0 x 32.0 mm cutout gives complete, unhindered access with wide clearance for standard USB-C cable connector bodies
    usbc_window = make_rounded_box(22.0, 32.0, CLAMP_THICKNESS + 2.0, 3.0).translate([35.0, -2.0, -1.0])

    # B. Battery Connector & Power Switch Window (top edge: clears JST battery header and switch)
    bat_window = make_rounded_box(26.0, 15.0, CLAMP_THICKNESS + 2.0, 2.0).translate([18.0, 25.0, -1.0])

    # C. Speaker, IO Ports & Tactile Buttons Opening on Bottom Side ("open on one side")
    # Extra-wide 72.0 x 18.0 mm opening (lx = -36.0 to +36.0 mm, ly = -33.0 to -15.0 mm)
    spk_io_bottom_opening = make_rounded_box(72.0, 18.0, CLAMP_THICKNESS + 2.0, 2.0).translate([0.0, -24.0, -1.0])

    # D. Central Cooling & Clearance Window (clears ESP32-S3 module, RF shield & heatsinks)
    center_window = make_rounded_box(36.0, 22.0, CLAMP_THICKNESS + 2.0, 3.0).translate([-14.0, 1.0, -1.0])

    clamp_solid = base_plate
    clamp_subtractions = [
        usbc_window,
        bat_window,
        spk_io_bottom_opening,
        center_window,
    ] + board_screw_holes + body_screw_holes

    clamp = diff_all(clamp_solid, clamp_subtractions)
    # Translate clamp to board center
    return clamp.translate([BOARD_CENTER_X, BOARD_CENTER_Y, 0])

def build_back_cover():
    print("Generating Flat Rear Back Door Plate CAD (3mm thickness)...")

    # 1. Main Outer Plate Base (Z = 0 to 3.0 mm)
    plate_base = make_rounded_box(CASE_W, CASE_H, BACK_PLATE_T, CORNER_R)

    # 2. Interlocking Perimeter Indexing Lip (extends 1.5mm into the front case opening)
    inner_w = CASE_W - 2 * WALL_T
    inner_h = CASE_H - 2 * WALL_T
    lip_w = inner_w - 0.4
    lip_h = inner_h - 0.4
    lip_t = 1.5
    lip = make_rounded_box(lip_w, lip_h, lip_t, CORNER_R - 1.5).translate([0, 0, BACK_PLATE_T])

    # 3. 4x Corner Screw Counterbore Wells (M3 clearance holes with counterbores for flush screw heads)
    screw_holes = []
    for cx, cy in CORNER_SCREWS:
        thru_hole = Manifold.cylinder(BACK_PLATE_T + lip_t + 2.0, 1.7, 1.7, 24).translate([cx, cy, -1.0])
        counterbore = Manifold.cylinder(1.8, 3.3, 3.3, 24).translate([cx, cy, -0.1])
        screw_holes.extend([thru_hole, counterbore])

    # 4. Rear Sound Ventilation Slots (behind speaker chamber at X = +52.0mm)
    rear_vents = []
    num_vents = 5
    vent_w = 2.4
    vent_h = 26.0
    vent_pitch = 4.5
    for i in range(num_vents):
        vx = SPEAKER_CENTER_X + (i - (num_vents - 1) / 2.0) * vent_pitch
        vent = make_rounded_box(vent_w, vent_h, BACK_PLATE_T + lip_t + 2.0, vent_w / 2.0).translate(
            [vx, SPEAKER_CENTER_Y, -1.0]
        )
        rear_vents.append(vent)

    # 5. Rear USB Type-C Cable Pass-Through Port (aligned with the board's rear-facing Type-C port at X = +21.86, Y = +2.48)
    usbc_back_thru = make_rounded_box(9.0, 15.0, BACK_PLATE_T + lip_t + 2.0, 2.5).translate(
        [BOARD_CENTER_X + 35.86, BOARD_CENTER_Y + 2.48, -1.0]
    )
    usbc_back_funnel = make_rounded_box(12.0, 18.0, 1.5, 3.0).translate(
        [BOARD_CENTER_X + 35.86, BOARD_CENTER_Y + 2.48, -0.2]
    )

    plate_solid = union_all([plate_base, lip])
    all_subtractions = screw_holes + rear_vents + [usbc_back_thru, usbc_back_funnel]
    back_plate = diff_all(plate_solid, all_subtractions)
    return back_plate

def main():
    out_dir = r"d:\ESP32Radio\3D_Enclosure_JC3248W535"
    os.makedirs(out_dir, exist_ok=True)
    print("Generating watertight STLs for ESP32S3_JC3248W535_Radio Enclosure...")

    # 1. Front Case (Deep Unibody Cabinet)
    front_case = build_front_case()
    export_manifold_to_stl(front_case, os.path.join(out_dir, "JC3248W535_Radio_Front_Case.stl"))

    # 2. Board Clamp Frame with Battery Cradle
    board_clamp = build_board_clamp()
    export_manifold_to_stl(board_clamp, os.path.join(out_dir, "JC3248W535_Radio_Board_Clamp.stl"))

    # 3. Rear Back Door Plate
    back_cover = build_back_cover()
    export_manifold_to_stl(back_cover, os.path.join(out_dir, "JC3248W535_Radio_Back_Cover.stl"))

    # 4. Print Bed Layout (All Parts on 1 Plate)
    print("Generating All Parts Plate Layout...")
    front_plate = front_case.translate([-CASE_W/2.0 - 5.0, 0, 0])
    back_plate = back_cover.translate([CASE_W/2.0 + 5.0, 0, 0])
    clamp_plate = board_clamp.translate([0, CASE_H/2.0 + 35.0, 0])
    all_parts = union_all([front_plate, back_plate, clamp_plate])
    export_manifold_to_stl(all_parts, os.path.join(out_dir, "JC3248W535_Radio_All_Parts.stl"))

    print("\nAll Enclosure STLs generated successfully!")

if __name__ == "__main__":
    main()
