"""
virtual_assembly_model.py
Detailed 3D Virtual Assembly and CAD Models for ESP32S3_JC3248W535_Radio
Produces:
 - Detailed bonded electronic module (Glass, LCD screen, black injection housing, connectors, TF card socket, MicroSD card, tactile buttons)
 - 2040 Loudspeaker with acoustic gasket and driver cone
 - 4x M3 Clamp Fasteners & 4x M3 Rear Closure Screws
 - Mated Assembly STL (all parts in assembled operating positions)
 - Exploded Assembly STL (stepped along Z axis showing internal placement and clamping)
"""

import os
import functools
import numpy as np
import trimesh
from manifold3d import Manifold

from generate_jc3248w535_stl import (
    build_front_case, build_board_clamp, build_back_cover,
    make_rounded_box, union_all, diff_all, export_manifold_to_stl,
    CASE_W, CASE_H, CASE_FRONT_D, BACK_PLATE_T, WALL_T, FRONT_FACE_T,
    BOARD_CENTER_X, BOARD_CENTER_Y, BOARD_MOUNT_HOLES, CHASSIS_BOSSES, CORNER_SCREWS,
    SPEAKER_CENTER_X, SPEAKER_CENTER_Y, SPEAKER_W, SPEAKER_H, SPEAKER_D,
    SD_SLOT_X, SD_SLOT_Z
)

def build_jc3248w535_module():
    """Builds a realistic 3D CAD representation of the JC3248W535 display module."""
    print("Building detailed JC3248W535 display module CAD...")

    bx = BOARD_CENTER_X
    by = BOARD_CENTER_Y

    # 1. Front Glass: 94.5 x 62.0 x 1.6 mm (Z = 0.6 to 2.2 mm)
    glass = make_rounded_box(94.5, 62.0, 1.6, 4.0).translate([bx, by, 0.6])

    # 2. Active Screen Panel: 73.4 x 49.0 x 0.5 mm (Z = 0.8 to 1.3 mm)
    screen = make_rounded_box(73.4, 49.0, 0.5, 1.0).translate([bx, by, 0.8])

    # 3. Main Black Injection Molded Rear Casing: 82.94 x 58.36 x 9.8 mm (Z = 2.2 to 12.0 mm)
    casing_solid = make_rounded_box(82.94, 58.36, 9.8, 3.0).translate([bx, by, 2.2])

    # 4 Corner Mounting Ears with pilot holes (scalloped corners at X = +/-42.15, Y = +/-26.15)
    ear_solids = []
    ear_holes = []
    for hx, hy in BOARD_MOUNT_HOLES:
        ear = Manifold.cylinder(9.8, 4.0, 4.0, 32).translate([hx, hy, 2.2])
        hole = Manifold.cylinder(12.0, 1.3, 1.3, 24).translate([hx, hy, 1.0])
        ear_solids.append(ear)
        ear_holes.append(hole)

    # 4. MicroSD Card Reader Slot on Bottom Wall of Black Shell (matches user image)
    # The slot is centered on bottom wall: 13.0 x 1.8 mm, from Z = 6.0 to 7.8 mm
    tf_slit = make_rounded_box(13.0, 6.0, 1.8, 0.5).translate([bx, by - 29.18, 6.9])
    
    # Internal MicroSD Card seated in socket, extending down to slot opening
    tf_card = make_rounded_box(11.0, 15.0, 1.0, 0.8).translate([bx, by - 31.0, 7.3])

    # 5. Rear Recessed Connector Well in Black Shell
    tc_x = bx + 35.86
    tc_y = by + 2.48
    jst_pwr_y = by - 7.50
    well_cutout = make_rounded_box(7.5, 22.5, 3.5, 2.0).translate([tc_x, by - 2.5, 11.0])

    # 6. USB Type-C Receptacle on REAR face:
    # Outer metal shell: 3.6mm in X, 9.0mm in Y, 4.5mm in Z (Z = 7.5 to 12.0 mm, facing +Z)
    usbc_outer = make_rounded_box(3.6, 9.0, 4.5, 1.0).translate([tc_x, tc_y, 9.75])
    usbc_inner = make_rounded_box(2.4, 6.8, 4.7, 0.8).translate([tc_x, tc_y, 9.85])
    usbc_tongue = make_rounded_box(0.7, 5.0, 3.2, 0.2).translate([tc_x, tc_y, 9.4])
    usbc_metal = (usbc_outer - usbc_inner) + usbc_tongue

    # JST 1.25 4P Power Header directly below Type-C on rear face
    jst_pwr = make_rounded_box(5.0, 7.5, 3.0, 0.5).translate([tc_x, jst_pwr_y, 10.5])

    # 7. Tactile Switches & Other JST Headers
    btn_boot = make_rounded_box(4.0, 3.0, 2.0, 0.5).translate([bx - 34.5, by - 25.5, 10.5])
    btn_rst = make_rounded_box(4.0, 3.0, 2.0, 0.5).translate([bx - 38.5, by - 25.5, 10.5])
    jst_spk = make_rounded_box(6.0, 5.0, 4.0, 0.5).translate([bx + 18.0, by - 25.5, 10.0])
    jst_io = make_rounded_box(12.0, 5.0, 4.0, 0.5).translate([bx + 0.0, by - 25.5, 10.0])
    jst_4p = make_rounded_box(8.0, 5.0, 4.0, 0.5).translate([bx - 15.0, by - 25.5, 10.0])
    jst_bat = make_rounded_box(5.0, 5.0, 4.0, 0.5).translate([bx + 13.5, by + 25.5, 10.0])
    btn_bat = Manifold.cylinder(3.0, 1.2, 1.2, 16).translate([bx + 23.0, by + 25.5, 10.0])

    # 8. ESP32-S3 Metal RF Shield Can & TF Card Socket
    esp32_shield = make_rounded_box(18.0, 25.5, 2.8, 1.0).translate([bx - 12.0, by + 3.0, 12.0])
    tf_socket = make_rounded_box(14.0, 14.5, 1.8, 0.8).translate([bx, by - 22.0, 12.0])

    # 9. 4x Brass Threaded Insert Rings in Corner Ears
    brass_inserts = []
    for hx, hy in BOARD_MOUNT_HOLES:
        b_outer = Manifold.cylinder(2.0, 2.5, 2.5, 24).translate([hx, hy, 10.1])
        b_inner = Manifold.cylinder(2.4, 1.5, 1.5, 24).translate([hx, hy, 9.9])
        brass_inserts.append(b_outer - b_inner)
    brass_solid = union_all(brass_inserts)

    casing_body = diff_all(union_all([casing_solid] + ear_solids), [tf_slit, well_cutout] + ear_holes)

    module_solid = union_all([
        glass, screen, casing_body, usbc_metal, tf_card, tf_socket, esp32_shield, brass_solid,
        btn_boot, btn_rst, jst_spk, jst_io, jst_4p, jst_pwr, jst_bat, btn_bat
    ])

    return module_solid, glass, screen, casing_body, tf_card, esp32_shield, usbc_metal, brass_solid

