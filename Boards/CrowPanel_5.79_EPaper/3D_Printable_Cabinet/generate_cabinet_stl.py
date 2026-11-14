import os
import numpy as np
import trimesh
from shapely.geometry import Polygon

# =============================================================================
# 1. PARAMETERS & GEOMETRY CONSTANTS
# =============================================================================
W_outer  = 173.0
H_outer  =  62.5
D_front  =  14.0
D_rear   =  28.0  # Increased from 22.0mm to 28.0mm to give 25.8mm internal cavity depth!
R_corner =   4.0
Wall     =   2.2

# CrowPanel 5.79" Display Module Pocket (Board: 167.92 x 56.97 x 12.0 mm)
W_board  = 168.2
H_board  =  57.8
D_board  =  12.0

# Active E-Paper Display Window (Glass active: 139.0 x 47.74 mm)
W_win    = 140.5
H_win    =  48.5

# Factory Board M3 Inserts (X = +/-50.0mm, Y = 0.0mm)
x_screw_l = -50.0
x_screw_r =  50.0
y_screw   =   0.0

# SINGLE Corner Assembly Screws (Rear Cabinet to Front Bezel: ONE hole per corner!)
x_corner = W_outer / 2.0 - 4.5 # 82.0mm
y_corner = H_outer / 2.0 - 4.5 # 26.75mm

# Spring-Action Latch Positions on Top and Bottom Edges of Bracket & Bezel Pocket
snap_x_top = [-28.0, 28.0]
snap_x_bot = [-12.0, 12.0]
snap_w     =   8.0  # Width of spring latch tooth
snap_h     =   1.3  # Protrusion depth into wall
snap_z     =   8.0  # Z height where spring latch locks into bezel pocket

# Rear Buttons (Eagle BRD: K1 BOOT @ X=+40.48, Y=+23.27; K2 RESET @ X=+54.18, Y=+23.27)
x_boot  = 40.48
x_reset = 54.18
y_btn   = 23.27

# Dual Wall Keyholes (X = -65.0mm & X = +68.0mm, Y = +14.0mm: >10.2mm clear of Reset button!)
x_kh_l  = -65.0
x_kh_r  =  68.0
y_kh    =  14.0

# 40mm Speaker Subsystem (Left-Center: X = -25.0mm, Y = 0.0mm)
x_spk   = -25.0
y_spk   =   0.0
r_spk   =  20.25 # 40.5mm dia
h_spk_rim = 2.5  # Shallow 2.5mm retention rim on back door (NO deep cup!)

# Dual 18650 Battery Cradle (Right Half: X = 41.0mm, Y = -6.0mm)
# Expanded to 72.0mm x 40.0mm x 9.0mm deep with generous 25.8mm cavity! ZERO overlap with speaker!
x_bat_center = 41.0
y_bat_center = -6.0
w_bat_bay    = 72.0  # Fits unprotected (65mm) and protected (67-69mm) 18650s or standard holder
h_bat_bay    = 40.0  # Fits two 18.5mm cells side-by-side with 1.8mm partition
d_bat_bay    =  9.0  # 9.0mm tall curved cradle saddles

# MAX98357A I2S DAC Breakout (Far Left: X = -60.0mm, Y = 0.0mm)
x_amp = -60.0
y_amp =   0.0

# Bottom Edge Ports
x_usbc =  36.03
x_sd   = -39.87

def make_rounded_rect_poly(w, h, r, n_pts=24):
    th = np.linspace(0, np.pi/2, n_pts)
    c1 = np.column_stack([ w/2 - r + r*np.cos(th),  h/2 - r + r*np.sin(th)])
    c2 = np.column_stack([-w/2 + r - r*np.sin(th),  h/2 - r + r*np.cos(th)])
    c3 = np.column_stack([-w/2 + r - r*np.cos(th), -h/2 + r - r*np.sin(th)])
    c4 = np.column_stack([ w/2 - r + r*np.sin(th), -h/2 + r - r*np.cos(th)])
    pts = np.vstack([c1, c2, c3, c4])
    return Polygon(pts)

