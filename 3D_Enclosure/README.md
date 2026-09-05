# 📻 ESP32-S3 JC3248W535 Desktop Internet Radio Enclosure

A 3D-printable, 2-piece retro-modern desktop radio enclosure designed specifically for:
1. **ESP32-S3 JC3248W535** (3.5" 320x480 Capacitive Touchscreen Display Board).
2. **2W / 3W 2040 Mini Loudspeaker** (20mm x 40mm x 7mm).
3. **Single 3.7V Lithium-Ion Battery** (18650 Cell or 3.7V 1000–2500mAh LiPo Pouch).
4. **Direct Rear USB-C Charging Port Access** & cooling acoustic vents.

---

## 📐 Enclosure Specifications & Layout

```
 ┌─────────────────────────────────────────────────────────────┐
 │  ┌───────────────────────────────┐     ┌──────────────┐     │
 │  │                               │     │   ||||||||   │     │
 │  │   3.5" IPS Touchscreen        │     │   ||||||||   │     │
 │  │   (JC3248W535 Board)          │     │  2W Speaker  │     │
 │  │   Active: 74mm x 49.5mm       │     │  (20x40mm)   │     │
 │  │                               │     │   ||||||||   │     │
 │  └───────────────────────────────┘     └──────────────┘     │
 │                                                             │
 │  [ Internal: 3.7V Li-Ion Battery Cradle & Cable Routing ]   │
 └─────────────────────────────────────────────────────────────┘
  ◄──────────────────────── 136.8 mm ────────────────────────►
```

- **Outer Dimensions**: 136.8 mm (W) × 78.8 mm (H) × 34.4 mm (D)
- **Wall Thickness**: 2.4 mm (rigid, vibration-damped acoustic resonance)
- **Parts**: 2 parts (Front Faceplate/Shell + Rear Backplate)

---

## 🔩 Hardware Bill of Materials (BOM)

| Item | Specification | Quantity | Purpose |
|---|---|---|---|
| **Display Board** | `ESP32-S3 JC3248W535` (3.5" IPS Touch) | 1 | Main system & UI |
| **Speaker** | 2040 Loudspeaker (4Ω or 8Ω, 2W/3W) | 1 | Mono audio output |
| **Battery** | 3.7V Li-Ion (18650 or 603450/803040 Li-Po) | 1 | Portable power |
| **Board Screws** | M2.5 × 6mm or M3 × 6mm Self-Tapping | 4 | Securing board to front posts |
| **Case Screws** | M3 × 12mm – 16mm (Flat Countersunk, Button Head, or Socket Cap) | 4 | Fastening rear backplate flush to front shell |
| **Acoustic Seal** | Thin Double-Sided Foam Tape / Blu-Tack | Small strip | Sealing speaker perimeter for rich bass |

---

## 🖨️ 3D Printing Recommendations

| Parameter | Recommended Setting | Note |
|---|---|---|
| **Material** | PLA / PETG / ABS | PETG or Matte PLA recommended for finish |
| **Layer Height** | `0.20 mm` (or `0.16 mm` for ultra-smooth bezel) | Standard 0.4mm nozzle |
| **Wall Loops / Perimeters** | `3` to `4` walls | Ensures strong screw posts & acoustic density |
| **Top / Bottom Layers** | `4` Top / `4` Bottom | Solid top surface for the faceplate |
| **Infill** | `20%` – `25%` (Gyroid or Grid) | Good balance of strength and sound insulation |
| **Supports** | **NONE required** | Both parts are designed to lay flat on the print bed |
| **Print Orientation** | - **Front Shell**: Face down on print bed<br>- **Back Plate**: Outer face down on print bed | Max bed adhesion, clean exterior finish & zero supports |

---

## 🛠️ Step-by-Step Assembly Guide

### Step 1: Print the Ultra-Thin Alignment Gauge (Recommended First Step)
Before doing the multi-hour enclosure print, slice and print **[`ESP32S3_Radio_Front_Test_Template.stl`](./ESP32S3_Radio_Front_Test_Template.stl)**.
- **Thickness**: Only **`0.8 mm`** (just 4 layers at 0.20mm layer height).
- **Print Time**: **~2 to 3 minutes** (consumes ~3g of filament).
- **Physical Test**: Place the printed card directly over the front and back of your `ESP32-S3 JC3248W535` board and 2040 speaker to visually check that all 4 board mounting screw holes ($\varnothing 2.8\,\text{mm}$ at $84.5 \times 52.0\,\text{mm}$ pitch), the screen window, and the speaker window match 100%.

### Step 2: Generate or Slice the Full Enclosure STLs
1. The full STL files are already generated and validated in this folder:
   - **[`ESP32S3_Radio_Front_Case.stl`](./ESP32S3_Radio_Front_Case.stl)**
   - **[`ESP32S3_Radio_Back_Plate.stl`](./ESP32S3_Radio_Back_Plate.stl)**
   - **[`ESP32S3_Radio_All_Parts.stl`](./ESP32S3_Radio_All_Parts.stl)** (Combined bed)
2. (Optional) If you customize dimensions, you can re-run `python generate_stl.py` or open `JC3248W535_Radio_Case.scad` in OpenSCAD.

### Step 3: Mount the Display Board (External Panel Mount)
1. Insert the `ESP32-S3 JC3248W535` board from the **FRONT / OUTSIDE** of the cabinet into the $82.5\,\text{mm} \times 57.5\,\text{mm}$ panel cutout.
2. The front glass flange rests flat against the front face of the enclosure.
3. Fasten 4× M2.5 or M3 screws from the **INSIDE / REAR** of the front wall through the 4 pre-drilled through-holes directly into the back of the board's 4 corner mounting ears.

### Step 4: Mount the Speaker
1. Place a thin strip of double-sided foam tape around the rim of the 2040 speaker.
2. Press the speaker firmly into the dedicated speaker chamber on the right.
3. Plug the 2-pin speaker lead into the `SPK` header on the back of the display board.

### Step 5: Install Battery & Wire Power
1. Slide the 3.7V Lithium-Ion cell into the bottom floor battery retention cradle.
2. Plug the battery 2-pin JST connector into the `BAT` header on the board.

### Step 6: Close the Enclosure (Zero-Gap Flush Fit)
1. Lift the printed back plate off the print bed and fold it over onto the front case like closing a book:
   - **Inner Aligning Lip**: Features corner boss relief cutouts ($\varnothing 9.6\,\text{mm}$ arc, $1.0\,\text{mm}$ clearance) that cleanly clear the 4 corner screw bosses. The lip drops $2.2\,\text{mm}$ into the case cavity, guiding the back plate into perfect zero-gap closure.
   - **Cutout Alignment**: The rear USB-C port cutout ($X = +27.75, Y = +4.0$), battery switch button access ($X = +10.0, Y = +23.0$), CPU cooling vents, and speaker louvers ($X = +46.0$) align 100% with all internal components.
2. Insert 4× M3 screws (12mm to 16mm length) through the 4 corner holes:
   - **Universal Screw Head Counterbores**: Sized at $\varnothing 7.2\,\text{mm} \times 1.6\,\text{mm}$ depth with a 45° chamfer transition. M3 flat countersunk, button-head, pan-head, or socket-head cap screws all sink completely flush ("in sync with the back surface") with zero protrusion.
   - **Pilot Holes**: The front case corner bosses feature $\varnothing 2.8\,\text{mm} \times 20\,\text{mm}$ deep pilot holes for smooth, tight screw driving without splitting.

---

## ⚡ Wiring Summary (JC3248W535)

```
             ┌────────────────────────────────────────┐
             │       JC3248W535 REAR CONNECTIONS      │
             │                                        │
             │  [ SPK Header ] ───> 2040 2W Speaker   │
             │  [ BAT Header ] ───> 3.7V Li-Ion/LiPo  │
             │  [ USB-C Port ] ───> 5V Power/Charging │
             └────────────────────────────────────────┘
```