def build_2040_speaker():
    """Builds a realistic 3D CAD representation of the 2040 loudspeaker."""
    print("Building 2040 Loudspeaker CAD...")
    sx = SPEAKER_CENTER_X
    sy = SPEAKER_CENTER_Y

    body = make_rounded_box(20.0, 40.0, 7.2, 2.0).translate([sx, sy, 2.4])
    gasket_outer = make_rounded_box(18.5, 38.5, 0.8, 1.8).translate([sx, sy, 1.6])
    gasket_inner = make_rounded_box(14.0, 32.0, 1.0, 1.2).translate([sx, sy, 1.5])
    gasket = gasket_outer - gasket_inner
    cone = Manifold.cylinder(1.5, 6.0, 4.5, 32).translate([sx, sy, 1.8])
    dust_cap = Manifold.cylinder(0.8, 3.0, 3.0, 24).translate([sx, sy, 1.2])

    return union_all([body, gasket, cone, dust_cap])

def build_bracket_to_board_screws(seated_z=12.0):
    """4x Screws fastening the bracket onto the board's 4 corner ears."""
    screws = []
    for bx, by in BOARD_MOUNT_HOLES:
        head = Manifold.cylinder(2.0, 2.5, 2.5, 24).translate([bx, by, seated_z + 2.4])
        shank = Manifold.cylinder(7.5, 1.25, 1.25, 20).translate([bx, by, seated_z - 5.1])
        screws.append(head + shank)
    return union_all(screws)

def build_bracket_to_body_screws(seated_z=12.0):
    """4x Screws fastening the bracket outer tabs to the front case chassis bosses."""
    screws = []
    for bx, by in CHASSIS_BOSSES:
        head = Manifold.cylinder(2.5, 2.9, 2.9, 24).translate([bx, by, seated_z + 2.4])
        shank = Manifold.cylinder(10.0, 1.45, 1.45, 20).translate([bx, by, seated_z - 7.6])
        screws.append(head + shank)
    return union_all(screws)

def build_corner_screws(counterbore_floor_z=CASE_FRONT_D + BACK_PLATE_T - 1.8):
    """4x M3 cap head screws for securing the rear cover plate."""
    screws = []
    for cx, cy in CORNER_SCREWS:
        head = Manifold.cylinder(2.0, 2.8, 2.8, 24).translate([cx, cy, counterbore_floor_z])
        shank = Manifold.cylinder(16.0, 1.45, 1.45, 20).translate([cx, cy, counterbore_floor_z - 16.0])
        screws.append(head + shank)
    return union_all(screws)