# =============================================================================
# 2. GENERATE FRONT BEZEL (WITH EXACTLY 1 SCREW BOSS PER CORNER + SNAP DETENTS)
# =============================================================================
def generate_front_bezel():
    print("Generating CrowPanel 5.79 Front Bezel (SINGLE boss per corner + snap detents)...")
    p_outer = make_rounded_rect_poly(W_outer, H_outer, R_corner)
    bezel = trimesh.creation.extrude_polygon(p_outer, height=D_front)
    
    # 1. Window Cutout (140.5mm x 48.5mm) through front face (Wall = 2.0mm)
    win_cut = trimesh.creation.box(extents=[W_win, H_win, Wall + 4.0])
    win_cut.apply_translation([0, 0, (Wall + 4.0)/2.0 - 1.0])
    bezel = bezel.difference(win_cut)
    
    # 2. CrowPanel Module Internal Pocket (168.2 x 57.8 x 12.0 mm)
    p_pocket = make_rounded_rect_poly(W_board, H_board, 2.0)
    pocket = trimesh.creation.extrude_polygon(p_pocket, height=D_front - Wall + 1.0)
    pocket.apply_translation([0, 0, Wall])
    bezel = bezel.difference(pocket)
    
    # 3. ONLY 1 SINGLE CORNER BOSS PER CORNER (M2.5/M3 threaded core: dia 2.6mm)
    # The redundant second inner boss has been 100% eliminated!
    for mx in [-1, 1]:
        for my in [-1, 1]:
            cx = mx * x_corner
            cy = my * y_corner
            cboss = trimesh.creation.cylinder(radius=3.5, height=D_front - Wall, sections=32)
            cboss.apply_translation([cx, cy, Wall + (D_front - Wall)/2.0])
            bezel = bezel.union(cboss)
            
            chole = trimesh.creation.cylinder(radius=1.3, height=D_front + 2.0, sections=24)
            chole.apply_translation([cx, cy, D_front/2.0])
            bezel = bezel.difference(chole)
            
    # 4. Spring Latch Receiver Detent Notches on Internal Pocket Walls (Top & Bottom)
    # 2 notches on Top wall
    for sx in snap_x_top:
        d_notch = trimesh.creation.box(extents=[snap_w + 2.0, 2.0, 3.5])
        d_notch.apply_translation([sx, H_board/2.0 - 0.2, Wall + snap_z])
        bezel = bezel.difference(d_notch)
        
    # 2 notches on Bottom wall
    for sx in snap_x_bot:
        d_notch = trimesh.creation.box(extents=[snap_w + 2.0, 2.0, 3.5])
        d_notch.apply_translation([sx, -H_board/2.0 + 0.2, Wall + snap_z])
        bezel = bezel.difference(d_notch)
        
    # 5. Left Edge Thumb Window for Jog Dial K5 & Navigation Keys
    dial_win = trimesh.creation.box(extents=[10.0, 28.0, D_front - Wall + 2.0])
    dial_win.apply_translation([-W_outer/2.0 + 3.0, 0.0, Wall + (D_front - Wall)/2.0])
    bezel = bezel.difference(dial_win)
    
    # 6. Bottom Edge USB-C & MicroSD Slots
    usbc_cut = trimesh.creation.box(extents=[13.0, 10.0, 7.0])
    usbc_cut.apply_translation([x_usbc, -H_outer/2.0 + 3.0, Wall + 3.5])
    bezel = bezel.difference(usbc_cut)
    
    sd_cut = trimesh.creation.box(extents=[15.5, 10.0, 4.0])
    sd_cut.apply_translation([x_sd, -H_outer/2.0 + 3.0, Wall + 2.0])
    bezel = bezel.difference(sd_cut)
    
    print("Front Bezel compiled. Watertight:", bezel.is_watertight, "Volume:", bezel.volume)
    return bezel

