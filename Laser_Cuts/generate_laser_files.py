"""
Laser Cutting File Generator for Creality Falcon 5W
Generates:
1. DXF (AutoCAD R12 compatible standard ASCII format)
2. SVG (1:1 scale with mm units for LightBurn and LaserGRBL)
3. G-Code (GRBL M3/M5 tailored for Creality Falcon 5W 72W/5W Diode Laser)
4. PDF (True 1:1 scale printable test-fit template with reportlab)
5. Visual Verification Overlays
"""

import os
import math
import cv2
import numpy as np

OUTPUT_DIR = "d:/ESP32Radio/3D_Enclosure_JC3248W535"

# ==============================================================================
# PHYSICAL MEASUREMENTS (in Millimeters)
# ==============================================================================
DOOR_W = 153.0
DOOR_H = 90.0
DOOR_CORNER_R = 8.0

# Air holes (Vent grille) reference bounds:
AIR_HOLES_X1 = 69.3
AIR_HOLES_X2 = 125.4
AIR_HOLES_Y1 = 46.2
AIR_HOLES_Y2 = 65.5
AIR_HOLES_CX = (AIR_HOLES_X1 + AIR_HOLES_X2) / 2.0  # 97.35 mm
AIR_HOLES_CY = (AIR_HOLES_Y1 + AIR_HOLES_Y2) / 2.0  # 55.85 mm

# Bottom screw bosses (outer boss diameter ~8mm, hole dia ~3mm):
BOTTOM_SCREW_HOLES = [
    {"name": "Left Boss", "x": 53.7, "y": 81.3, "boss_r": 4.0, "hole_r": 1.5},
    {"name": "Middle Boss", "x": 98.6, "y": 82.5, "boss_r": 4.0, "hole_r": 1.5},
    {"name": "Right Corner Boss", "x": 144.7, "y": 84.4, "boss_r": 4.0, "hole_r": 1.5},
    {"name": "Left Corner Boss", "x": 5.4, "y": 79.8, "boss_r": 4.0, "hole_r": 1.5},
]

# Top edge label safe boundary:
TOP_LABEL_BOTTOM_Y = 16.0

# Display mounting screw hole pitch:
DISPLAY_HOLE_PITCH_X = 84.3
DISPLAY_HOLE_PITCH_Y = 52.3

# ==============================================================================
# CUTOUT PROFILES
# ==============================================================================
# Option 1 (Full Glass Cutout): 96.0 x 63.0 mm (R = 3.0 mm)
# Shifted so bottom is at Y = 74.5 mm, leaving 4.0 mm above middle screw boss.
# Option 2 (Flush Rear Body): 85.0 x 56.0 mm (R = 2.5 mm)
# Bottom at Y = 73.5 mm, leaving 5.0 mm above boss; top at Y = 17.5 mm (cleanly preserves labels).
# Option 3 (Screen Viewing Window): 75.0 x 51.0 mm (R = 1.5 mm) + 4 Corner Screws (dia 2.6 mm)
# Center at X = 97.5 mm, Y = 47.5 mm.

PROFILES = [
    {
        "id": "full_glass_96x63",
        "name": "Full Display Glass Cutout (96 x 63 mm)",
        "w": 96.0,
        "h": 63.0,
        "corner_r": 3.0,
        "cx": 95.0,
        "cy": 43.0,  # y_min = 11.5 mm, y_max = 74.5 mm
        "x_min": 47.0,
        "x_max": 143.0,
        "y_min": 11.5,
        "y_max": 74.5,
        "holes": False,
        "desc": "Drop-in cutout for entire 94.5x62mm glass module. 4.0mm clearance above bottom screw boss."
    },
    {
        "id": "flush_body_85x56",
        "name": "Flush Rear Body Cutout (85 x 56 mm)",
        "w": 85.0,
        "h": 56.0,
        "corner_r": 2.5,
        "cx": 97.5,
        "cy": 45.5,
        "x_min": 55.0,
        "x_max": 140.0,
        "y_min": 17.5,
        "y_max": 73.5,
        "holes": False,
        "desc": "Housing body pass-through. Leaves 5.0mm above screw boss, preserves top labels completely."
    },
    {
        "id": "screen_window_75x51",
        "name": "Active Screen Window (75 x 51 mm) + 4 Screws",
        "w": 75.0,
        "h": 51.0,
        "corner_r": 1.5,
        "cx": 97.5,
        "cy": 47.5,
        "x_min": 60.0,
        "x_max": 135.0,
        "y_min": 22.0,
        "y_max": 73.0,
        "holes": True,
        "desc": "Display mounted on INSIDE face. 75x51mm screen aperture + 4 corner pilot holes (pitch 84.3x52.3mm)."
    }
]