def build_battery():
    """Builds a realistic 3D 18650 Li-ion battery cell seated inside the front body cradle."""
    print("Building 18650 Battery CAD...")
    bx = BOARD_CENTER_X
    by = -13.5
    bz = 25.0
    # Cylinder along X axis (length 65mm, radius 9.1mm)
    cell = Manifold.cylinder(65.0, 9.1, 9.1, 32).rotate([0, 90, 0]).translate([bx - 32.5, by, bz])
    cap = Manifold.cylinder(2.0, 3.5, 3.5, 24).rotate([0, 90, 0]).translate([bx + 32.5, by, bz])
    return union_all([cell, cap])

def main():
    out_dir = r"d:\ESP32Radio\3D_Enclosure_JC3248W535"
    os.makedirs(out_dir, exist_ok=True)
    print("Building full Virtual Assembly CAD models...")

    # 1. Enclosure Parts
    front_case = build_front_case()
    board_clamp = build_board_clamp()
    back_cover = build_back_cover()

    # 2. Electronics & Hardware
    board_module, glass, screen, casing, tf_card, esp32_shield, usbc_metal, brass_solid = build_jc3248w535_module()
    speaker = build_2040_speaker()
    battery = build_battery()

    # Fastener Sets
    screws_b2board = build_bracket_to_board_screws(seated_z=12.0)
    screws_b2body = build_bracket_to_body_screws(seated_z=12.0)
    corner_screws = build_corner_screws()

    # Back plate when mated: flipped 180 around X so rim meets front case at Z=39.0mm
    rot_x_180 = trimesh.transformations.rotation_matrix(np.pi, [1, 0, 0])
    back_tri = trimesh.Trimesh(
        vertices=back_cover.to_mesh().vert_properties[:, :3],
        faces=back_cover.to_mesh().tri_verts,
        process=False
    )
    back_mated_tri = back_tri.apply_transform(rot_x_180).apply_translation([0, 0, CASE_FRONT_D + BACK_PLATE_T])

    # 3. Mated Virtual Assembly STL
    print("\nAssembling Mated Model...")
    clamp_mated = board_clamp.translate([0, 0, 12.0])

    mated_parts = [
        front_case,
        board_module,
        clamp_mated,
        screws_b2board,
        screws_b2body,
        speaker,
        battery,
        corner_screws,
    ]
    mated_solid = union_all(mated_parts)

    mated_mesh = trimesh.Trimesh(
        vertices=mated_solid.to_mesh().vert_properties[:, :3],
        faces=mated_solid.to_mesh().tri_verts,
        process=True
    )
    full_mated_assembly = trimesh.util.concatenate([mated_mesh, back_mated_tri])
    print(f"Exporting JC3248W535_Radio_Virtual_Assembly_Mated.stl: Vertices={len(full_mated_assembly.vertices)}, Faces={len(full_mated_assembly.faces)}")
    full_mated_assembly.export(os.path.join(out_dir, "JC3248W535_Radio_Virtual_Assembly_Mated.stl"))

    # 4. Exploded Virtual Assembly STL (Step-by-step along Z-axis)
    print("\nAssembling Exploded Model...")
    # Front Case: Z = 0.0 mm
    # Speaker: Z + 15.0 mm
    # Board Module: Z + 30.0 mm
    # Board Clamp with Battery Cradle: Z + 50.0 mm
    # Screws Bracket to Board: Z + 62.0 mm
    # Screws Bracket to Body: Z + 74.0 mm
    # Battery: Z + 88.0 mm
    # Back Door Plate: Z + 110.0 mm
    # Corner Screws: Z + 130.0 mm
    front_exp = front_case
    spk_exp = speaker.translate([0, 0, 15.0])
    board_exp = board_module.translate([0, 0, 30.0])
    clamp_exp = board_clamp.translate([0, 0, 50.0])
    b2board_exp = screws_b2board.translate([0, 0, 50.0 + 12.0])
    b2body_exp = screws_b2body.translate([0, 0, 50.0 + 24.0])
    battery_exp = battery.translate([0, 0, 60.0])

    back_exp_tri = back_tri.copy().apply_transform(rot_x_180).apply_translation([0, 0, CASE_FRONT_D + BACK_PLATE_T + 110.0])
    corner_screws_exp = corner_screws.translate([0, 0, 110.0 + 20.0])

    exploded_solid = union_all([front_exp, spk_exp, board_exp, clamp_exp, b2board_exp, b2body_exp, battery_exp, corner_screws_exp])
    exploded_mesh = trimesh.Trimesh(
        vertices=exploded_solid.to_mesh().vert_properties[:, :3],
        faces=exploded_solid.to_mesh().tri_verts,
        process=True
    )
    full_exploded_assembly = trimesh.util.concatenate([exploded_mesh, back_exp_tri])
    print(f"Exporting JC3248W535_Radio_Virtual_Assembly_Exploded.stl: Vertices={len(full_exploded_assembly.vertices)}, Faces={len(full_exploded_assembly.faces)}")
    full_exploded_assembly.export(os.path.join(out_dir, "JC3248W535_Radio_Virtual_Assembly_Exploded.stl"))

    print("\nVirtual Assembly CAD models generated successfully!")

if __name__ == "__main__":
    main()
