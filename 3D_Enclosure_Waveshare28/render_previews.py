"""
Render 3D visual preview images of the Waveshare ESP32-S3-Touch-LCD-2.8 Stereo Cabinet.
Renders:
1. Front Cabinet Perspective & Orthographic View (with dual speaker grilles, screen window, and standoffs)
2. Rear Cover Perspective & Orthographic View (with ventilation slots, battery cradle, tilt feet)
3. Full Assembled Enclosure View
"""

import os
import trimesh
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

def render_mesh_views(mesh_path, output_png, title="3D CAD Preview", elev=25, azim=-45, color='#4A90E2', alpha=0.9):
    mesh = trimesh.load(mesh_path)
    
    fig = plt.figure(figsize=(12, 8), dpi=150)
    ax = fig.add_subplot(111, projection='3d')
    
    # Subsample faces if high count for fast crisp rendering
    faces = mesh.faces
    vertices = mesh.vertices
    if len(faces) > 8000:
        step = len(faces) // 6000
        faces = faces[::step]
        
    poly3d = [[vertices[idx] for idx in face] for face in faces]
    collection = Poly3DCollection(poly3d, facecolors=color, linewidths=0.1, edgecolors='#1A365D', alpha=alpha)
    ax.add_collection3d(collection)
    
    # Auto scale limits
    bounds = mesh.bounds
    max_range = np.array([bounds[1][0] - bounds[0][0], 
                          bounds[1][1] - bounds[0][1], 
                          bounds[1][2] - bounds[0][2]]).max() / 2.0
    mid_x = (bounds[1][0] + bounds[0][0]) * 0.5
    mid_y = (bounds[1][1] + bounds[0][1]) * 0.5
    mid_z = (bounds[1][2] + bounds[0][2]) * 0.5
    
    ax.set_xlim(mid_x - max_range, mid_x + max_range)
    ax.set_ylim(mid_y - max_range, mid_y + max_range)
    ax.set_zlim(mid_z - max_range, mid_z + max_range)
    
    ax.view_init(elev=elev, azim=azim)
    ax.set_box_aspect([bounds[1][0]-bounds[0][0], bounds[1][1]-bounds[0][1], bounds[1][2]-bounds[0][2]])
    ax.set_title(f"{title}\nWatertight={mesh.is_watertight}, Dimensions: {mesh.extents[0]:.1f} x {mesh.extents[1]:.1f} x {mesh.extents[2]:.1f} mm", 
                 fontsize=13, fontweight='bold', pad=10)
    ax.set_xlabel("X (mm, Width)")
    ax.set_ylabel("Y (mm, Height)")
    ax.set_zlabel("Z (mm, Depth)")
    
    plt.tight_layout()
    plt.savefig(output_png, bbox_inches='tight')
    plt.close()
    print(f"Rendered: {output_png}")

def main():
    base_dir = r"d:\ESP32Radio\3D_Enclosure_Waveshare28"
    
    front_stl = os.path.join(base_dir, "Waveshare28_Radio_Front_Case.stl")
    back_stl = os.path.join(base_dir, "Waveshare28_Radio_Back_Cover.stl")
    all_stl = os.path.join(base_dir, "Waveshare28_Radio_All_Parts.stl")
    
    render_mesh_views(front_stl, os.path.join(base_dir, "preview_front_exterior.png"), 
                      title="Waveshare 2.8 Radio - Front Cabinet (Exterior View)", elev=25, azim=-55, color='#2B6CB0')
    render_mesh_views(front_stl, os.path.join(base_dir, "preview_front_interior.png"), 
                      title="Waveshare 2.8 Radio - Front Cabinet (Internal Standoffs & Speaker Pockets)", elev=-60, azim=125, color='#3182CE')
    render_mesh_views(back_stl, os.path.join(base_dir, "preview_rear_cover.png"), 
                      title="Waveshare 2.8 Radio - Rear Cover (Vents, Mating Lip, Battery Bay)", elev=30, azim=-45, color='#2C7A7B')
    render_mesh_views(all_stl, os.path.join(base_dir, "preview_all_parts_plate.png"), 
                      title="Waveshare 2.8 Radio - Full Print Plate Layout", elev=35, azim=-60, color='#4A5568')

if __name__ == "__main__":
    main()
