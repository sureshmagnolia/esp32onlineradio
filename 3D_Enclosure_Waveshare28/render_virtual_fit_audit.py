"""
Virtual Board Fitment & Alignment Audit Renderer.
Generates multi-angle diagnostic views showing the physical Waveshare board
seated virtually inside the 3D-printed enclosure.
"""

import os
import trimesh
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from virtual_board import build_virtual_board

def render_scene(meshes_with_colors, output_png, title="Virtual Fitment Audit", elev=30, azim=-45, xlim=None, ylim=None, zlim=None):
    fig = plt.figure(figsize=(12, 8), dpi=150)
    ax = fig.add_subplot(111, projection='3d')
    
    all_bounds = []
    for mesh, color, alpha, edge_color in meshes_with_colors:
        faces = mesh.faces
        vertices = mesh.vertices
        if len(faces) > 6000:
            step = len(faces) // 4000
            faces = faces[::step]
        poly3d = [[vertices[idx] for idx in face] for face in faces]
        coll = Poly3DCollection(poly3d, facecolors=color, linewidths=0.15, edgecolors=edge_color, alpha=alpha)
        ax.add_collection3d(coll)
        all_bounds.append(mesh.bounds)
        
    all_bounds = np.concatenate(all_bounds, axis=0)
    min_b = all_bounds.min(axis=0)
    max_b = all_bounds.max(axis=0)
    
    mid_x = (min_b[0] + max_b[0]) * 0.5
    mid_y = (min_b[1] + max_b[1]) * 0.5
    mid_z = (min_b[2] + max_b[2]) * 0.5
    max_range = (max_b - min_b).max() * 0.55
    
    if xlim: ax.set_xlim(xlim)
    else: ax.set_xlim(mid_x - max_range, mid_x + max_range)
    if ylim: ax.set_ylim(ylim)
    else: ax.set_ylim(mid_y - max_range, mid_y + max_range)
    if zlim: ax.set_zlim(zlim)
    else: ax.set_zlim(mid_z - max_range, mid_z + max_range)
    
    ax.view_init(elev=elev, azim=azim)
    ax.set_title(title, fontsize=13, fontweight='bold', pad=10)
    ax.set_xlabel("X (mm, Width)")
    ax.set_ylabel("Y (mm, Height)")
    ax.set_zlabel("Z (mm, Depth)")
    
    plt.tight_layout()
    plt.savefig(output_png, bbox_inches='tight')
    plt.close()
    print(f"Rendered: {output_png}")

def main():
    base_dir = r"d:\ESP32Radio\3D_Enclosure_Waveshare28"
    
    front_case_path = os.path.join(base_dir, "Waveshare28_Radio_Front_Case.stl")
    front_mesh = trimesh.load(front_case_path)
    
    board_manifold, info = build_virtual_board()
    b_mesh = board_manifold.to_mesh()
    board_mesh = trimesh.Trimesh(vertices=b_mesh.vert_properties[:, :3], faces=b_mesh.tri_verts, process=True)
    # Position board inside the case:
    # Front of glass sits at Z = WALL_T (2.4mm)
    board_seated = board_mesh.copy().apply_translation([0, 0, 2.4])
    
    # 1. Interior View: Board seated on 4 standoffs with dual 2030 speaker chambers
    render_scene([
        (front_mesh, '#2B6CB0', 0.45, '#1A365D'),
        (board_seated, '#38A169', 0.95, '#1C4532')
    ], os.path.join(base_dir, "virtual_audit_internal_seating.png"),
       title="Virtual Audit: Waveshare Board Seated on 4 Standoff Posts\n(Blue = Cabinet, Green = ESP32-S3 Board)",
       elev=-55, azim=130)
       
    # 2. Bottom View: Exact Alignment of 3 Buttons & MicroSD Slot
    render_scene([
        (front_mesh, '#2B6CB0', 0.4, '#1A365D'),
        (board_seated, '#E53E3E', 0.95, '#742A2A')
    ], os.path.join(base_dir, "virtual_audit_bottom_buttons_and_sd.png"),
       title="Virtual Audit: Bottom Wall Alignment - 3x Buttons & MicroSD Slot\n(Red = Board Components, Blue = Case Tunnels)",
       elev=-85, azim=-90,
       xlim=[-45, 15], ylim=[-40, -15], zlim=[0, 15])
       
    # 3. Right Wall View: USB Type-C Port Alignment
    render_scene([
        (front_mesh, '#2B6CB0', 0.4, '#1A365D'),
        (board_seated, '#D69E2E', 0.95, '#744210')
    ], os.path.join(base_dir, "virtual_audit_right_usbc.png"),
       title="Virtual Audit: Right Wall USB-C Alignment\n(Yellow = USB-C Port, Blue = Case Cutout & Collar)",
       elev=0, azim=-10,
       xlim=[20, 75], ylim=[-20, 20], zlim=[2, 14])

    # 4. Exterior Front View: Screen Window & Dual Speaker Grilles
    render_scene([
        (front_mesh, '#2B6CB0', 0.85, '#1A365D'),
        (board_seated, '#319795', 0.9, '#1D4044')
    ], os.path.join(base_dir, "virtual_audit_exterior_front.png"),
       title="Virtual Audit: Front Face View - Screen Aperture & Dual Speaker Grilles\n(Teal = Active Screen & Glass, Blue = Front Case)",
       elev=25, azim=-55)

if __name__ == '__main__':
    main()
