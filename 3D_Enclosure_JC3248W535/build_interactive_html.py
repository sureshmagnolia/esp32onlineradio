"""
build_interactive_html.py
Generates a standalone, interactive 3D WebGL / Three.js assembly & exploded view application
for the new ESP32S3_JC3248W535_Radio cabinet enclosure.
Features:
 - X-Ray Mode: Instantly see through the outer enclosure to inspect all internal components
 - Two-Stage Screw System:
    * 4x Bracket-to-Board Screws: Fastens bracket directly onto the board's 4 corner standoffs
    * 4x Bracket-to-Body Screws: Fastens the bracket+board assembly onto the front case chassis bosses
 - Bottom MicroSD card slot: Clear demonstration of card insertion and push-push ejection
 - 18650 / LiPo Battery: Seated in the molded rear retention cradle with cable routing
 - 2040 Loudspeaker: Right acoustic chamber with front sound grille slots
 - Interactive Explode Slider (0% to 100%)
 - Component visibility toggles and detailed inspector
"""

import os
import json
import base64
import numpy as np
import trimesh
from manifold3d import Manifold

from generate_jc3248w535_stl import (
    build_front_case, build_board_clamp, build_back_cover,
    make_rounded_box, union_all, diff_all,
    CASE_W, CASE_H, CASE_FRONT_D, BACK_PLATE_T, WALL_T, FRONT_FACE_T,
    BOARD_CENTER_X, BOARD_CENTER_Y, BOARD_MOUNT_HOLES, CHASSIS_BOSSES, CORNER_SCREWS,
    SPEAKER_CENTER_X, SPEAKER_CENTER_Y, SPEAKER_W, SPEAKER_H, SPEAKER_D,
    SD_SLOT_X, SD_SLOT_Z
)
from virtual_assembly_model import (
    build_jc3248w535_module, build_2040_speaker, build_battery,
    build_bracket_to_board_screws, build_bracket_to_body_screws, build_corner_screws
)

def mesh_to_dict(mesh, name, color, metalness=0.2, roughness=0.5, opacity=1.0, transparent=False):
    """Converts a trimesh or Manifold to a compact serializable dict."""
    if hasattr(mesh, 'to_mesh'):
        m = mesh.to_mesh()
        verts = np.asarray(m.vert_properties[:, :3], dtype=np.float32)
        faces = np.asarray(m.tri_verts, dtype=np.int32)
    elif isinstance(mesh, trimesh.Trimesh):
        verts = np.asarray(mesh.vertices, dtype=np.float32)
        faces = np.asarray(mesh.faces, dtype=np.int32)
    else:
        raise ValueError(f"Unknown mesh type for {name}")

    verts_rounded = np.round(verts, 2).flatten().tolist()
    faces_flat = faces.flatten().tolist()

    return {
        'name': name,
        'vertices': verts_rounded,
        'faces': faces_flat,
        'color': color,
        'metalness': metalness,
        'roughness': roughness,
        'opacity': opacity,
        'transparent': transparent,
    }