# ==============================================================================
# DXF GENERATION (AutoCAD R12 ASCII)
# In DXF standard: Y points UP.
# For laser cutting, origin is at bottom-left of door (X=0..153, Y=0..90).
# In our door coordinate system (where top is Y=0, bottom is Y=90),
# we map: Y_dxf = 90.0 - Y_door.
# ==============================================================================

def make_dxf(profile, include_door_ref=True):
    entities = []

    # Map function
    def my(y):
        return DOOR_H - y

    # Layer 1: Door outline reference (Gray / Cyan)
    if include_door_ref:
        # Door outer rounded rectangle: (0, 0) to (153, 90)
        # In DXF coords: X in [0..153], Y in [0..90]
        entities.extend(dxf_rounded_rect(0, 0, DOOR_W, DOOR_H, DOOR_CORNER_R, layer="DOOR_OUTLINE"))
        
        # Reference air vents (3 horizontal slots)
        for vy1, vy2 in [(46.2, 50.2), (53.8, 57.8), (61.5, 65.5)]:
            vy_dxf_bot = my(vy2)
            vy_dxf_top = my(vy1)
            entities.extend(dxf_rounded_rect(AIR_HOLES_X1, vy_dxf_bot, AIR_HOLES_X2, vy_dxf_top, 1.5, layer="REF_AIR_HOLES"))

        # Reference bottom screw holes & bosses
        for b in BOTTOM_SCREW_HOLES:
            by_dxf = my(b["y"])
            entities.extend(dxf_circle(b["x"], by_dxf, b["boss_r"], layer="REF_SCREW_BOSSES"))
            entities.extend(dxf_circle(b["x"], by_dxf, b["hole_r"], layer="REF_SCREW_HOLES"))

    # Layer 2: Laser Cut Outline (Red)
    # Cutout bounds in door coords: [x_min..x_max], [y_min..y_max]
    # In DXF coords: Y_bot = my(y_max), Y_top = my(y_min)
    cut_y_bot = my(profile["y_max"])
    cut_y_top = my(profile["y_min"])
    entities.extend(dxf_rounded_rect(profile["x_min"], cut_y_bot, profile["x_max"], cut_y_top, profile["corner_r"], layer="CUT_OUTLINE"))

    # Layer 3: Corner Pilot Holes if applicable (Blue)
    if profile.get("holes"):
        cx = profile["cx"]
        cy = profile["cy"]
        for sx in [-1, 1]:
            for sy in [-1, 1]:
                hx = cx + sx * (DISPLAY_HOLE_PITCH_X / 2.0)
                hy_door = cy + sy * (DISPLAY_HOLE_PITCH_Y / 2.0)
                hy_dxf = my(hy_door)
                entities.extend(dxf_circle(hx, hy_dxf, 1.3, layer="PILOT_HOLES"))  # dia 2.6mm

    return dxf_format_r12(entities)

