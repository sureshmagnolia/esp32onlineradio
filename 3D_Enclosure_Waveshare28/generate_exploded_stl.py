"""
Generates 3D STL files of the Virtual Assembly:
1. Waveshare28_Radio_Virtual_Assembly_Exploded.stl: Full exploded 3D CAD stackup showing every component.
2. Waveshare28_Radio_Virtual_Assembly_Mated.stl: Fully assembled radio showing all internals nested in place.
"""

import os
import functools
import numpy as np
import trimesh
from manifold3d import Manifold
from virtual_assembly_model import (
    build_detailed_board_components,
    build_speakers,
    build_installed_plungers,
    build_fasteners
)

def union_all_meshes(mesh_list):
    valid = [m for m in mesh_list if m is not None]
    if not valid:
        return None
    return trimesh.util.concatenate(valid)

def main():
    base_dir = r"d:\ESP32Radio\3D_Enclosure_Waveshare28"
    
    # 1. Load enclosure parts
    front_case = trimesh.load(os.path.join(base_dir, "Waveshare28_Radio_Front_Case.stl"))
    back_cover = trimesh.load(os.path.join(base_dir, "Waveshare28_Radio_Back_Cover.stl"))
    clamp = trimesh.load(os.path.join(base_dir, "Waveshare28_Radio_Board_Clamp.stl"))
    
    # Rear cover: flip so lip points towards front (-Z) and outer face is at Z = 32.0mm
    rot_x_180 = trimesh.transformations.rotation_matrix(np.pi, [1, 0, 0])
    back_mated = back_cover.copy().apply_transform(rot_x_180).apply_translation([0, 0, 32.0])
    
    # Clamp seated position: rests directly on top of factory standoffs at Z = 9.8mm
    clamp_seated = clamp.copy().apply_translation([0, 0, 9.8])
    
    # 2. Build virtual board, speakers, plungers, and fasteners
    board = build_detailed_board_components()
    speakers = build_speakers()
    plungers = build_installed_plungers()
    fasteners = build_fasteners()
    
    # Combine board sub-parts into single unified board mesh
    board_parts = [
        board['glass'],
        board['active_screen'],
        board['lcd_midframe'],
        board['fpc_ribbon'],
        board['pcb'],
        board['standoffs'],
        board['buttons'],
        board['tf_socket'],
        board['tf_card'],
        board['usbc'],
        board['connectors'],
        board['smd_chips']
    ]
    board_unified = union_all_meshes(board_parts)
    
    # -------------------------------------------------------------------------
    # STL 1: EXPLODED VIRTUAL ASSEMBLY
    # -------------------------------------------------------------------------
    # Stagger components along Z (and Y for plungers)
    exp_front = front_case.copy()
    exp_spk = speakers.copy().apply_translation([0, 0, 12.0])
    exp_plungers = plungers.copy().apply_translation([0, -14.0, 0])
    exp_board = board_unified.copy().apply_translation([0, 0, 26.0])
    exp_clamp = clamp_seated.copy().apply_translation([0, 0, 48.0])
    exp_clamp_screws = fasteners['clamp_screws'].copy().apply_translation([0, 0, 56.0])
    exp_back = back_mated.copy().apply_translation([0, 0, 68.0])
    exp_corner_screws = fasteners['corner_screws'].copy().apply_translation([0, 0, 88.0])
    
    exploded_assembly = union_all_meshes([
        exp_front,
        exp_spk,
        exp_plungers,
        exp_board,
        exp_clamp,
        exp_clamp_screws,
        exp_back,
        exp_corner_screws
    ])
    
    exploded_stl_path = os.path.join(base_dir, "Waveshare28_Radio_Virtual_Assembly_Exploded.stl")
    exploded_assembly.export(exploded_stl_path)
    print(f"Exported Exploded STL: {exploded_stl_path}")
    print(f"  Vertices: {len(exploded_assembly.vertices)}, Faces: {len(exploded_assembly.faces)}, Watertight: {exploded_assembly.is_watertight}")
    
    # -------------------------------------------------------------------------
    # STL 2: FULLY MATED / ASSEMBLED VIRTUAL ASSEMBLY
    # -------------------------------------------------------------------------
    mated_assembly = union_all_meshes([
        front_case,
        speakers,
        plungers,
        board_unified,
        clamp_seated,
        fasteners['clamp_screws'],
        back_mated,
        fasteners['corner_screws']
    ])
    
    mated_stl_path = os.path.join(base_dir, "Waveshare28_Radio_Virtual_Assembly_Mated.stl")
    mated_assembly.export(mated_stl_path)
    print(f"Exported Mated STL: {mated_stl_path}")
    print(f"  Vertices: {len(mated_assembly.vertices)}, Faces: {len(mated_assembly.faces)}, Watertight: {mated_assembly.is_watertight}")

if __name__ == '__main__':
    main()