# =============================================================================
# 3. GENERATE ULTRA-STRONG SNAP-FIT MOUNTING BRACKET (4.0mm BASE + PERIMETER RIBS)
# =============================================================================
def generate_mounting_bracket():
    print("Generating CrowPanel 5.79 Heavy-Duty Snap-Fit Bracket (Reinforced 4.0mm + Ribs)...")
    t_base = 4.0   # Substantial 4.0mm thick solid plate (up from 2.4mm!)
    h_rib  = 2.5   # 2.5mm tall perimeter stiffener flange (total height = 6.5mm)
    
    # 1. Main Solid Base Plate (167.0mm x 56.5mm)
    p_brk = make_rounded_rect_poly(167.0, 56.5, 3.0)
    bracket = trimesh.creation.extrude_polygon(p_brk, height=t_base)
    
    # 2. Perimeter Structural Box-Truss Rib (Stiffens bracket against any flex or breakage!)
    p_rib_outer = make_rounded_rect_poly(167.0, 56.5, 3.0)
    p_rib_inner = make_rounded_rect_poly(162.0, 51.5, 2.0)
    rib_poly = p_rib_outer.difference(p_rib_inner)
    rib_mesh = trimesh.creation.extrude_polygon(rib_poly, height=h_rib)
    rib_mesh.apply_translation([0, 0, t_base])
    bracket = bracket.union(rib_mesh)
    
    # 3. Heavy-Duty Central Backbone Spine (Width = 22mm, Height = 6.5mm across X = -65mm to +65mm)
    spine = trimesh.creation.box(extents=[130.0, 20.0, h_rib])
    spine.apply_translation([0, 0, t_base + h_rib/2.0])
    bracket = bracket.union(spine)
    
    # 4. Reinforced Cylindrical Bosses around the 2 Factory M3 Screws (Dia 14mm)
    for xs in [x_screw_l, x_screw_r]:
        sboss = trimesh.creation.cylinder(radius=6.5, height=h_rib, sections=32)
        sboss.apply_translation([xs, y_screw, t_base + h_rib/2.0])
        bracket = bracket.union(sboss)
        
        # M3 clearance hole (dia 3.4mm) through the entire 6.5mm thickness
        shole = trimesh.creation.cylinder(radius=1.7, height=t_base + h_rib + 2.0, sections=24)
        shole.apply_translation([xs, y_screw, (t_base + h_rib)/2.0])
        bracket = bracket.difference(shole)
        
        # M3 socket/pan head counterbore (dia 6.5mm, depth 3.5mm from top)
        scb = trimesh.creation.cylinder(radius=3.25, height=3.6, sections=24)
        scb.apply_translation([xs, y_screw, t_base + h_rib - 1.7])
        bracket = bracket.difference(scb)
        
    # 5. REUSABLE SPRING-ACTION COMPLIANT LATCHES (Spring-Mode Fit with Pinch-to-Release)
    # Replaces rigid snap tabs with elastic cantilever spring arms!
    # Features relief flexure slots, ramped engagement teeth, and raised thumb/finger pinch release pads.
    
    # 2 Spring Latches on Top Edge (X = +/-28.0mm)
    for sx in snap_x_top:
        # A. Cantilever Spring Tooth (Ramped catch tooth, protrusion = 1.3mm into bezel detent)
        tooth = trimesh.creation.box(extents=[snap_w, snap_h * 2, 3.5])
        tooth.apply_translation([sx, 56.5/2.0 + snap_h/2.0, t_base / 2.0])
        bracket = bracket.union(tooth)
        
        # B. Flexure Relief Slot (Width = 2.0mm, Length = 22.0mm behind the latch)
        # Gives the 1.8mm cantilever arm room to bend inward elastically when pushed or pinched
        slot = trimesh.creation.box(extents=[22.0, 2.0, t_base + h_rib + 2.0])
        slot.apply_translation([sx, 56.5/2.0 - 2.8, (t_base + h_rib)/2.0])
        bracket = bracket.difference(slot)
        
        # C. Ergonomic Finger-Pinch Release Pad (Raised by 2.0mm on top of the spring arm)
        fpad = trimesh.creation.box(extents=[snap_w, 1.8, 2.0])
        fpad.apply_translation([sx, 56.5/2.0 - 0.9, t_base + 1.0])
        bracket = bracket.union(fpad)
        
    # 2 Spring Latches on Bottom Edge (X = +/-12.0mm)
    for sx in snap_x_bot:
        # A. Cantilever Spring Tooth
        tooth = trimesh.creation.box(extents=[snap_w, snap_h * 2, 3.5])
        tooth.apply_translation([sx, -56.5/2.0 - snap_h/2.0, t_base / 2.0])
        bracket = bracket.union(tooth)
        
        # B. Flexure Relief Slot
        slot = trimesh.creation.box(extents=[22.0, 2.0, t_base + h_rib + 2.0])
        slot.apply_translation([sx, -56.5/2.0 + 2.8, (t_base + h_rib)/2.0])
        bracket = bracket.difference(slot)
        
        # C. Ergonomic Finger-Pinch Release Pad
        fpad = trimesh.creation.box(extents=[snap_w, 1.8, 2.0])
        fpad.apply_translation([sx, -56.5/2.0 + 0.9, t_base + 1.0])
        bracket = bracket.union(fpad)

    # 5B. Dual Finger-Pry / Lift Extraction Notches on Left and Right Ends
    for ex in [-62.0, 62.0]:
        lift_notch = trimesh.creation.box(extents=[8.0, 6.0, 2.0])
        lift_notch.apply_translation([ex, 0.0, t_base + h_rib - 1.0])
        bracket = bracket.difference(lift_notch)
        
    # 6. Clearance Radius Notches in all 4 Corners for the Front Bezel Corner Screw Posts
    for mx in [-1, 1]:
        for my in [-1, 1]:
            cx = mx * x_corner
            cy = my * y_corner
            c_notch = trimesh.creation.cylinder(radius=5.5, height=t_base + h_rib + 2.0, sections=24)
            c_notch.apply_translation([cx, cy, (t_base + h_rib)/2.0])
            bracket = bracket.difference(c_notch)
            
    # 7. Filleted Component Windows (Leaves massive structural web everywhere!)
    # Electronics Window (ESP32-S3 / PSRAM)
    c_win = trimesh.creation.box(extents=[55.0, 30.0, t_base + 2.0])
    c_win.apply_translation([0.0, 0.0, t_base/2.0])
    bracket = bracket.difference(c_win)
    
    # K1/K2 Rear Button Clearance Window (X = 47.33mm, Y = 23.27mm)
    btn_win = trimesh.creation.box(extents=[26.0, 12.0, t_base + 2.0])
    btn_win.apply_translation([47.33, 23.27, t_base/2.0])
    bracket = bracket.difference(btn_win)
    
    # Battery Connector & USB-C Area Window
    bat_win = trimesh.creation.box(extents=[25.0, 14.0, t_base + 2.0])
    bat_win.apply_translation([45.0, -20.0, t_base/2.0])
    bracket = bracket.difference(bat_win)
    
    # MicroSD Slot Area Window
    sd_win = trimesh.creation.box(extents=[22.0, 14.0, t_base + 2.0])
    sd_win.apply_translation([-40.0, -20.0, t_base/2.0])
    bracket = bracket.difference(sd_win)
    
    # Left Side Jog Dial Window
    dial_win = trimesh.creation.box(extents=[14.0, 32.0, t_base + 2.0])
    dial_win.apply_translation([-76.0, 0.0, t_base/2.0])
    bracket = bracket.difference(dial_win)
    
    print("Mounting Bracket compiled. Watertight:", bracket.is_watertight, "Volume:", bracket.volume)
    return bracket

