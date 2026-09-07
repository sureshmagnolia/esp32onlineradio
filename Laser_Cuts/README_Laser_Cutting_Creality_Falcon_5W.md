# Creality Falcon 5W Laser Cutting Guide — Speaker Back Door Display Cutout
**ESP32-S3 JC3248W535 3.5" Touch Display Installation**

---

## 1. Overview & Geometry Analysis

This package contains precision vector CAD and G-code files tailored for the **Creality Falcon 5W Laser Engraver Machine** (72W machine, 5W optical diode, 0.06mm spot size, 400 × 415 mm working area) to cut an opening in your commercial speaker back door ($153.0 \times 90.0\text{ mm}$) to fit the ESP32-S3 JC3248W535 capacitive touch display module.

### Door Layout & Clearance Constraints:
* **Total Door Dimensions**: $153.0\text{ mm}$ (width) $\times 90.0\text{ mm}$ (height), $R = 8.0\text{ mm}$ corner rounds.
* **Top Controls Safe Zone**: Ports (`LINE IN`, `USB`, `DC-5V`, `TF`) occupy $Y = 0\text{ to } 11.5\text{ mm}$; printed text labels end at $Y = 16.0\text{ mm}$.
* **Bottom Screw Holes & Molded Bosses**:
  * Left-bottom boss: Center $(X = 53.7, Y = 81.3\text{ mm})$, boss outer circle top at $Y = 77.3\text{ mm}$.
  * Middle-bottom boss: Center $(X = 98.6, Y = 82.5\text{ mm})$, boss outer circle top at $Y = 78.5\text{ mm}$.
  * Right-bottom corner boss: Center $(X = 144.7, Y = 84.4\text{ mm})$.
* **Existing Air Holes (Vent Grille)**: $X = 69.3\text{ mm to } 125.4\text{ mm}$ ($56.1\text{ mm}$ wide, centered at $X = 97.35\text{ mm}$), $Y = 46.2\text{ mm to } 65.5\text{ mm}$.
* **Right-Bottom Cutout Placement**:
  * The cutout sits directly over the existing air holes on the right-bottom half of the door.
  * **Bottom Edge**: Placed at $Y = 74.5\text{ mm}$ (leaving **$4.0\text{ mm}$ of solid plastic above the screw boss** and **$8.0\text{ mm}$ above the screw hole center**).
  * This guarantees structural rigidity so the screw bosses never crack or flex when the back door is screwed onto the speaker body.

---

## 2. File Inventory

All files are located in `d:/ESP32Radio/3D_Enclosure_JC3248W535/`:

| File Name | Format | Purpose / Description |
| :--- | :--- | :--- |
| [`CrealityFalcon_DisplayCutout_full_glass_96x63.dxf`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/CrealityFalcon_DisplayCutout_full_glass_96x63.dxf) | DXF (AutoCAD R12) | **Option 1 (Full Glass Cutout)**: $96.0 \times 63.0\text{ mm}$ opening ($R=3\text{ mm}$) for inserting the entire $94.5 \times 62.0\text{ mm}$ glass module. $4.0\text{ mm}$ space above bottom screw bosses. |
| [`CrealityFalcon_DisplayCutout_full_glass_96x63.svg`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/CrealityFalcon_DisplayCutout_full_glass_96x63.svg) | SVG (1:1 mm) | Vector file for LightBurn / LaserGRBL with distinct cut (red) and door reference (gray) layers. |
| [`CrealityFalcon_DisplayCutout_full_glass_96x63.gcode`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/CrealityFalcon_DisplayCutout_full_glass_96x63.gcode) | G-Code (GRBL) | Ready-to-run G-code for offline TF/MicroSD card usage on Creality Falcon 5W ($400\text{ mm/min}$, $90\%$ power, 4 passes, framing pass included). |
| [`CrealityFalcon_DisplayCutout_flush_body_85x56.dxf`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/CrealityFalcon_DisplayCutout_flush_body_85x56.dxf) | DXF (AutoCAD R12) | **Option 2 (Flush Body Cutout)**: $85.0 \times 56.0\text{ mm}$ opening ($R=2.5\text{ mm}$). Rear housing body passes through, outer glass flange rests on door surface. Leaves $5.0\text{ mm}$ space above screw bosses and completely preserves top printed labels. |
| [`CrealityFalcon_DisplayCutout_flush_body_85x56.svg`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/CrealityFalcon_DisplayCutout_flush_body_85x56.svg) | SVG (1:1 mm) | Option 2 vector file for LightBurn and LaserGRBL. |
| [`CrealityFalcon_DisplayCutout_flush_body_85x56.gcode`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/CrealityFalcon_DisplayCutout_flush_body_85x56.gcode) | G-Code (GRBL) | Option 2 ready-to-run G-code for Falcon 5W. |
| [`CrealityFalcon_DisplayCutout_screen_window_75x51.dxf`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/CrealityFalcon_DisplayCutout_screen_window_75x51.dxf) | DXF (AutoCAD R12) | **Option 3 (Active Screen Window + 4 Screws)**: $75.0 \times 51.0\text{ mm}$ viewing aperture + 4 corner pilot holes ($\varnothing 2.6\text{ mm}$ at $84.3 \times 52.3\text{ mm}$ pitch). Board mounts to inside face of door. |
| [`CrealityFalcon_DisplayCutout_screen_window_75x51.svg`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/CrealityFalcon_DisplayCutout_screen_window_75x51.svg) | SVG (1:1 mm) | Option 3 vector file for LightBurn / LaserGRBL. |
| [`CrealityFalcon_DisplayCutout_screen_window_75x51.gcode`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/CrealityFalcon_DisplayCutout_screen_window_75x51.gcode) | G-Code (GRBL) | Option 3 ready-to-run G-code for Falcon 5W. |
| [`CrealityFalcon_SpeakerDoor_Template_1to1.pdf`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/CrealityFalcon_SpeakerDoor_Template_1to1.pdf) | Printable PDF | **True 1:1 Scale Printable Paper Template**: Print on standard A4 paper at 100% scale. Includes 100mm verification ruler, door outline, screw hole positions, and cutout boundaries. |

---

## 3. Which Option Should You Choose?

* **Option 1 ($96 \times 63\text{ mm}$ — Full Glass Drop-In)**:
  * Choose this if you want the **entire display module (front glass + rear body)** to push smoothly through the hole from the outside.
  * Dimensions: $96.0\text{ mm}$ wide $\times 63.0\text{ mm}$ high ($R = 3.0\text{ mm}$).
  * Clearance above bottom screw bosses: **$4.0\text{ mm}$ of solid plastic**.
  * Top edge: $Y = 11.5\text{ mm}$ (cuts slightly into the lower half of the printed words `USB`, `DC-5V`, `TF`).

* **Option 2 ($85 \times 56\text{ mm}$ — Flush Body Pass-Through)**:
  * Choose this if you want the **rear black body ($82.9 \times 58.4\text{ mm}$)** to pass through, while the front glass ($94.5 \times 62.0\text{ mm}$) rests flat on the door face like a picture frame bezel.
  * Dimensions: $85.0\text{ mm}$ wide $\times 56.0\text{ mm}$ high ($R = 2.5\text{ mm}$).
  * Clearance above bottom screw bosses: **$5.0\text{ mm}$ of solid plastic**.
  * Top edge: $Y = 17.5\text{ mm}$ (**completely preserves all top printed text labels**).

* **Option 3 ($75 \times 51\text{ mm}$ + 4 Corner Screws — Internal Mount)**:
  * Choose this if you mount the display board to the **inside face of the door** using 4 self-tapping screws through the board's corner tabs.
  * The door provides the front bezel; the active $3.5"$ IPS screen ($73.4 \times 49.0\text{ mm}$) shows cleanly through the window.
  * Corner screw pilot holes: $\varnothing 2.6\text{ mm}$ at $X = \pm 42.15\text{ mm}, Y = \pm 26.15\text{ mm}$ pitch ($84.3 \times 52.3\text{ mm}$).

---

## 4. Creality Falcon 5W Laser Operating Guide

### Machine Specifications:
* **Optical Laser Power**: 5W Diode Laser (wavelength 450nm blue light).
* **Machine Power**: 72W.
* **Laser Spot**: $0.06 \times 0.06\text{ mm}$ ultra-fine compressed spot.
* **Working Area**: $400 \times 415\text{ mm}$.
* **Max Speed**: $10,000\text{ mm/min}$.
* **Controller**: 32-bit silent mainboard, GRBL 1.1f firmware.
* **Offline Usage**: Single multi-function button on top of laser module + TF card slot.

