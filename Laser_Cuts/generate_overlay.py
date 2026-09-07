import cv2
import numpy as np

img = cv2.imread('d:/ESP32Radio/3D_Enclosure_JC3248W535/speaker_backdoor_photo.png')
x0, y0 = 92, 85
scale_x = 766.0 / 153.0
scale_y = 448.0 / 90.0

vis = img.copy()

# Draw door coordinate frame at top-left:
cv2.arrowedLine(vis, (x0, y0), (x0 + 60, y0), (0, 200, 255), 2, tipLength=0.25)
cv2.putText(vis, '+X (0 to 153mm)', (x0 + 65, y0 + 5), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 200, 255), 1)
cv2.arrowedLine(vis, (x0, y0), (x0, y0 + 60), (0, 200, 255), 2, tipLength=0.25)
cv2.putText(vis, '+Y (0 to 90mm)', (x0 + 5, y0 + 75), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 200, 255), 1)

# Draw Screw Bosses & Keep-Out Circles
bosses = [(53.7, 81.3, 'Left Boss'), (98.6, 82.5, 'Middle Boss'), (144.7, 84.4, 'Right Boss')]
for hx, hy, label in bosses:
    cx, cy = int(x0 + hx*scale_x), int(y0 + hy*scale_y)
    cv2.circle(vis, (cx, cy), int(4.0*scale_y), (0, 255, 255), 2)
    cv2.circle(vis, (cx, cy), int(1.5*scale_y), (0, 255, 255), -1)
    cv2.putText(vis, label, (cx - 25, cy + 18), cv2.FONT_HERSHEY_SIMPLEX, 0.35, (0, 255, 255), 1)

# Highlight Keep-Out Zone above bottom screw holes (Y >= 74.5mm)
ko_py = int(y0 + 74.5*scale_y)
cv2.line(vis, (x0 + int(45*scale_x), ko_py), (x0 + int(147*scale_x), ko_py), (0, 255, 255), 1)

# Primary Cutout: Option 1 (Full Glass: 96 x 63 mm, X=[47..143], Y=[11.5..74.5])
px1 = int(x0 + 47.0 * scale_x)
px2 = int(x0 + 143.0 * scale_x)
py1 = int(y0 + 11.5 * scale_y)
py2 = int(y0 + 74.5 * scale_y)

# Red Cut Path with semi-transparent fill
overlay = vis.copy()
cv2.rectangle(overlay, (px1, py1), (px2, py2), (0, 0, 255), -1)
cv2.addWeighted(overlay, 0.20, vis, 0.80, 0, vis)
cv2.rectangle(vis, (px1, py1), (px2, py2), (0, 0, 255), 3)

# Inside Cutout: Draw the actual 3.5 Active Screen area (73.4 x 49 mm centered)
cx_sc = (px1 + px2) / 2
cy_sc = (py1 + py2) / 2
sc_w_px = int(73.4 * scale_x)
sc_h_px = int(49.0 * scale_y)
sc_x1 = int(cx_sc - sc_w_px / 2)
sc_x2 = int(cx_sc + sc_w_px / 2)
sc_y1 = int(cy_sc - sc_h_px / 2)
sc_y2 = int(cy_sc + sc_h_px / 2)
cv2.rectangle(vis, (sc_x1, sc_y1), (sc_x2, sc_y2), (255, 255, 255), 2)
cv2.putText(vis, '3.5 inch Active Screen (73.4 x 49.0mm)', (sc_x1 + 10, int(cy_sc - 5)), cv2.FONT_HERSHEY_SIMPLEX, 0.42, (255, 255, 255), 1)

# Clearance Dimension Callouts
cv2.putText(vis, 'Cutout Width: 96.0 mm (+1.5mm clearance)', (px1 + 15, py1 - 8), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 0, 255), 1)
cv2.putText(vis, '63.0 mm', (px2 + 6, int((py1+py2)/2)), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 0, 255), 1)

# Space above boss callout
boss_top = int(y0 + 78.5*scale_y)
cv2.arrowedLine(vis, (int(x0 + 98.6*scale_x), py2), (int(x0 + 98.6*scale_x), boss_top), (0, 255, 0), 2, tipLength=0.3)
cv2.putText(vis, '4.0mm SOLID SPACE ABOVE BOSS', (int(x0 + 55*scale_x), py2 + 14), cv2.FONT_HERSHEY_SIMPLEX, 0.42, (0, 255, 0), 1)

# Header banner
cv2.rectangle(vis, (0, 0), (vis.shape[1], 36), (15, 23, 42), -1)
cv2.putText(vis, 'Creality Falcon 5W Laser Cutout Plan - Right-Bottom Door Placement', (15, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.62, (255, 255, 255), 2)

out_vis = 'd:/ESP32Radio/3D_Enclosure_JC3248W535/laser_cutout_exact_overlay.png'
cv2.imwrite(out_vis, vis)

art_vis = 'C:/Users/sures/.gemini/antigravity-ide/brain/535b617e-c771-4e94-b46b-92a21c7f6ee3/laser_cutout_exact_overlay.png'
cv2.imwrite(art_vis, vis)
print('Saved inspection overlay to', out_vis, 'and', art_vis)