# =============================================================================
# 4. GENERATE REAR CABINET (SHALLOW SPEAKER RIM, CLEAN BATTERY BAY, NON-MERGING KEYHOLES)
# =============================================================================
def generate_rear_cabinet():
    print("Generating CrowPanel 5.79 Rear Cabinet (Clean, non-merging, single corner screws)...")
    p_outer = make_rounded_rect_poly(W_outer, H_outer, R_corner)
    rear = trimesh.creation.extrude_polygon(p_outer, height=D_rear)
    
    # 1. Main Interior Cavity (Wall = 2.2mm)
    p_cavity = make_rounded_rect_poly(W_outer - Wall*2, H_outer - Wall*2, R_corner - 0.8)
    cavity = trimesh.creation.extrude_polygon(p_cavity, height=D_rear - Wall + 1.0)
    cavity.apply_translation([0, 0, Wall])
    rear = rear.difference(cavity)
    
    # 2. 40mm Round Speaker: SHALLOW RETENTION RIM ONLY (Height = 2.5mm, Wall = 1.5mm)
    spk_rim = trimesh.creation.cylinder(radius=r_spk + 1.5, height=h_spk_rim, sections=48)
    spk_rim.apply_translation([x_spk, y_spk, Wall + h_spk_rim/2.0])
    rear = rear.union(spk_rim)
    
    spk_rim_inner = trimesh.creation.cylinder(radius=r_spk, height=h_spk_rim + 1.0, sections=48)
    spk_rim_inner.apply_translation([x_spk, y_spk, Wall + (h_spk_rim + 1.0)/2.0])
    rear = rear.difference(spk_rim_inner)
    
    # 3. Rear Speaker Acoustic Grille Slits (Concentric rings)
    grille_holes = []
    c_hole = trimesh.creation.cylinder(radius=2.5, height=Wall + 2.0, sections=24)
    c_hole.apply_translation([x_spk, y_spk, (Wall + 2.0)/2.0 - 1.0])
    grille_holes.append(c_hole)
    
    for r_ring in [8.0, 14.0, 20.0, 26.0, 32.0]:
        n_vents = int(r_ring * 0.85)
        for i in range(n_vents):
            th = i * (2 * np.pi / n_vents)
            vx = x_spk + (r_ring/2.0) * np.cos(th)
            vy = y_spk + (r_ring/2.0) * np.sin(th)
            v_hole = trimesh.creation.cylinder(radius=0.9, height=Wall + 2.0, sections=16)
            v_hole.apply_translation([vx, vy, (Wall + 2.0)/2.0 - 1.0])
            grille_holes.append(v_hole)
            
    grille_mesh = trimesh.util.concatenate(grille_holes)
    rear = rear.difference(grille_mesh)
    
    # 4. REAR BOOT & RESET BUTTON ACCESS & RIGID GUIDE TUBES
    for bx in [x_boot, x_reset]:
        b_guide = trimesh.creation.cylinder(radius=4.5, height=10.0, sections=32)
        b_guide.apply_translation([bx, y_btn, Wall + 5.0])
        rear = rear.union(b_guide)

        b_bore = trimesh.creation.cylinder(radius=2.6, height=Wall + 14.0, sections=24)
        b_bore.apply_translation([bx, y_btn, (Wall + 14.0)/2.0 - 1.0])
        rear = rear.difference(b_bore)
        
        b_well = trimesh.creation.cylinder(radius=3.9, height=1.6, sections=32)
        b_well.apply_translation([bx, y_btn, 0.6])
        rear = rear.difference(b_well)
        
        b_inner = trimesh.creation.cylinder(radius=3.6, height=2.5, sections=32)
        b_inner.apply_translation([bx, y_btn, Wall + 1.25])
        rear = rear.difference(b_inner)
        
    # 5. DUAL 18650 BATTERY CRADLE (Cleanly in Right Half: X = 41.0, Y = -6.0)
    # ZERO overlap with speaker! (Gap = 6.75mm, Inner length = 72mm, Inner width = 40mm)
    b_cradle = trimesh.creation.box(extents=[w_bat_bay + 3.0, h_bat_bay + 3.0, d_bat_bay])
    b_cradle.apply_translation([x_bat_center, y_bat_center, Wall + d_bat_bay/2.0])
    rear = rear.union(b_cradle)
    
    b_cav = trimesh.creation.box(extents=[w_bat_bay, h_bat_bay, d_bat_bay + 2.0])
    b_cav.apply_translation([x_bat_center, y_bat_center, Wall + (d_bat_bay + 2.0)/2.0])
    rear = rear.difference(b_cav)
    
    b_div = trimesh.creation.box(extents=[w_bat_bay, 1.8, d_bat_bay])
    b_div.apply_translation([x_bat_center, y_bat_center, Wall + d_bat_bay/2.0])
    rear = rear.union(b_div)
    
    # Wire routing exit notch at top of battery cradle
    b_wire = trimesh.creation.box(extents=[14.0, 4.0, d_bat_bay + 2.0])
    b_wire.apply_translation([x_bat_center, y_bat_center + h_bat_bay/2.0, Wall + (d_bat_bay + 2.0)/2.0])
    rear = rear.difference(b_wire)
    
    # 6. MAX98357A I2S DAC Breakout Retention Rails (Far Left)
    amp_rail1 = trimesh.creation.box(extents=[2.0, 20.0, 6.0])
    amp_rail1.apply_translation([x_amp - 10.0, y_amp, Wall + 3.0])
    amp_rail2 = trimesh.creation.box(extents=[2.0, 20.0, 6.0])
    amp_rail2.apply_translation([x_amp + 10.0, y_amp, Wall + 3.0])
    rear = rear.union(amp_rail1)
    rear = rear.union(amp_rail2)
    
    # 7. DUAL WALL KEYHOLE SLOTS (Cleanly moved: X = -65.0mm & X = +68.0mm, Y = +14.0mm)
    for kx in [x_kh_l, x_kh_r]:
        k_entry = trimesh.creation.cylinder(radius=3.75, height=Wall + 2.0, sections=32)
        k_entry.apply_translation([kx, y_kh, (Wall + 2.0)/2.0 - 1.0])
        rear = rear.difference(k_entry)
        
        k_slot = trimesh.creation.box(extents=[3.8, 6.5, Wall + 2.0])
        k_slot.apply_translation([kx, y_kh + 3.25, (Wall + 2.0)/2.0 - 1.0])
        rear = rear.difference(k_slot)
        
    # 8. EXACTLY 1 COUNTERBORED SCREW HOLE PER CORNER (M2.5/M3)
    for mx in [-1, 1]:
        for my in [-1, 1]:
            cx = mx * x_corner
            cy = my * y_corner
            c_shank = trimesh.creation.cylinder(radius=1.6, height=D_rear + 4.0, sections=24)
            c_shank.apply_translation([cx, cy, D_rear / 2.0])
            rear = rear.difference(c_shank)
            
            c_head = trimesh.creation.cylinder(radius=3.0, height=4.5, sections=24)
            c_head.apply_translation([cx, cy, 2.25 - 0.5])
            rear = rear.difference(c_head)
            
    # 9. Bottom Edge USB-C & MicroSD Matching Reliefs
    usbc_relief = trimesh.creation.box(extents=[13.0, 10.0, 5.0])
    usbc_relief.apply_translation([x_usbc, -H_outer/2.0 + 3.0, D_rear - 2.5])
    rear = rear.difference(usbc_relief)
    
    sd_relief = trimesh.creation.box(extents=[15.5, 10.0, 5.0])
    sd_relief.apply_translation([x_sd, -H_outer/2.0 + 3.0, D_rear - 2.5])
    rear = rear.difference(sd_relief)
    
    # 10. Desktop Non-Slip Foot Pad Recesses
    for mx in [-1, 1]:
        foot = trimesh.creation.box(extents=[16.0, 4.0, 2.0])
        foot.apply_translation([mx * (W_outer/2.0 - 20.0), -H_outer/2.0 + 2.5, 1.0])
        rear = rear.difference(foot)
        
    print("Rear Cabinet compiled. Watertight:", rear.is_watertight, "Volume:", rear.volume)
    return rear

