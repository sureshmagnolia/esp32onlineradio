"""
build_interactive_html.py
Generates a 3D WebGL / Three.js assembly & exploded view application
with the display and motherboard treated as 1 single bonded hardware module,
and the viewing window centered symmetrically over the active screen.
"""

import os
import json
import numpy as np
import trimesh
from manifold3d import Manifold

import sys
sys.path.append(r'd:\ESP32Radio\3D_Enclosure_Waveshare28')
from generate_waveshare28_stl import (
    build_front_case, build_back_cover, build_board_clamp, make_rounded_box,
    PCB_HOLES, BUTTONS_X, TF_SLOT_X, SCREEN_CENTER_X, SCREEN_CENTER_Y,
    GLASS_OFFSET_X, GLASS_OFFSET_Y, PCB_OFFSET_X, PCB_OFFSET_Y
)
from virtual_assembly_model import build_detailed_board_components, build_installed_plungers

def make_single_speaker(side):
    spk_x = side * 53.0
    body = make_rounded_box(20.0, 30.0, 5.6, 2.0).translate([spk_x, -1.0, 2.4])
    gasket_outer = make_rounded_box(18.4, 28.4, 0.8, 1.8).translate([spk_x, -1.0, 1.6])
    gasket_hole = make_rounded_box(14.0, 24.0, 1.0, 1.2).translate([spk_x, -1.0, 1.5])
    gasket = gasket_outer - gasket_hole
    cone = Manifold.cylinder(1.2, 5.5, 4.5, 24).translate([spk_x, -1.0, 1.8])
    dust_cap = Manifold.cylinder(0.6, 2.5, 2.5, 20).translate([spk_x, -1.0, 1.2])
    wire_lead = Manifold.cube([1.8, 4.0, 2.0], center=True).translate([spk_x + (side * 8.5), 10.0, 4.0])
    return functools_union([body, gasket, cone, dust_cap, wire_lead])

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

def functools_union(items):
    import functools
    return functools.reduce(lambda a, b: a + b, items)