### Laser Parameters for Injection-Molded Plastic (Gold/Bronze):
| Operation | Speed ($F$) | Power ($S$) | Passes | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Framing (Preview)** | $1200\text{ mm/min}$ | $0.5\%$ ($S5$) | 1 | Low-power pilot dot outlines the cut box safely without marking plastic. |
| **Test Scoring (Line Trace)** | $1500\text{ mm/min}$ | $15\%$ ($S150$) | 1 | Lightly scores a hairline surface mark to double-check alignment. |
| **Plastic Cutting ($1.5 - 2.5\text{ mm}$)** | $400\text{ mm/min}$ | $90\%$ ($S900$) | $3 - 5$ | Multiple fast passes prevent heat buildup and melted edges. |
| **Pilot Holes ($\varnothing 2.6\text{ mm}$)** | $200\text{ mm/min}$ | $85\%$ ($S850$) | 2 | Circular interpolation for clean M2.5 screw holes. |

> [!TIP]
> **Air Assist & Cooling**: Directing a small stream of air (or USB air pump) at the cut point cools the plastic instantly, drastically prevents melt ridges/burrs, and yields crisp, sharp laser cuts.

---

## 5. Step-by-Step Execution Workflow

### Step 1: Physical Paper Template Check
1. Open [`CrealityFalcon_SpeakerDoor_Template_1to1.pdf`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/CrealityFalcon_SpeakerDoor_Template_1to1.pdf).
2. Print on A4 paper with **"Actual Size" / 100% Scale** selected (turn off "Fit to Page").
3. Measure the $100\text{ mm}$ calibration ruler on the printout with a physical ruler to confirm exact $1:1$ scale.
4. Cut along the outer line with scissors and place it directly over your physical door. Check the cutout location relative to the air holes and bottom screw holes.

### Step 2: Laser Bed Setup & Focusing
1. Place the speaker back door flat on the Falcon's honeycomb bed or metal slats.
2. Clamp or tape the edges with painter's tape so the door cannot vibrate or shift.
3. **Set the Laser Focus**:
   * Lower the laser module and place the Creality multi-step aluminum focus block between the nozzle tip and the top surface of the door.
   * Rest the laser nozzle on the correct step ($0.06\text{ mm}$ focal distance, typically the lowest step for $5\text{W}$), tighten the two thumbscrews on the side of the bracket, and remove the focus block.

### Step 3: Setting the Origin $(0,0)$
* In **LightBurn / LaserGRBL**:
  1. Set **Start From**: "User Origin" or "Current Position".
  2. Set **Job Origin**: Top-Left corner (or Bottom-Left depending on preference).
  3. Jog the laser head until the red crosshair / pilot dot rests exactly on the **Top-Left corner of the door plate**.
  4. Click **"Set Origin"**.

* In **Offline Mode (TF Card)**:
  1. Copy the `.gcode` file (e.g. `CrealityFalcon_DisplayCutout_full_glass_96x63.gcode`) to the root of the TF card.
  2. Insert TF card into the Falcon controller.
  3. Manually position the laser head over the top-left corner of the door.
  4. Press the top button **once** to run the Framing pass. The laser dot will travel around the perimeter of the cutout.
  5. Verify that the bottom edge of the frame pass stays cleanly above the bottom screw bosses.
  6. Press the top button **again** to start the cut.
  7. To pause at any time, press the button once. To cancel, press and hold the button for 3 seconds.

### Step 4: Post-Processing & Display Mounting
1. After cutting, let the plastic cool for 30 seconds.
2. If there are small melt tabs at corners, trim them flush with a hobby knife or fine needle file.
3. Insert the JC3248W535 display module into the cutout:
   * It will slip in smoothly with $1.0 - 1.5\text{ mm}$ clearance.
   * If using Option 3, secure the 4 corner tabs from the inside using $M2.5 \times 6\text{ mm}$ self-tapping screws.
   * If using Option 1 or 2, a drop of B-7000 electronics adhesive or 4 corner retention tabs inside the speaker enclosure will lock the display firmly in place.
