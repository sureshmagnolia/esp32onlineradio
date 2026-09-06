import numpy as np

# Coordinates Verification:
# Active Screen: 57.6 x 43.2 mm, center at X = 0.0, Y = -3.23
X_screen = 0.0
Y_screen = -3.23
W_screen = 57.6
H_screen = 43.2

# Glass: 73.06 x 50.54 mm
# In Waveshare drawing, active screen has 4.41mm left margin and 11.05mm right margin on the glass.
# So Glass Left = X_screen - W_screen/2 - 4.41 = 0 - 28.8 - 4.41 = -33.21 mm
# Glass Right = X_screen + W_screen/2 + 11.05 = 0 + 28.8 + 11.05 = +39.85 mm
# Glass Width = 39.85 - (-33.21) = 73.06 mm (Exact!)
# Glass Center X = (-33.21 + 39.85) / 2 = +3.32 mm
X_glass = +3.32
Y_glass = -3.23
W_glass = 73.06
H_glass = 50.54

# PCB: 69.0 x 49.9 mm
# In Waveshare drawing, PCB left edge is 0.57mm inside glass left edge:
# PCB Left = -33.21 + 0.57 = -32.64 mm
# PCB Right = -32.64 + 69.00 = +36.36 mm
# PCB Center X = (-32.64 + 36.36) / 2 = +1.86 mm
X_pcb = +1.86
Y_pcb = -3.23
W_pcb = 69.0
H_pcb = 49.9

# Case Viewing Window: 58.4 x 44.0 mm, centered at X = 0.0, Y = -3.23
W_win = 58.4
H_win = 44.0
X_win = 0.0
Y_win = -3.23

print(f"Active Screen X range: [{X_screen - W_screen/2:.2f}, {X_screen + W_screen/2:.2f}]")
print(f"Window Cutout X range: [{X_win - W_win/2:.2f}, {X_win + W_win/2:.2f}]")
print(f"Margin per side between screen and window: {(W_win - W_screen)/2:.2f} mm (Perfect symmetrical reveal!)")

print(f"\nGlass X range: [{X_glass - W_glass/2:.2f}, {X_glass + W_glass/2:.2f}]")
print(f"PCB X range:   [{X_pcb - W_pcb/2:.2f}, {X_pcb + W_pcb/2:.2f}]")
print(f"Left overhang:  {-(X_glass - W_glass/2) + (X_pcb - W_pcb/2):.2f} mm")
print(f"Right overhang: {(X_glass + W_glass/2) - (X_pcb + W_pcb/2):.2f} mm")

# 4 Corner Mounting Holes:
hole_xs = [X_pcb - 26.27, X_pcb + 26.27]
hole_ys = [Y_pcb - 18.43, Y_pcb + 18.43]
print(f"\nMounting Holes X: {hole_xs[0]:.2f}, {hole_xs[1]:.2f}")
print(f"Mounting Holes Y: {hole_ys[0]:.2f}, {hole_ys[1]:.2f}")

# Buttons X:
btn_xs = [X_pcb - 22.5, X_pcb - 14.5, X_pcb - 6.5]
print(f"Buttons X: {[round(x, 2) for x in btn_xs]}")