# =============================================================================
# 5. GENERATE CAPTIVE REAR PUSH-BUTTON PLUNGERS (REINFORCED & EXTENDED)
# =============================================================================
def generate_button_caps():
    print("Generating CrowPanel 5.79 Rear Button Plungers (Boot & Reset)...")
    b1_cap = trimesh.creation.cylinder(radius=2.3, height=2.2, sections=32)
    b1_cap.apply_translation([0, 0, 1.1])
    
    b1_flange = trimesh.creation.cylinder(radius=3.4, height=1.5, sections=32)
    b1_flange.apply_translation([0, 0, 2.2 + 0.75])
    
    # Stiff guide stem (dia 3.6mm, length 16mm)
    b1_stem = trimesh.creation.cylinder(radius=1.8, height=16.0, sections=24)
    b1_stem.apply_translation([0, 0, 3.7 + 8.0])
    
    # Tactile switch contact tip (dia 2.2mm, length 5.5mm)
    b1_tip = trimesh.creation.cylinder(radius=1.1, height=5.5, sections=24)
    b1_tip.apply_translation([0, 0, 19.7 + 2.75])
    
    btn1 = b1_cap.union(b1_flange).union(b1_stem).union(b1_tip)
    
    btn2 = btn1.copy()
    btn1.apply_translation([-6.0, 0, 0])
    btn2.apply_translation([ 6.0, 0, 0])
    
    btns = btn1.union(btn2)
    print("Button Plungers compiled. Watertight:", btns.is_watertight, "Volume:", btns.volume)
    return btns