def dxf_format_r12(entities):
    header = [
        "0", "SECTION",
        "2", "HEADER",
        "9", "$ACADVER",
        "1", "AC1009",
        "9", "$INSUNITS",
        "70", "4",  # Millimeters
        "0", "ENDSEC",
        "0", "SECTION",
        "2", "TABLES",
        "0", "TABLE",
        "2", "LTYPE",
        "70", "1",
        "0", "LTYPE",
        "2", "CONTINUOUS",
        "70", "64",
        "3", "Solid line",
        "72", "65",
        "73", "0",
        "40", "0.0",
        "0", "ENDTAB",
        "0", "TABLE",
        "2", "LAYER",
        "70", "5",
        "0", "LAYER",
        "2", "CUT_OUTLINE",
        "70", "64",
        "62", "1",  # 1 = Red
        "6", "CONTINUOUS",
        "0", "LAYER",
        "2", "PILOT_HOLES",
        "70", "64",
        "62", "5",  # 5 = Blue
        "6", "CONTINUOUS",
        "0", "LAYER",
        "2", "DOOR_OUTLINE",
        "70", "64",
        "62", "7",  # 7 = White/Black
        "6", "CONTINUOUS",
        "0", "LAYER",
        "2", "REF_AIR_HOLES",
        "70", "64",
        "62", "8",  # 8 = Dark Gray
        "6", "CONTINUOUS",
        "0", "LAYER",
        "2", "REF_SCREW_BOSSES",
        "70", "64",
        "62", "4",  # 4 = Cyan
        "6", "CONTINUOUS",
        "0", "ENDTAB",
        "0", "ENDSEC",
        "0", "SECTION",
        "2", "ENTITIES",
    ]
    footer = ["0", "ENDSEC", "0", "EOF"]
    return "\n".join(header + entities + footer) + "\n"

def dxf_line(x1, y1, x2, y2, layer):
    return [
        "0", "LINE",
        "8", layer,
        "10", f"{x1:.4f}",
        "20", f"{y1:.4f}",
        "30", "0.0",
        "11", f"{x2:.4f}",
        "21", f"{y2:.4f}",
        "31", "0.0",
    ]

def dxf_circle(cx, cy, r, layer):
    return [
        "0", "CIRCLE",
        "8", layer,
        "10", f"{cx:.4f}",
        "20", f"{cy:.4f}",
        "30", "0.0",
        "40", f"{r:.4f}",
    ]

def dxf_arc(cx, cy, r, a1, a2, layer):
    return [
        "0", "ARC",
        "8", layer,
        "10", f"{cx:.4f}",
        "20", f"{cy:.4f}",
        "30", "0.0",
        "40", f"{r:.4f}",
        "50", f"{a1:.2f}",
        "51", f"{a2:.2f}",
    ]

def dxf_rounded_rect(x1, y1, x2, y2, r, layer):
    """x1, y1 is bottom-left; x2, y2 is top-right."""
    ent = []
    # 4 straight segments
    ent.extend(dxf_line(x1 + r, y1, x2 - r, y1, layer))
    ent.extend(dxf_line(x2, y1 + r, x2, y2 - r, layer))
    ent.extend(dxf_line(x2 - r, y2, x1 + r, y2, layer))
    ent.extend(dxf_line(x1, y2 - r, x1, y1 + r, layer))
    # 4 arcs (AutoCAD R12 angles: 0 is +X, CCW)
    ent.extend(dxf_arc(x2 - r, y1 + r, r, 270, 360, layer))
    ent.extend(dxf_arc(x2 - r, y2 - r, r, 0, 90, layer))
    ent.extend(dxf_arc(x1 + r, y2 - r, r, 90, 180, layer))
    ent.extend(dxf_arc(x1 + r, y1 + r, r, 180, 270, layer))
    return ent

# ==============================================================================
# SVG GENERATION (1:1 Scale with mm units for LightBurn & LaserGRBL)
# ==============================================================================

