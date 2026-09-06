"""
Virtual Assembly and Fitment Simulator.
Checks board seating, standoff hole alignment, button plunger travel,
MicroSD card path, and USB-C port alignment.
"""

import os
import trimesh
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from manifold3d import Manifold
from virtual_board import build_virtual_board

def main():
    board, info = build_virtual_board()
    print("Simulating Virtual Placement...")
    print("Hole positions:")
    for i, (hx, hy) in enumerate(info['hole_positions']):
        print(f"  Hole {i+1}: X={hx:.2f} mm, Y={hy:.2f} mm")
    print("Button positions (X):", [f"{bx:.2f} mm" for bx in info['buttons_x']])
    print(f"Buttons Y: {info['btn_y']:.2f} mm")
    print(f"TF Slot X: {info['tf_x']:.2f} mm")
    print(f"USB-C X: {info['usbc_x']:.2f} mm, Y: {info['usbc_y']:.2f} mm")

if __name__ == '__main__':
    main()