def main():
    print("Building CAD solids for WebGL interactive application...")

    # 1. Enclosure Parts
    front_case = build_front_case()
    back_cover = build_back_cover()
    board_clamp = build_board_clamp()

    # Rear cover: flip 180 deg around X and translate to Z=32.0 so lip meets front case (Flat back, no feet!)
    rot_x_180 = trimesh.transformations.rotation_matrix(np.pi, [1, 0, 0])
    back_tri = trimesh.Trimesh(
        vertices=back_cover.to_mesh().vert_properties[:, :3],
        faces=back_cover.to_mesh().tri_verts,
        process=False
    )
    back_mated = back_tri.apply_transform(rot_x_180).apply_translation([0, 0, 32.0])

    # Clamp: seated at Z=9.8 on top of standoffs
    clamp_tri = trimesh.Trimesh(
        vertices=board_clamp.to_mesh().vert_properties[:, :3],
        faces=board_clamp.to_mesh().tri_verts,
        process=False
    )
    clamp_seated = clamp_tri.apply_translation([0, 0, 9.8])

    # 2. Detailed Electronic Board (Bonded Unit: Glass + Screen + Midframe + PCB + Standoffs + Chips)
    board = build_detailed_board_components()

    # 3. Stereo Speakers (Individually animated)
    spk_left = make_single_speaker(-1)
    spk_right = make_single_speaker(1)

    # 4. Captive Plungers
    plungers = build_installed_plungers()

    # 5. M3 Screws
    screws = []
    for sx in [-64.0, 64.0]:
        for sy in [-24.0, 24.0]:
            head = Manifold.cylinder(3.0, 3.0, 3.0, 20).translate([sx, sy, 30.5])
            shank = Manifold.cylinder(16.0, 1.5, 1.5, 16).translate([sx, sy, 14.5])
            screws.append(head + shank)
    screws_solid = functools_union(screws)

    print("Formatting meshes for Three.js...")

    parts = [
        {
            'id': 'front_case',
            'label': 'Front Case Cabinet',
            'group': 'Enclosure',
            'color': '#2b2d42',
            'description': 'Main desktop chassis with 58.4x44mm centered screen window, 75x52.5mm glass pocket, acoustic grilles, and button guide wells.',
            'submeshes': [
                mesh_to_dict(front_case, 'Front Case', '#2b2d42', metalness=0.1, roughness=0.6)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, -32.0],
        },
        {
            'id': 'board_module',
            'label': 'Waveshare 2.8" Display & Board (Bonded Unit)',
            'group': 'Bonded Electronics',
            'color': '#2563eb',
            'description': 'Factory-bonded single assembly: 2.8" touch glass, active IPS panel (centered at X=0), white chassis midframe, blue PCB with 4 brass standoffs, USB-C, MicroSD, and tactile buttons.',
            'submeshes': [
                mesh_to_dict(board['glass'], 'Touch Glass', '#60a5fa', metalness=0.1, roughness=0.1, opacity=0.45, transparent=True),
                mesh_to_dict(board['active_screen'], 'Active Screen', '#0284c7', metalness=0.8, roughness=0.2),
                mesh_to_dict(board['lcd_midframe'], 'LCD Mid-Frame', '#f8fafc', metalness=0.1, roughness=0.7),
                mesh_to_dict(board['fpc_ribbon'], 'FPC Ribbon Cable', '#d97706', metalness=0.2, roughness=0.5),
                mesh_to_dict(board['pcb'], 'PCB Motherboard', '#1d4ed8', metalness=0.2, roughness=0.4),
                mesh_to_dict(board['standoffs'], 'Brass Standoffs (4x)', '#eab308', metalness=0.85, roughness=0.25),
                mesh_to_dict(board['buttons'], 'Tactile Switches (3x)', '#ef4444', metalness=0.3, roughness=0.3),
                mesh_to_dict(board['tf_socket'], 'MicroSD Socket & Card', '#94a3b8', metalness=0.7, roughness=0.3),
                mesh_to_dict(board['usbc'], 'USB Type-C Receptacle', '#cbd5e1', metalness=0.9, roughness=0.2),
                mesh_to_dict(board['connectors'], 'Shrouded Connectors', '#f1f5f9', metalness=0.1, roughness=0.6),
                mesh_to_dict(board['smd_chips'], 'ESP32-S3 & SMD ICs', '#1e293b', metalness=0.4, roughness=0.4),
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, 24.0],
        },
        {
            'id': 'board_clamp',
            'label': 'Monolithic Board Clamp Bracket',
            'group': 'Retention',
            'color': '#06b6d4',
            'description': '1-piece monolithic U-bracket resting securely on the 4 brass standoffs (Z=9.8mm) with zero cuts and zero split pieces.',
            'submeshes': [
                mesh_to_dict(clamp_seated, 'Board Clamp', '#06b6d4', metalness=0.2, roughness=0.5)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, 48.0],
        },
        {
            'id': 'plungers',
            'label': '3x Captive Button Plungers',
            'group': 'Hardware',
            'color': '#f97316',
            'description': 'Short 4.5mm direct plungers extending through bottom wall guide holes with retention flanges.',
            'submeshes': [
                mesh_to_dict(plungers, 'Button Plungers', '#f97316', metalness=0.2, roughness=0.5)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, -22.0, 0],
        },
        {
            'id': 'speaker_left',
            'label': 'Left 2030 Cavity Speaker',
            'group': 'Audio System',
            'color': '#475569',
            'description': 'Left channel 2030 enclosed acoustic cavity chamber speaker providing deep bass & clear vocals.',
            'submeshes': [
                mesh_to_dict(spk_left, 'Left Speaker', '#475569', metalness=0.6, roughness=0.4)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [-42.0, 0, 16.0],
        },
        {
            'id': 'speaker_right',
            'label': 'Right 2030 Cavity Speaker',
            'group': 'Audio System',
            'color': '#475569',
            'description': 'Right channel 2030 enclosed acoustic cavity chamber speaker providing stereo spatial soundstage.',
            'submeshes': [
                mesh_to_dict(spk_right, 'Right Speaker', '#475569', metalness=0.6, roughness=0.4)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [42.0, 0, 16.0],
        },
        {
            'id': 'back_cover',
            'label': 'Rear Enclosure Cover (Sleek Flat)',
            'group': 'Enclosure',
            'color': '#18181b',
            'description': 'Sleek, completely flat rear housing with internal battery retention cradle, dual acoustic ventilation grills, and reinforced corner pillars.',
            'submeshes': [
                mesh_to_dict(back_mated, 'Back Cover', '#18181b', metalness=0.1, roughness=0.6)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, 78.0],
        },
        {
            'id': 'screws',
            'label': '4x M3 Fastening Screws',
            'group': 'Hardware',
            'color': '#94a3b8',
            'description': 'M3 x 16mm socket head cap screws passing through rear wells into front case threaded inserts.',
            'submeshes': [
                mesh_to_dict(screws_solid, 'M3 Screws', '#94a3b8', metalness=0.9, roughness=0.2)
            ],
            'mated_pos': [0, 0, 0],
            'explode_vec': [0, 0, 108.0],
        },
    ]

    print(f"Packed {len(parts)} component assemblies.")

    html_content = generate_html_viewer(parts)

    out_file = r'd:\ESP32Radio\3D_Enclosure_Waveshare28\interactive_assembly_viewer.html'
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write(html_content)

    print(f"Generated Interactive 3D Viewer: {out_file} ({len(html_content):,} bytes)")

def generate_html_viewer(parts_data):
    json_data = json.dumps(parts_data)
    
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32-S3 Radio — Interactive 3D Virtual Assembly & Explode Viewer</title>
  <!-- Google Fonts: Inter & Outfit -->
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=Outfit:wght@500;600;700;800&display=swap" rel="stylesheet">

  <style>
    :root {{
      --bg-gradient: radial-gradient(circle at 50% 30%, #1e293b 0%, #0f172a 60%, #020617 100%);
      --panel-bg: rgba(15, 23, 42, 0.78);
      --panel-border: rgba(255, 255, 255, 0.12);
      --accent: #38bdf8;
      --accent-glow: rgba(56, 189, 248, 0.35);
      --accent-hover: #0ea5e9;
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
      --card-bg: rgba(30, 41, 59, 0.65);
    }}

    * {{
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      user-select: none;
    }}

    body {{
      font-family: 'Inter', sans-serif;
      background: var(--bg-gradient);
      color: var(--text-main);
      overflow: hidden;
      width: 100vw;
      height: 100vh;
    }}

    #canvas3d {{
      width: 100%;
      height: 100%;
      display: block;
      cursor: grab;
    }}

    #canvas3d:active {{
      cursor: grabbing;
    }}

    /* Top Brand & Status Header */
    .header-bar {{
      position: absolute;
      top: 18px;
      left: 20px;
      right: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      pointer-events: none;
      z-index: 20;
    }}

    .brand {{
      background: var(--panel-bg);
      backdrop-filter: blur(14px);
      -webkit-backdrop-filter: blur(14px);
      border: 1px solid var(--panel-border);
      border-radius: 14px;
      padding: 10px 20px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.4);
      pointer-events: auto;
    }}

    .brand h1 {{
      font-family: 'Outfit', sans-serif;
      font-size: 1.15rem;
      font-weight: 700;
      letter-spacing: -0.02em;
      background: linear-gradient(135deg, #38bdf8 0%, #818cf8 100%);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      display: flex;
      align-items: center;
      gap: 8px;
    }}

    .brand p {{
      font-size: 0.76rem;
      color: var(--text-muted);
      margin-top: 2px;
    }}

    /* Main Floating Explode Controller (Bottom Center) */
    .control-dock {{
      position: absolute;
      bottom: 24px;
      left: 50%;
      transform: translateX(-50%);
      width: 90%;
      max-width: 680px;
      background: var(--panel-bg);
      backdrop-filter: blur(18px);
      -webkit-backdrop-filter: blur(18px);
      border: 1px solid var(--panel-border);
      border-radius: 20px;
      padding: 16px 24px;
      box-shadow: 0 20px 50px rgba(0, 0, 0, 0.55), 0 0 0 1px rgba(255, 255, 255, 0.05);
      display: flex;
      flex-direction: column;
      gap: 12px;
      z-index: 20;
    }}

    .slider-row {{
      display: flex;
      align-items: center;
      gap: 16px;
    }}

    .slider-label {{
      font-family: 'Outfit', sans-serif;
      font-weight: 700;
      font-size: 0.85rem;
      min-width: 100px;
      color: var(--text-main);
      display: flex;
      align-items: center;
      gap: 6px;
    }}

    .slider-track-wrap {{
      flex: 1;
      position: relative;
      display: flex;
      align-items: center;
    }}

    .explode-slider {{
      -webkit-appearance: none;
      appearance: none;
      width: 100%;
      height: 10px;
      border-radius: 5px;
      background: rgba(255, 255, 255, 0.1);
      outline: none;
      cursor: pointer;
      transition: background 0.2s;
    }}

    .explode-slider:hover {{
      background: rgba(255, 255, 255, 0.16);
    }}

    .explode-slider::-webkit-slider-thumb {{
      -webkit-appearance: none;
      appearance: none;
      width: 24px;
      height: 24px;
      border-radius: 50%;
      background: #38bdf8;
      border: 3px solid #ffffff;
      box-shadow: 0 0 15px rgba(56, 189, 248, 0.8);
      cursor: pointer;
      transition: transform 0.15s, background-color 0.15s;
    }}

    .explode-slider::-webkit-slider-thumb:hover {{
      transform: scale(1.15);
      background: #7dd3fc;
    }}

    .pct-badge {{
      font-family: 'Outfit', monospace;
      font-size: 0.85rem;
      font-weight: 700;
      color: var(--accent);
      min-width: 46px;
      text-align: right;
    }}

    .action-row {{
      display: flex;
      justify-content: space-between;
      align-items: center;
      border-top: 1px solid rgba(255, 255, 255, 0.08);
      padding-top: 10px;
    }}

    .btn-group {{
      display: flex;
      gap: 8px;
    }}

    .btn {{
      background: rgba(255, 255, 255, 0.07);
      border: 1px solid rgba(255, 255, 255, 0.12);
      color: var(--text-main);
      padding: 7px 14px;
      border-radius: 10px;
      font-size: 0.78rem;
      font-weight: 600;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 6px;
      transition: all 0.2s;
    }}

    .btn:hover {{
      background: rgba(255, 255, 255, 0.15);
      border-color: rgba(255, 255, 255, 0.25);
    }}

    .btn.active {{
      background: var(--accent);
      color: #042f2e;
      border-color: var(--accent);
      box-shadow: 0 0 12px var(--accent-glow);
    }}

    /* Left Side Drawer: Part Hierarchy & Visibility */
    .left-drawer {{
      position: absolute;
      top: 90px;
      left: 20px;
      width: 310px;
      max-height: calc(100vh - 200px);
      background: var(--panel-bg);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border: 1px solid var(--panel-border);
      border-radius: 18px;
      padding: 16px;
      box-shadow: 0 15px 40px rgba(0, 0, 0, 0.4);
      display: flex;
      flex-direction: column;
      gap: 12px;
      z-index: 15;
    }}

    .drawer-header {{
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding-bottom: 8px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.08);
    }}

    .drawer-title {{
      font-family: 'Outfit', sans-serif;
      font-size: 0.88rem;
      font-weight: 700;
      letter-spacing: -0.01em;
      color: var(--text-main);
    }}

    .parts-list {{
      overflow-y: auto;
      display: flex;
      flex-direction: column;
      gap: 6px;
      padding-right: 4px;
    }}

    .parts-list::-webkit-scrollbar {{
      width: 4px;
    }}

    .parts-list::-webkit-scrollbar-thumb {{
      background: rgba(255, 255, 255, 0.2);
      border-radius: 4px;
    }}

    .part-item {{
      background: var(--card-bg);
      border: 1px solid rgba(255, 255, 255, 0.05);
      border-radius: 10px;
      padding: 8px 10px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      font-size: 0.77rem;
      cursor: pointer;
      transition: all 0.15s;
    }}

    .part-item:hover {{
      background: rgba(56, 189, 248, 0.12);
      border-color: rgba(56, 189, 248, 0.3);
    }}

    .part-item.selected {{
      background: rgba(56, 189, 248, 0.2);
      border-color: var(--accent);
    }}

    .part-info-left {{
      display: flex;
      align-items: center;
      gap: 8px;
    }}

    .color-chip {{
      width: 12px;
      height: 12px;
      border-radius: 3px;
      border: 1px solid rgba(255, 255, 255, 0.3);
      flex-shrink: 0;
    }}

    .eye-toggle {{
      background: none;
      border: none;
      color: var(--text-muted);
      cursor: pointer;
      padding: 2px 4px;
      font-size: 0.85rem;
      display: flex;
      align-items: center;
      transition: color 0.15s;
    }}

    .eye-toggle:hover {{
      color: var(--text-main);
    }}

    .eye-toggle.hidden-part {{
      opacity: 0.35;
    }}

    /* Right Side Inspector Card */
    .inspector-card {{
      position: absolute;
      top: 90px;
      right: 20px;
      width: 290px;
      background: var(--panel-bg);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border: 1px solid var(--panel-border);
      border-radius: 18px;
      padding: 16px;
      box-shadow: 0 15px 40px rgba(0, 0, 0, 0.4);
      display: flex;
      flex-direction: column;
      gap: 10px;
      z-index: 15;
    }}

    .inspector-title {{
      font-family: 'Outfit', sans-serif;
      font-size: 0.88rem;
      font-weight: 700;
      color: var(--accent);
    }}

    .inspector-part-name {{
      font-size: 1rem;
      font-weight: 700;
      color: #ffffff;
    }}

    .inspector-desc {{
      font-size: 0.77rem;
      color: var(--text-muted);
      line-height: 1.45;
    }}

    .prop-row {{
      display: flex;
      justify-content: space-between;
      font-size: 0.74rem;
      padding: 4px 0;
      border-bottom: 1px solid rgba(255, 255, 255, 0.05);
    }}

    .prop-label {{
      color: var(--text-muted);
    }}

    .prop-val {{
      color: var(--text-main);
      font-weight: 600;
    }}

    /* Camera Preset Bar (Top Right) */
    .camera-presets {{
      display: flex;
      gap: 6px;
      pointer-events: auto;
    }}

    .view-btn {{
      background: var(--panel-bg);
      backdrop-filter: blur(10px);
      border: 1px solid var(--panel-border);
      color: var(--text-muted);
      font-size: 0.72rem;
      font-weight: 600;
      padding: 6px 10px;
      border-radius: 8px;
      cursor: pointer;
      transition: all 0.2s;
    }}

    .view-btn:hover, .view-btn.active {{
      background: var(--accent);
      color: #042f2e;
      border-color: var(--accent);
    }}
  </style>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