def make_svg(profile, include_door_ref=True):
    # In SVG, origin (0, 0) is top-left, matching door coordinates directly!
    svg = []
    svg.append('<?xml version="1.0" encoding="UTF-8" standalone="no"?>')
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{DOOR_W}mm" height="{DOOR_H}mm" viewBox="0 0 {DOOR_W} {DOOR_H}">')
    svg.append('  <title>ESP32-S3 JC3248W535 Display Laser Cutout Template</title>')
    svg.append('  <desc>Precision Laser Cutting File for Creality Falcon 5W</desc>')
    svg.append('  <defs>')
    svg.append('    <style type="text/css">')
    svg.append('      .door-outline { fill: none; stroke: #94a3b8; stroke-width: 0.35; stroke-dasharray: 2,1; }')
    svg.append('      .ref-vent { fill: none; stroke: #cbd5e1; stroke-width: 0.25; }')
    svg.append('      .ref-boss { fill: none; stroke: #0284c7; stroke-width: 0.25; }')
    svg.append('      .laser-cut { fill: none; stroke: #ef4444; stroke-width: 0.2; }')
    svg.append('      .pilot-hole { fill: none; stroke: #3b82f6; stroke-width: 0.2; }')
    svg.append('      .text-label { font-family: sans-serif; font-size: 2.2px; fill: #64748b; }')
    svg.append('    </style>')
    svg.append('  </defs>')

    # 1. Door reference geometry
    if include_door_ref:
        svg.append('  <g id="DOOR_REFERENCE">')
        # Door outer boundary
        svg.append(f'    <rect class="door-outline" x="0" y="0" width="{DOOR_W}" height="{DOOR_H}" rx="{DOOR_CORNER_R}" ry="{DOOR_CORNER_R}" />')
        
        # Reference air holes
        for vy1, vy2 in [(46.2, 50.2), (53.8, 57.8), (61.5, 65.5)]:
            svg.append(f'    <rect class="ref-vent" x="{AIR_HOLES_X1}" y="{vy1}" width="{AIR_HOLES_X2 - AIR_HOLES_X1}" height="{vy2 - vy1}" rx="1.5" ry="1.5" />')
            
        # Reference screw holes & bosses
        for b in BOTTOM_SCREW_HOLES:
            svg.append(f'    <circle class="ref-boss" cx="{b["x"]}" cy="{b["y"]}" r="{b["boss_r"]}" />')
            svg.append(f'    <circle class="ref-boss" cx="{b["x"]}" cy="{b["y"]}" r="{b["hole_r"]}" />')
            
        # Text annotations
        svg.append(f'    <text class="text-label" x="5" y="10">DOOR: 153.0 x 90.0 mm</text>')
        svg.append(f'    <text class="text-label" x="5" y="14">TOP CONTROLS SAFE ZONE: Y=0..16mm</text>')
        svg.append('  </g>')

    # 2. Main Laser Cutout Path (Red)
    svg.append('  <g id="CUT_LAYER">')
    px = profile["x_min"]
    py = profile["y_min"]
    pw = profile["w"]
    ph = profile["h"]
    pr = profile["corner_r"]
    svg.append(f'    <rect id="display_cutout" class="laser-cut" x="{px}" y="{py}" width="{pw}" height="{ph}" rx="{pr}" ry="{pr}" />')
    svg.append('  </g>')

    # 3. Corner Pilot Holes (Blue)
    if profile.get("holes"):
        svg.append('  <g id="PILOT_HOLES_LAYER">')
        cx = profile["cx"]
        cy = profile["cy"]
        for sx in [-1, 1]:
            for sy in [-1, 1]:
                hx = cx + sx * (DISPLAY_HOLE_PITCH_X / 2.0)
                hy = cy + sy * (DISPLAY_HOLE_PITCH_Y / 2.0)
                svg.append(f'    <circle class="pilot-hole" cx="{hx:.3f}" cy="{hy:.3f}" r="1.3" />')
        svg.append('  </g>')

    svg.append('</svg>')
    return "\n".join(svg) + "\n"

# ==============================================================================
# G-CODE GENERATION FOR CREALITY FALCON 5W
# Creality Falcon 5W Specs:
# - Working area: 400 x 415 mm
# - Optical Laser Power: 5W (diode 450nm)
# - Max Speed: 10000 mm/min
# - Firmware: GRBL 1.1f compatible
# - Offline usage: reads .gcode / .nc files directly from TF / MicroSD card
# ==============================================================================