def main():
    print("Building CAD solids for WebGL interactive application...")

    # 1. Enclosure Parts
    front_case = build_front_case()
    board_clamp = build_board_clamp()
    back_cover = build_back_cover()

    # Rear plate: flip 180 deg around X and translate to Z=42.0 so rim meets front case at Z=39.0
    rot_x_180 = trimesh.transformations.rotation_matrix(np.pi, [1, 0, 0])
    back_tri = trimesh.Trimesh(
        vertices=back_cover.to_mesh().vert_properties[:, :3],
        faces=back_cover.to_mesh().tri_verts,
        process=False
    )
    back_mated = back_tri.apply_transform(rot_x_180).apply_translation([0, 0, CASE_FRONT_D + BACK_PLATE_T])

    # Clamp: seated at Z=12.0 on top of module ears
    clamp_tri = trimesh.Trimesh(
        vertices=board_clamp.to_mesh().vert_properties[:, :3],
        faces=board_clamp.to_mesh().tri_verts,
        process=False
    )
    clamp_seated = clamp_tri.apply_translation([0, 0, 12.0])

    # 2. Detailed Electronic Board & Battery
    module_solid, glass, screen, casing, tf_card, esp32_shield, usbc_metal, brass_solid = build_jc3248w535_module()
    battery = build_battery()

    # Load high-resolution photo textures
    out_dir = r"d:\ESP32Radio\3D_Enclosure_JC3248W535"
    f_front = os.path.join(out_dir, "tex_front.jpg")
    f_rear = os.path.join(out_dir, "tex_rear.jpg")
    with open(f_front, "rb") as f:
        b64_front = base64.b64encode(f.read()).decode("utf-8")
    with open(f_rear, "rb") as f:
        b64_rear = base64.b64encode(f.read()).decode("utf-8")

    # 3. 2040 Loudspeaker
    speaker = build_2040_speaker()

    # 4. Fastener Sets
    screws_b2board = build_bracket_to_board_screws(seated_z=12.0)
    screws_b2body = build_bracket_to_body_screws(seated_z=12.0)
    corner_screws = build_corner_screws()

    print("Formatting meshes for Three.js...")

    parts = [
        {
            'id': 'front_case',
            'label': 'Deep Unibody Front Cabinet (39mm depth)',
            'group': 'Enclosure',
            'color': '#20242c',
            'isEnclosure': True,
            'description': 'Main unibody desktop radio chassis (39mm depth) housing the display screen, motherboard, mounting plate, speaker chamber, bottom MicroSD card access slot, and solid merged corner bosses (100% monolithic with outer/inner walls) with a 1.5mm recessed landing shelf (Z=37.5mm) providing exact clearance for the back door lip.',
            'submeshes': [
                mesh_to_dict(front_case, 'Front Case', '#20242c', metalness=0.15, roughness=0.65)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, -40.0],
        },
        {
            'id': 'board_module',
            'label': 'ESP32-S3 JC3248W535 Display Module',
            'group': 'Electronics',
            'color': '#2563eb',
            'description': 'Photo-realistic 3.5" IPS Capacitive Touch Display module textured with actual board photography: capacitive touch glass with black border, active IPS color display panel, rear PCB with real ICs, silkscreen traces, shiny metal ESP32-S3 RF shield, rear-facing vertical Type-C port, bottom connectors, and corner brass inserts.',
            'submeshes': [
                # Front Glass overlay with glossy reflection
                mesh_to_dict(glass, 'Front Glass (94.5x62mm)', '#38bdf8', metalness=0.1, roughness=0.06, opacity=0.35, transparent=True),
                # Photo-realistic Front Display Screen Plane (faces -Z towards front)
                {
                    'name': 'Active IPS Screen (Photo-Realistic)',
                    'isTexturedPlane': True,
                    'texType': 'front',
                    'width': 94.5,
                    'height': 62.0,
                    'pos': [BOARD_CENTER_X, BOARD_CENTER_Y, 0.62],
                    'rot': [0, np.pi, 0],
                    'metalness': 0.15,
                    'roughness': 0.25,
                },
                # Rear Module Housing
                mesh_to_dict(casing, 'Rear Module Housing', '#1e293b', metalness=0.2, roughness=0.55),
                # Photo-realistic Rear PCB Plane (faces +Z towards rear)
                {
                    'name': 'Rear PCB & Electronics (Photo-Realistic)',
                    'isTexturedPlane': True,
                    'texType': 'rear',
                    'width': 82.94,
                    'height': 58.36,
                    'pos': [BOARD_CENTER_X, BOARD_CENTER_Y, 12.02],
                    'rot': [0, 0, 0],
                    'metalness': 0.25,
                    'roughness': 0.45,
                },
                # 3D Physical Relief Components
                mesh_to_dict(esp32_shield, 'ESP32-S3 Metal RF Shield', '#cbd5e1', metalness=0.92, roughness=0.22),
                mesh_to_dict(usbc_metal, 'USB-C Metal Receptacle (Rear-Facing)', '#f1f5f9', metalness=0.95, roughness=0.15),
                mesh_to_dict(brass_solid, '4x Corner Brass Inserts', '#f59e0b', metalness=0.95, roughness=0.12),
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, 20.0],
        },
        {
            'id': 'tf_card',
            'label': 'MicroSD (TF) Memory Card',
            'group': 'Electronics',
            'color': '#dc2626',
            'description': 'Removable MicroSD memory card seated in the module bottom slot and accessible directly through the dedicated bottom opening in the enclosure wall for easy insertion/removal.',
            'submeshes': [
                mesh_to_dict(tf_card, 'MicroSD Card', '#dc2626', metalness=0.5, roughness=0.3)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, -28.0, 20.0],
        },
        {
            'id': 'board_clamp',
            'label': 'Heavy-Duty Board Mounting Clamp Plate',
            'group': 'Retention',
            'color': '#06b6d4',
            'description': '3.2mm thick pure solid polymer mounting plate with generous rear USB-C & JST 4P access window (22x32mm) leaving the rear Type-C port 100% uncovered with ample clearance for plugging in any standard USB-C cable, dedicated battery & switch window, cooling center window, extra-wide 72mm bottom opening for all IO ports, 4x Bracket-to-Board inner screw counterbores, and 4x Bracket-to-Body outer chassis screw counterbores.',
            'submeshes': [
                mesh_to_dict(clamp_seated, 'Board Mounting Plate', '#06b6d4', metalness=0.25, roughness=0.45)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, 50.0],
        },
        {
            'id': 'screws_b2board',
            'label': '4x Bracket-to-Board Screws',
            'group': 'Hardware',
            'color': '#38bdf8',
            'description': '4x Screws (M2.5/M3 x 8mm) passing through the plate inner holes to screw the mounting plate directly onto the 4 corner standoffs of the JC3248W535 board.',
            'submeshes': [
                mesh_to_dict(screws_b2board, 'Bracket-to-Board Screws', '#38bdf8', metalness=0.92, roughness=0.18)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, 65.0],
        },
        {
            'id': 'screws_b2body',
            'label': '4x Bracket-to-Body Screws',
            'group': 'Hardware',
            'color': '#f59e0b',
            'description': '4x Screws (M3 x 10mm) passing through the plate outer tabs to screw the assembled plate+board firmly into the front case chassis bosses.',
            'submeshes': [
                mesh_to_dict(screws_b2body, 'Bracket-to-Body Screws', '#f59e0b', metalness=0.92, roughness=0.18)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, 78.0],
        },
        {
            'id': 'battery',
            'label': '18650 Rechargeable Battery (Seated in Front Body)',
            'group': 'Power System',
            'color': '#10b981',
            'description': '3.7V 18650 Li-ion battery (or 3000mAh+ LiPo pouch) seated securely inside the molded retention cradle in the front body itself with dedicated wire routing notch leading to the top board battery header.',
            'submeshes': [
                mesh_to_dict(battery, '18650 Battery', '#10b981', metalness=0.4, roughness=0.35)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, 92.0],
        },
        {
            'id': 'speaker',
            'label': '2040 Cavity Loudspeaker',
            'group': 'Audio System',
            'color': '#475569',
            'description': '20x40x7.5mm enclosed cavity speaker seated in the front case acoustic chamber, radiating sound forward through the 7 front vertical acoustic slots.',
            'submeshes': [
                mesh_to_dict(speaker, '2040 Loudspeaker', '#475569', metalness=0.55, roughness=0.4)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [30.0, 0, 25.0],
        },
        {
            'id': 'back_cover',
            'label': 'Flat Rear Back Door Plate (3mm thickness)',
            'group': 'Enclosure',
            'color': '#18181b',
            'isEnclosure': True,
            'description': 'Slim 3.0mm flat screw-on rear cover door with rear-facing USB Type-C cable pass-through port (allowing direct plug-in to the board without removing the back cover), 1.5mm interlocking perimeter indexing lip nesting into the front case recessed shelf, 5 rear acoustic resonance slots, and 4 flush counterbored M3 corner screw wells.',
            'submeshes': [
                mesh_to_dict(back_mated, 'Back Cover Plate', '#18181b', metalness=0.15, roughness=0.65)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, 115.0],
        },
        {
            'id': 'corner_screws',
            'label': '4x M3 Corner Closure Screws',
            'group': 'Hardware',
            'color': '#94a3b8',
            'description': '4x M3 x 18mm socket head cap screws passing through rear counterbore wells to securely close the rear plate flush onto the front cabinet corner bosses.',
            'submeshes': [
                mesh_to_dict(corner_screws, 'Corner Screws', '#94a3b8', metalness=0.9, roughness=0.2)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, 140.0],
        },
    ]

    print(f"Packed {len(parts)} component assemblies.")

    html_content = generate_html_viewer(parts, b64_front, b64_rear)

    out_file = r'd:\ESP32Radio\3D_Enclosure_JC3248W535\interactive_assembly_viewer.html'
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write(html_content)

    print(f"Generated Interactive 3D Viewer: {out_file} ({len(html_content):,} bytes)")

def generate_html_viewer(parts_data, b64_front, b64_rear):
    json_data = json.dumps(parts_data)
    
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32-S3 JC3248W535 Radio — Interactive 3D CAD & X-Ray Viewer</title>
  <!-- Google Fonts -->
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=Outfit:wght@500;600;700;800&display=swap" rel="stylesheet">

  <style>
    :root {{
      --bg-gradient: radial-gradient(circle at 50% 30%, #1e293b 0%, #0f172a 60%, #020617 100%);
      --panel-bg: rgba(15, 23, 42, 0.82);
      --panel-border: rgba(255, 255, 255, 0.12);
      --accent: #38bdf8;
      --accent-glow: rgba(56, 189, 248, 0.35);
      --accent-hover: #0ea5e9;
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
      --card-bg: rgba(30, 41, 59, 0.65);
      --card-border: rgba(255, 255, 255, 0.08);
      --xray-color: #06b6d4;
    }}

    * {{
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      user-select: none;
    }}

    body {{
      font-family: 'Inter', sans-serif;
      background: #020617;
      background-image: var(--bg-gradient);
      color: var(--text-main);
      overflow: hidden;
      width: 100vw;
      height: 100vh;
    }}

    #canvas-container {{
      width: 100vw;
      height: 100vh;
      position: absolute;
      top: 0;
      left: 0;
      z-index: 1;
    }}

    /* Top Header */
    header {{
      position: absolute;
      top: 16px;
      left: 20px;
      right: 20px;
      z-index: 10;
      display: flex;
      justify-content: space-between;
      align-items: center;
      pointer-events: none;
    }}

    .title-badge {{
      background: var(--panel-bg);
      backdrop-filter: blur(14px);
      -webkit-backdrop-filter: blur(14px);
      border: 1px solid var(--panel-border);
      border-radius: 14px;
      padding: 12px 20px;
      pointer-events: auto;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.45);
    }}

    .title-badge h1 {{
      font-family: 'Outfit', sans-serif;
      font-size: 1.15rem;
      font-weight: 700;
      letter-spacing: -0.01em;
      color: #fff;
      display: flex;
      align-items: center;
      gap: 8px;
    }}

    .title-badge h1 span.tag {{
      font-size: 0.68rem;
      background: rgba(56, 189, 248, 0.18);
      color: var(--accent);
      border: 1px solid rgba(56, 189, 248, 0.3);
      padding: 2px 8px;
      border-radius: 6px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }}

    .title-badge p {{
      font-size: 0.78rem;
      color: var(--text-muted);
      margin-top: 2px;
    }}

    .header-actions {{
      display: flex;
      gap: 10px;
      pointer-events: auto;
    }}

    .btn {{
      background: var(--panel-bg);
      backdrop-filter: blur(14px);
      border: 1px solid var(--panel-border);
      color: var(--text-main);
      padding: 8px 14px;
      border-radius: 10px;
      font-size: 0.8rem;
      font-weight: 500;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 6px;
      transition: all 0.2s ease;
      box-shadow: 0 4px 15px rgba(0, 0, 0, 0.25);
    }}

    .btn:hover {{
      border-color: var(--accent);
      color: #fff;
      box-shadow: 0 0 15px var(--accent-glow);
      transform: translateY(-1px);
    }}

    .btn.active {{
      background: rgba(56, 189, 248, 0.25);
      border-color: var(--accent);
      color: #fff;
      box-shadow: 0 0 20px var(--accent-glow);
    }}

    .btn-xray.active {{
      background: rgba(6, 182, 212, 0.35);
      border-color: var(--xray-color);
      color: #fff;
      box-shadow: 0 0 20px rgba(6, 182, 212, 0.5);
    }}

    /* Bottom Explode Control Panel */
    .bottom-controls {{
      position: absolute;
      bottom: 20px;
      left: 50%;
      transform: translateX(-50%);
      z-index: 10;
      background: var(--panel-bg);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border: 1px solid var(--panel-border);
      border-radius: 16px;
      padding: 14px 24px;
      display: flex;
      align-items: center;
      gap: 20px;
      box-shadow: 0 12px 40px rgba(0, 0, 0, 0.5);
      width: min(94vw, 760px);
    }}

    .slider-group {{
      flex: 1;
      display: flex;
      flex-direction: column;
      gap: 6px;
    }}

    .slider-header {{
      display: flex;
      justify-content: space-between;
      font-size: 0.78rem;
      font-weight: 600;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }}

    .slider-header span.val {{
      color: var(--accent);
      font-family: 'Outfit', sans-serif;
      font-size: 0.88rem;
    }}

    input[type=range] {{
      -webkit-appearance: none;
      width: 100%;
      height: 6px;
      border-radius: 3px;
      background: rgba(255, 255, 255, 0.15);
      outline: none;
    }}

    input[type=range]::-webkit-slider-thumb {{
      -webkit-appearance: none;
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: var(--accent);
      cursor: pointer;
      box-shadow: 0 0 10px var(--accent-glow);
      transition: transform 0.1s ease;
    }}

    input[type=range]::-webkit-slider-thumb:hover {{
      transform: scale(1.2);
    }}

    .view-buttons {{
      display: flex;
      gap: 6px;
      border-left: 1px solid rgba(255, 255, 255, 0.1);
      padding-left: 14px;
    }}

    .btn-icon {{
      padding: 7px 10px;
      font-size: 0.75rem;
      border-radius: 8px;
    }}

    /* Left Sidebar: Components Tree */
    .sidebar-left {{
      position: absolute;
      top: 90px;
      left: 20px;
      bottom: 95px;
      width: 300px;
      background: var(--panel-bg);
      backdrop-filter: blur(14px);
      -webkit-backdrop-filter: blur(14px);
      border: 1px solid var(--panel-border);
      border-radius: 16px;
      padding: 16px;
      display: flex;
      flex-direction: column;
      gap: 12px;
      z-index: 10;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.4);
      overflow: hidden;
    }}

    .sidebar-header {{
      font-family: 'Outfit', sans-serif;
      font-size: 0.88rem;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      color: #fff;
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding-bottom: 8px;
      border-bottom: 1px solid var(--panel-border);
    }}

    .part-list {{
      flex: 1;
      overflow-y: auto;
      display: flex;
      flex-direction: column;
      gap: 6px;
      padding-right: 4px;
    }}

    .part-list::-webkit-scrollbar {{
      width: 4px;
    }}
    .part-list::-webkit-scrollbar-thumb {{
      background: rgba(255, 255, 255, 0.2);
      border-radius: 2px;
    }}

    .part-item {{
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 10px;
      padding: 9px 12px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      cursor: pointer;
      transition: all 0.2s ease;
    }}

    .part-item:hover {{
      border-color: rgba(56, 189, 248, 0.4);
      background: rgba(30, 41, 59, 0.85);
      transform: translateX(2px);
    }}

    .part-item.selected {{
      border-color: var(--accent);
      background: rgba(56, 189, 248, 0.15);
    }}

    .part-info {{
      display: flex;
      align-items: center;
      gap: 10px;
      min-width: 0;
    }}

    .part-color {{
      width: 12px;
      height: 12px;
      border-radius: 3px;
      flex-shrink: 0;
    }}

    .part-name {{
      font-size: 0.76rem;
      font-weight: 500;
      color: var(--text-main);
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }}

    .vis-toggle {{
      background: none;
      border: none;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 0.9rem;
      padding: 2px 4px;
      transition: color 0.2s;
    }}

    .vis-toggle:hover {{
      color: var(--accent);
    }}

    .vis-toggle.hidden {{
      opacity: 0.3;
    }}

    /* Right Sidebar: Component Inspector */
    .sidebar-right {{
      position: absolute;
      top: 90px;
      right: 20px;
      width: 320px;
      background: var(--panel-bg);
      backdrop-filter: blur(14px);
      -webkit-backdrop-filter: blur(14px);
      border: 1px solid var(--panel-border);
      border-radius: 16px;
      padding: 18px;
      display: flex;
      flex-direction: column;
      gap: 14px;
      z-index: 10;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.4);
      transition: transform 0.3s ease;
    }}

    .inspector-title {{
      font-family: 'Outfit', sans-serif;
      font-size: 1.05rem;
      font-weight: 700;
      color: #fff;
    }}

    .inspector-group {{
      font-size: 0.7rem;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      color: var(--accent);
      font-weight: 600;
    }}

    .inspector-desc {{
      font-size: 0.78rem;
      line-height: 1.45;
      color: var(--text-muted);
    }}

    .specs-grid {{
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 8px;
      margin-top: 4px;
    }}

    .spec-box {{
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 8px;
      padding: 8px 10px;
    }}

    .spec-label {{
      font-size: 0.68rem;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }}

    .spec-value {{
      font-family: 'Outfit', sans-serif;
      font-size: 0.86rem;
      font-weight: 600;
      color: #fff;
      margin-top: 2px;
    }}

    .feature-chips {{
      display: flex;
      flex-wrap: wrap;
      gap: 6px;
      margin-top: 4px;
    }}

    .chip {{
      font-size: 0.7rem;
      background: rgba(255, 255, 255, 0.07);
      border: 1px solid rgba(255, 255, 255, 0.1);
      padding: 3px 8px;
      border-radius: 6px;
      color: #cbd5e1;
    }}

    /* Loading Overlay */
    #loading {{
      position: absolute;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: #020617;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      z-index: 100;
      gap: 16px;
      transition: opacity 0.5s ease;
    }}

    .spinner {{
      width: 44px;
      height: 44px;
      border: 3px solid rgba(56, 189, 248, 0.2);
      border-top-color: var(--accent);
      border-radius: 50%;
      animation: spin 0.8s linear infinite;
    }}

    @keyframes spin {{
      to {{ transform: rotate(360deg); }}
    }}

    @media (max-width: 900px) {{
      .sidebar-left {{ width: 240px; }}
      .sidebar-right {{ display: none; }}
    }}
  </style>

  <!-- Three.js & OrbitControls -->
  <script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