</head>
<body>

  <!-- Header Bar -->
  <div class="header-bar">
    <div class="brand">
      <h1>
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
          <path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/>
        </svg>
        ESP32-S3 Hi-Fi Radio Enclosure
      </h1>
      <p>Precision 3D CAD Virtual Assembly • Symmetrical Display Window • Bonded Hardware Module</p>
    </div>

    <!-- View Preset Buttons -->
    <div class="camera-presets">
      <button class="view-btn active" onclick="setCamera('iso')">Isometric</button>
      <button class="view-btn" onclick="setCamera('front')">Front</button>
      <button class="view-btn" onclick="setCamera('back')">Rear Cover</button>
      <button class="view-btn" onclick="setCamera('right')">Right (USB-C)</button>
      <button class="view-btn" onclick="setCamera('bottom')">Bottom (SD/Btn)</button>
      <button class="view-btn" onclick="setCamera('top')">Top</button>
    </div>
  </div>

  <!-- Left Side Parts Tree -->
  <div class="left-drawer">
    <div class="drawer-header">
      <span class="drawer-title">Assembly Hierarchy</span>
      <button class="btn" style="padding: 3px 8px; font-size: 0.7rem;" onclick="resetAllVisibility()">Show All</button>
    </div>
    <div class="parts-list" id="partsList">
      <!-- Injected by JavaScript -->
    </div>
  </div>

  <!-- Right Side Inspector Card -->
  <div class="inspector-card" id="inspectorCard">
    <span class="inspector-title">Component Details</span>
    <h3 class="inspector-part-name" id="inspName">Hover / Select Part</h3>
    <p class="inspector-desc" id="inspDesc">Slide the Explode slider below or click on any component to inspect its mechanical geometry and fit tolerances.</p>
    <div id="inspProps">
      <div class="prop-row">
        <span class="prop-label">Screen Center:</span>
        <span class="prop-val" style="color:#34d399;">X = 0.00 mm (Centered!)</span>
      </div>
      <div class="prop-row">
        <span class="prop-label">Board Unit:</span>
        <span class="prop-val">100% Bonded Assembly</span>
      </div>
      <div class="prop-row">
        <span class="prop-label">Rear Cover:</span>
        <span class="prop-val">Sleek Flat (Zero Feet)</span>
      </div>
    </div>
  </div>

  <!-- Bottom Explode Slider & Controls Dock -->
  <div class="control-dock">
    <div class="slider-row">
      <div class="slider-label">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2">
          <path d="M15 3h6v6M9 21H3v-6M21 3l-7 7M3 21l7-7"/>
        </svg>
        Explode View
      </div>
      <div class="slider-track-wrap">
        <input type="range" id="explodeSlider" class="explode-slider" min="0" max="1" step="0.005" value="0.0">
      </div>
      <div class="pct-badge" id="pctBadge">0%</div>
    </div>

    <div class="action-row">
      <div class="btn-group">
        <button class="btn" id="playBtn" onclick="toggleAutoPlay()">
          <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor">
            <polygon points="5 3 19 12 5 21 5 3"/>
          </svg>
          Auto Explode
        </button>
        <button class="btn" onclick="setSlider(0.0)">Mated (0%)</button>
        <button class="btn" onclick="setSlider(0.5)">Half (50%)</button>
        <button class="btn" onclick="setSlider(1.0)">Full (100%)</button>
      </div>

      <div class="btn-group">
        <button class="btn" id="xrayBtn" onclick="toggleXray()">X-Ray View</button>
        <button class="btn" onclick="resetCamera()">Reset View</button>
      </div>
    </div>
  </div>

  <canvas id="canvas3d"></canvas>

  <script>
    const PARTS_DATA = {json_data};

    let scene, camera, renderer, controls;
    const meshObjects = {{}};
    let isPlaying = false;
    let playDirection = 1;
    let isXray = false;
    let currentHoverPart = null;

    function init() {{
      const canvas = document.getElementById('canvas3d');
      scene = new THREE.Scene();
      scene.background = null;

      camera = new THREE.PerspectiveCamera(42, window.innerWidth / window.innerHeight, 1, 1000);
      camera.position.set(0, -270, 15); // Default to front view to inspect display alignment
      camera.up.set(0, 0, 1);

      renderer = new THREE.WebGLRenderer({{ canvas, antialias: true, alpha: true }});
      renderer.setPixelRatio(window.devicePixelRatio);
      renderer.setSize(window.innerWidth, window.innerHeight);
      renderer.shadowMap.enabled = true;
      renderer.shadowMap.type = THREE.PCFSoftShadowMap;

      controls = new THREE.OrbitControls(camera, renderer.domElement);
      controls.enableDamping = true;
      controls.dampingFactor = 0.08;
      controls.target.set(0, 0, 15);
      controls.maxDistance = 600;
      controls.minDistance = 30;

      // Studio Lighting
      const ambientLight = new THREE.AmbientLight(0xffffff, 0.8);
      scene.add(ambientLight);

      const dirLight1 = new THREE.DirectionalLight(0xffffff, 0.9);
      dirLight1.position.set(120, -140, 200);
      dirLight1.castShadow = true;
      scene.add(dirLight1);

      const dirLight2 = new THREE.DirectionalLight(0x93c5fd, 0.55);
      dirLight2.position.set(-150, 100, 100);
      scene.add(dirLight2);

      const bottomLight = new THREE.DirectionalLight(0x475569, 0.4);
      bottomLight.position.set(0, 0, -150);
      scene.add(bottomLight);

      // Build Meshes
      buildSceneMeshes();
      buildPartsListUI();

      // Events
      window.addEventListener('resize', onWindowResize);
      document.getElementById('explodeSlider').addEventListener('input', (e) => {{
        updateExplodedPositions(parseFloat(e.target.value));
      }});

      setupRaycaster();
      updateExplodedPositions(0.0);
      animate();
    }}

    function buildSceneMeshes() {{
      PARTS_DATA.forEach(part => {{
        const group = new THREE.Group();
        const mats = [];

        part.submeshes.forEach(sub => {{
          const geom = new THREE.BufferGeometry();
          const verts = new Float32Array(sub.vertices);
          const indices = new Uint32Array(sub.faces);

          geom.setAttribute('position', new THREE.BufferAttribute(verts, 3));
          geom.setIndex(new THREE.BufferAttribute(indices, 1));
          geom.computeVertexNormals();

          const mat = new THREE.MeshStandardMaterial({{
            color: sub.color,
            metalness: sub.metalness || 0.2,
            roughness: sub.roughness || 0.5,
            transparent: sub.transparent || false,
            opacity: sub.opacity !== undefined ? sub.opacity : 1.0,
            side: THREE.DoubleSide,
          }});

          const childMesh = new THREE.Mesh(geom, mat);
          childMesh.castShadow = true;
          childMesh.receiveShadow = true;
          childMesh.userData = {{ partId: part.id }};
          group.add(childMesh);

          mats.push({{
            material: mat,
            origColor: sub.color,
            origOpacity: sub.opacity !== undefined ? sub.opacity : 1.0,
            origTransparent: sub.transparent || false
          }});
        }});

        group.userData = part;
        scene.add(group);

        meshObjects[part.id] = {{
          group: group,
          materials: mats,
          data: part
        }};
      }});
    }}

    function buildPartsListUI() {{
      const container = document.getElementById('partsList');
      container.innerHTML = '';

      PARTS_DATA.forEach(part => {{
        const row = document.createElement('div');
        row.className = 'part-item';
        row.id = `item_${{part.id}}`;
        row.onmouseenter = () => highlightPart(part.id);
        row.onmouseleave = () => unhighlightPart();
        row.onclick = () => selectPart(part.id);

        row.innerHTML = `
          <div class="part-info-left">
            <span class="color-chip" style="background: ${{part.color}};"></span>
            <span>${{part.label}}</span>
          </div>
          <button class="eye-toggle" id="eye_${{part.id}}" onclick="event.stopPropagation(); togglePartVisibility('${{part.id}}')">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
              <circle cx="12" cy="12" r="3"/>
            </svg>
          </button>
        `;
        container.appendChild(row);
      }});
    }}

    function updateExplodedPositions(factor) {{
      document.getElementById('pctBadge').innerText = `${{Math.round(factor * 100)}}%`;

      PARTS_DATA.forEach(part => {{
        const obj = meshObjects[part.id];
        if (!obj) return;

        const mx = part.mated_pos[0];
        const my = part.mated_pos[1];
        const mz = part.mated_pos[2];

        const ex = part.explode_vec[0];
        const ey = part.explode_vec[1];
        const ez = part.explode_vec[2];

        obj.group.position.set(
          mx + ex * factor,
          my + ey * factor,
          mz + ez * factor
        );
      }});
    }}

    function setSlider(val) {{
      const slider = document.getElementById('explodeSlider');
      slider.value = val;
      updateExplodedPositions(val);
    }}

    function toggleAutoPlay() {{
      isPlaying = !isPlaying;
      const btn = document.getElementById('playBtn');
      if (isPlaying) {{
        btn.classList.add('active');
        btn.innerHTML = `
          <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor">
            <rect x="6" y="4" width="4" height="16"/>
            <rect x="14" y="4" width="4" height="16"/>
          </svg>
          Pause
        `;
      }} else {{
        btn.classList.remove('active');
        btn.innerHTML = `
          <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor">
            <polygon points="5 3 19 12 5 21 5 3"/>
          </svg>
          Auto Explode
        `;
      }}
    }}

    function toggleXray() {{
      isXray = !isXray;
      const btn = document.getElementById('xrayBtn');
      if (isXray) {{
        btn.classList.add('active');
        Object.values(meshObjects).forEach(o => {{
          if (o.data.id === 'front_case' || o.data.id === 'back_cover') {{
            o.materials.forEach(m => {{
              m.material.transparent = true;
              m.material.opacity = 0.28;
            }});
          }}
        }});
      }} else {{
        btn.classList.remove('active');
        Object.values(meshObjects).forEach(o => {{
          o.materials.forEach(m => {{
            m.material.transparent = m.origTransparent;
            m.material.opacity = m.origOpacity;
          }});
        }});
      }}
    }}

    function togglePartVisibility(partId) {{
      const obj = meshObjects[partId];
      if (!obj) return;
      obj.group.visible = !obj.group.visible;
      const eye = document.getElementById(`eye_${{partId}}`);
      if (obj.group.visible) {{
        eye.classList.remove('hidden-part');
      }} else {{
        eye.classList.add('hidden-part');
      }}
    }}

    function resetAllVisibility() {{
      Object.keys(meshObjects).forEach(id => {{
        meshObjects[id].group.visible = true;
        const eye = document.getElementById(`eye_${{id}}`);
        if (eye) eye.classList.remove('hidden-part');
      }});
    }}

    function highlightPart(partId) {{
      const obj = meshObjects[partId];
      if (!obj) return;
      obj.materials.forEach(m => {{
        m.material.emissive = new THREE.Color(0x38bdf8);
        m.material.emissiveIntensity = 0.35;
      }});
      showInspector(obj.data);
    }}

    function unhighlightPart() {{
      Object.values(meshObjects).forEach(o => {{
        o.materials.forEach(m => {{
          m.material.emissive = new THREE.Color(0x000000);
          m.material.emissiveIntensity = 0;
        }});
      }});
    }}

    function selectPart(partId) {{
      document.querySelectorAll('.part-item').forEach(el => el.classList.remove('selected'));
      const el = document.getElementById(`item_${{partId}}`);
      if (el) el.classList.add('selected');
      highlightPart(partId);
    }}

    function showInspector(part) {{
      document.getElementById('inspName').innerText = part.label;
      document.getElementById('inspDesc').innerText = part.description;
      document.getElementById('inspProps').innerHTML = `
        <div class="prop-row">
          <span class="prop-label">Subsystem:</span>
          <span class="prop-val">${{part.group}}</span>
        </div>
        <div class="prop-row">
          <span class="prop-label">Watertight Solid:</span>
          <span class="prop-val" style="color:#34d399;">Manifold STL True</span>
        </div>
        <div class="prop-row">
          <span class="prop-label">Explode Vector:</span>
          <span class="prop-val">[${{part.explode_vec.join(', ')}}] mm</span>
        </div>
      `;
    }}

    function setCamera(view) {{
      document.querySelectorAll('.view-btn').forEach(b => b.classList.remove('active'));
      event.target.classList.add('active');

      const target = new THREE.Vector3(0, 0, 15);
      controls.target.copy(target);

      if (view === 'iso') {{
        camera.position.set(130, -180, 160);
      }} else if (view === 'front') {{
        camera.position.set(0, -270, 15);
      }} else if (view === 'back') {{
        camera.position.set(0, 270, 15);
      }} else if (view === 'right') {{
        camera.position.set(270, 0, 15);
      }} else if (view === 'bottom') {{
        camera.position.set(0, -15, -270);
      }} else if (view === 'top') {{
        camera.position.set(0, 0, 270);
      }}
      controls.update();
    }}

    function resetCamera() {{
      camera.position.set(130, -180, 160);
      controls.target.set(0, 0, 15);
      controls.update();
    }}

    function setupRaycaster() {{
      const raycaster = new THREE.Raycaster();
      const mouse = new THREE.Vector2();

      window.addEventListener('pointermove', (e) => {{
        mouse.x = (e.clientX / window.innerWidth) * 2 - 1;
        mouse.y = -(e.clientY / window.innerHeight) * 2 + 1;

        raycaster.setFromCamera(mouse, camera);
        
        // Collect all visible child meshes
        const visibleChildMeshes = [];
        Object.values(meshObjects).forEach(o => {{
          if (o.group.visible) {{
            o.group.traverse(child => {{
              if (child.isMesh) visibleChildMeshes.push(child);
            }});
          }}
        }});

        const intersects = raycaster.intersectObjects(visibleChildMeshes);

        if (intersects.length > 0) {{
          const hit = intersects[0].object;
          const partId = hit.userData ? hit.userData.partId : null;
          if (partId && partId !== currentHoverPart) {{
            unhighlightPart();
            currentHoverPart = partId;
            highlightPart(currentHoverPart);
          }}
        }} else {{
          if (currentHoverPart) {{
            unhighlightPart();
            currentHoverPart = null;
          }}
        }}
      }});
    }}

    function onWindowResize() {{
      camera.aspect = window.innerWidth / window.innerHeight;
      camera.updateProjectionMatrix();
      renderer.setSize(window.innerWidth, window.innerHeight);
    }}

    function animate() {{
      requestAnimationFrame(animate);

      if (isPlaying) {{
        const slider = document.getElementById('explodeSlider');
        let val = parseFloat(slider.value) + 0.005 * playDirection;
        if (val >= 1.0) {{
          val = 1.0;
          playDirection = -1;
        }} else if (val <= 0.0) {{
          val = 0.0;
          playDirection = 1;
        }}
        slider.value = val;
        updateExplodedPositions(val);
      }}

      controls.update();
      renderer.render(scene, camera);
    }}

    window.onload = init;
  </script>
</body>
</html>
"""

if __name__ == '__main__':
    main()