def make_gcode(profile, speed=400, power_pct=90, passes=4, frame_speed=1200):
    """
    Generates standard GRBL G-code for Creality Falcon 5W.
    Origin (0,0): Top-Left Corner of the Door plate.
    Power range: S0 to S1000 (S900 = 90% power).
    Cutting path: rounded rectangle approximated by line segments and G2/G3 arcs.
    """
    s_val = int(power_pct * 10)  # S900 for 90%
    x1 = profile["x_min"]
    x2 = profile["x_max"]
    y1 = profile["y_min"]
    y2 = profile["y_max"]
    r = profile["corner_r"]

    gcode = []
    gcode.append("; ========================================================")
    gcode.append(f"; Creality Falcon 5W Laser G-Code")
    gcode.append(f"; Cutout Profile: {profile['name']}")
    gcode.append(f"; Cut Dimensions: {profile['w']} x {profile['h']} mm (R={r}mm)")
    gcode.append(f"; Speed: {speed} mm/min | Laser Power: {power_pct}% (S{s_val}) | Passes: {passes}")
    gcode.append(f"; Origin: (0,0) at Top-Left Corner of Door Plate (153 x 90 mm)")
    gcode.append("; ========================================================")
    gcode.append("G21          ; Set units to millimeters")
    gcode.append("G90          ; Absolute positioning")
    gcode.append("M5           ; Ensure laser is OFF")
    gcode.append("G0 Z0 F1000  ; Home/Neutral Z")
    gcode.append("")
    gcode.append("; --- PHASE 1: FRAMING PASS (Visual alignment at 0.5% power) ---")
    gcode.append("; The laser dot will illuminate the boundary box so you can verify positioning")
    gcode.append(f"G0 X{x1} Y{y1} F{frame_speed}")
    gcode.append("M3 S5        ; Low power pilot beam (0.5%)")
    gcode.append(f"G1 X{x2} Y{y1} F{frame_speed}")
    gcode.append(f"G1 X{x2} Y{y2} F{frame_speed}")
    gcode.append(f"G1 X{x1} Y{y2} F{frame_speed}")
    gcode.append(f"G1 X{x1} Y{y1} F{frame_speed}")
    gcode.append("M5           ; Laser OFF after framing")
    gcode.append("G4 P1.0      ; Pause 1 second for visual check")
    gcode.append("")
    gcode.append("; --- PHASE 2: CUTTING PASSES ---")

    for p in range(1, passes + 1):
        gcode.append(f"; --- Pass {p} of {passes} ---")
        # Start at top edge after corner round: (x1 + r, y1)
        gcode.append(f"G0 X{x1 + r:.3f} Y{y1:.3f} F{speed}")
        gcode.append(f"M3 S{s_val}   ; Laser ON at {power_pct}% power")
        
        # 1. Top edge -> top-right corner
        gcode.append(f"G1 X{x2 - r:.3f} Y{y1:.3f} F{speed}")
        gcode.append(f"G2 X{x2:.3f} Y{y1 + r:.3f} I0.0 J{r:.3f}")
        
        # 2. Right edge -> bottom-right corner
        gcode.append(f"G1 X{x2:.3f} Y{y2 - r:.3f}")
        gcode.append(f"G2 X{x2 - r:.3f} Y{y2:.3f} I{-r:.3f} J0.0")
        
        # 3. Bottom edge -> bottom-left corner
        gcode.append(f"G1 X{x1 + r:.3f} Y{y2:.3f}")
        gcode.append(f"G2 X{x1:.3f} Y{y2 - r:.3f} I0.0 J{-r:.3f}")
        
        # 4. Left edge -> top-left corner
        gcode.append(f"G1 X{x1:.3f} Y{y1 + r:.3f}")
        gcode.append(f"G2 X{x1 + r:.3f} Y{y1:.3f} I{r:.3f} J0.0")
        
        gcode.append("M5           ; Laser OFF at end of pass")
        gcode.append("G4 P0.2      ; Short cooldown pause (0.2s)")
        gcode.append("")

    # Corner pilot holes if applicable
    if profile.get("holes"):
        gcode.append("; --- PHASE 3: CORNER PILOT HOLES ---")
        cx = profile["cx"]
        cy = profile["cy"]
        for i, (sx, sy) in enumerate([(-1, -1), (1, -1), (1, 1), (-1, 1)]):
            hx = cx + sx * (DISPLAY_HOLE_PITCH_X / 2.0)
            hy = cy + sy * (DISPLAY_HOLE_PITCH_Y / 2.0)
            gcode.append(f"; Hole {i+1} at ({hx:.2f}, {hy:.2f})")
            gcode.append(f"G0 X{hx:.3f} Y{hy:.3f} F{speed}")
            gcode.append(f"M3 S{s_val}")
            # Small circular interpolation for dia 2.6mm hole (r=1.3)
            gcode.append(f"G2 X{hx:.3f} Y{hy:.3f} I1.3 J0.0 F{speed/2.0}")
            gcode.append("M5")
            gcode.append("")

    # End sequence
    gcode.append("; --- FINISH SEQUENCE ---")
    gcode.append("M5           ; Ensure laser is OFF")
    gcode.append("G0 X0 Y0 F2000 ; Return to origin (0,0)")
    gcode.append("M2           ; Program end")
    return "\n".join(gcode) + "\n"