# =============================================================================
# 6. MAIN COMPILATION & EXPORT
# =============================================================================
if __name__ == '__main__':
    out_dir = os.path.dirname(os.path.abspath(__file__))
    
    bezel = generate_front_bezel()
    bracket = generate_mounting_bracket()
    rear = generate_rear_cabinet()
    buttons = generate_button_caps()
    
    path_bezel   = os.path.join(out_dir, "CrowPanel_5.79_Front_Bezel.stl")
    path_bracket = os.path.join(out_dir, "CrowPanel_5.79_Mounting_Bracket.stl")
    path_rear    = os.path.join(out_dir, "CrowPanel_5.79_Rear_Cabinet.stl")
    path_buttons = os.path.join(out_dir, "CrowPanel_5.79_Button_Caps.stl")
    
    bezel.export(path_bezel)
    bracket.export(path_bracket)
    rear.export(path_rear)
    buttons.export(path_buttons)
    
    print("\n[SUCCESS] All 4 Production STLs Exported:")
    print(" 1. Front Bezel (Single Boss per Corner): ", path_bezel,   f"({os.path.getsize(path_bezel):,} bytes) [Watertight: {bezel.is_watertight}]")
    print(" 2. Reinforced Snap-Fit Bracket:         ", path_bracket, f"({os.path.getsize(path_bracket):,} bytes) [Watertight: {bracket.is_watertight}]")
    print(" 3. Rear Cabinet:                         ", path_rear,    f"({os.path.getsize(path_rear):,} bytes) [Watertight: {rear.is_watertight}]")
    print(" 4. Button Plungers:                      ", path_buttons, f"({os.path.getsize(path_buttons):,} bytes) [Watertight: {buttons.is_watertight}]")