</head>
<body>

  <div id="loading">
    <div class="spinner"></div>
    <div style="font-family:'Outfit';font-weight:600;font-size:1.1rem;color:#38bdf8;">Initializing 3D Assembly CAD & X-Ray Engine...</div>
    <div style="font-size:0.8rem;color:#94a3b8;">ESP32-S3 JC3248W535 Desktop Radio Enclosure</div>
  </div>

  <div id="canvas-container"></div>

  <!-- Header -->
  <header>
    <div class="title-badge">
      <h1>
        ESP32-S3 JC3248W535 Radio
        <span class="tag">Heavy-Duty Plate & Deep Battery Bay</span>
      </h1>
      <p>3.5" IPS Capacitive Touch &bull; 3.2mm Mounting Plate &bull; Dual M3 Fasteners &bull; Bottom SD Slot &bull; 24mm Deep Back Cover</p>
    </div>

    <div class="header-actions">
      <button class="btn btn-xray" id="btn-xray" title="Toggle X-Ray Transparency to see internal components">
        ⚡ X-Ray View
      </button>
      <button class="btn" id="btn-animate" title="Smoothly explode and collapse">
        &#9654; Auto Explode
      </button>
      <button class="btn" id="btn-turntable" title="Turntable camera rotation">
        &#8635; Turntable
      </button>
      <button class="btn" id="btn-reset" title="Reset camera to default view">
        &#10227; Reset View
      </button>
    </div>
  </header>

  <!-- Left Sidebar: Parts Tree -->
  <div class="sidebar-left">
    <div class="sidebar-header">
      <span>Assembly Parts</span>
      <button class="btn btn-icon" id="btn-toggle-all" style="padding:2px 8px;font-size:0.7rem;">Toggle All</button>
    </div>
    <div class="part-list" id="parts-container"></div>
  </div>

  <!-- Right Sidebar: Inspector -->
  <div class="sidebar-right" id="inspector">
    <div class="inspector-group" id="insp-group">Fastener System</div>
    <div class="inspector-title" id="insp-title">Select a Component</div>
    <div class="inspector-desc" id="insp-desc">Click any component in the 3D scene or part list to inspect dimensions and assembly details.</div>
    
    <div class="specs-grid" id="insp-specs">
      <div class="spec-box">
        <div class="spec-label">Mounting Type</div>
        <div class="spec-value" id="spec-mount">Internal Nest</div>
      </div>
      <div class="spec-box">
        <div class="spec-label">Fastener System</div>
        <div class="spec-value" id="spec-fastener">Dual-Stage M3</div>
      </div>
      <div class="spec-box">
        <div class="spec-label">Material</div>
        <div class="spec-value" id="spec-mat">PLA / PETG / ABS</div>
      </div>
      <div class="spec-box">
        <div class="spec-label">Status</div>
        <div class="spec-value" id="spec-status">Watertight 3D</div>
      </div>
    </div>

    <div class="feature-chips" id="insp-chips">
      <span class="chip">Bracket-to-Board Screws</span>
      <span class="chip">Bracket-to-Body Screws</span>
      <span class="chip">Bottom TF Slot</span>
      <span class="chip">18650 Battery Cradle</span>
    </div>
  </div>

  <!-- Bottom Explode Controls -->
  <div class="bottom-controls">
    <div class="slider-group">
      <div class="slider-header">
        <span>Explode Assembly</span>
        <span class="val" id="explode-val">0%</span>
      </div>
      <input type="range" id="explode-slider" min="0" max="100" value="0" step="0.5">
    </div>

    <div class="view-buttons">
      <button class="btn btn-icon" id="btn-view-front" title="Front Aperture View">Front</button>
      <button class="btn btn-icon" id="btn-view-iso" title="Isometric 3D Hero View">Iso</button>
      <button class="btn btn-icon" id="btn-view-rear" title="Rear Cover & Battery View">Rear</button>
      <button class="btn btn-icon" id="btn-view-bottom" title="Bottom MicroSD Slot View">Bottom</button>
      <button class="btn btn-icon" id="btn-view-right" title="Right USB-C View">Right</button>
    </div>
  </div>

  <script>
    // Embedded Geometry & Photo Texture Payload
    const PARTS_DATA = {json_data};
    const FRONT_TEX_URI = "data:image/jpeg;base64,{b64_front}";
    const REAR_TEX_URI = "data:image/jpeg;base64,{b64_rear}";

    let scene, camera, renderer, controls;
    let frontTex, rearTex;
    let partGroups = [];
    let isTurntable = false;
    let isAutoExplode = false;
    let isXRay = false;
    let explodeProgress = 0.0;
    let explodeDirection = 1;
    let selectedPartId = null;

    function init() {{
      const container = document.getElementById('canvas-container');

      // Texture Loaders for Photo-Realistic Board Appearance
      const texLoader = new THREE.TextureLoader();
      frontTex = texLoader.load(FRONT_TEX_URI);
      frontTex.colorSpace = THREE.SRGBColorSpace;
      rearTex = texLoader.load(REAR_TEX_URI);
      rearTex.colorSpace = THREE.SRGBColorSpace;

      // Scene
      scene = new THREE.Scene();
      scene.background = new THREE.Color(0x020617);
      scene.fog = new THREE.FogExp2(0x020617, 0.0018);

      // Camera
      camera = new THREE.PerspectiveCamera(40, window.innerWidth / window.innerHeight, 1, 1000);
      camera.position.set(130, -110, 160);

      // Renderer
      renderer = new THREE.WebGLRenderer({{ antialias: true, alpha: true, powerPreference: 'high-performance' }});
      renderer.setSize(window.innerWidth, window.innerHeight);
      renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
      renderer.shadowMap.enabled = true;
      renderer.shadowMap.type = THREE.PCFSoftShadowMap;
      renderer.toneMapping = THREE.ACESFilmicToneMapping;
      renderer.toneMappingExposure = 1.15;
      container.appendChild(renderer.domElement);

      // Controls
      controls = new THREE.OrbitControls(camera, renderer.domElement);
      controls.enableDamping = true;
      controls.dampingFactor = 0.06;
      controls.maxDistance = 600;
      controls.minDistance = 20;
      controls.target.set(0, 0, 16);

      // Lighting Setup
      setupLighting();

      // Load Meshes
      buildSceneParts();

      // UI Bindings
      setupUI();

      // Hide loading screen
      setTimeout(() => {{
        const loader = document.getElementById('loading');
        loader.style.opacity = '0';
        setTimeout(() => loader.style.display = 'none', 500);
      }}, 300);

      window.addEventListener('resize', onWindowResize);
      animate();
    }}

    function setupLighting() {{
      const ambient = new THREE.AmbientLight(0xffffff, 0.85);
      scene.add(ambient);

      const keyLight = new THREE.DirectionalLight(0xffffff, 1.25);
      keyLight.position.set(120, 150, 200);
      keyLight.castShadow = true;
      keyLight.shadow.mapSize.width = 2048;
      keyLight.shadow.mapSize.height = 2048;
      keyLight.shadow.camera.near = 10;
      keyLight.shadow.camera.far = 600;
      const d = 120;
      keyLight.shadow.camera.left = -d;
      keyLight.shadow.camera.right = d;
      keyLight.shadow.camera.top = d;
      keyLight.shadow.camera.bottom = -d;
      keyLight.shadow.bias = -0.0005;
      scene.add(keyLight);

      const fillLight = new THREE.DirectionalLight(0x38bdf8, 0.45);
      fillLight.position.set(-150, -80, -100);
      scene.add(fillLight);

      const rimLight = new THREE.DirectionalLight(0xffffff, 0.6);
      rimLight.position.set(0, -180, 120);
      scene.add(rimLight);

      // Shadow receiver grid floor
      const grid = new THREE.GridHelper(300, 30, 0x1e293b, 0x0f172a);
      grid.rotation.x = Math.PI / 2;
      grid.position.z = -38;
      scene.add(grid);
    }}

    function buildSceneParts() {{
      const partsContainer = document.getElementById('parts-container');

      PARTS_DATA.forEach((part, index) => {{
        const group = new THREE.Group();
        group.name = part.id;
        group.userData = {{
          id: part.id,
          label: part.label,
          group: part.group,
          desc: part.description,
          color: part.color,
          isEnclosure: !!part.isEnclosure,
          mated_pos: new THREE.Vector3(...part.mated_pos),
          explode_vec: new THREE.Vector3(...part.explode_vec),
        }};

        part.submeshes.forEach(sub => {{
          if (sub.isTexturedPlane) {{
            const planeGeom = new THREE.PlaneGeometry(sub.width, sub.height);
            const tex = (sub.texType === 'front') ? frontTex : rearTex;
            const planeMat = new THREE.MeshStandardMaterial({{
              map: tex,
              metalness: sub.metalness !== undefined ? sub.metalness : 0.15,
              roughness: sub.roughness !== undefined ? sub.roughness : 0.35,
              side: THREE.DoubleSide
            }});
            planeMat.userData = {{
              origColor: '#ffffff',
              origOpacity: 1.0,
              origTransparent: false,
              isTextured: true
            }};
            const pMesh = new THREE.Mesh(planeGeom, planeMat);
            pMesh.position.set(...sub.pos);
            if (sub.rot) {{
              pMesh.rotation.set(...sub.rot);
            }}
            pMesh.castShadow = true;
            pMesh.receiveShadow = true;
            pMesh.name = sub.name;
            group.add(pMesh);
            return;
          }}

          const geom = new THREE.BufferGeometry();
          geom.setAttribute('position', new THREE.Float32BufferAttribute(sub.vertices, 3));
          geom.setIndex(sub.faces);
          geom.computeVertexNormals();

          let mat;
          if (sub.transparent) {{
            mat = new THREE.MeshPhysicalMaterial({{
              color: new THREE.Color(sub.color),
              metalness: sub.metalness || 0.1,
              roughness: sub.roughness || 0.06,
              transmission: 0.82,
              opacity: sub.opacity || 0.35,
              transparent: true,
              ior: 1.52,
              clearcoat: 1.0,
            }});
          }} else {{
            mat = new THREE.MeshStandardMaterial({{
              color: new THREE.Color(sub.color),
              metalness: sub.metalness !== undefined ? sub.metalness : 0.2,
              roughness: sub.roughness !== undefined ? sub.roughness : 0.5,
            }});
          }}

          mat.userData = {{
            origColor: sub.color,
            origOpacity: sub.opacity || 1.0,
            origTransparent: !!sub.transparent,
            isTextured: false
          }};

          const mesh = new THREE.Mesh(geom, mat);
          mesh.castShadow = true;
          mesh.receiveShadow = true;
          mesh.name = sub.name;
          group.add(mesh);
        }});

        scene.add(group);
        partGroups.push(group);

        // Sidebar item
        const item = document.createElement('div');
        item.className = 'part-item';
        item.dataset.id = part.id;
        item.innerHTML = `
          <div class="part-info">
            <div class="part-color" style="background: ${{part.color}}"></div>
            <div class="part-name">${{part.label}}</div>
          </div>
          <button class="vis-toggle" title="Toggle visibility">&#128065;</button>
        `;

        item.addEventListener('click', (e) => {{
          if (e.target.classList.contains('vis-toggle')) return;
          selectPart(part.id);
        }});

        const visBtn = item.querySelector('.vis-toggle');
        visBtn.addEventListener('click', (e) => {{
          e.stopPropagation();
          group.visible = !group.visible;
          visBtn.classList.toggle('hidden', !group.visible);
          visBtn.innerHTML = group.visible ? '&#128065;' : '&#8212;';
        }});

        partsContainer.appendChild(item);
      }});

      if (PARTS_DATA.length > 0) selectPart('board_clamp');
    }}

    function setXRayMode(enable) {{
      isXRay = enable;
      const btn = document.getElementById('btn-xray');
      btn.classList.toggle('active', isXRay);

      partGroups.forEach(group => {{
        const isEnc = group.userData.isEnclosure;
        group.children.forEach(mesh => {{
          if (isEnc) {{
            if (isXRay) {{
              mesh.material.transparent = true;
              mesh.material.opacity = 0.22;
              mesh.material.depthWrite = false;
              mesh.material.color.set(0x38bdf8);
              mesh.material.wireframe = false;
            }} else {{
              mesh.material.transparent = mesh.material.userData.origTransparent;
              mesh.material.opacity = mesh.material.userData.origOpacity;
              mesh.material.depthWrite = true;
              mesh.material.color.set(mesh.material.userData.origColor);
              mesh.material.wireframe = false;
            }}
          }} else {{
            if (mesh.material.userData && mesh.material.userData.isTextured) {{
              mesh.material.emissive = isXRay ? new THREE.Color(0x222222) : new THREE.Color(0x000000);
            }} else if (mesh.material.color) {{
              if (isXRay) {{
                mesh.material.emissive = new THREE.Color(mesh.material.color).multiplyScalar(0.15);
              }} else {{
                mesh.material.emissive = new THREE.Color(0x000000);
              }}
            }}
          }}
          mesh.material.needsUpdate = true;
        }});
      }});
    }}

    function selectPart(partId) {{
      selectedPartId = partId;
      document.querySelectorAll('.part-item').forEach(el => {{
        el.classList.toggle('selected', el.dataset.id === partId);
      }});

      const partData = PARTS_DATA.find(p => p.id === partId);
      if (!partData) return;

      document.getElementById('insp-group').innerText = partData.group;
      document.getElementById('insp-title').innerText = partData.label;
      document.getElementById('insp-desc').innerText = partData.description;

      const chips = document.getElementById('insp-chips');
      if (partId === 'screws_b2board') {{
        chips.innerHTML = `
          <span class="chip" style="color:#38bdf8;">Stage 1 Fastening</span>
          <span class="chip" style="color:#38bdf8;">Fastens Bracket to Board</span>
          <span class="chip" style="color:#38bdf8;">4x M2.5 / M3 Screws</span>
          <span class="chip" style="color:#38bdf8;">X = ±42.15, Y = ±26.15</span>
        `;
        document.getElementById('spec-mount').innerText = "Module Standoffs";
        document.getElementById('spec-fastener').innerText = "4x M3 x 8mm";
      }} else if (partId === 'screws_b2body') {{
        chips.innerHTML = `
          <span class="chip" style="color:#f59e0b;">Stage 2 Fastening</span>
          <span class="chip" style="color:#f59e0b;">Fastens Bracket to Body</span>
          <span class="chip" style="color:#f59e0b;">4x Chassis Bosses</span>
          <span class="chip" style="color:#f59e0b;">X = -67/+33, Y = ±27.5</span>
        `;
        document.getElementById('spec-mount').innerText = "Chassis Bosses";
        document.getElementById('spec-fastener').innerText = "4x M3 x 10mm";
      }} else if (partId === 'board_clamp') {{
        chips.innerHTML = `
          <span class="chip" style="color:#06b6d4;">Dual-Stage Retention</span>
          <span class="chip" style="color:#06b6d4;">4 Inner Screw Holes</span>
          <span class="chip" style="color:#06b6d4;">4 Outer Chassis Tabs</span>
          <span class="chip" style="color:#06b6d4;">Port Relief Cutouts</span>
        `;
        document.getElementById('spec-mount').innerText = "Inner Board + Body";
        document.getElementById('spec-fastener').innerText = "Dual 4x Screws";
      }} else if (partId === 'battery') {{
        chips.innerHTML = `
          <span class="chip" style="color:#10b981;">18650 / LiPo Pouch</span>
          <span class="chip" style="color:#10b981;">72 x 24 mm Cradle</span>
          <span class="chip" style="color:#10b981;">Floor Retention Walls</span>
          <span class="chip" style="color:#10b981;">Wire Routing Notch</span>
        `;
        document.getElementById('spec-mount').innerText = "Rear Molded Cradle";
        document.getElementById('spec-fastener').innerText = "Snap Friction Fit";
      }} else if (partId === 'tf_card') {{
        chips.innerHTML = `
          <span class="chip" style="color:#ef4444;">Bottom Wall Slot</span>
          <span class="chip" style="color:#ef4444;">14.0 x 3.2 mm</span>
          <span class="chip" style="color:#ef4444;">Push-Push Ejection</span>
          <span class="chip" style="color:#ef4444;">Direct Finger Access</span>
        `;
        document.getElementById('spec-mount').innerText = "Module Bottom Socket";
        document.getElementById('spec-fastener').innerText = "Spring Latch";
      }} else {{
        chips.innerHTML = `
          <span class="chip">Zero Front Screws</span>
          <span class="chip">Internal Nest</span>
          <span class="chip">2040 Loudspeaker</span>
          <span class="chip">Interlocking Lip</span>
        `;
        document.getElementById('spec-mount').innerText = "Desktop Enclosure";
        document.getElementById('spec-fastener').innerText = "M3 Screws";
      }}
    }}

    function updateExplode(progress) {{
      explodeProgress = progress;
      document.getElementById('explode-slider').value = progress * 100;
      document.getElementById('explode-val').innerText = Math.round(progress * 100) + '%';

      partGroups.forEach(group => {{
        const mated = group.userData.mated_pos;
        const vec = group.userData.explode_vec;
        group.position.x = mated.x + vec.x * progress;
        group.position.y = mated.y + vec.y * progress;
        group.position.z = mated.z + vec.z * progress;
      }});
    }}

    function setupUI() {{
      const slider = document.getElementById('explode-slider');
      slider.addEventListener('input', (e) => {{
        isAutoExplode = false;
        document.getElementById('btn-animate').classList.remove('active');
        updateExplode(parseFloat(e.target.value) / 100.0);
      }});

      // X-Ray View Button
      document.getElementById('btn-xray').addEventListener('click', () => {{
        setXRayMode(!isXRay);
      }});

      // Auto Explode Animation Button
      const btnAnimate = document.getElementById('btn-animate');
      btnAnimate.addEventListener('click', () => {{
        isAutoExplode = !isAutoExplode;
        btnAnimate.classList.toggle('active', isAutoExplode);
        btnAnimate.innerHTML = isAutoExplode ? '&#10074;&#10074; Pause' : '&#9654; Auto Explode';
      }});

      // Turntable Rotation Button
      const btnTurntable = document.getElementById('btn-turntable');
      btnTurntable.addEventListener('click', () => {{
        isTurntable = !isTurntable;
        btnTurntable.classList.toggle('active', isTurntable);
      }});

      // Reset View Button
      document.getElementById('btn-reset').addEventListener('click', () => {{
        setCameraView(130, -110, 160, 0, 0, 16);
      }});

      // View Presets
      document.getElementById('btn-view-front').addEventListener('click', () => {{
        setCameraView(0, -220, 16, 0, 0, 16);
      }});

      document.getElementById('btn-view-iso').addEventListener('click', () => {{
        setCameraView(130, -110, 160, 0, 0, 16);
      }});

      document.getElementById('btn-view-rear').addEventListener('click', () => {{
        setCameraView(0, 220, 16, 0, 0, 16);
      }});

      document.getElementById('btn-view-bottom').addEventListener('click', () => {{
        setCameraView(0, -60, -200, 0, -25, 10);
      }});

      document.getElementById('btn-view-right').addEventListener('click', () => {{
        setCameraView(220, 0, 16, 0, 0, 16);
      }});

      // Toggle All Parts
      let allVisible = true;
      document.getElementById('btn-toggle-all').addEventListener('click', () => {{
        allVisible = !allVisible;
        partGroups.forEach(g => g.visible = allVisible);
        document.querySelectorAll('.vis-toggle').forEach(btn => {{
          btn.classList.toggle('hidden', !allVisible);
          btn.innerHTML = allVisible ? '&#128065;' : '&#8212;';
        }});
      }});
    }}

    function setCameraView(cx, cy, cz, tx, ty, tz) {{
      const startPos = camera.position.clone();
      const endPos = new THREE.Vector3(cx, cy, cz);
      const startTarget = controls.target.clone();
      const endTarget = new THREE.Vector3(tx, ty, tz);

      let t = 0;
      function stepCamera() {{
        t += 0.05;
        camera.position.lerpVectors(startPos, endPos, easeInOutCubic(Math.min(t, 1.0)));
        controls.target.lerpVectors(startTarget, endTarget, easeInOutCubic(Math.min(t, 1.0)));
        controls.update();
        if (t < 1.0) requestAnimationFrame(stepCamera);
      }}
      stepCamera();
    }}

    function easeInOutCubic(x) {{
      return x < 0.5 ? 4 * x * x * x : 1 - Math.pow(-2 * x + 2, 3) / 2;
    }}

    function onWindowResize() {{
      camera.aspect = window.innerWidth / window.innerHeight;
      camera.updateProjectionMatrix();
      renderer.setSize(window.innerWidth, window.innerHeight);
    }}

    function animate() {{
      requestAnimationFrame(animate);

      if (isTurntable) {{
        controls.autoRotate = true;
        controls.autoRotateSpeed = 2.0;
      }} else {{
        controls.autoRotate = false;
      }}

      if (isAutoExplode) {{
        explodeProgress += 0.007 * explodeDirection;
        if (explodeProgress >= 1.0) {{
          explodeProgress = 1.0;
          explodeDirection = -1;
        }} else if (explodeProgress <= 0.0) {{
          explodeProgress = 0.0;
          explodeDirection = 1;
        }}
        updateExplode(explodeProgress);
      }}

      controls.update();
      renderer.render(scene, camera);
    }}

    window.onload = init;
  </script>
</body>
</html>"""

if __name__ == "__main__":
    main()