# ==============================================================================
# TRUE 1:1 SCALE PRINTABLE PDF TEMPLATE (Using ReportLab)
# ==============================================================================

def make_pdf_template(filepath):
    from reportlab.lib.pagesizes import A4
    from reportlab.pdfgen import canvas
    from reportlab.lib import colors
    from reportlab.lib.units import mm

    c = canvas.Canvas(filepath, pagesize=A4)
    page_w, page_h = A4

    # Center the door drawing on A4 page
    # A4 is 210 x 297 mm
    margin_x = (page_w - DOOR_W * mm) / 2.0
    margin_y = (page_h - DOOR_H * mm) / 2.0 + 30 * mm

    # In reportlab: origin is bottom-left, units in points (1 mm = mm)
    # We map door coords: X_pdf = margin_x + x * mm, Y_pdf = margin_y + (DOOR_H - y) * mm
    def px(x):
        return margin_x + x * mm
    def py(y):
        return margin_y + (DOOR_H - y) * mm

    # Header / Title block
    c.setFont("Helvetica-Bold", 16)
    c.drawString(30 * mm, page_h - 25 * mm, "ESP32-S3 JC3248W535 Back Door Laser Cutting Template")
    c.setFont("Helvetica", 10)
    c.setFillColor(colors.HexColor("#475569"))
    c.drawString(30 * mm, page_h - 32 * mm, "True 1:1 Scale Printout - DO NOT FIT TO PAGE (Select 'Actual Size' / 100% Scale in Print Dialog)")
    c.drawString(30 * mm, page_h - 37 * mm, "Compatible with: Creality Falcon 5W Laser Engraver / Cutter (400x415mm Working Area)")

    # 100mm Calibration ruler to verify printer scaling
    ruler_y = page_h - 48 * mm
    c.setStrokeColor(colors.black)
    c.setLineWidth(1)
    c.line(30 * mm, ruler_y, 130 * mm, ruler_y)
    c.line(30 * mm, ruler_y - 3 * mm, 30 * mm, ruler_y + 3 * mm)
    c.line(130 * mm, ruler_y - 3 * mm, 130 * mm, ruler_y + 3 * mm)
    for tick in range(10, 100, 10):
        c.line((30 + tick) * mm, ruler_y - 1.5 * mm, (30 + tick) * mm, ruler_y + 1.5 * mm)
    c.setFont("Helvetica", 8)
    c.drawCentredString(80 * mm, ruler_y + 4 * mm, "<-- EXACT 100 mm SCALE CHECK (Measure with ruler before cutting) -->")

    # Draw Door Outer Contour (153 x 90 mm with R=8mm corners)
    c.setStrokeColor(colors.HexColor("#1e293b"))
    c.setLineWidth(1.5)
    c.roundRect(px(0), py(DOOR_H), DOOR_W * mm, DOOR_H * mm, DOOR_CORNER_R * mm)

    # Label door dimensions
    c.setFont("Helvetica-Bold", 9)
    c.setFillColor(colors.HexColor("#1e293b"))
    c.drawCentredString(px(DOOR_W / 2.0), py(-4), "153.0 mm (Door Width)")
    c.saveState()
    c.translate(px(-4), py(DOOR_H / 2.0))
    c.rotate(90)
    c.drawCentredString(0, 0, "90.0 mm (Door Height)")
    c.restoreState()

    # Draw Reference Air Vents
    c.setStrokeColor(colors.HexColor("#94a3b8"))
    c.setLineWidth(0.7)
    for vy1, vy2 in [(46.2, 50.2), (53.8, 57.8), (61.5, 65.5)]:
        c.roundRect(px(AIR_HOLES_X1), py(vy2), (AIR_HOLES_X2 - AIR_HOLES_X1) * mm, (vy2 - vy1) * mm, 1.5 * mm)
    c.setFont("Helvetica-Oblique", 7)
    c.setFillColor(colors.HexColor("#64748b"))
    c.drawString(px(AIR_HOLES_X1 + 2), py(AIR_HOLES_Y1 - 2), "Original Air Vents Grille")

    # Draw Bottom Screw Bosses
    for b in BOTTOM_SCREW_HOLES:
        c.setStrokeColor(colors.HexColor("#0284c7"))
        c.setLineWidth(0.8)
        c.circle(px(b["x"]), py(b["y"]), b["boss_r"] * mm)
        c.setFillColor(colors.HexColor("#0284c7"))
        c.circle(px(b["x"]), py(b["y"]), b["hole_r"] * mm, fill=1)
        c.setFont("Helvetica", 6)
        c.drawString(px(b["x"] - 5), py(b["y"] + 6), f"{b['name']}")

    # Draw Cutout Option 1 (Full Glass Cutout - Red Solid Line)
    p1 = PROFILES[0]
    c.setStrokeColor(colors.HexColor("#dc2626"))
    c.setLineWidth(1.4)
    c.roundRect(px(p1["x_min"]), py(p1["y_max"]), p1["w"] * mm, p1["h"] * mm, p1["corner_r"] * mm)
    c.setFont("Helvetica-Bold", 8)
    c.setFillColor(colors.HexColor("#dc2626"))
    c.drawString(px(p1["x_min"] + 3), py(p1["y_min"] + 4), f"Option 1: Full Glass Cutout ({p1['w']} x {p1['h']} mm)")

    # Draw Cutout Option 2 (Rear Body Cutout - Green Dashed Line)
    p2 = PROFILES[1]
    c.setStrokeColor(colors.HexColor("#16a34a"))
    c.setLineWidth(1.2)
    c.setDash(4, 2)
    c.roundRect(px(p2["x_min"]), py(p2["y_max"]), p2["w"] * mm, p2["h"] * mm, p2["corner_r"] * mm)
    c.setDash()  # Reset dash
    c.setFont("Helvetica-Bold", 8)
    c.setFillColor(colors.HexColor("#16a34a"))
    c.drawString(px(p2["x_min"] + 3), py(p2["y_min"] + 11), f"Option 2: Flush Body Cutout ({p2['w']} x {p2['h']} mm)")

    # Clearance Callouts
    c.setStrokeColor(colors.HexColor("#eab308"))
    c.setLineWidth(1.0)
    # Boss clearance line
    boss_top_y = 78.5
    c.line(px(98.6), py(p1["y_max"]), px(98.6), py(boss_top_y))
    c.setFont("Helvetica-Bold", 7)
    c.setFillColor(colors.HexColor("#ca8a04"))
    c.drawString(px(101.0), py(76.5), "4.0mm Solid Plastic Space Above Screw Boss")

    # Instruction Guide Box at Bottom of Page
    box_y = 20 * mm
    box_h = 45 * mm
    c.setStrokeColor(colors.HexColor("#cbd5e1"))
    c.setFillColor(colors.HexColor("#f8fafc"))
    c.rect(20 * mm, box_y, page_w - 40 * mm, box_h, fill=1)

    c.setFillColor(colors.HexColor("#0f172a"))
    c.setFont("Helvetica-Bold", 10)
    c.drawString(25 * mm, box_y + box_h - 7 * mm, "CREALITY FALCON 5W - STEP-BY-STEP LASER CUTTING PROCEDURE:")
    c.setFont("Helvetica", 8)
    lines = [
        "1. Paper Test-Fit: Cut out this template with scissors and lay it directly over your physical door to verify all clearances.",
        "2. Machine Bed Setup: Place the door flat on the honeycomb bed or aluminum slats. Use the Creality multi-level focus block to set laser height to focal point.",
        "3. Set Origin (0,0): Position the laser head dot directly over the TOP-LEFT corner of the door plate.",
        "4. Framing Verification: In LightBurn or via offline button (1 click), run the Framing pass. Ensure the laser stays 4mm above the bottom screw bosses.",
        "5. Recommended Cut Parameters: Diode Laser (5W, 450nm) on gold/bronze plastic: Speed 400 mm/min, Power 90%, 4 passes. Use air assist to prevent charring.",
    ]
    for i, line in enumerate(lines):
        c.drawString(25 * mm, box_y + box_h - (13 + i * 5.5) * mm, line)

    c.showPage()
    c.save()
    print(f"Generated 1:1 scale printable PDF: {filepath}")

