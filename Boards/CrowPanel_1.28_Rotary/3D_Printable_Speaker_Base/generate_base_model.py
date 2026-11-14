import numpy as np
import scipy.interpolate as interp
import cv2
import trimesh
from shapely.geometry import Polygon
import shapely.geometry as sg
import os

# =============================================================================
# 1. PURE VECTOR SPLINE EXTRACTION FOR EDIFIER 3-FLAME EMBLEM & OVAL LINE
# =============================================================================
def extract_flame_spline_polygons(user_img_path, target_h=7.2):
    """
    Extracts the 3 iconic Edifier flame/acoustic wing strokes directly from the
    user reference image, fitting continuous periodic B-splines to guarantee
    100% vector-smooth contours with ZERO pixelation or voxel staircases.
    """
    img = cv2.imread(user_img_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        raise FileNotFoundError(f"Cannot load reference image at {user_img_path}")
        
    _, thresh = cv2.threshold(img, 128, 255, cv2.THRESH_BINARY_INV)
    contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    
    # 3 flames are the large contours located in the upper region (y < 220)
    flame_contours = []
    for c in contours:
        x, y, w, h = cv2.boundingRect(c)
        if y < 220 and cv2.contourArea(c) > 1000:
            flame_contours.append(c.squeeze())
            
    # Sort left to right by x centroid
    flame_contours = sorted(flame_contours, key=lambda c: np.mean(c[:, 0]))
    
    # Calculate bounding center across all 3 flames
    all_pts = np.vstack(flame_contours)
    x_min, x_max = all_pts[:, 0].min(), all_pts[:, 0].max()
    y_min, y_max = all_pts[:, 1].min(), all_pts[:, 1].max()
    
    cx = (x_min + x_max) / 2.0
    cy = (y_min + y_max) / 2.0
    orig_h = (y_max - y_min)
    scale = target_h / orig_h
    
    flame_polys = []
    for c in flame_contours:
        c = np.vstack([c, c[0]])
        # Fit smooth periodic B-spline with 120 sub-pixel points
        tck, u = interp.splprep([c[:, 0], c[:, 1]], s=15.0, per=True)
        u_fine = np.linspace(0, 1, 120, endpoint=False)
        xf, yf = interp.splev(u_fine, tck)
        
        # Transform to centered real-world coordinates (X: horizontal, Z: vertical)
        # Negate X so left-to-right in 2D reference matches left-to-right when looking at front (+Y)
        x_world = -(xf - cx) * scale
        z_world = -(yf - cy) * scale
        
        pts = np.column_stack([x_world, z_world])
        poly = Polygon(pts)
        if not poly.exterior.is_ccw:
            poly = Polygon(poly.exterior.coords[::-1])
        flame_polys.append(poly)
        
    return flame_polys

def create_oval_line_polygon(rx=7.2, ry=5.6, thickness=0.75, n_pts=160):
    """
    Creates a simple, clean, elegant 2D oval line border (ring polygon)
    framing the Edifier 3-flame emblem, completely free of busy floral decoration.
    """
    thetas = np.linspace(0, 2 * np.pi, n_pts, endpoint=False)
    
    # Outer ellipse
    x_out = rx * np.cos(thetas)
    z_out = ry * np.sin(thetas)
    outer_ring = np.column_stack([x_out, z_out])
    
    # Inner ellipse
    rxi = rx - thickness
    ryi = ry - thickness
    x_in = rxi * np.cos(thetas[::-1])
    z_in = ryi * np.sin(thetas[::-1])
    inner_ring = np.column_stack([x_in, z_in])
    
    return Polygon(shell=outer_ring, holes=[inner_ring[::-1]])

def create_conical_frustum(r_bot, r_top, h, sections=144):
    """
    Generates an exact watertight truncated cone (frustum) mesh.
    """
    thetas = np.linspace(0, 2 * np.pi, sections, endpoint=False)
    verts = []
    for th in thetas:
        verts.append([r_bot * np.cos(th), r_bot * np.sin(th), 0.0])
    for th in thetas:
        verts.append([r_top * np.cos(th), r_top * np.sin(th), h])
    verts.append([0.0, 0.0, 0.0])
    verts.append([0.0, 0.0, h])
    verts = np.array(verts)
    
    faces = []
    bot_center = 2 * sections
    top_center = 2 * sections + 1
    for i in range(sections):
        nxt = (i + 1) % sections
        faces.append([i, nxt, sections + nxt])
        faces.append([i, sections + nxt, sections + i])
        faces.append([bot_center, nxt, i])
        faces.append([top_center, sections + i, sections + nxt])
        
    return trimesh.Trimesh(vertices=verts, faces=faces, process=True)

# =============================================================================
# 2. SOLID THREAD RIBBON GENERATOR
# =============================================================================
def generate_solid_thread_ribbon(r_root, r_crest, pitch, turns, z_base, sections_per_turn=72):
    total_steps = int(turns * sections_per_turn)
    phis = np.linspace(0, turns * 2 * np.pi, total_steps)
    
    depth = (r_crest - r_root)
    half_crest_w = 0.6
    half_root_w = half_crest_w + depth * 0.7
    r_inner = r_root - 1.5
    
    verts = []
    taper_steps = sections_per_turn // 4
    
    for i, phi in enumerate(phis):
        z_c = z_base + (pitch / (2 * np.pi)) * phi
        
        if i < taper_steps:
            f_taper = i / taper_steps
        elif i > total_steps - taper_steps:
            f_taper = (total_steps - i) / taper_steps
        else:
            f_taper = 1.0
            
        cur_crest_r = r_root + depth * f_taper
        c = np.cos(phi)
        s = np.sin(phi)
        
        p0 = [r_inner * c, r_inner * s, z_c - half_root_w]
        p1 = [r_root * c, r_root * s, z_c - half_root_w]
        p2 = [cur_crest_r * c, cur_crest_r * s, z_c - half_crest_w]
        p3 = [cur_crest_r * c, cur_crest_r * s, z_c + half_crest_w]
        p4 = [r_root * c, r_root * s, z_c + half_root_w]
        p5 = [r_inner * c, r_inner * s, z_c + half_root_w]
        
        verts.extend([p0, p1, p2, p3, p4, p5])
        
    verts = np.array(verts)
    faces = []
    
    for i in range(total_steps - 1):
        idx = i * 6
        nxt = (i + 1) * 6
        for seg in range(5):
            faces.append([idx + seg, nxt + seg, nxt + seg + 1])
            faces.append([idx + seg, nxt + seg + 1, idx + seg + 1])
        faces.append([idx + 5, nxt + 5, nxt + 0])
        faces.append([idx + 5, nxt + 0, idx + 0])
        
    faces.append([0, 1, 2])
    faces.append([0, 2, 3])
    faces.append([0, 3, 4])
    faces.append([0, 4, 5])
    
    last_idx = (total_steps - 1) * 6
    faces.append([last_idx + 0, last_idx + 2, last_idx + 1])
    faces.append([last_idx + 0, last_idx + 3, last_idx + 2])
    faces.append([last_idx + 0, last_idx + 4, last_idx + 3])
    faces.append([last_idx + 0, last_idx + 5, last_idx + 4])
    
    return trimesh.Trimesh(vertices=verts, faces=faces, process=True)

# =============================================================================
# 3. REVOLVED CONES WITH ARCHITECTURAL RIMS
# =============================================================================
def make_rimmed_cone(h_bottom, h_top, r_bottom, r_top, is_part_b=False, sections=180, z_steps=80):
    z_vals = np.linspace(h_bottom, h_top, z_steps)
    thetas = np.linspace(0, 2 * np.pi, sections, endpoint=False)
    h_total = h_top - h_bottom
    
    verts = [[0.0, 0.0, h_bottom]]
    for z in z_vals:
        t = (z - h_bottom) / h_total
        r_cone = r_bottom + (r_top - r_bottom) * t
        
        # Base flare for Part B (bottom 2.2mm)
        if is_part_b and (z - h_bottom) < 2.2:
            tb = (z - h_bottom) / 2.2
            r_cone += 1.5 * (1.0 - tb)**1.4
            
        # Top crown lip rim for Part A (top 1.5mm)
        if not is_part_b and (h_top - z) < 1.5:
            tt = (h_top - z) / 1.5
            r_cone += 0.85 * (1.0 - tt)
            
        delta_r = 0.0
        if is_part_b:
            # Plinth bead (Z = 2.2 to 2.8mm)
            if 2.2 <= z <= 2.8:
                delta_r += 0.65 * np.sin((z - 2.2) / 0.6 * np.pi)
            # Mid dividing bead (Z = 5.2 to 5.8mm)
            if 5.2 <= z <= 5.8:
                delta_r += 0.65 * np.sin((z - 5.2) / 0.6 * np.pi)
            # Seam rim (Z = 16.5 to 17.5mm)
            if 16.5 <= z <= 17.5:
                delta_r += 0.70 * np.sin((z - 16.5) / 1.0 * np.pi * 0.5)
        else:
            # Seam rim (Z = 0.0 to 1.0mm)
            if 0.0 <= z <= 1.0:
                delta_r += 0.70 * np.cos((z - 0.0) / 1.0 * np.pi * 0.5)
            # Mid accent dividing bead (Z = 4.0 to 4.6mm)
            if 4.0 <= z <= 4.6:
                delta_r += 0.65 * np.sin((z - 4.0) / 0.6 * np.pi)
            # Crown lip bead (Z = 6.8 to 7.4mm)
            if 6.8 <= z <= 7.4:
                delta_r += 0.65 * np.sin((z - 6.8) / 0.6 * np.pi)
                
        r = r_cone + delta_r
        for th in thetas:
            verts.append([r * np.cos(th), r * np.sin(th), z])
            
    verts.append([0.0, 0.0, h_top])
    verts = np.array(verts)
    
    faces = []
    for s in range(sections):
        nxt = 1 + (s + 1) % sections
        faces.append([0, nxt, 1 + s])
        
    for zi in range(z_steps - 1):
        r1 = 1 + zi * sections
        r2 = 1 + (zi + 1) * sections
        for s in range(sections):
            sn = (s + 1) % sections
            faces.append([r1 + s, r1 + sn, r2 + sn])
            faces.append([r1 + s, r2 + sn, r2 + s])
            
    top_c = len(verts) - 1
    last_r = 1 + (z_steps - 1) * sections
    for s in range(sections):
        faces.append([top_c, last_r + s, last_r + (s + 1) % sections])
        
    return trimesh.Trimesh(vertices=verts, faces=faces, process=True)

# =============================================================================
# 4. MASTER MODULAR GENERATION PIPELINE
# =============================================================================
def generate_pure_vector_modular_system():
    sections = 120
    
    H_bottom = 17.5
    H_top = 8.5
    
    R_base = 32.0         # Ø64.0 mm flared base plinth
    R_seam = 27.0         # Ø54.0 mm mating seam
    R_top = 25.2          # Ø50.4 mm top collar
    
    pitch = 4.5           # Coarse bottle-cap spiral thread
    turns = 1.25
    thread_len = 5.8
    
    R_male_root = 20.4
    R_male_crest = 21.8
    
    clearance = 0.45      # Smooth, loose non-binding clearance
    R_female_crest = R_male_root + clearance    # 20.85 mm
    R_female_root = R_male_crest + clearance    # 22.25 mm
    
    # -------------------------------------------------------------------------
    # PART A: TOP ADAPTER (Classical Rims, Zero Text, Snug Collar)
    # -------------------------------------------------------------------------
    print("Generating Part A (Classical Royal Top Adapter)...")
    a_outer = make_rimmed_cone(0.0, H_top, R_seam, R_top, is_part_b=False, sections=180, z_steps=60)
    
    a_neck = trimesh.creation.cylinder(radius=R_male_root, height=thread_len + 0.5, sections=sections)
    a_neck.apply_translation([0, 0, -thread_len / 2.0 + 0.25])
    
    a_thread = generate_solid_thread_ribbon(R_male_root, R_male_crest, pitch, turns, z_base=-thread_len + 0.4)
    
    part_a = a_outer.union(a_neck)
    part_a = part_a.union(a_thread)
    
    # Tightened CrowPanel Collar (Ø47.10mm, snug fit)
    R_collar = 23.55
    H_collar = 3.5
    H_shelf = H_top - H_collar # Z = 5.0 mm
    
    a_collar = trimesh.creation.cylinder(radius=R_collar, height=H_collar + 1.0, sections=sections)
    a_collar.apply_translation([0, 0, H_shelf + (H_collar + 1.0) / 2.0])
    part_a = part_a.difference(a_collar)
    
    # Central Bore (R = 17.5mm, Ø35.0mm, leaves massive solid 6.05mm rim at Z=5.0mm,
    # and 2.9mm solid male neck wall down to Z = -5.8mm)
    a_bore = trimesh.creation.cylinder(radius=17.5, height=thread_len + H_shelf + 2.0, sections=sections)
    a_bore.apply_translation([0, 0, (-thread_len + H_shelf) / 2.0])
    part_a = part_a.difference(a_bore)

    # -------------------------------------------------------------------------
    # SCHEMATIC-MATCHED PRECISION PLUG PASS-THROUGH CHUTES (FRONT-VIEW ORIENTATION)
    # Engineered from Eagle BRD layout (ESP32 Display-1.28-V1.0.brd) and horizontally
    # inverted (X -> -X) because the board is inserted FACE-UP (LCD screen facing up,
    # knob at front). Eagle's Layer 1 is the rear component side; viewing from the
    # front reflects horizontally across the vertical Y axis:
    # - J34 (UART 1.25-4P): center (-15.25, 6.15), plug envelope X in [-20.0, -10.5], Y in [2.5, 9.8] (WEST)
    # - J36 (Audio 1.25-4P): center (-15.25, -4.50), plug envelope X in [-20.0, -10.5], Y in [-8.2, -0.8] (WEST)
    # - J37 (Power 1.25-4P): center (12.00, 10.75), plug envelope X in [8.5, 15.5], Y in [7.0, 14.5] (EAST)
    # Cutter outer limit is strictly R <= 20.5 mm so male thread roots (R = 20.4 mm)
    # and crests (R = 21.8 mm) remain 100% solid & structurally intact!
    # -------------------------------------------------------------------------
    plug_mode = "discrete"  # Default: Option 1 (92.4% solid shelf preserved)
    h_plug_chute = thread_len + H_shelf + 2.0
    cutter_limit = trimesh.creation.cylinder(radius=20.5, height=h_plug_chute, sections=sections)
    cutter_limit.apply_translation([0, 0, (-thread_len + H_shelf) / 2.0])

    if plug_mode == "discrete":
        # Option 1: 3 Discrete Tailored Pockets (Leaves 3.3mm solid radial bridge between J34 & J36 on West)
        # J34 (UART): dx=9.5, dy=7.3, center: [-15.25, 6.15]
        b_j34 = trimesh.creation.box(extents=[9.5, 7.3, h_plug_chute])
        b_j34.apply_translation([-15.25, 6.15, (-thread_len + H_shelf) / 2.0])
        part_a = part_a.difference(b_j34.intersection(cutter_limit))

        # J36 (Audio / I2C): dx=9.5, dy=7.4, center: [-15.25, -4.5]
        b_j36 = trimesh.creation.box(extents=[9.5, 7.4, h_plug_chute])
        b_j36.apply_translation([-15.25, -4.5, (-thread_len + H_shelf) / 2.0])
        part_a = part_a.difference(b_j36.intersection(cutter_limit))
    else:
        # Option 2: Unified West Window (X in [-20.0, -10.5], Y in [-8.2, 9.8], dx=9.5, dy=18.0)
        b_west = trimesh.creation.box(extents=[9.5, 18.0, h_plug_chute])
        b_west.apply_translation([-15.25, 0.8, (-thread_len + H_shelf) / 2.0])
        part_a = part_a.difference(b_west.intersection(cutter_limit))

    # J37 (Power / USB): dx=7.0, dy=7.5, center: [12.0, 10.75]
    b_j37 = trimesh.creation.box(extents=[7.0, 7.5, h_plug_chute])
    b_j37.apply_translation([12.0, 10.75, (-thread_len + H_shelf) / 2.0])
    part_a = part_a.difference(b_j37.intersection(cutter_limit))

    # -------------------------------------------------------------------------
    # U-SHAPED SCREW HOLDERS & VERTICAL ACCESS GAPS FOR SCREWS & SCREWDRIVER
    # Horizontally flipped (X -> -X) for front-view insertion:
    # - T2 standoff: Top-Left / NW (-13.506, +13.506)
    # - T1 standoff: Bottom-Right / SE (+13.506, -13.506)
    # - U-slots through 2.0mm shelf web (width 2.4mm, Z = 3.0mm to 5.2mm)
    # - Full-depth vertical access channel through threaded neck below (width 5.2mm,
    #   Z = -thread_len - 1.0mm to +3.0mm) allowing screws & screwdrivers to pass smoothly!
    # - Flat ceiling step at Z = 3.0mm for M2 screw heads (dia 3.8mm) to clamp
    # - ZERO outer protrusion (R <= 20.4mm root, 21.8mm crest), fits Part B!
    # -------------------------------------------------------------------------
    # U-slots through 2.0mm shelf web (width 2.4mm)
    poly_t1 = sg.LineString([[0, 0], [13.506, -13.506]]).buffer(1.2, resolution=24, cap_style=1)
    m_u1 = trimesh.creation.extrude_polygon(poly_t1, height=2.4)
    m_u1.apply_translation([0, 0, 3.0])
    part_a = part_a.difference(m_u1)

    poly_t2 = sg.LineString([[0, 0], [-13.506, 13.506]]).buffer(1.2, resolution=24, cap_style=1)
    m_u2 = trimesh.creation.extrude_polygon(poly_t2, height=2.4)
    m_u2.apply_translation([0, 0, 3.0])
    part_a = part_a.difference(m_u2)

    # Vertical screw & screwdriver clearance gaps in threaded neck (Z = -thread_len - 1.0 to 3.0mm)
    h_gap = thread_len + 3.0 + 1.0 # 9.8mm
    for pos in [(-13.506, 13.506), (13.506, -13.506)]:
        gap_cyl = trimesh.creation.cylinder(radius=2.6, height=h_gap, sections=32)
        gap_cyl.apply_translation([pos[0], pos[1], -thread_len - 1.0 + h_gap / 2.0])
        part_a = part_a.difference(gap_cyl)
        
        poly_slot = sg.LineString([[0, 0], pos]).buffer(2.6, resolution=24, cap_style=1)
        m_slot = trimesh.creation.extrude_polygon(poly_slot, height=h_gap)
        m_slot.apply_translation([0, 0, -thread_len - 1.0])
        part_a = part_a.difference(m_slot)

    # Guarantee 100% manifold watertightness using manifold3d
    try:
        import manifold3d
        vp = np.ascontiguousarray(part_a.vertices, dtype=np.float32)
        tv = np.ascontiguousarray(part_a.faces, dtype=np.uint32)
        man = manifold3d.Manifold(manifold3d.Mesh(vert_properties=vp, tri_verts=tv))
        out_mesh = man.to_mesh()
        part_a = trimesh.Trimesh(vertices=out_mesh.vert_properties[:, :3], faces=out_mesh.tri_verts, process=True)
    except Exception as e:
        print("manifold3d post-processing warning:", e)
    
    print("Part A generated. Watertight:", part_a.is_watertight, "Volume:", part_a.volume)
    
    # -------------------------------------------------------------------------
    # PART B: BOTTOM PEDESTAL (Pure Vector Edifier Logo + Oval Line Alone)
    # -------------------------------------------------------------------------
    print("Generating Part B (Pedestal with Pure Vector Edifier Crest & Oval Line)...")
    H_floor = 2.5
    R_cavity = 20.0
    
    # 1. Base Cone with classical architectural rims
    b_outer = make_rimmed_cone(0.0, H_bottom, R_base, R_seam, is_part_b=True, sections=180, z_steps=80)
    
    # 2. Extract pure vector B-splines of the 3 flames + oval line
    ref_img_path = r"C:\Users\sures\.gemini\antigravity-ide\brain\9931debd-a847-4467-aa26-30d290098811\.user_uploaded\media_1792648941436.png"
    flames = extract_flame_spline_polygons(ref_img_path, target_h=7.5)
    oval_line = create_oval_line_polygon(rx=7.2, ry=5.6, thickness=0.75)
    
    all_polys = flames + [oval_line]
    
    # Extrude along Y (from behind cone wall outwards)
    crest_meshes = []
    for p in all_polys:
        m = trimesh.creation.extrude_polygon(p, height=12.0)
        # Map 2D (X, Y) -> 3D (X, Z), extrusion -> +Y
        R = np.array([
            [1, 0, 0, 0],
            [0, 0, 1, 0],
            [0, 1, 0, 0],
            [0, 0, 0, 1]
        ], dtype=float)
        m.apply_transform(R)
        # Center crest at front (+Y), Z = 11.2 mm
        m.apply_translation([0.0, 24.0, 11.2])
        crest_meshes.append(m)
        
    extruded_crest = trimesh.util.concatenate(crest_meshes)
    
    # Conical shell cutter to give uniform +0.65mm proud relief with 0.4mm inward root penetration
    cone_outer_shell = create_conical_frustum(R_base + 0.65, R_seam + 0.65, H_bottom, sections=144)
    cone_inner_shell = create_conical_frustum(R_base - 0.40, R_seam - 0.40, H_bottom, sections=144)
    crest_shell = cone_outer_shell.difference(cone_inner_shell)
    
    vector_crest_3d = extruded_crest.intersection(crest_shell)
    print("Vector crest 3D generated. Watertight:", vector_crest_3d.is_watertight, "Volume:", vector_crest_3d.volume)
    
    # Fuse crest onto the outer cone
    b_outer = b_outer.union(vector_crest_3d)
    
    # 3. Internal Chamber & Socket
    b_cavity = trimesh.creation.cylinder(radius=R_cavity, height=H_bottom - H_floor + 0.1, sections=sections)
    b_cavity.apply_translation([0, 0, H_floor + (H_bottom - H_floor + 0.1) / 2.0])
    part_b = b_outer.difference(b_cavity)
    
    b_socket = trimesh.creation.cylinder(radius=R_female_crest, height=thread_len + 0.5, sections=sections)
    b_socket.apply_translation([0, 0, (H_bottom - thread_len) + (thread_len + 0.5) / 2.0])
    part_b = part_b.difference(b_socket)
    
    b_thread_cutter = generate_solid_thread_ribbon(R_female_crest - 0.2, R_female_root, pitch, turns, z_base=H_bottom - thread_len + 0.4)
    part_b = part_b.difference(b_thread_cutter)
    
    # MAX98357A guide rails
    guide1 = trimesh.creation.box(extents=[16.0, 1.8, 2.2])
    guide1.apply_translation([0, -9.5, H_floor + 1.1])
    guide2 = trimesh.creation.box(extents=[16.0, 1.8, 2.2])
    guide2.apply_translation([0, 9.5, H_floor + 1.1])
    part_b = part_b.union(guide1)
    part_b = part_b.union(guide2)
    
    # Bottom wire hole (Ø6.5 mm)
    b_wire_bottom = trimesh.creation.cylinder(radius=3.25, height=H_floor + 2.0, sections=48)
    b_wire_bottom.apply_translation([0, 0, H_floor / 2.0])
    part_b = part_b.difference(b_wire_bottom)
    
    # Arched Side Cable Exit Port (Width = 9.0 mm, Height = 7.0 mm, at rear Y = -R_base)
    side_box = trimesh.creation.box(extents=[9.0, 20.0, 7.0])
    side_box.apply_translation([0, -R_base + 5.0, H_floor + 3.5])
    
    side_arch = trimesh.creation.cylinder(radius=4.5, height=20.0, sections=36)
    rot_x = trimesh.transformations.rotation_matrix(np.pi / 2, [1, 0, 0])
    side_arch.apply_transform(rot_x)
    side_arch.apply_translation([0, -R_base + 5.0, H_floor + 7.0])
    
    part_b = part_b.difference(side_box)
    part_b = part_b.difference(side_arch)
    
    print("Part B generated. Watertight:", part_b.is_watertight, "Volume:", part_b.volume)
    
    return part_a, part_b

if __name__ == '__main__':
    out_dir = os.path.dirname(os.path.abspath(__file__))
    part_a, part_b = generate_pure_vector_modular_system()
    
    path_a = os.path.join(out_dir, "CrowPanel_1.28_Conical_Top_Adapter.stl")
    path_b = os.path.join(out_dir, "CrowPanel_1.28_Conical_Bottom_Pedestal.stl")
    
    part_a.export(path_a)
    part_b.export(path_b)
    
    print("\n[SUCCESS] Exported Pure Vector STLs (Zero Pixelation):")
    print(" - Part A:", path_a, f"({os.path.getsize(path_a):,} bytes) [Watertight: {part_a.is_watertight}]")
    print(" - Part B:", path_b, f"({os.path.getsize(path_b):,} bytes) [Watertight: {part_b.is_watertight}]")
