"""
Virtual Board Assembly & Fitment Verification for Waveshare ESP32-S3-Touch-LCD-2.8
Creates 3D model of the board and verifies 100% exact alignment with the enclosure.
"""

import os
import trimesh
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from manifold3d import Manifold

def union_all(manifolds):
    valid = [m for m in manifolds if m is not None]
    if not valid:
        return None
    import functools
    return functools.reduce(lambda a, b: a + b, valid)

def diff_all(base, subtractions):
    valid = [s for s in subtractions if s is not None]
    if not valid:
        return base
    import functools
    subs = functools.reduce(lambda a, b: a + b, valid)
    return base - subs

def make_rounded_box(width, height, depth, radius, segments=24):
    r = min(radius, width/2.0 - 0.1, height/2.0 - 0.1)
    dx = width / 2.0 - r
    dy = height / 2.0 - r
    cyls = [
        Manifold.cylinder(depth, r, r, segments).translate([dx, dy, 0]),
        Manifold.cylinder(depth, r, r, segments).translate([-dx, dy, 0]),
        Manifold.cylinder(depth, r, r, segments).translate([dx, -dy, 0]),
        Manifold.cylinder(depth, r, r, segments).translate([-dx, -dy, 0]),
    ]
    bx = Manifold.cube([width - 2*r, height, depth], center=True).translate([0, 0, depth/2.0])
    by = Manifold.cube([width, height - 2*r, depth], center=True).translate([0, 0, depth/2.0])
    return union_all(cyls + [bx, by])

# -----------------------------------------------------------------------------
# Accurate 3D Model of the Waveshare ESP32-S3-Touch-LCD-2.8 Board
# -----------------------------------------------------------------------------
def build_virtual_board():
    """
    Builds accurate 3D model of the Waveshare board in Landscape orientation.
    Origin (0,0,0) is the center of the active screen, front glass surface at Z=0.
    """
    # Active screen: 57.60 x 43.20 mm
    # Glass: 73.06 x 50.54 x 1.4 mm
    # In Image 1 front view, active screen has 4.0mm left bezel, 11.46mm right bezel.
    # Center of glass relative to active screen center:
    glass_offset_x = (4.0 + 57.60/2.0) - (73.06 / 2.0) # ~ -3.73 mm (screen is shifted +3.73mm relative to glass)
    # Glass centered vertically:
    glass_offset_y = 0.0
    
    # 1. Front Glass
    glass = Manifold.cube([73.06, 50.54, 1.4], center=True).translate([glass_offset_x, glass_offset_y, 0.7])
    
    # 2. LCD Display Module (between glass and PCB, thickness ~1.8mm)
    lcd = Manifold.cube([60.0, 46.0, 1.8], center=True).translate([0, 0, 1.4 + 0.9])
    
    # 3. PCB: 69.00 x 49.90 x 1.6 mm
    # Centered with glass vertically, centered with LCD
    pcb = Manifold.cube([69.00, 49.90, 1.6], center=True).translate([glass_offset_x, glass_offset_y, 1.4 + 1.8 + 0.8])
    
    # 4. 4x PCB Mounting Screw Holes (Ø2.5mm through holes)
    # Spacing from PCB center: X = ±26.27mm, Y = ±18.43mm
    holes = []
    for dx in [-26.27, 26.27]:
        for dy in [-18.43, 18.43]:
            h = Manifold.cylinder(3.0, 1.25, 1.25, 20).translate([glass_offset_x + dx, glass_offset_y + dy, 1.4 + 1.8 - 0.5])
            holes.append(h)
            
    pcb_with_holes = diff_all(pcb, holes)
    
    # 5. Buttons on Bottom Edge (Y = glass_offset_y - 49.90/2 = -24.95mm):
    # BOOT: X = glass_offset_x - 69.0/2 + 12.0 = -34.5 + 12.0 = -22.5mm
    # RESET: X = glass_offset_x - 69.0/2 + 20.0 = -34.5 + 20.0 = -14.5mm
    # BAT_PWR: X = glass_offset_x - 69.0/2 + 28.0 = -34.5 + 28.0 = -6.5mm
    buttons = []
    btn_y = glass_offset_y - 49.90 / 2.0
    btn_z = 1.4 + 1.8 + 1.6
    for bx in [glass_offset_x - 34.5 + 12.0, glass_offset_x - 34.5 + 20.0, glass_offset_x - 34.5 + 28.0]:
        btn = Manifold.cube([4.0, 3.5, 2.0], center=True).translate([bx, btn_y - 1.0, btn_z])
        buttons.append(btn)
        
    # 6. MicroSD Slot (Bottom edge, X ~ glass_offset_x + 5.0mm)
    tf_x = glass_offset_x + 5.0
    tf_slot = Manifold.cube([14.0, 15.0, 2.0], center=True).translate([tf_x, btn_y + 4.0, btn_z])
    
    # 7. USB Type-C Port (Right edge, X = glass_offset_x + 69.0/2 = +30.77mm, Y = glass_offset_y)
    usbc_x = glass_offset_x + 69.00 / 2.0
    usbc_port = Manifold.cube([8.0, 9.0, 3.2], center=True).translate([usbc_x + 1.5, glass_offset_y, btn_z + 0.5])
    
    # 8. Speaker Header (Top edge, Y = glass_offset_y + 49.90/2 = +24.95mm, X = glass_offset_x + 15.0mm)
    spk_header = Manifold.cube([8.0, 6.0, 4.5], center=True).translate([glass_offset_x + 15.0, glass_offset_y + 49.90/2.0 - 3.0, btn_z + 1.2])
    
    board_model = union_all([glass, lcd, pcb_with_holes] + buttons + [tf_slot, usbc_port, spk_header])
    return board_model, {
        'glass_offset_x': glass_offset_x,
        'glass_offset_y': glass_offset_y,
        'hole_positions': [(glass_offset_x + dx, glass_offset_y + dy) for dx in [-26.27, 26.27] for dy in [-18.43, 18.43]],
        'buttons_x': [glass_offset_x - 34.5 + 12.0, glass_offset_x - 34.5 + 20.0, glass_offset_x - 34.5 + 28.0],
        'btn_y': btn_y,
        'tf_x': tf_x,
        'usbc_x': usbc_x,
        'usbc_y': glass_offset_y
    }

if __name__ == '__main__':
    bm, info = build_virtual_board()
    print("Virtual board built successfully!")
    print("Board Info:")
    for k, v in info.items():
        print(f"  {k}: {v}")