# ==============================================================================
# MAIN EXECUTION
# ==============================================================================

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print("Generating laser cutting vector & G-code files...")

    # 1. Generate individual DXF and SVG files for each profile
    for p in PROFILES:
        pid = p["id"]
        
        # DXF
        dxf_content = make_dxf(p, include_door_ref=True)
        dxf_path = os.path.join(OUTPUT_DIR, f"CrealityFalcon_DisplayCutout_{pid}.dxf")
        with open(dxf_path, "w", encoding="utf-8") as f:
            f.write(dxf_content)
        print(f"Wrote DXF: {dxf_path}")

        # SVG
        svg_content = make_svg(p, include_door_ref=True)
        svg_path = os.path.join(OUTPUT_DIR, f"CrealityFalcon_DisplayCutout_{pid}.svg")
        with open(svg_path, "w", encoding="utf-8") as f:
            f.write(svg_content)
        print(f"Wrote SVG: {svg_path}")

        # G-Code (offline usage for Creality Falcon 5W)
        gcode_content = make_gcode(p, speed=400, power_pct=90, passes=4)
        gcode_path = os.path.join(OUTPUT_DIR, f"CrealityFalcon_DisplayCutout_{pid}.gcode")
        with open(gcode_path, "w", encoding="utf-8") as f:
            f.write(gcode_content)
        print(f"Wrote G-Code: {gcode_path}")

    # 2. Generate Master DXF and SVG containing all features
    master_svg_path = os.path.join(OUTPUT_DIR, "CrealityFalcon_SpeakerDoor_Master.svg")
    with open(master_svg_path, "w", encoding="utf-8") as f:
        f.write(make_svg(PROFILES[0], include_door_ref=True))

    # 3. Generate True 1:1 Scale Printable PDF
    pdf_path = os.path.join(OUTPUT_DIR, "CrealityFalcon_SpeakerDoor_Template_1to1.pdf")
    make_pdf_template(pdf_path)

    print("All laser files successfully generated!")

if __name__ == "__main__":
    main()
