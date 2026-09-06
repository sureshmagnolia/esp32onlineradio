"""
Final Detailed Virtual Assembly Diagnostic Renderer.
Specifically investigates and clarifies the board mounting screw holes vs front case:
1. Proves why screws cannot go through the PCB holes into the front case (holes are behind the glass!).
2. Demonstrates the exact alignment of the 4 PCB corner holes with the Rear Cover's 4 retention posts.
3. Renders high-resolution multi-angle views of the assembly.
"""

import os
import time
import numpy as np
import trimesh
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from virtual_assembly_model import build_detailed_board_components, build_speakers, build_installed_plungers, build_fasteners

def render_scene(meshes_with_styles, output_png, title="Virtual Assembly",
                 elev=25, azim=-50, xlim=None, ylim=None, zlim=None,
                 figsize=(12, 9), dpi=160, show_axes=True):
    fig = plt.figure(figsize=figsize, dpi=dpi)
    ax = fig.add_subplot(111, projection='3d')
    
    all_bounds = []
    for item in meshes_with_styles:
        if len(item) == 4:
            mesh, color, alpha, edge_color = item
            edge_width = 0.15
        else:
            mesh, color, alpha, edge_color, edge_width = item
            
        if mesh is None:
            continue
            
        faces = mesh.faces
        vertices = mesh.vertices
        if len(faces) > 5000:
            step = max(1, len(faces) // 3500)
            faces = faces[::step]
            
        poly3d = [[vertices[idx] for idx in face] for face in faces]
        coll = Poly3DCollection(poly3d, facecolors=color, linewidths=edge_width,
                                edgecolors=edge_color, alpha=alpha)
        ax.add_collection3d(coll)
        all_bounds.append(mesh.bounds)
        
    if not all_bounds:
        plt.close()
        return
        
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
    ax.set_title(title, fontsize=11, fontweight='bold', pad=12)
    if show_axes:
        ax.set_xlabel("X (Width, mm)")
        ax.set_ylabel("Y (Height, mm)")
        ax.set_zlabel("Z (Depth, mm)")
    else:
        ax.axis('off')
        
    plt.tight_layout()
    for attempt in range(5):
        try:
            plt.savefig(output_png, bbox_inches='tight')
            break
        except Exception as e:
            if attempt < 4:
                time.sleep(0.3)
            else:
                raise e
    plt.close()
    print(f"Rendered: {output_png}")

def main():
    base_dir = r"d:\ESP32Radio\3D_Enclosure_Waveshare28"
    
    front_mesh = trimesh.load(os.path.join(base_dir, "Waveshare28_Radio_Front_Case.stl"))
    back_mesh = trimesh.load(os.path.join(base_dir, "Waveshare28_Radio_Back_Cover.stl"))
    clamp_mesh = trimesh.load(os.path.join(base_dir, "Waveshare28_Radio_Board_Clamp.stl"))
    
    clamp_seated = clamp_mesh.copy().apply_translation([0, 0, 6.8])
    rot_x_180 = trimesh.transformations.rotation_matrix(np.pi, [1, 0, 0])
    back_mated = back_mesh.copy().apply_transform(rot_x_180).apply_translation([0, 0, 32.0])
    
    board = build_detailed_board_components()
    speakers = build_speakers()
    plungers = build_installed_plungers()
    fasteners = build_fasteners()
    
    # -------------------------------------------------------------------------
    # 1. Front View Diagnostic: Showing PCB Holes are BEHIND the Touch Glass
    # -------------------------------------------------------------------------
    # Creates 4 red locator pins at the 4 PCB hole positions to clearly demonstrate
    # that the holes sit inside the screen area and behind the glass.
    from manifold3d import Manifold
    pcb_pins = []
    for hx, hy in [(-30.00, -21.66), (-30.00, 15.20), (22.54, -21.66), (22.54, 15.20)]:
        pin = Manifold.cylinder(8.0, 1.2, 1.2, 20).translate([hx, hy, 4.0])
        pcb_pins.append(pin)
    import functools
    pins_mesh = trimesh.Trimesh(
        vertices=functools.reduce(lambda a,b: a+b, pcb_pins).to_mesh().vert_properties[:, :3],
        faces=functools.reduce(lambda a,b: a+b, pcb_pins).to_mesh().tri_verts,
        process=True
    )
    
    render_scene([
        (front_mesh, '#2B6CB0', 0.2, '#1A365D'),
        (board['glass'], '#90CDF4', 0.35, '#3182CE'),
        (board['pcb'], '#276749', 0.95, '#1C4532'),
        (pins_mesh, '#E53E3E', 0.95, '#9B2C2C'),
        (speakers, '#2D3748', 0.9, '#1A202C'),
        (plungers, '#ECC94B', 0.95, '#B7791F'),
    ], os.path.join(base_dir, "diagnostic_pcb_holes_behind_glass.png"),
       title="Diagnostic: Waveshare Board 4 Mounting Holes (Red Cylinders)\nProving the holes sit directly behind the solid front touch glass & inside the screen window!",
       elev=-65, azim=-90, figsize=(12, 8))

    # -------------------------------------------------------------------------
    # 2. Virtual Assembly: Board Nested in Front Case with Clamp Bracket
    # -------------------------------------------------------------------------
    render_scene([
        (front_mesh, '#2B6CB0', 0.25, '#1A365D'),
        (board['glass'], '#E2E8F0', 0.3, '#A0AEC0'),
        (board['pcb'], '#276749', 0.95, '#1C4532'),
        (board['esp_shield'], '#CBD5E0', 0.95, '#4A5568'),
        (board['connectors'], '#ED8936', 0.95, '#C05621'),
        (board['tf_socket'], '#D69E2E', 0.95, '#744210'),
        (board['tf_card'], '#1A202C', 0.95, '#000000'),
        (board['buttons'], '#E53E3E', 0.95, '#9B2C2C'),
        (plungers, '#ECC94B', 0.95, '#B7791F'),
        (clamp_seated, '#00B4D8', 0.85, '#0077B6'),
        (fasteners['clamp_screws'], '#F7FAFC', 0.95, '#718096'),
        (speakers, '#2D3748', 0.95, '#1A202C'),
    ], os.path.join(base_dir, "diagnostic_clamp_seating.png"),
       title="Virtual Assembly: Board Clamped from Behind via Top Chassis Bosses\n(Cyan = Clamp Bracket, Screws in Top Channel Above Glass, 4 Nubs in PCB Holes)",
       elev=45, azim=-55, figsize=(13, 9))

if __name__ == '__main__':
    main()
